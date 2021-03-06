/* bump
 * Copyright 2005-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/fsl_devices.h>
#include <media/v4l2-chip-ident.h>
#include "v4l2-int-device.h"
#include "mxc_v4l2_capture.h"

#ifdef pr_debug
#undef pr_debug
#endif
#ifdef pr_info
#undef pr_info
#endif

#define pr_debug(fmt, ...) \
	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)

#define pr_info(fmt, ...) \
	printk(KERN_DEBUG pr_fmt(fmt), ##__VA_ARGS__)

#define NOCTURN_VOLTAGE_ANALOG               1800000          //  @ECA
#define NOCTURN_VOLTAGE_DIGITAL_CORE         1800000          //  @ECA
#define NOCTURN_VOLTAGE_DIGITAL_IO           3300000          //  @ECA

/* Check these values! */
#define MIN_FPS 30
#define MAX_FPS 100
#define DEFAULT_FPS 60

#define NOCTURN_XCLK_MIN 48000000			//  @ECA
#define NOCTURN_XCLK_MAX 97200000                             //  @ECA

#define NOCTURN_PIXEL_WIDTH 1280
#define NOCTURN_PIXEL_HEIGHT 1024 

enum nocturn_mode {                                           //  @ECA
nocturn_mode_MIN = 0,
	nocturn_mode_SXGA_1280_1024 = 0,                             //  @ECA
nocturn_mode_MAX = 0,
nocturn_mode_INIT = 0xff,
};

enum nocturn_frame_rate {                                     //  @ECA
	nocturn_30_fps,                                            //  @ECA
	nocturn_50_fps,                                             //  @ECA
	nocturn_60_fps,                                             //  @ECA
	nocturn_100_fps                                             //  @ECA
};

static int nocturn_framerates[] = {
        [nocturn_30_fps] = 30,
        [nocturn_50_fps] = 50,
        [nocturn_60_fps] = 60,
        [nocturn_100_fps] = 100
};

struct nocturn_mode_info {                                    //  @ECA
	enum nocturn_mode mode;                                    //  @ECA
	u32 width;
	u32 height;
       	struct reg_value *init_data_ptr;
	u32 init_data_size;
};

static struct nocturn_mode_info nocturn_mode_info_data[2][nocturn_mode_MAX + 1] = {
	{

	   {nocturn_mode_SXGA_1280_1024, NOCTURN_PIXEL_WIDTH,  NOCTURN_PIXEL_HEIGHT,
		 NULL,
		 0},
	},
	{
     {nocturn_mode_SXGA_1280_1024,  NOCTURN_PIXEL_WIDTH,  NOCTURN_PIXEL_HEIGHT,
      NULL,
      0},
	},
};

/*!
 * Maintains the information on the current state of the sesor.
 */

static struct sensor_data nocturn_data;

static struct mxc_camera_platform_data *camera_plat;

extern void gpio_sensor_active(unsigned int csi_index);
extern void gpio_sensor_inactive(unsigned int csi);
static int nocturn_probe(struct i2c_client *adapter,
				const struct i2c_device_id *device_id);
static int nocturn_remove(struct i2c_client *client);


static const struct i2c_device_id nocturn_id[] = {
	{"ovnocturn_xs", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, nocturn_id);
/* probe struct here */
static struct i2c_driver nocturn_i2c_driver = {
	.driver = {
		  .owner = THIS_MODULE,
		  .name  = "ovnocturn_xs",
		  },
	.probe  = nocturn_probe,
	.remove = nocturn_remove,
	.id_table = nocturn_id,
};



static void nocturn_standby(s32 enable)
{
        /*if (enable)
                gpio_set_value(pwn_gpio, 1);
        else
                gpio_set_value(pwn_gpio, 0);
*/
        msleep(2);
}

static void nocturn_reset(void)
{
        /* camera reset */
 //       gpio_set_value(rst_gpio, 1);

        /* camera power down */
  /*      gpio_set_value(pwn_gpio, 1);
        msleep(5);

        gpio_set_value(pwn_gpio, 0);
        msleep(5);

        gpio_set_value(rst_gpio, 0);
        msleep(1);

        gpio_set_value(rst_gpio, 1);
        msleep(5);

        gpio_set_value(pwn_gpio, 1);*/
}






// Full PAL frame size                                                 //  @ECA
#define  FRAME_TOTAL_HEIGHT      626                                   //  @ECA
#define  FRAME_TOTAL_WIDTH       720                                   //  @ECA
// frequency for 25 fps                                                //  @ECA
#define  FRAME_FREQUENCY         (FRAME_TOTAL_WIDTH * FRAME_TOTAL_WIDTH * 25) //@ECA
//
#define  FRAME_REAL_HEIGHT       576                                   //  @ECA
#define  FRAME_REAL_WIDTH        704                                   //  @ECA
#define  BUFFER_STRIDE           (FRAME_REAL_WIDTH * 2)                //  @ECA
#define  BUFFER_SIZE             (BUFFER_STRIDE * FRAME_REAL_HEIGHT)   //  @ECA


/* --------------- IOCTL functions from v4l2_int_ioctl_desc --------------- */

static int ioctl_g_ifparm(struct v4l2_int_device *s, struct v4l2_ifparm *p)
{
	if (s == NULL) {
		pr_err("   ERROR!! no slave device set!\n");
		return -1;
	}

   	pr_info("ovnocturn_xs_camera.ioctl_g_ifparm() -\r\n");                         //  @ECA

	memset(p, 0, sizeof(*p));
	p->u.bt656.clock_curr = nocturn_data.mclk;                 //  @ECA
	pr_debug("   clock_curr=mclk=%d\n", nocturn_data.mclk);    //  @ECA
	p->if_type = V4L2_IF_TYPE_BT656;
	p->u.bt656.mode = V4L2_IF_TYPE_BT656_MODE_NOBT_8BIT;
	p->u.bt656.clock_min = NOCTURN_XCLK_MIN;                   //  @ECA
	p->u.bt656.clock_max = NOCTURN_XCLK_MAX;                   //  @ECA
	p->u.bt656.bt_sync_correct = 1;  /* Indicate external vsync */

	return 0;
}

/*!
 * ioctl_s_power - V4L2 sensor interface handler for VIDIOC_S_POWER ioctl
 * @s: pointer to standard V4L2 device structure
 * @on: indicates power mode (on or off)
 *
 * Turns the power on or off, depending on the value of on and returns the
 * appropriate error code.
 */
static int ioctl_s_power(struct v4l2_int_device *s, int on)
{
	struct sensor_data *sensor = s->priv;

   	pr_info("ovnocturn_xs_camera.ioctl_s_power() -\r\n");                          //  @ECA
   	pr_info("ovnocturn_xs_camera.ioctl_s_power() on=%d, sensor->on=%d-\r\n",on,sensor->on);                          //  @ECA



//THIS IS WHERE GPIO PINS WILL BE PULLED TO TURN THE CAMERA ON!!! JSM

   if (on && !sensor->on) {
                /* Make sure power on */
   		pr_info("	ovnocturn_xs_camera.ioctl_s_power()  on==TRUE & !sensor->on\r\n");                          //  @ECA
                nocturn_standby(0);
        } else if (!on && sensor->on) {
   		pr_info("	ovnocturn_xs_camera.ioctl_s_power()  on==FALSE& sensor->on\r\n");                          //  @ECA
                nocturn_standby(1);
        }




	sensor->on = on;

	return 0;
}

/*!
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor_data *sensor = s->priv;
	struct v4l2_captureparm *cparm = &a->parm.capture;
	int ret = 0;

   pr_info("ovnocturn_xs_camera.ioctl_g_parm() -\r\n");                           //  @ECA

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		memset(a, 0, sizeof(*a));
		a->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cparm->capability = sensor->streamcap.capability;
		cparm->timeperframe = sensor->streamcap.timeperframe;
		cparm->capturemode = sensor->streamcap.capturemode;
		ret = 0;
		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s, struct v4l2_streamparm *a)
{
	struct sensor_data *sensor = s->priv;
	struct v4l2_fract *timeperframe = &a->parm.capture.timeperframe;
	u32 tgt_fps;	/* target frames per secound */
	enum nocturn_frame_rate frame_rate;                        //  @ECA
	int ret = 0;

   pr_info("ovnocturn_xs_camera.ioctl_s_parm() -\r\n");                           //  @ECA

	switch (a->type) {
	/* This is the only case currently handled. */
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		/* Check that the new frame rate is allowed. */
		if ((timeperframe->numerator == 0) ||
		    (timeperframe->denominator == 0)) {
			timeperframe->denominator = DEFAULT_FPS;
			timeperframe->numerator = 1;
		}

		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		if (tgt_fps > MAX_FPS) {
			timeperframe->denominator = MAX_FPS;
			timeperframe->numerator = 1;
		} else if (tgt_fps < MIN_FPS) {
			timeperframe->denominator = MIN_FPS;
			timeperframe->numerator = 1;
		}

		/* Actual frame rate we use */
		tgt_fps = timeperframe->denominator /
			  timeperframe->numerator;

		pr_debug("   target frame rate is  %d fps\n", tgt_fps);

		if (tgt_fps == 30)
			frame_rate = nocturn_30_fps;                         //  @ECA
		else if (tgt_fps == 50)
			frame_rate = nocturn_50_fps;                         //  @ECA
		else if (tgt_fps == 60)
			frame_rate = nocturn_60_fps;                         //  @ECA
		else if (tgt_fps == 100)
			frame_rate = nocturn_100_fps;                         //  @ECA
		else {
			pr_err(" The camera frame rate is not supported!\n");
			return -EINVAL;
		}

     		nocturn_data.pix.width = NOCTURN_PIXEL_WIDTH;
                nocturn_data.pix.height = NOCTURN_PIXEL_HEIGHT;


		sensor->streamcap.timeperframe = *timeperframe;
		sensor->streamcap.capturemode =
				(u32)a->parm.capture.capturemode;


		      pr_debug("ovnocturn_xs:%s: timeperframe num: %d denom: %d, capturemode: %d\n", __func__,
            sensor->streamcap.timeperframe.denominator, sensor->streamcap.timeperframe.numerator,
            sensor->streamcap.capturemode);


		break;

	/* These are all the possible cases. */
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		pr_debug("   type is not " \
			"V4L2_BUF_TYPE_VIDEO_CAPTURE but %d\n",
			a->type);
		ret = -EINVAL;
		break;

	default:
		pr_debug("   type is unknown - %d\n", a->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*!
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s, struct v4l2_format *f)
{
	struct sensor_data *sensor = s->priv;

   	pr_info("ovnocturn_xs_camera.ioctl_g_fmt_cap() -\r\n");                        //  @ECA

	f->fmt.pix = sensor->pix;

	return 0;
}

/*!
 * ioctl_g_ctrl - V4L2 sensor interface handler for VIDIOC_G_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_G_CTRL ioctl structure
 *
 * If the requested control is supported, returns the control's current
 * value from the video_control[] array.  Otherwise, returns -EINVAL
 * if the control is not supported.
 */
static int ioctl_g_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int ret = 0;

   pr_info("ovnocturn_xs_camera.ioctl_g_ctrl() -\r\n");                           //  @ECA
	
	pr_debug("In nocturn:ioctl_g_ctrl \n");

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		vc->value = nocturn_data.brightness;                    //  @ECA
		break;
	case V4L2_CID_HUE:
		vc->value = nocturn_data.hue;                           //  @ECA
		break;
	case V4L2_CID_CONTRAST:
		vc->value = nocturn_data.contrast;                      //  @ECA
		break;
	case V4L2_CID_SATURATION:
		vc->value = nocturn_data.saturation;                    //  @ECA
		break;
	case V4L2_CID_RED_BALANCE:
		vc->value = nocturn_data.red;                           //  @ECA
		break;
	case V4L2_CID_BLUE_BALANCE:
		vc->value = nocturn_data.blue;                          //  @ECA
		break;
	case V4L2_CID_EXPOSURE:
		vc->value = nocturn_data.ae_mode;                       //  @ECA
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

/*!
 * ioctl_s_ctrl - V4L2 sensor interface handler for VIDIOC_S_CTRL ioctl
 * @s: pointer to standard V4L2 device structure
 * @vc: standard V4L2 VIDIOC_S_CTRL ioctl structure
 *
 * If the requested control is supported, sets the control's current
 * value in HW (and updates the video_control[] array).  Otherwise,
 * returns -EINVAL if the control is not supported.
 */
static int ioctl_s_ctrl(struct v4l2_int_device *s, struct v4l2_control *vc)
{
	int retval = 0;

   pr_info("ovnocturn_xs_camera.ioctl_s_ctrl() -\r\n");                           //  @ECA

	pr_debug("In nocturn:ioctl_s_ctrl %d\n", vc->id);

	switch (vc->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	case V4L2_CID_SATURATION:
		break;
	case V4L2_CID_HUE:
		break;
	case V4L2_CID_AUTO_WHITE_BALANCE:
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		break;
	case V4L2_CID_RED_BALANCE:
		break;
	case V4L2_CID_BLUE_BALANCE:
		break;
	case V4L2_CID_GAMMA:
		break;
	case V4L2_CID_EXPOSURE:
		break;
	case V4L2_CID_AUTOGAIN:
		break;
	case V4L2_CID_GAIN:
		break;
	case V4L2_CID_HFLIP:
		break;
	case V4L2_CID_VFLIP:
		break;
	case V4L2_CID_MXC_ROT:
        case V4L2_CID_MXC_VF_ROT:
		break;

	default:
		retval = -EPERM;
		break;
	}

	return retval;
}



/*!
 * ioctl_enum_framesizes - V4L2 sensor interface handler for
 *                         VIDIOC_ENUM_FRAMESIZES ioctl
 * @s: pointer to standard V4L2 device structure
 * @fsize: standard V4L2 VIDIOC_ENUM_FRAMESIZES ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
                                 struct v4l2_frmsizeenum *fsize)
{
   	pr_info("ovnocturn_xs_camera.ioctl_enum_framesizes() -\r\n");                             //  @ECA
        if (fsize->index > nocturn_mode_MAX)
                return -EINVAL;

        fsize->pixel_format = nocturn_data.pix.pixelformat;
        fsize->discrete.width =
                        max(nocturn_mode_info_data[0][fsize->index].width,
                            nocturn_mode_info_data[1][fsize->index].width);
        fsize->discrete.height =
                        max(nocturn_mode_info_data[0][fsize->index].height,
                            nocturn_mode_info_data[1][fsize->index].height);
        return 0;
}





/*!
 * ioctl_enum_frameintervals - V4L2 sensor interface handler for
 *                             VIDIOC_ENUM_FRAMEINTERVALS ioctl
 * @s: pointer to standard V4L2 device structure
 * @fival: standard V4L2 VIDIOC_ENUM_FRAMEINTERVALS ioctl structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */


static int ioctl_enum_frameintervals(struct v4l2_int_device *s,
                                         struct v4l2_frmivalenum *fival)
{
        int i, j, count;
   	pr_info("ovnocturn_xs_camera.ioctl_enum_frameintervals() -\r\n");                             //  @ECA

        if (fival->index < 0 || fival->index > nocturn_mode_MAX)
                return -EINVAL;

        if (fival->pixel_format == 0 || fival->width == 0 ||
                        fival->height == 0) {
                pr_warning("Please assign pixelformat, width and height.\n");
                return -EINVAL;
        }

        fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
        fival->discrete.numerator = 1;

        count = 0;
        for (i = 0; i < ARRAY_SIZE(nocturn_mode_info_data); i++) {
                for (j = 0; j < (nocturn_mode_MAX + 1); j++) {
                        if (fival->pixel_format == nocturn_data.pix.pixelformat
                         && fival->width == nocturn_mode_info_data[i][j].width
                         && fival->height == nocturn_mode_info_data[i][j].height
                         && nocturn_mode_info_data[i][j].init_data_ptr != NULL) {
                                count++;
                        }
                        if (fival->index == (count - 1)) {
                                fival->discrete.denominator =
                                                nocturn_framerates[i];
                                return 0;
                        }
                }
        }

        return -EINVAL;
}



/*!
 * ioctl_g_chip_ident - V4L2 sensor interface handler for
 *			VIDIOC_DBG_G_CHIP_IDENT ioctl
 * @s: pointer to standard V4L2 device structure
 * @id: pointer to int
 *
 * Return 0.
 */
static int ioctl_g_chip_ident(struct v4l2_int_device *s, int *id)
{
	((struct v4l2_dbg_chip_ident *)id)->match.type =
					V4L2_CHIP_MATCH_I2C_DRIVER;
	strcpy(((struct v4l2_dbg_chip_ident *)id)->match.name,
		"ovnocturn_xs_camera");
   	pr_info("ovnocturn_xs_camera.ioctl_g_chip_ident() -\r\n");                             //  @ECA

	return 0;
}

/*!
 * ioctl_init - V4L2 sensor interface handler for VIDIOC_INT_INIT
 * @s: pointer to standard V4L2 device structure
 */
static int ioctl_init(struct v4l2_int_device *s)
{
   pr_info("ovnocturn_xs_camera.ioctl_init() -\r\n");                             //  @ECA

	return 0;
}



/*!
 * ioctl_enum_fmt_cap - V4L2 sensor interface handler for VIDIOC_ENUM_FMT
 * @s: pointer to standard V4L2 device structure
 * @fmt: pointer to standard V4L2 fmt description structure
 *
 * Return 0.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
                              struct v4l2_fmtdesc *fmt)
{
   pr_info("ovnocturn_xs_camera.ioctl_enum_fmt_cap() -\r\n");                             //  @ECA
        if (fmt->index > 0)     /* only 1 pixelformat support so far */
                return -EINVAL;

        fmt->pixelformat = nocturn_data.pix.pixelformat;

        return 0;
}

/*!
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	struct sensor_data *sensor = s->priv;
	u32 tgt_xclk;	/* target xclk */
	u32 tgt_fps;	/* target frames per secound */
	enum nocturn_frame_rate frame_rate;                        //  @ECA
	int ret = 0;

   	pr_info("ovnocturn_xs_camera.ioctl_dev_init() - Entering\r\n");                //  @ECA
	nocturn_data.on = true;                                    //  @ECA

	/* mclk */
	tgt_xclk = nocturn_data.mclk;                              //  @ECA
	tgt_xclk = min(tgt_xclk, (u32)NOCTURN_XCLK_MAX);
	tgt_xclk = max(tgt_xclk, (u32)NOCTURN_XCLK_MIN);           //  @ECA
	nocturn_data.mclk = tgt_xclk;                              //  @ECA

	pr_debug("   Setting mclk to %d MHz\n", tgt_xclk / 1000000);
	//set_mclk_rate(&nocturn_data.mclk, nocturn_data.csi);//  @ECA

	/* Default camera frame rate is set in probe */
	tgt_fps = sensor->streamcap.timeperframe.denominator /
		  sensor->streamcap.timeperframe.numerator;

	pr_debug("   target frame rate is  %d fps\n", tgt_fps);

	if (tgt_fps == 30)
		frame_rate = nocturn_30_fps;                            //  @ECA
	else if (tgt_fps == 50)
		frame_rate = nocturn_50_fps;                            //  @ECA
	else if (tgt_fps == 60)
		frame_rate = nocturn_60_fps;                            //  @ECA
	else if (tgt_fps == 100)
		frame_rate = nocturn_100_fps;                            //  @ECA
	else{
		pr_err("   Frame rate unsupported");
		return -EINVAL; 
	} 
		
	pr_debug("   Setting frame rate to  %d fps\n", frame_rate);

	pr_debug("   Adjusting frame rate to  %d fps\n", nocturn_60_fps);
	frame_rate = nocturn_60_fps;                            //  @ECA

	return ret;

}

/*!
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the device when slave detaches to the master.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
   pr_info("ovnocturn_xs_camera.ioctl_dev_exit() -\r\n");                         //  @ECA

	return 0;
}


/*!
 * This structure defines all the ioctls for this module and links them to the
 * enumeration.
 */
static struct v4l2_int_ioctl_desc nocturn_ioctl_desc[] = {    //  @ECA
        {vidioc_int_dev_init_num, (v4l2_int_ioctl_func *)ioctl_dev_init},
        {vidioc_int_dev_exit_num, ioctl_dev_exit},
        {vidioc_int_s_power_num, (v4l2_int_ioctl_func *)ioctl_s_power},
        {vidioc_int_g_ifparm_num, (v4l2_int_ioctl_func *)ioctl_g_ifparm},
        {vidioc_int_init_num, (v4l2_int_ioctl_func *)ioctl_init},
        {vidioc_int_enum_fmt_cap_num,(v4l2_int_ioctl_func *)ioctl_enum_fmt_cap},
        {vidioc_int_g_fmt_cap_num, (v4l2_int_ioctl_func *)ioctl_g_fmt_cap},
        {vidioc_int_g_parm_num, (v4l2_int_ioctl_func *)ioctl_g_parm},
        {vidioc_int_s_parm_num, (v4l2_int_ioctl_func *)ioctl_s_parm},
        {vidioc_int_g_ctrl_num, (v4l2_int_ioctl_func *)ioctl_g_ctrl},
        {vidioc_int_s_ctrl_num, (v4l2_int_ioctl_func *)ioctl_s_ctrl},
        {vidioc_int_enum_framesizes_num,(v4l2_int_ioctl_func *)ioctl_enum_framesizes},
        {vidioc_int_enum_frameintervals_num,(v4l2_int_ioctl_func *)ioctl_enum_frameintervals},
	{vidioc_int_g_chip_ident_num,(v4l2_int_ioctl_func *)ioctl_g_chip_ident},
};


static struct v4l2_int_slave nocturn_slave = {                //  @ECA
        .ioctls = nocturn_ioctl_desc,                              //  @ECA
        .num_ioctls = ARRAY_SIZE(nocturn_ioctl_desc),              //  @ECA
};

static struct v4l2_int_device nocturn_int_device = {          //  @ECA
        .module = THIS_MODULE,
        .name = "ovnocturn_xs",                                         //  @ECA
        .type = v4l2_int_type_slave,
        .u = {
                .slave = &nocturn_slave,                                //  @ECA
        },
};


/*!
 * nocturn1280 I2C probe function
 *
 * @param adapter            struct i2c_adapter *
 * @return  Error code indicating success or failure
 */
static int nocturn_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct pinctrl * pinctrl;
	struct device  * dev = &client->dev;
	int retval;

	struct sensor_data *sensor = &nocturn_data;

        pr_info("ovnocturn_xs_camera.nocturn_probe() - Entering\r\n");         //  @ECA
        //GRAFTED FROM OV5642.c probe() I2C functions
        /* Set initial values for the sensor struct. */
        memset(&nocturn_data, 0, sizeof(nocturn_data));

        //nocturn_data.sensor_clk = devm_clk_get(dev, "csi_mclk");
         if (IS_ERR(nocturn_data.sensor_clk)) {
                /* assuming clock enabled by default */
                nocturn_data.sensor_clk = NULL;
                //dev_err(dev, "clock-frequency missing or invalid\n");
                return PTR_ERR(nocturn_data.sensor_clk);
        }


        clk_prepare_enable(nocturn_data.sensor_clk);

	nocturn_data.i2c_client = client;

        nocturn_data.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        nocturn_data.pix.width = NOCTURN_PIXEL_WIDTH;
        nocturn_data.pix.height = NOCTURN_PIXEL_HEIGHT;
        nocturn_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
                                           V4L2_CAP_TIMEPERFRAME;
        nocturn_data.streamcap.capturemode = 0;
        nocturn_data.streamcap.timeperframe.denominator = DEFAULT_FPS;
        nocturn_data.streamcap.timeperframe.numerator = 1;

        nocturn_data.csi = 1;
        nocturn_data.ipu_id = 1;
        pr_info("nocturn_data struct setup done\n");

        nocturn_int_device.priv = &nocturn_data;

        retval = v4l2_int_device_register(&nocturn_int_device);
        pr_info("camera is found retval=%d\n", retval);

        return retval;

}




/*!
 * nocturn I2C detach function
 *
 * @param client            struct i2c_client *
 * @return  Error code indicating success or failure
 */
static int nocturn_remove(struct i2c_client *client)
{
        pr_info("ovnocturn_xs_camera.nocturn_remove() - Entering\r\n");         //  @ECA
	v4l2_int_device_unregister(&nocturn_int_device);

	return 0;
}




/*!
 * nocturn init function
 * Called by insmod nocturn_camera.ko.
 *
 * @return  Error code indicating success or failure
 */
static __init int nocturn_init(void)                          //  @ECA
{
	u8 err;
       int retval; 
 	struct sensor_data *sensor = &nocturn_data;

        pr_info("ovnocturn_xs_camera.nocturn_init() - Entering\r\n");         //  @ECA
        /* Set initial values for the sensor struct. */
        memset(&nocturn_data, 0, sizeof(nocturn_data));

        //nocturn_data.sensor_clk = devm_clk_get(dev, "csi_mclk");
         if (IS_ERR(nocturn_data.sensor_clk)) {
                /* assuming clock enabled by default */
                nocturn_data.sensor_clk = NULL;
                //dev_err(dev, "clock-frequency missing or invalid\n");
                return PTR_ERR(nocturn_data.sensor_clk);
        }


        clk_prepare_enable(nocturn_data.sensor_clk);
	nocturn_data.io_init = nocturn_reset;
        nocturn_data.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        nocturn_data.pix.width = NOCTURN_PIXEL_WIDTH;
        nocturn_data.pix.height = NOCTURN_PIXEL_HEIGHT;
        nocturn_data.streamcap.capability = V4L2_MODE_HIGHQUALITY |
                                           V4L2_CAP_TIMEPERFRAME;
        nocturn_data.streamcap.capturemode = 0;
        nocturn_data.streamcap.timeperframe.denominator = DEFAULT_FPS;
        nocturn_data.streamcap.timeperframe.numerator = 1;

        nocturn_data.csi = 1; 
        nocturn_data.ipu_id = 1;
        pr_info("nocturn_data struct setup done\n");

        nocturn_int_device.priv = &nocturn_data;

        retval = v4l2_int_device_register(&nocturn_int_device);

	clk_disable_unprepare(nocturn_data.sensor_clk);
        pr_info("camera nocturn_xs is found retval=%d\n", retval);

        return retval;

        
        /*err = i2c_add_driver(&nocturn_i2c_driver);
        if (err != 0)
                pr_err("%s:driver registration failed, error=%d\n",
                        __func__, err);

        return err;*/

}


/*
 * nocturn cleanup function
 * Called on rmmod nocturn_camera.ko
 *
 * @return  Error code indicating success or failure
 */
static void __exit nocturn_clean(void)                        //  @ECA
{
   pr_info("ovnocturn_xs_camera.nocturn_clean() - Entering\r\n");        //  @ECA
   pr_info("ovnocturn_xs_camera.nocturn_clean() - Skipping i2c driver removal\r\n");        //  @ECA
	
   //i2c_del_driver(&nocturn_i2c_driver);

}

module_init(nocturn_init);                                    //  @ECA
module_exit(nocturn_clean);                                   //  @ECA
//module_i2c_driver(nocturn_i2c_driver);			


MODULE_AUTHOR("JANUS Research Group, Inc.");
MODULE_DESCRIPTION("NOCTURN XS Camera Driver");                  //  @ECA
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_ALIAS("CSI");
