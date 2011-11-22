#define LOG_TAG "RIL-SIM"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

extern const struct RIL_Env *rilenv;
extern struct radio_state radio;
extern struct ipc_client *ipc_client;

/**
 * Update the radio state based on SIM status
 *
 * Out: RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED
 *   Indicate when value of RIL_RadioState has changed
 *   Callee will invoke RIL_RadioStateRequest method on main thread
 */
void updateRadioState(SIM_Status status)
{
	/* If power mode isn't at least normal, don't update RIL state */
	if(radio.power_mode < POWER_MODE_NORMAL)
		return;

	switch(status) {
		case SIM_READY:
			radio.radio_state = RADIO_STATE_SIM_READY;
			break;
		case SIM_NOT_READY:
			radio.radio_state = RADIO_STATE_SIM_NOT_READY;
			break;
		case SIM_ABSENT:
		case SIM_PIN:
		case SIM_PUK:
		case SIM_BLOCKED:
		case SIM_NETWORK_PERSO:
		case SIM_NETWORK_SUBSET_PERSO:
		case SIM_CORPORATE_PERSO:
		case SIM_SERVICE_PROVIDER_PERSO:
			radio.radio_state = RADIO_STATE_SIM_LOCKED_OR_ABSENT;
			break;
		default:
			radio.radio_state = RADIO_STATE_SIM_NOT_READY;
			break;
	}

	RadioTokensCheck();
	RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

/**
 * In: IPC_SEC_PIN_STATUS
 *   Provides SIM initialization/lock status
 *
 * Out: RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED
 *   Indicates that SIM state changes.
 *   Callee will invoke RIL_REQUEST_GET_SIM_STATUS on main thread
 *
 * Out: RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED
 *   Indicate when value of RIL_RadioState has changed
 *   Callee will invoke RIL_RadioStateRequest method on main thread
 */
void respondSimStatusChanged(RIL_Token t, void *data, int length)
{
	struct ipc_sec_pin_status_noti *pin_status = (struct ipc_sec_pin_status_noti*)data;

	/* Don't consider this if modem isn't in normal power mode. */
	if(radio.power_mode < POWER_MODE_NORMAL)
		return;

	if(radio.power_mode == POWER_MODE_NORMAL && radio.tokens.radio_power != 0) {
		RIL_onRequestComplete(radio.tokens.radio_power, RIL_E_SUCCESS, NULL, 0);
		radio.tokens.radio_power = 0;
	}

	/* Determine SIM status */
	switch(pin_status->type) {
		case IPC_SEC_PIN_SIM_INITIALIZING:
			radio.sim_status = SIM_NOT_READY;
			break;
		case IPC_SEC_PIN_SIM_LOCK_SC:
			switch(pin_status->key) {
				case IPC_SEC_PIN_SIM_LOCK_SC_PIN1_REQ:
					radio.sim_status = SIM_PIN;
					break;
				case IPC_SEC_PIN_SIM_LOCK_SC_PUK_REQ:
					radio.sim_status = SIM_PUK;
					break;
				case IPC_SEC_PIN_SIM_LOCK_SC_CARD_BLOCKED:
					radio.sim_status = SIM_BLOCKED;
					break;
				default:
					radio.sim_status = SIM_ABSENT;
					LOGE("%s: unknown SC substate %d --> setting SIM_ABSENT", __FUNCTION__, pin_status->key);
					break;
			}
			break;
		case IPC_SEC_PIN_SIM_LOCK_FD:
			radio.sim_status = SIM_ABSENT;
			LOGE("%s: FD lock present (unhandled state --> setting SIM_ABSENT)", __FUNCTION__);
			break;
		case IPC_SEC_PIN_SIM_LOCK_PN:
			radio.sim_status = SIM_NETWORK_PERSO;
			break;
		case IPC_SEC_PIN_SIM_LOCK_PU:
			radio.sim_status = SIM_NETWORK_SUBSET_PERSO;
			break;
		case IPC_SEC_PIN_SIM_LOCK_PP:
			radio.sim_status = SIM_SERVICE_PROVIDER_PERSO;
			break;
		case IPC_SEC_PIN_SIM_LOCK_PC:
			radio.sim_status = SIM_CORPORATE_PERSO;
			break;
		case IPC_SEC_PIN_SIM_INIT_COMPLETE:
			radio.sim_status = SIM_READY;
			break;
		case IPC_SEC_PIN_SIM_PB_INIT_COMPLETE:
			/* Ignore phone book init complete */
			return;
		case IPC_SEC_PIN_SIM_SIM_LOCK_REQUIRED:
		case IPC_SEC_PIN_SIM_INSIDE_PF_ERROR:
		case IPC_SEC_PIN_SIM_CARD_NOT_PRESENT:
		case IPC_SEC_PIN_SIM_CARD_ERROR:
		default:
			/* Catchall for locked, card error and unknown states */
			radio.sim_status = SIM_ABSENT;
			break;	
	}

	/* Update radio state based on SIM state */
	updateRadioState(radio.sim_status);

	RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0);
}

/**
 * In: RIL_REQUEST_GET_SIM_STATUS
 *   Requests status of the SIM interface and the SIM card
 */
void requestSimStatus(RIL_Token t)
{
	static RIL_AppStatus app_status_array[] = {
		/* SIM_ABSENT = 0 */
		{ RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
		/* SIM_NOT_READY = 1 */
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
		/* SIM_READY = 2 */
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
		NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
		/* SIM_PIN = 3 */
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
		/* SIM_PUK = 4 */
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN },
		/* SIM_BLOCKED = 4 */
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_PERM_BLOCKED, RIL_PINSTATE_UNKNOWN },
		/* SIM_NETWORK_PERSO = 6 */
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
		/* SIM_NETWORK_SUBSET_PERSO = 7 */
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK_SUBSET,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
		/* SIM_CORPORATE_PERSO = 8 */
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_CORPORATE,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
		/* SIM_SERVICE_PROVIDER_PERSO = 9 */
		{ RIL_APPTYPE_SIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_SERVICE_PROVIDER,
		NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
	};

	int i, num_applications;
	RIL_CardState card_state;
	RIL_CardStatus card_status;

	/* Card is assumed to be present if not explicitly absent */
	if(radio.sim_status == SIM_ABSENT) {
		card_state = RIL_CARDSTATE_ABSENT;
	} else {
		card_state = RIL_CARDSTATE_PRESENT;
	}

	/* Fill in struct */
	card_status.card_state = card_state;
	card_status.universal_pin_state = RIL_PINSTATE_UNKNOWN;
	card_status.gsm_umts_subscription_app_index = RIL_CARD_MAX_APPS;
	card_status.cdma_subscription_app_index = RIL_CARD_MAX_APPS;
	card_status.num_applications = 0;

	/* Initialize apps */
	for (i = 0; i < RIL_CARD_MAX_APPS; i++) {
		card_status.applications[i] = app_status_array[0];
	}

	/* If a card is present, add the gsm/umts application */
	if(card_status.card_state == RIL_CARDSTATE_PRESENT) {
		card_status.gsm_umts_subscription_app_index = 0;
		card_status.num_applications = 1;
		card_status.applications[0] = app_status_array[radio.sim_status];

		/* FIXME: if USIM, set apptype */
		//card_status.applications[0] = RIL_APPTYPE_USIM
	}

	RIL_onRequestComplete(t, RIL_E_SUCCESS, &card_status, sizeof(card_status));
}

/**
 * In: RIL_REQUEST_SIM_IO
 *   Request SIM I/O operation.
 *   This is similar to the TS 27.007 "restricted SIM" operation
 *   where it assumes all of the EF selection will be done by the
 *   callee.
 *
 * Out: IPC_SEC_RSIM_ACCESS
 *   Performs a restricted SIM read operation
 */
void requestSimIo(RIL_Token t, void *data, size_t datalen)
{
	const RIL_SIM_IO *sim_io;
	unsigned char message[262];
	struct ipc_sec_rsim_access_request *rsim_data;

	unsigned char *rsim_payload;
	int payload_length;

	sim_io = (const RIL_SIM_IO*)data;
	rsim_payload = message + sizeof(*rsim_data);

	/* Set up RSIM header */
	rsim_data = (struct ipc_sec_rsim_access_request*)message;
	rsim_data->command = sim_io->command;
	rsim_data->fileid = sim_io->fileid;
	rsim_data->p1 = sim_io->p1;
	rsim_data->p2 = sim_io->p2;
	rsim_data->p3 = sim_io->p3;

	/* Add payload if present */
	if(sim_io->data) {
		payload_length = (2 * strlen(sim_io->data));

		if(sizeof(*rsim_data) + payload_length > sizeof(message))
			return;

		hex2bin(sim_io->data, strlen(sim_io->data), rsim_payload);
	}

	ipc_client_send(ipc_client, IPC_SEC_RSIM_ACCESS, IPC_TYPE_GET, (unsigned char*)&message, sizeof(message), getRequestId(t));
}

/**
 * In: IPC_SEC_RSIM_ACCESS
 *   Provides restricted SIM read operation result
 *
 * Out: RIL_REQUEST_SIM_IO
 *   Request SIM I/O operation.
 *   This is similar to the TS 27.007 "restricted SIM" operation
 *   where it assumes all of the EF selection will be done by the
 *   callee.
 */
void respondSecRsimAccess(RIL_Token t, void *data, int length)
{
	struct ipc_sec_rsim_access_response *rsim_resp = (struct ipc_sec_rsim_access_response*)data;
	const unsigned char *data_ptr = ((unsigned char*)data + sizeof(*rsim_resp));
	char *sim_resp;
	RIL_SIM_IO_Response response;

	response.sw1 = rsim_resp->sw1;
	response.sw2 = rsim_resp->sw2;

	if(rsim_resp->len) {
		sim_resp = (char*)malloc(rsim_resp->len * 2 + 1);
		bin2hex(data_ptr, rsim_resp->len, sim_resp);
		response.simResponse = sim_resp;
	} else {
		response.simResponse = malloc(1);
		response.simResponse[0] = '\0';
	}

	RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));

	free(response.simResponse);
}

/**
 * In: RIL_REQUEST_GET_IMSI
 *   Get the SIM IMSI
 *   Only valid when radio state is "RADIO_STATE_SIM_READY"
 *
 * Out: IPC_MISC_ME_IMSI
 *   Requests ME's IMSI
 */
void requestIMSI(RIL_Token t)
{
	ipc_client_send_get(IPC_MISC_ME_IMSI, getRequestId(t));
}

/**
 * In: IPC_MISC_ME_IMSI
 *   Provides ME's IMSI
 *
 * Out: RIL_REQUEST_GET_IMSI
 *   Get the SIM IMSI
 *   Only valid when radio state is "RADIO_STATE_SIM_READY"
 */
void respondIMSI(struct ipc_message_info *request)
{
	unsigned char *imsi_length;
	char *imsi;

	if(request->length < 1) {
		LOGE("%s: zero data length", __FUNCTION__);
		RIL_onRequestComplete(getToken(request->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	imsi_length = (unsigned char*)request->data;

	if(((int)request->length) < *imsi_length + 1) {
		LOGE("%s: missing IMSI data", __FUNCTION__);
		RIL_onRequestComplete(getToken(request->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	/* Copy IMSI */
	imsi = (char*)malloc(*imsi_length+1);
	memcpy(imsi, ((unsigned char*)request->data)+1, *imsi_length);
	imsi[*imsi_length] = '\0';

	RIL_onRequestComplete(getToken(request->aseq), RIL_E_SUCCESS, imsi, *imsi_length+1);
}

/**
 * In: RIL_REQUEST_ENTER_SIM_PIN
 *   Supplies SIM PIN. Only called if RIL_CardStatus has RIL_APPSTATE_PIN state
 * 
 * Out: IPC_SEC_PIN_STATUS SET
 *   Attempts to unlock SIM PIN1
 *
 * Out: IPC_SEC_LOCK_INFO
 *   Retrieves PIN1 lock status
 */
void requestEnterSimPin(RIL_Token t, void *data, size_t datalen)
{
	struct ipc_sec_pin_status_set pin_status;
	char *pin = ((char**)data)[0];
	unsigned char buf[9];

	/* 1. Send PIN */
	if(strlen(data) > 16) {
		LOGE("%s: pin exceeds maximum length", __FUNCTION__);
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	ipc_sec_pin_status_set_setup(&pin_status, IPC_SEC_PIN_TYPE_PIN1, pin, NULL);

	ipc_client_send_set(IPC_SEC_PIN_STATUS, getRequestId(t), (unsigned char*)&pin_status, sizeof(pin_status));

	/* 2. Get lock status */
	memset(buf, 0, sizeof(buf));
	buf[0] = 1;
	buf[1] = IPC_SEC_PIN_TYPE_PIN1;

	ipc_client_send(ipc_client, IPC_SEC_LOCK_INFO, IPC_TYPE_GET, buf, sizeof(buf), getRequestId(t));
}

/**
 * In: IPC_GEN_PHONE_RES
 *   Provides result of IPC_SEC_PIN_STATUS SET
 *
 * Out: RIL_REQUEST_ENTER_SIM_PIN
 *   Returns PIN SIM unlock result
 */
void respondSecPinStatus(struct ipc_message_info *request)
{
	struct ipc_gen_phone_res *gen_res = (struct ipc_gen_phone_res*)request->data;
	int attempts = -1;

	if(gen_res->code == 0x0010) {
		RIL_onRequestComplete(getToken(request->aseq), RIL_E_PASSWORD_INCORRECT, &attempts, sizeof(int*));
	} else if(gen_res->code == 0x8000) {
		RIL_onRequestComplete(getToken(request->aseq), RIL_E_SUCCESS, &attempts, sizeof(int*));
	} else {
		LOGE("%s: unhandled code %04x", __FUNCTION__, gen_res->code);
	}
}

/**
 * In: IPC_SEC_LOCK_INFO
 *   Provides number of retries left for a lock type
 */
void respondLockInfo(struct ipc_message_info *request)
{
	/*
	 * FIXME: solid way of handling lockinfo and sim unlock response together
	 * so we can return the number of attempts left in respondSecPinStatus
	 */
	int attempts;
	struct ipc_sec_lock_info_response *lock_info = (struct ipc_sec_lock_info_response*)request->data;

	if(lock_info->type == IPC_SEC_PIN_TYPE_PIN1) {
		attempts = lock_info->attempts;
		LOGD("%s: PIN1 %d attempts left", __FUNCTION__, attempts);
	} else {
		LOGE("%s: unhandled lock type %d", __FUNCTION__, lock_info->type);
	}
}

/**
 * In: RIL_REQUEST_QUERY_FACILITY_LOCK
 *   Query the status of a facility lock state
 *
 * Out: IPC_SEC_PHONE_LOCK GET
 */
void requestQueryFacilityLock(RIL_Token t, void *data, size_t datalen)
{
	unsigned char lock_type;
	char *facility = ((char**)data)[0];

	if(!strcmp(facility, "SC")) {
		lock_type = IPC_SEC_PIN_SIM_LOCK_SC;
	} else if(!strcmp(facility, "FD")) {
		lock_type = IPC_SEC_PIN_SIM_LOCK_FD;
	} else {
		LOGE("%s: unsupported facility: %s", __FUNCTION__, facility);
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	ipc_client_send(ipc_client, IPC_SEC_PHONE_LOCK, IPC_TYPE_GET, &lock_type, sizeof(lock_type), getRequestId(t));
}

/**
 * In: IPC_SEC_PHONE_LOCK
 *
 * Out: RIL_REQUEST_QUERY_FACILITY_LOCK
 *   Query the status of a facility lock state
 */
void respondSecPhoneLock(struct ipc_message_info *request)
{
	int status;
	struct ipc_sec_phone_lock_response *lock = (struct ipc_sec_phone_lock_response*)request->data;
	
	status = lock->status;

	RIL_onRequestComplete(getToken(request->aseq), RIL_E_SUCCESS, &status, sizeof(int*));
}

/**
 * In: RIL_REQUEST_SET_FACILITY_LOCK
 *   Enable/disable one facility lock
 *
 * Out: IPC_SEC_PHONE_LOCK SET
 */
void requestSetFacilityLock(RIL_Token t, void *data, size_t datalen)
{
	unsigned char lock_type;
	char *facility = ((char**)data)[0];

	if(!strcmp(facility, "SC")) {
		lock_type = IPC_SEC_PIN_SIM_LOCK_SC;
	} else if(!strcmp(facility, "FD")) {
		lock_type = IPC_SEC_PIN_SIM_LOCK_FD;
	} else {
		LOGE("%s: unsupported facility: %s", __FUNCTION__, facility);
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	ipc_client_send(ipc_client, IPC_SEC_PHONE_LOCK, IPC_TYPE_GET, &lock_type, sizeof(lock_type), getRequestId(t));
}
