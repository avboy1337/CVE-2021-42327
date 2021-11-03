#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>

MODULE_DESCRIPTION("mimic vuln device");
MODULE_AUTHOR("th3lsh3ll");
MODULE_LICENSE("GPL");

#define LOG_LEVEL 	KERN_INFO

#define MY_MAJOR		42
#define MY_MINOR		0
#define NUM_MINORS		1
#define MODULE_NAME		"vuln"
#define DEVICE_NAME		"vuln"
dev_t dev_num;
struct cdev *mcdev;
int major_number;

static int vuln_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int
vuln_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t
dp_link_settings_read(struct file *f,
		char __user *buf,
		size_t size, loff_t *pos)
{
// https://elixir.bootlin.com/linux/latest/source/drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm_debugfs.c#L179
	char *rd_buf_ptr = NULL;
	if(*pos & 3 || size & 3)
		return -EINVAL;
	char* rd_buf = kcalloc(100, sizeof(char), GFP_KERNEL);
	uint32_t result = 0;
	if(!rd_buf)
		return 0;
	rd_buf_ptr = rd_buf;
	const uint32_t rd_buf_size = 100;
	int lane_count = 0;
	int link_rate = 1;
	int link_spread = 2;
	uint8_t str_len = 0;
	int r;
	size_t i;
	for(i = 0; i < 4; i++) {
		str_len = strlen("Reported:  %d  0x%x  %d  ");
		snprintf(rd_buf_ptr, str_len, "Reported:  %d  0x%x  %d  ",
				lane_count, link_rate, link_spread);
		rd_buf_ptr += str_len;
	}
	while (size) {
		if (*pos >= rd_buf_size)
			break;
		r = put_user(*(rd_buf + result), buf);
		if (r)
			return r;
		buf += 1;
		size -= 1;
		*pos += 1;
		result += 1;
	}
	kfree(rd_buf);
	return result;
}

static int parse_write_buffer_into_params(char *wr_buf, uint32_t wr_buf_size,
					long *param, const char __user *buf,
					int max_param_num, uint8_t *param_nums)
{
	char *wr_buf_ptr = NULL;
	uint32_t wr_buf_count = 0;
	int r;
	char* sub_str = NULL;
	const char delimiter[3] = {' ', '\n', '\0'};
	uint8_t param_index = 0;
	*param_nums = 0;
	wr_buf_ptr = wr_buf;
	r = copy_from_user(wr_buf_ptr, buf, wr_buf_size);
	if (r >= wr_buf_size) {
		printk(LOG_LEVEL "user data could not be read\n");
		return -EINVAL;
	}
	/* check number of parameters. isspace could not differ space and \n */
	while ((*wr_buf_ptr != 0xa) && (wr_buf_count < wr_buf_size)) {
		/* skip space*/
		printk("[thelshell] wr_buf_ptr %d wr_buf_count %d\n wr_buf_size %d *param_nums %d\n", *wr_buf_ptr, wr_buf_count, wr_buf_size, *param_nums);
		while (isspace(*wr_buf_ptr) && (wr_buf_count < wr_buf_size)) {
			wr_buf_ptr++;
			wr_buf_count++;										    }
		if (wr_buf_count == wr_buf_size)
			break;
			/* skip non-space*/
		while ((!isspace(*wr_buf_ptr)) && (wr_buf_count < wr_buf_size)) {
			wr_buf_ptr++;
			wr_buf_count++;
		}
		(*param_nums)++;
		if (wr_buf_count == wr_buf_size) {
			break;
		}
	}
	if (*param_nums > max_param_num)
		*param_nums = max_param_num;
	printk("[thelshell] param_index %d param_nums %d\n", param_index, *param_nums);
	wr_buf_ptr = wr_buf; /* reset buf pointer */
	wr_buf_count = 0; /* number of char already checked */
	while (isspace(*wr_buf_ptr) && (wr_buf_count < wr_buf_size)) {
		wr_buf_ptr++;
		wr_buf_count++;
	}
	while (param_index < *param_nums) {
		/* after strsep, wr_buf_ptr will be moved to after space */
		sub_str = strsep(&wr_buf_ptr, delimiter);
		r = kstrtol(sub_str, 16, &(param[param_index]));
		if (r)
			printk(LOG_LEVEL "[sanity_check] string to int convert error code: %d\n", r);
		param_index++;
	}
	return 0;
}

static ssize_t dp_phy_test_pattern_debugfs_write(struct file *f, const char __user *buf,
				 size_t size, loff_t *pos)
{
// https://elixir.bootlin.com/linux/v5.14.15/source/drivers/gpu/drm/amd/display/amdgpu_dm/amdgpu_dm_debugfs.c#L610
	char *wr_buf = NULL;
	uint32_t wr_buf_size = 100;
	long param[11] = {0x0};
	int max_param_num = 11;
	bool disable_hpd = false;
	bool valid_test_pattern = false;
	uint8_t param_nums = 0;
	/* init with default 80bit custom pattern */
	uint8_t custom_pattern[10] = {
			0x1f, 0x7c, 0xf0, 0xc1, 0x07,
			0x1f, 0x7c, 0xf0, 0xc1, 0x07
			};
	if (size == 0)
		return -EINVAL;

	wr_buf = kcalloc(wr_buf_size, sizeof(char), GFP_KERNEL);
	if (!wr_buf)
		return -ENOSPC;

	if (parse_write_buffer_into_params(wr_buf, size,
					   (long *)param, buf,
					   max_param_num,
					   &param_nums)) {
		kfree(wr_buf);
		return -EINVAL;
	}

	if (param_nums <= 0) {
		kfree(wr_buf);
		printk("user data not be read\n");
		return -EINVAL;
	}
	return size;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = vuln_open,
	.release = vuln_release,
	.read = dp_link_settings_read,
	.write = dp_phy_test_pattern_debugfs_write,
};

static int vuln_init(void)
{
	printk( LOG_LEVEL "Registering the char device");
	if (alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) < 0) {
		printk(KERN_ALERT "failed to allocate major number\n");
		return -EINVAL;
	} else {
		printk(KERN_INFO "major number allocated successfully\n");
	}
	major_number = MAJOR(dev_num);
	printk(KERN_INFO "major number %d\n", major_number);
	printk(KERN_INFO "use mknod /dev/%s c %d 0\n", DEVICE_NAME, major_number);
	mcdev = cdev_alloc();
	mcdev->ops = &fops;
	mcdev->owner = THIS_MODULE;
	if (cdev_add(mcdev, dev_num, 1) < 0) {
		printk(KERN_INFO "adding device to kernel failed\n");
	} else {
		printk(KERN_INFO "added device to kernel\n");
	}
	return 0;
}

static void vuln_exit(void)
{
	cdev_del(mcdev);
	printk( LOG_LEVEL "Unregistering char device");
	unregister_chrdev_region(dev_num, 1);
}

module_init(vuln_init);
module_exit(vuln_exit);
