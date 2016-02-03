#include "mypthread.h"

static mypthread_t threads[MAX_THREADS];

int mypthread_create(mypthread_t *thread, const mypthread_attr_t *attr,
			void *(*start_routine) (void *), void *arg)
{
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