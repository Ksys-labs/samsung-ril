#include <telephony/ril.h>

#include <radio.h>
#include <util.h>

#define LOG_TAG "RIL"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

#define RIL_VERSION_STRING "Samsung H1 RIL"

struct radio_state radio;
const struct RIL_Env *rilenv;
struct ipc_client *ipc_client;

/* Tokens associated with requests */
RIL_Token request_ids[256];
int rid = 0;

void start_thread();

int getRequestId(RIL_Token token)
{
	int id = (rid++ % 0x7E); //fixme
	request_ids[id] = token;

	//LOGD("Assigned request id token=%08X id=%02X\n", token, id);

	return id;
}

void setToken(int id, RIL_Token token)
{
	request_ids[id] = token;
}

RIL_Token getToken(int id)
{
	//LOGD("Got token=%08X id=%02X\n", request_ids[id], id);
	return request_ids[id];
}

void respondNitz(void *data, int length)
{
	struct ipc_misc_time_info *nitz = (struct ipc_misc_time_info*)data;
	char str[128];

	sprintf(str, "%02u/%02u/%02u,%02u:%02u:%02u+%02d,%02d",
		nitz->year, nitz->mon, nitz->day, nitz->hour, nitz->min, nitz->sec, nitz->tz, 0);

	RIL_onUnsolicitedResponse(RIL_UNSOL_NITZ_TIME_RECEIVED, str, strlen(str) + 1);
}

void requestIMEI(RIL_Token t)
{
	if(radio.radio_state != RADIO_STATE_OFF) {
		ipc_client_send_get(IPC_MISC_ME_SN, getRequestId(t));
	} else {
		radio.token_imei = t;
	}
}

void respondIMEI(RIL_Token t, void *data, int length)
{
	struct ipc_misc_me_sn *imei_info;
	char imei[33];
	char imeisv[3];

	imei_info = (struct ipc_misc_me_sn*)data;

	if(imei_info->length > 32)
		return;

	memset(imei, 0, sizeof(imei));
	memset(imeisv, 0, sizeof(imeisv));

	memcpy(imei, imei_info->imei, imei_info->length);

	/* Last two bytes of IMEI in imei_info are the SV bytes */
	memcpy(imeisv, (imei_info->imei + imei_info->length - 2), 2);

	/* IMEI */
	RIL_onRequestComplete(t, RIL_E_SUCCESS, imei, sizeof(char*));

	/* IMEI SV */
	RIL_onRequestComplete(radio.token_imeisv, RIL_E_SUCCESS, imeisv, sizeof(char*));
}

void requestIMEISV(RIL_Token t)
{
	radio.token_imeisv = t;
}

void requestOperator(RIL_Token t)
{
	ipc_client_send_get(IPC_NET_CURRENT_PLMN, getRequestId(t));
}

void respondOperator(RIL_Token t, void *data, int length)
{
	struct ipc_net_current_plmn *plmndata = (struct ipc_net_current_plmn*)data;

	char plmn[7];
	memset(plmn, 0, sizeof(plmn));
	memcpy(plmn, plmndata->plmn, 6);

	if(plmn[5] == '#')
		plmn[5] = '\0'; //FIXME: remove #?

	char *response[3];
	asprintf(&response[0], "%s", plmn_lookup(plmn));
	//asprintf(&response[1], "%s", "Voda NL");
	response[1] = NULL;
	response[2] = plmn;

	RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

	free(response[0]);
	free(response[1]);
}

void requestRegistrationState(RIL_Token t)
{
	char *response[15];
	memset(response, 0, sizeof(response));

	asprintf(&response[0], "%d", 1); //FIXME: registration state
	asprintf(&response[1], "%x", radio.netinfo.lac);
	asprintf(&response[2], "%x", radio.netinfo.cid);
	asprintf(&response[3], "%d", 1); //FIXME: network technology

	RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

	free(response[0]);
	free(response[1]);
	free(response[2]);
}

void requestNwSelectionMode(RIL_Token t)
{
	unsigned int mode = 0;
	RIL_onRequestComplete(t, RIL_E_SUCCESS, &mode, sizeof(mode));
}

void requestAvailNetworks(RIL_Token t)
{
	ipc_client_send_get(IPC_NET_PLMN_LIST, getRequestId(t));
}

/* FIXME: cleanup struct names & resp[] addressing */
void respondAvailNetworks(RIL_Token t, void *data, int length)
{
	struct ipc_net_plmn_entries *entries_info = (struct ipc_net_plmn_entries*)data;
	struct ipc_net_plmn_entry *entries = entries_info->data;

	int i ;
	int size = (4 * entries_info->num * sizeof(char*));
	int actual_size = 0;

	char **resp = malloc(size);
	char **resp_ptr = resp;

	for(i = 0; i < entries_info->num; i++) {
		/* Assumed type for 'emergency only' PLMNs */
		if(entries[i].type == 0x01)
			continue;

		char *plmn = plmn_string(entries[i].plmn);

		/* Long (E)ONS */
		asprintf(&resp_ptr[0], "%s", plmn_lookup(plmn));

		/* Short (E)ONS - FIXME: real short EONS */
		asprintf(&resp_ptr[1], "%s", plmn_lookup(plmn));

		/* PLMN */
		asprintf(&resp_ptr[2], "%s", plmn);

		free(plmn);

		/* PLMN status */
		switch(entries[i].status) {
			case IPC_NET_PLMN_STATUS_AVAILABLE:
				asprintf(&resp_ptr[3], "available");
				break;
			case IPC_NET_PLMN_STATUS_CURRENT:
				asprintf(&resp_ptr[3], "current");
				break;
			case IPC_NET_PLMN_STATUS_FORBIDDEN:
				asprintf(&resp_ptr[3], "forbidden");
				break;
			default:
				asprintf(&resp_ptr[3], "unknown");
				break;
		}

		actual_size++;
		resp_ptr += 4;
	}

	RIL_onRequestComplete(t, RIL_E_SUCCESS, resp, (4 * sizeof(char*) * actual_size));

	/* FIXME: free individual strings */
	free(resp);
}

void respondSignalStrength(RIL_Token t, void *data, int length)
{
	struct ipc_disp_icon_info *signal_info = (struct ipc_disp_icon_info*)data;
	int rssi;

	RIL_SignalStrength ss;
	memset(&ss, 0, sizeof(ss));

	/* Multiplying the number of bars by 3 yields
	 * an asu with an equal number of bars in Android
	 */
	ss.GW_SignalStrength.signalStrength = (3 * signal_info->rssi);
	ss.GW_SignalStrength.bitErrorRate = 99;

	RIL_onUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, &ss, sizeof(ss));
}

void requestPower(RIL_Token t, void *data, size_t datalen)
{
	int *power_state = (int*)data;

	if(*power_state) {
		start_thread();
		RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	} else {
		LOGE("%s: power off not implemented", __FUNCTION__);
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
}

void onRequest(int request, void *data, size_t datalen, RIL_Token t)
{
	//LOGD("%s: start %d %08X", __FUNCTION__, request, t);
	
	switch(request) {
		case RIL_REQUEST_RADIO_POWER:
			requestPower(t, data, datalen);
			break;
		case RIL_REQUEST_BASEBAND_VERSION:
			requestBasebandVersion(t);
			break;
		case RIL_REQUEST_GET_IMSI:
			requestIMSI(t);
			break;
		case RIL_REQUEST_GET_IMEI:
			requestIMEI(t);
			break;
		case RIL_REQUEST_GET_IMEISV:
			requestIMEISV(t);
			break;
		case RIL_REQUEST_OPERATOR:
			requestOperator(t);
			break;
		case RIL_REQUEST_REGISTRATION_STATE:
			requestRegistrationState(t);
			break;
		case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:
			requestGetPreferredNetworkType(t);
			break;
		case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
			requestSetPreferredNetworkType(t, data, datalen);
			break;
		case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
			requestNwSelectionMode(t);
			break;
		case RIL_REQUEST_GPRS_REGISTRATION_STATE:
			requestGPRSRegistrationState(t);
			break;
		case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:
			requestAvailNetworks(t);
			break;
		case RIL_REQUEST_DIAL:
			requestDial(t, data, datalen);
			break;
		case RIL_REQUEST_GET_CURRENT_CALLS:
			requestGetCurrentCalls(t);
			break;
		case RIL_REQUEST_HANGUP:
		//case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND: /* FIXME: We actually don't support background calls */
		//case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:  /* FIXME: We actually don't support background calls */
			requestHangup(t);
			break;
		case RIL_REQUEST_ANSWER:
			requestAnswer(t);
			break;
		case RIL_REQUEST_SEND_SMS:
			requestSendSms(t, data, datalen);
			break;
		case RIL_REQUEST_SEND_SMS_EXPECT_MORE:
			requestSendSms(t, data, datalen);
			break;
		case RIL_REQUEST_SMS_ACKNOWLEDGE:
			requestSmsAcknowledge(t);
			break;
		case RIL_REQUEST_GET_SIM_STATUS:
			requestSimStatus(t);
			break;
		case RIL_REQUEST_SIM_IO:
			requestSimIo(t, data, datalen);
			break;
		case RIL_REQUEST_ENTER_SIM_PIN:
			requestEnterSimPin(t, data, datalen);
			break;
		case RIL_REQUEST_QUERY_FACILITY_LOCK:
			requestQueryFacilityLock(t, data, datalen);
			break;
		case RIL_REQUEST_SET_FACILITY_LOCK:
			requestSetFacilityLock(t, data, datalen);
			break;
		case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:
			requestSatSendTerminalResponse(t, data, datalen);
			break;
		case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:
			requestSatSendEnvelopeCommand(t, data, datalen);
			break;
		case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:
			RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
			break;
		default:
			LOGE("Request not implemented: %d\n", request);
			RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
			break;
	}
}

RIL_RadioState currentState()
{
	return radio.radio_state;
}

int onSupports(int requestCode)
{
	switch(requestCode) {
		default:
			return 1;
	}
}

void onCancel(RIL_Token t)
{
	/* Todo */
}

const char *getVersion(void)
{
	return RIL_VERSION_STRING;
}

void respondPowerup()
{
	/* Update radio state */
	radio.radio_state = RADIO_STATE_SIM_NOT_READY;
	RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);

	/* Send pending IMEI and baseband version requests */
	if(radio.token_imei) {
		ipc_client_send_get(IPC_MISC_ME_SN, getRequestId(radio.token_imei));
		radio.token_imei = 0;
	}

	if(radio.token_baseband_ver) {
		ipc_client_send_get(IPC_MISC_ME_VERSION, getRequestId(radio.token_baseband_ver));
	}
}

void respondGenPhonRes(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *gen_res = (struct ipc_gen_phone_res*)info->data;
	unsigned short msg_type = ((gen_res->group << 8) | gen_res->type);

	if(msg_type == IPC_SEC_PIN_STATUS) {
		respondSecPinStatus(info);
	} else {
		LOGD("%s: unhandled generic response for msg %04x", __FUNCTION__, msg_type);
	}
}

void onReceive(struct ipc_message_info *info)
{
	/* FIXME: This _needs_ to be moved to each individual function. Unsollicited calls do not have a token! */
	RIL_Token t = getToken(info->aseq);

	switch(IPC_COMMAND(info)) {
		case IPC_PWR_PHONE_PWR_UP:
			/* H1 baseband firmware bug workaround: sleep for 25ms to allow for nvram to initialize */
			usleep(25000);
			respondPowerup();
			break;
		case IPC_MISC_ME_VERSION:
			respondBasebandVersion(info);
			break;
		case IPC_MISC_ME_IMSI:
			respondIMSI(info);
			break;
		case IPC_MISC_ME_SN:
			respondIMEI(t, info->data, info->length);
			break;
		case IPC_MISC_TIME_INFO:
			respondNitz(info->data, info->length);
			break;
		case IPC_NET_CURRENT_PLMN:
			respondOperator(t, info->data, info->length);
			break;
		case IPC_NET_PLMN_LIST:
			respondAvailNetworks(t, info->data, info->length);
			break;
		case IPC_NET_REGIST:
			respondNetRegist(info);
			break;
		case IPC_NET_MODE_SEL:
			respondModeSel(info);
			break;
		case IPC_DISP_ICON_INFO:
			respondSignalStrength(t, info->data, info->length);
			break;
		case IPC_CALL_INCOMING:
			respondCallIncoming(t, info->data, info->length);
			break;
		case IPC_CALL_LIST:
			respondCallList(t, info->data, info->length);
			break;
		case IPC_CALL_STATUS:
			respondCallStatus(t, info->data, info->length);
			break;
		case IPC_SMS_INCOMING_MSG:
			respondSmsIncoming(t, info->data, info->length);
			break;
		case IPC_SEC_PIN_STATUS:
			respondSimStatusChanged(t, info->data, info->length);
			break;
		case IPC_SEC_LOCK_INFO:
			respondLockInfo(info);
			break;
		case IPC_SEC_RSIM_ACCESS:
			respondSecRsimAccess(t, info->data, info->length);
			break;
		case IPC_SEC_PHONE_LOCK:
			respondSecPhoneLock(info);
			break;
		case IPC_SAT_PROACTIVE_CMD:
			respondSatProactiveCmd(info);
			break;
		case IPC_SAT_ENVELOPE_CMD:
			respondSatEnvelopeCmd(info);
			break;
		case IPC_GEN_PHONE_RES:
			respondGenPhonRes(info);
			break;
		default:
			//LOGD("Unknown msgtype: %04x", info->type);
			break;
	}
}

void ipc_log_handler(const char *message, void *user_data)
{
	LOGD("ipc: %s", message);
}

void *init_loop()
{
	struct ipc_message_info resp;

	ipc_client = ipc_client_new(IPC_CLIENT_TYPE_FMT);

	ipc_client_set_log_handler(ipc_client, ipc_log_handler, NULL);

	ipc_client_bootstrap_modem(ipc_client);

	if(ipc_client_open(ipc_client)) {
		LOGE("%s: failed to open ipc client", __FUNCTION__);
		return 0;
	}

	if(ipc_client_power_on(ipc_client)) {
		LOGE("%s: failed to power on ipc client", __FUNCTION__);
		return 0;
	}

	while(1) {
		ipc_client_recv(ipc_client, &resp);

		onReceive(&resp);

		if(resp.data != NULL)
			free(resp.data);
	}

	ipc_client_power_off(ipc_client);
	ipc_client_close(ipc_client);

	ipc_client_free(ipc_client);

	return 0;
}

void start_thread()
{
	pthread_t thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread, &attr, init_loop, NULL);
}

static const RIL_RadioFunctions radio_ops = {
	RIL_VERSION,
	onRequest,
	currentState,
	onSupports,
	onCancel,
	getVersion
};

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
	memset(&radio, 0, sizeof(radio));
	radio.radio_state = RADIO_STATE_OFF;
	radio.token_ps = 0;
	radio.token_cs = 0;

	rilenv = env;

	return &radio_ops;
}

int main(int argc, char *argv[])
{
	return 0;
}

