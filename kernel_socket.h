#ifndef KERNEL_SOCKET_H
#define KERNEL_SOCKET_H

#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_cc.h"

typedef struct socket_control_block SOCKET_CB;

typedef enum socket_types{
	SOCKET_UNBOUND,
	SOCKET_PEER,
	SOCKET_LISTENER
} socket_type;

typedef struct unbound_socket_type{
	rlnode unbound_socket;
} unbound_socket;

typedef struct peer_socket_type{
	SOCKET_CB* peer;
	PIPE_CB* write_pipe;
	PIPE_CB* read_pipe;
} peer_socket;

typedef struct listener_socket_type{
	rlnode queue;
	CondVar req_available;
} listener_socket;

typedef struct socket_control_block{
	
	uint refcount;
	FCB* fcb;
	socket_type type;
	port_t port;

	union{
		unbound_socket unbound_s;
		listener_socket listener_s;
		peer_socket peer_s;
	};

} SOCKET_CB;

typedef struct connection_request{

	int admitted;
	SOCKET_CB* peer;

	CondVar connected_cv;
	rlnode queue_node;

} CON_REQ;

SOCKET_CB* PORT_MAP[MAX_PORT + 1];


Fid_t sys_Socket(port_t port);

int sys_Listen(Fid_t sock);

Fid_t sys_Accept (Fid_t lsock);

int sys_Connect(Fid_t sock, port_t port, timeout_t timeout);

int sys_Shutdown(Fid_t sock, shutdown_mode how);


void SCB_decref(SOCKET_CB* socket_cb);

SOCKET_CB* get_SCB(Fid_t sock);

int socket_read(void* socketcb_t, char *buf, unsigned int size);

int socket_write(void* socketcb_t, const char *buf, unsigned int size);

int socket_close(void* _socketcb);

#endif