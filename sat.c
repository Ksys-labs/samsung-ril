#define LOG_TAG "RIL-SAT"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

/**
 * In: IPC_SAT_PROACTIVE_CMD
 *   STK proactive command
 *
 * Out: RIL_UNSOL_STK_PROACTIVE_COMMAND
 */
void respondSatProactiveCmdIndi(struct ipc_message_info *request)
{
	int data_len = (request->length-2);
	char *hexdata;

	hexdata = (char*)malloc(data_len*2+1);

	bin2hex((unsigned char*)request->data+2, data_len, hexdata);

	RIL_onUnsolicitedResponse(RIL_UNSOL_STK_PROACTIVE_COMMAND, hexdata, sizeof(char*));

	free(hexdata);
}

/**
 * In: IPC_SAT_PROACTIVE_CMD RESP
 *   STK proactive command
 *
 * Out: RIL_UNSOL_STK_SESSION_END
 */
void respondSatProactiveCmdResp(struct ipc_message_info *request)
{
	unsigned char sw1, sw2;

	sw1 = ((unsigned char*)request->data)[0];
	sw2 = ((unsigned char*)request->data)[1];

	if(sw1 == 0x90 && sw2 == 0x00) {
		RIL_onUnsolicitedResponse(RIL_UNSOL_STK_SESSION_END, NULL, 0);
	} else {
		LOGE("%s: unhandled response sw1=%02x sw2=%02x", __FUNCTION__, sw1, sw2);
	}
}

/**
 * Proactive command indi/resp helper function
 */
void respondSatProactiveCmd(struct ipc_message_info *request)
{
	if(request->type == IPC_TYPE_INDI) {
		respondSatProactiveCmdIndi(request);
	} else if(request->type == IPC_TYPE_RESP) {
		respondSatProactiveCmdResp(request);
	} else {
		LOGE("%s: unhandled proactive command response type %d", __FUNCTION__, request->type);
	}
}

/**
 * In: RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE
 *   Requests to send a terminal response to SIM for a received
 *   proactive command
 *
 * Out: IPC_SAT_PROACTIVE_CMD GET
 */
void requestSatSendTerminalResponse(RIL_Token t, void *data, size_t datalen)
{
	unsigned char buf[264];
	int len = (strlen(data) / 2);

	if(len > 255) {
		LOGE("%s: data exceeds maximum length", __FUNCTION__);
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	memset(buf, 0, sizeof(buf));

	buf[0] = len;
	hex2bin(data, strlen(data), &buf[1]);

	ipc_client_send(ipc_client, IPC_SAT_PROACTIVE_CMD, IPC_TYPE_GET, buf, sizeof(buf), getRequestId(t));

	RIL_onRequestComplete(t, RIL_E_SUCCESS, buf, sizeof(char*));
}

/**
 * In: RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND
 *   Requests to send a SAT/USAT envelope command to SIM.
 *   The SAT/USAT envelope command refers to 3GPP TS 11.14 and 3GPP TS 31.111
 *
 * Out: IPC_SAT_ENVELOPE_CMD EXEC
 */
void requestSatSendEnvelopeCommand(RIL_Token t, void *data, size_t datalen)
{
	unsigned char buf[264];
	int len = (strlen(data) / 2);

	if(len > 255) {
		LOGE("%s: data exceeds maximum length", __FUNCTION__);
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	memset(buf, 0, sizeof(buf));

	buf[0] = len;
	hex2bin(data, strlen(data), &buf[1]);

	ipc_client_send(ipc_client, IPC_SAT_ENVELOPE_CMD, IPC_TYPE_EXEC, buf, sizeof(buf), getRequestId(t));
}

/**
 * In: IPC_SAT_ENVELOPE_CMD EXEC
 *
 * Out: RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND
 *   Requests to send a SAT/USAT envelope command to SIM.
 *   The SAT/USAT envelope command refers to 3GPP TS 11.14 and 3GPP TS 31.111
 */
void respondSatEnvelopeCmd(struct ipc_message_info *request)
{
	int data_len = (request->length-2);
	char *hexdata;

	hexdata = (char*)malloc(data_len*2+1);

	bin2hex((unsigned char*)request->data+2, data_len, hexdata);

	RIL_onRequestComplete(getToken(request->aseq), RIL_E_SUCCESS, hexdata, sizeof(char*));

	free(hexdata);
}

