#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/random.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "nxp_simtemp.h"

/* ------------ device state ------------ */
static struct miscdevice simtemp_dev;
static struct device *simtemp_device;

static int sampling_ms = 100;
static int threshold_mC = 45000;
static int default_mode = MODE_NORMAL;

static u64 sample_count = 0;
static u64 alert_count = 0;
static u64 last_error = 0;

static DEFINE_MUTEX(simtemp_lock);
static enum simtemp_mode simtemp_mode = MODE_NORMAL;

/* ------------ mode names array ------------ */
static const char *mode_names[] = {
    "normal",
    "noisy",
    "ramp",
};

/* ------------ module parameters ------------ */
module_param(sampling_ms, int, 0644);
MODULE_PARM_DESC(sampling_ms, "Sampling period in milliseconds");

module_param(threshold_mC, int, 0644);
MODULE_PARM_DESC(threshold_mC, "Temperature threshold in millicelsius");

module_param(default_mode, int, 0644);
MODULE_PARM_DESC(default_mode, "Default mode (0=normal, 1=noisy, 2=ramp)");

/* ------------ forward declarations ------------ */
static ssize_t mode_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t threshold_mC_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t threshold_mC_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t reset_alerts_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t simtemp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);

/* ------------ sysfs attributes ------------ */
static DEVICE_ATTR_RW(sampling_ms);
static DEVICE_ATTR_RW(threshold_mC);
static DEVICE_ATTR_RW(mode);
static DEVICE_ATTR_RO(stats);
static DEVICE_ATTR_WO(reset_alerts);

/* ------------ file operations ------------ */
static const struct file_operations simtemp_fops = {
    .owner = THIS_MODULE,
    .read  = simtemp_read,
};

/* ------------ device tree parsing ------------ */
static int simtemp_parse_dt(struct device *dev)
{
    struct device_node *np = dev->of_node;
    const char *mode_str;
    int ret;

    if (!np)
        return 0;

    /* Get sampling period from device tree */
    ret = of_property_read_u32(np, "sampling-period-ms", &sampling_ms);
    if (ret && ret != -EINVAL)
        dev_warn(dev, "Could not read sampling-period-ms\n");

    /* Get threshold from device tree */
    ret = of_property_read_u32(np, "temperature-threshold-mc", &threshold_mC);
    if (ret && ret != -EINVAL)
        dev_warn(dev, "Could not read temperature-threshold-mc\n");

    /* Get default mode from device tree */
    ret = of_property_read_string(np, "default-mode", &mode_str);
    if (!ret) {
        int i;
        for (i = 0; i < ARRAY_SIZE(mode_names); i++) {
            if (sysfs_streq(mode_str, mode_names[i])) {
                default_mode = i;
                simtemp_mode = i;
                break;
            }
        }
        if (i == ARRAY_SIZE(mode_names))
            dev_warn(dev, "Invalid default-mode: %s\n", mode_str);
    }

    dev_info(dev, "DT params: sampling=%dms, threshold=%dmC, mode=%s\n",
             sampling_ms, threshold_mC, mode_names[default_mode]);

    return 0;
}

/* ------------ platform driver ------------ */
static int nxp_simtemp_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    int ret;

    dev_info(dev, "NXP SimTemp probe started\n");

    /* Use module parameters if no device tree */
    if (!dev->of_node) {
        simtemp_mode = default_mode;
        dev_info(dev, "Using module params: sampling=%dms, threshold=%dmC, mode=%s\n",
                 sampling_ms, threshold_mC, mode_names[default_mode]);
    } else {
        /* Parse device tree parameters */
        simtemp_parse_dt(dev);
    }

    /* Initialize misc device */
    simtemp_dev.minor = MISC_DYNAMIC_MINOR;
    simtemp_dev.name = "simtemp";
    simtemp_dev.fops = &simtemp_fops;
    simtemp_dev.parent = dev;

    ret = misc_register(&simtemp_dev);
    if (ret) {
        dev_err(dev, "misc_register failed: %d\n", ret);
        return ret;
    }

    simtemp_device = simtemp_dev.this_device;

    /* Create sysfs attributes */
    device_create_file(simtemp_device, &dev_attr_sampling_ms);
    device_create_file(simtemp_device, &dev_attr_threshold_mC);
    device_create_file(simtemp_device, &dev_attr_mode);
    device_create_file(simtemp_device, &dev_attr_stats);
    device_create_file(simtemp_device, &dev_attr_reset_alerts);

    dev_info(dev, "NXP SimTemp loaded (sampling=%dms, threshold=%dmC, mode=%s)\n",
             sampling_ms, threshold_mC, mode_names[simtemp_mode]);

    return 0;
}

static int nxp_simtemp_remove(struct platform_device *pdev)
{
    device_remove_file(simtemp_device, &dev_attr_sampling_ms);
    device_remove_file(simtemp_device, &dev_attr_threshold_mC);
    device_remove_file(simtemp_device, &dev_attr_mode);
    device_remove_file(simtemp_device, &dev_attr_stats);
    device_remove_file(simtemp_device, &dev_attr_reset_alerts);

    misc_deregister(&simtemp_dev);

    dev_info(&pdev->dev, "NXP SimTemp unloaded\n");
    return 0;
}

/* Device Tree match table */
static const struct of_device_id nxp_simtemp_of_match[] = {
    { .compatible = "nxp,simtemp" },
    { .compatible = "simtemp" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nxp_simtemp_of_match);

/* Platform driver structure */
static struct platform_driver nxp_simtemp_driver = {
    .probe = nxp_simtemp_probe,
    .remove = nxp_simtemp_remove,
    .driver = {
        .name = "nxp-simtemp",
        .of_match_table = nxp_simtemp_of_match,
        .owner = THIS_MODULE,
    },
};

/* ------------ sysfs attribute implementations ------------ */
static ssize_t mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", mode_names[simtemp_mode]);
}

static ssize_t mode_store(struct device *dev, struct device_attribute *attr,
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

static ssize_t sampling_ms_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", sampling_ms);
}

static ssize_t sampling_ms_store(struct device *dev, struct device_attribute *attr,
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

static ssize_t threshold_mC_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", threshold_mC);
}

static ssize_t threshold_mC_store(struct device *dev, struct device_attribute *attr,
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

static ssize_t stats_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "samples=%llu alerts=%llu last_error=%llu\n",
                   sample_count, alert_count, last_error);
}

static ssize_t reset_alerts_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    int val;
    
    if (kstrtoint(buf, 10, &val))
        return -EINVAL;
    
    if (val == 1) {
        mutex_lock(&simtemp_lock);
        alert_count = 0;
        sample_count = 0;
        last_error = 0;
        mutex_unlock(&simtemp_lock);
        pr_info("simtemp: alerts counter reset\n");
    }
    
    return count;
}

/* ------------ file operations implementation ------------ */
static ssize_t simtemp_read(struct file *file, char __user *buf,
                            size_t count, loff_t *ppos)
{
    struct simtemp_sample sample;
    static int temp_mC = 20000;
    static bool ramp_up = true;
    int min_temp = 10000;
    int max_temp = 50000;
    int step_normal = 200;
    int step_ramp = 500;
    u32 rand_val;

    mutex_lock(&simtemp_lock);

    switch (simtemp_mode) {
    case MODE_NORMAL:
        if (temp_mC < threshold_mC - 200) {
            temp_mC += step_normal;
        } else if (temp_mC > threshold_mC + 200) {
            temp_mC -= step_normal;
        } else {
            get_random_bytes(&rand_val, sizeof(rand_val));
            temp_mC += (int)(rand_val % 400) - 200;
        }

        if (temp_mC < min_temp) temp_mC = min_temp;
        if (temp_mC > max_temp) temp_mC = max_temp;
        break;

    case MODE_NOISY:
        get_random_bytes(&rand_val, sizeof(rand_val));
        temp_mC = 25000 + ((int)(rand_val % 10000) - 5000);
        break;

    case MODE_RAMP:
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

/* ------------ module init/exit ------------ */
static int __init nxp_simtemp_init(void)
{
    int ret;

    pr_info("NXP SimTemp driver initializing\n");

    ret = platform_driver_register(&nxp_simtemp_driver);
    if (ret) {
        pr_err("Failed to register platform driver: %d\n", ret);
        return ret;
    }

    return 0;
}

static void __exit nxp_simtemp_exit(void)
{
    platform_driver_unregister(&nxp_simtemp_driver);
    pr_info("NXP SimTemp driver unloaded\n");
}

module_init(nxp_simtemp_init);
module_exit(nxp_simtemp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gabriela de la Fuente");
MODULE_DESCRIPTION("NXP Simulated temperature device with device tree support");
MODULE_ALIAS("platform:nxp-simtemp");