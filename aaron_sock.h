#ifndef AARON_SOCK_H
#define AARON_SOCK_H

#include <linux/net.h>

/* Start listening and spawn accept/worker threads. */
int aaron_sock_start(unsigned short port, int backlog);

/* Tear down listener and stop all threads. */
void aaron_sock_stop(void);

#endif /* AARON_SOCK_H */
