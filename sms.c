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
	unsigned char *pdu = ((unsigned char*)data+sizeof(struct ipc_sms_incoming_msg));

	/**
	 * H1 libtelplugin seems to provide the SMSC address length
         * instead of the number of octects used for the SMSC info struct
	 */
	pdu[0] = (pdu[0] >> 1) + (pdu[0] & 0x01) + 1;

	int resp_length = length * 2 + 1;
	char *resp = (char*)malloc(resp_length);

	bin2hex(pdu, length, resp);

	if(info->type == IPC_SMS_TYPE_POINT_TO_POINT) {
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS, resp, resp_length);
	} else if(info->type == IPC_SMS_TYPE_STATUS_REPORT) {
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT, resp, resp_length);
	} else {
		LOGE("%s: Unknown message type", __FUNCTION__);
	}

	free(resp);
}

/**
 * Helper function to send a single SMS
 * Optionally notifying the network that
 * additional messages are to be expected
 */
void requestSendSmsEx(RIL_Token t, void *data, size_t datalen, unsigned char hint)
{
	int sc_length, data_length, length;
	struct ipc_sms_send_msg *msg;
	unsigned char *p, *buf;
	const char **request = (const char**)data;

	/**
	 * If the SC is not provided we need to specify length 0 -> 1 zero byte
	 */
	sc_length = (request[0] != NULL) ? (strlen(request[0]) / 2) : 1;
	data_length = (strlen(request[1]) / 2);

	length = sizeof(struct ipc_sms_send_msg) + sc_length + data_length;

	buf = (unsigned char*)malloc(length);
	memset(buf, 0, length);
	p = buf;

	/* First, setup the header required by IPC */
	msg = (struct ipc_sms_send_msg*)p;
	msg->hint = hint;
	msg->length = sc_length + data_length;

	p += sizeof(struct ipc_sms_send_msg);

	/* Add SC data */
	if(sc_length > 1) {
		hex2bin(request[0], strlen(request[0]), p);
	} else {
		*p = 0x00;
	}

	p += sc_length;

	/* Add SMS PDU data */
	hex2bin(request[1], strlen(request[1]), p);

	/* FIXME: ipc_sms_send_msg(buf, length, getRequestId(t)); */
	LOGE("%s: missing impl", __FUNCTION__);

	/* FIXME: Move to baseband response handler */
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

	free(buf);
}

/**
 * In: RIL_REQUEST_SEND_SMS
 *   Send an SMS message
 *
 * Out: IPC_SMS_SEND_MSG
 */
void requestSendSms(RIL_Token t, void *data, size_t datalen)
{
	requestSendSmsEx(t, data, datalen, IPC_SMS_MSG_SINGLE);
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
	requestSendSmsEx(t, data, datalen, IPC_SMS_MSG_MULTIPLE);
}

/**
 * In: RIL_REQUEST_SMS_ACKNOWLEDGE
 *   Acknowledge successful or failed receipt of SMS previously indicated
 *   via RIL_UNSOL_RESPONSE_NEW_SMS
 *
 * Out: IPC_SMS_DELIVER_REPORT
 *   Sends a SMS delivery report
 */
void requestSmsAcknowledge(RIL_Token t)
{
	/* FIXME ipc_sms_deliver_report(getRequestId(t)); */
	LOGE("%s: missing impl", __FUNCTION__);

	/* FIXME: Move to baseband response handler */
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

