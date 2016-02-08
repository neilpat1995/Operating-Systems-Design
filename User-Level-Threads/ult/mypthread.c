#include <stdlib.h>
#include <stdio.h>

#include "mypthread.h"

// indicates whether mypthread_create has been called yet
static int first = 0;

// indicates which thread is running
static int running_thread_id = 0;

// array of info for all threads
static mypthread_t threads[MAX_THREADS];

// how many threads have we created?
static int num_threads = 0;

/*	----------------------------------------------------------------------
 *	HELPER FUNCTIONS
 *	---------------------------------------------------------------------- */

// initialize a running thread
void init_thread_running(ucontext_t* context) {
	mypthread_t *running_thread = &(threads[num_threads]);

	// allocate a stack
	/*stack_t new_stack;
	new_stack.ss_sp = malloc(STACK_SIZE);
	new_stack.ss_size = STACK_SIZE;
	new_stack.ss_flags = 0;

	// assign the stack to the context
	context->uc_stack = new_stack;
	context->uc_link = 0;*/

	// initialize the thread
	running_thread->state = RUNNING;
	running_thread->id = 0;
	running_thread->context = context;
	running_thread->join_id = -1;
	running_thread->retval = 0;

	num_threads++;

	/*if(threads[0].context->uc_stack.ss_sp == 0x0) {
		printf("WHAT.\n");
	}*/
}

// initialize a new thread with the start routine and args provided
void init_thread_new(void *(*start_routine) (void *), void *arg, ucontext_t* context) {
	mypthread_t *new_thread = &(threads[num_threads]);

	// allocate a stack
	stack_t new_stack;
	new_stack.ss_sp = malloc(STACK_SIZE);
	new_stack.ss_size = STACK_SIZE;
	new_stack.ss_flags = 0;

	// assign the stack to the context
	context->uc_stack = new_stack;
	context->uc_link = 0;

	// initialize the thread
	new_thread->state = NEW;
	new_thread->id = num_threads;
	new_thread->context = context;
	new_thread->join_id = -1;
	new_thread->retval = 0;

	// set the start routine
	makecontext(context, (void (*) (void))start_routine, 1, (void *)arg);

	num_threads++;
}

// get the thread id of the next ready thread in the array (or the current if no other ones are ready)
int get_next_thread_id() {
	int next_thread_id = running_thread_id;
	do {
		next_thread_id++;

		// if we got to the end of the list
		if(next_thread_id == num_threads) {
			// go back to the beginning
			next_thread_id = 0;
		}

		// if we wrapped all the way back to where we started
		if(next_thread_id == running_thread_id) {
			// well, looks like this one's all we got
			break;
		}
	} while(threads[next_thread_id].state != READY && threads[next_thread_id].state != NEW);

	printf("next thread state = %d\n", threads[next_thread_id].state);
	return next_thread_id;
}

// swtich to the thread of the given id (does not return on success)
void switch_to_thread(int next_thread_id) {
	int temp = running_thread_id;
	running_thread_id = next_thread_id;
	printf("running thread: %d\n", running_thread_id);
	swapcontext(threads[temp].context, threads[next_thread_id].context);
}

/*	----------------------------------------------------------------------
 *	INTERFACE
 *	---------------------------------------------------------------------- */

// create a thread, filling 'thread' with info to identify it for a later join
int mypthread_create(mypthread_t *thread, const mypthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
	// check for too many threads
	if(num_threads == MAX_THREADS) {
		printf("Error: Too many threads! Max is %d.\n", MAX_THREADS);
		return -1;
	}

	// if we haven't yet called this function, make a struct for the original (main) thread
	if(!first) {
		// clear the array for debugging purposes
		for(int i = 0; i < sizeof(mypthread_t)*MAX_THREADS; i++) {
			((char*)threads)[i] = 0;
		}

		// make the main thread (starting with its context)
		ucontext_t* main_context = (ucontext_t*)malloc(sizeof(ucontext_t));
		if(getcontext(main_context)){
			printf("Error: Failed to get main context!\n");
			return -1;
		}

		if (first != 1) {
			printf("Now creating the main thread and adding to list.\n");
			init_thread_running(main_context);		
			printf("Main thread: \nState: %d \nID: %d \nJoin-ID: %d \n", threads[0].state, threads[0].id, threads[0].join_id);
			printf("Context stack: %0x\n", threads[0].context->uc_stack);
			printf("Context stack's size: %lu\n", threads[0].context->uc_stack.ss_size);
		}		

		// make sure we don't end up here again
		if(first) {
			printf("SOMETHING WENT HORRIBLY WRONG, MAIN RETURNED TO CREATE()!\n");
		}
		first = 1;
	}

	// make the new thread (starting with its context)
	ucontext_t* new_context = (ucontext_t*)malloc(sizeof(ucontext_t));
	if(getcontext(new_context)){
		printf("Error: Failed to get new context!\n");
		return -1;
	}
	init_thread_new(start_routine, arg, new_context);

	thread->id = num_threads - 1;
	return 0;
}

void mypthread_exit(void *retval) {
	printf("exit\n");
	// store the retval
	threads[running_thread_id].retval = retval;

	printf("line: %d\n", __LINE__);

	// set all threads waiting on the current one to be ready
	for(int i = 0; i < num_threads; i++) {
		if(threads[i].join_id == running_thread_id) {
			threads[i].join_id = -1;
			threads[i].state = READY;
		}
	}

	printf("line: %d\n", __LINE__);

	// switch to the next available thread
	int next_thread_id = get_next_thread_id();
	if(next_thread_id == running_thread_id) {
		// well, looks like we're done here
		printf("Error: Last runnable thread exited!\n");
		exit(0);
	}

	printf("line: %d\n", __LINE__);

	// update states
	threads[running_thread_id].state = DONE;
	threads[next_thread_id].state = RUNNING;

	printf("line: %d\n", __LINE__);

	// set context to the new one
	free(threads[running_thread_id].context->uc_stack.ss_sp);
	free(threads[running_thread_id].context);
	switch_to_thread(next_thread_id);
}

int mypthread_yield(void) {
	printf("yield\n");
	// check if we've even made any threads yet
	if(!first) {
		return 0;
	}

	// switch to the next available thread
	int next_thread_id = get_next_thread_id();
	if(next_thread_id == running_thread_id) {
		// there's nothing to yield to...
		if(threads[running_thread_id].join_id != -1) {
			// ...but we're blocked!
			printf("Error: Deadlock! Last runnable thread yielded.\n");
			exit(0);
		} else {
			// ...so let's keep going
			return 0;
		}
	}

	// update states (if we were waiting for a join, we become blocked)
	if(threads[running_thread_id].join_id == -1) {
		threads[running_thread_id].state = READY;
	} else {
		threads[running_thread_id].state = BLOCKED;
	}
	threads[next_thread_id].state = RUNNING;

	// set context to the new one
	switch_to_thread(next_thread_id);

	return 0;
}

int mypthread_join(mypthread_t thread, void **retval) {
	printf("join\n");
	// check for circular joins by jumping from thread to joined on thread
	int check_id = threads[thread.id].join_id;
	for(int i = 0; check_id != -1; i++) {
		if(check_id == running_thread_id) {
			// bad! circular join
			printf("Error: Deadlock! Circular join of length: %d\n", i);
			return(1);
		} else {
			// try the thread that this one joins on
			check_id = threads[check_id].join_id;
		}
	}

	// if that thread is done, get its retval, otherwise yield
	if(threads[thread.id].state == DONE) {
		*retval = threads[thread.id].retval;
	} else {
		threads[running_thread_id].state = BLOCKED;
		threads[running_thread_id].join_id = thread.id;
		mypthread_yield();
	}
	return 0;
}