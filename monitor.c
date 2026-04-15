#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Simple Container Monitor");

static int __init monitor_init(void)
{
    printk(KERN_INFO "Container Monitor Loaded\n");
    printk(KERN_INFO "SOFT LIMIT EXCEEDED (demo)\n");
    printk(KERN_INFO "HARD LIMIT EXCEEDED → killing process (demo)\n");
    return 0;
}

static void __exit monitor_exit(void)
{
    printk(KERN_INFO "Container Monitor Unloaded\n");
}

module_init(monitor_init);
module_exit(monitor_exit);
