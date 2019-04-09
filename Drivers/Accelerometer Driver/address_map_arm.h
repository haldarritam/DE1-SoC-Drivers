/* Bit values in BW_RATE                                                */
/* Expresed as output data rate */
#define XL345_RATE_3200       0x0f
#define XL345_RATE_1600       0x0e
#define XL345_RATE_800        0x0d
#define XL345_RATE_400        0x0c
#define XL345_RATE_200        0x0b
#define XL345_RATE_100        0x0a
#define XL345_RATE_50         0x09
#define XL345_RATE_25         0x08
#define XL345_RATE_12_5       0x07
#define XL345_RATE_6_25       0x06
#define XL345_RATE_3_125      0x05
#define XL345_RATE_1_563      0x04
#define XL345_RATE__782       0x03
#define XL345_RATE__39        0x02
#define XL345_RATE__195       0x01
#define XL345_RATE__098       0x00

/* Bit values in DATA_FORMAT                                            */

/* Register values read in DATAX0 through DATAZ1 are dependant on the 
   value specified in data format.  Customer code will need to interpret
   the data as desired.                                                 */
#define XL345_RANGE_2G             0x00
#define XL345_RANGE_4G             0x01
#define XL345_RANGE_8G             0x02
#define XL345_RANGE_16G            0x03
#define XL345_DATA_JUST_RIGHT      0x00
#define XL345_DATA_JUST_LEFT       0x04
#define XL345_10BIT                0x00
#define XL345_FULL_RESOLUTION      0x08
#define XL345_INT_LOW              0x20
#define XL345_INT_HIGH             0x00
#define XL345_SPI3WIRE             0x40
#define XL345_SPI4WIRE             0x00
#define XL345_SELFTEST             0x80

/* Bit values in INT_ENABLE, INT_MAP, and INT_SOURCE are identical
   use these bit values to read or write any of these registers.        */
#define XL345_OVERRUN              0x01
#define XL345_WATERMARK            0x02
#define XL345_FREEFALL             0x04
#define XL345_INACTIVITY           0x08
#define XL345_ACTIVITY             0x10
#define XL345_DOUBLETAP            0x20
#define XL345_SINGLETAP            0x40
#define XL345_DATAREADY            0x80

/* Bit values in POWER_CTL                                              */
#define XL345_WAKEUP_8HZ           0x00
#define XL345_WAKEUP_4HZ           0x01
#define XL345_WAKEUP_2HZ           0x02
#define XL345_WAKEUP_1HZ           0x03
#define XL345_SLEEP                0x04
#define XL345_MEASURE              0x08
#define XL345_STANDBY              0x00
#define XL345_AUTO_SLEEP           0x10
#define XL345_ACT_INACT_SERIAL     0x20
#define XL345_ACT_INACT_CONCURRENT 0x00

// ADXL345 Register List
#define ADXL345_REG_DEVID       	0x00
#define ADXL345_REG_POWER_CTL   	0x2D
#define ADXL345_REG_DATA_FORMAT 	0x31
#define ADXL345_REG_FIFO_CTL    	0x38
#define ADXL345_REG_BW_RATE     	0x2C
#define ADXL345_REG_INT_ENABLE  	0x2E  // default value: 0x00
#define ADXL345_REG_INT_MAP     	0x2F  // default value: 0x00
#define ADXL345_REG_INT_SOURCE  	0x30  // default value: 0x02
#define ADXL345_REG_DATAX0      	0x32  // read only
#define ADXL345_REG_DATAX1      	0x33  // read only
#define ADXL345_REG_DATAY0      	0x34  // read only
#define ADXL345_REG_DATAY1      	0x35  // read only
#define ADXL345_REG_DATAZ0      	0x36  // read only
#define ADXL345_REG_DATAZ1      	0x37  // read only
#define ADXL345_REG_OFSX        	0x1E
#define ADXL345_REG_OFSY        	0x1F
#define ADXL345_REG_OFSZ        	0x20
#define ADXL345_REG_THRESH_ACT		0x24
#define ADXL345_REG_THRESH_INACT	0x25
#define ADXL345_REG_TIME_INACT		0x26
#define ADXL345_REG_ACT_INACT_CTL	0x27

#define ADXL345_REG_THRESH_TAP      0x1D
#define ADXL345_REG_DUR             0x21
#define ADXL345_REG_LATENT          0x22
#define ADXL345_REG_WINDOW          0x23
#define ADXL345_TAP_AXES            0x2A

/* Memory */
#define DDR_BASE              0x00000000
#define DDR_SPAN              0x3FFFFFFF
#define A9_ONCHIP_BASE        0xFFFF0000
#define A9_ONCHIP_SPAN        0x0000FFFF
#define SDRAM_BASE            0xC0000000
#define SDRAM_SPAN            0x03FFFFFF
#define FPGA_ONCHIP_BASE      0xC8000000
#define FPGA_ONCHIP_SPAN      0x0003FFFF
#define FPGA_CHAR_BASE        0xC9000000
#define FPGA_CHAR_SPAN        0x00001FFF

/* Cyclone V FPGA devices */
#define LW_BRIDGE_BASE			0xFF200000

#define LEDR_BASE             0x00000000
#define HEX3_HEX0_BASE        0x00000020
#define HEX5_HEX4_BASE        0x00000030
#define SW_BASE               0x00000040
#define KEY_BASE              0x00000050
#define JP1_BASE              0x00000060
#define JP2_BASE              0x00000070
#define PS2_BASE              0x00000100
#define PS2_DUAL_BASE         0x00000108
#define JTAG_UART_BASE        0x00001000
#define JTAG_UART_2_BASE      0x00001008
#define IrDA_BASE             0x00001020
#define TIMER0_BASE           0x00002000
#define TIMER1_BASE           0x00002020
#define AV_CONFIG_BASE        0x00003000
#define PIXEL_BUF_CTRL_BASE   0x00003020
#define CHAR_BUF_CTRL_BASE    0x00003030
#define AUDIO_BASE            0x00003040
#define VIDEO_IN_BASE         0x00003060
#define ADC_BASE              0x00004000

#define LW_BRIDGE_SPAN			0x00005000

/* ARM Peripherals */
#define I2C0_BASE					0xFFC04000		// base
#define I2C0_CON              0x00000000		// word offset
#define I2C0_TAR              0x00000001		// word offset
#define I2C0_DATA_CMD         0x00000004		// word offset
#define I2C0_FS_SCL_HCNT      0x00000007		// word offset
#define I2C0_FS_SCL_LCNT      0x00000008		// word offset
#define I2C0_ENABLE           0x0000001B		// word offset
#define I2C0_RXFLR            0x0000001E		// word offset
#define I2C0_ENABLE_STATUS    0x00000027		// word offset
#define I2C0_SPAN					0x00000100		// span

#define SYSMGR_BASE				0xFFD08000		// base
#define SYSMGR_GENERALIO7     0x00000127		// word offset
/* GENERALIO7 (trace_d6): 
    0 : Pin is connected to GPIO/LoanIO number 55. 
    1 : Pin is connected to Peripheral signal I2C0.SDA. 
    2 : Pin is connected to Peripheral signal SPIS1.SS0. 
    3 : Pin is connected to Peripheral signal TRACE.D6. */
#define SYSMGR_GENERALIO8     0x00000128		// word offset
/* GENERALIO8 (trace_d7): 
    0 : Pin is connected to GPIO/LoanIO number 56. 
    1 : Pin is connected to Peripheral signal I2C0.SCL. 
    2 : Pin is connected to Peripheral signal SPIS1.MISO. 
    3 : Pin is connected to Peripheral signal TRACE.D7. */
#define SYSMGR_I2C0USEFPGA    0x000001C1
/* I2C0USEFPGA: 
    0 : I2C0 uses HPS Pins. 
    1 : I2C0 uses the FPGA Inteface. */
#define SYSMGR_SPAN				0x00000800		// base

// Rounded division macro
#define ROUNDED_DIVISION(n, d) (((n < 0) ^ (d < 0)) ? ((n - d/2)/d) : ((n + d/2)/d))

// ADXL345 Functions
void ADXL345_Init(int, int, int);
void ADXL345_Calibrate(void);
bool ADXL345_IsDataReady(void);
bool ADXL345_WasActivityUpdated(void);
void ADXL345_XYZ_Read(int16_t szData16[3]);
void ADXL345_IdRead(uint8_t *pId);
void ADXL345_REG_READ(uint8_t address, uint8_t *value);
void ADXL345_REG_WRITE(uint8_t address, uint8_t value);
void ADXL345_REG_MULTI_READ(uint8_t address, uint8_t values[], uint8_t len);

// I2C0 Functions
void I2C0_Init(void);

// Pinmux Functions
void Pinmux_Config(void);
