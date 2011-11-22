#define LOG_TAG "RIL-NET"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

#define RIL_TOKEN_NET_DATA_WAITING	(RIL_Token) 0xff

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

/**
 *  NET Utility functions
 */

/**
 * Converts IPC network registration status to Android RIL format
 */
unsigned char reg_state_ipc2ril(unsigned char reg_state)
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
			return 255;
	}
}

/**
 * Converts IPC network access technology to Android RIL format
 */
unsigned char act_ipc2ril(unsigned char act)
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
unsigned char gprs_act_ipc2ril(unsigned char act)
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
unsigned char modesel_ipc2ril(unsigned char mode)
{
	switch(mode) {
		case 0:
			return 7;
		case 1:
		case 3:
			return 1;
		case 2:
		case 4:
			return 2;
		default:
			return 255;
	}
}

/**
 * Converts Android RIL preferred network type to IPC format
 */
unsigned char modesel_ril2ipc(unsigned char mode)
{
	switch(mode) {
		case 1:
			return 2;
		case 2:
			return 3;
		default:
			return 1;
	}
}

/**
 * Converts IPC PLMC to Android format
 */
void plmn_ipc2ril(struct ipc_net_current_plmn *plmndata, char *response[3])
{
	char plmn[7];

	memset(plmn, 0, sizeof(plmn));

	memcpy(plmn, plmndata->plmn, 6);

	if(plmn[5] == '#')
		plmn[5] = '\0'; //FIXME: remove #?

	asprintf(&response[0], "%s", plmn_lookup(plmn));
	//asprintf(&response[1], "%s", "Voda NL");
	response[1] = NULL;
	asprintf(&response[2], "%s", plmn);
}

/**
 * Converts IPC reg state to Android format
 */
void reg_state_resp_ipc2ril(struct ipc_net_regist *netinfo, char *response[15])
{
	memset(response, 0, sizeof(response));

	asprintf(&response[0], "%d", reg_state_ipc2ril(netinfo->reg_state));
	asprintf(&response[1], "%x", netinfo->lac);
	asprintf(&response[2], "%x", netinfo->cid);
	asprintf(&response[3], "%d", act_ipc2ril(netinfo->act));
}

/**
 * Converts IPC GPRS reg state to Android format
 */
void gprs_reg_state_resp_ipc2ril(struct ipc_net_regist *netinfo, char *response[4])
{
	memset(response, 0, sizeof(response));

	asprintf(&response[0], "%d", reg_state_ipc2ril(netinfo->reg_state));
	asprintf(&response[1], "%x", netinfo->lac);
	asprintf(&response[2], "%x", netinfo->cid);
	asprintf(&response[3], "%d", gprs_act_ipc2ril(netinfo->act));
}

/**
 * Set all the tokens to data waiting.
 * For instance when only operator is updated by modem NOTI, we don't need
 * to ask the modem new NET Regist and GPRS Net Regist states so act like we got
 * these from modem NOTI too so we don't have to make the requests
 */
void net_set_tokens_data_waiting(void)
{
	radio.tokens.registration_state = RIL_TOKEN_NET_DATA_WAITING;
	radio.tokens.gprs_registration_state = RIL_TOKEN_NET_DATA_WAITING;
	radio.tokens.operator = RIL_TOKEN_NET_DATA_WAITING;
}

/**
 * Returns 1 if unsol data is waiting, 0 if not
 */
int net_get_tokens_data_waiting(void)
{
	return radio.tokens.registration_state == RIL_TOKEN_NET_DATA_WAITING || radio.tokens.gprs_registration_state == RIL_TOKEN_NET_DATA_WAITING || radio.tokens.operator == RIL_TOKEN_NET_DATA_WAITING;
}

/**
 * Print net tokens values
 */
void net_tokens_state_dump(void)
{
	LOGD("NET_TOKENS_STATE_DUMP:\n\tradio.tokens.registration_state = 0x%x\n\tradio.tokens.gprs_registration_state = 0x%x\n\tradio.tokens.operator = 0x%x\n", radio.tokens.registration_state, radio.tokens.gprs_registration_state, radio.tokens.operator);
}

/**
 * How to handle NET unsol data from modem:
 * 1- Rx UNSOL (NOTI) data from modem
 * 2- copy data in a sized variable stored in radio
 * 3- make sure no SOL request is going on for this token
 * 4- copy data to radio structure
 * 5- if no UNSOL data is already waiting for a token, tell RILJ NETWORK_STATE_CHANGED
 * 6- set all the net tokens to RIL_TOKEN_NET_DATA_WAITING
 * 7- RILJ will ask for OPERATOR, GPRS_REG_STATE and REG_STATE
 * for each request: 
 * 8- if token is RIL_TOKEN_NET_DATA_WAITING it's SOL request for modem UNSOL data
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
 * 7- if token != RIL_TOKEN_NET_DATA_WAITING, reset token to 0x00
 * 
 * What if both are appening at the same time?
 * 1- RILJ requests modem data (UNSOL)
 * 2- token is 0x00 so send request to modem
 * 3- UNSOL data arrives from modem
 * 4- set all tokens to RIL_TOKEN_NET_DATA_WAITING
 * 5- store data, tell RILJ NETWORK_STATE_CHANGED
 * 6- Rx requested data from modem
 * 7- copy data to radio structure
 * 8- token mismatch (is now RIL_TOKEN_NET_DATA_WAITING)
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
void requestOperator(RIL_Token t)
{
	char *response[3];
	int i;

	if(radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_EMERGENCY ||
	radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_NONE ||
	radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_SEARCHING ||
	radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_UNKNOWN) {
		RIL_onRequestComplete(t, RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW, NULL, 0);

		radio.tokens.operator = (RIL_Token) 0x00;
		return;
	}

	if(radio.tokens.operator == RIL_TOKEN_NET_DATA_WAITING) {
		LOGD("Got RILJ request for UNSOL data");

		/* Send back the data we got UNSOL */
		plmn_ipc2ril(&(radio.plmndata), response);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

		for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
			if(response[i] != NULL)
				free(response[i]);
		}

		radio.tokens.operator = (RIL_Token) 0x00;
	} else if(radio.tokens.operator == (RIL_Token) 0x00) {
		LOGD("Got RILJ request for SOL data");
		/* Request data to the modem */
		radio.tokens.operator = t;

		ipc_client_send_get(IPC_NET_CURRENT_PLMN, getRequestId(t));
	} else {
		LOGE("Another request is going on, reporting failure");
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, response, sizeof(response));
	}

	net_tokens_state_dump();
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
void respondOperator(struct ipc_message_info *message)
{
	RIL_Token t = getToken(message->aseq);
	struct ipc_net_current_plmn *plmndata = (struct ipc_net_current_plmn *) message->data;

	char *response[3];
	int i;

	switch(message->type) {
		case IPC_TYPE_NOTI:
			LOGD("Got UNSOL Operator message");

			if(radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_EMERGENCY ||
			radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_NONE ||
			radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_SEARCHING ||
			radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_UNKNOWN) {
				/* Better keeping it up to date */
				memcpy(&(radio.plmndata), plmndata, sizeof(struct ipc_net_current_plmn));

				return;
			} else {
				if(radio.tokens.operator != (RIL_Token) 0x00 && radio.tokens.operator != RIL_TOKEN_NET_DATA_WAITING) {
					LOGE("Another Operator Req is in progress, skipping");
					return;
				}

				memcpy(&(radio.plmndata), plmndata, sizeof(struct ipc_net_current_plmn));

				/* we already told RILJ to get the new data but it wasn't done yet */
				if(net_get_tokens_data_waiting() && radio.tokens.operator == RIL_TOKEN_NET_DATA_WAITING) {
					LOGD("Updating Operator data in background");
				} else {
					net_set_tokens_data_waiting();
					RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
				}
			}
			break;
		case IPC_TYPE_RESP:
			if(radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_EMERGENCY ||
			radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_NONE ||
			radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_SEARCHING ||
			radio.netinfo.reg_state == IPC_NET_REGISTRATION_STATE_UNKNOWN) {
				/* Better keeping it up to date */
				memcpy(&(radio.plmndata), plmndata, sizeof(struct ipc_net_current_plmn));

				RIL_onRequestComplete(t, RIL_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW, NULL, 0);

				if(radio.tokens.operator != RIL_TOKEN_NET_DATA_WAITING)
					radio.tokens.operator = (RIL_Token) 0x00;
				return;
			} else {
				if(radio.tokens.operator != t)
					LOGE("Operator tokens mismatch");

				/* Better keeping it up to date */
				memcpy(&(radio.plmndata), plmndata, sizeof(struct ipc_net_current_plmn));

				plmn_ipc2ril(plmndata, response);

				RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

				for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
					if(response[i] != NULL)
						free(response[i]);
				}

				if(radio.tokens.operator != RIL_TOKEN_NET_DATA_WAITING)
					radio.tokens.operator = (RIL_Token) 0x00;
			}
			break;
		default:
			LOGE("%s: unhandled ipc method: %d", __FUNCTION__, message->type);
			break;
	}

	net_tokens_state_dump();
}

/**
 * In: RIL_REQUEST_REGISTRATION_STATE
 *   Request reg state
 *
 * Out: IPC_NET_REGIST
 *   return modem UNSOL data if available
 *   request IPC_NET_REGIST if no data is there
 */
void requestRegistrationState(RIL_Token t)
{
	struct ipc_net_regist_get regist_req;
	char *response[4];
	int i;

	if(radio.tokens.registration_state == RIL_TOKEN_NET_DATA_WAITING) {
		LOGD("Got RILJ request for UNSOL data");

		/* Send back the data we got UNSOL */
		reg_state_resp_ipc2ril(&(radio.netinfo), response);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

		for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
			if(response[i] != NULL)
				free(response[i]);
		}

		radio.tokens.registration_state = (RIL_Token) 0x00;
	} else if(radio.tokens.registration_state == (RIL_Token) 0x00) {
		LOGD("Got RILJ request for SOL data");
		/* Request data to the modem */
		radio.tokens.registration_state = t;

		ipc_net_regist_get(&regist_req, IPC_NET_SERVICE_DOMAIN_GSM);
		ipc_client_send(ipc_client, IPC_NET_REGIST, IPC_TYPE_GET, (void *)&regist_req, sizeof(struct ipc_net_regist_get), getRequestId(t));
	} else {
		LOGE("Another request is going on, reporting failure");
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	net_tokens_state_dump();
}

/**
 * In: RIL_REQUEST_GPRS_REGISTRATION_STATE
 *   Request GPRS reg state
 *
 * Out: IPC_NET_REGIST
 *   return modem UNSOL data if available
 *   request IPC_NET_REGIST if no data is there
 */
void requestGPRSRegistrationState(RIL_Token t)
{
	struct ipc_net_regist_get regist_req;
	char *response[4];
	int i;

	if(radio.tokens.gprs_registration_state == RIL_TOKEN_NET_DATA_WAITING) {
		LOGD("Got RILJ request for UNSOL data");

		/* Send back the data we got UNSOL */
		gprs_reg_state_resp_ipc2ril(&(radio.gprs_netinfo), response);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

		for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
			if(response[i] != NULL)
				free(response[i]);
		}

		radio.tokens.gprs_registration_state = (RIL_Token) 0x00;
	} else if(radio.tokens.gprs_registration_state == (RIL_Token) 0x00) {
		LOGD("Got RILJ request for SOL data");

		/* Request data to the modem */
		radio.tokens.gprs_registration_state = t;

		ipc_net_regist_get(&regist_req, IPC_NET_SERVICE_DOMAIN_GPRS);
		ipc_client_send(ipc_client, IPC_NET_REGIST, IPC_TYPE_GET, (void *)&regist_req, sizeof(struct ipc_net_regist_get), getRequestId(t));
	} else {
		LOGE("Another request is going on, reporting failure");
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	net_tokens_state_dump();
}

void respondNetRegistUnsol(struct ipc_message_info *message)
{
	struct ipc_net_regist *netinfo;
	netinfo = (struct ipc_net_regist *) message->data;

	LOGD("Got UNSOL NetRegist message");

	switch(netinfo->domain) {
		case IPC_NET_SERVICE_DOMAIN_GSM:
			if(radio.tokens.registration_state != (RIL_Token) 0 && radio.tokens.registration_state != RIL_TOKEN_NET_DATA_WAITING) {
				LOGE("Another NetRegist Req is in progress, skipping");
				return;
			}

			memcpy(&(radio.netinfo), netinfo, sizeof(struct ipc_net_regist));

			/* we already told RILJ to get the new data but it wasn't done yet */
			if(net_get_tokens_data_waiting() && radio.tokens.registration_state == RIL_TOKEN_NET_DATA_WAITING) {
				LOGD("Updating NetRegist data in background");
			} else {
				net_set_tokens_data_waiting();
				RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
			}
			break;

		case IPC_NET_SERVICE_DOMAIN_GPRS:
			if(radio.tokens.gprs_registration_state != (RIL_Token) 0 && radio.tokens.gprs_registration_state != RIL_TOKEN_NET_DATA_WAITING) {
				LOGE("Another GPRS NetRegist Req is in progress, skipping");
				return;
			}

			memcpy(&(radio.gprs_netinfo), netinfo, sizeof(struct ipc_net_regist));

			/* we already told RILJ to get the new data but it wasn't done yet */
			if(net_get_tokens_data_waiting() && radio.tokens.gprs_registration_state == RIL_TOKEN_NET_DATA_WAITING) {
				LOGD("Updating GPRSNetRegist data in background");
			} else {
				net_set_tokens_data_waiting();
				RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
			}
			break;
		default:
			LOGE("%s: unhandled service domain: %d", __FUNCTION__, netinfo->domain);
			break;
	}

	net_tokens_state_dump();
}

void respondNetRegistSol(struct ipc_message_info *message)
{
	char *response[4];
	int i;

	struct ipc_net_regist *netinfo = (struct ipc_net_regist *) message->data;
	RIL_Token t = getToken(message->aseq);

	LOGD("Got SOL NetRegist message");

	switch(netinfo->domain) {
		case IPC_NET_SERVICE_DOMAIN_GSM:
			if(radio.tokens.registration_state != t)
				LOGE("Registration state tokens mismatch");

			/* Better keeping it up to date */
			memcpy(&(radio.netinfo), netinfo, sizeof(struct ipc_net_regist));

			reg_state_resp_ipc2ril(netinfo, response);

			RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

			for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
				if(response[i] != NULL)
					free(response[i]);
			}

			if(radio.tokens.registration_state != RIL_TOKEN_NET_DATA_WAITING)
				radio.tokens.registration_state = (RIL_Token) 0x00;
			break;
		case IPC_NET_SERVICE_DOMAIN_GPRS:
			if(radio.tokens.gprs_registration_state != t)
				LOGE("GPRS registration state tokens mismatch");

			/* Better keeping it up to date */
			memcpy(&(radio.gprs_netinfo), netinfo, sizeof(struct ipc_net_regist));

			gprs_reg_state_resp_ipc2ril(netinfo, response);

			RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

			for(i = 0; i < sizeof(response) / sizeof(char *) ; i++) {
				if(response[i] != NULL)
					free(response[i]);
			}
			if(radio.tokens.registration_state != RIL_TOKEN_NET_DATA_WAITING)
				radio.tokens.gprs_registration_state = (RIL_Token) 0x00;
			break;
		default:
			LOGE("%s: unhandled service domain: %d", __FUNCTION__, netinfo->domain);
			break;
	}

	net_tokens_state_dump();
}

/**
 * In: IPC_NET_REGIST
 *   This can be SOL (RESP) or UNSOL (NOTI) message from modem
 */
void respondNetRegist(struct ipc_message_info *message)
{
	/* Don't consider this if modem isn't in normal power mode. */
	if(radio.power_mode < POWER_MODE_NORMAL)
		return;

	switch(message->type) {
		case IPC_TYPE_NOTI:
			respondNetRegistUnsol(message);
			break;
		case IPC_TYPE_RESP:
			respondNetRegistSol(message);
			break;
		default:
			LOGE("%s: unhandled ipc method: %d", __FUNCTION__, message->type);
			break;
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

// FIXME: add corect implementation
void requestNwSelectionMode(RIL_Token t)
{
	unsigned int mode = 0;
	RIL_onRequestComplete(t, RIL_E_SUCCESS, &mode, sizeof(mode));
}

/**
 * In: RIL_REQUEST_QUERY_AVAILABLE_NETWORKS
 * 
 * Out: IPC_NET_PLMN_LIST
 */
void requestAvailNetworks(RIL_Token t)
{
	ipc_client_send_get(IPC_NET_PLMN_LIST, getRequestId(t));
}

/* FIXME: cleanup struct names & resp[] addressing */
/**
 * In: IPC_NET_PLMN_LIST
 * Send back available PLMN list
 *
 */
void respondAvailNetworks(RIL_Token t, void *data, int length)
{
	struct ipc_net_plmn_entries *entries_info = (struct ipc_net_plmn_entries*)data;
	struct ipc_net_plmn_entry *entries = (struct ipc_net_plmn_entry *)
		(data + sizeof(struct ipc_net_plmn_entries));

	int i;
	int size = (4 * entries_info->num * sizeof(char*));
	int actual_size = 0;

	char **resp = malloc(size);
	char **resp_ptr = resp;
LOGE("Listed %d PLMNs\n", entries_info->num);
	for(i = 0; i < entries_info->num; i++) {
		/* Assumed type for 'emergency only' PLMNs */
		if(entries[i].type == 0x01)
			continue;

		char *plmn = plmn_string(entries[i].plmn);

		LOGD("PLMN #%d: %s (%s)\n", i, plmn_lookup(plmn), plmn);

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
