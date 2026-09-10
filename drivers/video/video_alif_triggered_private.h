/* Proposed EVX Alif BSP extension. SPDX-License-Identifier: Apache-2.0 */
#include <zephyr/drivers/video/video_alif_triggered.h>
#define ALIF_CPI_OWNERS 8U
enum alif_cpi_owner { CPI_UNUSED, CPI_QUEUED, CPI_ACTIVE, CPI_COMPLETE, CPI_CLIENT };
struct alif_cpi_slot {
	struct video_buffer *buffer;
	enum alif_cpi_owner owner;
	uint32_t sequence, timestamp_us;
};
struct alif_cpi_triggered_data {
	struct alif_cpi_triggered_config config;
	struct alif_cpi_stats stats;
	struct alif_cpi_slot slots[ALIF_CPI_OWNERS];
	struct alif_cpi_slot *active;
	uint32_t last_cycles, last_vsync_us, csi_events;
	uint64_t elapsed_cycles;
	bool vsync_seen, pipeline_started;
};
