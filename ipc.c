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

#define LOG_TAG "RIL-IPC"
#include <utils/Log.h>

#include "samsung-ril.h"
#include <radio.h>

void ipc_log_handler(const char *message, void *user_data)
{
	LOGD("ipc: %s", message);
}

void ipc_fmt_send(const unsigned short command, const char type, unsigned char *data, const int length, unsigned char mseq)
{
	struct ipc_client *ipc_client;

	if(ipc_fmt_client == NULL) {
		LOGE("ipc_fmt_client is null, aborting!");
		return;
	}

	if(ipc_fmt_client->object == NULL) {
		LOGE("ipc_fmt_client object is null, aborting!");
		return;
	}

	ipc_client = ((struct ipc_client_object *) ipc_fmt_client->object)->ipc_client;

	pthread_mutex_lock(&(ipc_fmt_client->mutex));
	ipc_client_send(ipc_client, command, type, data, length, mseq);
	pthread_mutex_unlock(&(ipc_fmt_client->mutex));
}

int ipc_fmt_read_loop(struct ril_client *client)
{
	struct ipc_message_info info;
	struct ipc_client *ipc_client;
	int ipc_client_fd;
	fd_set fds;

	if(client == NULL) {
		LOGE("client is NULL, aborting!");
		return -1;
	}

	if(client->object == NULL) {
		LOGE("client object is NULL, aborting!");
		return -1;
	}

	ipc_client = ((struct ipc_client_object *) client->object)->ipc_client;
	ipc_client_fd = ((struct ipc_client_object *) client->object)->ipc_client_fd;

	FD_ZERO(&fds);
	FD_SET(ipc_client_fd, &fds);

	while(1) {
		if(ipc_client_fd < 0) {
			LOGE("IPC FMT client fd is negative, aborting!");
			return -1;
		}

		select(FD_SETSIZE, &fds, NULL, NULL, NULL);

		if(FD_ISSET(ipc_client_fd, &fds)) {
			if(ipc_client_recv(ipc_client, &info)) {
				LOGE("IPC FMT recv failed, aborting!");
				return -1;
			}
			LOGD("IPC FMT recv: aseq=0x%x mseq=0x%x data_length=%d\n", info.aseq, info.mseq, info.length);

			ipc_fmt_dispatch(&info);

			if(info.data != NULL)
				free(info.data);
		}
	}

	return 0;
}

int ipc_fmt_create(struct ril_client *client)
{
	struct ipc_client_object *client_object;
	struct ipc_client *ipc_client;
	int ipc_client_fd;
	int rc;

	client_object = malloc(sizeof(struct ipc_client_object));
	memset(client_object, 0, sizeof(struct ipc_client_object));
	client_object->ipc_client_fd = -1;

	client->object = client_object;

	ipc_client = (struct ipc_client *) client_object->ipc_client;

	LOGD("Creating new FMT client");
	ipc_client = ipc_client_new(IPC_CLIENT_TYPE_FMT);

	if(ipc_client == NULL) {
		LOGE("FMT client creation failed!");
		return -1;
	}

	client_object->ipc_client = ipc_client;

	LOGD("Setting log handler");
	rc = ipc_client_set_log_handler(ipc_client, ipc_log_handler, NULL);

	if(rc < 0) {
		LOGE("Setting log handler failed!");
		return -1;
	}

	//FIXME: ipc_client_set_handler

	LOGD("Passing client->object->ipc_client fd as handlers data");
	rc = ipc_client_set_all_handlers_data(ipc_client, &((struct ipc_client_object *) client->object)->ipc_client_fd);

	if(rc < 0) {
		LOGE("ipc_client_fd as handlers data failed!");
		return -1;
	}


	LOGD("Starting modem bootstrap");
	rc = ipc_client_bootstrap_modem(ipc_client);

	if(rc < 0) {
		LOGE("Modem bootstrap failed!");
		return -1;
	}

	LOGD("Client open...");
	if(ipc_client_open(ipc_client)) {
		LOGE("%s: failed to open ipc client", __FUNCTION__);
		return -1;
	}

	ipc_client_fd = ((struct ipc_client_object *) client->object)->ipc_client_fd;

	if(ipc_client_fd < 0) {
		LOGE("%s: client_fmt_fd is negative, aborting", __FUNCTION__);
		return -1;
	}

	LOGD("Client power on...");
	if(ipc_client_power_on(ipc_client)) {
		LOGE("%s: failed to power on ipc client", __FUNCTION__);
		return -1;
	}

	return 0;
}

int ipc_fmt_destroy(struct ril_client *client)
{
	struct ipc_client *ipc_client;
	int ipc_client_fd;
	int rc;

	LOGD("Destrying ipc fmt client");

	if(client == NULL) {
		LOGE("client was already destroyed");
		return 0;
	}

	if(client->object == NULL) {
		LOGE("client object was already destroyed");
		return 0;
	}

	ipc_client_fd = ((struct ipc_client_object *) client->object)->ipc_client_fd;

	if(ipc_client_fd)
		close(ipc_client_fd);

	ipc_client = ((struct ipc_client_object *) client->object)->ipc_client;

	if(ipc_client != NULL) {
		ipc_client_power_off(ipc_client);
		ipc_client_close(ipc_client);
		ipc_client_free(ipc_client);
	}

	free(client->object);

	return 0;
}

struct ril_client_funcs ipc_fmt_client_funcs = {
	.create = ipc_fmt_create,
	.destroy = ipc_fmt_destroy,
	.read_loop = ipc_fmt_read_loop,
};
