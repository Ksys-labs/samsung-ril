#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <cutils/sockets.h>

#include <cutils/log.h>

#include <samsung-ril-socket.h>
#include "socket.h"

struct srs_server *srs_server_new(void)
{
	struct srs_server *srs_server;

	srs_server = malloc(sizeof(struct srs_server));
	memset(srs_server, 0, sizeof(struct srs_server));
	srs_server->server_fd = -1;
	srs_server->client_fd = -1;

	return srs_server;
}

int srs_server_send_message(struct srs_server *srs_server, struct srs_message *message)
{
	fd_set fds;

	struct srs_header header;
	void *data;

	header.length = message->data_len + sizeof(header);
	header.group = SRS_GROUP(message->command);
	header.index = SRS_INDEX(message->command);
	header.msg_id = message->msg_id;

	data = malloc(header.length);
	memset(data, 0, header.length);

	memcpy(data, &header, sizeof(header));
	memcpy((void *) (data + sizeof(header)), message->data, message->data_len);

	FD_ZERO(&fds);
	FD_SET(srs_server->client_fd, &fds);

	select(FD_SETSIZE, NULL, &fds, NULL, NULL);

	write(srs_server->client_fd, data, header.length);

	//FIXME: can we free?

	return 0;
}

int srs_server_send(struct srs_server *srs_server, unsigned short command, void *data, int data_len, unsigned msg_id)
{
	struct srs_message message;
	int rc;

	message.command = command;
	message.data = data;
	message.data_len = data_len;
	message.msg_id = msg_id;

	rc = srs_server_send_message(srs_server, &message);

	return rc;
}

int srs_server_recv(struct srs_server *srs_server, struct srs_message *message)
{
	void *raw_data = malloc(SRS_DATA_MAX_SIZE);
	struct srs_header *header;
	int rc;

	rc = read(srs_server->client_fd, raw_data, SRS_DATA_MAX_SIZE);
	if(rc < sizeof(struct srs_header)) {
		return -1;
	}

	header = raw_data;

	message->command = SRS_COMMAND(header);
	message->msg_id = header->msg_id;
	message->data_len = header->length - sizeof(struct srs_header);
	message->data = malloc(message->data_len);

	memcpy(message->data, raw_data + sizeof(struct srs_header), message->data_len);

	free(raw_data);

	return 0;
}

int srs_server_accept(struct srs_server *srs_server)
{
	int client_fd = -1;
	struct sockaddr_un client_addr;
	int client_addr_len;

	if(srs_server->client_fd > 0) {
		return 0;
	}

	client_fd = accept(srs_server->server_fd, (struct sockaddr_un *) &client_addr, &client_addr_len);

	if(client_fd > 0) {
		srs_server->client_fd = client_fd;
		srs_server->client_addr = client_addr;
		srs_server->client_addr_len = client_addr_len;

		return 0;
	}

	return -1;
}

int srs_server_open(struct srs_server *srs_server)
{
	int server_fd = -1;

	int t = 0;

	while(t < 5) {
		unlink(SRS_SOCKET_NAME);
		server_fd = socket_local_server(SRS_SOCKET_NAME, ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);

		if(server_fd > 0)
			break;

		t++;
	}
	
	if(server_fd < 0)
		return -1;

	srs_server->server_fd = server_fd;

	return 0;
}
