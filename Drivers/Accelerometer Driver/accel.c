#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "aux_functions.h"
#include "address_map_arm.h"

#define DEVICE_NAME "accel"
#define MAX_SIZE 21+1
#define SUCCESS 0
#define FORMAT_CMD_PREAMBLE_SIZE 6
#define RATE_CMD_PREAMBLE_SIZE 4
#define PARAMS_FORMAT_EXPECTED 2

// Declare global variables needed to use the accelerometer
volatile int *I2C0_ptr; // virtual address for I2C communication
volatile int *SYSMGR_ptr; // virtual address for system manager communication

int resolution = XL345_10BIT;
int range = XL345_RANGE_16G;
int rate = XL345_RATE_12_5;

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t , loff_t *);
static ssize_t device_write(struct file *, const char *, size_t , loff_t *);

int16_t mg_per_lsb = 31;

// Declare variables needed for a character device driver

static dev_t dev_no = 0;
static struct cdev *cdev = NULL;
static struct class *class = NULL;
static char msg[MAX_SIZE];

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

/* Code to initialize the accelerometer driver */
static int __init start_accel(void)
{
	int err = 0;
	
	// initialize the dev_t, cdev, and class data structures
	/* Get a device number. Get one minor number (0) */
	if ((err = alloc_chrdev_region (&dev_no, 0, 1, DEVICE_NAME)) < 0) {
		printk (KERN_ERR "accel: alloc_chrdev_region() failed with return value %d\n", err);
		return err;
	}

	// Allocate and initialize the character device
	cdev = cdev_alloc (); 
	cdev->ops = &fops; 
	cdev->owner = THIS_MODULE; 
   
	// Add the character device to the kernel
	if ((err = cdev_add (cdev, dev_no, 1)) < 0) {
		printk (KERN_ERR "accel: cdev_add() failed with return value %d\n", err);
		return err;
	}
	
	class = class_create (THIS_MODULE, DEVICE_NAME);
	device_create (class, NULL, dev_no, NULL, DEVICE_NAME );

	// generate a virtual address for the FPGA lightweight bridge
	I2C0_ptr = ioremap_nocache (I2C0_BASE, I2C0_SPAN);
	SYSMGR_ptr = ioremap_nocache (SYSMGR_BASE, SYSMGR_SPAN);
	if ((I2C0_ptr == 0) || (SYSMGR_ptr == 0))
			printk (KERN_ERR "Error: ioremap_nocache returned NULL\n");
		
	Pinmux_Config();
	I2C0_Init();
	if (check_I2C())
		ADXL345_Init(resolution, range, rate);
	else {
		printk (KERN_ERR "Error: I2C communication failed\n");
		return -1;
	}
	return 0;
}

static void __exit stop_accel(void)
{
	/* unmap the physical-to-virtual mappings */
    iounmap (I2C0_ptr);
	iounmap (SYSMGR_ptr);

	/* Remove the device from the kernel */
	device_destroy (class, dev_no);
	cdev_del (cdev);
	class_destroy (class);
	unregister_chrdev_region (dev_no, 1);
}

static int device_open(struct inode *inode, struct file *file)
{
     return SUCCESS;
}
static int device_release(struct inode *inode, struct file *file)
{
     return 0;
}
static ssize_t device_read(struct file *filp, char *buffer,
                           size_t length, loff_t *offset)
{
	size_t bytes;
	format_read_data();
	bytes = strlen (msg) - (*offset);	// how many bytes not yet sent?
	bytes = bytes > length ? length : bytes;	// too much to send all at once?
	
	if (bytes)
		if (copy_to_user (buffer, &msg[*offset], bytes) != 0)
			printk (KERN_ERR "Error: copy_to_user unsuccessful");
	*offset = bytes;	// keep track of number of bytes sent to the user
	
	return bytes;
}


static ssize_t device_write(struct file *filp, const char
                            *buffer, size_t length, loff_t *offset)
{
	size_t bytes = length;
	unsigned long ret = 0;
	
	if (bytes > MAX_SIZE - 1)
		bytes = MAX_SIZE - 1;
	
	ret = copy_from_user (msg, buffer, bytes);
	msg[bytes] = '\0';
	if (msg[bytes-1] == '\n')
		msg[bytes-1] = '\0';
	
	if (!strcmp(msg, "device"))
		printk("DevId: %02x\n", getDeviceId());
	else if (!strcmp(msg, "init"))
	{
		if (check_I2C())
		{
			// In our interpretation, the 'init' command brings the
			// accelerometer back to "factory settings"
			resolution = XL345_10BIT;
			range = XL345_RANGE_16G;
			rate = XL345_RATE_12_5;
			
			calculate_mg_per_lsb();
			ADXL345_Init(resolution, range, rate);
		}
		else 
			printk (KERN_ERR "Error: I2C communication failed\n");
	}
	else if (!strcmp(msg, "calibrate"))
	{
		ADXL345_Calibrate();
	}
	else if (!strncmp(msg, "format", FORMAT_CMD_PREAMBLE_SIZE))
	{
		interpret_format_cmd(msg);
		calculate_mg_per_lsb();
	}
	else if (!strncmp(msg, "rate", RATE_CMD_PREAMBLE_SIZE))
	{
		interpret_rate_cmd(msg);
	}

	return bytes;
}

void interpret_format_cmd(char* msg)
{
	const int param_resolution = 0;
	const int param_range = 1;
	int param[PARAMS_FORMAT_EXPECTED] = {0, 16};
	int error = 0;
	
	char* first_num = strchr(msg, ' ');
	char* second_num;
	first_num = first_num + 1;
	second_num = strchr(first_num, ' ');
	*second_num = '\0';
	second_num = second_num + 1;	
	
	if(kstrtoint(first_num, 10, &param[param_resolution]) != 0)
	{
		printk("ERROR: format command received invalid resolution "
				"Keeping previous setting.\n");
		error++;
	}
	if(kstrtoint(second_num, 10, &param[param_range]) != 0)
	{
		printk("ERROR: format command received invalid range "
				"Keeping previous setting.\n");
		error++;
	}
	
	
	if(!error)
	{
		switch (param[param_resolution])
		{
			case 0:
				resolution = XL345_10BIT;
			break;
			case 1:
				resolution = XL345_FULL_RESOLUTION;
			break;
			default:
				printk("ERROR: format command received invalid resolution. " 
				"Keeping previous setting.\n");
				error++;
		}
		
		switch (param[param_range])
		{
			case 2:
				range = XL345_RANGE_2G;
			break;
			case 4:
				range = XL345_RANGE_4G;
			break;
			case 8:
				range = XL345_RANGE_8G;
			break;
			case 16:
				range = XL345_RANGE_16G;
			break;
			default:
				printk("ERROR: format command received invalid range. " 
				"Keeping previous setting.\n");
				error++;
		}
		
		if(!error) {			
			printk("Settings changed\nResolution: %s\nRange: %ig\n", param[param_resolution] ? "FULL" : "10-bit", param[param_range]);
			ADXL345_Init(resolution, range, rate);
		}
		
	}
}

void interpret_rate_cmd(char* msg)
{
	int param = 25;
	int error = 0;
	
	char* num = strchr(msg, ' ');
	num = num + 1;	
	
	if(kstrtoint(num, 10, &param) != 0)
	{
		printk("ERROR: rate command received invalid parameter "
				"Keeping previous setting.\n");
		error++;
	}	
	
	if(!error)
	{
		switch (param)
		{
			case 25:
				rate = XL345_RATE_25;
			break;
			case 50:
				rate = XL345_RATE_50;
			break;
			case 100:
				rate = XL345_RATE_100;
			break;
			case 200:
				rate = XL345_RATE_200;
			break;
			case 400:
				rate = XL345_RATE_400;
			break;
			case 800:
				rate = XL345_RATE_800;
			break;
			case 1600:
				rate = XL345_RATE_1600;
			break;
			case 3200:
				rate = XL345_RATE_3200;
			break;
			default:
				printk("ERROR: rate command received invalid parameter. " 
				"Keeping previous setting.\n");
				error++;
		}
		
		if(!error) {		
			printk("Settings changed\nRate: %iHz\n", param);
			ADXL345_Init(resolution, range, rate);
		}			
	}
}

void Pinmux_Config(void){
    // Set up pin muxing (in sysmgr) to connect ADXL345 wires to I2C0
    *(SYSMGR_ptr + SYSMGR_I2C0USEFPGA) = 0;
    *(SYSMGR_ptr + SYSMGR_GENERALIO7) = 1;
    *(SYSMGR_ptr + SYSMGR_GENERALIO8) = 1;
}

// Initialize the I2C0 controller for use with the ADXL345 chip
void I2C0_Init(void){

    // Abort any ongoing transmits and disable I2C0.
    *(I2C0_ptr + I2C0_ENABLE) = 2;
    
    // Wait until I2C0 is disabled
    while(((*(I2C0_ptr + I2C0_ENABLE_STATUS))&0x1) == 1){cond_resched();}
    
    // Configure the config reg with the desired setting (act as 
    // a master, use 7bit addressing, fast mode (400kb/s)).
    *(I2C0_ptr + I2C0_CON) = 0x65;
    
    // Set target address (disable special commands, use 7bit addressing)
    *(I2C0_ptr + I2C0_TAR) = 0x53;
    
    // Set SCL high/low counts (Assuming default 100MHZ clock input to I2C0 Controller).
    // The minimum SCL high period is 0.6us, and the minimum SCL low period is 1.3us,
    // However, the combined period must be 2.5us or greater, so add 0.3us to each.
    *(I2C0_ptr + I2C0_FS_SCL_HCNT) = 60 + 30; // 0.6us + 0.3us
    *(I2C0_ptr + I2C0_FS_SCL_LCNT) = 130 + 30; // 1.3us + 0.3us
    
    // Enable the controller
    *(I2C0_ptr + I2C0_ENABLE) = 1;
   
    // Wait until controller is enabled
    while(((*(I2C0_ptr + I2C0_ENABLE_STATUS))&0x1) == 0){cond_resched();}
}
// Write value to internal register at address
void ADXL345_REG_WRITE(uint8_t address, uint8_t value){
    
    // Send reg address (+0x400 to send START signal)
    *(I2C0_ptr + I2C0_DATA_CMD) = address + 0x400;
    
    // Send value
    *(I2C0_ptr + I2C0_DATA_CMD) = value;
}

// Read value from internal register at address
void ADXL345_REG_READ(uint8_t address, uint8_t *value){

    // Send reg address (+0x400 to send START signal)
    *(I2C0_ptr + I2C0_DATA_CMD) = address + 0x400;
    
    // Send read signal
    *(I2C0_ptr + I2C0_DATA_CMD) = 0x100;
    
    // Read the response (first wait until RX buffer contains data)  
    while (*(I2C0_ptr + I2C0_RXFLR) == 0){cond_resched();}
    *value = *(I2C0_ptr + I2C0_DATA_CMD);
}

// Read multiple consecutive internal registers
void ADXL345_REG_MULTI_READ(uint8_t address, uint8_t values[], uint8_t len){
	int i=0;
	int nth_byte=0;

    // Send reg address (+0x400 to send START signal)
    *(I2C0_ptr + I2C0_DATA_CMD) = address + 0x400;
    
    // Send read signal len times
    for (i=0;i<len;i++)
        *(I2C0_ptr + I2C0_DATA_CMD) = 0x100;
	
    // Read the bytes
	i=0;
    while (len){
        if ((*(I2C0_ptr + I2C0_RXFLR)) > 0){
            values[nth_byte] = *(I2C0_ptr + I2C0_DATA_CMD);
            nth_byte++;
            len--;
        }
	cond_resched();
    }
}

// Initialize the ADXL345 chip
void ADXL345_Init(int in_resolution, int in_range, int in_rate){
	
	printk("%s: started\n", __FUNCTION__);

    ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, in_range | in_resolution);
    ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, in_rate);

    // NOTE: The DATA_READY bit is not reliable. It is updated at a much higher rate than the Data Rate
    // Use the Activity and Inactivity interrupts.
    //----- Enabling interrupts -----//
    ADXL345_REG_WRITE(ADXL345_REG_THRESH_ACT, 0x04);	//activity threshold
    ADXL345_REG_WRITE(ADXL345_REG_THRESH_INACT, 0x02);	//inactivity threshold
    ADXL345_REG_WRITE(ADXL345_REG_TIME_INACT, 0x02);	//time for inactivity
    ADXL345_REG_WRITE(ADXL345_REG_ACT_INACT_CTL, 0xFF);	//Enables AC coupling for thresholds
	ADXL345_REG_WRITE(ADXL345_REG_THRESH_TAP, 0x30);    //tap accel. threshold: 3g
    ADXL345_REG_WRITE(ADXL345_REG_DUR, 0x20);           //tap time threshold: 0.02s
	ADXL345_REG_WRITE(ADXL345_REG_LATENT, 0x10);        //wait time for second tap 0.02s
	ADXL345_REG_WRITE(ADXL345_REG_WINDOW, 0xF0);        //time during which a second tap can happen: 0.3s
	ADXL345_REG_WRITE(ADXL345_TAP_AXES, 0x01);          //enable tap on z axis (tap on the plastic cover)
    ADXL345_REG_WRITE(ADXL345_REG_INT_ENABLE, XL345_ACTIVITY | XL345_INACTIVITY |
											  XL345_DOUBLETAP | XL345_SINGLETAP);	//enable interrupts
    //-------------------------------//
    
    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
}

// Calibrate the ADXL345. The DE1-SoC should be placed on a flat
// surface, and must remain stationary for the duration of the calibration.
void ADXL345_Calibrate(void){
	
    int average_x = 0;
    int average_y = 0;
    int average_z = 0;
    int16_t XYZ[3]={0, 0, 0};
    int8_t offset_x=0;
    int8_t offset_y=0;
    int8_t offset_z=0;
	uint8_t saved_bw=0;
	uint8_t saved_dataformat=0;
	int i = 0;
	
	printk("%s: started\n", __FUNCTION__);
    
    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // Get current offsets
    ADXL345_REG_READ(ADXL345_REG_OFSX, (uint8_t *)&offset_x);
    ADXL345_REG_READ(ADXL345_REG_OFSY, (uint8_t *)&offset_y);
    ADXL345_REG_READ(ADXL345_REG_OFSZ, (uint8_t *)&offset_z);
    
    // Use 100 hz rate for calibration. Save the current rate.
    ADXL345_REG_READ(ADXL345_REG_BW_RATE, &saved_bw);
    ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, XL345_RATE_100);
    
    // Use 16g range, full resolution. Save the current format.
    ADXL345_REG_READ(ADXL345_REG_DATA_FORMAT, &saved_dataformat);
    ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, XL345_RANGE_16G | XL345_FULL_RESOLUTION);
    
    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
    
    // Get the average x,y,z accelerations over 32 samples (LSB 3.9 mg)
    while (i < 32){
		// Note: use DATA_READY here, can't use ACTIVITY because board is stationary.
        if (ADXL345_IsDataReady()){
            ADXL345_XYZ_Read(XYZ);
            average_x += XYZ[0];
            average_y += XYZ[1];
            average_z += XYZ[2];
            i++;
        }
    }
    average_x = ROUNDED_DIVISION(average_x, 32);
    average_y = ROUNDED_DIVISION(average_y, 32);
    average_z = ROUNDED_DIVISION(average_z, 32);
    
    // stop measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_STANDBY);
    
    // printf("Average X=%d, Y=%d, Z=%d\n", average_x, average_y, average_z);
    
    // Calculate the offsets (LSB 15.6 mg)
    offset_x += ROUNDED_DIVISION(0-average_x, 4);
    offset_y += ROUNDED_DIVISION(0-average_y, 4);
    offset_z += ROUNDED_DIVISION(256-average_z, 4);
    
    // printf("Calibration: offset_x: %d, offset_y: %d, offset_z: %d (LSB: 15.6 mg)\n",offset_x,offset_y,offset_z);
    
    // Set the offset registers
    ADXL345_REG_WRITE(ADXL345_REG_OFSX, offset_x);
    ADXL345_REG_WRITE(ADXL345_REG_OFSY, offset_y);
    ADXL345_REG_WRITE(ADXL345_REG_OFSZ, offset_z);
    
    // Restore original bw rate
    ADXL345_REG_WRITE(ADXL345_REG_BW_RATE, saved_bw);
    
    // Restore original data format
    ADXL345_REG_WRITE(ADXL345_REG_DATA_FORMAT, saved_dataformat);
    
    // start measure
    ADXL345_REG_WRITE(ADXL345_REG_POWER_CTL, XL345_MEASURE);
}

// Return true if there was activity since the last read (checks ACTIVITY bit).
bool ADXL345_WasActivityUpdated(void){
	bool bReady = false;
    uint8_t data8;
    
    ADXL345_REG_READ(ADXL345_REG_INT_SOURCE,&data8);
    if (data8 & XL345_ACTIVITY)
        bReady = true;
    
    return bReady;
}

// Return true if there is new data (checks DATA_READY bit).
bool ADXL345_IsDataReady(void){
    bool bReady = false;
    uint8_t data8;
    
    ADXL345_REG_READ(ADXL345_REG_INT_SOURCE,&data8);
    if (data8 & XL345_DATAREADY)
        bReady = true;
    
    return bReady;
}

// Read acceleration data of all three axes
void ADXL345_XYZ_Read(int16_t szData16[3]){
    uint8_t szData8[6];
    ADXL345_REG_MULTI_READ(0x32, (uint8_t *)&szData8, sizeof(szData8));

    szData16[0] = (szData8[1] << 8) | szData8[0]; 
    szData16[1] = (szData8[3] << 8) | szData8[2];
    szData16[2] = (szData8[5] << 8) | szData8[4];
}

// Read the ID register
void ADXL345_IdRead(uint8_t *pId){
    ADXL345_REG_READ(ADXL345_REG_DEVID, pId);
}

void format_read_data(void)
{
	int16_t XYZ[3];
	uint8_t int_s = 0;

    
    if (check_I2C()){		
		ADXL345_XYZ_Read(XYZ);
		ADXL345_REG_READ(ADXL345_REG_INT_SOURCE, &int_s);

		sprintf(msg, "%02x %04d %04d %04d %d\n", int_s,
			XYZ[0], XYZ[1], XYZ[2], mg_per_lsb);
    }
}

int check_I2C(void)
{
	uint8_t devid = getDeviceId();
    
    // Correct Device ID
    if (devid == 0xE5)
		return 1;
	return 0;
	
}

int getDeviceId(void)
{
	uint8_t devid;
	// 0xE5 is read from DEVID(0x00) if I2C is functioning correctly
    ADXL345_REG_READ(0x00, &devid);
	
	return devid;
}

void calculate_mg_per_lsb(void)
{
	if (resolution == XL345_FULL_RESOLUTION)
		mg_per_lsb = 4;
	else
		{
			switch(range)
			{
				case XL345_RANGE_2G:
					mg_per_lsb = 4;
				break;
				case XL345_RANGE_4G:
					mg_per_lsb = 8;
				break;
				case XL345_RANGE_8G:
					mg_per_lsb = 16;
				break;
				case XL345_RANGE_16G:
					mg_per_lsb = 31;
				break;
			}
		}
}


MODULE_LICENSE("GPL");
module_init (start_accel);
module_exit (stop_accel);
