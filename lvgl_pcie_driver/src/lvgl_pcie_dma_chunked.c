/*
 * LVGL PCIe Driver - Chunked DMA Transfer for 4K Video
 * 
 * This file implements chunked DMA transfers for large 4K video frames
 * to improve performance and reduce memory pressure.
 */

#include "../include/lvgl_pcie_driver.h"

/* Structure to track chunked transfer state */
struct chunked_transfer {
    int buffer_idx;
    size_t total_size;
    size_t bytes_transferred;
    size_t chunk_size;
    int chunks_total;
    int chunks_completed;
    dma_addr_t current_phys_addr;
    void *current_virt_addr;
    bool in_progress;
    struct completion done;
    int error_count;
};

static struct chunked_transfer g_transfer_state = {0};

/**
 * lvgl_pcie_dma_transfer_chunked - Start chunked DMA transfer for large frames
 * @priv: Device private data
 * @buffer_idx: Buffer index to transfer
 * 
 * Initiates chunked DMA transfer of a 4K video frame
 */
int lvgl_pcie_dma_transfer_chunked(struct lvgl_pcie_device *priv, int buffer_idx)
{
    struct dma_buffer *buf;
    struct chunked_transfer *transfer = &g_transfer_state;
    size_t optimal_chunk_size;

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

    if (transfer->in_progress) {
        lvgl_pcie_warn(priv, "Chunked transfer already in progress\n");
        return -EBUSY;
    }

    /* Calculate optimal chunk size based on frame size */
    optimal_chunk_size = DMA_CHUNK_SIZE;
    if (buf->size < DMA_CHUNK_SIZE) {
        optimal_chunk_size = buf->size;
    }

    /* Initialize transfer state */
    transfer->buffer_idx = buffer_idx;
    transfer->total_size = buf->size;
    transfer->bytes_transferred = 0;
    transfer->chunk_size = optimal_chunk_size;
    transfer->chunks_total = (buf->size + optimal_chunk_size - 1) / optimal_chunk_size;
    transfer->chunks_completed = 0;
    transfer->current_phys_addr = buf->phys_addr;
    transfer->current_virt_addr = buf->virt_addr;
    transfer->in_progress = true;
    transfer->error_count = 0;
    reinit_completion(&transfer->done);

    /* Mark buffer as in use */
    buf->in_use = true;

    lvgl_pcie_info(priv, "Starting chunked transfer: buffer %d, size %.1f MB, %d chunks of %.1f MB each\n",
                   buffer_idx, (float)buf->size / (1024 * 1024), 
                   transfer->chunks_total, (float)optimal_chunk_size / (1024 * 1024));

    /* Start first chunk transfer */
    return lvgl_pcie_dma_transfer_next_chunk(priv);
}

/**
 * lvgl_pcie_dma_transfer_next_chunk - Transfer the next chunk
 * @priv: Device private data
 */
int lvgl_pcie_dma_transfer_next_chunk(struct lvgl_pcie_device *priv)
{
    struct chunked_transfer *transfer = &g_transfer_state;
    u32 ctrl_reg;
    size_t remaining_bytes;
    size_t current_chunk_size;
    unsigned long flags;

    if (!transfer->in_progress) {
        return -EINVAL;
    }

    /* Calculate current chunk size */
    remaining_bytes = transfer->total_size - transfer->bytes_transferred;
    current_chunk_size = min(transfer->chunk_size, remaining_bytes);

    if (current_chunk_size == 0) {
        /* All chunks completed */
        return lvgl_pcie_dma_complete_chunked_transfer(priv, false);
    }

    /* Check if DMA engine is ready */
    ctrl_reg = lvgl_pcie_read_reg(priv, REG_STATUS);
    if (!(ctrl_reg & STATUS_READY)) {
        lvgl_pcie_err(priv, "DMA engine not ready (status=0x%x)\n", ctrl_reg);
        return -EBUSY;
    }

    lvgl_pcie_dbg(priv, "Transferring chunk %d/%d: offset 0x%zx, size %zu bytes\n",
                  transfer->chunks_completed + 1, transfer->chunks_total,
                  transfer->bytes_transferred, current_chunk_size);

    /* Sync current chunk for device access */
    if (!priv->dma_coherent) {
        dma_sync_single_for_device(priv->dev, 
                                  transfer->current_phys_addr,
                                  current_chunk_size, DMA_TO_DEVICE);
    }

    /* Set up DMA transfer for current chunk */
    local_irq_save(flags);
    
    /* Configure DMA source address */
    lvgl_pcie_write_reg(priv, REG_DMA_SRC_ADDR, 
                       (u32)(transfer->current_phys_addr & 0xFFFFFFFF));
    if (sizeof(dma_addr_t) > 4) {
        lvgl_pcie_write_reg(priv, REG_DMA_SRC_ADDR + 4, 
                           (u32)(transfer->current_phys_addr >> 32));
    }

    /* Configure DMA size for current chunk */
    lvgl_pcie_write_reg(priv, REG_DMA_SIZE, current_chunk_size);

    /* Set chunk information for FPGA */
    lvgl_pcie_write_reg(priv, REG_DMA_CTRL, 
                       (transfer->chunks_completed << 16) | transfer->chunks_total);

    /* Ensure write ordering */
    wmb();

    /* Start DMA transfer */
    ctrl_reg = lvgl_pcie_read_reg(priv, REG_CONTROL);
    ctrl_reg |= CTRL_DMA_START;
    lvgl_pcie_write_reg(priv, REG_CONTROL, ctrl_reg);

    local_irq_restore(flags);

    /* Update statistics */
    atomic_inc(&priv->dma_pending);

    return 0;
}

/**
 * lvgl_pcie_dma_chunk_complete - Handle completion of a single chunk
 * @priv: Device private data
 * @error: Error flag
 */
void lvgl_pcie_dma_chunk_complete(struct lvgl_pcie_device *priv, bool error)
{
    struct chunked_transfer *transfer = &g_transfer_state;
    size_t current_chunk_size;

    if (!transfer->in_progress) {
        lvgl_pcie_warn(priv, "Chunk completion but no transfer in progress\n");
        return;
    }

    if (error) {
        transfer->error_count++;
        lvgl_pcie_err(priv, "Error in chunk %d/%d\n", 
                      transfer->chunks_completed + 1, transfer->chunks_total);
        
        /* Abort on too many errors */
        if (transfer->error_count >= 3) {
            lvgl_pcie_err(priv, "Too many chunk errors, aborting transfer\n");
            lvgl_pcie_dma_complete_chunked_transfer(priv, true);
            return;
        }
    }

    /* Calculate current chunk size */
    current_chunk_size = min(transfer->chunk_size, 
                            transfer->total_size - transfer->bytes_transferred);

    /* Update transfer state */
    transfer->bytes_transferred += current_chunk_size;
    transfer->chunks_completed++;
    transfer->current_phys_addr += current_chunk_size;
    transfer->current_virt_addr = (u8*)transfer->current_virt_addr + current_chunk_size;

    /* Update statistics */
    if (atomic_dec_and_test(&priv->dma_pending)) {
        wake_up(&priv->dma_wait);
    }

    lvgl_pcie_dbg(priv, "Chunk %d/%d completed (%.1f%% done)\n",
                  transfer->chunks_completed, transfer->chunks_total,
                  (float)transfer->bytes_transferred * 100.0 / transfer->total_size);

    /* Check if transfer is complete */
    if (transfer->bytes_transferred >= transfer->total_size) {
        lvgl_pcie_dma_complete_chunked_transfer(priv, false);
    } else if (!error) {
        /* Continue with next chunk */
        int ret = lvgl_pcie_dma_transfer_next_chunk(priv);
        if (ret) {
            lvgl_pcie_err(priv, "Failed to start next chunk: %d\n", ret);
            lvgl_pcie_dma_complete_chunked_transfer(priv, true);
        }
    }
}

/**
 * lvgl_pcie_dma_complete_chunked_transfer - Complete chunked transfer
 * @priv: Device private data
 * @error: Error flag
 */
int lvgl_pcie_dma_complete_chunked_transfer(struct lvgl_pcie_device *priv, bool error)
{
    struct chunked_transfer *transfer = &g_transfer_state;
    struct dma_buffer *buf;

    if (!transfer->in_progress) {
        return 0;
    }

    buf = &priv->buffers[transfer->buffer_idx];

    lvgl_pcie_info(priv, "Chunked transfer completed: buffer %d, %.1f MB transferred, %d chunks, %s\n",
                   transfer->buffer_idx, 
                   (float)transfer->bytes_transferred / (1024 * 1024),
                   transfer->chunks_completed,
                   error ? "WITH ERRORS" : "SUCCESS");

    /* Mark buffer as available */
    buf->in_use = false;
    
    /* Reset transfer state */
    transfer->in_progress = false;
    complete(&transfer->done);

    /* Update statistics */
    if (error) {
        atomic64_inc(&priv->dma_errors);
    } else {
        atomic64_inc(&priv->frames_sent);
    }

    /* Clear any remaining pending DMA */
    atomic_set(&priv->dma_pending, 0);
    wake_up(&priv->dma_wait);

    return 0;
}

/**
 * lvgl_pcie_dma_wait_chunked_complete - Wait for chunked transfer completion
 * @priv: Device private data
 * @timeout_ms: Timeout in milliseconds
 */
int lvgl_pcie_dma_wait_chunked_complete(struct lvgl_pcie_device *priv, int timeout_ms)
{
    struct chunked_transfer *transfer = &g_transfer_state;
    int ret;

    if (!transfer->in_progress) {
        return 0; /* No transfer in progress */
    }

    ret = wait_for_completion_timeout(&transfer->done, msecs_to_jiffies(timeout_ms));

    if (ret == 0) {
        lvgl_pcie_err(priv, "Chunked transfer timeout after %d ms\n", timeout_ms);
        lvgl_pcie_err(priv, "Progress: %zu/%zu bytes (%.1f%%), %d/%d chunks\n",
                      transfer->bytes_transferred, transfer->total_size,
                      (float)transfer->bytes_transferred * 100.0 / transfer->total_size,
                      transfer->chunks_completed, transfer->chunks_total);
        
        /* Abort the transfer */
        lvgl_pcie_dma_complete_chunked_transfer(priv, true);
        return -ETIMEDOUT;
    }

    return 0;
}

/**
 * lvgl_pcie_dma_abort_chunked - Abort chunked transfer
 * @priv: Device private data
 */
int lvgl_pcie_dma_abort_chunked(struct lvgl_pcie_device *priv)
{
    struct chunked_transfer *transfer = &g_transfer_state;
    
    if (!transfer->in_progress) {
        return 0;
    }

    lvgl_pcie_warn(priv, "Aborting chunked transfer at chunk %d/%d\n",
                   transfer->chunks_completed, transfer->chunks_total);

    return lvgl_pcie_dma_complete_chunked_transfer(priv, true);
}

/**
 * lvgl_pcie_dma_get_chunked_progress - Get transfer progress
 * @priv: Device private data
 * @bytes_done: Pointer to store bytes transferred
 * @bytes_total: Pointer to store total bytes
 * @chunks_done: Pointer to store chunks completed
 * @chunks_total: Pointer to store total chunks
 */
void lvgl_pcie_dma_get_chunked_progress(struct lvgl_pcie_device *priv,
                                       size_t *bytes_done, size_t *bytes_total,
                                       int *chunks_done, int *chunks_total)
{
    struct chunked_transfer *transfer = &g_transfer_state;

    if (bytes_done) *bytes_done = transfer->bytes_transferred;
    if (bytes_total) *bytes_total = transfer->total_size;
    if (chunks_done) *chunks_done = transfer->chunks_completed;
    if (chunks_total) *chunks_total = transfer->chunks_total;
}