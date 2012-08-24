/**
 * This file is part of samsung-ril.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
 * Copyright (C) 2011-2012 Paul Kocialkowski <contact@oaulk.fr>
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

#define LOG_TAG "RIL-SEC"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

SIM_Status ipc2ril_sim_status(struct ipc_sec_pin_status_response *pin_status)
{
	switch(pin_status->type) {
		case IPC_SEC_PIN_SIM_INITIALIZING:
			return SIM_NOT_READY;
		case IPC_SEC_PIN_SIM_LOCK_SC:
			switch(pin_status->key) {
				case IPC_SEC_PIN_SIM_LOCK_SC_PIN1_REQ:
					return SIM_PIN;
				case IPC_SEC_PIN_SIM_LOCK_SC_PUK_REQ:
					return SIM_PUK;
				case IPC_SEC_PIN_SIM_LOCK_SC_CARD_BLOCKED:
					return SIM_BLOCKED;
				default:
					LOGE("%s: unknown SC substate %d --> setting SIM_ABSENT", __FUNCTION__, pin_status->key);
					return SIM_ABSENT;
			}
			break;
		case IPC_SEC_PIN_SIM_LOCK_FD:
			LOGE("%s: FD lock present (unhandled state --> setting SIM_ABSENT)", __FUNCTION__);
			return SIM_ABSENT;
		case IPC_SEC_PIN_SIM_LOCK_PN:
			return SIM_NETWORK_PERSO;
		case IPC_SEC_PIN_SIM_LOCK_PU:
			return SIM_NETWORK_SUBSET_PERSO;
		case IPC_SEC_PIN_SIM_LOCK_PP:
			return SIM_SERVICE_PROVIDER_PERSO;
		case IPC_SEC_PIN_SIM_LOCK_PC:
			return SIM_CORPORATE_PERSO;
		case IPC_SEC_PIN_SIM_INIT_COMPLETE:
			return SIM_READY;
		case IPC_SEC_PIN_SIM_PB_INIT_COMPLETE:
			/* Ignore phone book init complete */
			return ril_state.sim_status;
		case IPC_SEC_PIN_SIM_SIM_LOCK_REQUIRED:
		case IPC_SEC_PIN_SIM_INSIDE_PF_ERROR:
		case IPC_SEC_PIN_SIM_CARD_NOT_PRESENT:
		case IPC_SEC_PIN_SIM_CARD_ERROR:
		default:
			/* Catchall for locked, card error and unknown states */
			return SIM_ABSENT;
	}
}

/**
 * Update the radio state based on SIM status
 *
 * Out: RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED
 *   Indicate when value of RIL_RadioState has changed
 *   Callee will invoke RIL_RadioStateRequest method on main thread
 */
void ril_state_update(SIM_Status status)
{
	RIL_RadioState radio_state;

	/* If power mode isn't at least normal, don't update RIL state */
	if(ril_state.power_mode < POWER_MODE_NORMAL)
		return;
	
	ril_state.sim_status = status;

	switch(status) {
		case SIM_READY:
			radio_state = COMPAT_RADIO_STATE_ON;
			break;
		case SIM_NOT_READY:
			radio_state = RADIO_STATE_SIM_NOT_READY;
			break;
		case SIM_ABSENT:
		case SIM_PIN:
		case SIM_PUK:
		case SIM_BLOCKED:
		case SIM_NETWORK_PERSO:
		case SIM_NETWORK_SUBSET_PERSO:
		case SIM_CORPORATE_PERSO:
		case SIM_SERVICE_PROVIDER_PERSO:
			radio_state = RADIO_STATE_SIM_LOCKED_OR_ABSENT;
			break;
		default:
			radio_state = RADIO_STATE_SIM_NOT_READY;
			break;
	}
	

	ril_state.radio_state = radio_state;

	ril_tokens_check();

	RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);
}

void ipc2ril_card_status(struct ipc_sec_pin_status_response *pin_status, RIL_CardStatus *card_status)
{
	SIM_Status sim_status;
	int app_status_array_length;
	int app_index;
	int i;

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

	app_status_array_length = sizeof(app_status_array) / sizeof(RIL_AppStatus);

	if(app_status_array_length > RIL_CARD_MAX_APPS)
		app_status_array_length = RIL_CARD_MAX_APPS;

	sim_status = ipc2ril_sim_status(pin_status);

	/* Card is assumed to be present if not explicitly absent */
	if(sim_status == SIM_ABSENT) {
		card_status->card_state = RIL_CARDSTATE_ABSENT;
	} else {
		card_status->card_state = RIL_CARDSTATE_PRESENT;
	}

	// FIXME: How do we know that?
	card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;

	// Initialize the apps
	for (i = 0 ; i < app_status_array_length ; i++) {
		memcpy((void *) &(card_status->applications[i]), (void *) &(app_status_array[i]), sizeof(RIL_AppStatus));
	}
	for(i = app_status_array_length ; i < RIL_CARD_MAX_APPS ; i++) {
		memset((void *) &(card_status->applications[i]), 0, sizeof(RIL_AppStatus));
	}

	// sim_status corresponds to the app index on the table
	card_status->gsm_umts_subscription_app_index = (int) sim_status;
	card_status->cdma_subscription_app_index = (int) sim_status;
	card_status->num_applications = app_status_array_length;

	LOGD("Selecting application #%d on %d", (int) sim_status, app_status_array_length);
}

void ril_tokens_pin_status_dump(void)
{
	LOGD("ril_tokens_pin_status_dump:\n\
	\tril_state.tokens.pin_status = 0x%p\n", ril_state.tokens.pin_status);
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
void ipc_sec_pin_status(struct ipc_message_info *info)
{
	RIL_Token t = reqGetToken(info->aseq);
	struct ipc_sec_pin_status_response *pin_status = (struct ipc_sec_pin_status_response *) info->data;
	RIL_CardStatus card_status;
	SIM_Status sim_status;

	if(ril_state.power_mode == POWER_MODE_NORMAL && ril_state.tokens.radio_power != (RIL_Token) 0x00) {
		RIL_onRequestComplete(ril_state.tokens.radio_power, RIL_E_SUCCESS, NULL, 0);
		ril_state.tokens.radio_power = (RIL_Token) 0x00;
	}

	switch(info->type) {
		case IPC_TYPE_NOTI:
			// Don't consider this if modem isn't in normal power mode
			if(ril_state.power_mode < POWER_MODE_NORMAL)
				return;

			LOGD("Got UNSOL PIN status message");

			if(ril_state.tokens.pin_status != (RIL_Token) 0x00 && ril_state.tokens.pin_status != RIL_TOKEN_DATA_WAITING) {
				LOGE("Another PIN status Req is in progress, skipping");
				return;
			}

			sim_status = ipc2ril_sim_status(pin_status);
			ril_state_update(sim_status);

			memcpy(&(ril_state.sim_pin_status), pin_status, sizeof(struct ipc_sec_pin_status_response));

			ril_state.tokens.pin_status = RIL_TOKEN_DATA_WAITING;
			RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0);
			break;
		case IPC_TYPE_RESP:
			LOGD("Got SOL PIN status message");

			if(ril_state.tokens.pin_status != t)
				LOGE("PIN status tokens mismatch");

			sim_status = ipc2ril_sim_status(pin_status);
			ril_state_update(sim_status);

			// Better keeping this up to date
			memcpy(&(ril_state.sim_pin_status), pin_status, sizeof(struct ipc_sec_pin_status_response));

			ipc2ril_card_status(pin_status, &card_status);
			RIL_onRequestComplete(t, RIL_E_SUCCESS, &card_status, sizeof(RIL_CardStatus));

			if(ril_state.tokens.pin_status != RIL_TOKEN_DATA_WAITING)
				ril_state.tokens.pin_status = (RIL_Token) 0x00;
			break;
		default:
			LOGE("%s: unhandled ipc method: %d", __FUNCTION__, info->type);
			break;
	}

	ril_tokens_pin_status_dump();
}

/**
 * In: RIL_REQUEST_GET_SIM_STATUS
 *   Requests status of the SIM interface and the SIM card
 */
void ril_request_get_sim_status(RIL_Token t)
{
	struct ipc_sec_pin_status_response *pin_status;
	RIL_CardStatus card_status;
	SIM_Status sim_status;

	if(ril_state.tokens.pin_status == RIL_TOKEN_DATA_WAITING) {
		LOGD("Got RILJ request for UNSOL data");
		hex_dump(&(ril_state.sim_pin_status), sizeof(struct ipc_sec_pin_status_response));
		pin_status = &(ril_state.sim_pin_status);

		ipc2ril_card_status(pin_status, &card_status);

		RIL_onRequestComplete(t, RIL_E_SUCCESS, &card_status, sizeof(RIL_CardStatus));

		ril_state.tokens.pin_status = (RIL_Token) 0x00;
	} else if(ril_state.tokens.pin_status == (RIL_Token) 0x00) {
		LOGD("Got RILJ request for SOL data");

		/* Request data to the modem */
		ril_state.tokens.pin_status = t;

		ipc_fmt_send_get(IPC_SEC_PIN_STATUS, reqGetId(t));
	} else {
		LOGE("Another request is going on, returning UNSOL data");

		pin_status = &(ril_state.sim_pin_status);

		ipc2ril_card_status(pin_status, &card_status);
		RIL_onRequestComplete(t, RIL_E_SUCCESS, &card_status, sizeof(card_status));
	}

	ril_tokens_pin_status_dump();
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
void ril_request_sim_io(RIL_Token t, void *data, size_t datalen)
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

	ipc_fmt_send(IPC_SEC_RSIM_ACCESS, IPC_TYPE_GET, (unsigned char*)&message, sizeof(message), reqGetId(t));
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
void ipc_sec_rsim_access(struct ipc_message_info *info)
{
	struct ipc_sec_rsim_access_response *rsim_resp = (struct ipc_sec_rsim_access_response *) info->data;
	const unsigned char *data_ptr = ((unsigned char *) info->data + sizeof(*rsim_resp));
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

	RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_SUCCESS, &response, sizeof(response));

	free(response.simResponse);
}

/**
 * In: IPC_GEN_PHONE_RES
 *   Provides result of IPC_SEC_PIN_STATUS SET
 *
 * Out: RIL_REQUEST_ENTER_SIM_PIN
 *   Returns PIN SIM unlock result
 */
void ipc_sec_pin_status_complete(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *phone_res = (struct ipc_gen_phone_res *) info->data;
	int rc;

	int attempts = -1;

	rc = ipc_gen_phone_res_check(phone_res);
	if(rc < 0) {
		if((phone_res->code & 0x00ff) == 0x10) {
			LOGE("Wrong password!");
			RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_PASSWORD_INCORRECT, &attempts, sizeof(attempts));
		} else if((phone_res->code & 0x00ff) == 0x0c) {
			LOGE("Wrong password and no attempts left!");

			attempts = 0;
			RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_PASSWORD_INCORRECT, &attempts, sizeof(attempts));

			RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0);
		} else {
			LOGE("There was an error during pin status complete!");
			RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
		}
		return;
	}

	RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_SUCCESS, &attempts, sizeof(attempts));
}

/**
 * In: IPC_SEC_LOCK_INFO
 *   Provides number of retries left for a lock type
 */
void ipc_sec_lock_info(struct ipc_message_info *info)
{
	/*
	 * FIXME: solid way of handling lockinfo and sim unlock response together
	 * so we can return the number of attempts left in respondSecPinStatus
	 */
	int attempts;
	struct ipc_sec_lock_info_response *lock_info = (struct ipc_sec_lock_info_response *) info->data;

	if(lock_info->type == IPC_SEC_PIN_TYPE_PIN1) {
		attempts = lock_info->attempts;
		LOGD("%s: PIN1 %d attempts left", __FUNCTION__, attempts);
	} else {
		LOGE("%s: unhandled lock type %d", __FUNCTION__, lock_info->type);
	}
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
void ril_request_enter_sim_pin(RIL_Token t, void *data, size_t datalen)
{
	struct ipc_sec_pin_status_set pin_status;
	char *pin = ((char **) data)[0];
	unsigned char buf[9];

	/* 1. Send PIN */
	if(strlen(data) > 16) {
		LOGE("%s: pin exceeds maximum length", __FUNCTION__);
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	ipc_sec_pin_status_set_setup(&pin_status, IPC_SEC_PIN_TYPE_PIN1, pin, NULL);

	ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_SEC_PIN_STATUS,
		ipc_sec_pin_status_complete);

	ipc_fmt_send_set(IPC_SEC_PIN_STATUS, reqGetId(t), (unsigned char *) &pin_status, sizeof(pin_status));

	/* 2. Get lock status */
	// FIXME: This is not clean at all
	memset(buf, 0, sizeof(buf));
	buf[0] = 1;
	buf[1] = IPC_SEC_PIN_TYPE_PIN1;

	ipc_fmt_send(IPC_SEC_LOCK_INFO, IPC_TYPE_GET, buf, sizeof(buf), reqGetId(t));
}

void ril_request_change_sim_pin(RIL_Token t, void *data, size_t datalen)
{
	char *password_old = ((char **) data)[0];
	char *password_new = ((char **) data)[1];
	struct ipc_sec_change_locking_pw locking_pw;

	memset(&locking_pw, 0, sizeof(locking_pw));

	locking_pw.type = IPC_SEC_PIN_SIM_LOCK_SC;

	locking_pw.length_new = strlen(password_new) > sizeof(locking_pw.password_new)
				? sizeof(locking_pw.password_new)
				: strlen(password_new);

	memcpy(locking_pw.password_new, password_new, locking_pw.length_new);

	locking_pw.length_old = strlen(password_old) > sizeof(locking_pw.password_old)
				? sizeof(locking_pw.password_old)
				: strlen(password_old);

	memcpy(locking_pw.password_old, password_old, locking_pw.length_old);

	ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_SEC_CHANGE_LOCKING_PW,
		ipc_sec_pin_status_complete);

	ipc_fmt_send_set(IPC_SEC_CHANGE_LOCKING_PW, reqGetId(t), (unsigned char *) &locking_pw, sizeof(locking_pw));
}

void ril_request_enter_sim_puk(RIL_Token t, void *data, size_t datalen)
{
	struct ipc_sec_pin_status_set pin_status;
	char *puk = ((char **) data)[0];
	char *pin = ((char **) data)[1];

	ipc_sec_pin_status_set_setup(&pin_status, IPC_SEC_PIN_TYPE_PIN1, pin, puk);

	ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_SEC_PIN_STATUS,
		ipc_sec_pin_status_complete);

	ipc_fmt_send_set(IPC_SEC_PIN_STATUS, reqGetId(t), (unsigned char *) &pin_status, sizeof(pin_status));
}

/**
 * In: IPC_SEC_PHONE_LOCK
 *
 * Out: RIL_REQUEST_QUERY_FACILITY_LOCK
 *   Query the status of a facility lock state
 */
void ipc_sec_phone_lock(struct ipc_message_info *info)
{
	int status;
	struct ipc_sec_phone_lock_response *lock = (struct ipc_sec_phone_lock_response *) info->data;
	
	status = lock->status;

	RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_SUCCESS, &status, sizeof(status));
}

/**
 * In: RIL_REQUEST_QUERY_FACILITY_LOCK
 *   Query the status of a facility lock state
 *
 * Out: IPC_SEC_PHONE_LOCK GET
 */
void ril_request_query_facility_lock(RIL_Token t, void *data, size_t datalen)
{
	struct ipc_sec_phone_lock_get lock_request;

	char *facility = ((char **) data)[0];

	if(!strcmp(facility, "SC")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_SC;
	} else if(!strcmp(facility, "FD")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_FD;
	} else if(!strcmp(facility, "PN")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_PN;
	} else if(!strcmp(facility, "PU")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_PU;
	} else if(!strcmp(facility, "PP")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_PP;
	} else if(!strcmp(facility, "PC")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_PC;
	} else {
		LOGE("%s: unsupported facility: %s", __FUNCTION__, facility);
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
	ipc_fmt_send(IPC_SEC_PHONE_LOCK, IPC_TYPE_GET, &lock_request, sizeof(lock_request), reqGetId(t));
}

// Both functions were the same
#define ipc_sec_phone_lock_complete \
	ipc_sec_pin_status_complete

/**
 * In: RIL_REQUEST_SET_FACILITY_LOCK
 *   Enable/disable one facility lock
 *
 * Out: IPC_SEC_PHONE_LOCK SET
 */
void ril_request_set_facility_lock(RIL_Token t, void *data, size_t datalen)
{
	struct ipc_sec_phone_lock_set lock_request;

	char *facility = ((char **) data)[0];
	char *lock = ((char **) data)[1];
	char *password = ((char **) data)[2];
	char *class = ((char **) data)[3];

	memset(&lock_request, 0, sizeof(lock_request));

	if(!strcmp(facility, "SC")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_SC;
	} else if(!strcmp(facility, "FD")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_FD;
	} else if(!strcmp(facility, "PN")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_PN;
	} else if(!strcmp(facility, "PU")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_PU;
	} else if(!strcmp(facility, "PP")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_PP;
	} else if(!strcmp(facility, "PC")) {
		lock_request.type = IPC_SEC_PIN_SIM_LOCK_PC;
	} else {
		LOGE("%s: unsupported facility: %s", __FUNCTION__, facility);
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	lock_request.lock = lock[0] == '1' ? 1 : 0;
	lock_request.length = strlen(password) > sizeof(lock_request.password)
				? sizeof(lock_request.password)
				: strlen(password);

	memcpy(lock_request.password, password, lock_request.length);

	ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_SEC_PHONE_LOCK,
		ipc_sec_phone_lock_complete);

	ipc_fmt_send(IPC_SEC_PHONE_LOCK, IPC_TYPE_SET, &lock_request, sizeof(lock_request), reqGetId(t));
}
