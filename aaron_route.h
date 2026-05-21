#ifndef AARON_ROUTE_H
#define AARON_ROUTE_H

#include <linux/net.h>
#include "aaron_http.h"

/*
 * Pre-built HTTP response — computed once at module init, sent verbatim
 * on every matching request with zero per-request allocation or formatting.
 */
struct aaron_prebuilt_response {
	char *data;
	size_t len;
};

/* Build all static responses. Call from module init. */
int aaron_route_init(void);

/* Free pre-built responses. Call from module exit. */
void aaron_route_exit(void);

/*
 * Dispatch a parsed request. Sends the pre-built response directly
 * via the provided socket. buf/buf_cap is scratch space owned by the
 * caller (used only for dynamic responses).
 */
void aaron_route_dispatch(struct socket *sock,
			  const struct aaron_http_request *req,
			  char *buf, size_t buf_cap);

#endif /* AARON_ROUTE_H */
