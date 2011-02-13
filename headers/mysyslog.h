#ifndef __MYSYSLOG_H
#define __MYSYSLOG_H

/* Atomic write to pipe
 * (http://lkml.indiana.edu/hypermail/linux/kernel/0605.0/0939.html) */
#define MYSYSLOG_FIFO_BUFSIZE 4096

/** FIFO name */
#define MYSYSLOG_FIFO_NAME "mysyslog.pipe"

/** LOG file name */
#define MYSYSLOG_LOGFILE_NAME "mysyslog.log"

struct mysyslog {
	char *pname;
	int pid;
	int fifo_fd;
};

/** Creates new mysyslog struct */
struct mysyslog* create_mysyslog(const char *pname);

/** Writes a message to the logfile */
int putlog(struct mysyslog *log, const char *message);

/** Free memory allocated for mysyslog struct */
void free_mysyslog(struct mysyslog *log);

#endif
