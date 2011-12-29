/**
 * This file is part of samsung-ril.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
 * Copyright (C) 2011 Paul Kocialkowski <contact@oaulk.fr>
 *
 * samsung-ril is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * samsung-ril is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with samsung-ril.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define LOG_TAG "RIL-SMS"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

/**
 * Format conversion utils
 */

unsigned short ril2ipc_sms_ack_error(int success, int failcause)
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

RIL_Errno ipc2ril_sms_ack_error(unsigned short error)
{
	switch(error) {
		case IPC_SMS_ACK_NO_ERROR:
			return RIL_E_SUCCESS;
		default:
			return RIL_E_GENERIC_FAILURE;
	}
}

void ipc_sms_incoming_msg(struct ipc_message_info *info)
{
	struct ipc_sms_incoming_msg *msg = (struct ipc_sms_incoming_msg *) info->data;
	unsigned char *pdu = ((unsigned char *) info->data + sizeof(struct ipc_sms_incoming_msg));

	int resp_length = msg->length * 2 + 1;
	char *resp = (char *) malloc(resp_length);

	bin2hex(pdu, msg->length, resp);
	LOGD("PDU string is '%s'\n", resp);

	if(ril_state.msg_tpid_lock != 0) {
		LOGE("Another message is already waiting for ACK, aborting");
		//FIXME: it would be cleaner to enqueue it
		goto exit;
	}

	ril_state.msg_tpid_lock = msg->msg_tpid;

	if(msg->type == IPC_SMS_TYPE_POINT_TO_POINT) {
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS, resp, resp_length);
	} else if(msg->type == IPC_SMS_TYPE_STATUS_REPORT) {
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT, resp, resp_length);
	} else {
		LOGE("%s: Unknown message type", __FUNCTION__);
	}

exit:
	free(resp);
}

/**
 * In: RIL_REQUEST_SMS_ACKNOWLEDGE
 *   Acknowledge successful or failed receipt of SMS previously indicated
 *   via RIL_UNSOL_RESPONSE_NEW_SMS
 *
 * Out: IPC_SMS_DELIVER_REPORT
 *   Sends a SMS delivery report
 */
void ril_request_sms_acknowledge(RIL_Token t, void *data, size_t datalen)
{
	struct ipc_sms_deliv_report_msg report_msg;
	int success = ((int *)data)[0];
	int failcause = ((int *)data)[1];

	if(ril_state.msg_tpid_lock == 0) {
		LOGE("No stored msg_tpid, aborting\n");
		return;
	}

	report_msg.type = IPC_SMS_TYPE_STATUS_REPORT;
	report_msg.error = ril2ipc_sms_ack_error(success, failcause);
	report_msg.msg_tpid = ril_state.msg_tpid_lock;
	report_msg.unk = 0;

	ril_state.msg_tpid_lock = 0;

	ipc_fmt_send(IPC_SMS_DELIVER_REPORT, IPC_TYPE_EXEC, (void *) &report_msg, sizeof(struct ipc_sms_deliv_report_msg), reqGetId(t));
}

void ipc_sms_deliver_report(struct ipc_message_info *info)
{
	//FIXME: check it's alright from data (or not, no need to check the ACK of our ACK)

	RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_SUCCESS, NULL, 0);
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

	ipc_fmt_send(IPC_SMS_SEND_MSG, IPC_TYPE_EXEC, data, length, reqGetId(t));

	/* Wait for ACK to return to RILJ */
	ril_state.tokens.send_sms = t;

	free(decoded_pdu);
	free(data);
}

void ril_request_send_sms(RIL_Token t, void *data, size_t datalen)
{
	const char **request;
	request = (char **) data;
	int pdu_len = strlen(request[1]);

	/* We first need to get SMS SVC before sending the message */

	if(request[0] == NULL) {
		LOGD("We have no SMSC, asking one before anything");

		if(ril_state.tokens.send_sms != 0 && ril_state.msg_pdu != NULL) {
			LOGE("Another message is being sent, aborting");
			//FIXME: it would be cleaner to enqueue it
		}

		ril_state.tokens.send_sms = t;
		ril_state.msg_pdu = malloc(pdu_len);
		memcpy(ril_state.msg_pdu, request[1], pdu_len);

		ipc_fmt_send_get(IPC_SMS_SVC_CENTER_ADDR, reqGetId(t));
		
	} else {
		sms_send_msg(t, request[1], request[0]);
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
void ril_request_send_sms_expect_more(RIL_Token t, void *data, size_t datalen)
{
	/* FIXME: We seriously need a decent queue here */

}

void ipc_sms_svc_center_addr(struct ipc_message_info *info)
{
	RIL_Token t = reqGetToken(info->aseq);

	if(ril_state.tokens.send_sms == t && ril_state.msg_pdu != NULL) {
		sms_send_msg(t, ril_state.msg_pdu, info->data);

		free(ril_state.msg_pdu);
	}
}

void ipc_sms_send_msg(struct ipc_message_info *info)
{
	struct ipc_sms_deliv_report_msg *report_msg = info->data;
	RIL_Errno ril_ack_err;
	RIL_Token t = reqGetToken(info->aseq);

	if(ril_state.tokens.send_sms != t) {
		LOGE("Wrong token to ACK");
	}

	LOGD("RECV ack for msg_tpid %d\n", report_msg->msg_tpid);

	ril_ack_err = ipc2ril_sms_ack_error(report_msg->error);

	ril_state.tokens.send_sms = 0;

	//messageRef = tpid, it's not NULL	↓
	RIL_onRequestComplete(t, ril_ack_err, NULL, 0);

}

void ipc_sms_device_ready(struct ipc_message_info *info)
{
	if(ril_state.radio_state == RADIO_STATE_SIM_READY) {
		ipc_fmt_send(IPC_SMS_DEVICE_READY, IPC_TYPE_SET, NULL, 0, info->aseq);
	}

	ril_tokens_check();
}
