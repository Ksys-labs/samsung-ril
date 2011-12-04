#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <arpa/inet.h>
#include <netinet/in.h>

struct srs_server {
	int server_fd;
	int client_fd;
	struct sockaddr_un client_addr;
	int client_addr_len;
};

struct srs_server *srs_server_new(void);
int srs_server_send(struct srs_server *srs_server, unsigned short command, void *data, int data_len, unsigned msg_id);
int srs_server_recv(struct srs_server *srs_server, struct srs_message *message);
int srs_server_accept(struct srs_server *srs_server);
int srs_server_open(struct srs_server *srs_server);

#endif
