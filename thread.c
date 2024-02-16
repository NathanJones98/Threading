#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include <stdbool.h>
#include <unistd.h>

//Global varuiables
//Define Thread States
const int ZERO_STATE = -1;
const int READY = 0;
const int RUNNING = 1;
const int EXIT = 2;

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Lab 3 ... */
};

/* This is the thread control block */
struct thread {
	
	//Thread ID
	Tid threadId;

	//-1 for none,  0 for Ready, 1 for Running, 2 for Exited
	int state;

	//State of CPU
	ucontext_t cpuState;

	/*The stack pointer*/
	void *sp;
	
	//false is not yielding, true states that it is
	bool yieldState;

	//If the thread should exit
	bool exit;

	//for thread yield
	Tid returnTid;
	
};

//Queues
struct readyQ{

	struct thread * curThread;
	struct readyQ * next;
	struct readyQ * prev;
};

struct readyQ * rHead = NULL;
struct readyQ * rTail = NULL;

//Not sure if i need this
struct deleteQ{
	struct thread * curThread;
	struct deleteQ * next;
	struct deleteQ * prev;
};

struct deleteQ * dHead = NULL;


//Declerations
Tid findEmptyThread();

//The main array, that holds all the threads
struct thread *threads[THREAD_MAX_THREADS];
int a; //for testing
Tid currentlyRunningThread;

//Fnc defintions
void thread_stub(void (*thread_main)(void *), void *arg);
void printReadyQ();
void addToReadyQ(Tid thread);
void removeFromReadyQ(Tid thread);
void thread_yield_exit(Tid tid);

//Double linked list defintion
struct readyQ * findThread(Tid thread);

//Print the ready Q linked list - debugging
void printReadyQ(){

	struct readyQ* node = rHead;
	
	printf("\nPrinting Ready Queue\n");

	while(node != NULL){

		printf("\nThread ID: %d\nThread Yield State: %d\na", node->curThread->threadId, node->curThread->yieldState);
				
		if(node->next != NULL){
			node = node->next;
		} else { 
			
			if(rHead != NULL && rTail != NULL)
			printf("\nHead: %d Tail: %d\nDONE\n", rHead->curThread->threadId, rTail->curThread->threadId);
			break;
		}	
	}
}


/////////////////////////////////////////////////////////////////////
void
thread_init(void)
{	
	//Initialize the kernal thread, and start it
	int i = 0;
	//currentlyRunningThread = (struct thread*)malloc(sizeof(struct thread));
	threads[0] = (struct thread*)malloc(sizeof(struct thread));
	threads[0]->threadId = 0;
	threads[0]->state = RUNNING;
	threads[0]->sp = NULL;
	threads[0]->yieldState = true;

	//memset(&thread[0], 0, sizeof(struct thread));
	getcontext(&threads[0]->cpuState);

	//Initialize the array to recieve new threads 
	for (i=1; i<THREAD_MAX_THREADS; i++){

		//threads[i]= (struct thread*)malloc(sizeof(struct thread));
		//threads[i]->threadId = -1;
		//threads[i]->state = -1;
		//threads[i]->sp = NULL;
		//threads[i]->yieldState = true;
		threads[i] = NULL;
	}
}

//////////////////////////////////////////////////////////////////////
Tid
thread_id()
{
	//Return the currently running thread's id
	return currentlyRunningThread; 
}

//////////////////////////////////////////////////////////////////////

//Search the array of threads to find an open thread, and return the index
Tid findEmptyThread(){
	
	Tid firstEmpty = -1;
	int i;

	for(i=1; i < THREAD_MAX_THREADS; i++){
		if (threads[i] == NULL){

			firstEmpty = i;
			break;
		}else{
			firstEmpty = -1;
		}
	}

	return firstEmpty;
}

//Given Function
void thread_stub(void (*thread_main)(void *), void *arg){
		
		struct readyQ * node;

		//Tid ret;
		if(threads[currentlyRunningThread]->exit == true)
			thread_exit();

		//turn interrupts on
		interrupts_on();
		
		//Clear ReadyQ
		node = findThread(currentlyRunningThread);

		if(node != NULL)
			removeFromReadyQ(currentlyRunningThread);
	
		//make sure all threads are empty
		
		thread_main(arg);
		thread_exit();
}

//create a thread
Tid
thread_create(void (*fn) (void *), void *parg)
{	
	//pause();
	
	interrupts_off();

	//Variable declerations
	Tid tIndex;

	//Thread ptr
	struct thread* temp = (struct thread*)malloc(sizeof(struct thread));
	
	temp->yieldState = true;

	//See if malloc failed
	if(temp == NULL){
		interrupts_on();
		return THREAD_NOMEMORY;
	}
	
	//get first empty thread -1: no more room for threads
	tIndex = findEmptyThread();

	if(tIndex == -1){
		interrupts_on();
		return THREAD_NOMORE;
	}
	
	//get all registers, and store the information in each thread
	getcontext(&temp->cpuState);
	
	temp->cpuState.uc_mcontext.gregs[REG_RIP] = (long long int) &thread_stub;	
    temp->cpuState.uc_mcontext.gregs[REG_RDI] = (long long int) fn;
	temp->cpuState.uc_mcontext.gregs[REG_RSI] = (long long int) parg;	
	
	void *stackptr  = malloc(THREAD_MIN_STACK);

	//Check if there is available memory
	if(stackptr == NULL){
		interrupts_on();
		//No more memort for thread, therefore free it
		free(temp);
		return THREAD_NOMEMORY;
	}
	
	//store the stack registers
	temp->cpuState.uc_stack.ss_sp = stackptr;
	temp->cpuState.uc_mcontext.gregs[REG_RSP] = (long long int) stackptr + THREAD_MIN_STACK - 8;
	temp->cpuState.uc_stack.ss_size = THREAD_MIN_STACK - 8;
	
	temp->threadId = tIndex;
	temp->sp = stackptr;

	threads[tIndex] = temp;
	threads[tIndex]->state = READY;
	threads[tIndex]->exit = false;
	addToReadyQ(tIndex);
	
	interrupts_on();
	//printf("\n tIndex: %d \n", tIndex);
	return tIndex;
}

//////////////////////////////////////////////////////////////////////

//return place in queue given Tid, else return NULL
struct readyQ * findThread(Tid thread){

	struct readyQ * node = NULL;
	node = rHead;
	
	//cycle through the ready q list 
	while (node != NULL){
		if(node->curThread->threadId == thread){
			return node;	
		}
	node = node->next;
	}

	return NULL;
}

void addToReadyQ(Tid thread){

		struct readyQ* queueNode;

		//Create a node to store the currently running thread
		//in the readyQ
		queueNode = malloc(sizeof(struct readyQ));
		queueNode->curThread = threads[thread];
		queueNode->next = NULL;
		queueNode->prev = NULL;
		
		if(rTail != NULL){

			rTail->next = queueNode;
			queueNode->prev = rTail;
			rTail = queueNode;
		}else{
			rHead = queueNode;
			rTail = queueNode;
		}
}

void removeFromReadyQ(Tid thread){
		
		struct readyQ* node = findThread(thread);

		if(node == NULL){

			return;
		}

		//If the node has a node behind it
		if(node->prev != NULL){

			//if the node has a node after
			if(node->next != NULL){
				node->prev->next = node->next;
				node->next->prev = node->prev;
				
			//if the node does not have a node after	
			} else {

				node->prev->next = NULL;
				rTail = node->prev;
			}
		//Head Case
		}else{
			//head is not only item in readyQ
			if(node->next != NULL){
				rHead = node->next;
				node->next->prev = NULL;

			//node to be run is only thing in readyQ
			} else {
				
				rHead = NULL;
				rTail = NULL;
			}	
		
				
		}
		free(node);
		node = NULL;
}

Tid
thread_yield(Tid want_tid)
{
	interrupts_off();
		
	//Find the node tied to the thread id given, and the node before
	struct thread * temp = threads[currentlyRunningThread];

	Tid returnTid;

	//If the input is equal to THREAD_ANY
	if(want_tid == THREAD_ANY){
		
		//The readyQ is not empty
		if(rHead != NULL){

			//Get the TID from the top item in the list
			want_tid = rHead->curThread->threadId;
			
		//The readyQ is empty 		
		}else {
			interrupts_on();
			return THREAD_NONE;
		}			
	}
	

	//If the thread is trying to switch to itself
	if (want_tid == THREAD_SELF || want_tid == currentlyRunningThread){
		interrupts_on();
		return currentlyRunningThread;
	}

	//If the thread given is outside the range of threads
	if(want_tid >= THREAD_MAX_THREADS || want_tid < 0){
		interrupts_on();
			return THREAD_INVALID;
	}

	//Check if the thread is marked for deletion
	if(threads[want_tid] == NULL){
		interrupts_on();
		return THREAD_INVALID;
	}
	
	//if the readyQ is empty
	if(rHead == NULL){
		interrupts_on();
		return THREAD_NONE;
	}
	
	//printf("\n ONE: WantTID: %d, currentlyRunningThread %d, Check: %d\n", want_tid, currentlyRunningThread, threads[currentlyRunningThread]->threadId);
	
	
	//Store the cpu state of the currently running thread
	getcontext(&temp->cpuState);

	//Check to see if the currently running thread can yield
	if(threads[currentlyRunningThread]->yieldState == true){
		
			threads[currentlyRunningThread]->yieldState = false;
			threads[currentlyRunningThread]->state = READY;
			addToReadyQ(currentlyRunningThread);
			currentlyRunningThread = want_tid;
			removeFromReadyQ(want_tid);
			threads[want_tid]->state = RUNNING;
			returnTid = want_tid;
			
			//printf("\nSWITCHING THREAD\n\ncurrentlyRunningThreadID:%d\n returnTid: %d\n", currentlyRunningThread, returnTid);
			
			setcontext(&threads[currentlyRunningThread]->cpuState);
	}

	//Reset the yield state
	if(threads[currentlyRunningThread]->exit == true)
		thread_exit();

	threads[currentlyRunningThread]->yieldState = true;

	//printf("\nRETURNING THREAD\n\ncurrentlyRunningThreadID:%d\n", currentlyRunningThread);
	//printf("Return22: %d\n", returnTid);

	//pause();
	interrupts_on();
	return returnTid;

}

//////////////////////////////////////////////////////////////////////
void
thread_exit()
{	
	interrupts_off();
	
	if(currentlyRunningThread == 0){
		interrupts_on();
		return;
	}

	//See if we can run another thread
	if(rHead != NULL){
		
		//Change the currently running thread
		threads[currentlyRunningThread]->exit = true;
		thread_yield_exit(THREAD_ANY);
		
	//No other thread, close program
	} else {
		
		threads[currentlyRunningThread]->exit = true;
		thread_yield_exit(0);
		
	}
}

//same as thread yield, except with no queueing
void thread_yield_exit(Tid tid){

	interrupts_off();

		if(tid == THREAD_ANY){
			free(threads[currentlyRunningThread]);
			threads[currentlyRunningThread] = NULL;
			currentlyRunningThread = rHead->curThread->threadId;
			removeFromReadyQ(currentlyRunningThread);
			threads[currentlyRunningThread]->state = RUNNING;
			setcontext(&threads[currentlyRunningThread]->cpuState);
		} else {
			free(threads[currentlyRunningThread]);
			threads[currentlyRunningThread] = NULL;
			currentlyRunningThread = 0;
			removeFromReadyQ(0);
			threads[currentlyRunningThread]->state= RUNNING;
			setcontext(&threads[currentlyRunningThread]->cpuState);
		}
	}


///////////////////////////////////////////////////////////////////////
Tid
thread_kill(Tid tid)
{	
	
	//Check if the thread is valid
	interrupts_off();
	if(tid < 0 || tid > THREAD_MAX_THREADS-1){
		interrupts_on();
		return THREAD_INVALID;
	}	

	if(tid == 0 || tid == currentlyRunningThread || threads[tid] == NULL){
		interrupts_on();
		return THREAD_INVALID;
	}
	
	//remove the TID from the readyQ and then clear the thread table of said thread
	if(threads[tid] != NULL){

		threads[tid]->exit = true;
		removeFromReadyQ(tid);
		free(threads[tid]);
		threads[tid] = NULL;

		interrupts_on();
		return tid;

	} else {

		interrupts_on();
		return THREAD_INVALID;
	}

	interrupts_on();
	return threads[tid]->threadId;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue *
wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	//TBD();

	return wq;
}

void
wait_queue_destroy(struct wait_queue *wq)
{
	
}

Tid
thread_sleep(struct wait_queue *queue)
{
	//TBD();
	return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int
thread_wakeup(struct wait_queue *queue, int all)
{
	//TBD();
	return 0;
}

/* suspen oQd current thread until Thread tid exits */
Tid
thread_wait(Tid tid)
{
	//TBD();
	return 0;
}

struct lock {
	/* ... Fill this in ... */
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	//TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

	//TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);

	//TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);

	//TBD();
}

struct cv {
	/* ... Fill this in ... */
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	//TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

	//TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	//TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	//TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);

	//TBD();
}
