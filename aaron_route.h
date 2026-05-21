#ifndef AARON_ROUTE_H
#define AARON_ROUTE_H

#include <linux/net.h>
#include "aaron_http.h"

/*
 * Route handler signature.
 * Receives the parsed request and the connected socket.
 * Must send a complete HTTP response on sock before returning.
 */
typedef void (*aaron_route_handler_t)(struct socket *sock,
				      const struct aaron_http_request *req);

/* Dispatch a parsed request to the matching handler and send the response. */
void aaron_route_dispatch(struct socket *sock,
			  const struct aaron_http_request *req);

#endif /* AARON_ROUTE_H */
