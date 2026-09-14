// SPDX-License-Identifier: GPL-2.0

#include <linux/init.h>
#include <linux/module.h>

static int __init sentinel_init(void)
{
    pr_info("sentinel: kernel component loaded\n");
    return 0;
}

static void __exit sentinel_exit(void)
{
    pr_info("sentinel: kernel component unloaded\n");
}

module_init(sentinel_init);
module_exit(sentinel_exit);

MODULE_DESCRIPTION("Project Sentinel telemetry kernel component");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Project Sentinel contributors");
