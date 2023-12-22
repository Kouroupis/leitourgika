#include "tinyos.h"
#include "kernel_socket.h"


static file_ops socket_file_ops = {
	.Open = NULL,
	.Read = socket_read,
	.Write = socket_write,
	.Close = socket_close
};

Fid_t sys_Socket(port_t port)
{
	if(port <= -1 || port >= MAX_PORT + 1)
		return NOFILE;

	Fid_t fid[1];
	FCB* fcb[1];

	if(FCB_reserve(1, fid, fcb) == 0)
		return NOFILE;

	SOCKET_CB* socket_cb = xmalloc(sizeof(SOCKET_CB));
	
	socket_cb->refcount = 0;
	socket_cb->fcb = fcb[0];
	socket_cb->type = SOCKET_UNBOUND;
	socket_cb->port = port;
	
	fcb[0]->streamobj = socket_cb;
	fcb[0]->streamfunc = &socket_file_ops;

	return fid[0];
}

int sys_Listen(Fid_t sock)
{
	SOCKET_CB* socket_cb = get_SCB(sock);

	if(socket_cb == NULL)
		return -1;
	if(socket_cb->port == NOPORT || PORT_MAP[socket_cb->port] != NULL)
		return -1;
	if(socket_cb->type != SOCKET_UNBOUND)
		return -1;

	PORT_MAP[socket_cb->port] = socket_cb;
	socket_cb->type = SOCKET_LISTENER;
	rlnode_init(&socket_cb->listener_s.queue, NULL);
	socket_cb->listener_s.req_available = COND_INIT;

	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	SOCKET_CB* server = get_SCB(lsock);

	if(server == NULL)
		return NOFILE;
	if(PORT_MAP[server->port] == NULL)
		return NOFILE;
	if(server->type != SOCKET_LISTENER)
		return NOFILE;

	while(is_rlist_empty(&server->listener_s.queue) && PORT_MAP[server->port] != NULL){
		kernel_wait(&server->listener_s.req_available, SCHED_IO);
	}

	server->refcount++;

	if(PORT_MAP[server->port] == NULL){
		SCB_decref(server);
		return NOFILE;
	}

	rlnode* client_node = rlist_pop_front(&server->listener_s.queue);
	//assert(client_node != NULL);

	CON_REQ* con_req = client_node->obj;
	
	//CON_REQ* con_req = (CON_REQ*)rlist_pop_front(&server->listener_s.queue)->con_req;
	
	SOCKET_CB* client_peer = con_req->peer;
	Fid_t server_peer_fid = sys_Socket(server->port);

	if(server_peer_fid == NOFILE){
		SCB_decref(server);
		kernel_signal(&con_req->connected_cv);
		return NOFILE;
	}

	con_req->admitted = 1;

	SOCKET_CB* server_peer = get_SCB(server_peer_fid);

	//init pipe_cb1
	PIPE_CB* pipe_cb1 = (PIPE_CB*)xmalloc(sizeof(PIPE_CB));

	if(pipe_cb1 == NULL)
		return NOFILE;

	FCB* fcb[2];

	fcb[0] = client_peer->fcb;
	fcb[1] = server_peer->fcb;

	pipe_cb1->has_space = COND_INIT;
	pipe_cb1->has_data = COND_INIT;
	pipe_cb1->r_position = 0;
	pipe_cb1->w_position = 0;
	pipe_cb1->reader = fcb[0];
	pipe_cb1->writer = fcb[1];
	pipe_cb1->space = PIPE_BUFFER_SIZE;

	//init pipe_cb2
	PIPE_CB* pipe_cb2 = (PIPE_CB*)xmalloc(sizeof(PIPE_CB));

	if(pipe_cb2 == NULL)
		return NOFILE;

	//FCB* fcb[2];

	fcb[0] = server_peer->fcb;
	fcb[1] = client_peer->fcb;

	pipe_cb2->has_space = COND_INIT;
	pipe_cb2->has_data = COND_INIT;
	pipe_cb2->r_position = 0;
	pipe_cb2->w_position = 0;
	pipe_cb2->reader = fcb[0];
	pipe_cb2->writer = fcb[1];
	pipe_cb2->space = PIPE_BUFFER_SIZE;

	//connect peers
	server_peer->type = SOCKET_PEER;
	server_peer->peer_s.read_pipe = pipe_cb2;
	server_peer->peer_s.write_pipe = pipe_cb1;
	server_peer->peer_s.peer = client_peer;

	client_peer->type = SOCKET_PEER;
	client_peer->peer_s.read_pipe = pipe_cb1;
	client_peer->peer_s.write_pipe = pipe_cb2;
	client_peer->peer_s.peer = server_peer;

	kernel_signal(&con_req->connected_cv);

	SCB_decref(server);

	return server_peer_fid;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	SOCKET_CB* peer = get_SCB(sock);

	if(peer == NULL)
		return -1;
	if(peer->type != SOCKET_UNBOUND)
		return -1;
	if(port <= 0 || port >= MAX_PORT)
		return -1;

	SOCKET_CB* server = PORT_MAP[port];

	if(server == NULL || server->type != SOCKET_LISTENER)
		return -1;

	CON_REQ* con_req = (CON_REQ*)xmalloc(sizeof(CON_REQ));
	con_req->admitted = 0;
	con_req->peer = peer;
	con_req->connected_cv = COND_INIT;
	rlnode_init(&con_req->queue_node, con_req);

	rlist_push_back(&server->listener_s.queue, &con_req->queue_node);
	kernel_signal(&server->listener_s.req_available);

	peer->refcount++;

	if(timeout > 0){
		kernel_timedwait(&con_req->connected_cv, SCHED_IO, timeout);
	}
	else{
		kernel_wait(&con_req->connected_cv, SCHED_IO);
	}

	SCB_decref(peer);

	if(con_req->admitted == 0){
		rlist_remove(&con_req->queue_node);
		free(con_req);
		return -1;
	}

	rlist_remove(&con_req->queue_node);
	free(con_req);	

	return 0;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	SOCKET_CB* socket_cb = get_SCB(sock);

	if(socket_cb == NULL || socket_cb->type != SOCKET_PEER)
		return -1;

	switch(how){
		case SHUTDOWN_READ:
			pipe_reader_close(socket_cb->peer_s.read_pipe);
			socket_cb->peer_s.read_pipe = NULL;
			break;
		case SHUTDOWN_WRITE:
			pipe_writer_close(socket_cb->peer_s.write_pipe);
			socket_cb->peer_s.write_pipe = NULL;
			break;
		case SHUTDOWN_BOTH:
			pipe_reader_close(socket_cb->peer_s.read_pipe);
			socket_cb->peer_s.read_pipe = NULL;
			pipe_writer_close(socket_cb->peer_s.write_pipe);
			socket_cb->peer_s.write_pipe = NULL;
			break;
	}

	return 0;
}

int socket_read(void* socketcb_t, char *buf, unsigned int size){

	SOCKET_CB* socket_cb = (SOCKET_CB*) socketcb_t;

	if(socketcb_t == NULL)
		return -1;
	if(socket_cb->type != SOCKET_PEER )
		return -1;
	if(socket_cb->peer_s.read_pipe == NULL)
		return -1;

	PIPE_CB* pipe_cb = socket_cb->peer_s.read_pipe;

	return pipe_read(pipe_cb, buf, size);
}

int socket_write(void* socketcb_t, const char *buf, unsigned int size){

	SOCKET_CB* socket_cb = (SOCKET_CB*) socketcb_t;
	
	if(socketcb_t == NULL)
		return -1;
	if(socket_cb->type != SOCKET_PEER)
		return -1;
	if(socket_cb->peer_s.write_pipe == NULL)
		return -1;

	PIPE_CB* pipe_cb = socket_cb->peer_s.write_pipe;

	return pipe_write(pipe_cb, buf, size);
}

int socket_close(void* _socketcb){

	SOCKET_CB* socket_cb = (SOCKET_CB*) _socketcb;

	if(socket_cb == NULL)
		return -1;
	
	switch(socket_cb->type){
		case SOCKET_PEER:
			pipe_reader_close(socket_cb->peer_s.read_pipe);
			pipe_writer_close(socket_cb->peer_s.write_pipe);
			break;
		case SOCKET_LISTENER:
			while(is_rlist_empty(&socket_cb->listener_s.queue) == 0){
				rlnode_ptr node = rlist_pop_front(&socket_cb->listener_s.queue);
				free(node);
			}
			PORT_MAP[socket_cb->port] = NULL;
			kernel_broadcast(&socket_cb->listener_s.req_available);
			break;
		case SOCKET_UNBOUND:
			break;
	}

	SCB_decref(socket_cb);

	return 0;
}

SOCKET_CB* get_SCB(Fid_t sock){

	FCB* fcb = get_fcb(sock);

	if(fcb == NULL || sock < 0 || sock > MAX_FILEID)
		return NULL;

	return fcb->streamobj;
}

void SCB_decref(SOCKET_CB* socket_cb){

	socket_cb->refcount--;

	if(socket_cb->refcount < 0)
		free(socket_cb);
}
