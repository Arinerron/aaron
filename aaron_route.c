#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/string.h>
#include <net/sock.h>
#include "aaron_route.h"

static struct aaron_prebuilt_response resp_index;
static struct aaron_prebuilt_response resp_404;

static void send_prebuilt(struct socket *sock,
			  const struct aaron_prebuilt_response *resp)
{
	struct kvec iov = { .iov_base = resp->data, .iov_len = resp->len };
	struct msghdr msg = { 0 };

	kernel_sendmsg(sock, &msg, &iov, 1, resp->len);
}

static int build_static_response(struct aaron_prebuilt_response *resp,
				 int status, const char *content_type,
				 const char *body, size_t body_len)
{
	size_t cap = body_len + 256;
	int len;

	resp->data = kmalloc(cap, GFP_KERNEL);
	if (!resp->data)
		return -ENOMEM;

	len = aaron_http_build_response(resp->data, cap, status,
					content_type, body, body_len);
	if (len < 0) {
		kfree(resp->data);
		resp->data = NULL;
		return len;
	}

	resp->len = len;
	return 0;
}

int aaron_route_init(void)
{
	static const char index_body[] =
		"<html><body><h1>aaron</h1>"
		"<p>kernel web server</p></body></html>";
	static const char not_found_body[] =
		"<html><body><h1>404</h1><p>not found</p></body></html>";
	int ret;

	ret = build_static_response(&resp_index, 200, "text/html",
				    index_body, sizeof(index_body) - 1);
	if (ret)
		return ret;

	ret = build_static_response(&resp_404, 404, "text/html",
				    not_found_body, sizeof(not_found_body) - 1);
	if (ret) {
		kfree(resp_index.data);
		resp_index.data = NULL;
		return ret;
	}

	return 0;
}

void aaron_route_exit(void)
{
	kfree(resp_index.data);
	kfree(resp_404.data);
	resp_index.data = NULL;
	resp_404.data = NULL;
}

void aaron_route_dispatch(struct socket *sock,
			  const struct aaron_http_request *req,
			  char *buf, size_t buf_cap)
{
	if (req->path_len == 1 && req->path[0] == '/') {
		send_prebuilt(sock, &resp_index);
		return;
	}

	send_prebuilt(sock, &resp_404);
}
