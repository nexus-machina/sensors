#include <linux/module.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

#define APDS9960_MAJOR 42
#define APDS9960_MAX_MINORS 1
#define APDS9960_ADDR 0x39
#define PROX_DATA_REG 0x9C
#define DEVICE_NAME "apds9960"

#define APDS9960_ENABLE 0x80
#define APDS9960_PDATA 0x90 // prox data
#define APDS9960_PILT 0x89
#define APDS9960_PIHT 0x8B
#define APDS9960_STATUS 0x93
#define APDS9960_WAIT_TIME 0x83
#define APDS9960_ADC_TIME 0x82
#define APDS9960_CONTROL_1 0x8f
#define APDS9960_PERS 0x8c
#define APDS9960_CONFIG_THREE 0x9f
#define APDS9960_FIFO_LEVEL 0xAE
#define APDS9960_GPENTH 0xA1
#define APDS9960_GEXTH 0xA1
#define APDS9960_GPULSE_REG 0xA6

// Clear interrupt registers
#define APDS9960_IFORCE  0xE4   // forces an interrupt
#define APDS9960_PICLEAR 0xE5   // prox interrupt clear
#define APDS9960_CICLEAR 0xE6   // als channel clear
#define APDS9960_AICLEAR 0xE7   // all non-gesture interrupts clear

#define APDS9960_GCONF1_REG 0xA2
#define APDS9960_GCONF2_REG 0xA3
#define APDS9960_GCONF3_REG 0xAA
#define APDS9960_GCONF4_REG 0xAB
#define APDS9960_GSTATUS_REG 0xAF
#define APDS9960_GFIFO_U_REG 0xFC

#define APDS9960_GOFFSET_U 0xA4
#define APDS9960_GOFFSET_D 0xA5
#define APDS9960_GOFFSET_L 0xA7
#define APDS9960_GOFFSET_R 0xA9

// TODO Have this be configurable
#define APDS9960_INT_PIN 23

/*
 * Color data is reported using two bytes, one
 * register for the low order bits, the next for
 * the high order bits.
 *
 * Reading low order bits latches the high order bits
 * until the next read, preventing data corruption. The
 * same is done for the clear channel, which will latch
 * all other channels. This means that any read to clear
 * must also be a read of all channels, and any read to
 * one channel must be a complete read. Otherwise, this
 * will lock-up the sensor.
 */
// clear data
#define APDS9960_CDATAL 0x94 // low byte
#define APDS9960_CDATAH 0x95 // high byte
// read data
#define APDS9960_RDATAL 0x96
#define APDS9960_RDATAH 0x97
// green data
#define APDS9960_GDATAL 0x98
#define APDS9960_GDATAH 0x99
// blue data
#define APDS9960_BDATAL 0x9A
#define APDS9960_BDATAH 0x9B

// Enable bitfields 
#define APDS9960_ENABLE_ON   (1)
#define APDS9960_ENABLE_ALS  (1<<1)
#define APDS9960_ENABLE_PROX (1<<2)
#define APDS9960_ENABLE_WAIT (1<<3)
#define APDS9960_ENABLE_ALS_INT  (1<<4)
#define APDS9960_ENABLE_PROX_INT (1<<5)
#define APDS9960_ENABLE_GESTURE  (1<<6)

// Gesture Status bitset
#define APDS9960_GSTATUS_GFOV (1<<1)
#define APDS9960_GSTATUS_GVALID (1)

// Gesture config four bitfields
#define APDS9960_GCONF4_GIEN (1<<1)
#define APDS9960_GCONF4_GMODE (1)

// Status event bits
#define APDS9960_STATUS_AVALID (1)
#define APDS9960_STATUS_PVALID (1<<1)
#define APDS9960_STATUS_GINT (1<<2)
#define APDS9960_STATUS_AINT (1<<4)
#define APDS9960_STATUS_PINT (1<<5)
#define APDS9960_STATUS_PGSTAT (1<<6)
#define APDS9960_STATUS_CPSAT (1<<7)

// Configuration three
#define APDS9960_CONFIG_PCMP_ENABLE (1<<5)

// Struct arguments below
struct apds9960_ctrl_1_cfg {
  unsigned ldrive :2;
  unsigned pgain :2;
  unsigned again :2;
};

struct apds9960_gconf_1_cfg {
  u8 gfifoth :2;
  u8 gexmsk :4;
  u8 gexpers :2;
};

struct apds9960_gconf_2_cfg {
  // u8 _reserved:1;
  u8 ggain :2;
  u8 gldrive :2;
  u8 gwtime :3;
};

struct apds9960_gpulse_cfg {
  u8 gplen :2;
  u8 gpulse :6;
};

/* 
 * State machine to keep track of current readout
 *
 * Driver will only assert one engine per cycle of the states.
 *
 */
enum apds9960_state_t {
  APDS9960_STATE_READY,
  APDS9960_STATE_MOTION,
  APDS9960_STATE_COLOR,
  APDS9960_STATE_PROX,
};


struct apds9960_dev {
  struct i2c_client* client;
  struct miscdevice apds9960_miscdevice;
  struct input_dev *input;
  struct gpio_desc *gpio;
  struct mutex lock;
  enum apds9960_state_t state;
  int irq;
  wait_queue_head_t wq;
  bool data_ready;
  bool avalid;
  bool pvalid;
  u8 gpenth;
  u8 gexth;
  bool udlr_kstate[4];
  struct timer_list timer;
  char name[8]; /* apds9960 */
  struct work_struct gesture_work; // Add workqueue for bottom half
};
