/*
 * LVGL PCIe Driver - DMA Buffer Management
 * 
 * This file implements DMA buffer allocation, management, and transfer
 * functions for the LVGL PCIe driver with double buffering support.
 */

#include "../include/lvgl_pcie_driver.h"
#include <linux/dma-direct.h>
#include <linux/highmem.h>

/**
 * lvgl_pcie_dma_init - Initialize DMA subsystem
 * @priv: Device private data
 * 
 * Allocates DMA buffers and sets up DMA engine
 */
int lvgl_pcie_dma_init(struct lvgl_pcie_device *priv)
{
    int i;
    size_t buffer_size = BUFFER_SIZE;
    
    lvgl_pcie_info(priv, "Initializing DMA subsystem\n");
    lvgl_pcie_info(priv, "Buffer size: %zu bytes, Count: %d\n", 
                   buffer_size, DMA_BUFFER_COUNT);

    /* Allocate DMA buffers */
    for (i = 0; i < DMA_BUFFER_COUNT; i++) {
        struct dma_buffer *buf = &priv->buffers[i];

        /* Allocate coherent DMA memory */
        buf->virt_addr = dma_alloc_coherent(priv->dev, buffer_size,
                                           &buf->phys_addr, GFP_KERNEL);
        if (!buf->virt_addr) {
            lvgl_pcie_err(priv, "Failed to allocate DMA buffer %d\n", i);
            goto err_free_buffers;
        }

        buf->size = buffer_size;
        buf->in_use = false;
        init_completion(&buf->done);

        lvgl_pcie_dbg(priv, "DMA buffer %d: virt=%p, phys=0x%llx, size=%zu\n",
                      i, buf->virt_addr, (unsigned long long)buf->phys_addr, buf->size);

        /* Clear buffer */
        memset(buf->virt_addr, 0, buffer_size);
    }

    /* Initialize buffer indices */
    priv->current_buffer = 0;
    priv->pending_buffer = 1;

    lvgl_pcie_info(priv, "DMA initialization completed successfully\n");
    return 0;

err_free_buffers:
    while (--i >= 0) {
        struct dma_buffer *buf = &priv->buffers[i];
        if (buf->virt_addr) {
            dma_free_coherent(priv->dev, buf->size,
                             buf->virt_addr, buf->phys_addr);
            buf->virt_addr = NULL;
        }
    }
    return -ENOMEM;
}

/**
 * lvgl_pcie_dma_cleanup - Cleanup DMA subsystem
 * @priv: Device private data
 */
void lvgl_pcie_dma_cleanup(struct lvgl_pcie_device *priv)
{
    int i;

    lvgl_pcie_info(priv, "Cleaning up DMA subsystem\n");

    /* Wait for any pending DMA operations */
    if (atomic_read(&priv->dma_pending)) {
        lvgl_pcie_warn(priv, "Waiting for pending DMA operations to complete\n");
        wait_event_timeout(priv->dma_wait, 
                          !atomic_read(&priv->dma_pending),
                          msecs_to_jiffies(DMA_TIMEOUT_MS * 2));
    }

    /* Free DMA buffers */
    for (i = 0; i < DMA_BUFFER_COUNT; i++) {
        struct dma_buffer *buf = &priv->buffers[i];
        if (buf->virt_addr) {
            dma_free_coherent(priv->dev, buf->size,
                             buf->virt_addr, buf->phys_addr);
            buf->virt_addr = NULL;
            buf->phys_addr = 0;
            buf->size = 0;
            buf->in_use = false;
        }
    }

    lvgl_pcie_info(priv, "DMA cleanup completed\n");
}

/**
 * lvgl_pcie_dma_transfer - Start DMA transfer to FPGA
 * @priv: Device private data
 * @buffer_idx: Buffer index to transfer
 * 
 * Initiates DMA transfer of the specified buffer to FPGA
 */
int lvgl_pcie_dma_transfer(struct lvgl_pcie_device *priv, int buffer_idx)
{
    struct dma_buffer *buf;
    u32 ctrl_reg;
    unsigned long flags;

    if (buffer_idx < 0 || buffer_idx >= DMA_BUFFER_COUNT) {
        lvgl_pcie_err(priv, "Invalid buffer index: %d\n", buffer_idx);
        return -EINVAL;
    }

    buf = &priv->buffers[buffer_idx];
    if (!buf->virt_addr) {
        lvgl_pcie_err(priv, "Buffer %d not allocated\n", buffer_idx);
        return -EINVAL;
    }

    if (buf->in_use) {
        lvgl_pcie_warn(priv, "Buffer %d already in use\n", buffer_idx);
        return -EBUSY;
    }

    /* Check if DMA engine is ready */
    ctrl_reg = lvgl_pcie_read_reg(priv, REG_STATUS);
    if (!(ctrl_reg & STATUS_READY)) {
        lvgl_pcie_err(priv, "DMA engine not ready (status=0x%x)\n", ctrl_reg);
        return -EBUSY;
    }

    lvgl_pcie_dbg(priv, "Starting DMA transfer for buffer %d (phys=0x%llx, size=%zu)\n",
                  buffer_idx, (unsigned long long)buf->phys_addr, buf->size);

    /* Mark buffer as in use */
    buf->in_use = true;
    reinit_completion(&buf->done);

    /* Set up DMA transfer */
    local_irq_save(flags);
    
    /* Configure DMA source address (host memory) */
    lvgl_pcie_write_reg(priv, REG_DMA_SRC_ADDR, (u32)(buf->phys_addr & 0xFFFFFFFF));
    if (sizeof(dma_addr_t) > 4) {
        lvgl_pcie_write_reg(priv, REG_DMA_SRC_ADDR + 4, 
                           (u32)(buf->phys_addr >> 32));
    }

    /* Configure DMA size */
    lvgl_pcie_write_reg(priv, REG_DMA_SIZE, buf->size);

    /* Ensure write ordering */
    wmb();

    /* Start DMA transfer */
    ctrl_reg = lvgl_pcie_read_reg(priv, REG_CONTROL);
    ctrl_reg |= CTRL_DMA_START;
    lvgl_pcie_write_reg(priv, REG_CONTROL, ctrl_reg);

    local_irq_restore(flags);

    /* Update statistics */
    atomic_inc(&priv->dma_pending);
    atomic64_inc(&priv->frames_sent);

    lvgl_pcie_dbg(priv, "DMA transfer initiated for buffer %d\n", buffer_idx);
    return 0;
}

/**
 * lvgl_pcie_dma_wait_complete - Wait for DMA transfer completion
 * @priv: Device private data
 * @timeout_ms: Timeout in milliseconds
 * 
 * Waits for the current DMA transfer to complete
 */
int lvgl_pcie_dma_wait_complete(struct lvgl_pcie_device *priv, int timeout_ms)
{
    int ret;

    if (!atomic_read(&priv->dma_pending)) {
        return 0; /* No pending DMA */
    }

    ret = wait_event_timeout(priv->dma_wait,
                            !atomic_read(&priv->dma_pending),
                            msecs_to_jiffies(timeout_ms));

    if (ret == 0) {
        lvgl_pcie_err(priv, "DMA transfer timeout after %d ms\n", timeout_ms);
        atomic64_inc(&priv->dma_errors);
        return -ETIMEDOUT;
    }

    lvgl_pcie_dbg(priv, "DMA transfer completed\n");
    return 0;
}

/**
 * lvgl_pcie_dma_abort - Abort current DMA transfer
 * @priv: Device private data
 */
int lvgl_pcie_dma_abort(struct lvgl_pcie_device *priv)
{
    u32 ctrl_reg;
    int i;

    lvgl_pcie_warn(priv, "Aborting DMA transfer\n");

    /* Stop DMA engine */
    ctrl_reg = lvgl_pcie_read_reg(priv, REG_CONTROL);
    ctrl_reg &= ~CTRL_DMA_START;
    lvgl_pcie_write_reg(priv, REG_CONTROL, ctrl_reg);

    /* Reset DMA engine */
    ctrl_reg |= CTRL_RESET;
    lvgl_pcie_write_reg(priv, REG_CONTROL, ctrl_reg);
    udelay(10);
    ctrl_reg &= ~CTRL_RESET;
    lvgl_pcie_write_reg(priv, REG_CONTROL, ctrl_reg);

    /* Mark all buffers as not in use */
    for (i = 0; i < DMA_BUFFER_COUNT; i++) {
        struct dma_buffer *buf = &priv->buffers[i];
        buf->in_use = false;
        complete(&buf->done);
    }

    /* Clear pending DMA flag */
    atomic_set(&priv->dma_pending, 0);
    wake_up(&priv->dma_wait);

    atomic64_inc(&priv->dma_errors);
    return 0;
}

/**
 * lvgl_pcie_dma_sync_for_device - Sync buffer for device access
 * @priv: Device private data
 * @buffer_idx: Buffer index
 * @offset: Offset within buffer
 * @size: Size to sync
 */
void lvgl_pcie_dma_sync_for_device(struct lvgl_pcie_device *priv, 
                                  int buffer_idx, size_t offset, size_t size)
{
    struct dma_buffer *buf;

    if (buffer_idx < 0 || buffer_idx >= DMA_BUFFER_COUNT)
        return;

    buf = &priv->buffers[buffer_idx];
    if (!buf->virt_addr)
        return;

    if (offset + size > buf->size)
        size = buf->size - offset;

    /* Sync cache if not coherent */
    if (!priv->dma_coherent) {
        dma_sync_single_for_device(priv->dev, buf->phys_addr + offset,
                                  size, DMA_TO_DEVICE);
    }
}

/**
 * lvgl_pcie_dma_sync_for_cpu - Sync buffer for CPU access
 * @priv: Device private data
 * @buffer_idx: Buffer index
 * @offset: Offset within buffer
 * @size: Size to sync
 */
void lvgl_pcie_dma_sync_for_cpu(struct lvgl_pcie_device *priv,
                               int buffer_idx, size_t offset, size_t size)
{
    struct dma_buffer *buf;

    if (buffer_idx < 0 || buffer_idx >= DMA_BUFFER_COUNT)
        return;

    buf = &priv->buffers[buffer_idx];
    if (!buf->virt_addr)
        return;

    if (offset + size > buf->size)
        size = buf->size - offset;

    /* Sync cache if not coherent */
    if (!priv->dma_coherent) {
        dma_sync_single_for_cpu(priv->dev, buf->phys_addr + offset,
                               size, DMA_FROM_DEVICE);
    }
}

/**
 * lvgl_pcie_dma_get_buffer_info - Get DMA buffer information
 * @priv: Device private data
 * @buffer_idx: Buffer index
 * @info: Buffer information structure to fill
 */
int lvgl_pcie_dma_get_buffer_info(struct lvgl_pcie_device *priv,
                                 int buffer_idx, struct dma_buffer **info)
{
    if (buffer_idx < 0 || buffer_idx >= DMA_BUFFER_COUNT)
        return -EINVAL;

    *info = &priv->buffers[buffer_idx];
    return 0;
}

/**
 * lvgl_pcie_dma_is_buffer_ready - Check if buffer is ready for transfer
 * @priv: Device private data
 * @buffer_idx: Buffer index
 */
bool lvgl_pcie_dma_is_buffer_ready(struct lvgl_pcie_device *priv, int buffer_idx)
{
    struct dma_buffer *buf;

    if (buffer_idx < 0 || buffer_idx >= DMA_BUFFER_COUNT)
        return false;

    buf = &priv->buffers[buffer_idx];
    return buf->virt_addr && !buf->in_use;
}

/**
 * lvgl_pcie_dma_complete_transfer - Mark DMA transfer as complete
 * @priv: Device private data
 * @buffer_idx: Buffer index that completed
 * @error: Error flag
 * 
 * Called from interrupt handler when DMA transfer completes
 */
void lvgl_pcie_dma_complete_transfer(struct lvgl_pcie_device *priv,
                                   int buffer_idx, bool error)
{
    struct dma_buffer *buf;

    if (buffer_idx < 0 || buffer_idx >= DMA_BUFFER_COUNT) {
        lvgl_pcie_err(priv, "Invalid buffer index in completion: %d\n", buffer_idx);
        return;
    }

    buf = &priv->buffers[buffer_idx];
    
    lvgl_pcie_dbg(priv, "DMA transfer completed for buffer %d (error=%d)\n",
                  buffer_idx, error);

    /* Mark buffer as available */
    buf->in_use = false;
    complete(&buf->done);

    /* Update counters */
    if (atomic_dec_and_test(&priv->dma_pending)) {
        wake_up(&priv->dma_wait);
    }

    if (error) {
        atomic64_inc(&priv->dma_errors);
        lvgl_pcie_err(priv, "DMA transfer error for buffer %d\n", buffer_idx);
    }
}