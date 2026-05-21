#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <net/sock.h>
#include "aaron_sock.h"
#include "aaron_http.h"
#include "aaron_route.h"

static struct socket *listen_sock;
static struct task_struct *accept_thread;

static void aaron_handle_client(struct socket *sock)
{
	struct aaron_http_request req;
	char *buf;
	struct kvec iov;
	struct msghdr msg = { 0 };
	int len;

	buf = kmalloc(AARON_HTTP_BUF_SIZE, GFP_KERNEL);
	if (!buf)
		goto out;

	iov.iov_base = buf;
	iov.iov_len = AARON_HTTP_BUF_SIZE - 1;

	len = kernel_recvmsg(sock, &msg, &iov, 1, AARON_HTTP_BUF_SIZE - 1, 0);
	if (len <= 0)
		goto out_free;

	buf[len] = '\0';

	if (aaron_http_parse_request(buf, len, &req) == 0)
		aaron_route_dispatch(sock, &req);

out_free:
	kfree(buf);
out:
	kernel_sock_shutdown(sock, SHUT_RDWR);
	sock_release(sock);
}

static int aaron_accept_loop(void *data)
{
	struct socket *client;
	int ret;

	while (!kthread_should_stop()) {
		ret = kernel_accept(listen_sock, &client, O_NONBLOCK);
		if (ret == -EAGAIN) {
			schedule_timeout_interruptible(msecs_to_jiffies(10));
			continue;
		}
		if (ret < 0) {
			if (kthread_should_stop())
				break;
			pr_err("accept error: %d\n", ret);
			schedule_timeout_interruptible(msecs_to_jiffies(100));
			continue;
		}

		aaron_handle_client(client);
	}

	return 0;
}

int aaron_sock_start(unsigned short port, int backlog)
{
	struct sockaddr_in addr;
	int ret, opt = 1;

	ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
			       &listen_sock);
	if (ret) {
		pr_err("sock_create_kern failed: %d\n", ret);
		return ret;
	}

	ret = kernel_setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
				(char *)&opt, sizeof(opt));
	if (ret)
		pr_warn("SO_REUSEADDR failed: %d\n", ret);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	ret = kernel_bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr));
	if (ret) {
		pr_err("bind failed: %d\n", ret);
		goto err;
	}

	ret = kernel_listen(listen_sock, backlog);
	if (ret) {
		pr_err("listen failed: %d\n", ret);
		goto err;
	}

	accept_thread = kthread_run(aaron_accept_loop, NULL, "aaron_accept");
	if (IS_ERR(accept_thread)) {
		ret = PTR_ERR(accept_thread);
		accept_thread = NULL;
		pr_err("kthread_run failed: %d\n", ret);
		goto err;
	}

	return 0;

err:
	sock_release(listen_sock);
	listen_sock = NULL;
	return ret;
}

void aaron_sock_stop(void)
{
	if (accept_thread) {
		kthread_stop(accept_thread);
		accept_thread = NULL;
	}

	if (listen_sock) {
		kernel_sock_shutdown(listen_sock, SHUT_RDWR);
		sock_release(listen_sock);
		listen_sock = NULL;
	}
}
