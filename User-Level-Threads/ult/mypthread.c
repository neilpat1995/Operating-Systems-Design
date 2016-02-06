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

int mypthread_create(mypthread_t *thread, const mypthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
	if(num_threads == MAX_THREADS) {
		printf("Error: Too many threads. Max is %d.\n", MAX_THREADS);
		return -1;
	}

	// if we haven't yet called this function, make a struct for the original (main) thread
	if(!first) {
		// clear the array for debugging purposes
		for(int i = 0; i < sizeof(mypthread_t)*MAX_THREADS; i++) {
			((char*)threads)[i] = 0;
		}

		ucontext_t* main_context = (ucontext_t*)malloc(sizeof(ucontext_t));
		if(getcontext(main_context)){
			printf("Failed to get main context.\n");
			return -1;
		}

		//NEIL'S ADDITION- INITIALIZE FIELDS OF CONTEXT AND THREAD
		mypthread_t* main_thread = &(threads[num_threads]);
		main_thread->state = RUNNING;
		main_thread->id = 0;
		main_thread->context = main_context;
		main_thread->join_id = -1;
		main_thread->retval = 0;

		if(first) {
			printf("SOMETHING WENT WRONG, MAIN RETURNED TO CREATE()!\n");
		}

		num_threads++;
		first = 1;
	}

	int my_thread_id = num_threads;

	ucontext_t* new_context = (ucontext_t*)malloc(sizeof(ucontext_t));
	if(getcontext(new_context)){
		printf("Failed to get new context.\n");
		return -1;
	}

	if(my_thread_id != num_threads) {
		// we were created. call the function. -NO- this thread is scheduled, but not immediately completed
		//Just exit from it if this is the created thread. 
		//threads[my_thread_id-1].retval = start_routine(arg);
		//NEIL'S ADDITION
		printf("SOMETHING WENT HORRIBLY WRONG, CREATED THREAD RETURNED TO CREATE()!\n");
		return 0;

	} else {
		// we did the creating. initialize the thread.
		mypthread_t *newThread = &(threads[num_threads]);

		//NEIL'S ADDITION
		char newStack[STACK_SIZE];
		stack_t newStk;
		newStk.ss_sp = (void *)newStack;
		newStk.ss_size = STACK_SIZE;
		newStk.ss_flags = 0;
		new_context->uc_stack = newStk;
		new_context->uc_link = 0;

		newThread->state = NEW;
		newThread->id = num_threads;
		newThread->context = new_context;
		newThread->join_id = -1;
		newThread->retval = 0;

		thread->id = num_threads;
		num_threads++;
		makecontext(new_context, (void (*) (void))start_routine, 1, (void *)arg);
	}
	return 0;
}

void mypthread_exit(void *retval) {
	// store the retval
	threads[running_thread_id].retval = retval;

	// set all threads waiting on the current one to be ready
	for(int i = 0; i < num_threads; i++) {
		if(threads[i].join_id == running_thread_id) {
			threads[i].join_id = -1;
			threads[i].state = READY;
		}
	}

	// switch to the next available thread
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
			// well, looks like we're done.
			printf("Last available thread exited.\n");
			exit(0);
		}
	} while(threads[next_thread_id].state != READY && threads[next_thread_id].state != NEW);

	// update states
	threads[running_thread_id].state = DONE;
	threads[next_thread_id].state = RUNNING;

	// set context to the new one
//	free(threads[running_thread_id].context);
	running_thread_id = next_thread_id;
	setcontext(threads[next_thread_id].context);
}

int mypthread_yield(void) {
	if(!first) {
		return 0;
	}

	// we haven't finished yielding yet
	threads[running_thread_id].yielded = 0;

	// save our current thread
	if(getcontext(threads[running_thread_id].context)){
		return -1;
	}

	// have we done the rest of this stuff already?
	if(threads[running_thread_id].yielded) {
		return 0;
	} else {
		threads[running_thread_id].yielded = 1;
	}

	// switch to the next available thread
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
			// well, looks like there's nothing to yield to.
			break;
		}
	} while(threads[next_thread_id].state != READY= && threads[next_thread_id].state != NEW);

	// update states - if we were waiting for a join, we become blocked
	if(threads[running_thread_id].join_id == -1) {
		threads[running_thread_id].state = READY;
	} else {
		threads[running_thread_id].state = BLOCKED;
	}
	threads[next_thread_id].state = RUNNING;

	// set context to the new one
	int temp = running_thread_id;
	running_thread_id = next_thread_id;
	printf("Next thread id: %d\n", threads[next_thread_id].id);
	setcontext(threads[next_thread_id].context);

	return 0;
}

int mypthread_join(mypthread_t thread, void **retval) {
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