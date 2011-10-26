#include <telephony/ril.h>
#include <radio.h>

#define RIL_onRequestComplete(t, e, response, responselen) rilenv->OnRequestComplete(t,e, response, responselen)
#define RIL_onUnsolicitedResponse(a,b,c) rilenv->OnUnsolicitedResponse(a,b,c)
#define RIL_requestTimedCallback(a,b,c) rilenv->RequestTimedCallback(a,b,c)

#define ipc_client_send_get(type, request) \
	ipc_client_send(ipc_client, type, IPC_TYPE_GET, NULL, 0, request)

#define ipc_client_send_set(type, request, data, len) \
	ipc_client_send(ipc_client, type, IPC_TYPE_SET, data, len, request)

#define ipc_client_send_exec(type, request) \
	ipc_client_send(ipc_client, type, IPC_TYPE_EXEC, NULL, 0, request)

typedef enum {
	SIM_ABSENT			= 0,
	SIM_NOT_READY			= 1,
	SIM_READY			= 2,
	SIM_PIN				= 3,
	SIM_PUK				= 4,
	SIM_BLOCKED			= 5,
	SIM_NETWORK_PERSO 		= 6,
	SIM_NETWORK_SUBSET_PERSO	= 7,
	SIM_CORPORATE_PERSO		= 8,
	SIM_SERVICE_PROVIDER_PERSO	= 9,
} SIM_Status;

struct radio_state {
	RIL_RadioState radio_state;
	RIL_CardState card_state;

	RIL_Token token_imei;
	RIL_Token token_imeisv;

	RIL_Token token_baseband_ver;

	struct ipc_net_regist netinfo;

	/* SIM status - RIL_REQUEST_GET_SIM_STATUS */
	SIM_Status sim_status;

	/* Samsung H1 baseband returns bogus request id for NET_REGIST GETs */
	RIL_Token token_ps, token_cs;
};

int getRequestId(RIL_Token token);
RIL_Token getToken(int id);

/* Call */
void requestCallList(RIL_Token t);
void requestGetCurrentCalls(RIL_Token t);
void requestHangup(RIL_Token t);
void requestAnswer(RIL_Token t);
void requestDial(RIL_Token t, void *data, size_t datalen);
void respondCallIncoming(RIL_Token t, void *data, int length);
void respondCallStatus(RIL_Token t, void *data, int length);
void respondCallList(RIL_Token t, void *data, int length);

/* Misc */
void requestBasebandVersion(RIL_Token t);
void respondBasebandVersion(struct ipc_message_info *request);

/* Net */
void requestGPRSRegistrationState(RIL_Token t);
void respondNetRegist(struct ipc_message_info *request);
void requestGetPreferredNetworkType(RIL_Token t);
void respondModeSel(struct ipc_message_info *request);
void requestSetPreferredNetworkType(RIL_Token t, void *data, size_t datalen);

/* SIM */
void respondSimStatusChanged(RIL_Token t, void *data, int length);
void requestSimStatus(RIL_Token t);
void requestSimIo(RIL_Token t, void *data, size_t datalen);
void respondSecRsimAccess(RIL_Token t, void *data, int length);
void requestEnterSimPin(RIL_Token t, void *data, size_t datalen);
void respondSecPinStatus(struct ipc_message_info *request);
void respondLockInfo(struct ipc_message_info *request);
void requestQueryFacilityLock(RIL_Token t, void *data, size_t datalen);
void respondSecPhoneLock(struct ipc_message_info *request);
void requestSetFacilityLock(RIL_Token t, void *data, size_t datalen);

/* SAT */
void respondSatProactiveCmd(struct ipc_message_info *request);
void requestSatSendTerminalResponse(RIL_Token t, void *data, size_t datalen);
void requestSatSendEnvelopeCommand(RIL_Token t, void *data, size_t datalen);
void respondSatEnvelopeCmd(struct ipc_message_info *request);

/* SMS */
void respondSmsIncoming(RIL_Token t, void *data, int length);
void requestSendSmsEx(RIL_Token t, void *data, size_t datalen, unsigned char hint);
void requestSendSms(RIL_Token t, void *data, size_t datalen);
void requestSendSmsExpectMore(RIL_Token t, void *data, size_t datalen);
void requestSmsAcknowledge(RIL_Token t);

void requestIMSI(RIL_Token t);
void respondIMSI(struct ipc_message_info *request);

