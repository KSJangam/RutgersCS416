// File:	rpthread.c

// List all group member's name:Kunal Jangam
// username of iLab:ksj48
// iLab Server:butter

#include "rpthread.h"
#define STACK_SIZE SIGSTKSZ
tcb* current;
ucontext_t schedctx,mainctx;
queue* table;
queue done;
int time_slice=5000;
int entries=0;
int init=1;//if first call of create, initialize everything
int quantums=0;
struct itimerval timer;
void ring(int signum){
  swapcontext(&(current->ctx),&schedctx);
}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf() {
  int level=0;
  if(current->status==1){//was just running a thread
    current->status=0;
    current->priority=current->priority+1;
    //put thread back into table
    node *t=(node*)malloc(sizeof(node));
    t->next=NULL;
    t->block=*current;
    int index=0;
    int index2;
    while(table[index].priority<current->priority)
      index++;
    //want to put current into table at index
    if(current->priority==table[index].priority){//if priority is correct
      if(table[index].size>0){
	table[index].tail->next=t;
	table[index].tail=table[index].tail->next;
        table[index].size=table[index].size+1;
      }
      else{
	table[index].head=t;
	table[index].tail=t;
	table[index].size=1;
      }
    }
    else{//if priority is not correct, make new queue
      for(index2=149;index2>index; index2--){
        table[index2]=table[index2-1];
      }
      table[index].head=t;
      table[index].tail=t;
      table[index].size=1;
      table[index].priority=current->priority;
    }
    entries++;
  }
  while(level<150 && table[level].size==0)//find level with a thread to run
    level++;
  if(level<150){
    current=&(table[level].head->block);
    if(table[level].size>1){//remove last node from queue
      table[level].head=table[level].head->next;
    }
    else{
      table[level].head=NULL;
      table[level].tail=NULL;
    }
    entries--;
    table[level].size=table[level].size-1; 
    timer.it_value.tv_usec=time_slice;//reset timer
    timer.it_value.tv_sec=0;
    current->status=1;
    setitimer(ITIMER_PROF, &timer, NULL);
    setcontext(&(current->ctx));
  }
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
  
  int level=0;
  //every 10 time quantums bump up the low priority threads
     
 if((quantums+1)%10==0){
   if(table[1].size>0){
    if(table[0].size>0)//append second level to first
      table[0].tail->next=table[1].head;
    else
      table[0].head=table[1].head;
    table[0].tail=table[1].tail;
    table[0].size=table[0].size+table[1].size;
   }
   if(table[2].size>0){
    table[1].head=table[2].head;
    table[1].tail=table[2].tail;
    table[1].size=table[2].size;
   }
   if(table[3].size>0){
    table[2].head=table[3].head;
    table[2].tail=table[3].tail;
    table[2].size=table[3].size;
    table[2].priority=2;
    table[3].size=0;
    table[3].head=NULL;
    table[3].tail=NULL;
   }    
    //    printf("a %d\n",table[0].tail->block.id);
  }
  
  if(current->status==1){//was just running a thread
    current->status=0;
    entries++;
    //put thread back into table
    node *t=(node*)malloc(sizeof(node));
    t->next=NULL;
    if(current->priority<3)
      current->priority=current->priority+1;
    t->block=*current;
    if(table[t->block.priority].size>0){
      table[t->block.priority].tail->next=t;
      table[t->block.priority].tail=table[t->block.priority].tail->next;
      table[t->block.priority].size=table[t->block.priority].size+1;
    }
    else{
      table[t->block.priority].head=t;
      table[t->block.priority].tail=t;
      table[t->block.priority].size=1;
    }
  }
  while(level<4 && table[level].size==0)//find level with a thread to run
    level++;
  if(level<4){
    current=&(table[level].head->block);    
    if(table[level].size>1){//remove last node from queue
      table[level].head=table[level].head->next;
    }
    else{
      table[level].head=NULL;
      table[level].tail=NULL;
    }
    entries--;
    table[level].size=table[level].size-1; 
    timer.it_value.tv_usec=time_slice;//reset timer
    timer.it_value.tv_sec=0;
    current->status=1;
    quantums++;
    setitimer(ITIMER_PROF, &timer, NULL);
    setcontext(&(current->ctx));
  }
}


/* scheduler */
static void schedule() {
// schedule policy
  if(current->status==1&&entries==0)
    swapcontext(&schedctx,&(current->ctx));
#ifndef MLFQ
	// Choose STCF
  sched_stcf();
#else 
	// Choose MLFQ
  sched_mlfq();
#endif

}

void setup(){
  done.head=NULL;//done queue setup
  done.tail=NULL;
  done.size=0;
  current=(tcb*)malloc(sizeof(tcb));
  getcontext(&mainctx);
  getcontext(&schedctx);
  void* stack=malloc(STACK_SIZE);//schedule context
  schedctx.uc_link=NULL;
  schedctx.uc_stack.ss_sp=stack;
  schedctx.uc_stack.ss_size=STACK_SIZE;
  schedctx.uc_stack.ss_flags=0;
  makecontext(&schedctx,(void*)&schedule,0);
  struct sigaction sa;//timer setup
  memset(&sa,0,sizeof(sa));
  sa.sa_handler=&ring;
  sigaction(SIGPROF, &sa, NULL);
  timer.it_interval.tv_usec=0;
  timer.it_interval.tv_sec=0;
  timer.it_value.tv_usec=time_slice;
  timer.it_value.tv_sec=0;
  node* t=(node*)malloc(sizeof(node));//adding main into scheduler table
  t->next=NULL;
  t->block.ctx=mainctx;
  t->block.priority=0;
  t->block.status=0;
  t->block.id=0;
#ifndef MLFQ
	// Choose STCF
  table=(queue*)malloc(sizeof(queue)*150);
  int counter;
  for(counter=0; counter<150; counter++){
    queue a;
    a.size=0;
    a.head=NULL;
    a.tail=NULL;
    a.priority=counter;
    table[counter]=a;//initialize table to have 150 threads max
  }
  table[0].head=t;
  table[0].tail=t;
  table[0].size=1;
#else //choose mlfq
  table=(queue*)malloc(sizeof(queue)*4);
  int counter;
  for(counter=0; counter<4; counter++){
    queue a;
    a.size=0;
    a.head=NULL;
    a.tail=NULL;
    a.priority=counter;
    table[counter]=a;
  }//initialize table with 4 priority levels
  table[0].head=t;
  table[0].tail=t;
  table[0].size=1;
#endif

}
/* create a new thread */
int rpthread_create(rpthread_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {
  if(init){//if first call of create, set up everything
    setup();
  }
  ucontext_t ctx;
  getcontext(&ctx);
  void* stack=malloc(STACK_SIZE);
  ctx.uc_link=NULL;
  ctx.uc_stack.ss_sp=stack;
  ctx.uc_stack.ss_size=STACK_SIZE;
  ctx.uc_stack.ss_flags=0;
  makecontext(&ctx,(void (*)())function,1,arg);
  tcb block;
  if(init){//setting parent
    init=0;
    block.parent=(tcb*)malloc(sizeof(tcb));
    block.parent=&(table[0].head->block);
  }
  else{
    block.parent=(tcb*)malloc(sizeof(tcb));
    block.parent=current;
  } 
  block.ctx=ctx;
  block.id=*thread;
  block.status=0;
  block.args=arg;
  block.priority=0;
  node* t=(node*)malloc(sizeof(node));
  t->next=NULL;
  t->block=block;
  current->status=0;
  //add thread to table and switch to scheduler
  entries++;
  if(table[0].size==0){
    table[0].head=t;
    table[0].tail=t;
    table[0].size=1;
    swapcontext(&(table[0].tail->block.parent->ctx),&schedctx);
  }
  else{
    table[0].tail->next=t;
    table[0].tail=table[0].tail->next;
    table[0].size=table[0].size+1;
    swapcontext(&(table[0].tail->block.parent->ctx),&schedctx);
  }//save context of whatever thread called create
  return 0;
}

/* give CPU possession to other user-level threads voluntarily */
int rpthread_yield() {
  timer.it_value.tv_usec=0;
  timer.it_value.tv_sec=0;//deactivate timer
  setitimer(ITIMER_PROF, &timer, NULL);
  current->status=0;
  int index=0;
  int index2;
  node* t=(node*)malloc(sizeof(node));
  t->next=NULL;
  t->block=*current;
  while(table[index].priority<current->priority)
    index++;
  //want to put current into table at index
  if(current->priority==table[index].priority){//if priority is correct
    if(table[index].size>0){
      table[index].tail->next=t;
      table[index].tail=table[index].tail->next;
      table[index].size=table[index].size+1;
    }
    else{
      table[index].head=t;
      table[index].tail=t;
      table[index].size=1;
    }
  }
  else{//if priority is not correct
    for(index2=34;index2>index; index2--){
      table[index2]=table[index2-1];
    }
    table[index].head=t;
    table[index].tail=t;
    table[index].size=1;
    table[index].priority=current->priority;
  }
  swapcontext(&(table[index].tail->block.ctx),&schedctx);
	return 0;
}

/* terminate a thread */
void rpthread_exit(void *value_ptr) {
  timer.it_value.tv_usec=0;
  timer.it_value.tv_sec=0;
  setitimer(ITIMER_PROF, &timer, NULL);
  free(current->stack);
  current->retval=value_ptr;
  current->status=3;
  node* t=(node*)malloc(sizeof(node));
  t->block=*current;
  t->next=NULL;
  if(done.size==0){//put into done queue
    done.head=t;
    done.tail=t;
    done.size=1;   
  }
  else{ 
    t->next=done.head;
    done.head=t;
    done.size=done.size+1;
  }
  current=current->parent;
  setcontext(&(current->ctx));
}

/* Wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr) {
  int looking=1;
  current->status=2;
  while(looking){
    node* ptr=done.head;
    node* prev=NULL;
    while(looking && ptr!=NULL){
      if(ptr->block.id==thread){
	value_ptr=&(ptr->block.retval);
        looking=0;
      //check if thread has terminated
      }
      else{
	prev=ptr;
	ptr=ptr->next;
      }
    }
    if(looking){
       swapcontext(&(current->ctx),&schedctx);
      //if thread has not terminated go to scheduler
    }
   
     //child exited, remove thread from done queue
    else{
      if(prev==NULL)//thread was first in done queue
        done.head=done.head->next;
      else if(ptr->next==NULL)//thread was last in done queue
        prev->next=NULL;
      else{
        prev->next=ptr->next;
      }
      done.size=done.size-1;
    }
  } 
	return 0;
}

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//Initialize data structures for this mutex
  mutex->locked=0;
  mutex->blocked.size=0;
  mutex->blocked.head=NULL;
  mutex->blocked.tail=NULL;
  return 0;
}

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
        // use the built-in test-and-set atomic function to test the mutex
        // When the mutex is acquired successfully, enter the critical section
        // If acquiring mutex fails, push current thread into block list and 
        // context switch to the scheduler thread
  if(mutex->locked==1){
  while(mutex->locked==1){//if locked, add to mutex blocked queue and swap out
    node* t=(node *)malloc(sizeof(node));
    current->status=2;
    t->next=NULL;
    t->block=*current;
    if(mutex->blocked.size==0){
      mutex->blocked.head=t;
      mutex->blocked.tail=t;
      mutex->blocked.size=1;
    }
    else{
      mutex->blocked.tail->next=t;
      mutex->blocked.tail=mutex->blocked.tail->next;
      mutex->blocked.size=mutex->blocked.size+1;
    }
    swapcontext(&(current->ctx),&schedctx);
  }
  }
  else//if unlocked, lock mutex
    mutex->locked=1;
        return 0;
}

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.
  mutex->locked=0;
  node* bptr=mutex->blocked.head;
  entries=entries+mutex->blocked.size;
  while(bptr!=NULL){
    int index=0;
    int index2;
    node* t=(node*)malloc(sizeof(node));
    t->next=NULL;
    t->block=bptr->block;
    t->block.status=0;
    while(table[index].priority<t->block.priority)
      index++;
    //want to put current into table at index
    if(t->block.priority==table[index].priority){//if priority is correct
      if(table[index].size>0){
        table[index].tail->next=t;
        table[index].tail=table[index].tail->next;
        table[index].size=table[index].size+1;
      }
      else{
	table[index].head=t;
	table[index].tail=t;
	table[index].size=1;
      }
    }
    else{//if priority is not correct
      for(index2=34;index2>index; index2--){
	table[index2]=table[index2-1];
      }
      table[index].head=t;
      table[index].tail=t;
      table[index].size=1;
      table[index].priority=t->block.priority;
    }
    bptr=bptr->next;
  }
  mutex->blocked.head=NULL;
  mutex->blocked.tail=NULL;
  mutex->blocked.size=0;
	return 0;
}

/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) { 
  node* bptr=mutex->blocked.head;
  while(bptr!=NULL){
    int index=0;
    int index2;
    node* t=(node*)malloc(sizeof(node));
    t->next=NULL;
    t->block=bptr->block;
    t->block.status=0;
    while(table[index].priority<t->block.priority)
      index++;
    //want to put current into table at index
    if(t->block.priority==table[index].priority){//if priority is correct
      if(table[index].size>0){
        table[index].tail->next=t;
        table[index].tail=table[index].tail->next;
        table[index].size=table[index].size+1;
      }
      else{
	table[index].head=t;
	table[index].tail=t;
	table[index].size=1;
      }
    }
    else{//if priority is not correct
      for(index2=34;index2>index; index2--){
	table[index2]=table[index2-1];
      }
      table[index].head=t;
      table[index].tail=t;
      table[index].size=1;
      table[index].priority=t->block.priority;
    }
    bptr=bptr->next;
  }

    mutex->blocked.head=NULL;
    mutex->blocked.tail=NULL;
    mutex->blocked.size=0;
	return 0;
}

