#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"
#include <stdbool.h>

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
	
};

//Queues
struct readyQ{

	struct thread * curThread;
	struct readyQ * next;
	struct readyQ * prev;
};

struct readyQ * rHead = NULL;
struct readyQ * rTail = NULL;

struct deleteQ{
	struct thread * curThread;
	struct deleteQ * next;
	struct deleteQ * prev;
};

struct deleteQ * dHead = NULL;


//Declerations
Tid findEmptyThread();
struct thread *threads[THREAD_MAX_THREADS];
void thread_stub(void (*thread_main)(void *), void *arg);
struct thread * currentlyRunningThread = NULL;

/////////////////////////////////////////////////////////////////////
void
thread_init(void)
{
	int i = 0;
	currentlyRunningThread = (struct thread*)malloc(sizeof(struct thread));
	threads[0] = (struct thread*)malloc(sizeof(struct thread));
	threads[0]->threadId = 0;
	threads[0]->state = ZERO_STATE;
	threads[0]->sp = NULL;
	threads[0]->yieldState = false;
	currentlyRunningThread = threads[0];
	currentlyRunningThread->yieldState = true;

	for (i=1; i<THREAD_MAX_THREADS; i++){

		threads[i]= (struct thread*)malloc(sizeof(struct thread));
		threads[i]->threadId = -1;
		threads[i]->state = -1;
		threads[i]->sp = NULL;
		threads[i]->yieldState = true;
	}
}

//////////////////////////////////////////////////////////////////////
Tid
thread_id()
{
	return currentlyRunningThread->threadId; 
}

//////////////////////////////////////////////////////////////////////

Tid findEmptyThread(){
	
	Tid firstEmpty = -1;
	int i;

	for(i=1; i < THREAD_MAX_THREADS; i++){
		if (threads[i]->state == ZERO_STATE){

			firstEmpty = i;
			break;
		}else{
			firstEmpty = -i;
		}
	}

	return firstEmpty;
}

//Given Function
void thread_stub(void (*thread_main)(void *), void *arg){
		
		//Tid ret;

		thread_main(arg);
		thread_exit();
}

Tid
thread_create(void (*fn) (void *), void *parg)
{
	Tid tIndex;
	struct thread* temp = (struct thread*)malloc(sizeof(struct thread));
	
	if(temp == NULL){
		return THREAD_NOMEMORY;
	}
	
	//get first empty thread -1: no more room for threads
	tIndex = findEmptyThread();
	
	if(tIndex == -1){
		return THREAD_NOMORE;
	}

	getcontext(&temp->cpuState);
	
	temp->cpuState.uc_mcontext.gregs[REG_RIP] = (long long) &thread_stub;	
    temp->cpuState.uc_mcontext.gregs[REG_RDI] = (long long) fn;
	temp->cpuState.uc_mcontext.gregs[REG_RSI] = (long long) parg;	

	temp->sp = malloc(THREAD_MIN_STACK);
	temp->threadId = tIndex;
	temp->state = READY;
	
	threads[tIndex] = temp;
	
	if(temp->sp == NULL){

		//No more memort for thread, therefore free it
		free(temp);
		return THREAD_NOMEMORY;
	}

	temp->cpuState.uc_stack.ss_sp = temp->sp;
	temp->cpuState.uc_mcontext.gregs[REG_RSP] = (long long) temp->sp + THREAD_MIN_STACK - 8;
	temp->cpuState.uc_stack.ss_size = THREAD_MIN_STACK - 8;

	struct readyQ * currentNode;
	struct readyQ * prevNode;

	currentNode = malloc(sizeof(struct readyQ));

	//Put in the queue at the end 
	if (rHead == NULL){
		
		currentNode->curThread = temp;
		currentNode->next = NULL;
		currentNode->prev = NULL;
		rHead = currentNode;
		rTail = currentNode;
		currentNode->curThread->state = READY;
		
	} else {
		
				
		prevNode = rHead;
	
		while (prevNode->next != NULL){
			
			prevNode = prevNode->next;
			
		}
		
		currentNode->curThread = temp;
		currentNode->next = NULL;
		currentNode->prev = prevNode;
		prevNode->next = currentNode;
		rTail = currentNode;
		currentNode->curThread->state = READY;
	}
	
	return currentNode->curThread->threadId;
}

//////////////////////////////////////////////////////////////////////

//return place in q given Tid, else return NULL
struct readyQ * findThread(Tid thread){

	struct readyQ * node = NULL;

	node = rHead;
	
	while(node != NULL){

		if(node->curThread->threadId == thread){
			return node;
			
		}

	node = node->next;

	}

	return NULL;
}


Tid
thread_yield(Tid want_tid)
{
	//Find the node tied to the thread id given, and the node before
	struct readyQ * node;
	struct readyQ * queueNode = NULL;
	Tid returnTid;

	
	if(want_tid == THREAD_ANY){
		
		if(rHead != NULL){
			want_tid = rHead->curThread->threadId;
			if(want_tid == currentlyRunningThread->threadId){
				want_tid = rTail->curThread->threadId;
			}
				
		}else {
			return THREAD_NONE;
		}
						
	}

	if (want_tid == THREAD_SELF || want_tid == currentlyRunningThread->threadId){
		return currentlyRunningThread->threadId;
	}


	if(want_tid >= THREAD_MAX_THREADS || want_tid < 0){
			return THREAD_INVALID;
	}


	if(threads[want_tid]->state == ZERO_STATE){
	
		return THREAD_INVALID;
	}
	
	if(rHead == NULL){
		return THREAD_NONE;
	}
	

	//printf("\n1\n");

	node = findThread(want_tid);
	
	//printf("\nNodeID: %d\n", node->curThread->threadId);
	//save context
	//
	//printf("\nCur State %d\n", currentlyRunningThread->threadId);
	
	getcontext(&currentlyRunningThread->cpuState);
	
	//printf("\nNodeID: %d\n", node->curThread->threadId);
	//
	if(currentlyRunningThread->yieldState == true){
		
		//printf("\nCur State %d\n", currentlyRunningThread->threadId);

		queueNode = malloc(sizeof(struct readyQ));
		queueNode->curThread = currentlyRunningThread;
		queueNode->next = NULL;
		queueNode->prev = NULL;

	//	printf("\nQueueodeID: %d\n", queueNode->curThread->threadId);

		currentlyRunningThread->yieldState =  false; 
		
		//regular case
		if(node->prev != NULL){

			//if the node has a node after
			if(node->next != NULL){
				node->prev->next = node->next;
				node->next->prev = node->prev;

				rTail->next = queueNode;
				queueNode->prev = rTail;
				rTail = queueNode;
				queueNode->curThread->state = READY;
				

				node->curThread->state = RUNNING;
				
			//if the node does not have a node after	
			} else {

				node->prev->next = queueNode;
				queueNode->prev = node->prev;
				rTail = queueNode;
				queueNode->curThread->state = READY;

				node->prev->next = NULL;
				node->curThread->state = RUNNING;

			}

		//Head Case
		}else{
			//head is not only item in readyQ
			if(node->next != NULL){
				rHead = node->next;
				

				node->curThread->state = RUNNING;
				rHead->prev = NULL;
				
				rTail->next = queueNode;
				queueNode->prev = rTail;
				rTail = queueNode;
				queueNode->curThread->state = READY;
				

			//node to be run is only thing in readyQ
			} else {
				node->curThread->state = RUNNING;
				rHead = queueNode;
				queueNode->curThread->state = READY;
	
			}

			
		}
		
			returnTid = currentlyRunningThread->threadId;
			currentlyRunningThread= node->curThread;
			setcontext(&(node->curThread)->cpuState);
		
	} else {
		
		
		currentlyRunningThread->yieldState = true;

	}
		printf("\nReturn2: %d\n", returnTid);
		return returnTid;


}

//////////////////////////////////////////////////////////////////////
void
thread_exit()
{	
	Tid threadTK;

	threadTK = currentlyRunningThread->threadId;

	thread_kill(threadTK);


	thread_yield(THREAD_ANY);
}

///////////////////////////////////////////////////////////////////////
Tid
thread_kill(Tid tid)
{
	struct readyQ * node;
	node = findThread(tid);

	if(node != NULL){
		threads[tid]->state=EXIT;
	} else {

		return THREAD_INVALID;
	}


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
	///TBD();
	free(wq);
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

/* suspend current thread until Thread tid exits */
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
