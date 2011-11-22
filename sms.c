#define LOG_TAG "RIL-SMS"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

void respondSmsIncoming(RIL_Token t, void *data, int length)
{
	struct ipc_sms_incoming_msg *info = (struct ipc_sms_incoming_msg*)data;
	unsigned char *pdu = ((unsigned char*)data + sizeof(struct ipc_sms_incoming_msg));

	int resp_length = info->length * 2 + 1;
	char *resp = (char*)malloc(resp_length);

	bin2hex(pdu, info->length, resp);
	LOGD("PDU string is '%s'\n", resp);

	if(radio.msg_tpid_lock != 0) {
		LOGE("Another message is already waiting for ACK, aborting");
		//FIXME: it would be cleaner to enqueue it
		goto exit;
	}

	radio.msg_tpid_lock = info->msg_tpid;

	if(info->type == IPC_SMS_TYPE_POINT_TO_POINT) {
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS, resp, resp_length);
	} else if(info->type == IPC_SMS_TYPE_STATUS_REPORT) {
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT, resp, resp_length);
	} else {
		LOGE("%s: Unknown message type", __FUNCTION__);
	}

exit:
	free(resp);
}

void requestSendSms(RIL_Token t, void *data, size_t datalen)
{
	const char **request;
	request = (char **) data;
	int pdu_len = strlen(request[1]);

	/* We first need to get SMS SVC before sending the message */

	if(request[0] == NULL) {
		LOGD("We have no SMSC, asking one before anything");

		if(radio.tokens.send_sms != 0 && radio.msg_pdu != NULL) {
			LOGE("Another message is being sent, aborting");
			//FIXME: it would be cleaner to enqueue it
		}

		radio.tokens.send_sms = t;
		radio.msg_pdu = malloc(pdu_len);
		memcpy(radio.msg_pdu, request[1], pdu_len);

		ipc_client_send_get(IPC_SMS_SVC_CENTER_ADDR, getRequestId(t));
		
	} else {
		sms_send_msg(t, request[1], request[0]);
	}
}

void sms_send_msg(RIL_Token t, char *pdu, char *smsc_string)
{
	char *data;
	char *p;
	struct ipc_sms_send_msg send_msg;
	char *decoded_pdu;

	int pdu_str_len = strlen(pdu);
	int pdu_len = pdu_str_len / 2;
	int smsc_len = smsc_string[0];
	int send_msg_len = sizeof(struct ipc_sms_send_msg);
	int length = pdu_len + smsc_len + send_msg_len;

	LOGD("Sending SMS message!");

	LOGD("length is %x + %x + %x = 0x%x\n", pdu_len, smsc_len, send_msg_len, length);

	decoded_pdu = malloc(pdu_len);
	hex2bin(pdu, pdu_str_len, decoded_pdu);

	data = malloc(length);
	memset(&send_msg, 0, sizeof(struct ipc_sms_send_msg));

	send_msg.type = IPC_SMS_TYPE_OUTGOING;
	send_msg.msg_type = IPC_SMS_MSG_SINGLE; //fixme: define based on PDU
	send_msg.length = (unsigned char) length;
	send_msg.smsc_len = (unsigned char) smsc_len;

	p = data;

	memcpy(p, &send_msg, send_msg_len);
	p +=  send_msg_len;
	memcpy(p, (char *) (smsc_string + 1), smsc_len);
	p += smsc_len;
	memcpy(p, decoded_pdu, pdu_len);

	ipc_client_send(ipc_client, IPC_SMS_SEND_MSG, IPC_TYPE_EXEC, data, length, getRequestId(t));

	/* Wait for ACK to return to RILJ */
	radio.tokens.send_sms = t;

	free(decoded_pdu);
	free(data);
}

void respondSmsSvcCenterAddr(RIL_Token t, void *data, size_t datalen)
{
	if(radio.tokens.send_sms == t && radio.msg_pdu != NULL) {
		sms_send_msg(t, radio.msg_pdu, data);

		free(radio.msg_pdu);
	}
}

/**
 * In: RIL_REQUEST_SEND_SMS_EXPECT_MORE
 *   Send an SMS message. Identical to RIL_REQUEST_SEND_SMS,
 *   except that more messages are expected to be sent soon. If possible,
 *   keep SMS relay protocol link open (eg TS 27.005 AT+CMMS command)
 *
 * Out: IPC_SMS_SEND_MSG
 */
void requestSendSmsExpectMore(RIL_Token t, void *data, size_t datalen)
{
	/* FIXME: We seriously need a decent queue here */

}

unsigned short sms_ack_error_ril2ipc(int success, int failcause)
{
	if(success) {
		return IPC_SMS_ACK_NO_ERROR;
	} else {
		switch(failcause) {
			case 0xD3:
				return IPC_SMS_ACK_PDA_FULL_ERROR;
			default:
				return IPC_SMS_ACK_UNSPEC_ERROR;
		}
	}
}

RIL_Errno sms_ack_error_ipc2ril(unsigned short error)
{
	switch(error) {
		case IPC_SMS_ACK_NO_ERROR:
			return RIL_E_SUCCESS;
		default:
			return RIL_E_GENERIC_FAILURE;
	}
}

void respondSmsSendMsg(RIL_Token t, void *data, size_t datalen)
{
	struct ipc_sms_deliv_report_msg *report_msg = data;
	RIL_Errno ril_ack_err;

	if(radio.tokens.send_sms != t) {
		LOGE("Wrong token to ACK");
	}

	LOGD("RECV ack for msg_tpid %d\n", report_msg->msg_tpid);

	ril_ack_err = sms_ack_error_ipc2ril(report_msg->error);

	radio.tokens.send_sms = 0;

	RIL_onRequestComplete(t, ril_ack_err, NULL, 0);

}

/**
 * In: RIL_REQUEST_SMS_ACKNOWLEDGE
 *   Acknowledge successful or failed receipt of SMS previously indicated
 *   via RIL_UNSOL_RESPONSE_NEW_SMS
 *
 * Out: IPC_SMS_DELIVER_REPORT
 *   Sends a SMS delivery report
 */
void requestSmsAcknowledge(RIL_Token t, void *data, size_t datalen)
{
	struct ipc_sms_deliv_report_msg report_msg;
	int success = ((int *)data)[0];
	int failcause = ((int *)data)[1];

	if(radio.msg_tpid_lock == 0) {
		LOGE("No stored msg_tpid, aborting\n");
		return;
	}

	report_msg.type = IPC_SMS_TYPE_STATUS_REPORT;
	report_msg.error = sms_ack_error_ril2ipc(success, failcause);
	report_msg.msg_tpid = radio.msg_tpid_lock;
	report_msg.unk = 0;

	radio.msg_tpid_lock = 0;

	ipc_client_send(ipc_client, IPC_SMS_DELIVER_REPORT, IPC_TYPE_EXEC, (void *) &report_msg, sizeof(struct ipc_sms_deliv_report_msg), getRequestId(t));
}

void respondSmsDeliverReport(RIL_Token t, void *data, size_t datalen)
{
	//FIXME: check it's alright from data (or not, no need to check the ACK of our ACK)

	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

void respondSmsDeviceReady(RIL_Token t, struct ipc_message_info *info)
{
	if(radio.radio_state == RADIO_STATE_SIM_READY) {
		ipc_client_send(ipc_client, IPC_SMS_DEVICE_READY, IPC_TYPE_SET, NULL, 0, getRequestId(t));
	}

	RadioTokensCheck();
}
