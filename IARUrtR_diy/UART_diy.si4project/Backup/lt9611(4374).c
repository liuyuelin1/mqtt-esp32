/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. *
 */


/*************************** 
*driver ic: LT9611 
*version: lt9611_driver_v1.4
*release date: 20180816
1. implement the adjustment of hdmi sync polarity. 
2. implement pattern picture output.
3. Add 1280x1024, 1024x768.

*version: lt9611_driver_v1.5
*release date: 20180823
1. Fixed audio issue.

*version: lt9611_driver_v1.6
*release date: 20180823
1. Fixed hdmi sync polarity issue, 0x831d.
2. Fiexd hdmi sync indicate issue, 0x82d6 = 0x8e.

*version: lt9611_driver_v2.0
*release date: 20190124
1. Fixed mipi EQ issue.
0x8106 = 0x60 ;
0x8107 = 0x3f ;
0x8108 = 0x3f ;
require SOC send EOTP.

*version: lt9611_driver_v2.1
*release date: 20190410
1. fixed HDMI compliance test fail
0x8131 = 0x44 ;
0x8140 = 0x90 ;
0x8141 = 0x90 ;
0x8142 = 0x90 ;
0x8143 = 0x90 ;
2. disable HDCP
****************************/

#include <linux/types.h>
# include <linux/kernel.h>
# include <linux/module.h>
# include <linux/init.h>
# include <linux/device.h>
# include <linux/platform_device.h>
# include <linux/fs.h>
# include <linux/delay.h>
# include <linux/i2c.h>
# include <linux/gpio.h>
# include <linux/interrupt.h>
# include <linux/of_gpio.h>
# include <linux/of_irq.h>
# include <linux/pm.h>
# include <linux/pm_runtime.h>
# include <linux/regulator/consumer.h>

/*******************************************************
   1. LT9611 I2C device address£º
   a) the address is depand on pin32(ADDR), if pin32(ADDR) is 3.3V, the address is 0x3b(7bit).
   	if pin32(ADDR) is 0V, the address is 0x39(7bit).

   2. IIC data rate should less than 100Kbps.

   3. Should reset LT9611 by pull-low(keep more than 30ms) and then pull-high pin33(RSTN) .

   5. Requirment for MIPI source:
   a) MIPI DSI.
   b) Video mode.
   c) non-burst mode.
   d) sync event or puls.
   e) clock should be continuous.
   f) EOTP enanble
   g) disable ssc

 *********************************************************/

#define CFG_HPD_INTERRUPTS BIT(0)
#define CFG_EDID_INTERRUPTS BIT(1)
#define CFG_CEC_INTERRUPTS BIT(2)
#define CFG_VID_CHK_INTERRUPTS BIT(3)

#define EDID_SEG_SIZE   0x100
#define AUDIO_DATA_SIZE 32


typedef enum {
	MIPI_1LANE = 1,
	MIPI_2LANE = 2,
	MIPI_3LANE = 3,
	MIPI_4LANE = 0,
}mipi_lane_counts;


typedef enum {
	MIPI_1PORT = 0x00,
	MIPI_2PORT = 0x03,
}mipi_port_counts;

typedef enum {
	VIDEO_640x480_60HZ = 0,
	VIDEO_720x480_60HZ = 1,
	VIDEO_1280x720_60HZ = 2,
	VIDEO_1920x1080_30HZ = 3,
	VIDEO_1920x1080_60HZ = 4,
	VIDEO_3840x1080_60HZ = 5,
	VIDEO_3840x2160_30HZ = 6,
	VIDEO_1280x1024_60HZ = 7,  //20180816
       VIDEO_1024x768_60HZ = 8     //20180816
}video_format_id;


struct lt9611_reg_cfg {
	u8	reg;
	u8	val;
	int	sleep_in_ms;
};

struct lt9611_video_cfg {
	u32	h_front_porch;
	u32	h_pulse_width;
	u32	h_back_porch;
	u32	h_active;
	u32	v_front_porch;
	u32	v_pulse_width;
	u32	v_back_porch;
	u32	v_active;
	bool	h_polarity;
	bool	v_polarity;
	u32	vic;
	u32	pclk_khz;
	bool	interlaced;
	bool hdmi_mode;
	//enum hdmi_picture_aspect	ar;
	u8	scaninfo;
};

struct lt9611 {
	struct device *	dev;
	u8				i2c_addr;
	int				irq;
	bool				ac_mode;

	u32				irq_gpio;
	u32				reset_gpio;
	u32				hdmi_ps_gpio;
	u32				hdmi_en_gpio;

	void *			edid_data;
	u8				edid_buf[EDID_SEG_SIZE];
	u8				audio_spkr_data[AUDIO_DATA_SIZE];

	mipi_port_counts		mipi_port_counts;
	mipi_lane_counts		mipi_lane_counts;
	video_format_id		video_format_id;

	struct i2c_client *		i2c_client;
	struct mutex			ops_mutex;


	struct lt9611_video_cfg *	video_cfg;
	u8 pcr_m;
};


static struct lt9611_video_cfg video_tab[] = {
	{ 8,    96,  40,	640,  33, 2,  10, 480,0, 0, 0, 25000	},         //video_640x480_60Hz
	{ 16,  62,  60,	720,  9,  6,  30, 480,0, 0, 0, 27000	},         //video_720x480_60Hz
	{ 110, 40, 220, 1280, 5,  5,  20, 720,1, 1, 0, 74250	},         //video_1280x720_60Hz
	{ 88,  44, 148, 1920, 4,  5,  36, 1080, 1, 1, 0, 74250},       //video_1920x1080_30Hz
	{ 88,  44, 148, 1920, 4,  5,  36, 1080, 1, 1, 16, 148500 },   //video_1920x1080_60Hz
	{ 176, 88, 296, 3840, 4,  5,  36, 1080, 1, 1, 0, 297000  },   //video_3840x1080_60Hz
	{ 176, 88, 296, 3840, 8,  10, 72, 2160, 1, 1, 0, 297000 },   //video_3840x2160_30Hz

	{ 48, 112, 248, 1280, 1,  3, 38, 1024, 1, 1, 0, 108000 },     //video_1280x1024_60Hz
       { 24, 136, 160, 1024, 3,  6, 29, 768, 1, 1, 0, 65000 },        //video_1024x768_60Hz
};

static int i2c_write_byte(struct i2c_client *client, u8 addr, u8 reg, u8 val)
{
	int rc = 0;
	struct i2c_msg msg;
	u8 buf[2] = { reg, val };
	if (!client) {
		printk("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	//printk("%s: [%s:0x%02x] : W[0x%02x, 0x%02x]\n", __func__,
	//       client->name, addr, reg, val);

	client->addr = addr;
	msg.addr = addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	if (i2c_transfer(client->adapter, &msg, 1) < 1) {
		printk("%s: i2c write failed\n", __func__);
		rc = -EIO;
	}
	return rc;
}

static int i2c_read(struct i2c_client *client, u8 addr, u8 reg, char *buf, u32 size)
{
	int rc = 0;
	struct i2c_msg msg[2];

	if (!client || !buf) {
		printk("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	client->addr = addr;

	msg[0].addr = addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = size;
	msg[1].buf = buf;

	if (i2c_transfer(client->adapter, msg, 2) != 2) {
		printk("%s: i2c read failed\n", __func__);
		rc = -EIO;
	}

	//printk("%s: [%s:0x%x] : R[0x%02x, 0x%02x]\n", __func__,
	//       client->name, addr, reg, *buf);
	return rc;
}

static int lt9611_read(struct lt9611 *pdata, u8 reg, char *buf, u32 size)
{
	u8 addr = 0;
	int ret = 0;

	if (!pdata) {
		printk("Invalid argument\n");
		return -EINVAL;
	}

	addr = pdata->i2c_addr;
	ret = i2c_read(pdata->i2c_client, addr, reg, buf, size);
	if (ret)
		printk("%s: read err: addr 0x%x, reg 0x%x, size 0x%x\n", addr, reg, size);
	return ret;
}

static int lt9611_write(struct lt9611 *pdata, u8 reg, u8 val)
{
	u8 addr = 0;
	int ret = 0;

	if (!pdata) {
		printk("Invalid argument\n");
		return -EINVAL;
	}

	addr = pdata->i2c_addr;
	ret = i2c_write_byte(pdata->i2c_client, addr, reg, val);
	if (ret)
		printk("%s: wr err: addr 0x%x, reg 0x%x, val 0x%x\n", __func__, addr, reg, val);
	return ret;
}

static int lt9611_write_array(struct lt9611 *pdata, struct lt9611_reg_cfg *cfg, int size)
{
	int ret = 0;
	int i;

	size = size / sizeof(struct lt9611_reg_cfg);
	for (i = 0; i < size; i++) {
		ret = lt9611_write(pdata, cfg[i].reg, cfg[i].val);

		if (ret != 0) {
			printk("lt9611 reg writes failed. Last write %02X to %02X\n",
			       cfg[i].val, cfg[i].reg);
			goto w_regs_fail;
		}

		if (cfg[i].sleep_in_ms)
			msleep(cfg[i].sleep_in_ms);
	}

w_regs_fail:
	if (ret != 0)
		printk("Exiting with ret = %d after %d writes\n", ret, i);

	return ret;
}



static int lt9611_read_device_rev(struct lt9611 *pdata)
{
	u8 rev = 0;
	int ret = 0;


	lt9611_write(pdata, 0xff, 0x80);
	lt9611_write(pdata, 0xee, 0x01);

	ret = lt9611_read(pdata, 0x02, &rev, 1);

	if (ret == 0)
		printk("%s: lt9611 version: 0x%x\n", __func__, rev);

	return ret;
}

static int lt9611_system_init(struct lt9611 *pdata)
{
	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	lt9611_write(pdata, 0xFF, 0x81);
	lt9611_write(pdata, 0x01, 0x18);         //sel xtal clock
	lt9611_write(pdata, 0xFF, 0x82);
	lt9611_write(pdata, 0x51, 0x11);        //
	//Timer for Frequency meter
	lt9611_write(pdata, 0xFF, 0x82);
	lt9611_write(pdata, 0x1b, 0x69);                //Timer 2
	lt9611_write(pdata, 0x1c, 0x78);
	lt9611_write(pdata, 0xcb, 0x69);                //Timer 1
	lt9611_write(pdata, 0xcc, 0x78);

	/*power consumption for work*/
	lt9611_write(pdata, 0xff, 0x80);
	lt9611_write(pdata, 0x04, 0xf0);
	lt9611_write(pdata, 0x06, 0xf0);
	lt9611_write(pdata, 0x0a, 0x80);
	lt9611_write(pdata, 0x0b, 0x40);
	lt9611_write(pdata, 0x0d, 0xef);
	lt9611_write(pdata, 0x11, 0xfa);
	return 0;
}

static int lt9611_low_power_mode(struct lt9611 *pdata, bool on)
{     
       //low power mode: only hpd detection is work to reduce power consumption.
	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	if(on){
		lt9611_write(pdata, 0xFF, 0x81);
		lt9611_write(pdata, 0x02, 0x48);
		lt9611_write(pdata, 0x23, 0x80);
		lt9611_write(pdata, 0x30, 0x00);
		
		lt9611_write(pdata, 0xFF, 0x80);
		lt9611_write(pdata, 0x11, 0x0a);
		}
	else{

		lt9611_write(pdata, 0xFF, 0x81);
		lt9611_write(pdata, 0x02, 0x12);
		lt9611_write(pdata, 0x23, 0x40);
		lt9611_write(pdata, 0x30, 0x6a);
		
		lt9611_write(pdata, 0xFF, 0x80);
		lt9611_write(pdata, 0x11, 0xfa);
		}
	return 0;
}


static int lt9611_mipi_input_analog(struct lt9611 *pdata)
{
	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	//mipi mode
	lt9611_write(pdata, 0xff, 0x81);
	lt9611_write(pdata, 0x06, 0x60);   //port A rx current  //20190124
	lt9611_write(pdata, 0x07, 0x3f);   //20180420 PortB EQ
	lt9611_write(pdata, 0x08, 0x3f);   //20180420 PortB EQ
	lt9611_write(pdata, 0x0a, 0xfe);   //port A ldo voltage set
	lt9611_write(pdata, 0x0b, 0xbf);   //enable port A lprx //20190124
	//lt9611_write(pdata, 0x0c, 0xff);   //lp bandwidth        //20190124
	lt9611_write(pdata, 0x11, 0x60);   //port B rx current   //20190124
	lt9611_write(pdata, 0x12, 0x3f);   //20180420 PortB EQ
	lt9611_write(pdata, 0x13, 0x3f);   //20180420 PortB EQ
	lt9611_write(pdata, 0x15, 0xfe);   //port B ldo voltage set
	lt9611_write(pdata, 0x16, 0xbf);   //enable port B lprx
	//lt9611_write(pdata, 0x17, 0xff);   //lp bandwidth        //20190124

	lt9611_write(pdata, 0x1c, 0x03);        //PortA clk lane no-LP mode.
	lt9611_write(pdata, 0x20, 0x03);        //PortB clk lane no-LP mode.
}

static int lt9611_mipi_input_digtal(struct lt9611 *pdata) //weiguo
{
	u8 lanes;
	u8 ports;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	lanes = pdata->mipi_lane_counts;
	ports = pdata->mipi_port_counts;

	lt9611_write(pdata, 0xff, 0x82);
	lt9611_write(pdata, 0x4f, 0x80);    //[7] = Select ad_txpll_d_clk.
	lt9611_write(pdata, 0x50, 0x10);
	//lt9611_write(pdata, 0x50, 0x14);   //signal port from portB

	lt9611_write(pdata, 0xff, 0x83);
	lt9611_write(pdata, 0x00, (lanes));  //port A lane cnts //20190214
	lt9611_write(pdata, 0x04, (lanes));  //port B lane cnts //20190214
	lt9611_write(pdata, 0x02, 0x0a);      //settle

	//lt9611_write(pdata, 0x03, 0x40);     //signal port from portB
	lt9611_write(pdata, 0x06, 0x0a);     //settle
	lt9611_write(pdata, 0x0a, ports);

	//lt9611_write(pdata, 0x4b, 0x3e); //ecc bypass //20190214
	//lt9611_write(pdata, 0x03, 0x40); //A/B swap
	//lt9611_write(pdata, 0x07, 0x40); //A/B swap

	return 0;
}


static int lt9611_mipi_video_setup(struct lt9611 *pdata)
{
	u32 h_total, h_act, hpw, hfp, hss;
	u32 v_total, v_act, vpw, vfp, vss;
	video_format_id video_id;
	struct lt9611_video_cfg *cfg;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	video_id = pdata->video_format_id;
	cfg = &video_tab[video_id];

	h_total = cfg->h_active + cfg->h_front_porch +
		  cfg->h_pulse_width + cfg->h_back_porch;
	v_total = cfg->v_active + cfg->v_front_porch +
		  cfg->v_pulse_width + cfg->v_back_porch;

	h_act = cfg->h_active;
	hpw = cfg->h_pulse_width;
	hfp = cfg->h_front_porch;
	hss = cfg->h_pulse_width + cfg->h_back_porch;

	v_act = cfg->v_active;
	vpw = cfg->v_pulse_width;
	vfp = cfg->v_front_porch;
	vss = cfg->v_pulse_width + cfg->v_back_porch;

	printk("%s: h_total=%d, h_active=%d, hfp=%d, hpw=%d, hbp=%d\n", __func__,
	       h_total, cfg->h_active, cfg->h_front_porch,
	       cfg->h_pulse_width, cfg->h_back_porch);

	printk("%s: v_total=%d, v_active=%d, vfp=%d, vpw=%d, vbp=%d\n", __func__,
	       v_total, cfg->v_active, cfg->v_front_porch,
	       cfg->v_pulse_width, cfg->v_back_porch);

	lt9611_write(pdata, 0xff, 0x83);

	lt9611_write(pdata, 0x0d, (u8)(v_total / 256));
	lt9611_write(pdata, 0x0e, (u8)(v_total % 256));

	lt9611_write(pdata, 0x0f, (u8)(v_act / 256));
	lt9611_write(pdata, 0x10, (u8)(v_act % 256));

	lt9611_write(pdata, 0x11, (u8)(h_total / 256));
	lt9611_write(pdata, 0x12, (u8)(h_total % 256));

	lt9611_write(pdata, 0x13, (u8)(h_act / 256));
	lt9611_write(pdata, 0x14, (u8)(h_act % 256));

	lt9611_write(pdata, 0x15, (u8)(vpw % 256));
	lt9611_write(pdata, 0x16, (u8)(hpw % 256));

	lt9611_write(pdata, 0x17, (u8)(vfp % 256));

	lt9611_write(pdata, 0x18, (u8)(vss % 256));

	lt9611_write(pdata, 0x19, (u8)(hfp % 256));

	lt9611_write(pdata, 0x1a, (u8)(hss / 256));
	lt9611_write(pdata, 0x1b, (u8)(hss % 256));

	return 0;
}


static int lt9611_pll_setup(struct lt9611 *pdata)
{
	u32 pclk;
	u8 pcr_m;
	u8 hdmi_post_div;
	u8 pll_lock_flag;
	u8 i;
	u8 format_id;
	int ret;
	struct lt9611_video_cfg *cfg;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	format_id = pdata->video_format_id;
	cfg = &video_tab[format_id];
	pclk = cfg->pclk_khz;
	printk("%s: set rx pll = %d\n", __func__, pclk);

	lt9611_write(pdata, 0xff, 0x81);
	lt9611_write(pdata, 0x23, 0x40);
	lt9611_write(pdata, 0x24, 0x64);
	lt9611_write(pdata, 0x25, 0x80); //pre-divider
	lt9611_write(pdata, 0x26, 0x55);
	lt9611_write(pdata, 0x2c, 0x37);
	//lt9611_write(pdata, 0x2d, 0x99); //txpll_divx_set&da_txpll_freq_set
	//lt9611_write(pdata, 0x2e, 0x01);
	lt9611_write(pdata, 0x2f, 0x01);
	lt9611_write(pdata, 0x26, 0x55);
	lt9611_write(pdata, 0x27, 0x66);
	lt9611_write(pdata, 0x28, 0x88);

	if (pclk > 150000){
		lt9611_write(pdata, 0x2d, 0x88);
		hdmi_post_div = 0x01;
		}
	else if (pclk > 80000){                          //20181221
		lt9611_write(pdata, 0x2d, 0x99);
		hdmi_post_div = 0x02;
		}
	else{
		lt9611_write(pdata, 0x2d, 0xaa);
		hdmi_post_div = 0x04;
		}
	
	pcr_m = (u8)((pclk*5*hdmi_post_div)/27000);
	lt9611_write(pdata, 0xff, 0x83); //M up limit
	lt9611_write(pdata, 0x2d, 0x40); //M down limit
	lt9611_write(pdata, 0x31, 0x08);
	lt9611_write(pdata, 0x26, 0x80|pcr_m);
       pdata->pcr_m = pcr_m;
	printk("%s: pcr_m = 0x%x\n", __func__,pcr_m);
	   
	pclk = pclk / 2;
	lt9611_write(pdata, 0xff, 0x82);             //13.5M
	lt9611_write(pdata, 0xe3, pclk / 65536);
	pclk = pclk % 65536;
	lt9611_write(pdata, 0xe4, pclk / 256);
	lt9611_write(pdata, 0xe5, pclk % 256);

	lt9611_write(pdata, 0xde, 0x20);
	lt9611_write(pdata, 0xde, 0xe0);

	lt9611_write(pdata, 0xff, 0x80);
	lt9611_write(pdata, 0x11, 0x5a);                /* Pcr clk reset */
	lt9611_write(pdata, 0x11, 0xfa);
	lt9611_write(pdata, 0x18, 0xdc);                /* pll analog reset */
	lt9611_write(pdata, 0x18, 0xfc);
	lt9611_write(pdata, 0x16, 0xf1);
	lt9611_write(pdata, 0x16, 0xf3);

	/* pll lock status */
	for (i = 0; i < 6; i++) {
		lt9611_write(pdata, 0xff, 0x80);
		lt9611_write(pdata, 0x16, 0xe3);         /* pll lock logic reset */
		lt9611_write(pdata, 0x16, 0xf3);
		lt9611_write(pdata, 0xff, 0x82);
		msleep(5);
		ret = lt9611_read(pdata, 0x15, &pll_lock_flag, 1);
		if (pll_lock_flag & 0x80) {
			printk("%s: HDMI pll locked\n", __func__);
			break;
		} else {
			lt9611_write(pdata, 0xff, 0x80);
			lt9611_write(pdata, 0x11, 0x5a);                /* Pcr clk reset */
			lt9611_write(pdata, 0x11, 0xfa);
			lt9611_write(pdata, 0x18, 0xdc);                /* pll analog reset */
			lt9611_write(pdata, 0x18, 0xfc);
			lt9611_write(pdata, 0x16, 0xf1);                /* pll cal reset*/
			lt9611_write(pdata, 0x16, 0xf3);
			printk("%s: HDMI pll unlocked, reset pll\n", __func__);
		}
	}
	return 0;
}
static int lt9611_pcr_setup(struct lt9611 *pdata)
{
	video_format_id format_id = pdata->video_format_id;

	u8 format_id, pol;
	bool h_polarity, v_polarity;
	struct lt9611_video_cfg *cfg;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	format_id = (u8)(pdata->video_format_id);
	cfg = &video_tab[format_id];
	h_polarity = cfg->h_polarity;
	v_polarity = cfg->v_polarity;

	pol = h_polarity * 2 + v_polarity;
	pol = ~pol;
	pol &= 0x03;

	lt9611_write(pdata, 0xff, 0x83);
	lt9611_write(pdata, 0x0b, 0x01);        //vsync mode
	lt9611_write(pdata, 0x0c, 0x10);        //=1/4 hact

	lt9611_write(pdata, 0x48, 0x00);        //de mode delay
	lt9611_write(pdata, 0x49, 0x81);        //=1/4 hact

	/* stage 1 */
	lt9611_write(pdata, 0x21, 0x4a); //bit[3:0] step[11:8]
	//lt9611_write(pdata, 0x22, 0x40);//step[7:0]

	lt9611_write(pdata, 0x24, 0x31);        //bit[7:4]v/h/de mode; line for clk stb[11:8]
	lt9611_write(pdata, 0x25, 0x30);        //line for clk stb[7:0]

	lt9611_write(pdata, 0x2a, 0x01);        //clk stable in

	/* stage 2 */
	lt9611_write(pdata, 0x4a, 0x40);        //offset //0x10
	lt9611_write(pdata, 0x1d, (0x10|pol));   //PCR de mode step setting.

	/* MK limit */
	//lt9611_write(pdata, 0x2d, 0x38);        //M up limit
	//lt9611_write(pdata, 0x31, 0x08);        //M down limit

	switch (video_format_id) {
	case VIDEO_3840x1080_60HZ:
	case VIDEO_3840x2160_30HZ:                                            

		lt9611_write(pdata, 0x0b, 0x03);        //vsync mode   0x02
		lt9611_write(pdata, 0x0c, 0xd0);        //=1/4 hact     0x70

		lt9611_write(pdata, 0x48, 0x03);        //de mode delay 0x02
		lt9611_write(pdata, 0x49, 0xe0);        //                      0x80

		lt9611_write(pdata, 0x24, 0x72);        //bit[7:4]v/h/de mode; line for clk stb[11:8]
		lt9611_write(pdata, 0x25, 0x00);        //line for clk stb[7:0]

		lt9611_write(pdata, 0x2a, 0x01);        //clk stable in

		lt9611_write(pdata, 0x4a, 0x10);        //offset
		lt9611_write(pdata, 0x1d, 0x10);        //PCR de mode step setting.
		break;

	case VIDEO_1920x1080_60HZ:
	case VIDEO_1920x1080_30HZ:
	case VIDEO_1280x720_60HZ:
	case VIDEO_720x480_60HZ:
	case VIDEO_640x480_60HZ:
	break;

	default: break;
	}

	return 0;
}


static int lt9611_pcr_start(struct lt9611 *pdata)
{
    u8 pcr_m;
	   
	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pcr_m = pdata->pcr_m;
	lt9611_write(pdata, 0xff, 0x83); 
	lt9611_write(pdata, 0x26, pcr_m); 
	printk("%s: pcr_m = 0x%x\n", __func__,pcr_m);

	lt9611_write(pdata, 0xff, 0x80);  //Pcr reset
	lt9611_write(pdata, 0x11, 0x5a);         
	lt9611_write(pdata, 0x11, 0xfa);
	return 0;
}

static int lt9611_audio_setup(struct lt9611 *pdata)
{
	int ret = -EINVAL;
	struct lt9611_video_cfg *cfg;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

//	lt9611_write(pdata, 0xff, 0x82);
//	lt9611_write(pdata, 0xd6, 0x8e); //20180915
//	lt9611_write(pdata, 0xd7, 0x04); 

       //i2s 2channel
	lt9611_write(pdata, 0xff,0x84);
	lt9611_write(pdata, 0x06,0x08);
	lt9611_write(pdata, 0x07,0x10);
	lt9611_write(pdata, 0x0f,0x2b); //20180823
       lt9611_write(pdata, 0x34,0xd5); //CTS_N 20180823 0xd5: sclk = 32fs, 0xd4: sclk = 64fs

	return 0;
}


static int lt9611_audio_on(struct lt9611 *pdata, bool on)
{
	int ret = -EINVAL;
	struct lt9611_video_cfg *cfg;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if(on){
	lt9611_write(pdata, 0xff,0x84);  
	lt9611_write(pdata, 0x07,0x10); //enable i2s input
		}
	else{
	lt9611_write(pdata, 0xff,0x84); 
	lt9611_write(pdata, 0x07,0x00); //disable i2s input
		}
	return 0;
}


static int lt9611_hdmi_tx_digital(struct lt9611 *pdata)
{
	int ret = -EINVAL;
	video_format_id video_id;
	struct lt9611_video_cfg *cfg;
	u32 checksum, vic;
	bool h_polarity, v_polarity; 

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	video_id = pdata->video_format_id;
	
	printk("%s: video_id: %d\n", __func__,video_id);
	
	cfg = &video_tab[video_id];
	vic = cfg->vic;


	printk("%s: vic: %d\n", __func__,vic);
	checksum = 0x46 - vic;

	lt9611_write(pdata, 0xff, 0x84);
	lt9611_write(pdata, 0x43, checksum);
	lt9611_write(pdata, 0x44, 0x10); //20180420
	lt9611_write(pdata, 0x47, vic);

	lt9611_write(pdata, 0x10, 0x02);  //20180823 start send data iland
	lt9611_write(pdata, 0x12, 0x6a);  //20180823 act Hblank for send data iland.

       //hdmi mode(default is dvi mode)
	lt9611_write(pdata, 0xff, 0x82);  //20180915
	lt9611_write(pdata, 0xd6, 0x8e);
	lt9611_write(pdata, 0xd7, 0x04);

	return 0;
}

static int lt9611_hdmi_tx_phy(struct lt9611 *pdata)
{
	int ret = -EINVAL;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	lt9611_write(pdata, 0xff, 0x81);
	lt9611_write(pdata, 0x30, 0x6a);
	lt9611_write(pdata, 0x31, 0x44);         //DC: 0x44, AC:0x73
	lt9611_write(pdata, 0x32, 0x4a);
	lt9611_write(pdata, 0x33, 0x0b);
	lt9611_write(pdata, 0x34, 0x00);
	lt9611_write(pdata, 0x35, 0x00);
	lt9611_write(pdata, 0x36, 0x00);
	lt9611_write(pdata, 0x37, 0x44);
	lt9611_write(pdata, 0x3f, 0x0f);
	lt9611_write(pdata, 0x40, 0x90); //0x98
	lt9611_write(pdata, 0x41, 0x90); //0x98
	lt9611_write(pdata, 0x42, 0x90); //0x98
	lt9611_write(pdata, 0x43, 0x90); //0x98
	lt9611_write(pdata, 0x44, 0x0a);
	return 0;
}

static void lt9611_hdmi_output_enable(struct lt9611 *pdata)
{
	lt9611_write(pdata, 0xff, 0x81);
	lt9611_write(pdata, 0x30, 0xea);
}


static void lt9611_gpio(struct lt9611 *pdata)
{
	lt9611_write(pdata, 0xff, 0x81);
	lt9611_write(pdata, 0x49, 0x8a);
	lt9611_write(pdata, 0x56, 0x90); //gpio7 = low
	lt9611_write(pdata, 0x56, 0xd0); //gpio7 = high
}


static void lt9611_hdmi_output_disable(struct lt9611 *pdata)
{
	lt9611_write(pdata, 0xff, 0x81);
	lt9611_write(pdata, 0x30, 0x6a);
}


static int lt9611_read_edid(struct lt9611 *pdata)
{
	int ret = 0;
	u8 i, j, temp;
	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	memset(pdata->edid_buf, 0, EDID_SEG_SIZE);

	lt9611_write(pdata, 0xff, 0x85);
	lt9611_write(pdata, 0x03, 0xc9);
	lt9611_write(pdata, 0x04, 0xa0);        /* 0xA0 is EDID device address */
	lt9611_write(pdata, 0x05, 0x00);        /* 0x00 is EDID offset address */
	lt9611_write(pdata, 0x06, 0x20);        /* length for read */
	lt9611_write(pdata, 0x14, 0x7f);

	for (i = 0; i < 8; i++) {
		lt9611_write(pdata, 0x05, i * 32); /* offset address */
		lt9611_write(pdata, 0x07, 0x36);
		lt9611_write(pdata, 0x07, 0x31);
		lt9611_write(pdata, 0x07, 0x37);
		mdelay(5);   /* wait 5ms before reading edid */

		lt9611_read(pdata, 0x40, &temp, 1);
		if (temp & 0x02) {                  /*KEY_DDC_ACCS_DONE=1*/
			if (temp & 0x50) {           /* DDC No Ack or Abitration lost */
				printk("%s: read edid failed: no ack\n",__func__);
				ret = -EINVAL;
				goto end;
			} else {
				for (j = 0; j < 32; j++)
					lt9611_read(pdata, 0x83,
						    &(pdata->edid_buf[i * 32 + j]), 1);
			}
		} else {
			printk("%s: read edid failed: access not done\n", __func__);
			ret = -EINVAL;
			goto end;
		}
	}

	printk("%s: read edid succeeded, checksum = 0x%x\n", __func__, 
				pdata->edid_buf[255]);
end:
	lt9611_write(pdata, 0x07, 0x1f);
	return ret;
}

static int lt9611_enable_interrupts(struct lt9611 *pdata, bool on)
{
	int ret = 0;
	u8 reg_val, init_reg_val;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	if(on){
		printk("%s: interrupt enabled\n", __func__);
		lt9611_write(pdata, 0xff, 0x82);
		lt9611_write(pdata, 0x58, 0x0a); //Det hpd
		lt9611_write(pdata, 0x59, 0x80); //HPD debounce width

		lt9611_write(pdata, 0x9e, 0xf7); //vid chk clk

		lt9611_write(pdata, 0x00, 0xfe); //msk0:  vid_chk_IRQ
		lt9611_write(pdata, 0x03, 0x3f); //msk3:  Tx_det

		lt9611_write(pdata, 0x04, 0xff); //clr0
		lt9611_write(pdata, 0x04, 0xfe);

		lt9611_write(pdata, 0x07, 0xff);//clr3
		lt9611_write(pdata, 0x07, 0x3f);
	}else{
		printk("%s: interrupt disabled\n", __func__);
		lt9611_write(pdata, 0xff, 0x82);
		lt9611_write(pdata, 0x00, 0xff); //msk0:  vid_chk_IRQ
		lt9611_write(pdata, 0x03, 0xff); //msk3:  Tx_det

		lt9611_write(pdata, 0x04, 0xff); //clr0
		lt9611_write(pdata, 0x07, 0xff); //clr3
	}

end:
	return ret;
}


static int lt9611_check_hpd(void *client)
{
  struct lt9611 *pdata = client;
	u8 reg_val = 0;
	int connected = 0
	
	lt9611_write(pdata, 0xff, 0x82);
	lt9611_read(pdata, 0x5e, &reg_val, 1);
	pr_err("0x825e =0x%x\n",reg_val);
	connected  = (reg_val & BIT(2));
	if (connected) {
      pr_err("cable is connected\n");
		lt9611_read_edid(pdata);
	}
return connected;
}



static int lt9611_hpd_Interrupt_Handle(struct lt9611 *pdata)
{
	bool connected;
	
     if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	lt9611_write(pdata, 0xff, 0x82); /* irq 3 clear flag */
	lt9611_write(pdata, 0x07, 0xff);
	lt9611_write(pdata, 0x07, 0x3f);

	connected = lt9611_check_hpd(pdata);

	if(connected){
	lt9611_low_power_mode(pdata, 0);
	lt9611_read_edid(pdata);

	lt9611_mipi_video_setup(pdata);
	lt9611_pll_setup(pdata);
	lt9611_pcr_setup(pdata);
	lt9611_pcr_start(pdata);
	lt9611_audio_setup(pdata);
	lt9611_hdmi_tx_digital(pdata);
	lt9611_hdmi_tx_phy(pdata);
	lt9611_hdmi_output_enable(pdata);

	printk("%s: lt9611 hdmi connected\n",__func__);
	}
	else{

	lt9611_low_power_mode(pdata, 1);
	printk("%s: lt9611 hdmi disconnected\n",__func__);
	}
}

static irqreturn_t lt9611_irq_thread_handler(int irq, struct lt9611 *pdata)
{
	/* TODO */
	u8 irq_flag0, irq_flag1, irq_flag3 ; 
	u8 hdmi_cec_status = 0xFF;
	bool connected;
	
	lt9611_write(pdata, 0xff, 0x82);
	lt9611_read(pdata, 0x0f, &irq_flag3, 1);
	lt9611_read(pdata, 0x0d, &irq_flag1, 1);
	lt9611_read(pdata, 0x0c, &irq_flag0, 1);
	
	/* hpd changed */
	if(irq_flag3 & 0xc0){
	printk("%s: hpd changed\n",__func__);
	lt9611_hpd_Interrupt_Handle(pdata);
	}
	
	/* video input changed */
	if(irq_flag0 & 0x01){
	lt9611_write(pdata, 0xff, 0x82); /* irq 3 clear flag */
	lt9611_write(pdata, 0x9e, 0xff);
	lt9611_write(pdata, 0x9e, 0xf7);	
	lt9611_write(pdata, 0x04, 0xff);
	lt9611_write(pdata, 0x04, 0xfe);
	printk("%s: video input changed\n",__func__);
	}
	
	/* cec irq */
	if(irq_flag1 & 0x80){
	lt9611_write(pdata, 0xff, 0x86);
	lt9611_read(pdata, 0xd2, &hdmi_cec_status, 1);
	printk("%s: cec irq event\n", __func__);
	}
	
	return IRQ_HANDLED;
}

static void lt9611_pattern_enable(struct lt9611 *pdata)
{
	lt9611_write(pdata, 0xff, 0x82);
	lt9611_write(pdata, 0x4f, 0x80);
	lt9611_write(pdata, 0x50, 0x20);
}

static void lt9611_pattern_gcm(struct lt9611 *pdata)
{
	u32 h_total, h_act, hpw, hfp, hsa, hss;
	u32 v_total, v_act, vpw, vfp, vsa, vss;
	video_format_id video_id;
	struct lt9611_video_cfg *cfg;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	video_id = pdata->video_format_id;
	cfg = &video_tab[video_id];

	h_total = cfg->h_active + cfg->h_front_porch +
		  cfg->h_pulse_width + cfg->h_back_porch;
	v_total = cfg->v_active + cfg->v_front_porch +
		  cfg->v_pulse_width + cfg->v_back_porch;

	h_act = cfg->h_active;
	hpw = cfg->h_pulse_width;
	hfp = cfg->h_front_porch;
	hsa = cfg->h_pulse_width
	hss = cfg->h_pulse_width + cfg->h_back_porch;

	v_act = cfg->v_active;
	vpw = cfg->v_pulse_width;
	vfp = cfg->v_front_porch;
	vsa = cfg->v_pulse_width;
	vss = cfg->v_pulse_width + cfg->v_back_porch;

	lt9611_write(pdata, 0xff, 0x82);
	
	lt9611_write(pdata, 0xa3, (u8)(hss / 256));
	lt9611_write(pdata, 0xa4, (u8)(hss % 256));
	lt9611_write(pdata, 0xa5, (u8)(vss % 256));
	lt9611_write(pdata, 0xa6, (u8)(h_act / 256));
	lt9611_write(pdata, 0xa7, (u8)(h_act % 256));
	lt9611_write(pdata, 0xa8, (u8)(v_act / 256));
	lt9611_write(pdata, 0xa9, (u8)(v_act % 256));
	lt9611_write(pdata, 0xaa, (u8)(h_total / 256));
	lt9611_write(pdata, 0xab, (u8)(h_total % 256));
	lt9611_write(pdata, 0xac, (u8)(v_total / 256));
	lt9611_write(pdata, 0xad, (u8)(v_total % 256));
	lt9611_write(pdata, 0xae, (u8)(hsa / 256));
	lt9611_write(pdata, 0xaf, (u8)(hsa % 256));
	lt9611_write(pdata, 0xb0, (u8)(vsa % 256));
}


#if 0
static void lt9611_dump_edid(struct lt9611 *pdata)
{
	char buf[600] = "";
	char str[] = "00";
	int i;

	for (i = 0; i <= EDID_SEG_SIZE; i++) {
		sprintf(str, "%.2x", pdata->edid_buf[i]);
		strcat(buf, str);
	}

	pr_info("raw edid : %s \n", buf);
}



static irqreturn_t lt9611_irq_thread_handler(int irq, void *dev_id)
{
	struct lt9611 *pdata = dev_id;
	u8 irq_flag0, irq_flag3;

	lt9611_write(pdata, 0xff, 0x82);
	lt9611_read(pdata, 0x0f, &irq_flag3, 1);
	lt9611_read(pdata, 0x0f, &irq_flag0, 1);  //0x0f -->0x0c

	/* hpd changed low */
	if (irq_flag3 & 0x80) {
		pr_info("hdmi cable disconnected\n");

		lt9611_write(pdata, 0xff, 0x82); /* irq 3 clear flag */
		lt9611_write(pdata, 0x07, 0xbf);
		lt9611_write(pdata, 0x07, 0x3f);
	}
	/* hpd changed high */
	if (irq_flag3 & 0x40) {
		pr_info("hdmi cable connected\n");
		lt9611_read_edid(pdata);

		lt9611_write(pdata, 0xff, 0x82); /* irq 3 clear flag */
		lt9611_write(pdata, 0x07, 0x7f);
		lt9611_write(pdata, 0x07, 0x3f);
	}

	/* video input changed */
	if (irq_flag0 & 0x01) {
		pr_info("video input changed\n");
		lt9611_write(pdata, 0xff, 0x82); /* irq 0 clear flag */
		lt9611_write(pdata, 0x9e, 0xff);
		lt9611_write(pdata, 0x9e, 0xf7);
		lt9611_write(pdata, 0x04, 0xff);
		lt9611_write(pdata, 0x04, 0xfe);
	}

	return IRQ_HANDLED;
}

static int lt9611_enable_interrupts(struct lt9611 *pdata, int interrupts)
{
	int ret = 0;
	u8 reg_val, init_reg_val;

	if (!pdata) {
		pr_err("invalid input\n");
		goto end;
	}

	if (interrupts & CFG_VID_CHK_INTERRUPTS) {
		lt9611_write(pdata, 0xff, 0x82);
		lt9611_read(pdata, 0x00, &reg_val, 1);

		if (reg_val & 0x01) { //reg_val | 0x01???
			init_reg_val = reg_val & 0xfe;
			pr_debug("enabling video check interrupts\n");
			lt9611_write(pdata, 0x00, init_reg_val);
		}
		lt9611_write(pdata, 0x04, 0xff); /* clear */
		lt9611_write(pdata, 0x04, 0xfe);
	}

	if (interrupts & CFG_HPD_INTERRUPTS) {
		lt9611_write(pdata, 0xff, 0x82);
		lt9611_read(pdata, 0x03, &reg_val, 1);

		if (reg_val & 0xc0) { //reg_val | 0xc0???
			init_reg_val = reg_val & 0x3f;
			pr_debug("enabling hpd interrupts\n");
			lt9611_write(pdata, 0x03, init_reg_val);
		}

		lt9611_write(pdata, 0x07, 0xff); //clear
		lt9611_write(pdata, 0x07, 0x3f);
	}

end:
	return ret;
}
#endif

#if 0
static int lt9611_power_on(struct lt9611 *pdata, bool on)
{
	int ret = 0;

	pr_debug("power_on: on=%d\n", on);

	if (on && !pdata->power_on) {
		lt9611_write_array(pdata, lt9611_init_setup,
				   sizeof(lt9611_init_setup));
		if (gpio_is_valid(pdata->irq_gpio)) {
            ret = lt9611_enable_interrupts(pdata, CFG_HPD_INTERRUPTS);
            if (ret) {
                pr_err("Failed to enable HPD intr %d\n", ret);
                return ret;
            }
        }
		pdata->power_on = true;
	} else if (!on) {
		lt9611_write(pdata, 0xff, 0x81);
		lt9611_write(pdata, 0x30, 0x6a);

		pdata->power_on = false;
	}

	return ret;
}

static int lt9611_video_on(struct lt9611 *pdata, bool on)
{
	int ret = 0;
	struct lt9611_video_cfg *cfg = &pdata->video_cfg;

	pr_debug("on=%d\n", on);

	if (on) {
		lt9611_mipi_input_analog(pdata, cfg);
		lt9611_mipi_input_digital(pdata, cfg);
		lt9611_pll_setup(pdata, cfg);
		lt9611_mipi_video_setup(pdata, cfg);
		lt9611_pcr_setup(pdata, cfg);
		lt9611_hdmi_tx_digital(pdata, cfg);
		lt9611_hdmi_tx_phy(pdata, cfg);

		msleep(500);

		lt9611_video_check(pdata);
		lt9611_hdmi_output_enable(pdata);
	} else {
		lt9611_hdmi_output_disable(pdata);
	}
	return ret;
}
#endif

static int lt9611_video_check_debug(struct lt9611 *pdata)
{
	int ret = 0;
	u32 v_total, v_act, h_act_a, h_act_b, h_total_sysclk;
	u8 val;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/* top module video check */
	lt9611_write(pdata, 0xff, 0x82);

	/* v_act */
	ret = lt9611_read(pdata, 0x82, &val, 1);
	if (ret)
		goto end;

	v_act = val << 8;
	ret = lt9611_read(pdata, 0x83, &val, 1);
	if (ret)
		goto end;
	v_act = v_act + val;

	/* v_total */
	ret = lt9611_read(pdata, 0x6c, &val, 1);
	if (ret)
		goto end;
	v_total = val << 8;
	ret = lt9611_read(pdata, 0x6d, &val, 1);
	if (ret)
		goto end;
	v_total = v_total + val;

	/* h_total_sysclk */
	ret = lt9611_read(pdata, 0x86, &val, 1);
	if (ret)
		goto end;
	h_total_sysclk = val << 8;
	ret = lt9611_read(pdata, 0x87, &val, 1);
	if (ret)
		goto end;
	h_total_sysclk = h_total_sysclk + val;

	/* h_act_a */
	lt9611_write(pdata, 0xff, 0x83);
	ret = lt9611_read(pdata, 0x82, &val, 1);
	if (ret)
		goto end;
	h_act_a = val << 8;
	ret = lt9611_read(pdata, 0x83, &val, 1);
	if (ret)
		goto end;
	h_act_a = (h_act_a + val) / 3;

	/* h_act_b */
	lt9611_write(pdata, 0xff, 0x83);
	ret = lt9611_read(pdata, 0x86, &val, 1);
	if (ret)
		goto end;
	h_act_b = val << 8;
	ret = lt9611_read(pdata, 0x87, &val, 1);
	if (ret)
		goto end;
	h_act_b = (h_act_b + val) / 3;

	printk("%s: video check: h_act_a=%d, h_act_b=%d, v_act=%d, v_total=%d, h_total_sysclk=%d\n",
	       __func__, h_act_a, h_act_b, v_act, v_total, h_total_sysclk);

	return 0;

end:
	printk("%s: read video check error\n", __func__);
	return ret;
}

static int lt9611_pcr_mk_debug(struct lt9611 *pdata)
{
	u8 m, k1, k2, k3;
	u8 reg_8397h;
	u8 i;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	
	for (i = 0; i < 30; i++) {
		lt9611_write(pdata, 0xff, 0x83);
		lt9611_read(pdata, 0x97, &reg_8397h, 1);
		lt9611_read(pdata, 0xb4, &m, 1);
		lt9611_read(pdata, 0xb5, &k1, 1);
		lt9611_read(pdata, 0xb6, &k2, 1);
		lt9611_read(pdata, 0xb7, &k3, 1);

		printk("%s: 0x8397 = 0x%x; pcr mk:0x%x 0x%x 0x%x 0x%x\n", __func__,
		       reg_8397h, m, k1, k2, k3);
	}
	return 0;
}

static int lt9611_htotal_stable_debug(struct lt9611 *pdata)
{
	int ret;
	u8 val;
	u8 i;
	u32 h_total_sysclk;

	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	lt9611_write(pdata, 0xff, 0x82);

	for (i = 0; i < 30; i++) {
		/* h_total_sysclk */
		ret = lt9611_read(pdata, 0x86, &val, 1);
		if (ret)
			goto end;
		h_total_sysclk = val << 8;
		ret = lt9611_read(pdata, 0x87, &val, 1);
		if (ret)
			goto end;
		h_total_sysclk = h_total_sysclk + val;

		printk("%s: h_total_sysclk = %d\n", __func__, h_total_sysclk);
	}
	return 0;
end:
	printk("%s: read htotal by system clock error\n", __func__);
	return ret;
}

static int lt9611_mipi_byte_clk_debug(struct lt9611 *pdata)
{
	u8 reg_val;
	u32 byte_clk;
	
	if (!pdata) {
		printk("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	/* port A byte clk meter */
	lt9611_write(pdata, 0xff, 0x82);
	lt9611_write(pdata, 0xc7, 0x03);        /* port A */
	msleep(50);
	lt9611_read(pdata, 0xcd, &reg_val, 1);

	if ((reg_val & 0x60) == 0x60) {
		byte_clk = (reg_val & 0x0f) * 65536;
		lt9611_read(pdata, 0xce, &reg_val, 1);
		byte_clk = byte_clk + reg_val * 256;
		lt9611_read(pdata, 0xcf, &reg_val, 1);
		byte_clk = byte_clk + reg_val;

		printk("%s: port A byte clk = %d khz,\n", __func__, byte_clk);
	} else {
		printk("%s: port A byte clk unstable\n", __func__);
	}

	/* port B byte clk meter */
	lt9611_write(pdata, 0xff, 0x82);
	lt9611_write(pdata, 0xc7, 0x04); /* port B */
	msleep(50);
	lt9611_read(pdata, 0xcd, &reg_val, 1);

	if ((reg_val & 0x60) == 0x60) {
		byte_clk = (reg_val & 0x0f) * 65536;
		lt9611_read(pdata, 0xce, &reg_val, 1);
		byte_clk = byte_clk + reg_val * 256;
		lt9611_read(pdata, 0xcf, &reg_val, 1);
		byte_clk = byte_clk + reg_val;

		printk("%s: port B byte clk = %d khz,\n", __func__, byte_clk);
	} else {
		printk("%s: port B byte clk unstable\n", __func__);
	}
	
	return 0;
}


static void lt9611_reset(struct lt9611 *pdata)
{
	gpio_set_value(pdata->reset_gpio, 1);
	msleep(20);
	gpio_set_value(pdata->reset_gpio, 0);
	msleep(20);
	gpio_set_value(pdata->reset_gpio, 1);
	msleep(20);
}

static int lt9611_parse_dt(struct device *dev, struct lt9611 *pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;
	int ret = 0;
	u32 flag_irq, flag_reset;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "irq-gpio", 0, &flag_irq);

	if (!gpio_is_valid(pdata->irq_gpio)) {
		printk("%s: irq gpio not specified\n", __func__);
//		ret = -EINVAL;
	}

	printk("%s: irq_gpio=%d\n", __func__, pdata->irq_gpio);

	pdata->reset_gpio = of_get_named_gpio_flags(np, "gpio-reset", 0, &flag_reset);

	if (!gpio_is_valid(pdata->reset_gpio)) {
		printk("%s: reset gpio not specified\n", __func__);
//		ret = -EINVAL;
	}

	printk("%s: reset_gpio=%d\n", __func__, pdata->reset_gpio);

/*
 *      pdata->hdmi_ps_gpio =
 *              of_get_named_gpio(np, "hdmi-ps-gpio", 0);
 *      if (!gpio_is_valid(pdata->hdmi_ps_gpio))
 *              printk("hdmi ps gpio not specified\n");
 *      else
 *              printk("hdmi_ps_gpio=%d\n", pdata->hdmi_ps_gpio);
 *
 *      pdata->hdmi_en_gpio =
 *              of_get_named_gpio(np, "lt,hdmi-en-gpio", 0);
 *      if (!gpio_is_valid(pdata->hdmi_en_gpio))
 *              printk("hdmi en gpio not specified\n");
 *      else
 *              printk("hdmi_en_gpio=%d\n", pdata->hdmi_en_gpio);
 *
 *      pdata->ac_mode = of_property_read_bool(np, "lt,ac-mode");
 *      printk("ac_mode=%d\n", pdata->ac_mode);
 */

	return ret;
}


static int lt9611_gpio_configure(struct lt9611 *pdata, bool on)
{
	int ret;

	if (on) {
        if (gpio_is_valid(pdata->reset_gpio)) {
            ret = gpio_request(pdata->reset_gpio,
                       "lt9611-reset-gpio");
            if (ret) {
                printk("%s: lt9611 reset gpio request failed\n", __func__);
                goto error;
            }

            ret = gpio_direction_output(pdata->reset_gpio, 0);
            if (ret) {
                printk("%s: lt9611 reset gpio direction failed\n", __func__);
                goto reset_error;
            }
        }
#if 0
		if (gpio_is_valid(pdata->hdmi_en_gpio)) {
			ret = gpio_request(pdata->hdmi_en_gpio,
					   "lt9611-hdmi-en-gpio");
			if (ret) {
				printk("lt9611 hdmi en gpio request failed\n");
				goto reset_error;
			}

			ret = gpio_direction_output(pdata->hdmi_en_gpio, 1);
			if (ret) {
				printk("lt9611 hdmi en gpio direction failed\n");
				goto hdmi_en_error;
			}
		}

		if (gpio_is_valid(pdata->hdmi_ps_gpio)) {
			ret = gpio_request(pdata->hdmi_ps_gpio,
					   "lt9611-hdmi-ps-gpio");
			if (ret) {
				printk("lt9611 hdmi ps gpio request failed\n");
				goto hdmi_en_error;
			}

			ret = gpio_direction_input(pdata->hdmi_ps_gpio);
			if (ret) {
				printk("lt9611 hdmi ps gpio direction failed\n");
				goto hdmi_ps_error;
			}
		}
#endif
		if (gpio_is_valid(pdata->irq_gpio)) {
            ret = gpio_request(pdata->irq_gpio, "lt9611-irq-gpio");
            if (ret) {
                printk("%s: lt9611 irq gpio request failed\n", __func__);
                goto hdmi_ps_error;
            }

            ret = gpio_direction_input(pdata->irq_gpio);
            if (ret) {
                printk("%s: lt9611 irq gpio direction failed\n", __func__);
                goto irq_error;
            }
        }
	} else {
		gpio_free(pdata->irq_gpio);
		if (gpio_is_valid(pdata->hdmi_ps_gpio))
			gpio_free(pdata->hdmi_ps_gpio);
		if (gpio_is_valid(pdata->hdmi_en_gpio))
			gpio_free(pdata->hdmi_en_gpio);
		gpio_free(pdata->reset_gpio);
	}

	return ret;

irq_error:
	gpio_free(pdata->irq_gpio);
hdmi_ps_error:
	if (gpio_is_valid(pdata->hdmi_ps_gpio))
		gpio_free(pdata->hdmi_ps_gpio);
hdmi_en_error:
	if (gpio_is_valid(pdata->hdmi_en_gpio))
		gpio_free(pdata->hdmi_en_gpio);
reset_error:
	gpio_free(pdata->reset_gpio);
error:
	return ret;
}


static int lt9611_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct lt9611 *pdata;
	int ret = 0;

	if (!client || !client->dev.of_node) {
		printk("%s invalid input\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&client->dev,
			     sizeof(struct lt9611), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = lt9611_parse_dt(&client->dev, pdata);
	if (ret) {
		printk("%s: failed to parse device tree\n", __func__);
		goto err_dt_parse;
	}

	pdata->i2c_addr = client->addr;
	pdata->i2c_client = client;
	printk("%s: I2C address is 0x%x\n", __func__, pdata->i2c_addr);

	pdata->dev = &client->dev;

	mutex_init(&pdata->ops_mutex);

	ret = lt9611_gpio_configure(pdata, true);       if (ret) {
		printk("%s: failed to configure GPIOs\n", __func__);
		goto err_dt_supply;
	}

	pdata->video_format_id = VIDEO_1920x1080_60HZ;
	pdata->mipi_lane_counts = MIPI_4LANE;
	pdata->mipi_port_counts = MIPI_1PORT;

	if (gpio_is_valid(pdata->reset_gpio)) {
		lt9611_reset(pdata);
	}
    
	ret = lt9611_read_device_rev(pdata);
	if (ret) {
		printk("%s: failed to read chip ver\n", __FUNCTION__);
		goto err_i2c_prog;
	}

	lt9611_system_init(pdata);
	lt9611_read_edid(pdata);
	lt9611_mipi_input_analog(pdata);
	lt9611_mipi_input_digtal(pdata);
	lt9611_mipi_video_setup(pdata);
	lt9611_pll_setup(pdata);
	lt9611_pcr_setup(pdata);
	lt9611_pcr_start(pdata);
	lt9611_audio_setup(pdata);
	lt9611_hdmi_tx_digital(pdata);
	lt9611_hdmi_tx_phy(pdata);
	lt9611_hdmi_output_enable(pdata);

	lt9611_enable_interrupts(pdata);
	
	/*these funcs print log info for debug, can be removed if no need debug*/
	msleep(2000);
	lt9611_video_check_debug(pdata);
	lt9611_mipi_byte_clk_debug(pdata);
	lt9611_pcr_mk_debug(pdata);
	lt9611_htotal_stable_debug(pdata);

       /*debug: hdmi output pattern video which is generated by lt9611, not mipi inut video*/
	/*
	lt9611_read_device_rev(pdata);
	lt9611_system_init(pdata);
	lt9611_pll_setup(pdata);
	lt9611_pattern_gcm(pdata);
	lt9611_pattern_enable(pdata);
	lt9611_pcr_start(pdata);
	lt9611_hdmi_tx_digital(pdata);
	lt9611_hdmi_tx_phy(pdata);
	lt9611_hdmi_output_enable(pdata);
	*/

//	pdata->irq = gpio_to_irq(pdata->irq_gpio);
//	ret = request_threaded_irq(pdata->irq, NULL, lt9611_irq_thread_handler,
//		IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "lt9611", pdata);
//	if (ret) {
//		printk("failed to request irq\n");
//		goto err_irq;
//	}

	return ret;

err_dba_helper:
//	disable_irq(pdata->irq);
//	free_irq(pdata->irq, pdata);
err_irq:
//	lt9611_unregister_dba(pdata);
err_i2c_prog:
//	lt9611_gpio_configure(pdata, false);
err_dt_supply:
//	lt9611_put_dt_supply(&client->dev, pdata);
err_dt_parse:
//	devm_kfree(&client->dev, pdata);
	return ret;
}


static int lt9611_remove(struct i2c_client *client)
{
#if 0
	int ret = -EINVAL;
	struct msm_dba_device_info *dev;
	struct lt9611 *pdata;

	if (!client)
		goto end;

	dev = dev_get_drvdata(&client->dev);
	if (!dev)
		goto end;

	pdata = container_of(dev, struct lt9611, dev_info);
	if (!pdata)
		goto end;

	pm_runtime_disable(&client->dev);
	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);

	ret = lt9611_gpio_configure(pdata, false);

	lt9611_put_dt_supply(&client->dev, pdata);

	mutex_destroy(&pdata->ops_mutex);

	devm_kfree(&client->dev, pdata);

end:
	return ret;

#endif
//	i2c_del_driver(&lontium_i2c_driver);
	printk("%s: remove lt9611 driver successfully", __FUNCTION__);
}


static struct i2c_device_id lt9611_id[] = {
	{ "lt9611", 0 },
	{}
};

static const struct of_device_id lt9611_match_table[] = {
	{ .compatible = "firefly, lt9611" },
	{}
};

MODULE_DEVICE_TABLE(of, lt9611_match_table);

static struct i2c_driver lt9611_driver = {
	.driver			= {
		.name		= "lt9611",
		.owner		= THIS_MODULE,
		.of_match_table = lt9611_match_table,
	},
	.probe			= lt9611_probe,
	.remove			= lt9611_remove,
	.id_table		= lt9611_id,
};

static int __init lt9611_init(void)
{
	return i2c_add_driver(&lt9611_driver);
}

static void __exit lt9611_exit(void)
{
	i2c_del_driver(&lt9611_driver);
}

module_init(lt9611_init);
module_exit(lt9611_exit);


MODULE_AUTHOR("xhguo@lontium.com");
MODULE_DESCRIPTION("Lontium bridge IC LT9611 that convert mipi to hdmi)");
MODULE_LICENSE("GPL");
