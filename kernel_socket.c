
#include "tinyos.h"
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"
#include "kernel_socket.h"


socket_cb* PORT_MAP[MAX_PORT+1];

static file_ops socket_file_ops ={
  .Open=NULL,
  .Read=socket_read,
  .Write=socket_write,
  .Close=socket_close
};

Fid_t sys_Socket(port_t port)
{
  if(port<NOPORT || port>MAX_PORT){
  	return -1;
  }

  Fid_t fid[1];//new file id 
  FCB* fcb[1]; //new file control block 
    
  if (FCB_reserve(1,fid,fcb)==0) {
    return -1;
  }

  socket_cb* scb=xmalloc(sizeof(socket_cb));
  scb->refcount=0;
  scb->fcb=fcb[0];
  scb->fid=fid[0];
  scb->type=SOCKET_UNBOUND;
  scb->port=port;

  fcb[0]->streamobj=scb;
  fcb[0]->streamfunc=&socket_file_ops;
  
  return (Fid_t) scb->fid;
}

int sys_Listen(Fid_t sock)
{ 
	FCB* fcb=get_fcb(sock);
	if(fcb==NULL){
		return -1;
	}
	socket_cb* socket_cb1=fcb->streamobj;
	
    if(socket_cb1==NULL || socket_cb1->type==SOCKET_LISTENER){ //an einai hdh arxikopoihmeno
    	return -1;
    }
    if(socket_cb1->port==NULL || PORT_MAP[socket_cb1->port]!=NULL){ //an den einai bound se port h an to port exei hdh listener
    	return -1;
    }
    socket_cb1->type=SOCKET_LISTENER;
	PORT_MAP[socket_cb1->port]=socket_cb1;
	listener_socket listener_s=socket_cb1->listener_s;

	rlnode_init(&listener_s.queue,NULL); //arxikopoihsh ouras
     
    return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{
	/*socket_cb* socket_cb1=(socket_cb*)lsock;
	listener_socket listener_s=socket_cb1->listener_s;
    
    socket_cb1->refcount++;

    while(is_rlist_empty(&listener_s.queue)){
	    kernel_wait(&listener_s.req_availiable,SCHED_PIPE);
    }

    if(socket_cb1==NULL || PORT_MAP[socket_cb1->port]==NULL ){
    	return -1;
    }
   
    rlnode* crnode =rlist_pop_front(&socket_cb1->listener_s.queue);
    connection_request* cr=crnode->cr;
    cr->admitted=1;

    socket_cb* socket_cb3=sys_Socket(socket_cb1->port);
    socket_cb3->type=SOCKET_PEER;
    socket_cb3->peer_s.peer=cr->peer; //pointer metaxu socket_cb
    
    kernel_broadcast(&cr->connect_cv);

    socket_cb1->refcount--;*/
    return 0;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	/*socket_cb* socket_cb2=(socket_cb*)sock;
	socket_cb* socket_cb1=PORT_MAP[port];

	connection_request* cr=xmalloc(sizeof(connection_request));
	cr->admitted=0;
	cr->peer=socket_cb2;
	cr->connect_cv=COND_INIT;

	rlnode_init(&cr->queue_node,cr);
    rlist_push_front(&socket_cb1->listener_s.queue,&cr->queue_node);
	kernel_broadcast(&socket_cb1->listener_s.req_availiable);
   
    while(cr->admitted!=1){
    kernel_timedwait(&cr->connect_cv,SCHED_PIPE,timeout);
    }
*/
   return 0;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}
int socket_write(void* this, const char *buf, unsigned int n){
  socket_cb* scb=(socket_cb*)this;
  assert(scb!=NULL);

  //scb->type=SOCKET_PEER;
  peer_socket peer_s=scb->peer_s;
  pipe_cb* p=peer_s.read_pipe;
  pipe_write(p,buf,n);

  return 0;
}

int socket_read(void* this, char *buf, unsigned int n){
  socket_cb* scb=(socket_cb*)this;
  assert(scb!=NULL);

  //scb->type=SOCKET_PEER;
  peer_socket peer_s=scb->peer_s;
  pipe_cb* p=peer_s.write_pipe;
  pipe_read(p,buf,n);

  return 0;
}

int socket_close(void* this){
	return -1;
}

