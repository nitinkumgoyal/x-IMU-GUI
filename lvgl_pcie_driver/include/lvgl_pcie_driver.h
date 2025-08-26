#ifndef LVGL_PCIE_DRIVER_H
#define LVGL_PCIE_DRIVER_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/fb.h>
#include "lvgl/lvgl.h"

/* Driver version and metadata */
#define DRIVER_NAME "lvgl_pcie_driver"
#define DRIVER_VERSION "1.0.0"
#define DRIVER_AUTHOR "LVGL PCIe Driver"
#define DRIVER_DESC "LVGL Display Driver with PCIe DMA and Double Buffer"

/* Device configuration */
#define VENDOR_ID 0x1234  /* Replace with actual vendor ID */
#define DEVICE_ID 0x5678  /* Replace with actual device ID */
#define MAX_DEVICES 4
#define DEVICE_NAME "lvgl_pcie"

/* Display configuration */
#define DEFAULT_SCREEN_WIDTH 1920
#define DEFAULT_SCREEN_HEIGHT 1080
#define DEFAULT_COLOR_DEPTH 32
#define BYTES_PER_PIXEL (DEFAULT_COLOR_DEPTH / 8)
#define BUFFER_SIZE (DEFAULT_SCREEN_WIDTH * DEFAULT_SCREEN_HEIGHT * BYTES_PER_PIXEL)

/* DMA configuration */
#define DMA_BUFFER_COUNT 2
#define DMA_TIMEOUT_MS 1000
#define MAX_DMA_SIZE (16 * 1024 * 1024)  /* 16MB max transfer */

/* Register offsets (FPGA specific - adjust as needed) */
#define REG_CONTROL       0x00
#define REG_STATUS        0x04
#define REG_DMA_SRC_ADDR  0x08
#define REG_DMA_DST_ADDR  0x0C
#define REG_DMA_SIZE      0x10
#define REG_DMA_CTRL      0x14
#define REG_INTERRUPT     0x18
#define REG_VERSION       0x1C

/* Control register bits */
#define CTRL_ENABLE       BIT(0)
#define CTRL_RESET        BIT(1)
#define CTRL_DMA_START    BIT(2)
#define CTRL_VSYNC_EN     BIT(3)

/* Status register bits */
#define STATUS_READY      BIT(0)
#define STATUS_DMA_DONE   BIT(1)
#define STATUS_ERROR      BIT(2)
#define STATUS_VSYNC      BIT(3)

/* Interrupt bits */
#define INT_DMA_COMPLETE  BIT(0)
#define INT_VSYNC         BIT(1)
#define INT_ERROR         BIT(2)

/* Double buffer structure */
struct dma_buffer {
    void *virt_addr;          /* Virtual address */
    dma_addr_t phys_addr;     /* Physical/DMA address */
    size_t size;              /* Buffer size */
    bool in_use;              /* Buffer in use flag */
    struct completion done;   /* DMA completion */
};

/* Device private data structure */
struct lvgl_pcie_device {
    struct pci_dev *pdev;           /* PCI device */
    struct device *dev;             /* Device */
    void __iomem *mmio;             /* Memory mapped I/O */
    resource_size_t mmio_start;     /* MMIO start address */
    resource_size_t mmio_len;       /* MMIO length */
    
    /* Character device */
    struct cdev cdev;
    dev_t devt;
    struct class *class;
    struct device *device;
    
    /* Display configuration */
    u32 screen_width;
    u32 screen_height;
    u32 color_depth;
    u32 bytes_per_pixel;
    
    /* Double buffer management */
    struct dma_buffer buffers[DMA_BUFFER_COUNT];
    int current_buffer;             /* Currently displayed buffer */
    int pending_buffer;             /* Buffer being prepared */
    struct mutex buffer_mutex;      /* Buffer access protection */
    
    /* DMA and synchronization */
    bool dma_coherent;
    wait_queue_head_t dma_wait;
    struct completion vsync_completion;
    atomic_t dma_pending;
    
    /* Interrupt handling */
    int irq;
    bool irq_enabled;
    
    /* Work queue for deferred processing */
    struct workqueue_struct *wq;
    struct work_struct vsync_work;
    struct work_struct dma_work;
    
    /* Statistics and debugging */
    atomic64_t frames_sent;
    atomic64_t dma_errors;
    atomic64_t vsync_count;
    
    /* LVGL integration */
    lv_disp_drv_t disp_drv;
    lv_disp_t *disp;
    lv_disp_draw_buf_t draw_buf;
    lv_color_t *draw_buf1;
    lv_color_t *draw_buf2;
    
    /* Device state */
    bool initialized;
    bool enabled;
};

/* Function prototypes */

/* PCIe driver functions */
static int lvgl_pcie_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void lvgl_pcie_remove(struct pci_dev *pdev);
static int lvgl_pcie_suspend(struct pci_dev *pdev, pm_message_t state);
static int lvgl_pcie_resume(struct pci_dev *pdev);

/* Hardware control functions */
int lvgl_pcie_hw_init(struct lvgl_pcie_device *priv);
void lvgl_pcie_hw_cleanup(struct lvgl_pcie_device *priv);
int lvgl_pcie_hw_reset(struct lvgl_pcie_device *priv);
u32 lvgl_pcie_read_reg(struct lvgl_pcie_device *priv, u32 offset);
void lvgl_pcie_write_reg(struct lvgl_pcie_device *priv, u32 offset, u32 value);

/* DMA functions */
int lvgl_pcie_dma_init(struct lvgl_pcie_device *priv);
void lvgl_pcie_dma_cleanup(struct lvgl_pcie_device *priv);
int lvgl_pcie_dma_transfer(struct lvgl_pcie_device *priv, int buffer_idx);
int lvgl_pcie_dma_wait_complete(struct lvgl_pcie_device *priv, int timeout_ms);

/* Buffer management */
int lvgl_pcie_buffer_init(struct lvgl_pcie_device *priv);
void lvgl_pcie_buffer_cleanup(struct lvgl_pcie_device *priv);
int lvgl_pcie_buffer_swap(struct lvgl_pcie_device *priv);
struct dma_buffer *lvgl_pcie_get_current_buffer(struct lvgl_pcie_device *priv);
struct dma_buffer *lvgl_pcie_get_pending_buffer(struct lvgl_pcie_device *priv);

/* Interrupt handling */
irqreturn_t lvgl_pcie_interrupt(int irq, void *dev_id);
void lvgl_pcie_enable_interrupts(struct lvgl_pcie_device *priv);
void lvgl_pcie_disable_interrupts(struct lvgl_pcie_device *priv);

/* Work queue functions */
void lvgl_pcie_vsync_work(struct work_struct *work);
void lvgl_pcie_dma_work(struct work_struct *work);

/* LVGL integration functions */
int lvgl_pcie_lvgl_init(struct lvgl_pcie_device *priv);
void lvgl_pcie_lvgl_cleanup(struct lvgl_pcie_device *priv);
void lvgl_pcie_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
void lvgl_pcie_rounder_cb(lv_disp_drv_t *disp_drv, lv_area_t *area);
void lvgl_pcie_set_px_cb(lv_disp_drv_t *disp_drv, u8 *buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y, lv_color_t color, lv_opa_t opa);

/* Character device functions */
int lvgl_pcie_cdev_init(struct lvgl_pcie_device *priv);
void lvgl_pcie_cdev_cleanup(struct lvgl_pcie_device *priv);
static int lvgl_pcie_open(struct inode *inode, struct file *file);
static int lvgl_pcie_release(struct inode *inode, struct file *file);
static ssize_t lvgl_pcie_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos);
static ssize_t lvgl_pcie_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos);
static long lvgl_pcie_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int lvgl_pcie_mmap(struct file *file, struct vm_area_struct *vma);

/* IOCTL commands */
#define LVGL_PCIE_IOC_MAGIC 'L'
#define LVGL_PCIE_IOC_GET_INFO      _IOR(LVGL_PCIE_IOC_MAGIC, 1, struct lvgl_pcie_info)
#define LVGL_PCIE_IOC_SET_CONFIG    _IOW(LVGL_PCIE_IOC_MAGIC, 2, struct lvgl_pcie_config)
#define LVGL_PCIE_IOC_GET_STATS     _IOR(LVGL_PCIE_IOC_MAGIC, 3, struct lvgl_pcie_stats)
#define LVGL_PCIE_IOC_RESET         _IO(LVGL_PCIE_IOC_MAGIC, 4)
#define LVGL_PCIE_IOC_ENABLE        _IO(LVGL_PCIE_IOC_MAGIC, 5)
#define LVGL_PCIE_IOC_DISABLE       _IO(LVGL_PCIE_IOC_MAGIC, 6)
#define LVGL_PCIE_IOC_MAXNR         6

/* IOCTL data structures */
struct lvgl_pcie_info {
    u32 screen_width;
    u32 screen_height;
    u32 color_depth;
    u32 buffer_size;
    u64 frames_sent;
    u64 dma_errors;
    u64 vsync_count;
    bool enabled;
};

struct lvgl_pcie_config {
    u32 screen_width;
    u32 screen_height;
    u32 color_depth;
};

struct lvgl_pcie_stats {
    u64 frames_sent;
    u64 dma_errors;
    u64 vsync_count;
    u64 total_bytes_transferred;
    u32 current_fps;
    u32 avg_dma_time_us;
};

/* Global variables */
extern struct lvgl_pcie_device *g_devices[MAX_DEVICES];
extern int g_device_count;
extern struct mutex g_device_mutex;

/* Utility macros */
#define lvgl_pcie_err(priv, fmt, ...) \
    dev_err(&(priv)->pdev->dev, fmt, ##__VA_ARGS__)

#define lvgl_pcie_warn(priv, fmt, ...) \
    dev_warn(&(priv)->pdev->dev, fmt, ##__VA_ARGS__)

#define lvgl_pcie_info(priv, fmt, ...) \
    dev_info(&(priv)->pdev->dev, fmt, ##__VA_ARGS__)

#define lvgl_pcie_dbg(priv, fmt, ...) \
    dev_dbg(&(priv)->pdev->dev, fmt, ##__VA_ARGS__)

#endif /* LVGL_PCIE_DRIVER_H */