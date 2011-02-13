/**
 * My syslog daemon
 *
 * Objective
 * =========
 * Write a re-implementation of the syslog daemon and an associated library.
 * The daemon should wait for input from some sort of pipe.  The client
 * applications are linked with your library.  The daemon should add date and
 * time with millisecond precision to the messages.  The program that sends the
 * log messages should know nothing of the implementation of the daemon or even
 * of the pipes that are used to relay the messages.  The library should handle
 * all this.  The implementation must support multiple simultaenous senders.
 * (Note: This requires some special work, write a "stresstester" to your
 * daemon and check if it can handle a lot of workload.)  In addition, messages
 * from different sources should be differentiated, for example identical
 * messages from two different senders should be differentiable.  Log messages
 * should resemble something like: DATE TIME WHO MESSAGE.
 *
 * Example:
 *
 * Dec 24 12:00:01.250 clientname_and/or_pid This is my message
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include "mysyslog.h"

#define TIMESTAMP_BUFSIZE 64
#define LOG_WRITER_BUFSIZE (TIMESTAMP_BUFSIZE + MYSYSLOG_FIFO_BUFSIZE + 1)
#define MAXTHREADS 100

/* Global variables */
#if defined(DEBUG)
	int daemonize = 0;
#else
	int daemonize = 1;
#endif

int flag_run = 1;

/* Log file fd */
int lfd;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Proxy to say something */
size_t say(const char *msg, ...);

/* Sigint handler */
void sigint_handler(int sig)
{
	flag_run = 0;
}

/* Prints help */
void print_usage(int argc, char **argv);

/* Saves log message to the log file. Threads use this function. */
void *save_msg(void *arg);

/* Writes timestamp to the given buffer.
 * Happily c/p from: http://stackoverflow.com/questions/1551597/
 * using-strftime-in-c-how-can-i-format-time-exactly-like-a-unix-timestamp */
char *get_timestamp(char *buf);

/* Reads log message. Blocks until message has arrived, '\n'. Returns number of
 * read characters. */
int read_msg(char *buf, FILE* fd);

/* Allocates memory for max number of log messages. */
char **create_msgbuf(int max);

/* Frees log message buffer */
void free_msgbuf(char **ptr, int max);

/* Main function: listens to fifo for log messages. When a new message has
 * been retrieved, program adds timestamp and saves the message to the logfile. */
int main(int argc, char **argv)
{
	int i;
	size_t L;
	pid_t pid;
	FILE* fifo;
	char buf[MYSYSLOG_FIFO_BUFSIZE], c;
	char **logmsg_buffer;
	struct sigaction sa;
	pthread_attr_t tattr;
	pthread_t tids[MAXTHREADS];

	/* init signal handlers */
	sa.sa_handler = sigint_handler;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	/* handle options */
	while ((c = getopt(argc, argv, "nh")) != -1) {
		switch (c) {
			case 'h':
				print_usage(argc, argv);
				exit(0);
				break;
			case 'n':
				daemonize = 0;
				break;
			default:
				print_usage(argc, argv);
				exit(0);
				break;
		}
	}

	/* summon a daemon */
	if (daemonize) {
		switch (pid = fork()) {
			case -1:
				perror("fork");
				exit(1);
				break;

			case 0:
				printf("mysyslog: started as a daemon, pid=%d\n", getpid());
				break;

			default:
				exit(EXIT_SUCCESS); /* kill parent */
				break;
		}
	}

	/* init thread attributes */
	if (pthread_attr_init(&tattr)) {
		perror("pthread_attr_init");
		exit(1);
	}

	/* we are not interested in threads' termination status, because
	 * if we do mysyslog will be blocked until the thread complites. */
	if (pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED)) {
		perror("pthread_attr_setdetachstate");
		exit(1);
	}

	if ((lfd = open(MYSYSLOG_LOGFILE_NAME,
			O_WRONLY | O_CREAT | O_APPEND, 0666)) == -1) {
		perror("open");
		exit(1);
	}

	/* allocate write buffer for log messages */
	logmsg_buffer = create_msgbuf(MAXTHREADS);

	/* open FIFO for incoming log messages. Blocks until first message arrives */
	say("open fifo \"%s\" for listening to new log messages...\n",
			MYSYSLOG_FIFO_NAME);

	mknod(MYSYSLOG_FIFO_NAME, S_IFIFO | 0666, 0);
	if ((fifo = fopen(MYSYSLOG_FIFO_NAME, "r")) == NULL) {
		perror(MYSYSLOG_FIFO_NAME);
		goto byebye;
	}

	/* read from FIFO line by line while flag_run has been set */
	do {
		if ((L = read_msg(buf, fifo))) {

			/* find the first free index from the write buffer */
			for (i = 0; i < MAXTHREADS; i++) {
				if (logmsg_buffer[i][0] == '\0') {

					/* copy the message from fifo buffer to the write buffer */
					strcpy(logmsg_buffer[i], buf);
					say("message copied to buf index, i = %d\n", i);

					/* ready to create a thread, which saves the log message
					 * to the log file */
					pthread_create(
						&tids[i],
						&tattr,
						save_msg,
						(void *)(logmsg_buffer[i])
					);

					say("thread created, tid = %u (0x%x)\n",
						(unsigned int) tids[i],	(unsigned int) tids[i]);

					/* thread is on her way, we can take a break here */
					break;
				}
			}
			if (i == MAXTHREADS) {
				say("ignored message %s", buf);
			}
		}
	} while (flag_run);

byebye:

	say("free allocated memory for write buffer.\n");
	free_msgbuf(logmsg_buffer, MAXTHREADS);

	say("free allocated memory for tread attributes.\n");
	pthread_attr_destroy(&tattr);

	say("Bye bye.\n");
	return 0;
}

char *get_timestamp(char *buf)
{
	char fmt[64];
	struct timeval tv;
	struct tm *tm;

	gettimeofday(&tv, NULL); /* always return 0 */
	if((tm = localtime(&tv.tv_sec)) != NULL) {
		strftime(fmt, sizeof fmt, "%b %d %H:%M:%S.%%03u", tm);
		snprintf(buf, 64, fmt, tv.tv_usec / 1000);
		return buf;
	}
	buf[0] = '\0';
	return buf;
}

int read_msg(char *buf, FILE* fd)
{
	if(!fgets(buf, MYSYSLOG_FIFO_BUFSIZE, fd)) {
		buf[0] = '\0';
		return 0;
	}

	return strlen(buf);
}

char **create_msgbuf(int max)
{
	int i;
	char **ptr;

	ptr = malloc(max * sizeof(char*));
	for (i = 0; i < max; i++) {
		ptr[i] = malloc(LOG_WRITER_BUFSIZE * sizeof(char));
		ptr[i][0] = '\0';
	}

	return ptr;
}

void free_msgbuf(char **ptr, int max)
{
	int i;

	for (i = 0; i < max; i++) {
		free(ptr[i]);
	}
	free(ptr);
}

void *save_msg(void *arg)
{
	int L;
	pthread_t tid;
	char tstamp[TIMESTAMP_BUFSIZE];
	char message[LOG_WRITER_BUFSIZE];

	tid = pthread_self();

	/* wait for mutex lock */
	say("waiting for mutex lock, tid=%u\n",	(unsigned int) tid);
	pthread_mutex_lock(&mutex);

	say("got mutex lock, tid=%u\n", (unsigned int) tid);

	/* format log message */
	get_timestamp(tstamp);
	snprintf(message, LOG_WRITER_BUFSIZE, "%s %s", tstamp, (char *) arg);
	L = strlen(message);

	/* save message into log file */
	if (write(lfd, message, L) == -1) {
		return ((void *) -1);
	}

	if (!daemonize) {
		message[L-1] = '\0';
		say("wrote \"%s\" to %s\n", message, MYSYSLOG_LOGFILE_NAME);
	}

	/* make write buffer available for new messages */
	*((char *) arg) = '\0';

	if (!daemonize) {
		say("release mutex lock, tid=%u\n", (unsigned int) tid);
	}

	/* release mutex lock and return */
	pthread_mutex_unlock(&mutex);
	return ((void *) 0);
}

size_t say(const char *msg, ...)
{
	size_t rval = 0;
	va_list arglist;

	va_start(arglist, msg);
	va_end(arglist);

	if (!daemonize) {
		rval = vfprintf(stdout, msg, arglist);
	}
	return rval;
}

void print_usage(int argc, char **argv)
{
	puts("Mysyslog");
	puts("When started listens to mysyslog.pipe for log messages. Messages have to be separated by '\\n'.");
	puts("Log messages are stored in to mysyslog.log file.");
	puts("");
	puts("Usage of the library:");
	puts("---------------------");
	puts("You should use the provided libmysyslog.a library to write log messages!");
	puts("");
	puts("\tcreate_mysyslog(char *process_name);                Creates a new mysyslog writer");
	puts("\tputlog(struct mysyslog *logger, char *log_message); Writes log message");
	puts("\tfree_mysyslog(logger);                              Release the memory");
	puts("");
	puts("EXAMPLE:");
	puts("\tstruct mysyslog *logger;");
	puts("\tlogger = create_mysyslog(argv[0]);");
	puts("\tputlog(logger, \"Hello mysyslog!\");");
	puts("\tfree_mysyslog(logger);");
	puts("");
	puts("Usage of the mysyslog daemon:");
	puts("-----------------------------");
	puts("\t-n\tDoes not daemonize");
}

