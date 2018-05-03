#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <asm/mach/map.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/sysfs.h>

#define DRV_VERSION "V1.0"
static struct i2c_client *dht12_client;
static struct mutex my_mutex;
static int i2c_dht12_read_len(struct i2c_client *client,unsigned char reg_addr,unsigned char len,unsigned char *buf)
{
	int ret;
        unsigned char txbuf = reg_addr;
        struct i2c_msg msg[] = {
                {client->addr,0,1,&txbuf},
                {client->addr,1,len,buf}
        };
        ret = i2c_transfer(client->adapter,msg,2);
        if(ret < 0) {
                printk("i2c_transfer read len error\n");
                return -1;
        }
        return 0;
}
static int i2c_dht12_read_byte(struct i2c_client *client,unsigned char reg_addr)
{
	int ret;
	unsigned char txbuf = reg_addr;
	unsigned char rxbuf;
	struct i2c_msg msg[] = {
		{client->addr,0,1,&txbuf},
		{client->addr,I2C_M_RD,1,&rxbuf}
	};
	ret = i2c_transfer(client->adapter,msg,2);
	if(ret < 0) {
		printk("i2c_transfer read error\n");
                return -1;
	}
	return rxbuf;
}
static int i2c_dht12_write_byte(struct i2c_client *client,unsigned char reg_addr,unsigned char data_buf)
{
	int ret;
	unsigned char txbuf[] = {reg_addr,data_buf};
	struct i2c_msg msg[] = {client->addr,0,2,txbuf};
	ret = i2c_transfer(client->adapter,msg,1);
	if(ret < 0) {
		printk("i2c_transfer write error\n");
		return -1;
	}
	return 0;
}

static ssize_t dht12_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	int i,checksum;
	unsigned char data_buf[5];
	mutex_lock(&my_mutex);
	data_buf[0] = i2c_smbus_read_byte_data(dht12_client,0x00);
        data_buf[1] = i2c_smbus_read_byte_data(dht12_client,0x01);
	data_buf[2] = i2c_smbus_read_byte_data(dht12_client,0x02);
	data_buf[3] = i2c_smbus_read_byte_data(dht12_client,0x03);
	data_buf[4] = i2c_smbus_read_byte_data(dht12_client,0x04);
	for(i = 0,checksum = 0;i < 4;i++) {
		checksum += data_buf[i];
	}
        mutex_unlock(&my_mutex);
	if(checksum == data_buf[4])
		return sprintf(buf,"Humi = %d.%d %%RH, Temp = %d.%d degree, checksum = %d \n",data_buf[0] , data_buf[1], data_buf[2], data_buf[3], data_buf[4]);
	else
		return sprintf(buf,"checksum error\n");

}
static DEVICE_ATTR(data, S_IRUGO | S_IWUSR, dht12_show, NULL);
static struct attribute *dht12_attrs[] = {
    &dev_attr_data.attr,
    NULL
};
static struct attribute_group mydrv_attr_group = {
    .name = "dht12_drv",
    .attrs = dht12_attrs,
};

static int dht12_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	//struct proc_dir_entry *file;
	int ret;
	dev_dbg(&i2c->dev, "%s\n", __func__);
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL))
                return -ENODEV;

	dev_info(&i2c->dev, "chip found, driver version " DRV_VERSION "\n");
	dht12_client = i2c;
	mutex_init(&my_mutex);
	printk("dht12 device component found!~\n");
	ret = sysfs_create_group(&i2c->dev.kobj, &mydrv_attr_group);
	return 0;
}
static int dht12_remove(struct i2c_client *i2c)
{
	sysfs_remove_group(&i2c->dev.kobj, &mydrv_attr_group);
	return 0;
}

static const struct i2c_device_id dht12_id[] = {  
    { "dht12", 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, dht12_id);

static struct of_device_id dht12_of_match[] = {
        { .compatible = "aosong,dht12" },
        { },
};
//MODULE_DEVICE_TABLE(of, dht12_of_match);
struct i2c_driver dht12_driver = {
    .driver = {
        .name           = "dht12",
        .owner          = THIS_MODULE,
        .of_match_table = of_match_ptr(dht12_of_match),
//	.of_match_table = dht12_of_match,
    },
    .probe      = dht12_probe,
    .remove     = dht12_remove,
    .id_table   = dht12_id,
};

static int dht12_init(void)
{
	return i2c_add_driver(&dht12_driver);
}

static void dht12_exit(void)
{
	printk("exit dht12 driver module");
	i2c_del_driver(&dht12_driver);
}

module_init(dht12_init);
module_exit(dht12_exit);

//module_i2c_driver(dht12_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kevin.Shen");
MODULE_DESCRIPTION("A i2c-dht12 driver for testing module ");
MODULE_VERSION("V1.0");
