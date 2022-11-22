
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
	//Initialize and return a new TCB(spawn)

  TCB* tcb = spawn_thread(CURPROC, start_thread);
  
  //Acquire PTCB

  PTCB* ptcb = xmalloc(sizeof(PTCB));

  rlnode_init(&ptcb->ptcb_list_node, NULL);
  rlist_push_front(&ptcb->ptcb_list_node, &CURPROC->ptcb_list);

  CURPROC->thread_count++;

  tcb->ptcb = ptcb;

  //Initialize PTCB

  ptcb->tcb = tcb;
  ptcb->task = task;
  ptcb->argl = argl;
  ptcb->args = args;

  ptcb->exited = 0;
  ptcb->detached = 0;
  ptcb->exit_cv = COND_INIT;

  ptcb->refcount = 0;

  //Wake up TCB

  wakeup(tcb);

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
  PTCB* ptcb = (PTCB*) tid;

  if(rlist_find(&CURPROC->ptcb_list, &tid, NULL))
    return -1;
  if(tid == ThreadSelf())
    return -1;
  if(ptcb->exited == 1)
    return -1;
  if(ptcb->detached == 1)
    return -1;

  ptcb->refcount++;
  
  int exit = kernel_wait(&ptcb->exit_cv, SCHED_USER);
  exitval = &exit;

  ptcb->refcount--;

  Cond_Broadcast(&ptcb->exit_cv);
  
  return 0;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	PTCB* ptcb = (PTCB*) tid;

  if(tid == 0)
    return -1;
  if(ptcb->exited == 1)
    return -1;

  Cond_Broadcast(&ptcb->exit_cv);

  ptcb->detached = 1;

  return 0;

}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  PCB *curproc = CURPROC;  /* cache for efficiency */

  /* First, store the exit status */
  curproc->exitval = exitval;

  if(curproc->thread_count == 0){

    /* Reparent any children of the exiting process to the 
       initial task */
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
    curproc->exitval = exitval;

    //kernel_sleep(EXITED, SCHED_USER);

  }

  kernel_sleep(EXITED, SCHED_USER);

}

