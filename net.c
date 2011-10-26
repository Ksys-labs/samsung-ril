#define LOG_TAG "RIL-NET"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

void requestGPRSRegistrationState(RIL_Token t)
{
	struct ipc_net_regist_get message;

	/* We only support one active GPRS registration state request */
	if(!radio.token_ps) {
		radio.token_ps = t;

		ipc_net_regist_get(&message, IPC_NET_SERVICE_DOMAIN_GPRS);
		LOGD("ipc_net_regist [net = %d; domain = %d]", message.net, message.domain);
		ipc_client_send(ipc_client, IPC_NET_REGIST, IPC_TYPE_GET, (unsigned char*)&message, sizeof(message), getRequestId(t));
	} else {
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
}

void respondGPRSRegistrationState(struct ipc_message_info *request, struct ipc_net_regist *netinfo)
{
	int i;
	char *response[4];

	asprintf(&response[0], "%d", registatus_ipc2ril(netinfo->reg_state));
	asprintf(&response[1], "%04x", netinfo->lac);
	asprintf(&response[2], "%08x", netinfo->cid);
	asprintf(&response[3], "%d", act_ipc2ril(netinfo->act));

	RIL_onRequestComplete(radio.token_ps, RIL_E_SUCCESS, response, sizeof(response));

	radio.token_ps = 0;

	for(i = 0; i < 4; i++) {
		free(response[i]);
	}
}

void respondNetRegist(struct ipc_message_info *request)
{
	if(request->type == IPC_TYPE_NOTI) {
		memcpy(&radio.netinfo, request->data, sizeof(radio.netinfo));
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
	} else if(request->type == IPC_TYPE_RESP) {
		struct ipc_net_regist *netinfo = (struct ipc_net_regist*)request->data;

		if(netinfo->domain == 3) {
			LOGD("%s: netinfo service domain = ps", __FUNCTION__);
			respondGPRSRegistrationState(request, netinfo);
		} else {
			LOGE("%s: unhandled service domain: %d", __FUNCTION__, netinfo->domain);
		}
	} else {
		LOGE("%s: unhandled ipc method: %d", __FUNCTION__, request->type);
	}
}

void requestGetPreferredNetworkType(RIL_Token t)
{
	ipc_client_send_get(IPC_NET_MODE_SEL, getRequestId(t));
}

void respondModeSel(struct ipc_message_info *request)
{
	unsigned char ipc_mode = *(unsigned char*)request->data;
	int ril_mode = modesel_ipc2ril(ipc_mode);

	RIL_onRequestComplete(getToken(request->aseq), RIL_E_SUCCESS, &ril_mode, sizeof(int*));
}

void requestSetPreferredNetworkType(RIL_Token t, void *data, size_t datalen)
{
	int ril_mode = *(int*)data;
	unsigned char ipc_mode = modesel_ril2ipc(ril_mode);

	ipc_client_send(ipc_client, IPC_NET_MODE_SEL, IPC_TYPE_SET, &ipc_mode, sizeof(ipc_mode), getRequestId(t));
}
