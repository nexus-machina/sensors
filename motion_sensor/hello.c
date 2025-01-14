#include <linux/module.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define APDS_MAJOR 42
#define APDS_MAX_MINORS 1
#define APDS9960_ADDR 0x39
#define PROX_DATA_REG 0x9C
#define DEVICE_NAME "apds9960"

#define APDS_ENABLE 0x80
#define APDS_PDATA 0x90 // prox data
#define APDS_PILT 0x89
#define APDS_PIHT 0x8B
#define APDS_STATUS 0x92
#define APDS_WAIT_TIME 0x83
#define APDS_ADC_TIME 0x82
#define APDS_CONTROL 0x8f

/*
 * Color data is reported using two bytes, one
 * register for the low order bits, the next for
 * the high order bits
 */
// clear data
#define APDS_CDATAL 0x94 // low byte
#define APDS_CDATAH 0x95 // high byte
// read data
#define APDS_RDATAL 0x96
#define APDS_RDATAH 0x97
// green data
#define APDS_GDATAL 0x98
#define APDS_GDATAH 0x99
// blue data
#define APDS_BDATAL 0x9A
#define APDS_BDATAH 0x9B

// Enable bitfields 
#define APDS_ON_ENABLE (1)
#define APDS_ALS_ENABLE (1<<1)
#define APDS_PROX_ENABLE (1<<2)
#define APDS_WAIT_ENABLE (1<<3)
#define APDS_ALS_INT_ENABLE (1<<4)
#define APDS_PROX_INT_ENABLE (1<<5)
#define APDS_GESTURE_ENABLE (1<<6)

struct apds9960_dev {
  struct i2c_client* client;
  struct miscdevice apds9960_miscdevice;
  wait_queue_head_t wq;
  bool color_ready;
  struct timer_list timer;
  char name[8]; /* apds9960 */
};

static const struct of_device_id apds9960_dt_ids[] = {
  { .compatible = "arrow,apds", },
  { }
};
MODULE_DEVICE_TABLE(of, apds9960_dt_ids);

static const struct i2c_device_id i2c_ids[] = {
  { .name = "apds", },
  { }
};
MODULE_DEVICE_TABLE(i2c, i2c_ids);

/* Timer callback that marks data as ready and wakes waiting processes */
static void sensor_timer_cb(struct timer_list *t)
{
    struct apds9960_dev *dev = from_timer(dev, t, timer);
    dev->color_ready = true;
    wake_up(&dev->wq);
}

static ssize_t apds9960_write_file(struct file *file, const char __user *userbuf,
    size_t count, loff_t *ppos)
{
  int ret;
  unsigned long val;
  char buf[5];
  struct apds9960_dev * apds9960;
  apds9960 = container_of(file->private_data, struct apds9960_dev, apds9960_miscdevice);
  copy_from_user(buf, userbuf, count);
  /* Convert char array to char string */
  buf[count-1] = '\0';
  /* Convert the string to an unsigned long */
  ret = kstrtoul(buf, 0, &val);

  /* TODO: Check if the user wrote a 'm' or 'g' command, and convert that to the 
   * motion readout or gesture engine readout respectively */
  i2c_smbus_write_byte_data(apds9960->client, APDS_ENABLE, val);
  return count;
}

/* User is reading data from /dev/apds9960XX */
static ssize_t apds9960_read_file(struct file *file, char __user *userbuf,
    size_t count, loff_t *ppos)
{
  int expval, size, pvalid, avalid, enable, ret;
  unsigned char clear_low, clear_high, red_low, red_high, green_low, green_high, blue_low, blue_high;
  char buf[60]; // Increase buffer size for RGB values
  struct apds9960_dev * apds9960;
  apds9960 = container_of(file->private_data, struct apds9960_dev, apds9960_miscdevice);

  apds9960->color_ready = false;
  i2c_smbus_write_byte_data(apds9960->client, APDS_ENABLE, APDS_ON_ENABLE | APDS_PROX_ENABLE | APDS_ALS_ENABLE);
  i2c_smbus_write_byte_data(apds9960->client, APDS_ADC_TIME, 0xff);
  // Sets als gain control
  i2c_smbus_write_byte_data(apds9960->client, APDS_CONTROL, 1);
  pvalid = i2c_smbus_read_byte_data(apds9960->client, APDS_STATUS) & (1<<1);
  if(pvalid) {
    pr_info("Reading prox data\n");
    expval = i2c_smbus_read_byte_data(apds9960->client, PROX_DATA_REG);
    if (expval < 0) {
        return -EFAULT;
    }
  } else {
    pr_info("Prox not valid, skipping!!!!");
  }

  mod_timer(&apds9960->timer, msecs_to_jiffies(30));
  ret = wait_event_interruptible(apds9960->wq, apds9960->color_ready);
  if(ret)
    return ret;

  avalid = i2c_smbus_read_byte_data(apds9960->client, APDS_STATUS) & 1;
  if(avalid) {
    pr_info("Reading color data\n");
    // Read RGB values
    clear_low = i2c_smbus_read_byte_data(apds9960->client, APDS_CDATAL);
    clear_high = i2c_smbus_read_byte_data(apds9960->client, APDS_CDATAH);
    red_low = i2c_smbus_read_byte_data(apds9960->client, APDS_RDATAL);
    red_high = i2c_smbus_read_byte_data(apds9960->client, APDS_RDATAH);
    green_low = i2c_smbus_read_byte_data(apds9960->client, APDS_GDATAL);
    green_high = i2c_smbus_read_byte_data(apds9960->client, APDS_GDATAH);
    blue_low = i2c_smbus_read_byte_data(apds9960->client, APDS_BDATAL);
    blue_high = i2c_smbus_read_byte_data(apds9960->client, APDS_BDATAH);
  } else {
    pr_info("Color not valid, skipping!!!!");
  }

  // Test if both failed
  if(!avalid && !pvalid) {
      pr_info("Data not ready!\n");
      return -EFAULT;
  }

  // Prepare the output buffer
  if(avalid)
    size = sprintf(buf, "ExpVal: %02x, CRGB: C(%02x%02x) R(%02x%02x) G(%02x%02x) B(%02x%02x)",
                 expval, clear_high, clear_low, red_high, red_low, green_high, green_low, blue_high, blue_low);
  /*
   * Replace NULL by \n. It is not needed to have a char array
   * ended with \0 character.
   */
  buf[size] = '\n';
  /* Send size+1 to include the \n character */
  if(*ppos == 0) {
    if(copy_to_user(userbuf, buf, size+1)) {
      pr_info("Failed to return value to user space\n");
      return -EFAULT;
    }
    *ppos+=1;
    return size+1;
  }
  return 0;
}

static const struct file_operations apds9960_fops = {
  .owner = THIS_MODULE,
  .read = apds9960_read_file,
  .write = apds9960_write_file,
};

static int apds9960_probe (struct i2c_client * client)
{
  static int counter = 0;
  struct apds9960_dev * apds9960;
  /* Allocate the private structure */
  apds9960 = devm_kzalloc(&client->dev, sizeof(struct apds9960_dev), GFP_KERNEL);
  /* Store pointer to the device-structure in bus device context */
  i2c_set_clientdata(client,apds9960);
  /* Store pointer to I2C client */
  apds9960->client = client;


  /* Wait mechanism */
  init_waitqueue_head(&apds9960->wq);
  timer_setup(&apds9960->timer, sensor_timer_cb, 0);

  /*
   * Initialize the misc device, apds9960 is incremented
   * after each probe call
   */
  sprintf(apds9960->name, "apds9960%02d", counter++);
  apds9960->apds9960_miscdevice.name = apds9960->name;
  apds9960->apds9960_miscdevice.minor = MISC_DYNAMIC_MINOR;
  apds9960->apds9960_miscdevice.fops = &apds9960_fops;
  /* Register the misc device */
  printk(KERN_ALERT "APDS9960 succesfully probed.");
  return misc_register(&apds9960->apds9960_miscdevice);
  return 0;
}

void apds9960_remove(struct i2c_client * client)
{
  struct apds9960_dev * apds9960;
  /* Get device structure from bus device context */
  apds9960 = i2c_get_clientdata(client);
  dev_info(&client->dev,
      "apds9960_remove is entered on %s\n", apds9960->name);
  /* Deregister misc device */
  misc_deregister(&apds9960->apds9960_miscdevice);
  dev_info(&client->dev,
      "apds9960_remove is exited on %s\n", apds9960->name);
}


static struct i2c_driver apds9960_driver = {
  .driver = {
    .name = "apds9960",
    .owner = THIS_MODULE,
    .of_match_table = apds9960_dt_ids,
  },
  .probe = apds9960_probe,
  .remove = apds9960_remove,
  .id_table = i2c_ids,
};


module_i2c_driver(apds9960_driver);

MODULE_AUTHOR("Christopher Odom");
MODULE_DESCRIPTION("APDS-9960 proximity sensor char device driver");
MODULE_LICENSE("GPL");
