/**
 * This file is part of samsung-ril.
 *
 * Copyright (C) 2011 Denis 'GNUtoo' Carikli <GNUtoo@no-log.org>
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

#include <netinet/in.h>
#include <arpa/inet.h>

#define LOG_TAG "RIL-GPRS"
#include <utils/Log.h>
#include <cutils/properties.h>

#include "samsung-ril.h"
#include "util.h"

/* FIXME: we don't want these hardcoded */
#define INTERFACE		"rmnet0"
#define IP_STRING_SIZE		15 + 1

// libnetutils missing prototype
extern int ifc_configure(const char *ifname,
	in_addr_t address,
	in_addr_t netmask,
	in_addr_t gateway,
	in_addr_t dns1,
	in_addr_t dns2);

void ipc_gprs_pdp_context_complete(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *phone_res = (struct ipc_gen_phone_res *) info->data;
	int rc;
	int aseq;

	rc = ipc_gen_phone_res_check(phone_res);
	if(rc < 0) {
		LOGE("There was an error, aborting PDP context complete");
		ril_state.tokens.gprs_context = (RIL_Token) 0x00;

		RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	/* We need to get a clean new aseq here */
	aseq = ril_request_reg_id(reqGetToken(info->aseq));

	ipc_gen_phone_res_expect_to_abort(aseq, IPC_GPRS_PDP_CONTEXT);

	/* activate the connection */
	ipc_fmt_send(IPC_GPRS_PDP_CONTEXT, IPC_TYPE_SET, 
			(void *) &(ril_state.gprs_context), sizeof(struct ipc_gprs_pdp_context), aseq);

	// TODO: if this aborts, last fail cause will be: PDP_FAIL_ERROR_UNSPECIFIED
}

void ipc_gprs_port_list_complete(struct ipc_message_info *info)
{
	struct ipc_gen_phone_res *phone_res = (struct ipc_gen_phone_res *) info->data;
	int rc;
	int aseq;

	rc = ipc_gen_phone_res_check(phone_res);
	if(rc < 0) {
		LOGE("There was an error, aborting port list complete");
		ril_state.tokens.gprs_context = (RIL_Token) 0x00;

		RIL_onRequestComplete(reqGetToken(info->aseq), RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
	}

	/* We need to get a clean new aseq here */
	aseq = ril_request_reg_id(reqGetToken(info->aseq));

	ipc_gen_phone_res_expect_to_func(aseq, IPC_GPRS_DEFINE_PDP_CONTEXT,
		ipc_gprs_pdp_context_complete);

	/* send the struct to the modem */
	ipc_fmt_send(IPC_GPRS_DEFINE_PDP_CONTEXT, IPC_TYPE_SET, 
		(void *) &(ril_state.gprs_define_context), sizeof(struct ipc_gprs_define_pdp_context), aseq);
}

void ril_request_setup_data_call(RIL_Token t, void *data, int length)
{
	struct ipc_gprs_port_list port_list;
	struct ipc_client *ipc_client;

	char *username = NULL;
	char *password = NULL;
	char *apn = NULL;

	ipc_client = ((struct ipc_client_object *) ipc_fmt_client->object)->ipc_client;

	/* get the apn, username and password */
	apn = ((char **) data)[2];
	username = ((char **) data)[3];

	if(username != NULL) {
		if(strlen(username) < 2)
			username = "dummy";
	} else {
		username = "dummy";
	}

	password = ((char **) data)[4];

	if(password != NULL) {
		if(strlen(password) < 2)
			password = "dummy";
	} else {
		password = "dummy";
	}

	LOGD("Requesting data connection to APN '%s'\n", apn);

	if(ril_state.tokens.gprs_context != (RIL_Token) 0x00) {
		LOGE("There is already a data call request going on!");

		// TODO: Fill last fail reason!
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}

	ril_state.tokens.gprs_context = t;

	/* create the structs with the apn */
	ipc_gprs_define_pdp_context_setup(&(ril_state.gprs_define_context), apn);

	/* create the structs with the username/password tuple */
	ipc_gprs_pdp_context_setup(&(ril_state.gprs_context), 1, username, password);

	// If handlers are available, deal with port list
	if(ipc_client_gprs_handlers_available(ipc_client)) {
		ipc_gprs_port_list_setup(&port_list);

		ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_GPRS_PORT_LIST,
			ipc_gprs_port_list_complete);

		ipc_fmt_send(IPC_GPRS_PORT_LIST, IPC_TYPE_SET, 
			(void *) &port_list, sizeof(struct ipc_gprs_port_list), reqGetId(t));
	} else {
		ipc_gen_phone_res_expect_to_func(reqGetId(t), IPC_GPRS_DEFINE_PDP_CONTEXT,
			ipc_gprs_pdp_context_complete);

		/* send the struct to the modem */
		ipc_fmt_send(IPC_GPRS_DEFINE_PDP_CONTEXT, IPC_TYPE_SET, 
			(void *) &(ril_state.gprs_define_context), sizeof(struct ipc_gprs_define_pdp_context), reqGetId(t));
	}
}

void ipc_gprs_ip_configuration(struct ipc_message_info *info)
{
	/* Quick and dirty configuration, TODO: Handle that better */

        struct ipc_gprs_ip_configuration *ip_config = (struct ipc_gprs_ip_configuration *) info->data;
	struct ipc_client *ipc_client;

	char local_ip[IP_STRING_SIZE];
	char gateway[IP_STRING_SIZE];
	char subnet_mask[IP_STRING_SIZE];
	char dns1[IP_STRING_SIZE];
	char dns2[IP_STRING_SIZE];

	char dns_prop_name[PROPERTY_KEY_MAX];
	char gw_prop_name[PROPERTY_KEY_MAX];

	char *interface;

	char *response[3];
	int rc;

	ipc_client = ((struct ipc_client_object *) ipc_fmt_client->object)->ipc_client;

	/* TODO: transform that into some macros */
	snprintf(local_ip, IP_STRING_SIZE, "%i.%i.%i.%i",(ip_config->ip)[0],(ip_config->ip)[1],
						(ip_config->ip)[2],(ip_config->ip)[3]);

        snprintf(gateway, IP_STRING_SIZE, "%i.%i.%i.%i",(ip_config->ip)[0],(ip_config->ip)[1],
                                                (ip_config->ip)[2],(ip_config->ip)[3]);

        snprintf(subnet_mask, IP_STRING_SIZE, "255.255.255.255");

        snprintf(dns1, IP_STRING_SIZE, "%i.%i.%i.%i",(ip_config->dns1)[0],(ip_config->dns1)[1],
                                                (ip_config->dns1)[2],(ip_config->dns1)[3]);

        snprintf(dns2, IP_STRING_SIZE , "%i.%i.%i.%i",(ip_config->dns2)[0],(ip_config->dns2)[1],
                                                (ip_config->dns2)[2],(ip_config->dns2)[3]);

        LOGD("GPRS configuration: ip:%s, gateway:%s, subnet_mask:%s, dns1:%s, dns2:%s",
							local_ip, gateway, subnet_mask ,dns1, dns2);

	if(ipc_client_gprs_handlers_available(ipc_client)) {
		rc = ipc_client_gprs_activate(ipc_client);
		if(rc < 0) {
			// This is not a critical issue
			LOGE("Failed to activate interface!");
		}
	}

	rc = ipc_client_gprs_get_iface(ipc_client, &interface);
	if(rc < 0) {
		// This is not a critical issue, fallback to rmnet0
		LOGE("Failed to get interface name!");
		asprintf(&interface, "rmnet0");
	}

	LOGD("Iface name is %s\n", interface);

	rc = ifc_configure(interface, 
			inet_addr(local_ip),
			inet_addr(subnet_mask),
			inet_addr(gateway),
			inet_addr(dns1),
			inet_addr(dns2));
        LOGD("ifc_configure: %d",rc);

	snprintf(dns_prop_name, sizeof(dns_prop_name), "net.%s.dns1", interface);
	property_set(dns_prop_name, dns1);
	snprintf(dns_prop_name, sizeof(dns_prop_name), "net.%s.dns2", interface);
	property_set(dns_prop_name, dns2);
	snprintf(gw_prop_name, sizeof(gw_prop_name), "net.%s.gw", interface);
	property_set(dns_prop_name, gateway);

	response[0] = "0"; //FIXME: connection id
	response[1] = interface;
	response[2] = local_ip;

	RIL_onRequestComplete(ril_state.tokens.gprs_context, RIL_E_SUCCESS, response, sizeof(response));

	ril_state.tokens.gprs_context = (RIL_Token) 0x00;

	// FIXME: is it wise to free returned data?
	free(interface);
}

void ril_request_deactivate_data_call(RIL_Token t, void *data, int length)
{
	struct ipc_gprs_pdp_context deactivate_message;
	struct ipc_client *ipc_client;
	int rc;

	ipc_client = ((struct ipc_client_object *) ipc_fmt_client->object)->ipc_client;

	memset(&deactivate_message, 0, sizeof(deactivate_message));
	deactivate_message.unk0[1]=1;

	/* send the struct to the modem */
	ipc_fmt_send(IPC_GPRS_PDP_CONTEXT, IPC_TYPE_SET, 
			(void *) &deactivate_message, sizeof(struct ipc_gprs_pdp_context), reqGetId(t));

	if(ipc_client_gprs_handlers_available(ipc_client)) {
		rc = ipc_client_gprs_deactivate(ipc_client);
		if(rc < 0) {
			// This is not a critical issue
			LOGE("Failed to deactivate interface!");
		}
	}

	// Clean the token
	ril_state.tokens.gprs_context = (RIL_Token) 0x00;

	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}
