/**
 * This file is part of samsung-ril.
 *
 * Copyright (C) 2011-2012 Paul Kocialkowski <contact@oaulk.fr>
 * Copyright (C) 2011 Denis 'GNUtoo' Carikli <GNUtoo@no-log.org>
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

#include <netinet/in.h>
#include <arpa/inet.h>

#define LOG_TAG "RIL-GPRS"
#include <utils/Log.h>
#include <cutils/properties.h>

#if RIL_VERSION >= 6
#include <netutils/ifc.h>
#endif

#include "samsung-ril.h"
#include "util.h"

struct ril_gprs_connection **ril_gprs_connections;
int ril_gprs_connections_count;

RIL_LastDataCallActivateFailCause ipc2ril_gprs_fail_cause(unsigned short fail_cause)
{
	switch(fail_cause) {

		case IPC_GPRS_FAIL_INSUFFICIENT_RESOURCES:
			return PDP_FAIL_INSUFFICIENT_RESOURCES;
		case IPC_GPRS_FAIL_MISSING_UKNOWN_APN:
			return PDP_FAIL_MISSING_UKNOWN_APN;
		case IPC_GPRS_FAIL_UNKNOWN_PDP_ADDRESS_TYPE:
			return PDP_FAIL_UNKNOWN_PDP_ADDRESS_TYPE;
		case IPC_GPRS_FAIL_USER_AUTHENTICATION:
			return PDP_FAIL_USER_AUTHENTICATION;
		case IPC_GPRS_FAIL_ACTIVATION_REJECT_GGSN:
			return PDP_FAIL_ACTIVATION_REJECT_GGSN;
		case IPC_GPRS_FAIL_ACTIVATION_REJECT_UNSPECIFIED:
			return PDP_FAIL_ACTIVATION_REJECT_UNSPECIFIED;
		case IPC_GPRS_FAIL_SERVICE_OPTION_NOT_SUPPORTED:
			return PDP_FAIL_SERVICE_OPTION_NOT_SUPPORTED;
		case IPC_GPRS_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED:
			return PDP_FAIL_SERVICE_OPTION_NOT_SUBSCRIBED;
		case IPC_GPRS_FAIL_SERVICE_OPTION_OUT_OF_ORDER:
			return PDP_FAIL_SERVICE_OPTION_OUT_OF_ORDER;
		case IPC_GPRS_FAIL_NSAPI_IN_USE:
			return PDP_FAIL_NSAPI_IN_USE;
		default:
			return PDP_FAIL_ERROR_UNSPECIFIED;
	}
}

int ipc2ril_gprs_connection_active(unsigned char state)
{
	switch(state) {
		case IPC_GPRS_STATE_DISABLED:
			return 1;
		case IPC_GPRS_STATE_ENABLED:
			return 2;
		case IPC_GPRS_STATE_NOT_ENABLED:
		default:
			return 0;
	}
}

void ril_gprs_connections_init(void)
{
	struct ipc_client_gprs_capabilities gprs_capabilities;
	struct ipc_client *ipc_client;
	int ril_gprs_connections_size = 0;

	ipc_client = ((struct ipc_client_object *) ipc_fmt_client->object)->ipc_client;
	ipc_client_gprs_get_capabilities(ipc_client, &gprs_capabilities);

	ril_gprs_connections_size =
		gprs_capabilities.cid_max * sizeof(struct ril_gprs_connection *);

	ril_gprs_connections = (struct ril_gprs_connection **)
		malloc(ril_gprs_connections_size);
	memset((void *) ril_gprs_connections, 0, ril_gprs_connections_size);

	ril_gprs_connections_count = gprs_capabilities.cid_max;
}

int ril_gprs_connection_reg_id(void)
{
	struct ril_gprs_connection *gprs_connection;
	int i;

	for(i=0 ; i < ril_gprs_connections_count ; i++) {
		if(ril_gprs_connections[i] == NULL)
			return i;
	}

	LOGD("No room left for another GPRS connection, trying to clean one up");

	// When all the slots are taken, see if some are in a non-working state
	for(i=0 ; i < ril_gprs_connections_count ; i++) {
		if(ril_gprs_connections[i]->enabled == 0) {
			ril_gprs_connection_del(ril_gprs_connections[i]);

			return i;
		}
	}

	return -1;
}

struct ril_gprs_connection *ril_gprs_connection_add(void)
{
	struct ril_gprs_connection *gprs_connection = NULL;
	int id = ril_gprs_connection_reg_id();

	if(id < 0) {
		LOGE("Unable to add another GPRS connection!");
		return NULL;
	}

	gprs_connection = malloc(sizeof(struct ril_gprs_connection));
	memset(gprs_connection, 0, sizeof(struct ril_gprs_connection));

	gprs_connection->cid = id + 1;
	gprs_connection->enabled = 0;
	gprs_connection->interface = NULL;
	gprs_connection->token = (RIL_Token) 0x00;

	ril_gprs_connections[id] = gprs_connection;

	return gprs_connection;
}

void ril_gprs_connection_del(struct ril_gprs_connection *gprs_connection)
{
	int i;

	if(gprs_connection == NULL)
		return;

	if(gprs_connection->interface != NULL)
		free(gprs_connection->interface);

	for(i=0 ; i < ril_gprs_connections_count ; i++)
		if(ril_gprs_connections[i] == gprs_connection)
				ril_gprs_connections[i] = NULL;

	free(gprs_connection);
}

struct ril_gprs_connection *ril_gprs_connection_get_token(RIL_Token token)
{
	int i;

	for(i=0 ; i < ril_gprs_connections_count ; i++)
		if(ril_gprs_connections[i] != NULL)
			if(ril_gprs_connections[i]->token == token)
				return ril_gprs_connections[i];

	return NULL;
}

struct ril_gprs_connection *ril_gprs_connection_get_cid(int cid)
{
	int i;

	for(i=0 ; i < ril_gprs_connections_count ; i++)
		if(ril_gprs_connections[i] != NULL)
			if(ril_gprs_connections[i]->cid == cid)
				return ril_gprs_connections[i];

	return NULL;
}

void ipc_gprs_pdp_context_enable_complete(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *phone_res = (struct ipc_gen_phone_res *) info->data;
	struct ril_gprs_connection *gprs_connection;
	int rc;

	gprs_connection = ril_gprs_connection_get_token(reqGetToken(info->aseq));

	if(!gprs_connection) {
		LOGE("Unable to find GPRS connection, aborting");

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	rc = ipc_gen_phone_res_check(phone_res);
	if(rc < 0) {
		LOGE("There was an error, aborting PDP context complete");

		gprs_connection->fail_cause = PDP_FAIL_ERROR_UNSPECIFIED;
		gprs_connection->token = (RIL_Token) 0x00;
		ril_state.gprs_last_failed_cid = gprs_connection->cid;

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	LOGD("Waiting for IP configuration!");
}

void ipc_gprs_define_pdp_context_complete(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *phone_res = (struct ipc_gen_phone_res *) info->data;
	struct ril_gprs_connection *gprs_connection;
	int aseq;
	int rc;

	gprs_connection = ril_gprs_connection_get_token(reqGetToken(info->aseq));

	if(!gprs_connection) {
		LOGE("Unable to find GPRS connection, aborting");

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	rc = ipc_gen_phone_res_check(phone_res);
	if(rc < 0) {
		LOGE("There was an error, aborting define PDP context complete");

		gprs_connection->fail_cause = PDP_FAIL_ERROR_UNSPECIFIED;
		gprs_connection->token = (RIL_Token) 0x00;
		ril_state.gprs_last_failed_cid = gprs_connection->cid;

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	// We need to get a clean new aseq here
	aseq = ril_request_reg_id(reqGetToken(info->aseq));

	ipc_gen_phone_res_expect_to_func(aseq, IPC_GPRS_PDP_CONTEXT,
		ipc_gprs_pdp_context_enable_complete);

	ipc_fmt_send(IPC_GPRS_PDP_CONTEXT, IPC_TYPE_SET,
			(void *) &(gprs_connection->context),
			sizeof(struct ipc_gprs_pdp_context_set), aseq);
}

void ipc_gprs_port_list_complete(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *phone_res = (struct ipc_gen_phone_res *) info->data;
	struct ril_gprs_connection *gprs_connection;
	int rc;
	int aseq;

	gprs_connection = ril_gprs_connection_get_token(reqGetToken(info->aseq));

	if(!gprs_connection) {
		LOGE("Unable to find GPRS connection, aborting");

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	rc = ipc_gen_phone_res_check(phone_res);
	if(rc < 0) {
		LOGE("There was an error, aborting port list complete");

		gprs_connection->fail_cause = PDP_FAIL_ERROR_UNSPECIFIED;
		gprs_connection->token = (RIL_Token) 0x00;
		ril_state.gprs_last_failed_cid = gprs_connection->cid;

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	// We need to get a clean new aseq here
	aseq = ril_request_reg_id(reqGetToken(info->aseq));

	ipc_gen_phone_res_expect_to_func(aseq, IPC_GPRS_DEFINE_PDP_CONTEXT,
		ipc_gprs_define_pdp_context_complete);

	ipc_fmt_send(IPC_GPRS_DEFINE_PDP_CONTEXT, IPC_TYPE_SET,
		(void *) &(gprs_connection->define_context),
		sizeof(struct ipc_gprs_define_pdp_context),
		aseq);
}

void ril_request_setup_data_call(RIL_Token t, void *data, int length)
{
	struct ril_gprs_connection *gprs_connection = NULL;
	struct ipc_client_gprs_capabilities gprs_capabilities;
	struct ipc_gprs_port_list port_list;
	struct ipc_client *ipc_client;

	char *username = NULL;
	char *password = NULL;
	char *apn = NULL;

	ipc_client = ((struct ipc_client_object *) ipc_fmt_client->object)->ipc_client;

	apn = ((char **) data)[2];
	username = ((char **) data)[3];
	password = ((char **) data)[4];

	LOGD("Requesting data connection to APN '%s'\n", apn);

	gprs_connection = ril_gprs_connection_add();

	if(!gprs_connection) {
		LOGE("Unable to create GPRS connection, aborting");

		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	gprs_connection->token = t;

	// Create the structs with the apn
	ipc_gprs_define_pdp_context_setup(&(gprs_connection->define_context),
		gprs_connection->cid, 1, apn);

	// Create the structs with the username/password tuple
	ipc_gprs_pdp_context_setup(&(gprs_connection->context),
		gprs_connection->cid, 1, username, password);

	ipc_client_gprs_get_capabilities(ipc_client, &gprs_capabilities);

	// If the device has the capability, deal with port list
	if(gprs_capabilities.port_list) {
		ipc_gprs_port_list_setup(&port_list);

		ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_GPRS_PORT_LIST,
			ipc_gprs_port_list_complete);

		ipc_fmt_send(IPC_GPRS_PORT_LIST, IPC_TYPE_SET,
			(void *) &port_list, sizeof(struct ipc_gprs_port_list), reqGetId(t));
	} else {
		ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_GPRS_DEFINE_PDP_CONTEXT,
			ipc_gprs_define_pdp_context_complete);

		ipc_fmt_send(IPC_GPRS_DEFINE_PDP_CONTEXT, IPC_TYPE_SET,
			(void *) &(gprs_connection->define_context),
				sizeof(struct ipc_gprs_define_pdp_context), reqGetId(t));
	}
}

void ipc_gprs_ip_configuration(struct ipc_message_info *info)
{
	struct ril_gprs_connection *gprs_connection;
        struct ipc_gprs_ip_configuration *ip_configuration =
		(struct ipc_gprs_ip_configuration *) info->data;

	gprs_connection = ril_gprs_connection_get_cid(ip_configuration->cid);

	if(!gprs_connection) {
		LOGE("Unable to find GPRS connection, aborting");

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	LOGD("Obtained IP Configuration");

	// Copy the obtained IP configuration to the GPRS connection structure
	memcpy(&(gprs_connection->ip_configuration),
		ip_configuration, sizeof(struct ipc_gprs_ip_configuration));

	LOGD("Waiting for GPRS call status");
}

void ipc_gprs_pdp_context_disable_complete(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *phone_res = (struct ipc_gen_phone_res *) info->data;
	struct ril_gprs_connection *gprs_connection;
	int rc;

	gprs_connection = ril_gprs_connection_get_token(reqGetToken(info->aseq));

	if(!gprs_connection) {
		LOGE("Unable to find GPRS connection, aborting");

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	rc = ipc_gen_phone_res_check(phone_res);
	if(rc < 0) {
		LOGE("There was an error, aborting PDP context complete");

		// RILJ is not going to ask for fail reason
		ril_gprs_connection_del(gprs_connection);

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	LOGD("Waiting for GPRS call status");
}

void ril_request_deactivate_data_call(RIL_Token t, void *data, int length)
{
	struct ril_gprs_connection *gprs_connection;
	struct ipc_gprs_pdp_context_set context;

	char *cid = ((char **) data)[0];
	int rc;

	gprs_connection = ril_gprs_connection_get_cid(atoi(cid));

	if(!gprs_connection) {
		LOGE("Unable to find GPRS connection, aborting");

		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	gprs_connection->token = t;

	ipc_gprs_pdp_context_setup(&context, gprs_connection->cid, 0, NULL, NULL);

	ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_GPRS_PDP_CONTEXT,
		ipc_gprs_pdp_context_disable_complete);

	ipc_fmt_send(IPC_GPRS_PDP_CONTEXT, IPC_TYPE_SET,
		(void *) &context, sizeof(struct ipc_gprs_pdp_context_set), reqGetId(t));
}

int ipc_gprs_connection_enable(struct ril_gprs_connection *gprs_connection,
	RIL_Data_Call_Response *setup_data_call_response)
{
	struct ipc_client *ipc_client;
        struct ipc_gprs_ip_configuration *ip_configuration;

	char *interface = NULL;
	char *ip;
	char *gateway;
	char *subnet_mask;
	char *dns1;
	char *dns2;

	char prop_name[PROPERTY_KEY_MAX];

	int rc;

	ipc_client = ((struct ipc_client_object *) ipc_fmt_client->object)->ipc_client;

	ip_configuration = &(gprs_connection->ip_configuration);

	asprintf(&ip, "%i.%i.%i.%i",
		(ip_configuration->ip)[0],
		(ip_configuration->ip)[1],
		(ip_configuration->ip)[2],
		(ip_configuration->ip)[3]);

	// FIXME: gateway isn't reliable!
	asprintf(&gateway, "%i.%i.%i.%i",
		(ip_configuration->ip)[0],
		(ip_configuration->ip)[1],
		(ip_configuration->ip)[2],
		(ip_configuration->ip)[3]);
	// FIXME: subnet isn't reliable!
    asprintf(&subnet_mask, "255.255.255.255");
    
	asprintf(&dns1, "%i.%i.%i.%i",
		(ip_configuration->dns1)[0],
		(ip_configuration->dns1)[1],
		(ip_configuration->dns1)[2],
		(ip_configuration->dns1)[3]);
    asprintf(&dns2, "%i.%i.%i.%i",
		(ip_configuration->dns2)[0],
		(ip_configuration->dns2)[1],
		(ip_configuration->dns2)[2],
		(ip_configuration->dns2)[3]);	

	if(ipc_client_gprs_handlers_available(ipc_client)) {
		rc = ipc_client_gprs_activate(ipc_client);
		if(rc < 0) {
			// This is not a critical issue
			LOGE("Failed to activate interface!");
		}
	}

	rc = ipc_client_gprs_get_iface(ipc_client, &interface, gprs_connection->cid);
	if(rc < 0) {
		// This is not a critical issue, fallback to rmnet
		LOGE("Failed to get interface name!");
		asprintf(&interface, "rmnet%d", gprs_connection->cid - 1);
	}

	if(gprs_connection->interface == NULL && interface != NULL) {
		gprs_connection->interface = strdup(interface);
	}

	LOGD("Using net interface: %s\n", interface);

        LOGD("GPRS configuration: iface: %s, ip:%s, "
			"gateway:%s, subnet_mask:%s, dns1:%s, dns2:%s",
		interface, ip, gateway, subnet_mask, dns1, dns2);

	int subnet_addr = inet_addr(subnet_mask);
	#if RIL_VERSION >= 6
		subnet_addr = ipv4NetmaskToPrefixLength(subnet_addr);
	#endif

	rc = ifc_configure(interface, inet_addr(ip),
		subnet_addr,
		inet_addr(gateway),
		inet_addr(dns1), inet_addr(dns2));

	if(rc < 0) {
		LOGE("ifc_configure failed");

		free(interface);
		return -1;
	}

	snprintf(prop_name, PROPERTY_KEY_MAX, "net.%s.dns1", interface);
	property_set(prop_name, dns1);
	snprintf(prop_name, PROPERTY_KEY_MAX, "net.%s.dns2", interface);
	property_set(prop_name, dns2);
	snprintf(prop_name, PROPERTY_KEY_MAX, "net.%s.gw", interface);
	property_set(prop_name, gateway);

	setup_data_call_response->cid = gprs_connection->cid;
	setup_data_call_response->active = 1;
	setup_data_call_response->type = strdup("IP");

#if RIL_VERSION >= 6
	setup_data_call_response->status = 0;
	setup_data_call_response->ifname = interface;
	setup_data_call_response->addresses = ip;
	setup_data_call_response->gateways = gateway;
	asprintf(&setup_data_call_response->dnses, "%s %s", dns1, dns2);
#else
	setup_data_call_response->address = ip;
	free(gateway);
#endif
	
	free(subnet_mask);
	free(dns1);
	free(dns2);

	return 0;
}

int ipc_gprs_connection_disable(struct ril_gprs_connection *gprs_connection)
{
	struct ipc_client *ipc_client;

	char *interface;
	int rc;

	ipc_client = ((struct ipc_client_object *) ipc_fmt_client->object)->ipc_client;

	if(gprs_connection->interface == NULL) {
		rc = ipc_client_gprs_get_iface(ipc_client, &interface, gprs_connection->cid);
		if(rc < 0) {
			// This is not a critical issue, fallback to rmnet
			LOGE("Failed to get interface name!");
			asprintf(&interface, "rmnet%d", gprs_connection->cid);
		}
	} else {
		interface = gprs_connection->interface;
	}

	LOGD("Using net interface: %s\n", interface);

	rc = ifc_down(interface);

	if(gprs_connection->interface == NULL)
		free(interface);

	if(rc < 0) {
		LOGE("ifc_down failed");
	}

	if(ipc_client_gprs_handlers_available(ipc_client)) {
		rc = ipc_client_gprs_deactivate(ipc_client);
		if(rc < 0) {
			// This is not a critical issue
			LOGE("Failed to deactivate interface!");
		}
	}

	return 0;
}

static void cleanup_ril_data_call_response(RIL_Data_Call_Response *response) {
	if (!response) {
		return;
	}
	free(response->type);
#if RIL_VERSION < 6
	free(response->apn);
	free(response->address);
#else
	free(response->addresses);
	free(response->ifname);
	free(response->dnses);
	free(response->gateways);
#endif
}

void ipc_gprs_call_status(struct ipc_message_info *info)
{
	struct ril_gprs_connection *gprs_connection;
	struct ipc_gprs_call_status *call_status =
		(struct ipc_gprs_call_status *) info->data;

	RIL_Data_Call_Response setup_data_call_response;
	memset(&setup_data_call_response, 0, sizeof(setup_data_call_response));
	int rc;

	gprs_connection = ril_gprs_connection_get_cid(call_status->cid);

	if(!gprs_connection) {
		LOGE("Unable to find GPRS connection, aborting");

		RIL_onRequestComplete(reqGetToken(info->aseq),
			RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	if(call_status->fail_cause == 0) {
		if(!gprs_connection->enabled &&
			call_status->state == IPC_GPRS_STATE_ENABLED &&
			gprs_connection->token != (RIL_Token) 0x00) {
			LOGD("GPRS connection is now enabled");

			rc = ipc_gprs_connection_enable(gprs_connection,
				&setup_data_call_response);
			if(rc < 0) {
				LOGE("Failed to enable and configure GPRS interface");

				gprs_connection->enabled = 0;
				gprs_connection->fail_cause = PDP_FAIL_ERROR_UNSPECIFIED;
				ril_state.gprs_last_failed_cid = gprs_connection->cid;

				RIL_onRequestComplete(gprs_connection->token,
					RIL_E_GENERIC_FAILURE, NULL, 0);
			} else {
				LOGD("GPRS interface enabled");

				gprs_connection->enabled = 1;

				RIL_onRequestComplete(gprs_connection->token,
					RIL_E_SUCCESS, &setup_data_call_response,
					sizeof(setup_data_call_response));
				gprs_connection->token = (RIL_Token) 0x00;
			}
			cleanup_ril_data_call_response(&setup_data_call_response);
		} else if(gprs_connection->enabled &&
			call_status->state == IPC_GPRS_STATE_DISABLED &&
			gprs_connection->token != (RIL_Token) 0x00) {
			LOGD("GPRS connection is now disabled");

			rc = ipc_gprs_connection_disable(gprs_connection);
			if(rc < 0) {
				LOGE("Failed to disable GPRS interface");

				RIL_onRequestComplete(gprs_connection->token,
					RIL_E_GENERIC_FAILURE, NULL, 0);

				// RILJ is not going to ask for fail reason
				ril_gprs_connection_del(gprs_connection);
			} else {
				LOGD("GPRS interface disabled");

				gprs_connection->enabled = 0;

				RIL_onRequestComplete(gprs_connection->token,
					RIL_E_SUCCESS, NULL, 0);

				ril_gprs_connection_del(gprs_connection);
			}
		} else {
			LOGE("GPRS connection reported as changed though state is not OK:"
			"\n\tgprs_connection->enabled=%d\n\tgprs_connection->token=0x%x",
				gprs_connection->enabled, (unsigned)gprs_connection->token);

			ril_unsol_data_call_list_changed();
		}
	} else {
		if(!gprs_connection->enabled &&
			(call_status->state == IPC_GPRS_STATE_NOT_ENABLED ||
			call_status->state == IPC_GPRS_STATE_DISABLED) &&
			gprs_connection->token != (RIL_Token) 0x00) {
			LOGE("Failed to enable GPRS connection");

			gprs_connection->enabled = 0;
			gprs_connection->fail_cause =
				ipc2ril_gprs_fail_cause(call_status->fail_cause);
			ril_state.gprs_last_failed_cid = gprs_connection->cid;

			RIL_onRequestComplete(gprs_connection->token,
				RIL_E_GENERIC_FAILURE, NULL, 0);
			gprs_connection->token = (RIL_Token) 0x00;

			ril_unsol_data_call_list_changed();
		} else if(gprs_connection->enabled &&
			call_status->state == IPC_GPRS_STATE_DISABLED) {
			LOGE("GPRS connection suddently got disabled");

			rc = ipc_gprs_connection_disable(gprs_connection);
			if(rc < 0) {
				LOGE("Failed to disable GPRS interface");

				// RILJ is not going to ask for fail reason
				ril_gprs_connection_del(gprs_connection);
			} else {
				LOGE("GPRS interface disabled");

				gprs_connection->enabled = 0;
				ril_gprs_connection_del(gprs_connection);
			}

			ril_unsol_data_call_list_changed();
		} else {
			LOGE("GPRS connection reported to have failed though state is OK:"
			"\n\tgprs_connection->enabled=%d\n\tgprs_connection->token=0x%x",
				gprs_connection->enabled, (unsigned)gprs_connection->token);

			ril_unsol_data_call_list_changed();
		}
	}
}

void ril_request_last_data_call_fail_cause(RIL_Token t)
{
	struct ril_gprs_connection *gprs_connection;
	int last_failed_cid;
	int fail_cause;

	last_failed_cid = ril_state.gprs_last_failed_cid;

	if(!last_failed_cid) {
		LOGE("No GPRS connection was reported to have failed");

		goto fail_cause_unspecified;
	}

	gprs_connection = ril_gprs_connection_get_cid(last_failed_cid);

	if(!gprs_connection) {
		LOGE("Unable to find GPRS connection");

		goto fail_cause_unspecified;
	}

	fail_cause = gprs_connection->fail_cause;

	LOGD("Destroying GPRS connection with cid: %d", gprs_connection->cid);
	ril_gprs_connection_del(gprs_connection);

	goto fail_cause_return;

fail_cause_unspecified:
	fail_cause = PDP_FAIL_ERROR_UNSPECIFIED;

fail_cause_return:
	ril_state.gprs_last_failed_cid = 0;
	RIL_onRequestComplete(t, RIL_E_SUCCESS, &fail_cause, sizeof(fail_cause));
}

/*
 * Some modem firmwares have a bug that will make the first cid (1) overriden
 * by the current cid, thus reporting it twice, with a wrong 2nd status.
 *
 * This shouldn't change anything to healthy structures.
 */
void ipc_gprs_pdp_context_fix(RIL_Data_Call_Response *data_call_list, int c)
{
	int i, j, k;

	for(i=0 ; i < c ; i++) {
		for(j=i-1 ; j >= 0 ; j--) {
			if(data_call_list[i].cid == data_call_list[j].cid) {
				for(k=0 ; k < c ; k++) {
					if(data_call_list[k].cid == 1) {
						data_call_list[i].cid = 0;
						break;
					}
				}

				data_call_list[i].cid = 1;
			}
		}
	}
}

void ipc_gprs_pdp_context(struct ipc_message_info *info)
{
	struct ril_gprs_connection *gprs_connection;
        struct ipc_gprs_ip_configuration *ip_configuration;
	struct ipc_gprs_pdp_context_get *context =
		(struct ipc_gprs_pdp_context_get *) info->data;

	RIL_Data_Call_Response data_call_list[IPC_GPRS_PDP_CONTEXT_GET_DESC_COUNT];
	memset(data_call_list, 0, sizeof(data_call_list));

	int i;

	for(i=0 ; i < IPC_GPRS_PDP_CONTEXT_GET_DESC_COUNT ; i++) {
		data_call_list[i].cid = context->desc[i].cid;
		data_call_list[i].active =
			ipc2ril_gprs_connection_active(context->desc[i].state);

		if(context->desc[i].state == IPC_GPRS_STATE_ENABLED) {
			gprs_connection = ril_gprs_connection_get_cid(context->desc[i].cid);

			if(gprs_connection == NULL) {
				LOGE("CID %d reported as enabled but not listed here",
					context->desc[i].cid);
				continue;
			}

			ip_configuration = &(gprs_connection->ip_configuration);

			char *addr = NULL;
			asprintf(&addr, "%i.%i.%i.%i",
				(ip_configuration->ip)[0],
				(ip_configuration->ip)[1],
				(ip_configuration->ip)[2],
				(ip_configuration->ip)[3]);

			RIL_Data_Call_Response *resp = &data_call_list[i];
			resp->type = strdup("IP");

			#if RIL_VERSION < 6
			resp->address = addr;
			asprintf(&(resp->apn), "%s",
				gprs_connection->define_context.apn);
			#else
			resp->addresses = addr;
			resp->gateways = strdup(addr);
			resp->ifname = strdup(gprs_connection->interface);
			asprintf(&resp->dnses, "%i.%i.%i.%i %i.%i.%i.%i",
				ip_configuration->dns1[0],
				ip_configuration->dns1[1],
				ip_configuration->dns1[2],
				ip_configuration->dns1[3],

				ip_configuration->dns2[0],
				ip_configuration->dns2[1],
				ip_configuration->dns2[2],
				ip_configuration->dns2[3]);
			#endif
		}
	}

	ipc_gprs_pdp_context_fix(data_call_list, IPC_GPRS_PDP_CONTEXT_GET_DESC_COUNT);

	if(info->aseq == 0xff)
		RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
			&data_call_list, sizeof(data_call_list));
	else
		RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_SUCCESS,
			&data_call_list, sizeof(data_call_list));

	for(i = 0; i < IPC_GPRS_PDP_CONTEXT_GET_DESC_COUNT; i++) {
		cleanup_ril_data_call_response(data_call_list + i);
	}
}

void ril_unsol_data_call_list_changed(void)
{
	ipc_fmt_send_get(IPC_GPRS_PDP_CONTEXT, 0xff);
}

void ril_request_data_call_list(RIL_Token t)
{
	ipc_fmt_send_get(IPC_GPRS_PDP_CONTEXT, reqGetId(t));
}
