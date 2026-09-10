/* Proposed EVX Alif BSP extension. SPDX-License-Identifier: Apache-2.0 */
#ifndef ZEPHYR_VIDEO_ALIF_TRIGGERED_H
#define ZEPHYR_VIDEO_ALIF_TRIGGERED_H

#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>

/* Explicit opt-in, stopped pipeline only. Zero selects the existing driver.
 * This is repeated snapshot capture, not continuous A/B/C/D streaming.
 * Caller must stop external triggers before stopping/flushing this driver. */
#define VIDEO_CID_ALIF_CPI_TRIGGERED (VIDEO_CID_PRIVATE_BASE + 2)
#define VIDEO_CID_ALIF_CPI_STATS     (VIDEO_CID_PRIVATE_BASE + 3)
#define VIDEO_CID_ALIF_CPI_FRAME     (VIDEO_CID_PRIVATE_BASE + 4)
#define VIDEO_CID_ALIF_CSI_HEALTH    (VIDEO_CID_PRIVATE_BASE + 5)
#define ALIF_CPI_TRIGGERED_ABI 1U

struct alif_cpi_triggered_config {
	uint32_t period_us;
	uint32_t tolerance_us;
};

struct alif_cpi_stats {
	uint32_t version, frames, vsyncs;
	uint32_t first_completion_us, last_completion_us, max_rearm_us;
	uint32_t cpi_errors, csi_errors, starvation, timing_errors;
	int32_t error;
	uint32_t quarantined;
};

struct alif_cpi_frame {
	struct video_buffer *buffer; /* in: buffer returned by video_dequeue */
	uint32_t sequence;          /* out: zero based accepted completion */
	uint32_t timestamp_us;      /* out: HP IRQ time, relative to stream start */
};

struct alif_csi_health {
	uint32_t events;            /* monotonic ISR count; modulo 2^32 */
	uint32_t status;            /* OR of CSI main status since boot */
	uint32_t pending;           /* live main status; never cleared by get */
};

#endif
