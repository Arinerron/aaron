#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/moduleparam.h>
#include "aaron_sock.h"

static unsigned short port = 8080;
module_param(port, ushort, 0444);
MODULE_PARM_DESC(port, "TCP port to listen on (default 8080)");

static int backlog = 128;
module_param(backlog, int, 0444);
MODULE_PARM_DESC(backlog, "Listen backlog (default 128)");

static int __init aaron_init(void)
{
	int ret;

	pr_info("starting on port %u\n", port);

	ret = aaron_sock_start(port, backlog);
	if (ret) {
		pr_err("failed to start: %d\n", ret);
		return ret;
	}

	pr_info("listening on port %u\n", port);
	return 0;
}

static void __exit aaron_exit(void)
{
	aaron_sock_stop();
	pr_info("stopped\n");
}

module_init(aaron_init);
module_exit(aaron_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aaron");
MODULE_DESCRIPTION("In-kernel HTTP web server");
