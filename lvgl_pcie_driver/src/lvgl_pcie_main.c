/*
 * LVGL PCIe Driver with Double Buffer and DMA Support
 * 
 * This driver provides LVGL display support through PCIe interface
 * with DMA transfers to FPGA and double buffering for smooth rendering.
 * 
 * Author: LVGL PCIe Driver
 * Version: 1.0.0
 */

#include "../include/lvgl_pcie_driver.h"

/* Global variables */
struct lvgl_pcie_device *g_devices[MAX_DEVICES];
int g_device_count = 0;
DEFINE_MUTEX(g_device_mutex);

/* PCI device ID table */
static const struct pci_device_id lvgl_pcie_id_table[] = {
    { PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, lvgl_pcie_id_table);

/* Character device file operations */
static const struct file_operations lvgl_pcie_fops = {
    .owner = THIS_MODULE,
    .open = lvgl_pcie_open,
    .release = lvgl_pcie_release,
    .read = lvgl_pcie_read,
    .write = lvgl_pcie_write,
    .unlocked_ioctl = lvgl_pcie_ioctl,
    .mmap = lvgl_pcie_mmap,
};

/* PCIe driver structure */
static struct pci_driver lvgl_pcie_driver = {
    .name = DRIVER_NAME,
    .id_table = lvgl_pcie_id_table,
    .probe = lvgl_pcie_probe,
    .remove = lvgl_pcie_remove,
    .suspend = lvgl_pcie_suspend,
    .resume = lvgl_pcie_resume,
};

/* Device class for automatic device node creation */
static struct class *lvgl_pcie_class;
static dev_t lvgl_pcie_devt;

/**
 * lvgl_pcie_probe - PCIe device probe function
 * @pdev: PCI device
 * @id: PCI device ID
 * 
 * Called when a matching PCIe device is found
 */
static int lvgl_pcie_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct lvgl_pcie_device *priv;
    int ret;
    int device_index;

    dev_info(&pdev->dev, "Probing LVGL PCIe device\n");

    /* Find available device slot */
    mutex_lock(&g_device_mutex);
    for (device_index = 0; device_index < MAX_DEVICES; device_index++) {
        if (!g_devices[device_index])
            break;
    }
    
    if (device_index >= MAX_DEVICES) {
        mutex_unlock(&g_device_mutex);
        dev_err(&pdev->dev, "Maximum number of devices (%d) reached\n", MAX_DEVICES);
        return -ENODEV;
    }
    mutex_unlock(&g_device_mutex);

    /* Allocate private device structure */
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->pdev = pdev;
    priv->dev = &pdev->dev;
    
    /* Initialize default display configuration */
    priv->screen_width = DEFAULT_SCREEN_WIDTH;
    priv->screen_height = DEFAULT_SCREEN_HEIGHT;
    priv->color_depth = DEFAULT_COLOR_DEPTH;
    priv->bytes_per_pixel = BYTES_PER_PIXEL;
    
    /* Initialize synchronization objects */
    mutex_init(&priv->buffer_mutex);
    init_waitqueue_head(&priv->dma_wait);
    init_completion(&priv->vsync_completion);
    atomic_set(&priv->dma_pending, 0);
    
    /* Initialize statistics */
    atomic64_set(&priv->frames_sent, 0);
    atomic64_set(&priv->dma_errors, 0);
    atomic64_set(&priv->vsync_count, 0);

    /* Set PCI driver data */
    pci_set_drvdata(pdev, priv);

    /* Enable PCI device */
    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to enable PCI device: %d\n", ret);
        goto err_free_priv;
    }

    /* Set PCI master */
    pci_set_master(pdev);

    /* Set DMA mask */
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if (ret) {
        ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
        if (ret) {
            dev_err(&pdev->dev, "Failed to set DMA mask: %d\n", ret);
            goto err_disable_device;
        }
    }
    priv->dma_coherent = dma_get_cache_alignment() == 1;

    /* Request memory regions */
    ret = pci_request_regions(pdev, DRIVER_NAME);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request PCI regions: %d\n", ret);
        goto err_disable_device;
    }

    /* Map BAR0 for MMIO */
    priv->mmio_start = pci_resource_start(pdev, 0);
    priv->mmio_len = pci_resource_len(pdev, 0);
    
    if (!priv->mmio_start || !priv->mmio_len) {
        dev_err(&pdev->dev, "Invalid BAR0 configuration\n");
        ret = -EINVAL;
        goto err_release_regions;
    }

    priv->mmio = pci_ioremap_bar(pdev, 0);
    if (!priv->mmio) {
        dev_err(&pdev->dev, "Failed to map BAR0\n");
        ret = -ENOMEM;
        goto err_release_regions;
    }

    /* Initialize hardware */
    ret = lvgl_pcie_hw_init(priv);
    if (ret) {
        dev_err(&pdev->dev, "Hardware initialization failed: %d\n", ret);
        goto err_unmap;
    }

    /* Initialize DMA buffers */
    ret = lvgl_pcie_dma_init(priv);
    if (ret) {
        dev_err(&pdev->dev, "DMA initialization failed: %d\n", ret);
        goto err_hw_cleanup;
    }

    /* Initialize buffer management */
    ret = lvgl_pcie_buffer_init(priv);
    if (ret) {
        dev_err(&pdev->dev, "Buffer initialization failed: %d\n", ret);
        goto err_dma_cleanup;
    }

    /* Create work queue */
    priv->wq = create_singlethread_workqueue("lvgl_pcie_wq");
    if (!priv->wq) {
        dev_err(&pdev->dev, "Failed to create work queue\n");
        ret = -ENOMEM;
        goto err_buffer_cleanup;
    }

    /* Initialize work structures */
    INIT_WORK(&priv->vsync_work, lvgl_pcie_vsync_work);
    INIT_WORK(&priv->dma_work, lvgl_pcie_dma_work);

    /* Request IRQ */
    priv->irq = pdev->irq;
    ret = devm_request_irq(&pdev->dev, priv->irq, lvgl_pcie_interrupt,
                          IRQF_SHARED, DRIVER_NAME, priv);
    if (ret) {
        dev_err(&pdev->dev, "Failed to request IRQ %d: %d\n", priv->irq, ret);
        goto err_destroy_wq;
    }

    /* Initialize character device */
    ret = lvgl_pcie_cdev_init(priv);
    if (ret) {
        dev_err(&pdev->dev, "Character device initialization failed: %d\n", ret);
        goto err_destroy_wq;
    }

    /* Initialize LVGL integration */
    ret = lvgl_pcie_lvgl_init(priv);
    if (ret) {
        dev_err(&pdev->dev, "LVGL initialization failed: %d\n", ret);
        goto err_cdev_cleanup;
    }

    /* Enable interrupts */
    lvgl_pcie_enable_interrupts(priv);

    /* Register device globally */
    mutex_lock(&g_device_mutex);
    g_devices[device_index] = priv;
    g_device_count++;
    mutex_unlock(&g_device_mutex);

    priv->initialized = true;
    priv->enabled = true;

    dev_info(&pdev->dev, "LVGL PCIe device initialized successfully (device %d)\n", device_index);
    dev_info(&pdev->dev, "Display: %dx%d, %d bpp, Buffer size: %zu bytes\n",
             priv->screen_width, priv->screen_height, priv->color_depth,
             BUFFER_SIZE);

    return 0;

err_cdev_cleanup:
    lvgl_pcie_cdev_cleanup(priv);
err_destroy_wq:
    destroy_workqueue(priv->wq);
err_buffer_cleanup:
    lvgl_pcie_buffer_cleanup(priv);
err_dma_cleanup:
    lvgl_pcie_dma_cleanup(priv);
err_hw_cleanup:
    lvgl_pcie_hw_cleanup(priv);
err_unmap:
    iounmap(priv->mmio);
err_release_regions:
    pci_release_regions(pdev);
err_disable_device:
    pci_disable_device(pdev);
err_free_priv:
    return ret;
}

/**
 * lvgl_pcie_remove - PCIe device remove function
 * @pdev: PCI device
 * 
 * Called when the PCIe device is removed
 */
static void lvgl_pcie_remove(struct pci_dev *pdev)
{
    struct lvgl_pcie_device *priv = pci_get_drvdata(pdev);
    int i;

    if (!priv)
        return;

    dev_info(&pdev->dev, "Removing LVGL PCIe device\n");

    priv->enabled = false;
    priv->initialized = false;

    /* Disable interrupts */
    lvgl_pcie_disable_interrupts(priv);

    /* Remove from global device list */
    mutex_lock(&g_device_mutex);
    for (i = 0; i < MAX_DEVICES; i++) {
        if (g_devices[i] == priv) {
            g_devices[i] = NULL;
            g_device_count--;
            break;
        }
    }
    mutex_unlock(&g_device_mutex);

    /* Cleanup LVGL integration */
    lvgl_pcie_lvgl_cleanup(priv);

    /* Cleanup character device */
    lvgl_pcie_cdev_cleanup(priv);

    /* Destroy work queue */
    if (priv->wq) {
        flush_workqueue(priv->wq);
        destroy_workqueue(priv->wq);
    }

    /* Cleanup buffers */
    lvgl_pcie_buffer_cleanup(priv);

    /* Cleanup DMA */
    lvgl_pcie_dma_cleanup(priv);

    /* Cleanup hardware */
    lvgl_pcie_hw_cleanup(priv);

    /* Unmap and release resources */
    if (priv->mmio)
        iounmap(priv->mmio);
    
    pci_release_regions(pdev);
    pci_disable_device(pdev);

    dev_info(&pdev->dev, "LVGL PCIe device removed successfully\n");
}

/**
 * lvgl_pcie_suspend - PCIe device suspend function
 * @pdev: PCI device
 * @state: Power management state
 */
static int lvgl_pcie_suspend(struct pci_dev *pdev, pm_message_t state)
{
    struct lvgl_pcie_device *priv = pci_get_drvdata(pdev);

    if (!priv)
        return 0;

    dev_info(&pdev->dev, "Suspending LVGL PCIe device\n");

    priv->enabled = false;
    
    /* Disable interrupts */
    lvgl_pcie_disable_interrupts(priv);

    /* Wait for pending DMA operations */
    if (atomic_read(&priv->dma_pending)) {
        wait_event_timeout(priv->dma_wait, 
                          !atomic_read(&priv->dma_pending),
                          msecs_to_jiffies(DMA_TIMEOUT_MS));
    }

    /* Flush work queue */
    if (priv->wq)
        flush_workqueue(priv->wq);

    pci_save_state(pdev);
    pci_set_power_state(pdev, pci_choose_state(pdev, state));

    return 0;
}

/**
 * lvgl_pcie_resume - PCIe device resume function
 * @pdev: PCI device
 */
static int lvgl_pcie_resume(struct pci_dev *pdev)
{
    struct lvgl_pcie_device *priv = pci_get_drvdata(pdev);
    int ret;

    if (!priv)
        return 0;

    dev_info(&pdev->dev, "Resuming LVGL PCIe device\n");

    pci_set_power_state(pdev, PCI_D0);
    pci_restore_state(pdev);

    ret = pci_enable_device(pdev);
    if (ret) {
        dev_err(&pdev->dev, "Failed to re-enable device: %d\n", ret);
        return ret;
    }

    pci_set_master(pdev);

    /* Re-initialize hardware */
    ret = lvgl_pcie_hw_init(priv);
    if (ret) {
        dev_err(&pdev->dev, "Hardware re-initialization failed: %d\n", ret);
        return ret;
    }

    /* Re-enable interrupts */
    lvgl_pcie_enable_interrupts(priv);

    priv->enabled = true;

    return 0;
}

/**
 * lvgl_pcie_init - Module initialization function
 */
static int __init lvgl_pcie_init(void)
{
    int ret;

    pr_info("Loading %s driver version %s\n", DRIVER_DESC, DRIVER_VERSION);

    /* Allocate character device numbers */
    ret = alloc_chrdev_region(&lvgl_pcie_devt, 0, MAX_DEVICES, DEVICE_NAME);
    if (ret) {
        pr_err("Failed to allocate character device numbers: %d\n", ret);
        return ret;
    }

    /* Create device class */
    lvgl_pcie_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(lvgl_pcie_class)) {
        ret = PTR_ERR(lvgl_pcie_class);
        pr_err("Failed to create device class: %d\n", ret);
        goto err_unregister_chrdev;
    }

    /* Register PCI driver */
    ret = pci_register_driver(&lvgl_pcie_driver);
    if (ret) {
        pr_err("Failed to register PCI driver: %d\n", ret);
        goto err_destroy_class;
    }

    pr_info("LVGL PCIe driver loaded successfully\n");
    return 0;

err_destroy_class:
    class_destroy(lvgl_pcie_class);
err_unregister_chrdev:
    unregister_chrdev_region(lvgl_pcie_devt, MAX_DEVICES);
    return ret;
}

/**
 * lvgl_pcie_exit - Module cleanup function
 */
static void __exit lvgl_pcie_exit(void)
{
    pr_info("Unloading LVGL PCIe driver\n");

    /* Unregister PCI driver */
    pci_unregister_driver(&lvgl_pcie_driver);

    /* Destroy device class */
    class_destroy(lvgl_pcie_class);

    /* Unregister character device numbers */
    unregister_chrdev_region(lvgl_pcie_devt, MAX_DEVICES);

    pr_info("LVGL PCIe driver unloaded\n");
}

module_init(lvgl_pcie_init);
module_exit(lvgl_pcie_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_DEVICE_TABLE(pci, lvgl_pcie_id_table);