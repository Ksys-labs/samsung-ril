#include <time.h>

#include <telephony/ril.h>

#include <samsung-ril-socket.h>
#include <radio.h>
#include <util.h>

#define LOG_TAG "RIL"
#include <utils/Log.h>

#include "socket.h"
#include "samsung-ril.h"
#include "util.h"

#define RIL_VERSION_STRING "Samsung RIL"

const struct RIL_Env *rilenv;
struct radio_state radio;

struct ipc_client *ipc_client;
struct srs_server *srs_server;
int client_fmt_fd = -1;

/*
 * Token-related function
 */

/* Tokens associated with requests */
RIL_Token request_ids[256];
int rid = 0;

void read_loop_thread();

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

void RadioTokensCheck(void)
{
	if(radio.tokens.baseband_version != 0) {
		if(radio.radio_state != RADIO_STATE_OFF) {
			requestBasebandVersion(radio.tokens.baseband_version);
			radio.tokens.baseband_version = 0;
		}
	}

	if(radio.tokens.get_imei != 0) {
		if(radio.radio_state != RADIO_STATE_OFF) {
			requestIMEI(radio.tokens.get_imei);
			radio.tokens.get_imei = 0;
		}
	}
}

/*
 * RILJ (RIL to modem) related functions 
 */

void onRequest(int request, void *data, size_t datalen, RIL_Token t)
{
	//LOGD("%s: start %d %08X", __FUNCTION__, request, t);
/*
	if(radio.tokens.radio_power != 0) {
		RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
		return;
	}
*/
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
		case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
		case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
			requestHangup(t);
			break;
		case RIL_REQUEST_ANSWER:
			requestAnswer(t);
			break;
		case RIL_REQUEST_SEND_SMS:
			requestSendSms(t, data, datalen);
			break;
		case RIL_REQUEST_SEND_SMS_EXPECT_MORE:
			requestSendSmsExpectMore(t, data, datalen);
			break;
		case RIL_REQUEST_SMS_ACKNOWLEDGE:
			requestSmsAcknowledge(t, data, datalen);
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
		case RIL_REQUEST_SCREEN_STATE:
			/* This doesn't affect anything */
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

/**
 * libsamsung-ipc (modem to RIL) related functions
 */

void onReceive(struct ipc_message_info *info)
{
	/* FIXME: This _needs_ to be moved to each individual function. Unsollicited calls do not have a token! */
	RIL_Token t = getToken(info->aseq);

	// TODO: add IPC_NET_SERVING_NETWORK

	switch(IPC_COMMAND(info)) {
		case IPC_PWR_PHONE_PWR_UP:
			respondPowerUp();
			break;
		case IPC_PWR_PHONE_STATE:
			respondPowerPhoneState(info);
			break;
		case IPC_MISC_ME_VERSION:
			respondBasebandVersion(info);
			break;
		case IPC_MISC_ME_IMSI:
			respondIMSI(info);
			break;
		case IPC_MISC_ME_SN:
			respondMeSn(t, info->data, info->length);
			break;
		case IPC_MISC_TIME_INFO:
			respondNitz(info->data, info->length);
			break;
		case IPC_NET_CURRENT_PLMN:
			respondOperator(info);
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
			respondIconSignalStrength(t, info->data, info->length);
			break;
		case IPC_DISP_RSSI_INFO:
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
		case IPC_SMS_DELIVER_REPORT:
			respondSmsDeliverReport(t, info->data, info->length);
			break;
		case IPC_SMS_SVC_CENTER_ADDR:
			respondSmsSvcCenterAddr(t, info->data, info->length);
			break;
		case IPC_SMS_SEND_MSG:
			respondSmsSendMsg(t, info->data, info->length);
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
		case IPC_SMS_DEVICE_READY:
			respondSmsDeviceReady(t, info);
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

void socketRespondGetHelo(struct srs_message *message)
{
	int helo = SRS_CONTROL_HELO;
	srs_server_send(srs_server, SRS_CONTROL_GET_HELO, &helo, sizeof(helo), message->msg_id);
}

void socketRespondLinkClose(struct srs_message *message)
{
	close(srs_server->client_fd);
	srs_server->client_fd = -1;
}

void socketRespondSetCallClockSync(struct srs_message *message)
{
	unsigned char data = *((unsigned char *) message->data);
	LOGE("SetCallClockSync data is 0x%x\n", data);

	ipc_client_send(ipc_client, IPC_SND_CLOCK_CTRL, IPC_TYPE_EXEC, &data, sizeof(data), 0xff);
}

void onSocketReceive(struct srs_message *message)
{
	switch(message->command) {
		case SRS_CONTROL_GET_HELO:
			socketRespondGetHelo(message);
			break;
		case SRS_CONTROL_LINK_CLOSE:
			socketRespondLinkClose(message);
			break;
		case SRS_SND_SET_CALL_CLOCK_SYNC:
			socketRespondSetCallClockSync(message);
			break;
	}
}

void *socket_loop()
{
	struct srs_message srs_message;
	fd_set fds;
	int rc;

	while(1) {
		rc = srs_server_accept(srs_server);

		LOGE("SRS server accept!");

		FD_ZERO(&fds);
		FD_SET(srs_server->client_fd, &fds);

		while(1) {
			usleep(300);

			if(srs_server->client_fd < 0)
				break;

			select(FD_SETSIZE, &fds, NULL, NULL, NULL);

			if(FD_ISSET(srs_server->client_fd, &fds)) {
				if(srs_server_recv(srs_server, &srs_message) < 0) {
					LOGE("SRS server RECV failure!!!");
					break;
				}

				LOGE("SRS OBTAINED DATA DUMP == %d == %d ======", srs_message.command, srs_message.data_len);
				ipc_hex_dump(ipc_client, srs_message.data, srs_message.data_len);

				onSocketReceive(&srs_message);

				if(srs_message.data != NULL) //check on datalen
					free(srs_message.data);
			}
		}

		if(srs_server->client_fd > 0) {
			close(srs_server->client_fd);
			srs_server->client_fd = -1;
		}

		LOGE("SRS server client ended!");
	}

	return 0;
}

/**
 * read_loop():
 * This function is the main RIL read loop
 */
void *read_loop()
{
	struct ipc_message_info resp;
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(client_fmt_fd, &fds);

	while(1) {
		usleep(300);

		select(FD_SETSIZE, &fds, NULL, NULL, NULL);

		if(FD_ISSET(client_fmt_fd, &fds)) {
			if(ipc_client_recv(ipc_client, &resp)) {
				LOGE("IPC RECV failure!!!");
				break;
			}
			LOGD("RECV aseq=0x%x mseq=0x%x data_length=%d\n", resp.aseq, resp.mseq, resp.length);

			onReceive(&resp);

			if(resp.data != NULL)
				free(resp.data);
		}
	}

	ipc_client_power_off(ipc_client);
	ipc_client_close(ipc_client);
	ipc_client_free(ipc_client);

	return 0;
}

void radio_init_tokens(void)
{
	memset(&(radio.tokens), 0, sizeof(struct ril_tokens));
}

void radio_init_lpm(void)
{
	memset(&radio, 0, sizeof(radio));
	radio.radio_state = RADIO_STATE_OFF;
	radio.power_mode = POWER_MODE_LPM;
}

void socket_loop_thread()
{
	pthread_t thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread, &attr, socket_loop, NULL);
}

void read_loop_thread()
{
	pthread_t thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread, &attr, read_loop, NULL);
}

/**
 * RIL_Init function
 */

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
	radio_init_lpm();
	radio_init_tokens();

	rilenv = env;

	LOGD("Creating new FMT client");
	ipc_client = ipc_client_new(IPC_CLIENT_TYPE_FMT);

	ipc_client_set_log_handler(ipc_client, ipc_log_handler, NULL);

	ipc_client_bootstrap_modem(ipc_client);

	LOGD("All handlers data is %p", &client_fmt_fd);
	ipc_client_set_all_handlers_data(ipc_client, &client_fmt_fd);

	LOGD("Client open...");
	if(ipc_client_open(ipc_client)) {
		LOGE("%s: failed to open ipc client", __FUNCTION__);
		return 0;
	}

	if(client_fmt_fd < 0) {
		LOGE("%s: client_fmt_fd is negative, aborting", __FUNCTION__);
		return 0;
	}

	LOGD("Client power on...");
	if(ipc_client_power_on(ipc_client)) {
		LOGE("%s: failed to power on ipc client", __FUNCTION__);
		return 0;
	}

	srs_server = srs_server_new();
	if(srs_server_open(srs_server) < 0) {
		LOGE("%s: samsung-ril-socket server open failed", __FUNCTION__);
		return 0;
	}

	read_loop_thread();
	socket_loop_thread();

	return &radio_ops;
}

int main(int argc, char *argv[])
{
	return 0;
}

