/*
 * LVGL PCIe Driver - Double Buffer Management
 * 
 * This file implements double buffer management for seamless rendering
 * and display updates without tearing or flickering.
 */

#include "../include/lvgl_pcie_driver.h"

/**
 * lvgl_pcie_buffer_init - Initialize buffer management system
 * @priv: Device private data
 */
int lvgl_pcie_buffer_init(struct lvgl_pcie_device *priv)
{
    lvgl_pcie_info(priv, "Initializing double buffer management\n");

    /* Verify DMA buffers are allocated */
    if (!priv->buffers[0].virt_addr || !priv->buffers[1].virt_addr) {
        lvgl_pcie_err(priv, "DMA buffers not allocated\n");
        return -EINVAL;
    }

    /* Initialize buffer indices */
    priv->current_buffer = 0;    /* Buffer currently being displayed */
    priv->pending_buffer = 1;    /* Buffer being rendered to */

    /* Clear both buffers */
    memset(priv->buffers[0].virt_addr, 0, priv->buffers[0].size);
    memset(priv->buffers[1].virt_addr, 0, priv->buffers[1].size);

    /* Mark buffers as ready */
    priv->buffers[0].in_use = false;
    priv->buffers[1].in_use = false;

    lvgl_pcie_info(priv, "Double buffer management initialized\n");
    lvgl_pcie_info(priv, "Current buffer: %d, Pending buffer: %d\n",
                   priv->current_buffer, priv->pending_buffer);

    return 0;
}

/**
 * lvgl_pcie_buffer_cleanup - Cleanup buffer management system
 * @priv: Device private data
 */
void lvgl_pcie_buffer_cleanup(struct lvgl_pcie_device *priv)
{
    lvgl_pcie_info(priv, "Cleaning up buffer management\n");

    /* Wait for any pending operations on both buffers */
    mutex_lock(&priv->buffer_mutex);
    
    /* Reset buffer indices */
    priv->current_buffer = 0;
    priv->pending_buffer = 1;
    
    mutex_unlock(&priv->buffer_mutex);

    lvgl_pcie_info(priv, "Buffer management cleanup completed\n");
}

/**
 * lvgl_pcie_buffer_swap - Swap front and back buffers
 * @priv: Device private data
 * 
 * Atomically swaps the current and pending buffers
 */
int lvgl_pcie_buffer_swap(struct lvgl_pcie_device *priv)
{
    int old_current, old_pending;
    int ret = 0;

    mutex_lock(&priv->buffer_mutex);

    /* Check if device is enabled */
    if (!priv->enabled || !priv->initialized) {
        lvgl_pcie_warn(priv, "Device not ready for buffer swap\n");
        ret = -ENODEV;
        goto unlock;
    }

    /* Check if pending buffer is ready */
    if (priv->buffers[priv->pending_buffer].in_use) {
        lvgl_pcie_warn(priv, "Pending buffer %d still in use\n", priv->pending_buffer);
        ret = -EBUSY;
        goto unlock;
    }

    old_current = priv->current_buffer;
    old_pending = priv->pending_buffer;

    /* Swap buffers */
    priv->current_buffer = old_pending;
    priv->pending_buffer = old_current;

    lvgl_pcie_dbg(priv, "Buffer swap: current %d->%d, pending %d->%d\n",
                  old_current, priv->current_buffer,
                  old_pending, priv->pending_buffer);

    /* Sync the new current buffer for device access */
    lvgl_pcie_dma_sync_for_device(priv, priv->current_buffer, 0, 
                                 priv->buffers[priv->current_buffer].size);

    /* Initiate DMA transfer of the new current buffer using chunked transfer for 4K */
    ret = lvgl_pcie_dma_transfer_chunked(priv, priv->current_buffer);
    if (ret) {
        /* Revert swap on error */
        priv->current_buffer = old_current;
        priv->pending_buffer = old_pending;
        lvgl_pcie_err(priv, "Failed to start chunked DMA transfer after swap: %d\n", ret);
        goto unlock;
    }

unlock:
    mutex_unlock(&priv->buffer_mutex);
    return ret;
}

/**
 * lvgl_pcie_get_current_buffer - Get current display buffer
 * @priv: Device private data
 * 
 * Returns pointer to the buffer currently being displayed
 */
struct dma_buffer *lvgl_pcie_get_current_buffer(struct lvgl_pcie_device *priv)
{
    struct dma_buffer *buf = NULL;

    mutex_lock(&priv->buffer_mutex);
    if (priv->current_buffer >= 0 && priv->current_buffer < DMA_BUFFER_COUNT) {
        buf = &priv->buffers[priv->current_buffer];
    }
    mutex_unlock(&priv->buffer_mutex);

    return buf;
}

/**
 * lvgl_pcie_get_pending_buffer - Get pending render buffer
 * @priv: Device private data
 * 
 * Returns pointer to the buffer available for rendering
 */
struct dma_buffer *lvgl_pcie_get_pending_buffer(struct lvgl_pcie_device *priv)
{
    struct dma_buffer *buf = NULL;

    mutex_lock(&priv->buffer_mutex);
    if (priv->pending_buffer >= 0 && priv->pending_buffer < DMA_BUFFER_COUNT) {
        buf = &priv->buffers[priv->pending_buffer];
    }
    mutex_unlock(&priv->buffer_mutex);

    return buf;
}

/**
 * lvgl_pcie_get_buffer_index - Get buffer index from buffer pointer
 * @priv: Device private data
 * @buf: Buffer pointer
 * 
 * Returns the index of the given buffer
 */
int lvgl_pcie_get_buffer_index(struct lvgl_pcie_device *priv, 
                              struct dma_buffer *buf)
{
    int i;

    if (!buf)
        return -EINVAL;

    for (i = 0; i < DMA_BUFFER_COUNT; i++) {
        if (&priv->buffers[i] == buf)
            return i;
    }

    return -EINVAL;
}

/**
 * lvgl_pcie_buffer_copy_region - Copy region from one buffer to another
 * @priv: Device private data
 * @src_idx: Source buffer index
 * @dst_idx: Destination buffer index
 * @x: X coordinate
 * @y: Y coordinate
 * @width: Width in pixels
 * @height: Height in pixels
 */
int lvgl_pcie_buffer_copy_region(struct lvgl_pcie_device *priv,
                                int src_idx, int dst_idx,
                                u32 x, u32 y, u32 width, u32 height)
{
    struct dma_buffer *src_buf, *dst_buf;
    u8 *src_ptr, *dst_ptr;
    u32 bytes_per_line;
    u32 src_stride, dst_stride;
    u32 i;

    if (src_idx < 0 || src_idx >= DMA_BUFFER_COUNT ||
        dst_idx < 0 || dst_idx >= DMA_BUFFER_COUNT) {
        return -EINVAL;
    }

    src_buf = &priv->buffers[src_idx];
    dst_buf = &priv->buffers[dst_idx];

    if (!src_buf->virt_addr || !dst_buf->virt_addr) {
        return -EINVAL;
    }

    /* Validate region bounds */
    if (x + width > priv->screen_width || y + height > priv->screen_height) {
        return -EINVAL;
    }

    bytes_per_line = width * priv->bytes_per_pixel;
    src_stride = priv->screen_width * priv->bytes_per_pixel;
    dst_stride = priv->screen_width * priv->bytes_per_pixel;

    /* Calculate starting pointers */
    src_ptr = (u8 *)src_buf->virt_addr + (y * src_stride) + (x * priv->bytes_per_pixel);
    dst_ptr = (u8 *)dst_buf->virt_addr + (y * dst_stride) + (x * priv->bytes_per_pixel);

    /* Copy line by line */
    for (i = 0; i < height; i++) {
        memcpy(dst_ptr, src_ptr, bytes_per_line);
        src_ptr += src_stride;
        dst_ptr += dst_stride;
    }

    return 0;
}

/**
 * lvgl_pcie_buffer_clear_region - Clear a region in a buffer
 * @priv: Device private data
 * @buffer_idx: Buffer index
 * @x: X coordinate
 * @y: Y coordinate
 * @width: Width in pixels
 * @height: Height in pixels
 * @color: Clear color (32-bit RGBA)
 */
int lvgl_pcie_buffer_clear_region(struct lvgl_pcie_device *priv,
                                 int buffer_idx, u32 x, u32 y, 
                                 u32 width, u32 height, u32 color)
{
    struct dma_buffer *buf;
    u8 *ptr;
    u32 stride;
    u32 bytes_per_line;
    u32 i, j;

    if (buffer_idx < 0 || buffer_idx >= DMA_BUFFER_COUNT) {
        return -EINVAL;
    }

    buf = &priv->buffers[buffer_idx];
    if (!buf->virt_addr) {
        return -EINVAL;
    }

    /* Validate region bounds */
    if (x + width > priv->screen_width || y + height > priv->screen_height) {
        return -EINVAL;
    }

    stride = priv->screen_width * priv->bytes_per_pixel;
    bytes_per_line = width * priv->bytes_per_pixel;
    ptr = (u8 *)buf->virt_addr + (y * stride) + (x * priv->bytes_per_pixel);

    /* Fill region with color */
    for (i = 0; i < height; i++) {
        if (priv->bytes_per_pixel == 4) {
            /* 32-bit color */
            u32 *pixel_ptr = (u32 *)ptr;
            for (j = 0; j < width; j++) {
                pixel_ptr[j] = color;
            }
        } else if (priv->bytes_per_pixel == 2) {
            /* 16-bit color */
            u16 *pixel_ptr = (u16 *)ptr;
            u16 color16 = (u16)color;
            for (j = 0; j < width; j++) {
                pixel_ptr[j] = color16;
            }
        } else {
            /* Fallback: byte-wise fill */
            memset(ptr, color & 0xFF, bytes_per_line);
        }
        ptr += stride;
    }

    return 0;
}

/**
 * lvgl_pcie_buffer_get_pixel - Get pixel value from buffer
 * @priv: Device private data
 * @buffer_idx: Buffer index
 * @x: X coordinate
 * @y: Y coordinate
 * @color: Pointer to store pixel color
 */
int lvgl_pcie_buffer_get_pixel(struct lvgl_pcie_device *priv,
                              int buffer_idx, u32 x, u32 y, u32 *color)
{
    struct dma_buffer *buf;
    u8 *ptr;
    u32 stride;

    if (buffer_idx < 0 || buffer_idx >= DMA_BUFFER_COUNT) {
        return -EINVAL;
    }

    buf = &priv->buffers[buffer_idx];
    if (!buf->virt_addr) {
        return -EINVAL;
    }

    /* Validate coordinates */
    if (x >= priv->screen_width || y >= priv->screen_height) {
        return -EINVAL;
    }

    stride = priv->screen_width * priv->bytes_per_pixel;
    ptr = (u8 *)buf->virt_addr + (y * stride) + (x * priv->bytes_per_pixel);

    if (priv->bytes_per_pixel == 4) {
        *color = *(u32 *)ptr;
    } else if (priv->bytes_per_pixel == 2) {
        *color = *(u16 *)ptr;
    } else {
        *color = *ptr;
    }

    return 0;
}

/**
 * lvgl_pcie_buffer_set_pixel - Set pixel value in buffer
 * @priv: Device private data
 * @buffer_idx: Buffer index
 * @x: X coordinate
 * @y: Y coordinate
 * @color: Pixel color
 */
int lvgl_pcie_buffer_set_pixel(struct lvgl_pcie_device *priv,
                              int buffer_idx, u32 x, u32 y, u32 color)
{
    struct dma_buffer *buf;
    u8 *ptr;
    u32 stride;

    if (buffer_idx < 0 || buffer_idx >= DMA_BUFFER_COUNT) {
        return -EINVAL;
    }

    buf = &priv->buffers[buffer_idx];
    if (!buf->virt_addr) {
        return -EINVAL;
    }

    /* Validate coordinates */
    if (x >= priv->screen_width || y >= priv->screen_height) {
        return -EINVAL;
    }

    stride = priv->screen_width * priv->bytes_per_pixel;
    ptr = (u8 *)buf->virt_addr + (y * stride) + (x * priv->bytes_per_pixel);

    if (priv->bytes_per_pixel == 4) {
        *(u32 *)ptr = color;
    } else if (priv->bytes_per_pixel == 2) {
        *(u16 *)ptr = (u16)color;
    } else {
        *ptr = (u8)color;
    }

    return 0;
}

/**
 * lvgl_pcie_buffer_wait_vsync - Wait for vertical sync
 * @priv: Device private data
 * @timeout_ms: Timeout in milliseconds
 * 
 * Waits for the next vertical sync signal
 */
int lvgl_pcie_buffer_wait_vsync(struct lvgl_pcie_device *priv, int timeout_ms)
{
    int ret;

    if (!priv->enabled) {
        return -ENODEV;
    }

    ret = wait_for_completion_timeout(&priv->vsync_completion,
                                     msecs_to_jiffies(timeout_ms));

    if (ret == 0) {
        lvgl_pcie_warn(priv, "VSYNC timeout after %d ms\n", timeout_ms);
        return -ETIMEDOUT;
    }

    return 0;
}

/**
 * lvgl_pcie_buffer_signal_vsync - Signal vertical sync completion
 * @priv: Device private data
 * 
 * Called from interrupt handler when VSYNC occurs
 */
void lvgl_pcie_buffer_signal_vsync(struct lvgl_pcie_device *priv)
{
    atomic64_inc(&priv->vsync_count);
    complete(&priv->vsync_completion);
    
    /* Schedule VSYNC work for deferred processing */
    if (priv->wq) {
        queue_work(priv->wq, &priv->vsync_work);
    }

    lvgl_pcie_dbg(priv, "VSYNC signal received\n");
}

/**
 * lvgl_pcie_buffer_get_stats - Get buffer statistics
 * @priv: Device private data
 * @current_idx: Pointer to store current buffer index
 * @pending_idx: Pointer to store pending buffer index
 */
void lvgl_pcie_buffer_get_stats(struct lvgl_pcie_device *priv,
                               int *current_idx, int *pending_idx)
{
    mutex_lock(&priv->buffer_mutex);
    if (current_idx)
        *current_idx = priv->current_buffer;
    if (pending_idx)
        *pending_idx = priv->pending_buffer;
    mutex_unlock(&priv->buffer_mutex);
}