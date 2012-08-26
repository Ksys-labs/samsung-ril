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

#include <time.h>
#include <pthread.h>

#define LOG_TAG "RIL"
#include <utils/Log.h>
#include <telephony/ril.h>

#include "samsung-ril.h"
#include "util.h"

#define RIL_VERSION_STRING "Samsung RIL"

/**
 * Samsung-RIL TODO:
 *
 * General:
 * - SIM SMS I/O
 * - Review code with requests that produce GEN_PHONE_RES messages to use the GEN_PHONE_RES engine
 *
 * Data-related:
 * - find a reliable way to configure data iface
 * - GPRS: IPC_GPRS_CALL_STATUS, LAST_DATA_CALL_FAIL_CAUSE
 * - check data with orange non-free ril: no gprs_ip_config
 * - data: use IPC_GPRS_CALL_STATUS with global token for possible fail or return anyway and store ip config global?
 * - update global fail cause in global after gprs_call_status, generic error on stored token
 */

/**
 * RIL global vars
 */

struct ril_client *ipc_fmt_client;
struct ril_client *ipc_rfs_client;
struct ril_client *srs_client;

const struct RIL_Env *ril_env;
struct ril_state ril_state;

static pthread_mutex_t ril_mutex = PTHREAD_MUTEX_INITIALIZER; 

static void ril_lock(void) {
	pthread_mutex_lock(&ril_mutex);
}

static void ril_unlock(void) {
	pthread_mutex_unlock(&ril_mutex);
}

/**
 * RIL request token
 */

struct ril_request_token ril_requests_tokens[0x100];
int ril_request_id = 0;

void ril_requests_tokens_init(void)
{
	memset(ril_requests_tokens, 0, sizeof(struct ril_request_token) * 0x100);
}

int ril_request_id_new(void)
{
	ril_request_id++;
	ril_request_id %= 0x100;
	return ril_request_id;
}

int ril_request_reg_id(RIL_Token token)
{
	int id = ril_request_id_new();

	ril_requests_tokens[id].token = token;
	ril_requests_tokens[id].canceled = 0;

	return id;
}

int ril_request_get_id(RIL_Token token)
{
	int i;

	for(i=0 ; i < 0x100 ; i++)
		if(ril_requests_tokens[i].token == token)
			return i;

	// If the token isn't registered yet, register it
	return ril_request_reg_id(token);
}

RIL_Token ril_request_get_token(int id)
{
	return ril_requests_tokens[id].token;
}

int ril_request_get_canceled(RIL_Token token)
{
	int id;

	id = ril_request_get_id(token);

	if(ril_requests_tokens[id].canceled > 0)
		return 1;
	else
		return 0;
}

void ril_request_set_canceled(RIL_Token token, int canceled)
{
	int id;

	id = ril_request_get_id(token);

	ril_requests_tokens[id].canceled = canceled;
}

void RIL_onRequestComplete(RIL_Token t, RIL_Errno e, void *response, size_t responselen)
{
	if(!ril_request_get_canceled(t))
		RIL_onRequestCompleteReal(t, e, response, responselen);
	else
		RIL_onRequestCompleteReal(t, RIL_E_CANCELLED, response, responselen);
}

/**
 * RIL tokens
 */

void ril_tokens_check(void)
{
	RIL_Token t;

	if(ril_state.tokens.baseband_version != 0) {
		if(ril_state.radio_state != RADIO_STATE_OFF) {
			t = ril_state.tokens.baseband_version;
			ril_state.tokens.baseband_version = 0;
			ril_request_baseband_version(t);
		}
	}

	if(ril_state.tokens.get_imei != 0 && ril_state.tokens.get_imeisv != 0) {
		if(ril_state.radio_state != RADIO_STATE_OFF) {
			t = ril_state.tokens.get_imei;
			ril_state.tokens.get_imei = 0;
			ril_request_get_imei(t);
		}
	}
}

/**
 * Clients dispatch functions
 */

void ipc_fmt_dispatch(struct ipc_message_info *info)
{
	ril_lock();
	switch(IPC_COMMAND(info)) {
		/* GEN */
		case IPC_GEN_PHONE_RES:
			ipc_gen_phone_res(info);
			break;
		/* PWR */
		case IPC_PWR_PHONE_PWR_UP:
			ipc_pwr_phone_pwr_up();
			break;
		case IPC_PWR_PHONE_STATE:
			ipc_pwr_phone_state(info);
			break;
		/* DISP */
		case IPC_DISP_ICON_INFO:
			ipc_disp_icon_info(info);
			break;
		case IPC_DISP_RSSI_INFO:
			ipc_disp_rssi_info(info);
			break;
		/* MISC */
		case IPC_MISC_ME_SN:
			ipc_misc_me_sn(info);
			break;
		case IPC_MISC_ME_VERSION:
			ipc_misc_me_version(info);
			break;
		case IPC_MISC_ME_IMSI:
			ipc_misc_me_imsi(info);
			break;
		case IPC_MISC_TIME_INFO:
			ipc_misc_time_info(info);
			break;
		/* SAT */
		case IPC_SAT_PROACTIVE_CMD:
			respondSatProactiveCmd(info);
			break;
		case IPC_SAT_ENVELOPE_CMD:
			respondSatEnvelopeCmd(info);
			break;
		/* SS */
		case IPC_SS_USSD:
			ipc_ss_ussd(info);
			break;
		/* SEC */
		case IPC_SEC_PIN_STATUS:
			ipc_sec_pin_status(info);
			break;
		case IPC_SEC_LOCK_INFO:
			ipc_sec_lock_info(info);
			break;
		case IPC_SEC_RSIM_ACCESS:
			ipc_sec_rsim_access(info);
			break;
		case IPC_SEC_PHONE_LOCK:
			ipc_sec_phone_lock(info);
			break;
		/* NET */
		case IPC_NET_CURRENT_PLMN:
			ipc_net_current_plmn(info);
			break;
		case IPC_NET_REGIST:
			ipc_net_regist(info);
			break;
		case IPC_NET_PLMN_LIST:
			ipc_net_plmn_list(info);
			break;
		case IPC_NET_PLMN_SEL:
			ipc_net_plmn_sel(info);
			break;
		case IPC_NET_MODE_SEL:
			ipc_net_mode_sel(info);
			break;
		/* SMS */
		case IPC_SMS_INCOMING_MSG:
			ipc_sms_incoming_msg(info);
			break;
		case IPC_SMS_DELIVER_REPORT:
			ipc_sms_deliver_report(info);
			break;
		case IPC_SMS_SVC_CENTER_ADDR:
			ipc_sms_svc_center_addr(info);
			break;
		case IPC_SMS_SEND_MSG:
			ipc_sms_send_msg(info);
			break;
		case IPC_SMS_DEVICE_READY:
			ipc_sms_device_ready(info);
			break;
		/* CALL */
		case IPC_CALL_INCOMING:
			ipc_call_incoming(info);
			break;
		case IPC_CALL_LIST:
			ipc_call_list(info);
			break;
		case IPC_CALL_STATUS:
			ipc_call_status(info);
			break;
		case IPC_CALL_BURST_DTMF:
			ipc_call_burst_dtmf(info);
			break;
		/* GPRS */
		case IPC_GPRS_IP_CONFIGURATION:
			ipc_gprs_ip_configuration(info);
			break;
		case IPC_GPRS_CALL_STATUS:
			ipc_gprs_call_status(info);
			break;
		case IPC_GPRS_PDP_CONTEXT:
			ipc_gprs_pdp_context(info);
			break;
		default:
			LOGD("Unhandled command: %s (%04x)", ipc_command_to_str(IPC_COMMAND(info)), IPC_COMMAND(info));
			break;
	}
	ril_unlock();
}

void ipc_rfs_dispatch(struct ipc_message_info *info)
{
	ril_lock();
	switch(IPC_COMMAND(info)) {
		case IPC_RFS_NV_READ_ITEM:
			ipc_rfs_nv_read_item(info);
			break;
		case IPC_RFS_NV_WRITE_ITEM:
			ipc_rfs_nv_write_item(info);
			break;
		default:
			LOGD("Unhandled command: %s (%04x)", ipc_command_to_str(IPC_COMMAND(info)), IPC_COMMAND(info));
			break;
	}
	ril_unlock();
}

void srs_dispatch(int fd, struct srs_message *message)
{
	ril_lock();
	switch(message->command) {
		case SRS_CONTROL_PING:
			srs_control_ping(fd, message);
			break;
		case SRS_SND_SET_CALL_CLOCK_SYNC:
			srs_snd_set_call_clock_sync(message);
			break;
		case SRS_SND_SET_CALL_VOLUME:
			srs_snd_set_call_volume(message);
			break;
		case SRS_SND_SET_CALL_AUDIO_PATH:
			srs_snd_set_call_audio_path(message);
			break;
		default:
			LOGD("Unhandled command: (%04x)", message->command);
			break;
	}
	ril_unlock();
}

/*
 * RIL main dispatch function
 */

int ril_modem_check(void)
{
	if(ipc_fmt_client == NULL)
		return -1;

	if(ipc_fmt_client->state != RIL_CLIENT_READY)
		return -1;

	return 0;
}

void onRequest(int request, void *data, size_t datalen, RIL_Token t)
{
	ril_lock();
	if(ril_modem_check() < 0) {
		RIL_onRequestComplete(t, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
		goto done;
	}

	switch(request) {
		/* PWR */
		case RIL_REQUEST_RADIO_POWER:
			ril_request_radio_power(t, data, datalen);
			break;
		case RIL_REQUEST_BASEBAND_VERSION:
			ril_request_baseband_version(t);
			break;
		/* DISP */
		case RIL_REQUEST_SIGNAL_STRENGTH:
			ril_request_signal_strength(t);
			break;
		/* MISC */
		case RIL_REQUEST_GET_IMEI:
			ril_request_get_imei(t);
			break;
		case RIL_REQUEST_GET_IMEISV:
			ril_request_get_imeisv(t);
			break;
		case RIL_REQUEST_GET_IMSI:
			ril_request_get_imsi(t);
			break;
		/* SAT */
		case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:
			requestSatSendTerminalResponse(t, data, datalen);
			break;
		case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:
			requestSatSendEnvelopeCommand(t, data, datalen);
			break;
		case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:
			RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
			break;
		/* SS */
		case RIL_REQUEST_SEND_USSD:
			ril_request_send_ussd(t, data, datalen);
			break;
		case RIL_REQUEST_CANCEL_USSD:
			ril_request_cancel_ussd(t, data, datalen);
		/* SEC */
		case RIL_REQUEST_GET_SIM_STATUS:
			ril_request_get_sim_status(t);
			break;
		case RIL_REQUEST_SIM_IO:
			ril_request_sim_io(t, data, datalen);
			break;
		case RIL_REQUEST_ENTER_SIM_PIN:
			ril_request_enter_sim_pin(t, data, datalen);
			break;
		case RIL_REQUEST_CHANGE_SIM_PIN:
			ril_request_change_sim_pin(t, data, datalen);
			break;
		case RIL_REQUEST_ENTER_SIM_PUK:
			ril_request_enter_sim_puk(t, data, datalen);
			break;
		case RIL_REQUEST_QUERY_FACILITY_LOCK:
			ril_request_query_facility_lock(t, data, datalen);
			break;
		case RIL_REQUEST_SET_FACILITY_LOCK:
			ril_request_set_facility_lock(t, data, datalen);
			break;
		/* NET */
		case RIL_REQUEST_OPERATOR:
			ril_request_operator(t);
			break;
		case RIL_REQUEST_REGISTRATION_STATE:
			ril_request_registration_state(t);
			break;
		case RIL_REQUEST_GPRS_REGISTRATION_STATE:
			ril_request_gprs_registration_state(t);
			break;
		case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:
			ril_request_query_available_networks(t);
			break;
		case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:
			ril_request_get_preferred_network_type(t);
			break;
		case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
			ril_request_set_preferred_network_type(t, data, datalen);
			break;
		case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
			ril_request_query_network_selection_mode(t);
			break;
		case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
			ril_request_set_network_selection_automatic(t);
			break;
		case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
			ril_request_set_network_selection_manual(t, data, datalen);
			break;
		/* SMS */
		case RIL_REQUEST_SEND_SMS:
			ril_request_send_sms(t, data, datalen);
			break;
		case RIL_REQUEST_SEND_SMS_EXPECT_MORE:
			ril_request_send_sms_expect_more(t, data, datalen);
			break;
		case RIL_REQUEST_SMS_ACKNOWLEDGE:
			ril_request_sms_acknowledge(t, data, datalen);
			break;
		/* CALL */
		case RIL_REQUEST_DIAL:
			ril_request_dial(t, data, datalen);
			break;
		case RIL_REQUEST_GET_CURRENT_CALLS:
			ril_request_get_current_calls(t);
			break;
		case RIL_REQUEST_HANGUP:
		case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
		case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
			ril_request_hangup(t);
			break;
		case RIL_REQUEST_ANSWER:
			ril_request_answer(t);
			break;
		case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:
			ril_request_last_call_fail_cause(t);
			break;
		case RIL_REQUEST_DTMF:
			ril_request_dtmf(t, data, datalen);
                       break;
		case RIL_REQUEST_DTMF_START:
			ril_request_dtmf_start(t, data, datalen);
                       break;
		case RIL_REQUEST_DTMF_STOP:
			ril_request_dtmf_stop(t);
                       break;
		/* GPRS */
		case RIL_REQUEST_SETUP_DATA_CALL:
			ril_request_setup_data_call(t, data, datalen);
			break;
		case RIL_REQUEST_DEACTIVATE_DATA_CALL:
			ril_request_deactivate_data_call(t, data, datalen);
			break;
		case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:
			ril_request_last_data_call_fail_cause(t);
			break;
		case RIL_REQUEST_DATA_CALL_LIST:
			ril_request_data_call_list(t);
			break;
		/* SND */
		case RIL_REQUEST_SET_MUTE:
			ril_request_set_mute(t, data, datalen);
		/* OTHER */
		case RIL_REQUEST_SCREEN_STATE:
			/* This doesn't affect anything */
			RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
			break;
		default:
			LOGE("Request not implemented: %d\n", request);
			RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
			break;
	}
done:
	ril_unlock();
}

/**
 * RILJ related functions
 */

RIL_RadioState currentState()
{
	LOGD("currentState()");
	return ril_state.radio_state;
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
	ril_request_set_canceled(t, 1);
}

const char *getVersion(void)
{
	return RIL_VERSION_STRING;
}

/**
 * RIL init function
 */

void ril_globals_init(void)
{
	memset(&ril_state, 0, sizeof(ril_state));
	memset(&(ril_state.tokens), 0, sizeof(struct ril_tokens));

	ril_requests_tokens_init();
	ipc_gen_phone_res_expects_init();
	ril_gprs_connections_init();
	ril_request_sms_init();
	ipc_sms_tpid_queue_init();
}

void ril_state_lpm(void)
{
	ril_state.radio_state = RADIO_STATE_OFF;
	ril_state.power_mode = POWER_MODE_LPM;
}


static const RIL_RadioFunctions ril_ops = {
	SAMSUNG_RIL_VERSION,
	onRequest,
	currentState,
	onSupports,
	onCancel,
	getVersion
};

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
	int rc;

	ril_env = env;

	LOGD("Creating IPC FMT client");

	ril_lock();
	ipc_fmt_client = ril_client_new(&ipc_fmt_client_funcs);
	rc = ril_client_create(ipc_fmt_client);

	if(rc < 0) {
		LOGE("IPC FMT client creation failed.");
		goto ipc_rfs;
	}

	rc = ril_client_thread_start(ipc_fmt_client);

	if(rc < 0) {
		LOGE("IPC FMT thread creation failed.");
		goto ipc_rfs;
	}

	LOGD("IPC FMT client ready");

ipc_rfs:
	LOGD("Creating IPC RFS client");

	ipc_rfs_client = ril_client_new(&ipc_rfs_client_funcs);
	rc = ril_client_create(ipc_rfs_client);

	if(rc < 0) {
		LOGE("IPC RFS client creation failed.");
		goto srs;
	}

	rc = ril_client_thread_start(ipc_rfs_client);

	if(rc < 0) {
		LOGE("IPC RFS thread creation failed.");
		goto srs;
	}

	LOGD("IPC RFS client ready");

srs:
	LOGD("Creating SRS client");

	srs_client = ril_client_new(&srs_client_funcs);
	rc = ril_client_create(srs_client);

	if(rc < 0) {
		LOGE("SRS client creation failed.");
		goto end;
	}

	rc = ril_client_thread_start(srs_client);

	if(rc < 0) {
		LOGE("SRS thread creation failed.");
		goto end;
	}

	LOGD("SRS client ready");

end:
	ril_globals_init();
	ril_state_lpm();
	ril_unlock();

	return &ril_ops;
}

int main(int argc, char *argv[])
{
	return 0;
}

