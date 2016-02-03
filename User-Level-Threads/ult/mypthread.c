#include <stdlib.h>
#include <stdio.h>

#include "mypthread.h"

// indicates whether mypthreadcreate has beeen called yet
static int first = 1;

// array of info for all threads
static mypthread_t threads[MAX_THREADS];

// how many threads have we created?
static int num_threads = 0;

int mypthread_create(mypthread_t *thread, const mypthread_attr_t *attr,
			void *(*start_routine) (void *), void *arg)
{
	// if we haven't yet called this function, make a struct for the original (main) thread
	if(!first) {
		ucontext_t* main_context = (ucontext_t*)malloc(sizeof(ucontext_t));
		if(getcontext(main_context)){
			printf("Failed to get main context.\n");
			return -1;
		}

		mypthread_t* main_thread = &(threads[num_threads]);
		main_thread->state = RUNNING;
		main_thread->id = 0;
		main_thread->context = main_context;
		main_thread->join_id = -1;

		first = 0;
		num_threads++;
	}

	ucontext_t* new_context = (ucontext_t*)malloc(sizeof(ucontext_t));
	if(getcontext(new_context)){
		printf("Failed to get new context.\n");
		return -1;
	}
	makecontext(new_context, start_routine, *(int*)arg);

	mypthread_t* new_thread = &(threads[num_threads]);
	new_thread->state = READY;
	new_thread->id = num_threads;
	new_thread->context = new_context;
	new_thread->join_id = -1;

	num_threads++;
	return 0;
}

void mypthread_exit(void *retval)
{
	return;
}

int mypthread_yield(void)
{
	return 0;
}

int mypthread_join(mypthread_t thread, void **retval)
{
	return 0;
}