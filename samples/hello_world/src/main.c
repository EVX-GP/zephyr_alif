/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#endif

int main(void)
{
	int ret = 0;
	uint32_t tick = 0U;

	printf("Hello World! %s\n", CONFIG_BOARD);

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
	if (!gpio_is_ready_dt(&led)) {
		printf("LED device not ready\n");
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		printf("LED configure failed: %d\n", ret);
		return ret;
	}
#else
	printf("No led0 alias defined for this board\n");
#endif

	while (1) {
#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
		(void)gpio_pin_toggle_dt(&led);
#endif
		printf("Tick %u\n", tick++);
		k_msleep(500);
	}
}
