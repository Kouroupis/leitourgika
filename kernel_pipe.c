#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

static file_ops reader ={
  .Open=NULL,
  .Read=pipe_read,
  .Write=useless,
  .Close=pipe_reader_close
};
static file_ops writer ={
  .Open=NULL,
  .Read=useless,
  .Write=pipe_write,
  .Close=pipe_writer_close
};

int useless(void* pipecb_t, const char *buf, unsigned int n){
  return -1;
}


int sys_Pipe(pipe_t* pipe)
{ 
  Fid_t fid[2];//new file id for read and write
  FCB* fcb[2]; //new file control block for read and write
  
  if (FCB_reserve(2,fid,fcb)==0)  //gia test 2 na mhn skaei
  {
    return -1;
  }

    pipe_cb* p=xmalloc(sizeof(pipe_cb));//reserve space for 

    pipe->read = fid[0];
    pipe->write = fid[1];
   
    p->empty=PIPE_BUFFER_SIZE;
    p->data=0;
    p->reader=0;
    p->writer=0;
    p->has_space=COND_INIT;
    p->has_data=COND_INIT;
    p->w_position=0;
    p->r_position=0;

    fcb[0]->streamobj=p;
    fcb[1]->streamobj=p;
    fcb[0]->streamfunc=&reader;
    fcb[1]->streamfunc=&writer;
    
    p->reader=fcb[0];
    p->writer=fcb[1];

return 0;

}

int pipe_write(void* this,const char *buf, unsigned int size){

  pipe_cb* p=(pipe_cb*)this;
  assert(p!=NULL);
  int count=0;

  if (p->writer==NULL || p->reader==NULL){ //an o reader i o writer einai closed, return
    return -1;
  }


  while(p->empty==0 && p->reader!= NULL){//an den uparxei xwros gia na grapsei perimenei mexri na uparksei
    kernel_wait(&p->has_space,SCHED_PIPE);
  }

  while ((p->w_position+1)%PIPE_BUFFER_SIZE==p->r_position && p->reader!=NULL){ //an paei o writer na grapsei sti thesi tou reader
      
          kernel_wait(&(p->has_space), SCHED_PIPE);//perimene mexri na eleutherwthei xwros
          //kernel_broadcast(&p->has_data);//ksipna ton reader gia na diavasei

   }

  while (count<PIPE_BUFFER_SIZE && count!=size && (p->w_position+1)%PIPE_BUFFER_SIZE!=p->r_position && p->reader!=NULL && p->reader!=NULL){ //oso uparxei xwros ston buffer grapse
        p->BUFFER[p->w_position]=buf[count]; //copy in pipe buffer                                                                           //kai oso o write pointer>apo read pointer
        p->w_position=(p->w_position+1)%PIPE_BUFFER_SIZE; //next write position for bounded buffer
        count++;
        p->data++;
        p->empty--;
      }

   kernel_broadcast(&p->has_data);
   return count;//returns number of bytes copied in pipe buffer


}

int pipe_read(void* this, char *buf, unsigned int size){

pipe_cb* p=(pipe_cb*)this;
assert(p!=NULL);
int count=0;


if (p->reader==NULL){
  return -1;
}

if(p->writer==NULL){
  if (p->data==0){
    return 0;
  }else {//periptwsi pou o writer exei kleisei kai o reader prepei na diavasei o,ti exei meinei
    while(p->r_position!=p->w_position && count<size && count<PIPE_BUFFER_SIZE){
            buf[count]=p->BUFFER[p->r_position];
            p->BUFFER[p->r_position]=0;        //read and delete element
            p->r_position=(p->r_position+1)%PIPE_BUFFER_SIZE; 
            count++;
            p->empty++;
            p->data--;

        }return count;//returns number of bytes copied in pipe buffer
    }
}


while(p->data==0 && p->writer!= NULL){//an o buffer einai adeios

  kernel_wait(&(p->has_data), SCHED_PIPE);

}

while (p->r_position==p->w_position && p->writer!=NULL ){ //an paei o reader na diavasei sti thesi pou grafei o writer
      kernel_wait(&(p->has_data), SCHED_PIPE);
          
  }


 while(count!=size && count<PIPE_BUFFER_SIZE && p->r_position!=p->w_position){
      buf[count]=p->BUFFER[p->r_position];
      p->BUFFER[p->r_position]=0;        //read and delete element
      p->r_position=(p->r_position+1)%PIPE_BUFFER_SIZE; 
      count++;
      p->data--;
      p->empty++;
      
   } 
 
 
  kernel_broadcast(&p->has_space);
  return count; //returns number of bytes copied in pipe buffer  

}

int pipe_writer_close(void* this){
  pipe_cb* p=(pipe_cb*) this;
  assert(p!=NULL);
  p->writer=NULL;  //close write end
   kernel_broadcast(&(p->has_data));

  if(p->reader==NULL){ //if read end closed free pipe
    p=NULL;
    free(p);
  }
  return 0;
}

int pipe_reader_close(void* this){
  pipe_cb* p=(pipe_cb*) this;
  assert(p!=NULL);
  p->reader=NULL;
  kernel_broadcast(&(p->has_space));

    if(p->writer==NULL){
    p=NULL;
    free(p);
   
  }
   return 0;
}