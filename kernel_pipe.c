#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"


static file_ops reader_file_ops ={
  .Open = NULL,
  .Read = pipe_read,
  .Write = not_used,
  .Close = pipe_reader_close
};

static file_ops writer_file_ops ={
  .Open = NULL,
  .Read = not_used,
  .Write = pipe_write,
  .Close = pipe_writer_close
};

int not_used(void* pipecb_t, const char *buf, unsigned int n){
  return -1;
}


int sys_Pipe(pipe_t* pipe)
{ 
  
  //Reserve
  Fid_t fid[2];
  FCB* fcb[2]; 
  
  if (FCB_reserve(2,fid,fcb) == 0)
  {
    return -1;
  }

  //Allocate space
  pipe_cb* pipeCB = xmalloc(sizeof(pipe_cb)); 

  pipe->read = fid[0];
  pipe->write = fid[1];
  
  //Init pipe control block
  pipeCB->space = PIPE_BUFFER_SIZE;
  pipeCB->reader = 0;
  pipeCB->writer = 0;
  pipeCB->has_space = COND_INIT;
  pipeCB->has_data = COND_INIT;
  pipeCB->w_position = 0;
  pipeCB->r_position = 0;

  fcb[0]->streamobj = pipeCB;
  fcb[1]->streamobj = pipeCB;
  fcb[0]->streamfunc = &reader_file_ops;
  fcb[1]->streamfunc = &writer_file_ops;
    
  pipeCB->reader = fcb[0];
  pipeCB->writer = fcb[1];

  return 0;

}

int pipe_write(void* this,const char *buf, unsigned int size){

  pipe_cb* pipeCB = (pipe_cb*)this;
  
  assert(pipeCB != NULL);
  
  int count = 0;

  //Wait while buffer is full and reader is open
  while(pipeCB->space == 0 && pipeCB->reader != NULL){

    kernel_wait(&pipeCB->has_space,SCHED_PIPE);
  
  }

  if(pipeCB->reader == NULL)
    return -1;

  while(count < pipeCB->space){ //oso uparxei xwros ston buffer grapse
       
        if(count >= size)
          break;

        pipeCB->BUFFER[pipeCB->w_position] = buf[count]; //copy in pipe buffer
        pipeCB->w_position = (pipeCB->w_position + 1)%PIPE_BUFFER_SIZE; //next write position for bounded buffer
        
        count++;
        
  }

  pipeCB->space -= count;

  kernel_broadcast(&pipeCB->has_data);

  return count;//returns number of bytes copied in pipe buffer

}

int pipe_read(void* this, char *buf, unsigned int size){

  pipe_cb* pipeCB = (pipe_cb*)this;
  
  assert(pipeCB != NULL);
  
  int count = 0;

  while(pipeCB->space == PIPE_BUFFER_SIZE && pipeCB->writer != NULL){//an o buffer einai adeios

    kernel_wait(&(pipeCB->has_data), SCHED_PIPE);

  }

  if(pipeCB->space == PIPE_BUFFER_SIZE)
    return 0;

  while(count < PIPE_BUFFER_SIZE - pipeCB->space){

    if(count >= size)
      break;

    buf[count] = pipeCB->BUFFER[pipeCB->r_position];
    pipeCB->r_position = (pipeCB->r_position + 1)%PIPE_BUFFER_SIZE;
    
    count++;
  }
   
  pipeCB->space += count;
  
  kernel_broadcast(&pipeCB->has_space);

  return count; //returns number of bytes copied in pipe buffer  

}

int pipe_writer_close(void* this){
  
  pipe_cb* pipeCB = (pipe_cb*) this;
  
  assert(pipeCB != NULL);

  pipeCB->writer = NULL;  //close write end
  
  kernel_broadcast(&(pipeCB->has_data));

  if(pipeCB->reader == NULL){ //if read end closed free pipe
    pipeCB = NULL;
    free(pipeCB);
  }
  
  return 0;

}

int pipe_reader_close(void* this){
  
  pipe_cb* pipeCB = (pipe_cb*) this;
  assert(pipeCB != NULL);

  pipeCB->reader = NULL;
  
  kernel_broadcast(&(pipeCB->has_space));

  if(pipeCB->writer == NULL){
    pipeCB = NULL;
    free(pipeCB); 
  }

  return 0;
}