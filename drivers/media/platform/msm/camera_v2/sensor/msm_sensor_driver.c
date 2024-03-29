/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define SENSOR_DRIVER_I2C "camera"
/* Header file declaration */
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_dt_util.h"

#if defined CONFIG_SEC_CAMERA_TUNING
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
static char *regs_table = NULL;
static int table_size;
#endif
#if defined(CONFIG_SR200PC20)
#include "sr200pc20.h"
#endif
#if defined(CONFIG_S5K4ECGX)
#include "s5k4ecgx.h"
#endif
#if defined(CONFIG_SR352)
#include "sr352.h"
#endif
#if defined(CONFIG_SR130PC20)
#include "sr130pc20.h"
#endif
#if defined(CONFIG_DB8221A)
#include "db8221a.h"
#endif

/* Logging macro */
//#define MSM_SENSOR_DRIVER_DEBUG
#undef CDBG
#ifdef MSM_SENSOR_DRIVER_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define SENSOR_MAX_MOUNTANGLE (360)

#ifdef CONFIG_CAM_DISABLE_LPM_MODE
extern int poweroff_charging;
#endif

#if defined (CONFIG_CAMERA_SYSFS_V2)
extern char rear_cam_info[100];		//camera_info
extern char front_cam_info[100];	//camera_info
#endif

/* Static declaration */
static struct msm_sensor_ctrl_t *g_sctrl[MAX_CAMERAS];

#if defined(CONFIG_SR352)
static struct msm_sensor_fn_t sr352_sensor_func_tbl = {
	.sensor_config = sr352_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
	.sensor_native_control = sr352_sensor_native_control,
}; 
#endif

#if defined(CONFIG_SR130PC20)
static struct msm_sensor_fn_t sr130pc20_sensor_func_tbl = {
	.sensor_config = sr130pc20_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
	.sensor_native_control = sr130pc20_sensor_native_control,
};
#endif

#if defined(CONFIG_SR200PC20)
static struct msm_sensor_fn_t sr200pc20_sensor_func_tbl = {
	.sensor_config = sr200pc20_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
	.sensor_native_control = sr200pc20_sensor_native_control,
};
#endif
#if defined(CONFIG_S5K4ECGX)
static struct msm_sensor_fn_t s5k4ecgx_sensor_func_tbl = {
	.sensor_config = s5k4ecgx_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = s5k4ecgx_sensor_match_id,
	.sensor_native_control = s5k4ecgx_sensor_native_control,
};
#endif
#if defined(CONFIG_DB8221A)
static struct msm_sensor_fn_t db8221a_sensor_func_tbl = {
	.sensor_config = db8221a_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
	.sensor_native_control = db8221a_sensor_native_control,
};
#endif

int32_t msm_sensor_remove_dev_node_for_eeprom(int id,int remove);

static int msm_sensor_platform_remove(struct platform_device *pdev)
{
	struct msm_sensor_ctrl_t  *s_ctrl;

	pr_err("%s: sensor FREE\n", __func__);

	s_ctrl = g_sctrl[pdev->id];
	if (!s_ctrl) {
		pr_err("%s: sensor device is NULL\n", __func__);
		return 0;
	}

	msm_sensor_free_sensor_data(s_ctrl);
	kfree(s_ctrl->msm_sensor_mutex);
	kfree(s_ctrl->sensor_i2c_client);
	kfree(s_ctrl);
	g_sctrl[pdev->id] = NULL;

	return 0;
}


static const struct of_device_id msm_sensor_driver_dt_match[] = {
	{.compatible = "qcom,camera"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_sensor_driver_dt_match);

static struct platform_driver msm_sensor_platform_driver = {
	.driver = {
		.name = "qcom,camera",
		.owner = THIS_MODULE,
		.of_match_table = msm_sensor_driver_dt_match,
	},
	.remove = msm_sensor_platform_remove,
};

static struct v4l2_subdev_info msm_sensor_driver_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static int32_t msm_sensor_driver_create_i2c_v4l_subdev
			(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t session_id = 0;
	struct i2c_client *client = s_ctrl->sensor_i2c_client->client;

	CDBG("%s %s I2c probe succeeded\n", __func__, client->name);
	rc = camera_init_v4l2(&client->dev, &session_id);
	if (rc < 0) {
		pr_err("failed: camera_init_i2c_v4l2 rc %d", rc);
		return rc;
	}
	CDBG("%s rc %d session_id %d\n", __func__, rc, session_id);
	snprintf(s_ctrl->msm_sd.sd.name,
		sizeof(s_ctrl->msm_sd.sd.name), "%s",
		s_ctrl->sensordata->sensor_name);
	v4l2_i2c_subdev_init(&s_ctrl->msm_sd.sd, client,
		s_ctrl->sensor_v4l2_subdev_ops);
	v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, client);
	s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
	s_ctrl->msm_sd.sd.entity.name =	s_ctrl->msm_sd.sd.name;
	s_ctrl->sensordata->sensor_info->session_id = session_id;
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	rc = msm_sd_register(&s_ctrl->msm_sd);
	if (rc < 0) {
		pr_err("failed: msm_sd_register rc %d", rc);
		return rc;
	}
	CDBG("%s:%d\n", __func__, __LINE__);
	return rc;
}

static int32_t msm_sensor_driver_create_v4l_subdev
			(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint32_t session_id = 0;

	rc = camera_init_v4l2(&s_ctrl->pdev->dev, &session_id);
	if (rc < 0) {
		pr_err("failed: camera_init_v4l2 rc %d", rc);
		return rc;
	}
	CDBG("rc %d session_id %d", rc, session_id);
	s_ctrl->sensordata->sensor_info->session_id = session_id;

	/* Create /dev/v4l-subdevX device */
	v4l2_subdev_init(&s_ctrl->msm_sd.sd, s_ctrl->sensor_v4l2_subdev_ops);
	snprintf(s_ctrl->msm_sd.sd.name, sizeof(s_ctrl->msm_sd.sd.name), "%s",
		s_ctrl->sensordata->sensor_name);
	v4l2_set_subdevdata(&s_ctrl->msm_sd.sd, s_ctrl->pdev);
	s_ctrl->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&s_ctrl->msm_sd.sd.entity, 0, NULL, 0);
	s_ctrl->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	s_ctrl->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_SENSOR;
	s_ctrl->msm_sd.sd.entity.name = s_ctrl->msm_sd.sd.name;
	s_ctrl->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x3;
	rc = msm_sd_register(&s_ctrl->msm_sd);
	if (rc < 0) {
		pr_err("failed: msm_sd_register rc %d", rc);
		return rc;
	}
	return rc;
}

static int32_t msm_sensor_fill_eeprom_subdevid_by_name(
				struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	const char *eeprom_name;
	struct device_node *src_node = NULL;
	uint32_t val = 0, count = 0, eeprom_name_len;
	int i;
	int32_t *eeprom_subdev_id;
	struct  msm_sensor_info_t *sensor_info;
	struct device_node *of_node = s_ctrl->of_node;
	const void *p;

	if (!s_ctrl->sensordata->eeprom_name || !of_node)
		return -EINVAL;

	eeprom_name_len = strlen(s_ctrl->sensordata->eeprom_name);
	if (eeprom_name_len >= MAX_SENSOR_NAME)
		return -EINVAL;

	sensor_info = s_ctrl->sensordata->sensor_info;
	eeprom_subdev_id = &sensor_info->subdev_id[SUB_MODULE_EEPROM];
	/*
	 * string for eeprom name is valid, set sudev id to -1
	 *  and try to found new id
	 */
	*eeprom_subdev_id = -1;

	if (0 == eeprom_name_len)
		return 0;

	CDBG("Try to find eeprom subdev for %s\n",
			s_ctrl->sensordata->eeprom_name);
	p = of_get_property(of_node, "qcom,eeprom-src", &count);
	if (!p || !count)
		return 0;

	count /= sizeof(uint32_t);
	for (i = 0; i < count; i++) {
		eeprom_name = NULL;
		src_node = of_parse_phandle(of_node, "qcom,eeprom-src", i);
		if (!src_node) {
			pr_err("eeprom src node NULL\n");
			continue;
		}
		rc = of_property_read_string(src_node, "qcom,eeprom-name",
			&eeprom_name);
		if (rc < 0) {
			pr_err("failed\n");
			of_node_put(src_node);
			continue;
		}
		if (strcmp(eeprom_name, s_ctrl->sensordata->eeprom_name))
			continue;

		rc = of_property_read_u32(src_node, "cell-index", &val);

		CDBG("%s qcom,eeprom cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("failed\n");
			of_node_put(src_node);
			continue;
		}

		*eeprom_subdev_id = val;
		CDBG("Done. Eeprom subdevice id is %d\n", val);
		of_node_put(src_node);
		src_node = NULL;
		break;
	}

	return rc;
}

static int32_t msm_sensor_fill_actuator_subdevid_by_name(
				struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device_node *src_node = NULL;
	uint32_t val = 0, actuator_name_len;
	int32_t *actuator_subdev_id;
	struct  msm_sensor_info_t *sensor_info;
	struct device_node *of_node = s_ctrl->of_node;

	if (!s_ctrl->sensordata->actuator_name || !of_node)
		return -EINVAL;

	actuator_name_len = strlen(s_ctrl->sensordata->actuator_name);
	if (actuator_name_len >= MAX_SENSOR_NAME) {
		pr_err("msm_sensor_fill_actuator_subdevid_by_name: actuator_name_len is greater than max len\n");
		return -EINVAL;
	}

	sensor_info = s_ctrl->sensordata->sensor_info;
	actuator_subdev_id = &sensor_info->subdev_id[SUB_MODULE_ACTUATOR];
	/*
	 * string for actuator name is valid, set sudev id to -1
	 * and try to found new id
	 */
	*actuator_subdev_id = -1;

	if (0 == actuator_name_len) {
		pr_err("msm_sensor_fill_actuator_subdevid_by_name: actuator_name_len is zero\n");
		return 0;
	}

	src_node = of_parse_phandle(of_node, "qcom,actuator-src", 0);
	if (!src_node) {
		pr_err("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		pr_err("%s qcom,actuator cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return -EINVAL;
		}
		*actuator_subdev_id = val;
		of_node_put(src_node);
		src_node = NULL;
	}

	return rc;
}

static int32_t msm_sensor_fill_slave_info_init_params(
	struct msm_camera_sensor_slave_info *slave_info,
	struct msm_sensor_info_t *sensor_info)
{
	struct msm_sensor_init_params *sensor_init_params;
	if (!slave_info ||  !sensor_info)
		return -EINVAL;

	if (!slave_info->is_init_params_valid)
		return 0;

	sensor_init_params = &slave_info->sensor_init_params;
	if (INVALID_CAMERA_B != sensor_init_params->position)
		sensor_info->position =
			sensor_init_params->position;

	if (SENSOR_MAX_MOUNTANGLE > sensor_init_params->sensor_mount_angle) {
		sensor_info->sensor_mount_angle =
			sensor_init_params->sensor_mount_angle;
		sensor_info->is_mount_angle_valid = 1;
	}

	if (CAMERA_MODE_INVALID != sensor_init_params->modes_supported)
		sensor_info->modes_supported =
			sensor_init_params->modes_supported;

	return 0;
}


static int32_t msm_sensor_validate_slave_info(
	struct msm_sensor_info_t *sensor_info)
{
	if (INVALID_CAMERA_B == sensor_info->position) {
		sensor_info->position = BACK_CAMERA_B;
		CDBG("%s:%d Set default sensor position\n",
			__func__, __LINE__);
	}
	if (CAMERA_MODE_INVALID == sensor_info->modes_supported) {
		sensor_info->modes_supported = CAMERA_MODE_2D_B;
		CDBG("%s:%d Set default sensor modes_supported\n",
			__func__, __LINE__);
	}
	if (SENSOR_MAX_MOUNTANGLE <= sensor_info->sensor_mount_angle) {
		sensor_info->sensor_mount_angle = 0;
		CDBG("%s:%d Set default sensor mount angle\n",
			__func__, __LINE__);
		sensor_info->is_mount_angle_valid = 1;
	}
	return 0;
}

/* static function definition */
int32_t msm_sensor_driver_probe(void *setting)
{
	int32_t                              rc = 0;
	uint16_t                             i = 0, size = 0, size_down = 0;
	struct msm_sensor_ctrl_t            *s_ctrl = NULL;
	struct msm_camera_cci_client        *cci_client = NULL;
	struct msm_camera_sensor_slave_info *slave_info = NULL;
	struct msm_sensor_power_setting     *power_setting = NULL;
	struct msm_sensor_power_setting     *power_down_setting = NULL;
	struct msm_camera_slave_info        *camera_info = NULL;
	struct msm_camera_power_ctrl_t      *power_info = NULL;
	int c, end;
	struct msm_sensor_power_setting     power_down_setting_t;
	unsigned long mount_pos = 0;
	int probe_fail = 0;

	/* Validate input parameters */
	if (!setting) {
		pr_err("failed: slave_info %pK", setting);
		return -EINVAL;
	}

	/* Allocate memory for slave info */
	slave_info = kzalloc(sizeof(*slave_info), GFP_KERNEL);
	if (!slave_info) 
		return -ENOMEM;

	if (copy_from_user(slave_info, (void *)setting, sizeof(*slave_info))) {
		pr_err("failed: copy_from_user");
		rc = -EFAULT;
		goto FREE_SLAVE_INFO;
	}

	/* Print slave info */
	CDBG("camera id %d", slave_info->camera_id);
	CDBG("slave_addr 0x%x", slave_info->slave_addr);
	CDBG("addr_type %d", slave_info->addr_type);
	CDBG("sensor_id_reg_addr 0x%x",
		slave_info->sensor_id_info.sensor_id_reg_addr);
	CDBG("sensor_id 0x%x", slave_info->sensor_id_info.sensor_id);
	CDBG("size %d", slave_info->power_setting_array.size);
	CDBG("size down %d", slave_info->power_setting_array.size_down);
	CDBG("sensor_name %s", slave_info->sensor_name);

	if (slave_info->is_init_params_valid) {
		CDBG("position %d",
			slave_info->sensor_init_params.position);
		CDBG("mount %d",
			slave_info->sensor_init_params.sensor_mount_angle);
	}

	/* Validate camera id */
	if (slave_info->camera_id >= MAX_CAMERAS) {
		pr_err("failed: invalid camera id %d max %d",
			slave_info->camera_id, MAX_CAMERAS);
		rc = -EINVAL;
		goto FREE_SLAVE_INFO;
	}

	/* Extract s_ctrl from camera id */
	s_ctrl = g_sctrl[slave_info->camera_id];
	if (!s_ctrl) {
		pr_err("failed: s_ctrl %pK for camera_id %d", s_ctrl,
			slave_info->camera_id);
		rc = -EINVAL;
		goto FREE_SLAVE_INFO;
	}
#if defined(CONFIG_SR352) && defined(CONFIG_SR130PC20)
	if ( slave_info->camera_id == CAMERA_2 ) {
		s_ctrl->func_tbl = &sr130pc20_sensor_func_tbl ;
	} else {
		s_ctrl->func_tbl = &sr352_sensor_func_tbl ;
	}
#endif
#if defined(CONFIG_SR200PC20)
	if(slave_info->camera_id == CAMERA_2){
		s_ctrl->func_tbl = &sr200pc20_sensor_func_tbl ;
	}
#endif
#if defined(CONFIG_S5K4ECGX)
	if (slave_info->camera_id == CAMERA_0){
		s_ctrl->func_tbl = &s5k4ecgx_sensor_func_tbl;
	}
#endif
#if defined(CONFIG_DB8221A)
	if(slave_info->camera_id == CAMERA_2){
		s_ctrl->func_tbl = &db8221a_sensor_func_tbl ;
	}
#endif
	CDBG("s_ctrl[%d] %pK", slave_info->camera_id, s_ctrl);

	if (s_ctrl->is_probe_succeed == 1) {
		/*
		 * Different sensor on this camera slot has been connected
		 * and probe already succeeded for that sensor. Ignore this
		 * probe
		 */
		pr_err("slot %d has some other sensor", slave_info->camera_id);
		rc = 0;
		goto FREE_SLAVE_INFO;
	}

	size = slave_info->power_setting_array.size;
	/* Allocate memory for power up setting */
	power_setting = kzalloc(sizeof(*power_setting) * size, GFP_KERNEL);
	if (!power_setting) {
		pr_err("failed: no memory power_setting %pK", power_setting);
		rc = -ENOMEM;
		goto FREE_SLAVE_INFO;
	}

	if (copy_from_user(power_setting,
		(void *)slave_info->power_setting_array.power_setting,
		sizeof(*power_setting) * size)) {
		pr_err("failed: copy_from_user");
		rc = -EFAULT;
		goto FREE_POWER_SETTING;
	}

	/* Print power setting */
	for (i = 0; i < size; i++) {
		CDBG("UP seq_type %d seq_val %d config_val %ld delay %d",
			power_setting[i].seq_type, power_setting[i].seq_val,
			power_setting[i].config_val, power_setting[i].delay);
	}
	/*DOWN*/
	size_down = slave_info->power_setting_array.size_down;
	if (!size_down)
		size_down = size;
	/* Allocate memory for power down setting */
	power_down_setting =
		kzalloc(sizeof(*power_setting) * size_down, GFP_KERNEL);
	if (!power_down_setting) {
		pr_err("failed: no memory power_setting %pK",
						power_down_setting);
		rc = -ENOMEM;
		goto FREE_POWER_SETTING;
	}

	if (slave_info->power_setting_array.power_down_setting) {
		if (copy_from_user(power_down_setting,
			(void *)slave_info->power_setting_array.
						power_down_setting,
			sizeof(*power_down_setting) * size_down)) {
			pr_err("failed: copy_from_user");
			rc = -EFAULT;
			goto FREE_POWER_DOWN_SETTING;
		}
	} else {
		pr_err("failed: no power_down_setting");
		if (copy_from_user(power_down_setting,
			(void *)slave_info->power_setting_array.
						power_setting,
			sizeof(*power_down_setting) * size_down)) {
			pr_err("failed: copy_from_user");
			rc = -EFAULT;
			goto FREE_POWER_DOWN_SETTING;
		}

		/*reverce*/
		end = size_down - 1;
		for (c = 0; c < size_down/2; c++) {
			power_down_setting_t = power_down_setting[c];
			power_down_setting[c] = power_down_setting[end];
			power_down_setting[end] = power_down_setting_t;
			end--;
		}

	}

	/* Print power setting */
	for (i = 0; i < size_down; i++) {
		CDBG("DOWN seq_type %d seq_val %d config_val %ld delay %d",
			power_down_setting[i].seq_type,
			power_down_setting[i].seq_val,
			power_down_setting[i].config_val,
			power_down_setting[i].delay);
	}

	camera_info = kzalloc(sizeof(struct msm_camera_slave_info), GFP_KERNEL);
	if (!camera_info) 
		goto FREE_POWER_DOWN_SETTING;


	/* Fill power up setting and power up setting size */
	power_info = &s_ctrl->sensordata->power_info;
	power_info->power_setting = power_setting;
	power_info->power_setting_size = size;
	power_info->power_down_setting = power_down_setting;
	power_info->power_down_setting_size = size_down;

	s_ctrl->sensordata->slave_info = camera_info;

	/* Fill sensor slave info */
	camera_info->sensor_slave_addr = slave_info->slave_addr;
	camera_info->sensor_id_reg_addr =
		slave_info->sensor_id_info.sensor_id_reg_addr;
	camera_info->sensor_id = slave_info->sensor_id_info.sensor_id;

	/* Fill CCI master, slave address and CCI default params */
	if (!s_ctrl->sensor_i2c_client) {
		pr_err("failed: sensor_i2c_client %pK",
			s_ctrl->sensor_i2c_client);
		rc = -EINVAL;
		goto FREE_CAMERA_INFO;
	}
	/* Fill sensor address type */
	s_ctrl->sensor_i2c_client->addr_type = slave_info->addr_type;
	if (s_ctrl->sensor_i2c_client->client)
		s_ctrl->sensor_i2c_client->client->addr =
			camera_info->sensor_slave_addr;

	cci_client = s_ctrl->sensor_i2c_client->cci_client;
	if (!cci_client) {
		pr_err("failed: cci_client %pK", cci_client);
		goto FREE_CAMERA_INFO;
	}
	cci_client->cci_i2c_master = s_ctrl->cci_i2c_master;
	cci_client->sid = slave_info->slave_addr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;
	cci_client->i2c_freq_mode = slave_info->i2c_freq_mode;

	/* Parse and fill vreg params for powerup settings */
	rc = msm_camera_fill_vreg_params(
		power_info->cam_vreg,
		power_info->num_vreg,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_dt_power_setting_data rc %d",
			rc);
		goto FREE_CAMERA_INFO;
	}

	/* Parse and fill vreg params for powerdown settings*/
	rc = msm_camera_fill_vreg_params(
		power_info->cam_vreg,
		power_info->num_vreg,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_fill_vreg_params for PDOWN rc %d",
			rc);
		goto FREE_CAMERA_INFO;
	}

	/* Update sensor, actuator and eeprom name in
	*  sensor control structure */
	s_ctrl->sensordata->sensor_name = slave_info->sensor_name;
	s_ctrl->sensordata->eeprom_name = slave_info->eeprom_name;
	s_ctrl->sensordata->actuator_name = slave_info->actuator_name;

	/*
	 * Update eeporm subdevice Id by input eeprom name
	 */
	rc = msm_sensor_fill_eeprom_subdevid_by_name(s_ctrl);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_CAMERA_INFO;
	}
	/*
	 * Update actuator subdevice Id by input actuator name
	 */
	rc = msm_sensor_fill_actuator_subdevid_by_name(s_ctrl);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto FREE_CAMERA_INFO;
	}
	msm_sensor_remove_dev_node_for_eeprom(slave_info->camera_id,
		s_ctrl->sensordata->sensor_info->subdev_id[SUB_MODULE_EEPROM]);
	/* Power up and probe sensor */
	rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
	if (rc < 0) {
		pr_err("%s power up failed", slave_info->sensor_name);
		probe_fail = rc;
//		goto FREE_CAMERA_INFO;
	}

	pr_err("%s probe succeeded", slave_info->sensor_name);

	/*
	  Set probe succeeded flag to 1 so that no other camera shall
	 * probed on this slot
	 */
	s_ctrl->is_probe_succeed = 1;

	/*
	 * Create /dev/videoX node, comment for now until dummy /dev/videoX
	 * node is created and used by HAL
	 */

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		rc = msm_sensor_driver_create_v4l_subdev(s_ctrl);
	else
		rc = msm_sensor_driver_create_i2c_v4l_subdev(s_ctrl);
	if (rc < 0) {
		pr_err("failed: camera creat v4l2 rc %d", rc);
		goto CAMERA_POWER_DOWN;
	}

	memcpy(slave_info->subdev_name, s_ctrl->msm_sd.sd.entity.name,
		sizeof(slave_info->subdev_name));
	slave_info->is_probe_succeed = 1;
	slave_info->sensor_info.session_id = s_ctrl->sensordata->sensor_info->session_id;
	for (i = 0; i < SUB_MODULE_MAX; i++) {
		slave_info->sensor_info.subdev_id[i] =
			s_ctrl->sensordata->sensor_info->subdev_id[i];
		pr_err("sensor_subdev_id = %d i = %d\n",slave_info->sensor_info.subdev_id[i], i);
	}
	if (copy_to_user((void __user *)setting,
		(void *)slave_info, sizeof(*slave_info))) {
		pr_err("%s:%d copy failed\n", __func__, __LINE__);
		rc = -EFAULT;
	}

	/* Power down */
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);

	rc = msm_sensor_fill_slave_info_init_params(
		slave_info,
		s_ctrl->sensordata->sensor_info);
	if (rc < 0) {
		pr_err("%s Fill slave info failed", slave_info->sensor_name);
		goto FREE_CAMERA_INFO;
	}
	rc = msm_sensor_validate_slave_info(s_ctrl->sensordata->sensor_info);
	if (rc < 0) {
		pr_err("%s Validate slave info failed",
			slave_info->sensor_name);
		goto FREE_CAMERA_INFO;
	}
	/* Update sensor mount angle and position in media entity flag */
	mount_pos = s_ctrl->sensordata->sensor_info->position << 16;
	mount_pos = mount_pos | ((s_ctrl->sensordata->sensor_info->
		sensor_mount_angle / 90) << 8);
	s_ctrl->msm_sd.sd.entity.flags = mount_pos | MEDIA_ENT_FL_DEFAULT;

	/*Save sensor info*/
	s_ctrl->sensordata->cam_slave_info = slave_info;

	if(probe_fail) {
		rc = probe_fail;
	}

	return rc;

CAMERA_POWER_DOWN:
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
FREE_CAMERA_INFO:
	kfree(camera_info);
FREE_POWER_DOWN_SETTING:
	kfree(power_down_setting);
FREE_POWER_SETTING:
	kfree(power_setting);
FREE_SLAVE_INFO:
	kfree(slave_info);
	return rc;
}

static int32_t msm_sensor_driver_get_gpio_data(
	struct msm_camera_sensor_board_info *sensordata,
	struct device_node *of_node)
{
	int32_t                      rc = 0, i = 0;
	struct msm_camera_gpio_conf *gconf = NULL;
	uint16_t                    *gpio_array = NULL;
	uint16_t                     gpio_array_size = 0;

	/* Validate input paramters */
	if (!sensordata || !of_node) {
		pr_err("failed: invalid params sensordata %p of_node %p",
			sensordata, of_node);
		return -EINVAL;
	}

	sensordata->power_info.gpio_conf = kzalloc(
			sizeof(struct msm_camera_gpio_conf), GFP_KERNEL);
	if (!sensordata->power_info.gpio_conf) {
		pr_err("failed");
		return -ENOMEM;
	}
	gconf = sensordata->power_info.gpio_conf;

	gpio_array_size = of_gpio_count(of_node);
	CDBG("gpio count %d", gpio_array_size);
	if (!gpio_array_size)
		return 0;

	gpio_array = kzalloc(sizeof(uint16_t) * gpio_array_size, GFP_KERNEL);
	if (!gpio_array) {
		pr_err("failed");
		goto FREE_GPIO_CONF;
	}
	for (i = 0; i < gpio_array_size; i++) {
		gpio_array[i] = of_get_gpio(of_node, i);
		CDBG("gpio_array[%d] = %d", i, gpio_array[i]);
	}

	rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc < 0) {
		pr_err("failed");
		goto FREE_GPIO_CONF;
	}

	rc = msm_camera_init_gpio_pin_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc < 0) {
		pr_err("failed");
		goto FREE_GPIO_REQ_TBL;
	}

	kfree(gpio_array);
	return rc;

FREE_GPIO_REQ_TBL:
	kfree(sensordata->power_info.gpio_conf->cam_gpio_req_tbl);
FREE_GPIO_CONF:
	kfree(sensordata->power_info.gpio_conf);
	kfree(gpio_array);
	return rc;
}

static int32_t msm_sensor_driver_get_dt_data(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t                              rc = 0;
	struct msm_camera_sensor_board_info *sensordata = NULL;
	struct device_node                  *of_node = s_ctrl->of_node;
	uint32_t cell_id;

	s_ctrl->sensordata = kzalloc(sizeof(*sensordata), GFP_KERNEL);
	if (!s_ctrl->sensordata) {
		pr_err("failed: no memory");
		return -ENOMEM;
	}

	sensordata = s_ctrl->sensordata;

	/*
	 * Read cell index - this cell index will be the camera slot where
	 * this camera will be mounted
	 */
	rc = of_property_read_u32(of_node, "cell-index", &cell_id);
	if (rc < 0) {
		pr_err("failed: cell-index rc %d", rc);
		goto FREE_SENSOR_DATA;
	}
	s_ctrl->id = cell_id;

	/* Validate cell_id */
	if (cell_id >= MAX_CAMERAS) {
		pr_err("failed: invalid cell_id %d", cell_id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	/* Check whether g_sctrl is already filled for this cell_id */
	if (g_sctrl[cell_id]) {
		pr_err("failed: sctrl already filled for cell_id %d", cell_id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	/* Read subdev info */
	rc = msm_sensor_get_sub_module_index(of_node, &sensordata->sensor_info);
	if (rc < 0) {
		pr_err("failed");
		goto FREE_SENSOR_DATA;
	}

	/* Read vreg information */
	rc = msm_camera_get_dt_vreg_data(of_node,
		&sensordata->power_info.cam_vreg,
		&sensordata->power_info.num_vreg);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_dt_vreg_data rc %d", rc);
		goto FREE_SUB_MODULE_DATA;
	}

	/* Read gpio information */
	rc = msm_sensor_driver_get_gpio_data(sensordata, of_node);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_get_gpio_data rc %d", rc);
		goto FREE_VREG_DATA;
	}

	/* Get CCI master */
	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&s_ctrl->cci_i2c_master);
	CDBG("qcom,cci-master %d, rc %d", s_ctrl->cci_i2c_master, rc);
	if (rc < 0) {
		/* Set default master 0 */
		s_ctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	/* Get mount angle */
	if (0 > of_property_read_u32(of_node, "qcom,mount-angle",
		&sensordata->sensor_info->sensor_mount_angle)) {
		/* Invalidate mount angle flag */
		sensordata->sensor_info->is_mount_angle_valid = 0;
		sensordata->sensor_info->sensor_mount_angle = 0;
	} else {
		sensordata->sensor_info->is_mount_angle_valid = 1;
	}
	CDBG("%s qcom,mount-angle %d\n", __func__,
		sensordata->sensor_info->sensor_mount_angle);
	if (0 > of_property_read_u32(of_node, "qcom,sensor-position",
		&sensordata->sensor_info->position)) {
		CDBG("%s:%d Invalid sensor position\n", __func__, __LINE__);
		sensordata->sensor_info->position = INVALID_CAMERA_B;
	}
	if (0 > of_property_read_u32(of_node, "qcom,sensor-mode",
		&sensordata->sensor_info->modes_supported)) {
		CDBG("%s:%d Invalid sensor mode supported\n",
			__func__, __LINE__);
		sensordata->sensor_info->modes_supported = CAMERA_MODE_INVALID;
	}
	/* Get vdd-cx regulator */
	/*Optional property, don't return error if absent */
	of_property_read_string(of_node, "qcom,vdd-cx-name",
		&sensordata->misc_regulator);
	CDBG("qcom,misc_regulator %s", sensordata->misc_regulator);

	s_ctrl->set_mclk_23880000 = of_property_read_bool(of_node,
						"qcom,mclk-23880000");

	CDBG("%s qcom,mclk-23880000 = %d\n", __func__,
		s_ctrl->set_mclk_23880000);

#if defined (CONFIG_CAMERA_SYSFS_V2)
	/* camera information */
	if (cell_id == 0) {
		rc = msm_camera_get_dt_camera_info(of_node, rear_cam_info);
		if (rc < 0) {
			pr_err("failed: msm_camera_get_dt_camera_info rc %d", rc);
			goto FREE_VREG_DATA;
		}
	} else {
		rc = msm_camera_get_dt_camera_info(of_node, front_cam_info);
		if (rc < 0) {
			pr_err("failed: msm_camera_get_dt_camera_info rc %d", rc);
			goto FREE_VREG_DATA;
		}
	}
#endif
	return rc;

FREE_VREG_DATA:
	kfree(sensordata->power_info.cam_vreg);
FREE_SUB_MODULE_DATA:
	kfree(sensordata->sensor_info);
FREE_SENSOR_DATA:
	kfree(sensordata);
	return rc;
}

static int32_t msm_sensor_driver_parse(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t                   rc = 0;

	CDBG("Enter");
	/* Validate input parameters */


	/* Allocate memory for sensor_i2c_client */
	s_ctrl->sensor_i2c_client = kzalloc(sizeof(*s_ctrl->sensor_i2c_client),
		GFP_KERNEL);
	if (!s_ctrl->sensor_i2c_client) {
		pr_err("failed: no memory sensor_i2c_client %pK",
			s_ctrl->sensor_i2c_client);
		return -ENOMEM;
	}

	/* Allocate memory for mutex */
	s_ctrl->msm_sensor_mutex = kzalloc(sizeof(*s_ctrl->msm_sensor_mutex),
		GFP_KERNEL);
	if (!s_ctrl->msm_sensor_mutex) {
		pr_err("failed: no memory msm_sensor_mutex %pK",
			s_ctrl->msm_sensor_mutex);
		goto FREE_SENSOR_I2C_CLIENT;
	}

	/* Parse dt information and store in sensor control structure */
	rc = msm_sensor_driver_get_dt_data(s_ctrl);
	if (rc < 0) {
		pr_err("failed: rc %d", rc);
		goto FREE_MUTEX;
	}

	/* Initialize mutex */
	mutex_init(s_ctrl->msm_sensor_mutex);

	/* Initilize v4l2 subdev info */
	s_ctrl->sensor_v4l2_subdev_info = msm_sensor_driver_subdev_info;
	s_ctrl->sensor_v4l2_subdev_info_size =
		ARRAY_SIZE(msm_sensor_driver_subdev_info);

	/* Initialize default parameters */
	rc = msm_sensor_init_default_params(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_init_default_params rc %d", rc);
		goto FREE_DT_DATA;
	}

	/* Store sensor control structure in static database */
	g_sctrl[s_ctrl->id] = s_ctrl;
	pr_err("g_sctrl[%d] %pK", s_ctrl->id, g_sctrl[s_ctrl->id]);

	return rc;

FREE_DT_DATA:
	kfree(s_ctrl->sensordata->power_info.gpio_conf->gpio_num_info);
	kfree(s_ctrl->sensordata->power_info.gpio_conf->cam_gpio_req_tbl);
	kfree(s_ctrl->sensordata->power_info.gpio_conf);
	kfree(s_ctrl->sensordata->power_info.cam_vreg);
	kfree(s_ctrl->sensordata);
FREE_MUTEX:
	kfree(s_ctrl->msm_sensor_mutex);
FREE_SENSOR_I2C_CLIENT:
	kfree(s_ctrl->sensor_i2c_client);
	return rc;
}

static int32_t msm_sensor_driver_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl = NULL;


	/* Create sensor control structure */
	s_ctrl = kzalloc(sizeof(*s_ctrl), GFP_KERNEL);
	if (!s_ctrl)
		return -ENOMEM;

	platform_set_drvdata(pdev, s_ctrl);

	/* Initialize sensor device type */
	s_ctrl->sensor_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	s_ctrl->of_node = pdev->dev.of_node;

	rc = msm_sensor_driver_parse(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_parse rc %d", rc);
		goto FREE_S_CTRL;
	}

	/* Fill platform device */
	pdev->id = s_ctrl->id;
	s_ctrl->pdev = pdev;

	/* Fill device in power info */
	s_ctrl->sensordata->power_info.dev = &pdev->dev;

	return rc;
FREE_S_CTRL:
	kfree(s_ctrl);
	return rc;
}

static int32_t msm_sensor_driver_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;

	CDBG("\n\nEnter: msm_sensor_driver_i2c_probe");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	/* Create sensor control structure */
	s_ctrl = kzalloc(sizeof(*s_ctrl), GFP_KERNEL);
	if (!s_ctrl)
		return -ENOMEM;

	i2c_set_clientdata(client, s_ctrl);

	/* Initialize sensor device type */
	s_ctrl->sensor_device_type = MSM_CAMERA_I2C_DEVICE;
	s_ctrl->of_node = client->dev.of_node;

	rc = msm_sensor_driver_parse(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_parse rc %d", rc);
		goto FREE_S_CTRL;
	}

	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		s_ctrl->sensordata->power_info.dev = &client->dev;

	}

	return rc;
FREE_S_CTRL:
	kfree(s_ctrl);
	return rc;
}

static int msm_sensor_driver_i2c_remove(struct i2c_client *client)
{
	struct msm_sensor_ctrl_t  *s_ctrl = i2c_get_clientdata(client);

	pr_err("%s: sensor FREE\n", __func__);

	if (!s_ctrl) {
		pr_err("%s: sensor device is NULL\n", __func__);
		return 0;
	}

	g_sctrl[s_ctrl->id] = NULL;
	msm_sensor_free_sensor_data(s_ctrl);
	kfree(s_ctrl->msm_sensor_mutex);
	kfree(s_ctrl->sensor_i2c_client);
	kfree(s_ctrl);

	return 0;
}

static const struct i2c_device_id i2c_id[] = {
	{SENSOR_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver msm_sensor_driver_i2c = {
	.id_table = i2c_id,
	.probe  = msm_sensor_driver_i2c_probe,
	.remove = msm_sensor_driver_i2c_remove,
	.driver = {
		.name = SENSOR_DRIVER_I2C,
	},
};

static int __init msm_sensor_driver_init(void)
{
	int32_t rc = 0;

	CDBG("Enter");

#ifdef CONFIG_CAM_DISABLE_LPM_MODE
	if(poweroff_charging) {
		pr_err("%s : Camera is not probed in LPM mode", __func__);
		return 0;
	}
#endif

	rc = platform_driver_probe(&msm_sensor_platform_driver,
		msm_sensor_driver_platform_probe);
	if (!rc) {
		CDBG("probe success");
		return rc;
	} else {
		CDBG("probe i2c");
		rc = i2c_add_driver(&msm_sensor_driver_i2c);
	}

	return rc;
}


static void __exit msm_sensor_driver_exit(void)
{
	CDBG("Enter");
	platform_driver_unregister(&msm_sensor_platform_driver);
	i2c_del_driver(&msm_sensor_driver_i2c);
	return;
}

#if defined CONFIG_SEC_CAMERA_TUNING
int register_table_init(char *filename) {
	struct file *filp;
	char *dp;
	long lsize;
	loff_t pos;
	int ret;
	/*Get the current address space */
	mm_segment_t fs = get_fs();
	pr_err("%s %d", __func__, __LINE__);
	/*Set the current segment to kernel data segment */
	set_fs(get_ds());
	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_err("file open error %ld",(long) filp);
		return -1;
	}
	lsize = filp->f_path.dentry->d_inode->i_size;
	pr_err("size : %ld", lsize);
	dp = vmalloc(lsize);
	if (dp == NULL) {
		pr_err("Out of Memory");
		filp_close(filp, current->files);
		return -1;
	}
	pos = 0;
	memset(dp, 0, lsize);
	ret = vfs_read(filp, (char __user *)dp, lsize, &pos);
	if (ret != lsize) {
		pr_err("Failed to read file ret = %d\n", ret);
		vfree(dp);
		filp_close(filp, current->files);
		return -1;
	}
	/*close the file*/
	filp_close(filp, current->files);
	/*restore the previous address space*/
	set_fs(fs);
	pr_err("coming to if part of string compare sr352_regs_table");
	regs_table = dp;
	table_size = lsize;
	*((regs_table+ table_size) - 1) = '\0';
	return 0;
}

void register_table_exit(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (regs_table) {
		vfree(regs_table);
		regs_table = NULL;
	}
}

int register_read_from_sdcard (struct msm_camera_i2c_reg_conf *settings,
								struct msm_sensor_ctrl_t *s_ctrl,
								enum msm_camera_i2c_data_type data_type,
								char *name) {
	char *start, *end, *reg,reg_buf[7], data_buf[7];
	int rc,addr,value;
	addr=0;
	rc=0;
	value=0;

	if(data_type == MSM_CAMERA_I2C_BYTE_DATA) {
		*(reg_buf + 4) = '\0';
		*(data_buf + 4) = '\0';
	} else {
		*(reg_buf + 6) = '\0';
		*(data_buf + 6) = '\0';
	}
	if(regs_table == NULL) {
		pr_err("regs_table is null ");
		return -1;
	}
	pr_err("@@@ %s",name);
	start = strstr(regs_table, name);
	if (start == NULL){
		return -1;
	}
	end = strstr(start, "};");
	while (1) {
		/* Find Address */
		reg = strstr(start, "{0x");
		if ((reg == NULL) || (reg > end))
			break;
		/* Write Value to Address */
		if (reg != NULL) {
			if(data_type == MSM_CAMERA_I2C_BYTE_DATA) {
				memcpy(reg_buf, (reg + 1), 4);
				memcpy(data_buf, (reg + 7), 4);
			} else {
				memcpy(reg_buf, (reg + 1), 6);
				memcpy(data_buf, (reg + 9), 6);
			}
			if(kstrtoint(reg_buf, 16, &addr))
				pr_err("kstrtoint error .Please Align contents of the Header file!!") ;
			if(kstrtoint(data_buf, 16, &value))
				pr_err("kstrtoint error .Please Align contents of the Header file!!");
			if(data_type == MSM_CAMERA_I2C_BYTE_DATA) {
				start = (reg + 14);
				if (addr == 0xff){
					msleep(value);
				} else	{
					rc=s_ctrl->sensor_i2c_client->i2c_func_tbl->
						i2c_write(s_ctrl->sensor_i2c_client, addr,
						value,MSM_CAMERA_I2C_BYTE_DATA);
				}
			} else {
				start = (reg + 18);
				if (addr == 0xff){
					msleep(value);
				} else {
					rc=s_ctrl->sensor_i2c_client->i2c_func_tbl->
						i2c_write(s_ctrl->sensor_i2c_client, addr,
						value,MSM_CAMERA_I2C_WORD_DATA);
				}
			}
		}
	}
	pr_err("register_read_from_sdcard end!");
	return rc;
}

#endif

module_init(msm_sensor_driver_init);
module_exit(msm_sensor_driver_exit);
MODULE_DESCRIPTION("msm_sensor_driver");
MODULE_LICENSE("GPL v2");
