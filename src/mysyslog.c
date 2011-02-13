#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "mysyslog.h"

struct mysyslog* create_mysyslog(const char* pname)
{
	struct mysyslog* log;

	if ((log = malloc(sizeof(struct mysyslog))) == NULL) {
		perror("malloc");
		return NULL;
	}

	/* set name */
	log->pname = malloc(strlen(pname) + 1);
	strcpy(log->pname, pname);

	/* set pid */
	log->pid = getpid();

	/* set fifo's fd */
	log->fifo_fd = open(MYSYSLOG_FIFO_NAME, O_WRONLY | O_NDELAY);

	return log;
}

int putlog(struct mysyslog *log, const char *message)
{
	int n;
	char buf[MYSYSLOG_FIFO_BUFSIZE];

	if (log->fifo_fd == -1) {
		return -1;
	}

	/* Mysyslog daemon reads messages from FIFO
	 * line by line, thus adding '\n' */
	n = snprintf(buf, MYSYSLOG_FIFO_BUFSIZE, "%s/%d %s\n",
			log->pname, log->pid, message);

	if (n < 0) {
		perror("sprintf");
	} else if (n >= MYSYSLOG_FIFO_BUFSIZE) {
		strcpy(&buf[MYSYSLOG_FIFO_BUFSIZE - 5], "...\n");
		n = MYSYSLOG_FIFO_BUFSIZE;
	}

	return write(log->fifo_fd, buf, n);
}

void free_mysyslog(struct mysyslog* log)
{
	if (log->fifo_fd > 0) {
		close(log->fifo_fd);
	}

	free(log->pname);
	free(log);
}
