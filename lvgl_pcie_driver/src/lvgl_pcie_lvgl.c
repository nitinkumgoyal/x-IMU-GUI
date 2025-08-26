/*
 * LVGL PCIe Driver - LVGL Integration
 * 
 * This file implements the LVGL display driver interface that integrates
 * with the PCIe DMA engine and double buffer system for 4K video frames.
 */

#include "../include/lvgl_pcie_driver.h"

/**
 * lvgl_pcie_lvgl_init - Initialize LVGL integration
 * @priv: Device private data
 */
int lvgl_pcie_lvgl_init(struct lvgl_pcie_device *priv)
{
    struct dma_buffer *buf1, *buf2;
    size_t buf_size_pixels;

    lvgl_pcie_info(priv, "Initializing LVGL integration for 4K display\n");

    /* Get DMA buffers for LVGL */
    buf1 = lvgl_pcie_get_current_buffer(priv);
    buf2 = lvgl_pcie_get_pending_buffer(priv);

    if (!buf1 || !buf2 || !buf1->virt_addr || !buf2->virt_addr) {
        lvgl_pcie_err(priv, "DMA buffers not ready for LVGL integration\n");
        return -EINVAL;
    }

    /* Calculate buffer size in pixels */
    buf_size_pixels = priv->screen_width * priv->screen_height;

    /* Allocate LVGL color buffers pointing to DMA buffers */
    priv->draw_buf1 = (lv_color_t*)buf1->virt_addr;
    priv->draw_buf2 = (lv_color_t*)buf2->virt_addr;

    /* Initialize LVGL draw buffer */
    lv_disp_draw_buf_init(&priv->draw_buf, priv->draw_buf1, priv->draw_buf2, buf_size_pixels);

    /* Initialize display driver */
    lv_disp_drv_init(&priv->disp_drv);
    
    /* Set resolution */
    priv->disp_drv.hor_res = priv->screen_width;
    priv->disp_drv.ver_res = priv->screen_height;
    
    /* Set draw buffer */
    priv->disp_drv.draw_buf = &priv->draw_buf;
    
    /* Set callback functions */
    priv->disp_drv.flush_cb = lvgl_pcie_flush_cb;
    priv->disp_drv.rounder_cb = lvgl_pcie_rounder_cb;
    priv->disp_drv.set_px_cb = lvgl_pcie_set_px_cb;
    
    /* Set user data to access our device structure */
    priv->disp_drv.user_data = priv;
    
    /* Configure for direct mode (full refresh) for better 4K performance */
    priv->disp_drv.full_refresh = 1;
    priv->disp_drv.direct_mode = 1;
    
    /* Enable anti-aliasing for better 4K image quality */
    priv->disp_drv.antialiasing = 1;
    
    /* Set color format based on our buffer configuration */
    if (priv->color_depth == 32) {
        priv->disp_drv.color_chroma_key = LV_COLOR_CHROMA_KEY_HEX(0x00FF00); /* Green chroma key */
    }

    /* Register the display driver */
    priv->disp = lv_disp_drv_register(&priv->disp_drv);
    if (!priv->disp) {
        lvgl_pcie_err(priv, "Failed to register LVGL display driver\n");
        return -ENODEV;
    }

    /* Set as default display */
    lv_disp_set_default(priv->disp);

    lvgl_pcie_info(priv, "LVGL integration initialized successfully\n");
    lvgl_pcie_info(priv, "Display resolution: %dx%d, Color depth: %d bpp\n",
                   priv->screen_width, priv->screen_height, priv->color_depth);
    lvgl_pcie_info(priv, "Buffer size per frame: %.1f MB\n", 
                   (float)buf_size_pixels * sizeof(lv_color_t) / (1024 * 1024));

    return 0;
}

/**
 * lvgl_pcie_lvgl_cleanup - Cleanup LVGL integration
 * @priv: Device private data
 */
void lvgl_pcie_lvgl_cleanup(struct lvgl_pcie_device *priv)
{
    lvgl_pcie_info(priv, "Cleaning up LVGL integration\n");

    if (priv->disp) {
        /* Remove the display */
        lv_disp_remove(priv->disp);
        priv->disp = NULL;
    }

    /* Clear buffer pointers (don't free as they point to DMA buffers) */
    priv->draw_buf1 = NULL;
    priv->draw_buf2 = NULL;

    lvgl_pcie_info(priv, "LVGL integration cleanup completed\n");
}

/**
 * lvgl_pcie_flush_cb - LVGL flush callback
 * @disp_drv: Display driver
 * @area: Area to flush
 * @color_p: Color data pointer
 * 
 * Called by LVGL when a frame is ready to be displayed
 */
void lvgl_pcie_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    struct lvgl_pcie_device *priv = (struct lvgl_pcie_device*)disp_drv->user_data;
    struct dma_buffer *pending_buf;
    int buffer_idx;
    int ret;

    if (!priv || !priv->enabled) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    /* Get the buffer that was just rendered to */
    pending_buf = lvgl_pcie_get_pending_buffer(priv);
    if (!pending_buf) {
        lvgl_pcie_err(priv, "No pending buffer available for flush\n");
        lv_disp_flush_ready(disp_drv);
        return;
    }

    buffer_idx = lvgl_pcie_get_buffer_index(priv, pending_buf);
    if (buffer_idx < 0) {
        lvgl_pcie_err(priv, "Invalid buffer in flush callback\n");
        lv_disp_flush_ready(disp_drv);
        return;
    }

    lvgl_pcie_dbg(priv, "Flush callback: area (%d,%d)-(%d,%d), buffer %d\n",
                  area->x1, area->y1, area->x2, area->y2, buffer_idx);

    /* Check if this is a full screen update */
    bool full_screen = (area->x1 == 0 && area->y1 == 0 && 
                       area->x2 == (priv->screen_width - 1) && 
                       area->y2 == (priv->screen_height - 1));

    if (full_screen) {
        /* Full screen update - swap buffers and start DMA transfer */
        ret = lvgl_pcie_buffer_swap(priv);
        if (ret) {
            lvgl_pcie_err(priv, "Buffer swap failed in flush callback: %d\n", ret);
        } else {
            lvgl_pcie_dbg(priv, "Full frame flush: %.1f MB transferred\n",
                         (float)pending_buf->size / (1024 * 1024));
        }
    } else {
        /* Partial update - sync only the modified region */
        size_t x_bytes = (area->x2 - area->x1 + 1) * priv->bytes_per_pixel;
        size_t y_lines = area->y2 - area->y1 + 1;
        size_t region_size = x_bytes * y_lines;
        size_t offset = (area->y1 * priv->screen_width + area->x1) * priv->bytes_per_pixel;

        lvgl_pcie_dma_sync_for_device(priv, buffer_idx, offset, region_size);
        
        lvgl_pcie_dbg(priv, "Partial flush: %zu bytes at offset %zu\n", region_size, offset);
    }

    /* Tell LVGL we're done */
    lv_disp_flush_ready(disp_drv);
}

/**
 * lvgl_pcie_rounder_cb - LVGL rounder callback
 * @disp_drv: Display driver
 * @area: Area to round
 * 
 * Called by LVGL to round coordinates for optimal performance
 */
void lvgl_pcie_rounder_cb(lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    struct lvgl_pcie_device *priv = (struct lvgl_pcie_device*)disp_drv->user_data;
    
    if (!priv) {
        return;
    }

    /* For 4K video, align to 32-pixel boundaries for better DMA performance */
    const int align = 32;
    
    /* Round down start coordinates */
    area->x1 = (area->x1 / align) * align;
    area->y1 = (area->y1 / align) * align;
    
    /* Round up end coordinates */
    area->x2 = ((area->x2 + align) / align) * align - 1;
    area->y2 = ((area->y2 + align) / align) * align - 1;
    
    /* Clamp to screen boundaries */
    if (area->x2 >= priv->screen_width) area->x2 = priv->screen_width - 1;
    if (area->y2 >= priv->screen_height) area->y2 = priv->screen_height - 1;

    lvgl_pcie_dbg(priv, "Rounded area: (%d,%d)-(%d,%d)\n",
                  area->x1, area->y1, area->x2, area->y2);
}

/**
 * lvgl_pcie_set_px_cb - LVGL set pixel callback
 * @disp_drv: Display driver
 * @buf: Buffer pointer
 * @buf_w: Buffer width
 * @x: X coordinate
 * @y: Y coordinate
 * @color: Color to set
 * @opa: Opacity
 * 
 * Called by LVGL to set individual pixels (used for special cases)
 */
void lvgl_pcie_set_px_cb(lv_disp_drv_t *disp_drv, u8 *buf, lv_coord_t buf_w, 
                        lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa)
{
    struct lvgl_pcie_device *priv = (struct lvgl_pcie_device*)disp_drv->user_data;
    u8 *pixel_ptr;
    u32 color32;

    if (!priv || !buf) {
        return;
    }

    /* Validate coordinates */
    if (x >= buf_w || y >= priv->screen_height) {
        return;
    }

    /* Calculate pixel address */
    pixel_ptr = buf + (y * buf_w + x) * priv->bytes_per_pixel;

    /* Convert LVGL color to our format */
    if (priv->bytes_per_pixel == 4) {
        /* 32-bit RGBA */
        color32 = LV_COLOR_TO32(color) | ((u32)opa << 24);
        *(u32*)pixel_ptr = color32;
    } else if (priv->bytes_per_pixel == 2) {
        /* 16-bit RGB */
        *(u16*)pixel_ptr = lv_color_to16(color);
    } else {
        /* 8-bit grayscale */
        *pixel_ptr = (u8)(color.ch.red + color.ch.green + color.ch.blue) / 3;
    }
}

/**
 * lvgl_pcie_wait_for_frame_complete - Wait for current frame transfer to complete
 * @priv: Device private data
 * @timeout_ms: Timeout in milliseconds
 * 
 * Blocks until the current frame transfer to FPGA is complete
 */
int lvgl_pcie_wait_for_frame_complete(struct lvgl_pcie_device *priv, int timeout_ms)
{
    int ret;

    if (!priv->enabled) {
        return -ENODEV;
    }

    /* Wait for chunked transfer completion */
    ret = lvgl_pcie_dma_wait_chunked_complete(priv, timeout_ms);
    if (ret) {
        lvgl_pcie_err(priv, "Frame transfer timeout: %d\n", ret);
        return ret;
    }

    return 0;
}

/**
 * lvgl_pcie_get_frame_stats - Get frame transfer statistics
 * @priv: Device private data
 * @stats: Statistics structure to fill
 */
void lvgl_pcie_get_frame_stats(struct lvgl_pcie_device *priv, struct lvgl_pcie_stats *stats)
{
    size_t bytes_done, bytes_total;
    int chunks_done, chunks_total;

    if (!priv || !stats) {
        return;
    }

    /* Get basic statistics */
    stats->frames_sent = atomic64_read(&priv->frames_sent);
    stats->dma_errors = atomic64_read(&priv->dma_errors);
    stats->vsync_count = atomic64_read(&priv->vsync_count);
    stats->total_bytes_transferred = stats->frames_sent * BUFFER_SIZE;

    /* Get current transfer progress */
    lvgl_pcie_dma_get_chunked_progress(priv, &bytes_done, &bytes_total, 
                                      &chunks_done, &chunks_total);

    /* Calculate approximate FPS (simple estimate) */
    if (stats->vsync_count > 0) {
        stats->current_fps = min(60U, (u32)stats->vsync_count); /* Cap at 60 FPS */
    } else {
        stats->current_fps = 0;
    }

    /* Estimate average DMA time based on chunk size and completion rate */
    if (chunks_done > 0 && chunks_total > 0) {
        stats->avg_dma_time_us = (chunks_done * DMA_TIMEOUT_MS * 1000) / chunks_total;
    } else {
        stats->avg_dma_time_us = 0;
    }
}

/**
 * lvgl_pcie_force_refresh - Force a full screen refresh
 * @priv: Device private data
 * 
 * Triggers a complete screen refresh through LVGL
 */
void lvgl_pcie_force_refresh(struct lvgl_pcie_device *priv)
{
    if (!priv || !priv->disp || !priv->enabled) {
        return;
    }

    lvgl_pcie_info(priv, "Forcing full screen refresh\n");
    
    /* Invalidate the entire screen */
    lv_obj_invalidate(lv_scr_act());
    
    /* Trigger immediate refresh */
    lv_refr_now(priv->disp);
}