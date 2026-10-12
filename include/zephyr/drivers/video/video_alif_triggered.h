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
#define VIDEO_CID_ALIF_CSI_GUARD     (VIDEO_CID_PRIVATE_BASE + 6)
#define VIDEO_CID_ALIF_CPI_INHIBIT   (VIDEO_CID_PRIVATE_BASE + 7)
#define ALIF_CPI_TRIGGERED_ABI 2U

struct alif_cpi_triggered_config {
	uint32_t period_us;
	uint32_t service_tolerance_us; /* IRQ-service jitter, NOT physical VSYNC timing */
	uint32_t isr_budget_us;        /* separate software handler budget */
};

struct alif_cpi_stats {
	uint32_t version, frames, vsyncs;
	uint32_t first_completion_us, last_completion_us, max_rearm_us;
	uint32_t cpi_errors, csi_errors, starvation, timing_errors;
	int32_t error;
	uint32_t quarantined;
	uint32_t max_isr_us, service_jitter_errors, isr_budget_errors, coalesced;
	uint32_t min_service_period_us, max_service_period_us;
	uint32_t min_queued, max_completed, max_dequeue_us, max_residence_us;
	uint32_t pixel_clock_requested_hz, pixel_clock_realized_hz, csi_status;
	uint32_t pending_latency_measured; /* zero: no physical event timestamp */
	uint32_t cpi_status, cpi_control_at_fault, cpi_config, csi_ipi_status;
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
	uint32_t pixel_clock_requested_hz, pixel_clock_realized_hz;
	uint32_t ipi_status;
};

#endif
