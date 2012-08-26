/**
 * This file is part of samsung-ril.
 *
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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <cutils/sockets.h>

#define LOG_TAG "RIL-SRS"
#include <utils/Log.h>

#include "samsung-ril.h"
#include "util.h"

static int srs_server_send_message(int client_fd, struct srs_message *message)
{
	fd_set fds;

	struct srs_header header;
	void *data;

	header.length = message->data_len + sizeof(header);
	header.group = SRS_GROUP(message->command);
	header.index = SRS_INDEX(message->command);

	data = malloc(header.length);
	memset(data, 0, header.length);

	memcpy(data, &header, sizeof(header));
	memcpy((void *) ((char*)data + sizeof(header)),
		message->data, message->data_len);

	FD_ZERO(&fds);
	FD_SET(client_fd, &fds);

	select(FD_SETSIZE, NULL, &fds, NULL, NULL);

	write(client_fd, data, header.length);

	free(data);

	return 0;
}

static int srs_server_send(int fd, unsigned short command, void *data,
	int data_len)
{
	struct srs_message message;
	int rc;

	message.command = command;
	message.data = data;
	message.data_len = data_len;

	rc = srs_server_send_message(fd, &message);

	return rc;
}

static int srs_server_recv(int client_fd, struct srs_message *message)
{
	void *raw_data = malloc(SRS_DATA_MAX_SIZE);
	struct srs_header *header;
	int rc;

	rc = read(client_fd, raw_data, SRS_DATA_MAX_SIZE);
	if(rc < (int)sizeof(struct srs_header)) {
		return -1;
	}

	header = raw_data;

	message->command = SRS_COMMAND(header);
	message->data_len = header->length - sizeof(struct srs_header);
	message->data = malloc(message->data_len);

	memcpy(message->data, (char*)raw_data + sizeof(struct srs_header),
		message->data_len);

	free(raw_data);

	return 0;
}

void srs_control_ping(int fd, struct srs_message *message)
{
	int caffe;

	if(message->data == NULL)
		return;

	caffe=*((int *) message->data);

	if(caffe == SRS_CONTROL_CAFFE) {
		srs_server_send(fd, SRS_CONTROL_PING, &caffe, sizeof(caffe));
	}
}

static int srs_server_open(void)
{
	int server_fd = -1;

	int t = 0;

	while(t < 5) {
		unlink(SRS_SOCKET_NAME);
		server_fd = socket_local_server(SRS_SOCKET_NAME, ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);

		if(server_fd >= 0)
			break;

		t++;
	}

	return server_fd;
}

static void* srs_process_client(void *pfd)
{
	struct srs_message srs_message;
	fd_set fds;
	int client_fd = -1;
	if (!pfd) {
		LOGE("SRS client data is NULL");
		goto fail;
	}

	client_fd = ((int*)pfd)[0];

	while (1) {
		if (client_fd < 0)
			break;

		FD_ZERO(&fds);
		FD_SET(client_fd, &fds);

		select(FD_SETSIZE, &fds, NULL, NULL, NULL);

		if (FD_ISSET(client_fd, &fds)) {
			if (srs_server_recv(client_fd, &srs_message) < 0) {
				LOGE("SRS recv failed, aborting!");
				break;
			}

			LOGD("SRS recv: command=%d data_len=%d",
			     srs_message.command, srs_message.data_len);
			hex_dump(srs_message.data, srs_message.data_len);

			srs_dispatch(client_fd, &srs_message);

			if (srs_message.data != NULL)
				free(srs_message.data);
		}
	}

fail:
	if(client_fd >= 0) {
		close(client_fd);
	}

	LOGE("SRS server client ended!");
	return NULL;
}

static int srs_read_loop(struct ril_client *client)
{
	int rc;

	struct sockaddr_un client_addr;
	int client_addr_len;

	if(client == NULL) {
		LOGE("client is NULL, aborting!");
		return -1;
	}

	if(client->object == NULL) {
		LOGE("client object is NULL, aborting!");
		return -1;
	}

	int server_fd = ((int*)client->object)[0];

	while(1) {
		if(server_fd < 0) {
			LOGE("SRS client server_fd is negative, aborting!");
			return -1;
		}

		rc = accept(server_fd, (struct sockaddr*)&client_addr,
			&client_addr_len);
		if (rc < 0) {
			LOGE("SRS Failed to accept errno %d error %s",
				errno, strerror(errno));
			return -1;
		}
		LOGI("SRS accepted fd %d", rc);
		int *pfd = (int*)malloc(sizeof(int));
		if (!pfd) {
			LOGE("out of memory for the client socket");
			close(rc);
			return -1;
		}
		*pfd = rc;

		pthread_t t;
		if (pthread_create(&t, NULL, srs_process_client, pfd)) {
			LOGE("SRS failed to start client thread errno %d error %s",
				errno, strerror(errno));
			close(rc);
			return -1;
		}
	}

	return 0;
}

static int srs_create(struct ril_client *client)
{
	int *srs_server = NULL;

	LOGD("Creating new SRS client");

	srs_server = malloc(sizeof(int));
	if (!srs_server) {
		LOGE("SRS out of memory for server fd");
		goto fail;
	}

	client->object = (void *) srs_server;
	if((*srs_server = srs_server_open()) < 0) {
		LOGE("%s: samsung-ril-socket server open failed", __FUNCTION__);
		goto fail;
	}

	return 0;

fail:
	if (srs_server) {
		free(srs_server);
	}
	return -1;
}

static int srs_destroy(struct ril_client *client)
{
	if (!client) {
		return 0;
	}

	int *srs_server = (int*) client->object;
	if (!srs_server) {
		return 0;
	}

	close(*srs_server);
	free(srs_server);

	return 0;
}

struct ril_client_funcs srs_client_funcs = {
	.create = srs_create,
	.destroy = srs_destroy,
	.read_loop = srs_read_loop,
};
