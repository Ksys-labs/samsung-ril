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

#define LOG_TAG "RIL-NET"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

#include <plmn_list.h>

/**
 * Format conversion utils
 */

/**
 * Converts IPC network registration status to Android RIL format
 */
unsigned char ipc2ril_reg_state(unsigned char reg_state)
{
	switch(reg_state) {
		case IPC_NET_REGISTRATION_STATE_NONE:
			return 0;
		case IPC_NET_REGISTRATION_STATE_HOME:
			return 1;
		case IPC_NET_REGISTRATION_STATE_SEARCHING:
			return 2;
		case IPC_NET_REGISTRATION_STATE_EMERGENCY:
			return 10;
		case IPC_NET_REGISTRATION_STATE_ROAMING:
			return 5;
		case IPC_NET_REGISTRATION_STATE_UNKNOWN:
			return 4;
		default:
			LOGE("%s: invalid reg_state: %d", __FUNCTION__, reg_state);
			return 0;
	}
}

/**
 * Converts IPC network access technology to Android RIL format
 */
unsigned char ipc2ril_act(unsigned char act)
{
	switch(act) {
		case IPC_NET_ACCESS_TECHNOLOGY_GPRS:
			return 1;
		case IPC_NET_ACCESS_TECHNOLOGY_EDGE:
			return 2;
		case IPC_NET_ACCESS_TECHNOLOGY_UMTS:
			return 3;
		case IPC_NET_ACCESS_TECHNOLOGY_GSM:
		case IPC_NET_ACCESS_TECHNOLOGY_GSM2:
		default:
			return 0;
	}
}

/**
 * Converts IPC GPRS network access technology to Android RIL format
 */
unsigned char ipc2ril_gprs_act(unsigned char act)
{
	switch(act) {
		case IPC_NET_ACCESS_TECHNOLOGY_GPRS:
			return 1;
		case IPC_NET_ACCESS_TECHNOLOGY_EDGE:
			return 2;
		case IPC_NET_ACCESS_TECHNOLOGY_UMTS:
			return 3;
		case IPC_NET_ACCESS_TECHNOLOGY_GSM:
		case IPC_NET_ACCESS_TECHNOLOGY_GSM2:
		default:
			return 0;
	}
}

/**
 * Converts IPC preferred network type to Android RIL format
 */
int ipc2ril_mode_sel(unsigned char mode)
{
	switch(mode) {
		case 0:
			return 7; // auto mode
		case IPC_NET_MODE_SEL_GSM_UMTS:
			return 0;
		case IPC_NET_MODE_SEL_GSM_ONLY:
			return 1;
		case IPC_NET_MODE_SEL_UMTS_ONLY:
			return 2;
		default:
			return 0;
	}
}

/**
 * Converts Android RIL preferred network type to IPC format
 */
unsigned char ril2ipc_mode_sel(int mode)
{
	switch(mode) {
		case 1: // GSM only
			return IPC_NET_MODE_SEL_GSM_ONLY;
		case 2: // WCDMA only
			return IPC_NET_MODE_SEL_UMTS_ONLY;
		case 0:
		default: // GSM/WCDMA + the rest
			return IPC_NET_MODE_SEL_GSM_UMTS;
	}
}

/**
 * Converts IPC preferred PLMN selection type to Android RIL format
 */
int ipc2ril_plmn_sel(unsigned char mode)
{
	switch(mode) {
		case IPC_NET_PLMN_SEL_MANUAL:
			return 1;
		case IPC_NET_PLMN_SEL_AUTO:
			return 0;
		default:
			return 0;
	}
}

/**
 * Converts Android RIL preferred PLMN selection type to IPC format
 */
unsigned char ril2ipc_plmn_sel(int mode)
{
	switch(mode) {
		case 0:
			return IPC_NET_PLMN_SEL_AUTO;
		case 1:
			return IPC_NET_PLMN_SEL_MANUAL;
		default:
			return 0;
	}
}

/**
 * Converts IPC reg state to Android format
 */
void ipc2ril_reg_state_resp(struct ipc_net_regist *netinfo, char *response[15])
{
	unsigned char reg_state = ipc2ril_reg_state(netinfo->reg_state);
	unsigned char act = ipc2ril_act(netinfo->act);

	memset(response, 0, sizeof(response));

	asprintf(&response[0], "%d", reg_state);
	asprintf(&response[1], "%x", netinfo->lac);
	asprintf(&response[2], "%x", netinfo->cid);
	asprintf(&response[3], "%d", act);
}

/**
 * Converts IPC GPRS reg state to Android format
 */
void ipc2ril_gprs_reg_state_resp(struct ipc_net_regist *netinfo, char *response[4])
{
	unsigned char reg_state = ipc2ril_reg_state(netinfo->reg_state);
	unsigned char act = ipc2ril_gprs_act(netinfo->act);

	memset(response, 0, sizeof(response));

	asprintf(&response[0], "%d", reg_state);
	asprintf(&response[1], "%x", netinfo->lac);
	asprintf(&response[2], "%x", netinfo->cid);
	asprintf(&response[3], "%d", act);
}

/**
 * Set all the tokens to data waiting.
 * For instance when only operator is updated by modem NOTI, we don't need
 * to ask the modem new NET Regist and GPRS Net Regist states so act like we got
 * these from modem NOTI too so we don't have to make the requests
 */
void ril_tokens_net_set_data_waiting(void)
{
	ril_state.tokens.registration_state = RIL_TOKEN_DATA_WAITING;
	ril_state.tokens.gprs_registration_state = RIL_TOKEN_DATA_WAITING;
	ril_state.tokens.operator = RIL_TOKEN_DATA_WAITING;
}

/**
 * Returns 1 if unsol data is waiting, 0 if not
 */
int ril_tokens_net_get_data_waiting(void)
{
	return ril_state.tokens.registration_state == RIL_TOKEN_DATA_WAITING || ril_state.tokens.gprs_registration_state == RIL_TOKEN_DATA_WAITING || ril_state.tokens.operator == RIL_TOKEN_DATA_WAITING;
}

/**
 * Print net tokens values
 */
void ril_tokens_net_state_dump(void)
{
	LOGD("ril_tokens_net_state_dump:\n\
	\tril_state.tokens.registration_state = 0x%p\n\
	\tril_state.tokens.gprs_registration_state = 0x%p\n\
	\tril_state.tokens.operator = 0x%p\n", ril_state.tokens.registration_state, ril_state.tokens.gprs_registration_state, ril_state.tokens.operator);
}

void ril_plmn_split(char *plmn_data, char **plmn, unsigned int *mcc, unsigned int *mnc)
{
	char plmn_t[7];
	int i;

	memset(plmn_t, 0, sizeof(plmn_t));
	memcpy(plmn_t, plmn_data, 6);

	if(plmn_t[5] == '#')
		plmn_t[5] = '\0';

	if(plmn != NULL) {
		*plmn = malloc(sizeof(plmn_t));
		memcpy(*plmn, plmn_t, sizeof(plmn_t));
	}

	if(mcc == NULL || mnc == NULL)
		return;

	sscanf(plmn_t, "%3u%2u", mcc, mnc);
}

void ril_plmn_string(char *plmn_data, char *response[3])
{
	unsigned int mcc, mnc;
	char *plmn;

	int plmn_entries;
	int i;

	ril_plmn_split(plmn_data, &plmn, &mcc, &mnc);

	asprintf(&response[2], "%s", plmn);
	free(plmn);

	plmn_entries = sizeof(plmn_list) / sizeof(struct plmn_list_entry);

	LOGD("Found %d plmn records", plmn_entries);

	for(i=0 ; i < plmn_entries ; i++) {
		if(plmn_list[i].mcc == mcc && plmn_list[i].mnc == mnc) {
			asprintf(&response[0], "%s", plmn_list[i].operator_short);
			asprintf(&response[1], "%s", plmn_list[i].operator_long);
			return;	
		}
	}

	response[0] = NULL;
	response[1] = NULL;
}

/**
 * How to handle NET unsol data from modem:
 * 1- Rx UNSOL (NOTI) data from modem
 * 2- copy data in a sized variable stored in radio
 * 3- make sure no SOL request is going on for this token
 * 4- copy data to radio structure
 * 5- if no UNSOL data is already waiting for a token, tell RILJ NETWORK_STATE_CHANGED
 * 6- set all the net tokens to RIL_TOKEN_DATA_WAITING
 * 7- RILJ will ask for OPERATOR, GPRS_REG_STATE and REG_STATE
 * for each request:
 * 8- if token is RIL_TOKEN_DATA_WAITING it's SOL request for modem UNSOL data
 * 9- send back modem data and tell E_SUCCESS to RILJ request
 * 10- set token to 0x00
 *
 * How to handle NET sol requests from RILJ:
 * 1- if token is 0x00 it's UNSOL RILJ request for modem data
 * 2- put RIL_Token in token
 * 3- request data to the modem
 * 4- Rx SOL (RESP) data from modem
 * 5- copy data to radio structure
 * 6- send back data to RILJ with token from modem message
 * 7- if token != RIL_TOKEN_DATA_WAITING, reset token to 0x00
 *
 * What if both are appening at the same time?
 * 1- RILJ requests modem data (UNSOL)
 * 2- token is 0x00 so send request to modem
 * 3- UNSOL data arrives from modem
 * 4- set all tokens to RIL_TOKEN_DATA_WAITING
 * 5- store data, tell RILJ NETWORK_STATE_CHANGED
 * 6- Rx requested data from modem
 * 7- copy data to radio structure
 * 8- token mismatch (is now RIL_TOKEN_DATA_WAITING)
 * 9- send back data to RIL with token from IPC message
 * 10- don't reset token to 0x00
 * 11- RILJ does SOL request for modem data (we know it's SOL because we didn't reset token)
 * 12- send back last data we have (from UNSOL RILJ request here)
 */

/**
 * In: RIL_REQUEST_OPERATOR
 *   Request Operator name
 *
 * Out: IPC_NET_CURRENT_PLMN
 *   return modem UNSOL data if available
 *   request IPC_NET_CURRENT_PLMN if no data is there
 *   return RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW if not registered
 */
void ril_request_operator(RIL_Token t)
{
	char *response[3];
	size_t i;

	// IPC_NET_REGISTRATION_STATE_ROAMING is the biggest valid value
	if(ril_state.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_NONE ||
	ril_state.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_SEARCHING ||
	ril_state.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_UNKNOWN ||
	ril_state.netinfo.reg_state > IPC_NET_REGISTRATION_STATE_ROAMING) {
		RIL_onRequestComplete(t, RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW, NULL, 0);

		ril_state.tokens.operator = (RIL_Token) 0x00;
		return;
	}

	if(ril_state.tokens.operator == RIL_TOKEN_DATA_WAITING) {
		LOGD("Got RILJ request for UNSOL data");

		/* Send back the data we got UNSOL */
		ril_plmn_string(ril_state.plmndata.plmn, response);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

		for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
			if(response[i] != NULL)
				free(response[i]);
		}

		ril_state.tokens.operator = (RIL_Token) 0x00;
	} else if(ril_state.tokens.operator == (RIL_Token) 0x00) {
		LOGD("Got RILJ request for SOL data");
		/* Request data to the modem */
		ril_state.tokens.operator = t;

		ipc_fmt_send_get(IPC_NET_CURRENT_PLMN, reqGetId(t));
	} else {
		LOGE("Another request is going on, returning UNSOL data");

		/* Send back the data we got UNSOL */
		ril_plmn_string(ril_state.plmndata.plmn, response);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

		for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
			if(response[i] != NULL)
				free(response[i]);
		}
	}

	ril_tokens_net_state_dump();
}

/**
 * In: IPC_NET_CURRENT_PLMN
 *   This can be SOL (RESP) or UNSOL (NOTI) message from modem
 *
 * Out: RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED
 *   enqueue modem data if UNSOL modem message and then call
 *   RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED
 *   if SOL message, send back data to RILJ
 */
void ipc_net_current_plmn(struct ipc_message_info *message)
{
	RIL_Token t = reqGetToken(message->aseq);
	struct ipc_net_current_plmn *plmndata = (struct ipc_net_current_plmn *) message->data;

	char *response[3];
	size_t i;

	switch(message->type) {
		case IPC_TYPE_NOTI:
			LOGD("Got UNSOL Operator message");

			// IPC_NET_REGISTRATION_STATE_ROAMING is the biggest valid value
			if(ril_state.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_NONE ||
			ril_state.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_SEARCHING ||
			ril_state.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_UNKNOWN ||
			ril_state.netinfo.reg_state > IPC_NET_REGISTRATION_STATE_ROAMING) {
				/* Better keeping it up to date */
				memcpy(&(ril_state.plmndata), plmndata, sizeof(struct ipc_net_current_plmn));

				return;
			} else {
				if(ril_state.tokens.operator != (RIL_Token) 0x00 && ril_state.tokens.operator != RIL_TOKEN_DATA_WAITING) {
					LOGE("Another Operator Req is in progress, skipping");
					return;
				}

				memcpy(&(ril_state.plmndata), plmndata, sizeof(struct ipc_net_current_plmn));

				/* we already told RILJ to get the new data but it wasn't done yet */
				if(ril_tokens_net_get_data_waiting() && ril_state.tokens.operator == RIL_TOKEN_DATA_WAITING) {
					LOGD("Updating Operator data in background");
				} else {
					ril_tokens_net_set_data_waiting();
					RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
				}
			}
			break;
		case IPC_TYPE_RESP:
			LOGD("Got SOL Operator message");

			// IPC_NET_REGISTRATION_STATE_ROAMING is the biggest valid value
			if(ril_state.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_NONE ||
			ril_state.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_SEARCHING ||
			ril_state.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_UNKNOWN ||
			ril_state.netinfo.reg_state > IPC_NET_REGISTRATION_STATE_ROAMING) {
				/* Better keeping it up to date */
				memcpy(&(ril_state.plmndata), plmndata, sizeof(struct ipc_net_current_plmn));

				RIL_onRequestComplete(t, RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW, NULL, 0);

				if(ril_state.tokens.operator != RIL_TOKEN_DATA_WAITING)
					ril_state.tokens.operator = (RIL_Token) 0x00;
				return;
			} else {
				if(ril_state.tokens.operator != t)
					LOGE("Operator tokens mismatch");

				/* Better keeping it up to date */
				memcpy(&(ril_state.plmndata), plmndata, sizeof(struct ipc_net_current_plmn));

				ril_plmn_string(plmndata->plmn, response);

				RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

				for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
					if(response[i] != NULL)
						free(response[i]);
				}

				if(ril_state.tokens.operator != RIL_TOKEN_DATA_WAITING)
					ril_state.tokens.operator = (RIL_Token) 0x00;
			}
			break;
		default:
			LOGE("%s: unhandled ipc method: %d", __FUNCTION__, message->type);
			break;
	}

	ril_tokens_net_state_dump();
}

/**
 * In: RIL_REQUEST_REGISTRATION_STATE
 *   Request reg state
 *
 * Out: IPC_NET_REGIST
 *   return modem UNSOL data if available
 *   request IPC_NET_REGIST if no data is there
 */
void ril_request_registration_state(RIL_Token t)
{
	struct ipc_net_regist_get regist_req;
	char *response[4];
	int i;

	if(ril_state.tokens.registration_state == RIL_TOKEN_DATA_WAITING) {
		LOGD("Got RILJ request for UNSOL data");

		/* Send back the data we got UNSOL */
		ipc2ril_reg_state_resp(&(ril_state.netinfo), response);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

		for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
			if(response[i] != NULL)
				free(response[i]);
		}

		ril_state.tokens.registration_state = (RIL_Token) 0x00;
	} else if(ril_state.tokens.registration_state == (RIL_Token) 0x00) {
		LOGD("Got RILJ request for SOL data");
		/* Request data to the modem */
		ril_state.tokens.registration_state = t;

		ipc_net_regist_setup(&regist_req, IPC_NET_SERVICE_DOMAIN_GSM);
		ipc_fmt_send(IPC_NET_REGIST, IPC_TYPE_GET, (void *)&regist_req, sizeof(struct ipc_net_regist_get), reqGetId(t));
	} else {
		LOGE("Another request is going on, returning UNSOL data");

		/* Send back the data we got UNSOL */
		ipc2ril_reg_state_resp(&(ril_state.netinfo), response);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

		for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
			if(response[i] != NULL)
				free(response[i]);
		}
	}

	ril_tokens_net_state_dump();
}

/**
 * In: RIL_REQUEST_GPRS_REGISTRATION_STATE
 *   Request GPRS reg state
 *
 * Out: IPC_NET_REGIST
 *   return modem UNSOL data if available
 *   request IPC_NET_REGIST if no data is there
 */
void ril_request_gprs_registration_state(RIL_Token t)
{
	struct ipc_net_regist_get regist_req;
	char *response[4];
	size_t i;

	if(ril_state.tokens.gprs_registration_state == RIL_TOKEN_DATA_WAITING) {
		LOGD("Got RILJ request for UNSOL data");

		/* Send back the data we got UNSOL */
		ipc2ril_gprs_reg_state_resp(&(ril_state.gprs_netinfo), response);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

		for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
			if(response[i] != NULL)
				free(response[i]);
		}

		ril_state.tokens.gprs_registration_state = (RIL_Token) 0x00;
	} else if(ril_state.tokens.gprs_registration_state == (RIL_Token) 0x00) {
		LOGD("Got RILJ request for SOL data");

		/* Request data to the modem */
		ril_state.tokens.gprs_registration_state = t;

		ipc_net_regist_setup(&regist_req, IPC_NET_SERVICE_DOMAIN_GPRS);
		ipc_fmt_send(IPC_NET_REGIST, IPC_TYPE_GET, (void *)&regist_req, sizeof(struct ipc_net_regist_get), reqGetId(t));
	} else {
		LOGE("Another request is going on, returning UNSOL data");

		/* Send back the data we got UNSOL */
		ipc2ril_gprs_reg_state_resp(&(ril_state.gprs_netinfo), response);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

		for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
			if(response[i] != NULL)
				free(response[i]);
		}
	}

	ril_tokens_net_state_dump();
}

void ipc_net_regist_unsol(struct ipc_message_info *message)
{
	struct ipc_net_regist *netinfo;
	netinfo = (struct ipc_net_regist *) message->data;

	LOGD("Got UNSOL NetRegist message");

	switch(netinfo->domain) {
		case IPC_NET_SERVICE_DOMAIN_GSM:
			if(ril_state.tokens.registration_state != (RIL_Token) 0 && ril_state.tokens.registration_state != RIL_TOKEN_DATA_WAITING) {
				LOGE("Another NetRegist Req is in progress, skipping");
				return;
			}

			memcpy(&(ril_state.netinfo), netinfo, sizeof(struct ipc_net_regist));

			/* we already told RILJ to get the new data but it wasn't done yet */
			if(ril_tokens_net_get_data_waiting() && ril_state.tokens.registration_state == RIL_TOKEN_DATA_WAITING) {
				LOGD("Updating NetRegist data in background");
			} else {
				ril_tokens_net_set_data_waiting();
				RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
			}
			break;

		case IPC_NET_SERVICE_DOMAIN_GPRS:
			if(ril_state.tokens.gprs_registration_state != (RIL_Token) 0 && ril_state.tokens.gprs_registration_state != RIL_TOKEN_DATA_WAITING) {
				LOGE("Another GPRS NetRegist Req is in progress, skipping");
				return;
			}

			memcpy(&(ril_state.gprs_netinfo), netinfo, sizeof(struct ipc_net_regist));

			/* we already told RILJ to get the new data but it wasn't done yet */
			if(ril_tokens_net_get_data_waiting() && ril_state.tokens.gprs_registration_state == RIL_TOKEN_DATA_WAITING) {
				LOGD("Updating GPRSNetRegist data in background");
			} else {
				ril_tokens_net_set_data_waiting();
				RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
			}
			break;
		default:
			LOGE("%s: unhandled service domain: %d", __FUNCTION__, netinfo->domain);
			break;
	}

	ril_tokens_net_state_dump();
}

void ipc_net_regist_sol(struct ipc_message_info *message)
{
	char *response[4];
	size_t i;

	struct ipc_net_regist *netinfo = (struct ipc_net_regist *) message->data;
	RIL_Token t = reqGetToken(message->aseq);

	LOGD("Got SOL NetRegist message");

	switch(netinfo->domain) {
		case IPC_NET_SERVICE_DOMAIN_GSM:
			if(ril_state.tokens.registration_state != t)
				LOGE("Registration state tokens mismatch");

			/* Better keeping it up to date */
			memcpy(&(ril_state.netinfo), netinfo, sizeof(struct ipc_net_regist));

			ipc2ril_reg_state_resp(netinfo, response);

			RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

			for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
				if(response[i] != NULL)
					free(response[i]);
			}

			if(ril_state.tokens.registration_state != RIL_TOKEN_DATA_WAITING)
				ril_state.tokens.registration_state = (RIL_Token) 0x00;
			break;
		case IPC_NET_SERVICE_DOMAIN_GPRS:
			if(ril_state.tokens.gprs_registration_state != t)
				LOGE("GPRS registration state tokens mismatch");

			/* Better keeping it up to date */
			memcpy(&(ril_state.gprs_netinfo), netinfo, sizeof(struct ipc_net_regist));

			ipc2ril_gprs_reg_state_resp(netinfo, response);

			RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

			for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
				if(response[i] != NULL)
					free(response[i]);
			}
			if(ril_state.tokens.registration_state != RIL_TOKEN_DATA_WAITING)
				ril_state.tokens.gprs_registration_state = (RIL_Token) 0x00;
			break;
		default:
			LOGE("%s: unhandled service domain: %d", __FUNCTION__, netinfo->domain);
			break;
	}

	ril_tokens_net_state_dump();
}

/**
 * In: IPC_NET_REGIST
 *   This can be SOL (RESP) or UNSOL (NOTI) message from modem
 */
void ipc_net_regist(struct ipc_message_info *message)
{
	/* Don't consider this if modem isn't in normal power mode. */
	if(ril_state.power_mode < POWER_MODE_NORMAL)
		return;

	switch(message->type) {
		case IPC_TYPE_NOTI:
			ipc_net_regist_unsol(message);
			break;
		case IPC_TYPE_RESP:
			ipc_net_regist_sol(message);
			break;
		default:
			LOGE("%s: unhandled ipc method: %d", __FUNCTION__, message->type);
			break;
	}

}

/**
 * In: RIL_REQUEST_QUERY_AVAILABLE_NETWORKS
 *
 * Out: IPC_NET_PLMN_LIST
 */
void ril_request_query_available_networks(RIL_Token t)
{
	ipc_fmt_send_get(IPC_NET_PLMN_LIST, reqGetId(t));
}

/* FIXME: cleanup struct names & resp[] addressing */
/**
 * In: IPC_NET_PLMN_LIST
 * Send back available PLMN list
 *
 */
void ipc_net_plmn_list(struct ipc_message_info *info)
{
	struct ipc_net_plmn_entries *entries_info = (struct ipc_net_plmn_entries *) info->data;
	struct ipc_net_plmn_entry *entries = (struct ipc_net_plmn_entry *)
		(info->data + sizeof(struct ipc_net_plmn_entries));

	int i;
	int size = (4 * entries_info->num * sizeof(char*));
	int actual_size = 0;

	char **resp = malloc(size);
	char **resp_ptr = resp;

	LOGD("Listed %d PLMNs\n", entries_info->num);

	for(i = 0; i < entries_info->num; i++) {
		/* Assumed type for 'emergency only' PLMNs */
		if(entries[i].type == 0x01)
			continue;

		ril_plmn_string(entries[i].plmn, resp_ptr);

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

	RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_SUCCESS, resp, (4 * sizeof(char*) * actual_size));

	/* FIXME: free individual strings */
	free(resp);
}

void ril_request_get_preferred_network_type(RIL_Token t)
{
	ipc_fmt_send_get(IPC_NET_MODE_SEL, reqGetId(t));
}

void ril_request_set_preferred_network_type(RIL_Token t, void *data, size_t datalen)
{
	int ril_mode = *(int*)data;
	struct ipc_net_mode_sel mode_sel;

	mode_sel.mode_sel = ril2ipc_mode_sel(ril_mode);

	ipc_gen_phone_res_expect_to_complete(reqGetId(t), IPC_NET_MODE_SEL);

	ipc_fmt_send(IPC_NET_MODE_SEL, IPC_TYPE_SET, &mode_sel, sizeof(mode_sel), reqGetId(t));
}

void ipc_net_mode_sel(struct ipc_message_info *info)
{
	struct ipc_net_mode_sel *mode_sel = (struct ipc_net_mode_sel *) info->data;
	int ril_mode = ipc2ril_mode_sel(mode_sel->mode_sel);

	RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_SUCCESS, &ril_mode, sizeof(int));
}


void ril_request_query_network_selection_mode(RIL_Token t)
{
	ipc_fmt_send_get(IPC_NET_PLMN_SEL, reqGetId(t));
}

void ipc_net_plmn_sel(struct ipc_message_info *info)
{
	struct ipc_net_plmn_sel_get *plmn_sel;
	int ril_mode;

	if (!info)
		return;
	
	if (!info->data || info->length < sizeof(struct ipc_net_plmn_sel_get))
		return;
	
	plmn_sel = (struct ipc_net_plmn_sel_get *) info->data;
	ril_mode = ipc2ril_plmn_sel(plmn_sel->plmn_sel);
	RIL_onRequestComplete(reqGetToken(info->aseq),
		RIL_E_SUCCESS, &ril_mode, sizeof(int));
}

void ipc_net_plmn_sel_complete(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *phone_res = (struct ipc_gen_phone_res *) info->data;
	int rc;

	rc = ipc_gen_phone_res_check(phone_res);
	if(rc < 0) {
		if((phone_res->code & 0x00ff) == 0x6f) {
			LOGE("Not authorized to register to this network!");
			RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_ILLEGAL_SIM_OR_ME, NULL, 0);
		} else {
			LOGE("There was an error during operator selection!");
			RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
		}
		return;
	}

	RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_SUCCESS, NULL, 0);
}

void ril_request_set_network_selection_automatic(RIL_Token t)
{
	struct ipc_net_plmn_sel_set plmn_sel;

	ipc_net_plmn_sel_setup(&plmn_sel, IPC_NET_PLMN_SEL_AUTO, NULL, IPC_NET_ACCESS_TECHNOLOGY_UNKNOWN);

	ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_NET_PLMN_SEL, ipc_net_plmn_sel_complete);

	ipc_fmt_send(IPC_NET_PLMN_SEL, IPC_TYPE_SET, &plmn_sel, sizeof(plmn_sel), reqGetId(t));
}

void ril_request_set_network_selection_manual(RIL_Token t, void *data, size_t datalen)
{
	struct ipc_net_plmn_sel_set plmn_sel;

	// FIXME: We always assume UMTS capability
	ipc_net_plmn_sel_setup(&plmn_sel, IPC_NET_PLMN_SEL_MANUAL, data, IPC_NET_ACCESS_TECHNOLOGY_UMTS);

	ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_NET_PLMN_SEL, ipc_net_plmn_sel_complete);

	ipc_fmt_send(IPC_NET_PLMN_SEL, IPC_TYPE_SET, &plmn_sel, sizeof(plmn_sel), reqGetId(t));
}
