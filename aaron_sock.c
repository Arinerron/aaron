#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kthread.h>
#include <linux/mempool.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/workqueue.h>
#include <net/sock.h>
#include "aaron_sock.h"
#include "aaron_http.h"
#include "aaron_route.h"

static struct socket *listen_sock;
static struct task_struct *accept_thread;
static struct workqueue_struct *aaron_wq;

/*
 * Per-request work item with embedded buffers.
 * Single allocation from a mempool — guaranteed to succeed even under
 * memory pressure, and the slab cache keeps them hot.
 */
struct aaron_work {
	struct work_struct work;
	struct socket *sock;
	char recv_buf[AARON_HTTP_BUF_SIZE];
	char scratch[AARON_HTTP_BUF_SIZE];
};

static struct kmem_cache *work_cache;
static mempool_t *work_pool;

#define WORK_POOL_MIN 64

static void aaron_handle_client(struct work_struct *w)
{
	struct aaron_work *aw = container_of(w, struct aaron_work, work);
	struct socket *sock = aw->sock;
	struct aaron_http_request req;
	struct kvec iov;
	struct msghdr msg = { 0 };
	int len;

	tcp_sock_set_nodelay(sock->sk);

	iov.iov_base = aw->recv_buf;
	iov.iov_len = AARON_HTTP_BUF_SIZE - 1;

	len = kernel_recvmsg(sock, &msg, &iov, 1, AARON_HTTP_BUF_SIZE - 1, 0);
	if (len <= 0)
		goto out;

	aw->recv_buf[len] = '\0';

	if (aaron_http_parse_request(aw->recv_buf, len, &req) == 0)
		aaron_route_dispatch(sock, &req, aw->scratch,
				     AARON_HTTP_BUF_SIZE);

out:
	kernel_sock_shutdown(sock, SHUT_RDWR);
	sock_release(sock);
	mempool_free(aw, work_pool);
}

static int aaron_accept_loop(void *data)
{
	struct socket *client;
	struct aaron_work *aw;
	int ret;

	allow_signal(SIGKILL);

	while (!kthread_should_stop()) {
		ret = kernel_accept(listen_sock, &client, 0);
		if (ret < 0) {
			if (ret == -EINVAL || kthread_should_stop() ||
			    signal_pending(current))
				break;
			pr_err_ratelimited("accept error: %d\n", ret);
			schedule_timeout_interruptible(msecs_to_jiffies(50));
			continue;
		}

		aw = mempool_alloc(work_pool, GFP_KERNEL);
		if (!aw) {
			kernel_sock_shutdown(client, SHUT_RDWR);
			sock_release(client);
			continue;
		}

		aw->sock = client;
		INIT_WORK(&aw->work, aaron_handle_client);
		queue_work(aaron_wq, &aw->work);
	}

	return 0;
}

int aaron_sock_start(unsigned short port, int backlog)
{
	struct sockaddr_in addr;
	int ret;

	work_cache = kmem_cache_create("aaron_work",
				       sizeof(struct aaron_work),
				       __alignof__(struct aaron_work),
				       SLAB_HWCACHE_ALIGN, NULL);
	if (!work_cache) {
		pr_err("failed to create slab cache\n");
		return -ENOMEM;
	}

	work_pool = mempool_create_slab_pool(WORK_POOL_MIN, work_cache);
	if (!work_pool) {
		pr_err("failed to create mempool\n");
		ret = -ENOMEM;
		goto err_cache;
	}

	aaron_wq = alloc_workqueue("aaron_wq", WQ_UNBOUND | WQ_HIGHPRI,
				   num_online_cpus() * 2);
	if (!aaron_wq) {
		pr_err("failed to create workqueue\n");
		ret = -ENOMEM;
		goto err_pool;
	}

	ret = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
			       &listen_sock);
	if (ret) {
		pr_err("sock_create_kern failed: %d\n", ret);
		goto err_wq;
	}

	sock_set_reuseaddr(listen_sock->sk);

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	ret = kernel_bind(listen_sock, (struct sockaddr_unsized *)&addr,
			  sizeof(addr));
	if (ret) {
		pr_err("bind failed: %d\n", ret);
		goto err_sock;
	}

	ret = kernel_listen(listen_sock, backlog);
	if (ret) {
		pr_err("listen failed: %d\n", ret);
		goto err_sock;
	}

	accept_thread = kthread_run(aaron_accept_loop, NULL, "aaron_accept");
	if (IS_ERR(accept_thread)) {
		ret = PTR_ERR(accept_thread);
		accept_thread = NULL;
		pr_err("kthread_run failed: %d\n", ret);
		goto err_sock;
	}

	return 0;

err_sock:
	sock_release(listen_sock);
	listen_sock = NULL;
err_wq:
	destroy_workqueue(aaron_wq);
	aaron_wq = NULL;
err_pool:
	mempool_destroy(work_pool);
	work_pool = NULL;
err_cache:
	kmem_cache_destroy(work_cache);
	work_cache = NULL;
	return ret;
}

void aaron_sock_stop(void)
{
	if (accept_thread) {
		kernel_sock_shutdown(listen_sock, SHUT_RDWR);
		kthread_stop(accept_thread);
		accept_thread = NULL;
	}

	if (listen_sock) {
		sock_release(listen_sock);
		listen_sock = NULL;
	}

	if (aaron_wq) {
		flush_workqueue(aaron_wq);
		destroy_workqueue(aaron_wq);
		aaron_wq = NULL;
	}

	if (work_pool) {
		mempool_destroy(work_pool);
		work_pool = NULL;
	}

	if (work_cache) {
		kmem_cache_destroy(work_cache);
		work_cache = NULL;
	}
}
