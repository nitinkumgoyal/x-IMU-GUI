/*
 * LVGL PCIe Driver - Interrupt Handling and Synchronization
 * 
 * This file implements interrupt handling for DMA completion, VSYNC signals,
 * and other hardware events for the 4K video frame transfers.
 */

#include "../include/lvgl_pcie_driver.h"

/**
 * lvgl_pcie_interrupt - Main interrupt handler
 * @irq: IRQ number
 * @dev_id: Device ID (our private data)
 * 
 * Handles all hardware interrupts from the FPGA
 */
irqreturn_t lvgl_pcie_interrupt(int irq, void *dev_id)
{
    struct lvgl_pcie_device *priv = (struct lvgl_pcie_device*)dev_id;
    u32 int_status;
    irqreturn_t ret = IRQ_NONE;

    if (!priv || !priv->enabled) {
        return IRQ_NONE;
    }

    /* Read interrupt status register */
    int_status = lvgl_pcie_read_reg(priv, REG_INTERRUPT);
    if (!int_status) {
        return IRQ_NONE; /* Not our interrupt */
    }

    lvgl_pcie_dbg(priv, "Interrupt received: status=0x%x\n", int_status);

    /* Handle DMA completion interrupt */
    if (int_status & INT_DMA_COMPLETE) {
        lvgl_pcie_handle_dma_complete(priv);
        ret = IRQ_HANDLED;
    }

    /* Handle VSYNC interrupt */
    if (int_status & INT_VSYNC) {
        lvgl_pcie_handle_vsync(priv);
        ret = IRQ_HANDLED;
    }

    /* Handle error interrupt */
    if (int_status & INT_ERROR) {
        lvgl_pcie_handle_error(priv);
        ret = IRQ_HANDLED;
    }

    /* Clear interrupt status */
    lvgl_pcie_write_reg(priv, REG_INTERRUPT, int_status);

    return ret;
}

/**
 * lvgl_pcie_handle_dma_complete - Handle DMA completion interrupt
 * @priv: Device private data
 */
void lvgl_pcie_handle_dma_complete(struct lvgl_pcie_device *priv)
{
    u32 status_reg;
    bool error = false;

    lvgl_pcie_dbg(priv, "DMA completion interrupt\n");

    /* Check for DMA errors */
    status_reg = lvgl_pcie_read_reg(priv, REG_STATUS);
    if (status_reg & STATUS_ERROR) {
        error = true;
        lvgl_pcie_err(priv, "DMA error detected (status=0x%x)\n", status_reg);
    }

    /* Handle chunked DMA completion */
    lvgl_pcie_dma_chunk_complete(priv, error);

    /* Schedule DMA work for deferred processing */
    if (priv->wq) {
        queue_work(priv->wq, &priv->dma_work);
    }
}

/**
 * lvgl_pcie_handle_vsync - Handle VSYNC interrupt
 * @priv: Device private data
 */
void lvgl_pcie_handle_vsync(struct lvgl_pcie_device *priv)
{
    lvgl_pcie_dbg(priv, "VSYNC interrupt\n");

    /* Signal VSYNC completion */
    lvgl_pcie_buffer_signal_vsync(priv);
}

/**
 * lvgl_pcie_handle_error - Handle error interrupt
 * @priv: Device private data
 */
void lvgl_pcie_handle_error(struct lvgl_pcie_device *priv)
{
    u32 status_reg;

    status_reg = lvgl_pcie_read_reg(priv, REG_STATUS);
    lvgl_pcie_err(priv, "Hardware error interrupt (status=0x%x)\n", status_reg);

    /* Increment error statistics */
    atomic64_inc(&priv->dma_errors);

    /* Abort any ongoing transfers */
    lvgl_pcie_dma_abort_chunked(priv);

    /* Try to reset the hardware */
    lvgl_pcie_hw_reset(priv);
}

/**
 * lvgl_pcie_enable_interrupts - Enable hardware interrupts
 * @priv: Device private data
 */
void lvgl_pcie_enable_interrupts(struct lvgl_pcie_device *priv)
{
    u32 int_mask;

    if (!priv->irq_enabled) {
        /* Enable all relevant interrupts */
        int_mask = INT_DMA_COMPLETE | INT_VSYNC | INT_ERROR;
        lvgl_pcie_write_reg(priv, REG_INTERRUPT, int_mask);

        /* Enable interrupts in control register */
        u32 ctrl_reg = lvgl_pcie_read_reg(priv, REG_CONTROL);
        ctrl_reg |= CTRL_VSYNC_EN;
        lvgl_pcie_write_reg(priv, REG_CONTROL, ctrl_reg);

        priv->irq_enabled = true;
        lvgl_pcie_info(priv, "Interrupts enabled (mask=0x%x)\n", int_mask);
    }
}

/**
 * lvgl_pcie_disable_interrupts - Disable hardware interrupts
 * @priv: Device private data
 */
void lvgl_pcie_disable_interrupts(struct lvgl_pcie_device *priv)
{
    if (priv->irq_enabled) {
        /* Disable interrupts in control register */
        u32 ctrl_reg = lvgl_pcie_read_reg(priv, REG_CONTROL);
        ctrl_reg &= ~CTRL_VSYNC_EN;
        lvgl_pcie_write_reg(priv, REG_CONTROL, ctrl_reg);

        /* Clear interrupt mask */
        lvgl_pcie_write_reg(priv, REG_INTERRUPT, 0);

        priv->irq_enabled = false;
        lvgl_pcie_info(priv, "Interrupts disabled\n");
    }
}

/**
 * lvgl_pcie_vsync_work - VSYNC work queue handler
 * @work: Work structure
 * 
 * Deferred processing for VSYNC events
 */
void lvgl_pcie_vsync_work(struct work_struct *work)
{
    struct lvgl_pcie_device *priv = container_of(work, struct lvgl_pcie_device, vsync_work);

    if (!priv->enabled) {
        return;
    }

    lvgl_pcie_dbg(priv, "VSYNC work processing\n");

    /* This could trigger LVGL refresh or other frame-related processing */
    /* For now, just update statistics */
    
    /* Optionally force LVGL refresh on VSYNC */
    if (priv->disp) {
        /* Only refresh if we're not already in the middle of a transfer */
        if (!atomic_read(&priv->dma_pending)) {
            lvgl_pcie_dbg(priv, "Triggering LVGL refresh on VSYNC\n");
            lv_timer_handler(); /* Process LVGL timers */
        }
    }
}

/**
 * lvgl_pcie_dma_work - DMA work queue handler
 * @work: Work structure
 * 
 * Deferred processing for DMA completion events
 */
void lvgl_pcie_dma_work(struct work_struct *work)
{
    struct lvgl_pcie_device *priv = container_of(work, struct lvgl_pcie_device, dma_work);

    if (!priv->enabled) {
        return;
    }

    lvgl_pcie_dbg(priv, "DMA work processing\n");

    /* Performance monitoring and optimization could go here */
    /* Check if we need to adjust chunk sizes based on performance */
    
    /* Example: Adjust chunk size based on error rate */
    u64 total_frames = atomic64_read(&priv->frames_sent);
    u64 total_errors = atomic64_read(&priv->dma_errors);
    
    if (total_frames > 100) { /* After some frames */
        u64 error_rate = (total_errors * 100) / total_frames;
        if (error_rate > 5) { /* More than 5% error rate */
            lvgl_pcie_warn(priv, "High error rate detected: %llu%%, consider reducing chunk size\n", error_rate);
        }
    }
}

/**
 * lvgl_pcie_wait_for_idle - Wait for hardware to become idle
 * @priv: Device private data
 * @timeout_ms: Timeout in milliseconds
 */
int lvgl_pcie_wait_for_idle(struct lvgl_pcie_device *priv, int timeout_ms)
{
    unsigned long timeout = jiffies + msecs_to_jiffies(timeout_ms);
    u32 status;

    while (time_before(jiffies, timeout)) {
        status = lvgl_pcie_read_reg(priv, REG_STATUS);
        
        /* Check if hardware is ready and no DMA pending */
        if ((status & STATUS_READY) && !atomic_read(&priv->dma_pending)) {
            return 0;
        }

        /* Check for errors */
        if (status & STATUS_ERROR) {
            lvgl_pcie_err(priv, "Hardware error while waiting for idle\n");
            return -EIO;
        }

        cpu_relax();
        usleep_range(100, 1000); /* Sleep 100-1000 microseconds */
    }

    lvgl_pcie_err(priv, "Timeout waiting for hardware idle (status=0x%x)\n", status);
    return -ETIMEDOUT;
}

/**
 * lvgl_pcie_sync_with_hardware - Synchronize with hardware state
 * @priv: Device private data
 * 
 * Ensures software state matches hardware state
 */
int lvgl_pcie_sync_with_hardware(struct lvgl_pcie_device *priv)
{
    u32 status, version;
    
    if (!priv->enabled) {
        return -ENODEV;
    }

    /* Read hardware status */
    status = lvgl_pcie_read_reg(priv, REG_STATUS);
    version = lvgl_pcie_read_reg(priv, REG_VERSION);

    lvgl_pcie_dbg(priv, "Hardware sync: status=0x%x, version=0x%x\n", status, version);

    /* Check if hardware is in expected state */
    if (!(status & STATUS_READY)) {
        lvgl_pcie_warn(priv, "Hardware not ready, attempting reset\n");
        return lvgl_pcie_hw_reset(priv);
    }

    /* Verify version compatibility */
    u32 major_version = (version >> 16) & 0xFF;
    u32 minor_version = (version >> 8) & 0xFF;
    
    lvgl_pcie_info(priv, "FPGA firmware version: %d.%d\n", major_version, minor_version);

    /* Check version compatibility (adjust as needed) */
    if (major_version < 1) {
        lvgl_pcie_warn(priv, "Old firmware version, some features may not work\n");
    }

    return 0;
}

/**
 * lvgl_pcie_setup_msi_interrupts - Setup MSI interrupts if available
 * @priv: Device private data
 * 
 * Attempts to enable MSI interrupts for better performance
 */
int lvgl_pcie_setup_msi_interrupts(struct lvgl_pcie_device *priv)
{
    int ret;
    int nvecs = 1; /* We only need one interrupt vector */

    /* Try to enable MSI interrupts */
    ret = pci_alloc_irq_vectors(priv->pdev, nvecs, nvecs, PCI_IRQ_MSI);
    if (ret < 0) {
        lvgl_pcie_info(priv, "MSI interrupts not available, using legacy interrupts\n");
        return ret;
    }

    if (ret != nvecs) {
        lvgl_pcie_warn(priv, "Requested %d MSI vectors, got %d\n", nvecs, ret);
        pci_free_irq_vectors(priv->pdev);
        return -EINVAL;
    }

    /* Get the MSI IRQ number */
    priv->irq = pci_irq_vector(priv->pdev, 0);
    
    lvgl_pcie_info(priv, "MSI interrupts enabled, IRQ: %d\n", priv->irq);
    return 0;
}

/**
 * lvgl_pcie_cleanup_msi_interrupts - Cleanup MSI interrupts
 * @priv: Device private data
 */
void lvgl_pcie_cleanup_msi_interrupts(struct lvgl_pcie_device *priv)
{
    if (pci_dev_has_msi(priv->pdev)) {
        pci_free_irq_vectors(priv->pdev);
        lvgl_pcie_info(priv, "MSI interrupts cleaned up\n");
    }
}