#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ktime.h> 
#include "nxp_simtemp.h"

static ssize_t simtemp_read(struct file *file, char __user *buf,
                                size_t count, loff_t *ppos)
{
    struct simtemp_sample sample;

    /* Fill with predictable test data */
    sample.timestamp_ns = ktime_get_ns();      /* real timestamp */
    sample.temp_mC = 25000;                    /* 25.000 °C */
    sample.flags = 0x1;                        /* new sample flag */

    if (count < sizeof(sample))
        return -EINVAL;

    if (copy_to_user(buf, &sample, sizeof(sample)))
        return -EFAULT;

    return sizeof(sample);
}

static const struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .read  = simtemp_read,
};

static struct miscdevice simtemp_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "simtemp",      /* this creates /dev/simtemp */
    .fops  = &simtemp_fops,
};

static int __init simtemp_init(void)
{
    int ret = misc_register(&simtemp_dev);
    if (ret) {
        pr_err("simtemp: misc_register failed\n");
        return ret;
    }
    pr_info("simtemp: loaded\n");
    return 0;
}

static void __exit simtemp_exit(void)
{
    misc_deregister(&simtemp_dev);
    pr_info("simtemp: unloaded\n");
}

module_init(simtemp_init);
module_exit(simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gabriela de la Fuente");
MODULE_DESCRIPTION("Simulated temperature device");
