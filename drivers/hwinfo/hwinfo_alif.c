/* Copyright (c) 2025 Alif Semiconductor
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/drivers/hwinfo.h>
#include <string.h>
#include <zephyr/sys/byteorder.h>
#include <soc_common.h>
#include "se_service.h"

#if defined(CONFIG_RTSS_HP)
#define RESET_STATUS_REG            (AON_RTSS_HP_RESET)
#elif defined(CONFIG_RTSS_HE)
#define RESET_STATUS_REG            (AON_RTSS_HE_RESET)
#else
#error "Invalid CPU"
#endif

#define DEV_SERIAL_NUM_SIZE                     8
#define POR_OR_SESS_RESET                       BIT(0)
#define NSRST                                   BIT(1)
#define HOST_SYS_RESET_REQ_BY_SESS              BIT(3)
#define RESET_REQ_TO_THE_POWER_DOMAIN           BIT(4)

ssize_t z_impl_hwinfo_get_device_id(uint8_t *buffer, size_t length)
{
	get_device_revision_data_t dev_data;
	int ret;

	/* Input validation */
	if (!buffer || length == 0) {
		return -EINVAL;
	}

	/* Get device data from Secure Enclave */
	ret = se_service_system_get_device_data(&dev_data);
	if (ret) {
		return ret;
	}

	if (length > DEV_SERIAL_NUM_SIZE) {
		length = DEV_SERIAL_NUM_SIZE;
	}
	/* hwinfo returns raw ID bytes; callers perform their own encoding.
	 * Formatting eight bytes as hex into an eight-byte array overflowed it.
	 */
	memcpy(buffer, (const void *)dev_data.SerialN, length);

	return length;
}

int z_impl_hwinfo_get_reset_cause(uint32_t *cause)
{
	uint32_t flags = 0;
	uint32_t reason = sys_read32(RESET_STATUS_REG);

	if (reason & POR_OR_SESS_RESET) {
		flags |= RESET_POR;
	}
	if (reason & NSRST) {
		flags |= RESET_PIN;
	}
	if (reason & HOST_SYS_RESET_REQ_BY_SESS) {
		flags |= RESET_SOFTWARE;
	}
	if (reason & RESET_REQ_TO_THE_POWER_DOMAIN) {
		flags |= RESET_SOFTWARE;
	}
	*cause = flags;

	return 0;
}

int z_impl_hwinfo_clear_reset_cause(void)
{
	/* Read the set bits */
	uint32_t reason = sys_read32(RESET_STATUS_REG);

	/* Write them back to clear the reset */
	sys_write32(reason, RESET_STATUS_REG);

	return 0;
}

int z_impl_hwinfo_get_supported_reset_cause(uint32_t *supported)
{
	*supported = (RESET_POR | RESET_PIN | RESET_SOFTWARE);

	return 0;
}
