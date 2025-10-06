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

/* ------------ device state ------------ */
static struct miscdevice simtemp_dev;
static struct device *simtemp_device;

static int sampling_ms = 100;
static int threshold_mC = 45000;

static u64 sample_count = 0;
static u64 alert_count = 0;
static u64 last_error = 0;

static DEFINE_MUTEX(simtemp_lock);

static enum simtemp_mode simtemp_mode = MODE_NORMAL;

static const char *mode_names[] = {
    "normal",
    "noisy",
    "ramp",
};

/* ------------ sysfs attributes ------------ */

static ssize_t mode_show(struct device *dev,
                         struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", mode_names[simtemp_mode]);
}

static ssize_t mode_store(struct device *dev,
                          struct device_attribute *attr,
                          const char *buf, size_t count)
{
    int i;

    mutex_lock(&simtemp_lock);
    for (i = 0; i < ARRAY_SIZE(mode_names); i++) {
        if (sysfs_streq(buf, mode_names[i])) {
            simtemp_mode = i;
            mutex_unlock(&simtemp_lock);
            return count;
        }
    }
    last_error++;
    mutex_unlock(&simtemp_lock);
    return -EINVAL;
}

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
    return sprintf(buf, "samples=%llu alerts=%llu last_error=%llu\n",
                   sample_count, alert_count, last_error);
}

/* define the attributes */
static DEVICE_ATTR_RW(sampling_ms);
static DEVICE_ATTR_RW(threshold_mC);
static DEVICE_ATTR_RW(mode);
static DEVICE_ATTR_RO(stats);

/* ------------ file operations ------------ */

static ssize_t simtemp_read(struct file *file, char __user *buf,
                            size_t count, loff_t *ppos)
{
    struct simtemp_sample sample;
    static int temp_mC = 20000;   /* current simulated temperature */
    static bool ramp_up = true;   /* direction for ramp mode */
    int min_temp = 10000;         /* 10°C */
    int max_temp = 50000;         /* 50°C */
    int step_normal = 200;        /* 0.2°C per sample */
    int step_ramp = 500;          /* 0.5°C per sample */
    u32 rand_val;

    mutex_lock(&simtemp_lock);

    switch (simtemp_mode) {
    case MODE_NORMAL:
        /* Slowly increase until reaching threshold, then fluctuate */
        if (temp_mC < threshold_mC - 200) {
            temp_mC += step_normal;
        } else if (temp_mC > threshold_mC + 200) {
            temp_mC -= step_normal;
        } else {
            /* Small fluctuation around the target */
            get_random_bytes(&rand_val, sizeof(rand_val));
            temp_mC += (int)(rand_val % 400) - 200; /* +/-0.2°C noise */
        }

        /* clamp temperature */
        if (temp_mC < min_temp) temp_mC = min_temp;
        if (temp_mC > max_temp) temp_mC = max_temp;
        break;

    case MODE_NOISY:
        /* Random temp around 25°C with noise */
        get_random_bytes(&rand_val, sizeof(rand_val));
        temp_mC = 25000 + ((int)(rand_val % 10000) - 5000);
        break;

    case MODE_RAMP:
        /* Ramp up and down */
        if (ramp_up)
            temp_mC += step_ramp;
        else
            temp_mC -= step_ramp;

        if (temp_mC >= max_temp) {
            temp_mC = max_temp;
            ramp_up = false;
        } else if (temp_mC <= min_temp) {
            temp_mC = min_temp;
            ramp_up = true;
        }
        break;
    }

    sample.temp_mC = temp_mC;
    sample.timestamp_ns = ktime_get_ns();
    sample.flags = 0x1;

    /* Update stats */
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
    device_create_file(simtemp_device, &dev_attr_mode);
    device_create_file(simtemp_device, &dev_attr_stats);

    pr_info("simtemp: loaded\n");
    return 0;
}

static void __exit simtemp_exit(void)
{
    device_remove_file(simtemp_device, &dev_attr_sampling_ms);
    device_remove_file(simtemp_device, &dev_attr_threshold_mC);
    device_remove_file(simtemp_device, &dev_attr_mode);
    device_remove_file(simtemp_device, &dev_attr_stats);

    misc_deregister(&simtemp_dev);
    pr_info("simtemp: unloaded\n");
}

module_init(simtemp_init);
module_exit(simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gabriela de la Fuente");
MODULE_DESCRIPTION("Simulated temperature device with sysfs attributes + modes");
