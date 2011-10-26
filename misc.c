#define LOG_TAG "RIL-MISC"
#include <utils/Log.h>

#include "samsung-ril.h"

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

void requestBasebandVersion(RIL_Token t)
{
	if(radio.radio_state != RADIO_STATE_OFF) {
		ipc_client_send_get(IPC_MISC_ME_VERSION, getRequestId(t));
	} else {
		radio.token_baseband_ver = t;
	}
}

void respondBasebandVersion(struct ipc_message_info *request)
{
	char sw_version[33];
	struct ipc_misc_me_version *version = (struct ipc_misc_me_version*)request->data;

	memcpy(sw_version, version->sw_version, 32);
	sw_version[32] = '\0';

	RIL_onRequestComplete(getToken(request->aseq), RIL_E_SUCCESS, sw_version, sizeof(sw_version));
}

