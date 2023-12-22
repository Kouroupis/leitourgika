#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

static file_ops reader_file_ops ={
	.Open = NULL,
	.Read = pipe_read,
	.Write = NULL,
	.Close = pipe_reader_close
};

static file_ops writer_file_ops ={
	.Open = NULL,
	.Read = NULL,
	.Write = pipe_write,
	.Close = pipe_writer_close
};


int sys_Pipe(pipe_t* pipe)
{
	Fid_t fid[2];
	FCB* fcb[2];

	if(FCB_reserve(2, fid, fcb) == 0)
		return -1;

	PIPE_CB* pipe_cb = xmalloc(sizeof(PIPE_CB));

	pipe->read = fid[0];
	pipe->write = fid[1];

	pipe_cb->space = PIPE_BUFFER_SIZE;
	pipe_cb->reader = 0;
	pipe_cb->writer = 0;
	pipe_cb->has_space = COND_INIT;
	pipe_cb->has_data = COND_INIT;
	pipe_cb->w_position = 0;
	pipe_cb->r_position = 0;

	fcb[0]->streamobj = pipe_cb;
	fcb[1]->streamobj = pipe_cb;
	fcb[0]->streamfunc = &reader_file_ops;
	fcb[1]->streamfunc = &writer_file_ops;

	pipe_cb->reader = fcb[0];
	pipe_cb->writer = fcb[1];

	return 0;
}

int pipe_write(void* pipecb_t, const char *buf, unsigned int size){
	
	PIPE_CB* pipe_cb = (PIPE_CB*) pipecb_t;

	assert(pipe_cb != NULL);

	int bytes_written = 0;

	while(pipe_cb->space == 0 && pipe_cb->reader != NULL){
		kernel_wait(&pipe_cb->has_space, SCHED_PIPE);
	}

	if(pipe_cb->reader == NULL)
		return -1;

	while(bytes_written < pipe_cb->space){

		if(bytes_written >= size)
			break;

		pipe_cb->BUFFER[pipe_cb->w_position] = buf[bytes_written];
		pipe_cb->w_position = (pipe_cb->w_position + 1) %PIPE_BUFFER_SIZE;

		bytes_written++;
	}

	pipe_cb->space -= bytes_written;

	kernel_broadcast(&pipe_cb->has_data);

	return bytes_written;
}

int pipe_read(void* pipecb_t, char *buf, unsigned int size){
	
	PIPE_CB* pipe_cb = (PIPE_CB*) pipecb_t;

	assert(pipe_cb != NULL);

	int bytes_read = 0;

	while(pipe_cb->space == PIPE_BUFFER_SIZE && pipe_cb->writer != NULL){
		kernel_wait(&(pipe_cb->has_data), SCHED_PIPE);
	}

	if(pipe_cb->space == PIPE_BUFFER_SIZE)
		return 0;

	while(bytes_read < PIPE_BUFFER_SIZE - pipe_cb->space){
		if(bytes_read >= size)
			break;

		buf[bytes_read] = pipe_cb->BUFFER[pipe_cb->r_position];
		pipe_cb->r_position = (pipe_cb->r_position + 1) % PIPE_BUFFER_SIZE;

		bytes_read ++;
	}

	pipe_cb->space += bytes_read;

	kernel_broadcast(&pipe_cb->has_space);

	return bytes_read;
}

int pipe_writer_close(void* _pipecb){
	
	PIPE_CB* pipe_cb = (PIPE_CB*) _pipecb;

	assert(pipe_cb != NULL);

	pipe_cb->writer = NULL;
	//kernel_broadcast(&pipe_cb->has_data);

	if(pipe_cb->reader == NULL){
		pipe_cb = NULL;
		free(pipe_cb);
	}

	return 0;
}

int pipe_reader_close(void* _pipecb){
	
	PIPE_CB* pipe_cb = (PIPE_CB*) _pipecb;

	assert(pipe_cb != NULL);

	pipe_cb->reader = NULL;
	//kernel_broadcast(&pipe_cb->has_space);

	if(pipe_cb->writer == NULL){
		pipe_cb = NULL;
		free(pipe_cb);
	}

	return 0;
}