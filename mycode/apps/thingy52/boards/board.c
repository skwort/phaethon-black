/*
 * Code adapted from original board.c for Thingy52 by Aapo Vienamo
 */

#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

static const struct gpio_dt_spec ccs_gpio =
	GPIO_DT_SPEC_GET(DT_NODELABEL(ccs_pwr), enable_gpios);

static const struct gpio_dt_spec mpu_gpio =
	GPIO_DT_SPEC_GET(DT_NODELABEL(mpu_pwr), enable_gpios);

static int pwr_ctrl_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&ccs_gpio)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&ccs_gpio, GPIO_OUTPUT_HIGH);
	if (ret < 0) {
		return ret;
	}

	k_sleep(K_MSEC(1)); /* Wait for the rail to come up and stabilize */

    /* Init MPU power */
    if (!gpio_is_ready_dt(&mpu_gpio)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&mpu_gpio, GPIO_OUTPUT_HIGH);
	if (ret < 0) {
		return ret;
	}

    k_sleep(K_MSEC(100)); /* Wait for the rail to come up and stabilize */

	return 0;
}


#if CONFIG_SENSOR_INIT_PRIORITY <= CONFIG_BOARD_CCS_VDD_PWR_CTRL_INIT_PRIORITY
#error BOARD_CCS_VDD_PWR_CTRL_INIT_PRIORITY must be lower than SENSOR_INIT_PRIORITY
#endif

SYS_INIT(pwr_ctrl_init, POST_KERNEL,
	 CONFIG_BOARD_CCS_VDD_PWR_CTRL_INIT_PRIORITY);
