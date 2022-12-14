
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

/*

Allages pou exoun ginei

6.1 Threads

-util.h: typedef ptcb
-kernel_sched.h: ptcb kai edit thn tcb
-kernel_proc.h: edit thn pcb
-kernel_proc.c: sys_Exec(), sys_Exit(), start_thread(), initialize_PCB()
-kernel_threads.c: Ola

6.2 MLFQ

-kernel_sched.h: int priority sthn tcb
-kernel_sched.c: 
  
  define queueNum, maxYieldCalls kai int yieldCalls
  to rlnode SCHED egine rlnode SCHED[queueNum]
  yield(): switch(cause) kai priority boost
  initialize_scheduler(): edit gia array
  sched_queue_add()
  sched_queue_select()
*/

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{

  /*Init and return a new TCB*/
  TCB* tcb = spawn_thread(CURPROC, start_thread);
  
  /*Acquire a PTCB*/
  PTCB* ptcb = (PTCB*)xmalloc(sizeof(PTCB)); //Allocate space

  //Make connections with PCB and TCB
  tcb->ptcb = ptcb;
  ptcb->tcb = tcb;

  rlnode_init(&ptcb->ptcb_list_node, ptcb);
  rlist_push_back(&CURPROC->ptcb_list, &ptcb->ptcb_list_node);

  CURPROC->thread_count++;

  /*Init PTCB*/
  ptcb->task = task;
  ptcb->argl = argl;
  ptcb->args = args;

  ptcb->exitval = CURPROC->exitval;
  
  ptcb->exited = 0;
  ptcb->detached = 0;
  ptcb->exit_cv = COND_INIT;
  
  ptcb->refcount = 1;
  
  /*Wake up TCB*/
  wakeup(ptcb->tcb);

  return (Tid_t) ptcb;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) cur_thread()->ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
  /*Checks*/
  PTCB* ptcb = NULL;

  if(rlist_find(&CURPROC->ptcb_list, (PTCB*)tid, NULL) != NULL) //Check if tid exists in ptcb_list
    ptcb = (PTCB*)tid;
  if(ptcb == NULL)
    return -1;
  if(sys_ThreadSelf() == tid) //Check if trying to join itself
    return -1;
 
  /*Wait*/
  ptcb->refcount++;
  
  while(ptcb->exited != 1 && ptcb->detached != 1){
    kernel_wait(&ptcb->exit_cv, SCHED_USER);
  }

  ptcb->refcount--;

  if(ptcb->detached)  //Check if detached
    return -1;

  if(exitval!=NULL)
    *exitval=ptcb->exitval;

  
  if(ptcb->refcount == 1){
    rlist_remove(&ptcb->ptcb_list_node);
    free(ptcb);
  }

  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	PTCB* ptcb = NULL;

  if(rlist_find(&CURPROC->ptcb_list, (PTCB*)tid, NULL) != NULL) //Check if tid exists in ptcb_list
    ptcb = (PTCB*)tid;
  if(ptcb == NULL)
    return -1;
  if(ptcb->exited == 1) //Check if exited
    return -1;

  ptcb->detached = 1; //Detach thread
  kernel_broadcast(&ptcb->exit_cv);

  return 0;

}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  PTCB* ptcb = cur_thread()->ptcb;
  ptcb->exited = 1;
  ptcb->exitval=exitval;

  kernel_broadcast(&ptcb->exit_cv);


  PCB *curproc = CURPROC;  /* cache for efficiency */

  curproc->thread_count--;

  if(curproc->thread_count == 0){

    /* Reparent any children of the exiting process to the 
       initial task */
    
    if(get_pid(CURPROC)!=1){

        PCB* initpcb = get_pcb(1);
        
        while(!is_rlist_empty(& curproc->children_list)) {
          rlnode* child = rlist_pop_front(& curproc->children_list);
          child->pcb->parent = initpcb;
          rlist_push_front(& initpcb->children_list, child);
        }

        /* Add exited children to the initial task's exited list 
           and signal the initial task */
        if(!is_rlist_empty(& curproc->exited_list)) {
          rlist_append(& initpcb->exited_list, &curproc->exited_list);
          kernel_broadcast(& initpcb->child_exit);
        }

        /* Put me into my parent's exited list */
        
        rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
        kernel_broadcast(& curproc->parent->child_exit);
      
    }
    assert(is_rlist_empty(& curproc->children_list));
    assert(is_rlist_empty(& curproc->exited_list));

    /* 
     Do all the other cleanup we want here, close files etc. 
    */

    /* Release the args data */
    if(curproc->args) {
     free(curproc->args);
     curproc->args = NULL;
    }

    /* Clean up FIDT */
    for(int i=0;i<MAX_FILEID;i++) {
      if(curproc->FIDT[i] != NULL) {
        FCB_decref(curproc->FIDT[i]);
        curproc->FIDT[i] = NULL;
      }
    }

    /* Disconnect my main_thread */
    curproc->main_thread = NULL;

    /* Now, mark the process as exited. */
    curproc->pstate = ZOMBIE;
    
    //kernel_sleep(EXITED, SCHED_USER);

  }

  kernel_sleep(EXITED, SCHED_USER);

}

