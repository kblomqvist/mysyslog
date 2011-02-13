/**
 * Stresstester to test mysyslog and its library component.
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/inotify.h>
#include "mysyslog.h"

#define MAXTHREADS 200

void *thread_routine(void *arg)
{
	putlog(arg, "hello mysyslog");
	return ((void *) 0);
}

int main(int argc, char **argv)
{
	int i;

	struct mysyslog *mysyslog;
	pthread_t tid[MAXTHREADS];
	void *thread_result;

	mysyslog = create_mysyslog(argv[0]);

	for (i = 0; i < MAXTHREADS; i++) {
		pthread_create(&tid[i], NULL, thread_routine, mysyslog);
	}

	for (i = 0; i < MAXTHREADS; i++) {
		pthread_join(tid[i], &thread_result);
	}

	free_mysyslog(mysyslog);

	return 0;
}
