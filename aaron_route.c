#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/string.h>
#include <net/sock.h>
#include "aaron_route.h"

static void send_buf(struct socket *sock, const char *buf, size_t len)
{
	struct kvec iov = { .iov_base = (void *)buf, .iov_len = len };
	struct msghdr msg = { 0 };

	kernel_sendmsg(sock, &msg, &iov, 1, len);
}

static void send_response(struct socket *sock, int status,
			   const char *content_type,
			   const char *body, size_t body_len)
{
	char *buf;
	int len;

	buf = kmalloc(AARON_HTTP_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		return;

	len = aaron_http_build_response(buf, AARON_HTTP_BUF_SIZE, status,
					content_type, body, body_len);
	if (len > 0)
		send_buf(sock, buf, len);

	kfree(buf);
}

static void handle_index(struct socket *sock,
			 const struct aaron_http_request *req)
{
	static const char body[] =
		"<html><body><h1>aaron</h1>"
		"<p>kernel web server</p></body></html>";

	send_response(sock, 200, "text/html", body, sizeof(body) - 1);
}

static void handle_not_found(struct socket *sock,
			     const struct aaron_http_request *req)
{
	static const char body[] =
		"<html><body><h1>404</h1><p>not found</p></body></html>";

	send_response(sock, 404, "text/html", body, sizeof(body) - 1);
}

struct route_entry {
	const char *path;
	size_t path_len;
	aaron_route_handler_t handler;
};

static const struct route_entry routes[] = {
	{ "/", 1, handle_index },
};

void aaron_route_dispatch(struct socket *sock,
			  const struct aaron_http_request *req)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(routes); i++) {
		if (req->path_len == routes[i].path_len &&
		    !memcmp(req->path, routes[i].path, routes[i].path_len)) {
			routes[i].handler(sock, req);
			return;
		}
	}

	handle_not_found(sock, req);
}
