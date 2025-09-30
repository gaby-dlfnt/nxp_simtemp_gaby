#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/random.h>
#include "nxp_simtemp.h"

/* device state */
static struct miscdevice simtemp_dev;
static struct device *simtemp_device;

static int sampling_ms = 100;
static int threshold_mC = 45000;
static u64 sample_count = 0;
static u64 alert_count = 0;
static DEFINE_MUTEX(simtemp_lock);

/* ------------ sysfs attributes ------------ */

/* show/store for sampling_ms */
static ssize_t sampling_ms_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", sampling_ms);
}

static ssize_t sampling_ms_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    int val;
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;

    mutex_lock(&simtemp_lock);
    sampling_ms = val;
    mutex_unlock(&simtemp_lock);

    return count;
}

/* show/store for threshold_mC */
static ssize_t threshold_mC_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", threshold_mC);
}

static ssize_t threshold_mC_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    int val;
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;

    mutex_lock(&simtemp_lock);
    threshold_mC = val;
    mutex_unlock(&simtemp_lock);

    return count;
}

/* stats read-only */
static ssize_t stats_show(struct device *dev,
                          struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "samples=%llu alerts=%llu\n",
                   sample_count, alert_count);
}

/* define the attributes */
static DEVICE_ATTR_RW(sampling_ms);
static DEVICE_ATTR_RW(threshold_mC);
static DEVICE_ATTR_RO(stats);

/* ------------ file operations ------------ */

static ssize_t simtemp_read(struct file *file, char __user *buf,
                            size_t count, loff_t *ppos)
{
    struct simtemp_sample sample;
    u32 rand_val;
    int min_temp = 10000;   /* 10.000 °C in milliCelsius */
    int max_temp = 50000;   /* 50.000 °C in milliCelsius */
    int range = max_temp - min_temp;

    /* Get a random value within range */
    rand_val = get_random_u32() % (range + 1);

    sample.timestamp_ns = ktime_get_ns();
    sample.temp_mC = min_temp + rand_val;      /* random temp between 20–30°C */
    sample.flags = 0x1;                        /* new sample flag */

    /* update counters */
    mutex_lock(&simtemp_lock);
    sample_count++;
    if (sample.temp_mC > threshold_mC)
        alert_count++;
    mutex_unlock(&simtemp_lock);

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

/* ------------ module init/exit ------------ */

static int __init simtemp_init(void)
{
    int ret;

    simtemp_dev.minor = MISC_DYNAMIC_MINOR;
    simtemp_dev.name  = "simtemp";
    simtemp_dev.fops  = &simtemp_fops;

    ret = misc_register(&simtemp_dev);
    if (ret) {
        pr_err("simtemp: misc_register failed\n");
        return ret;
    }

    simtemp_device = simtemp_dev.this_device;

    /* create sysfs attributes */
    device_create_file(simtemp_device, &dev_attr_sampling_ms);
    device_create_file(simtemp_device, &dev_attr_threshold_mC);
    device_create_file(simtemp_device, &dev_attr_stats);

    pr_info("simtemp: loaded\n");
    return 0;
}

static void __exit simtemp_exit(void)
{
    device_remove_file(simtemp_device, &dev_attr_sampling_ms);
    device_remove_file(simtemp_device, &dev_attr_threshold_mC);
    device_remove_file(simtemp_device, &dev_attr_stats);

    misc_deregister(&simtemp_dev);
    pr_info("simtemp: unloaded\n");
}

module_init(simtemp_init);
module_exit(simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gabriela de la Fuente");
MODULE_DESCRIPTION("Simulated temperature device with sysfs attributes");
