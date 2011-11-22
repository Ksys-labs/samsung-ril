#define LOG_TAG "RIL-MISC"
#include <utils/Log.h>

#include "samsung-ril.h"

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

void respondNitz(void *data, int length)
{
	struct ipc_misc_time_info *nitz = (struct ipc_misc_time_info*)data;
	char str[128];

	sprintf(str, "%02u/%02u/%02u,%02u:%02u:%02u+%02d,%02d",
		nitz->year, nitz->mon, nitz->day, nitz->hour, nitz->min, nitz->sec, nitz->tz, 0);

	RIL_onUnsolicitedResponse(RIL_UNSOL_NITZ_TIME_RECEIVED, str, strlen(str) + 1);
}

// TODO: implement RIL_REQUEST_DEVICE_IDENTITY also

void respondIMEI(RIL_Token t, void *data, int length)
{
	struct ipc_misc_me_sn *imei_info;
	char imei[33];
	char imeisv[3];

	imei_info = (struct ipc_misc_me_sn *) data;

	if(radio.tokens.get_imei != 0 && radio.tokens.get_imei != t) 
		LOGE("IMEI tokens mismatch");

	if(imei_info->length > 32)
		return;

	memset(imei, 0, sizeof(imei));
	memset(imeisv, 0, sizeof(imeisv));

	memcpy(imei, imei_info->data, imei_info->length);

	/* Last two bytes of IMEI in imei_info are the SV bytes */
	memcpy(imeisv, (imei_info->data + imei_info->length - 2), 2);

	/* IMEI */
	RIL_onRequestComplete(t, RIL_E_SUCCESS, imei, sizeof(char *));
	radio.tokens.get_imei = 0;

	/* IMEI SV */
	if(radio.tokens.get_imeisv != 0) {
		RIL_onRequestComplete(radio.tokens.get_imeisv, RIL_E_SUCCESS, imeisv, sizeof(char *));
		radio.tokens.get_imeisv = 0;
	}
}

void respondMeSn(RIL_Token t, void *data, int length)
{
	struct ipc_misc_me_sn *me_sn_info;

	me_sn_info = (struct ipc_misc_me_sn *) data;

	switch(me_sn_info->type) {
		case IPC_MISC_ME_SN_SERIAL_NUM:
			respondIMEI(t, data, length);
			break;
		case IPC_MISC_ME_SN_SERIAL_NUM_SERIAL:
			LOGD("Got IPC_MISC_ME_SN_SERIAL_NUM_SERIAL: %s\n", me_sn_info->data);
			break;
	}
}

void requestIMEI(RIL_Token t)
{
	char data;

	if(radio.radio_state != RADIO_STATE_OFF) {
		data = IPC_MISC_ME_SN_SERIAL_NUM;

		ipc_client_send(ipc_client, IPC_MISC_ME_SN, IPC_TYPE_GET, (unsigned char *) &data, sizeof(data), getRequestId(t));
	} else {
		radio.tokens.get_imei = t;
	}
}

void requestIMEISV(RIL_Token t)
{
	radio.tokens.get_imeisv = t;
}

void requestBasebandVersion(RIL_Token t)
{
	if(radio.radio_state != RADIO_STATE_OFF) {
		ipc_client_send_get(IPC_MISC_ME_VERSION, getRequestId(t));
	} else {
		radio.tokens.baseband_version = t;
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

