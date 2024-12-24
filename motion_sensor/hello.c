/*
 * TODO:
 *  [x] Basic char driver initialization
 *  [ ] SysFs initialization (no need to run mknod)
 *  [ ] i2c client initialization
 *  [ ] Sensor read
 */
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#define APDS_MAJOR 42
#define APDS_MAX_MINORS 1
#define APDS9960_ADDR 0x39
#define PROX_DATA_REG 0x9C
#define DEVICE_NAME "apds9960"

struct apds9960_data {
  struct cdev cdev;
};

struct apds9960_data apds9960_data;

ssize_t apds9960_read(struct file* file, char* buf, size_t len, loff_t* off) {
  printk(KERN_ALERT "Hello read!");
  return 0;
}

ssize_t apds9960_write(struct file* file, const char* buf, size_t len, loff_t* off) {
  printk(KERN_ALERT "Hello write!");
  return 0;
}

int apds9960_open(struct inode* inode, struct file* file) {
  printk(KERN_ALERT "Hello open!");
  return 0;
}

int apds9960_release(struct inode* inode, struct file* file) {
  return 0;
}

long int apds9960_ioctl(struct file* file, unsigned int req, long unsigned arg) {
  return 0;
}

const struct file_operations apds9960_fops = {
  .owner = THIS_MODULE,
  .open = apds9960_open,
  .read = apds9960_read,
  .write = apds9960_write,
  .release = apds9960_release,
  .unlocked_ioctl = apds9960_ioctl
};

static int apds9960_init(void)
{
  int err;

  dev_t dev = MKDEV(APDS_MAJOR, 0);
  register_chrdev_region(dev, APDS_MAX_MINORS, DEVICE_NAME);

  cdev_init(&apds9960_data.cdev, &apds9960_fops);
  err = cdev_add(&apds9960_data.cdev, dev, APDS_MAX_MINORS);
  if(err < 0) {
    printk(KERN_ERR "Failed to initialize hello!");
    unregister_chrdev_region(dev, APDS_MAX_MINORS);
    return err;
  }
  printk(KERN_ALERT "Hello, world\n");
  return 0;
}

static void apds9960_exit(void)
{
  dev_t dev = MKDEV(APDS_MAJOR, 0);
  cdev_del(&apds9960_data.cdev);
  unregister_chrdev_region(dev, APDS_MAX_MINORS);
  printk(KERN_ALERT "Goodbye, cruel world\n");
}

module_init(apds9960_init);
module_exit(apds9960_exit);

MODULE_AUTHOR("Christopher Odom");
MODULE_DESCRIPTION("APDS-9960 proximity sensor char device driver");
MODULE_LICENSE("GPL");
