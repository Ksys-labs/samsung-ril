#define LOG_TAG "RIL-CALL"
#include <utils/Log.h>

#include "samsung-ril.h"

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

/**
 * In: RIL_REQUEST_GET_CURRENT_CALLS
 *   Requests current call list
 *
 * Out: IPC_CALL_LIST GET
 *   Requests a list of active calls
 */
void requestGetCurrentCalls(RIL_Token t)
{
	ipc_client_send_get(IPC_CALL_LIST, getRequestId(t));
}

/**
 * In: IPC_CALL_LIST GET
 *   Provides a list of active calls
 *
 * Out: RIL_REQUEST_GET_CURRENT_CALLS
 *   Requests current call list
 */
void respondCallList(RIL_Token t, void *data, int length)
{
	struct ipc_call_list_entry *entry;
	unsigned char num_entries;
	char *number, *number_ril;
	int i;

	num_entries = *((unsigned char*)data);
	entry = (struct ipc_call_list_entry*)((char*)data+1);

	RIL_Call **calls = (RIL_Call**)malloc(num_entries * sizeof(RIL_Call*));

	for(i = 0; i < num_entries; i++) {
		RIL_Call *call = (RIL_Call*)malloc(sizeof(RIL_Call));

		/* Number is located after call list entry */
		number = ((char*)entry) + sizeof(*entry);

		number_ril = (char*)malloc(entry->number_len + 1);
		memset(number_ril, 0, (entry->number_len + 1));
		memcpy(number_ril, number, entry->number_len);

		call->state = (entry->state-1); /* FIXME: mapping func */
		call->index = (entry->idx+1);
		call->toa = (entry->number_len > 0 && number[0] == '+') ? 145 : 129;
		call->isMpty = entry->mpty;
		call->isMT = (entry->term == IPC_CALL_TERM_MT);
		call->als = 0;
		call->isVoice  = (entry->type == IPC_CALL_TYPE_VOICE);
		call->isVoicePrivacy = 0;
		call->number = number_ril;
		call->numberPresentation = (entry->number_len > 0) ? 0 : 2;
		call->name = NULL;
		call->namePresentation = 2;
		call->uusInfo = NULL;

		calls[i] = call;

		/* Next entry after current number */
		entry = (struct ipc_call_list_entry*)(number + entry->number_len);
	}

	RIL_onRequestComplete(t, RIL_E_SUCCESS, calls, (num_entries * sizeof(RIL_Call*)));

	for(i = 0; i < num_entries; i++) {
		free(calls[i]);
	}

	free(calls);
}

/**
 * In: RIL_REQUEST_HANGUP
 *   Hang up a specific line (like AT+CHLD=1x)
 *
 * Out: IPC_CALL_RELEASE EXEC
 */
void requestHangup(RIL_Token t)
{
	ipc_client_send_exec(IPC_CALL_RELEASE, getRequestId(t));

	/* FIXME: This should actually be sent based on the response from baseband */
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

	/* FIXME: Do we really need to send this? */
	RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
}

/**
 * In: RIL_REQUEST_ANSWER
 *   Answer incoming call
 *
 * Out: IPC_CALL_ANSWER
 */
void requestAnswer(RIL_Token t)
{
	ipc_client_send_exec(IPC_CALL_ANSWER, getRequestId(t));

	/* FIXME: This should actually be sent based on the response from baseband */
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);

	/* FIXME: Do we really need to send this? */
	RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
}

/**
 * In: RIL_REQUEST_DIAL
 *   Initiate voice call
 *
 * Out: IPC_CALL_OUTGOING
 */
void requestDial(RIL_Token t, void *data, size_t datalen)
{
	const RIL_Dial *dial = (const RIL_Dial*)data;
	struct ipc_call_outgoing call;
	int clir;

	if(strlen(dial->address) > sizeof(call.number)) {
		printf("Outgoing call number too long\n");
		return;
	}

	/* FIXME: separate method? */
	switch(dial->clir) {
		case 0:
			clir = IPC_CALL_IDENTITY_DEFAULT;
			break;
		case 1:
			clir = IPC_CALL_IDENTITY_SHOW;
			break;
		case 2:
			clir = IPC_CALL_IDENTITY_HIDE;
			break;
		default:
			clir = IPC_CALL_IDENTITY_DEFAULT;
			break;
	}

	memset(&call, 0x00, sizeof(call));

	call.type = IPC_CALL_TYPE_VOICE;
	call.identity = clir;
	call.prefix = IPC_CALL_PREFIX_NONE;

	call.length = strlen(dial->address);
	memcpy(call.number, dial->address, strlen(dial->address));

	ipc_client_send(ipc_client, IPC_CALL_OUTGOING, IPC_TYPE_EXEC, (unsigned char*)&call, sizeof(call), getRequestId(t));
	
	/* FIXME: This should actually be sent based on the response from baseband */
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

/**
 * In: RIL_UNSOL_CALL_RING
 *   Ring indication for an incoming call (eg, RING or CRING event).
 */
void respondCallIncoming(RIL_Token t, void *data, int length)
{
	RIL_onUnsolicitedResponse(RIL_UNSOL_CALL_RING, NULL, 0);

	/* FIXME: Do we really need to send this? */
	RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
}

/**
 * In: IPC_CALL_STATUS
 *   Indicates that a call's status has changed
 *
 * RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED
 *   Indicate when call state has changed
 */
void respondCallStatus(RIL_Token t, void *data, int length)
{
	RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
}
