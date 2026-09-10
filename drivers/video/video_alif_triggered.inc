/* Proposed EVX Alif BSP extension. SPDX-License-Identifier: Apache-2.0
 * Included by video_alif.c before its video API entry points.
 * STOP is the completion boundary. VSYNC alone never releases a DMA buffer.
 * Single-core peripheral ownership; irq_lock serializes thread/ISR state.
 */
static struct alif_cpi_slot *triggered_slot(struct video_cam_data *data,
					 struct video_buffer *buffer)
{
	for (size_t i = 0; i < ALIF_CPI_OWNERS; ++i) {
		if (data->triggered.slots[i].buffer == buffer) {
			return &data->triggered.slots[i];
		}
	}
	return NULL;
}

static uint32_t triggered_time(struct alif_cpi_triggered_data *t)
{
	uint32_t now = k_cycle_get_32();
	/* Extend the CPU cycle counter before conversion. Direct conversion of
	 * a wrapping 32-bit cycle value would jump backwards during a 10s run. */
	t->elapsed_cycles += (uint32_t)(now - t->last_cycles);
	t->last_cycles = now;
	return (uint32_t)k_cyc_to_us_floor64(t->elapsed_cycles);
}

static void triggered_fail(const struct device *dev, int error)
{
	struct video_cam_data *data = dev->data;
	if (!data->triggered.stats.error) { data->triggered.stats.error = error; }
	sys_write32(0, DEVICE_MMIO_GET(dev) + CAM_INTR_ENA);
	sys_write32(0, DEVICE_MMIO_GET(dev) + CAM_CTRL);
	/* Keep the active buffer owned until STOP/BUSY quiescence is proven. */
}

static int triggered_health(const struct device *dev)
{
	const struct video_cam_config *config = dev->config;
	struct video_cam_data *data = dev->data;
	struct alif_csi_health health;
	int ret = video_get_ctrl(config->endpoint_dev, VIDEO_CID_ALIF_CSI_HEALTH, &health);
	if (ret) { return ret; }
	if (health.pending || health.events != data->triggered.csi_events) {
		data->triggered.stats.csi_errors += health.events - data->triggered.csi_events;
		if (health.pending) { data->triggered.stats.csi_errors++; }
		data->triggered.csi_events = health.events;
		return -EIO;
	}
	return 0;
}

static void triggered_arm(const struct device *dev, struct video_buffer *buffer)
{
	struct video_cam_data *data = dev->data;
	struct alif_cpi_slot *slot = triggered_slot(data, buffer);
	slot->owner = CPI_ACTIVE;
	data->triggered.active = slot;
	data->curr_vid_buf = (uint32_t)(uintptr_t)buffer->buffer;
	sys_write32(local_to_global(buffer->buffer), DEVICE_MMIO_GET(dev) + CAM_FRAME_ADDR);
	/* Initial reset happens once in start(). No workqueue, I2C, allocation,
	 * logging or per-frame reset on the interrupt rearm path. */
	sys_write32(CAM_CTRL_FIFO_CLK_SEL | CAM_CTRL_SNAPSHOT | CAM_CTRL_START,
		    DEVICE_MMIO_GET(dev) + CAM_CTRL);
}

static int triggered_enqueue(const struct device *dev, struct video_buffer *buffer)
{
	struct video_cam_data *data = dev->data;
	uint32_t bytes = data->current_format.pitch * data->current_format.height;
	if (!buffer || !buffer->buffer || !bytes || buffer->size < bytes ||
	    ((uintptr_t)buffer->buffer & 31U) || (bytes & 31U)) { return -EINVAL; }
	unsigned int key = irq_lock();
	struct alif_cpi_slot *slot = triggered_slot(data, buffer);
	if (data->triggered.stats.quarantined) { irq_unlock(key); return -EIO; }
	if (slot && slot->owner != CPI_CLIENT) { irq_unlock(key); return -EALREADY; }
	if (!slot) { slot = triggered_slot(data, NULL); }
	if (!slot) { irq_unlock(key); return -ENOSPC; }
	/* Caller prepares cache before enqueue. Once queued, CPU cannot touch
	 * this memory again until dequeue and cache invalidation. */
	slot->buffer = buffer;
	slot->owner = CPI_QUEUED;
	buffer->bytesused = 0;
	buffer->line_offset = 0;
	k_fifo_put(&data->fifo_in, buffer);
	irq_unlock(key);
	return 0;
}

static int triggered_start(const struct device *dev)
{
	struct video_cam_data *data = dev->data;
	struct alif_cpi_triggered_data *t = &data->triggered;
	const struct video_cam_config *config = dev->config;
	uintptr_t regs = DEVICE_MMIO_GET(dev);
	if (t->stats.error || t->stats.quarantined) { return t->stats.error ? t->stats.error : -EIO; }
	if (data->is_streaming || t->pipeline_started || (sys_read32(regs + CAM_CTRL) & CAM_CTRL_BUSY)) {
		return -EBUSY;
	}
	if (!config->axi_bus_ep || config->is_lpcam || config->interface != CAM_INTERFACE_SERIAL) {
		return -ENOTSUP;
	}
	unsigned int queued = 0;
	for (size_t i = 0; i < ALIF_CPI_OWNERS; ++i) { queued += t->slots[i].owner == CPI_QUEUED; }
	if (queued < 4) { return -ENOBUFS; }
	struct alif_csi_health health;
	int ret = video_get_ctrl(config->endpoint_dev, VIDEO_CID_ALIF_CSI_HEALTH, &health);
	if (ret) { return ret; }
	if (health.pending) { return -EIO; }
	t->csi_events = health.events;
	ret = video_stream_start(config->endpoint_dev);
	if (ret) { return ret; }
	t->pipeline_started = true;
	unsigned int key = irq_lock();
	t->stats = (struct alif_cpi_stats){ .version = ALIF_CPI_TRIGGERED_ABI };
	t->elapsed_cycles = 0;
	t->last_cycles = k_cycle_get_32();
	t->vsync_seen = false;
	data->is_streaming = true;
	sys_write32(0, regs + CAM_CTRL);
	sys_write32(CAM_CTRL_SW_RESET, regs + CAM_CTRL);
	sys_write32(0, regs + CAM_CTRL);
	sys_write32(UINT32_MAX, regs + CAM_INTR);
	sys_write32(INTR_STOP | INTR_VSYNC | INTR_BRESP_ERR | INTR_INFIFO_OVERRUN |
		    INTR_OUTFIFO_OVERRUN, regs + CAM_INTR_ENA);
	triggered_arm(dev, k_fifo_get(&data->fifo_in, K_NO_WAIT));
	irq_unlock(key);
	return 0;
}

static int triggered_stop(const struct device *dev)
{
	struct video_cam_data *data = dev->data;
	struct alif_cpi_triggered_data *t = &data->triggered;
	const struct video_cam_config *config = dev->config;
	uintptr_t regs = DEVICE_MMIO_GET(dev);
	unsigned int key = irq_lock();
	data->is_streaming = false;
	sys_write32(0, regs + CAM_INTR_ENA);
	sys_write32(0, regs + CAM_CTRL);
	irq_unlock(key);
	/* External REQ_EXP must already be stopped. Keep CSI/sensor alive while
	 * CPI finishes the in-flight frame; disabling IPI first can strand BUSY. */
	for (unsigned int i = 0; i < 20 && (sys_read32(regs + CAM_CTRL) & CAM_CTRL_BUSY); ++i) {
		k_msleep(1);
	}
	if (sys_read32(regs + CAM_CTRL) & CAM_CTRL_BUSY) {
		t->stats.quarantined = 1;
		triggered_fail(dev, -ETIMEDOUT);
		return -ETIMEDOUT; /* no reset/reuse/release of possibly live DMA */
	}
	int ret = t->pipeline_started ? video_stream_stop(config->endpoint_dev) : 0;
	if (ret) { t->stats.quarantined = 1; triggered_fail(dev, ret); return ret; }
	t->pipeline_started = false;
	data->curr_vid_buf = 0;
	return 0;
}

static int triggered_flush(const struct device *dev, bool cancel)
{
	struct video_cam_data *data = dev->data;
	if (!cancel && data->is_streaming) { return -EBUSY; }
	int ret = triggered_stop(dev);
	if (ret || data->triggered.stats.quarantined) { return ret ? ret : -EIO; }
	unsigned int key = irq_lock();
	struct video_buffer *buffer;
	while ((buffer = k_fifo_get(&data->fifo_in, K_NO_WAIT))) {
		triggered_slot(data, buffer)->owner = CPI_COMPLETE;
		buffer->bytesused = 0;
		k_fifo_put(&data->fifo_out, buffer);
	}
	if (data->triggered.active) {
		struct alif_cpi_slot *slot = data->triggered.active;
		slot->owner = CPI_COMPLETE;
		slot->buffer->bytesused = 0;
		k_fifo_put(&data->fifo_out, slot->buffer);
		data->triggered.active = NULL;
	}
	for (size_t i = 0; i < ALIF_CPI_OWNERS; ++i) {
		if (data->triggered.slots[i].owner == CPI_COMPLETE) {
			data->triggered.slots[i].buffer->bytesused = 0;
		}
	}
	irq_unlock(key);
	return 0;
}

static int triggered_dequeue(const struct device *dev, struct video_buffer **buffer, k_timeout_t timeout)
{
	struct video_cam_data *data = dev->data;
	if (data->is_streaming && data->triggered.stats.error) { return data->triggered.stats.error; }
	*buffer = k_fifo_get(&data->fifo_out, timeout);
	if (!*buffer) { return data->triggered.stats.error ? data->triggered.stats.error : -EAGAIN; }
	unsigned int key = irq_lock();
	triggered_slot(data, *buffer)->owner = CPI_CLIENT;
	irq_unlock(key);
	return 0;
}

static void triggered_isr(const struct device *dev)
{
	struct video_cam_data *data = dev->data;
	struct alif_cpi_triggered_data *t = &data->triggered;
	uintptr_t regs = DEVICE_MMIO_GET(dev);
	uint32_t entry = k_cycle_get_32();
	uint32_t status = sys_read32(regs + CAM_INTR) & sys_read32(regs + CAM_INTR_ENA);
	sys_write32(status, regs + CAM_INTR);
	if (!data->is_streaming || t->stats.error || !status) { return; }
	uint32_t now = triggered_time(t);
	if (status & (INTR_BRESP_ERR | INTR_INFIFO_OVERRUN | INTR_OUTFIFO_OVERRUN)) {
		t->stats.cpi_errors++;
		triggered_fail(dev, -EIO);
		return;
	}
	if (triggered_health(dev)) { triggered_fail(dev, -EIO); return; }
	if (status & INTR_VSYNC) {
		if (t->vsync_seen || (t->stats.vsyncs &&
		    ((uint32_t)(now - t->last_vsync_us) > t->config.period_us + t->config.tolerance_us ||
		     (uint32_t)(now - t->last_vsync_us) < t->config.period_us - t->config.tolerance_us))) {
			t->stats.timing_errors++;
			triggered_fail(dev, -ETIME);
			return;
		}
		t->last_vsync_us = now;
		t->vsync_seen = true;
		t->stats.vsyncs++;
	}
	if (!(status & INTR_STOP)) { return; }
	sys_write32(0, regs + CAM_CTRL);
	if (!t->active || !t->vsync_seen || (sys_read32(regs + CAM_CTRL) & CAM_CTRL_BUSY)) {
		t->stats.cpi_errors++;
		triggered_fail(dev, -EIO);
		return;
	}
	struct alif_cpi_slot *done = t->active;
	t->active = NULL;
	t->vsync_seen = false;
	done->sequence = t->stats.frames++;
	done->timestamp_us = now;
	done->buffer->timestamp = now / 1000U;
	done->buffer->bytesused = data->current_format.pitch * data->current_format.height;
	done->owner = CPI_COMPLETE;
	if (!done->sequence) { t->stats.first_completion_us = now; }
	t->stats.last_completion_us = now;
	struct video_buffer *next = k_fifo_get(&data->fifo_in, K_NO_WAIT);
	if (next) {
		triggered_arm(dev, next);
		uint32_t rearm = k_cyc_to_us_floor32(k_cycle_get_32() - entry);
		if (rearm > t->stats.max_rearm_us) { t->stats.max_rearm_us = rearm; }
		if (rearm >= t->config.tolerance_us) {
			t->stats.timing_errors++;
			triggered_fail(dev, -ETIME);
		}
	} else {
		t->stats.starvation++;
		triggered_fail(dev, -ENOBUFS);
	}
	k_fifo_put(&data->fifo_out, done->buffer);
}

static int triggered_set_config(const struct device *dev, const struct alif_cpi_triggered_config *cfg)
{
	struct video_cam_data *data = dev->data;
	if (!cfg || (cfg->period_us && (cfg->period_us < 500 || cfg->period_us > 10000 ||
	    cfg->tolerance_us < 10 || cfg->tolerance_us >= cfg->period_us / 2))) { return -EINVAL; }
	if (data->is_streaming || data->triggered.pipeline_started || data->triggered.active ||
	    !k_fifo_is_empty(&data->fifo_in) || !k_fifo_is_empty(&data->fifo_out)) { return -EBUSY; }
	if (data->triggered.stats.quarantined) { return -EIO; }
	data->triggered = (struct alif_cpi_triggered_data){ .config = *cfg };
	return 0;
}

static int triggered_get_stats(const struct device *dev, struct alif_cpi_stats *stats)
{
	struct video_cam_data *data = dev->data;
	unsigned int key = irq_lock();
	if (data->is_streaming && triggered_health(dev)) { triggered_fail(dev, -EIO); }
	*stats = data->triggered.stats;
	irq_unlock(key);
	return 0;
}

static int triggered_get_frame(const struct device *dev, struct alif_cpi_frame *frame)
{
	struct video_cam_data *data = dev->data;
	unsigned int key = irq_lock();
	struct alif_cpi_slot *slot = triggered_slot(data, frame->buffer);
	int ret = -EINVAL;
	if (slot && slot->owner == CPI_CLIENT && slot->buffer->bytesused && !data->triggered.stats.error) {
		frame->sequence = slot->sequence;
		frame->timestamp_us = slot->timestamp_us;
		ret = 0;
	}
	irq_unlock(key);
	return ret;
}
