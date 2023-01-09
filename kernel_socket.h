
#include "kernel_streams.h"
#include "kernel_dev.h"
#include "tinyos.h"
#include "kernel_cc.h"
#include "util.h"

typedef enum{
	SOCKET_LISTENER,
	SOCKET_UNBOUND,
	SOCKET_PEER
}socket_type;

typedef struct listenersocket{
	rlnode queue;
	CondVar req_availiable;
}listener_socket;

typedef struct unboundsocket{
	rlnode unbound_socket;
}unbound_socket;

typedef struct peersocket{
	socket_cb* peer;
	pipe_cb* write_pipe;
	pipe_cb* read_pipe;
}peer_socket;

typedef struct socket_control_block{
	uint refcount;
	FCB* fcb;
	Fid_t fid;
	socket_type type;
	port_t port;

	union{
		listener_socket listener_s;
		unbound_socket unbound_s;
		peer_socket peer_s;
	};

}socket_cb;

typedef struct request{
	int admitted;
	socket_cb* peer;
	CondVar connect_cv;
	rlnode queue_node;
}connection_request;

int socket_write(void* this, const char *buf, unsigned int n);

int socket_read(void* this, char *buf, unsigned int n);

int socket_close(void* this);