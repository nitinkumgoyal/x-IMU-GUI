/**
 * @file lv_custom_widget.h
 * @brief Customizable LVGL Widget Header
 * 
 * A highly customizable widget that supports:
 * - Multiple indicator types and configurations
 * - Customizable backgrounds
 * - BMP/logo support
 * - Flexible layout options
 */

#ifndef LV_CUSTOM_WIDGET_H
#define LV_CUSTOM_WIDGET_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl.h"

/*********************
 *      DEFINES
 *********************/
#define LV_CUSTOM_WIDGET_DEF_WIDTH  200
#define LV_CUSTOM_WIDGET_DEF_HEIGHT 150

/**********************
 *      TYPEDEFS
 **********************/

/**
 * Indicator types supported by the custom widget
 */
typedef enum {
    LV_CUSTOM_WIDGET_INDICATOR_CIRCLE,
    LV_CUSTOM_WIDGET_INDICATOR_SQUARE,
    LV_CUSTOM_WIDGET_INDICATOR_TRIANGLE,
    LV_CUSTOM_WIDGET_INDICATOR_LED,
    LV_CUSTOM_WIDGET_INDICATOR_BAR,
    LV_CUSTOM_WIDGET_INDICATOR_ARC,
    LV_CUSTOM_WIDGET_INDICATOR_CUSTOM_BMP
} lv_custom_widget_indicator_type_t;

/**
 * Background types for the widget
 */
typedef enum {
    LV_CUSTOM_WIDGET_BG_NONE,
    LV_CUSTOM_WIDGET_BG_COLOR,
    LV_CUSTOM_WIDGET_BG_GRADIENT,
    LV_CUSTOM_WIDGET_BG_IMAGE,
    LV_CUSTOM_WIDGET_BG_PATTERN
} lv_custom_widget_bg_type_t;

/**
 * Layout arrangement for indicators
 */
typedef enum {
    LV_CUSTOM_WIDGET_LAYOUT_HORIZONTAL,
    LV_CUSTOM_WIDGET_LAYOUT_VERTICAL,
    LV_CUSTOM_WIDGET_LAYOUT_GRID,
    LV_CUSTOM_WIDGET_LAYOUT_CIRCULAR,
    LV_CUSTOM_WIDGET_LAYOUT_CUSTOM
} lv_custom_widget_layout_t;

/**
 * Configuration structure for indicators
 */
typedef struct {
    lv_custom_widget_indicator_type_t type;
    lv_color_t color_active;
    lv_color_t color_inactive;
    lv_coord_t size;
    lv_coord_t border_width;
    lv_color_t border_color;
    const void * custom_img_src;  /* For BMP/custom images */
    bool enabled;
} lv_custom_widget_indicator_config_t;

/**
 * Configuration structure for the widget
 */
typedef struct {
    /* Indicator settings */
    uint8_t indicator_count;
    lv_custom_widget_indicator_config_t * indicators;
    lv_custom_widget_layout_t layout;
    lv_coord_t indicator_gap;
    lv_coord_t margin_x;
    lv_coord_t margin_y;
    
    /* Background settings */
    lv_custom_widget_bg_type_t bg_type;
    lv_color_t bg_color_primary;
    lv_color_t bg_color_secondary;  /* For gradients */
    const void * bg_img_src;
    lv_img_cf_t bg_img_cf;
    
    /* Logo/BMP settings */
    const void * logo_src;
    lv_coord_t logo_x;
    lv_coord_t logo_y;
    lv_coord_t logo_width;
    lv_coord_t logo_height;
    bool logo_visible;
    
    /* Animation settings */
    uint32_t anim_time;
    lv_anim_path_cb_t anim_path;
    
    /* Border and styling */
    lv_coord_t border_width;
    lv_color_t border_color;
    lv_coord_t radius;
    lv_opa_t opa;
} lv_custom_widget_config_t;

/**
 * Custom widget data structure
 */
typedef struct {
    lv_obj_t obj;
    lv_custom_widget_config_t config;
    lv_obj_t ** indicator_objs;  /* Array of indicator objects */
    lv_obj_t * bg_obj;           /* Background object */
    lv_obj_t * logo_obj;         /* Logo object */
    uint32_t active_indicators;  /* Bitmask of active indicators */
    lv_anim_t anim;              /* Animation object */
} lv_custom_widget_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create a custom widget
 * @param parent pointer to a parent object
 * @return pointer to the created custom widget
 */
lv_obj_t * lv_custom_widget_create(lv_obj_t * parent);

/**
 * Initialize widget configuration with default values
 * @param config pointer to configuration structure
 */
void lv_custom_widget_config_init(lv_custom_widget_config_t * config);

/**
 * Apply configuration to the widget
 * @param obj pointer to the custom widget object
 * @param config pointer to configuration structure
 */
void lv_custom_widget_set_config(lv_obj_t * obj, const lv_custom_widget_config_t * config);

/**
 * Get current configuration of the widget
 * @param obj pointer to the custom widget object
 * @return pointer to the configuration structure
 */
const lv_custom_widget_config_t * lv_custom_widget_get_config(lv_obj_t * obj);

/**
 * Set the number of indicators
 * @param obj pointer to the custom widget object
 * @param count number of indicators (1-32)
 */
void lv_custom_widget_set_indicator_count(lv_obj_t * obj, uint8_t count);

/**
 * Configure a specific indicator
 * @param obj pointer to the custom widget object
 * @param index indicator index (0-based)
 * @param config pointer to indicator configuration
 */
void lv_custom_widget_set_indicator_config(lv_obj_t * obj, uint8_t index, 
                                          const lv_custom_widget_indicator_config_t * config);

/**
 * Set indicator state (active/inactive)
 * @param obj pointer to the custom widget object
 * @param index indicator index (0-based)
 * @param active true for active, false for inactive
 */
void lv_custom_widget_set_indicator_state(lv_obj_t * obj, uint8_t index, bool active);

/**
 * Set multiple indicators state using bitmask
 * @param obj pointer to the custom widget object
 * @param mask bitmask where each bit represents an indicator state
 */
void lv_custom_widget_set_indicators_mask(lv_obj_t * obj, uint32_t mask);

/**
 * Get indicator state
 * @param obj pointer to the custom widget object
 * @param index indicator index (0-based)
 * @return true if active, false if inactive
 */
bool lv_custom_widget_get_indicator_state(lv_obj_t * obj, uint8_t index);

/**
 * Set widget layout
 * @param obj pointer to the custom widget object
 * @param layout layout type
 */
void lv_custom_widget_set_layout(lv_obj_t * obj, lv_custom_widget_layout_t layout);

/**
 * Set gap between indicators
 * @param obj pointer to the custom widget object
 * @param gap gap in pixels
 */
void lv_custom_widget_set_indicator_gap(lv_obj_t * obj, lv_coord_t gap);

/**
 * Set background type and properties
 * @param obj pointer to the custom widget object
 * @param bg_type background type
 * @param color_primary primary color (or single color for solid background)
 * @param color_secondary secondary color (for gradients)
 */
void lv_custom_widget_set_background(lv_obj_t * obj, lv_custom_widget_bg_type_t bg_type,
                                    lv_color_t color_primary, lv_color_t color_secondary);

/**
 * Set background image
 * @param obj pointer to the custom widget object
 * @param img_src pointer to image source
 * @param img_cf image color format
 */
void lv_custom_widget_set_background_image(lv_obj_t * obj, const void * img_src, lv_img_cf_t img_cf);

/**
 * Set logo/BMP image
 * @param obj pointer to the custom widget object
 * @param logo_src pointer to logo image source
 * @param x logo X position
 * @param y logo Y position
 * @param width logo width (0 for original size)
 * @param height logo height (0 for original size)
 */
void lv_custom_widget_set_logo(lv_obj_t * obj, const void * logo_src, 
                              lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height);

/**
 * Show/hide logo
 * @param obj pointer to the custom widget object
 * @param visible true to show, false to hide
 */
void lv_custom_widget_set_logo_visible(lv_obj_t * obj, bool visible);

/**
 * Set animation properties
 * @param obj pointer to the custom widget object
 * @param anim_time animation duration in milliseconds
 * @param path_cb animation path callback
 */
void lv_custom_widget_set_animation(lv_obj_t * obj, uint32_t anim_time, lv_anim_path_cb_t path_cb);

/**
 * Animate indicator state changes
 * @param obj pointer to the custom widget object
 * @param enable true to enable animations, false to disable
 */
void lv_custom_widget_set_animate_indicators(lv_obj_t * obj, bool enable);

/**
 * Refresh the widget (redraw all components)
 * @param obj pointer to the custom widget object
 */
void lv_custom_widget_refresh(lv_obj_t * obj);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_CUSTOM_WIDGET_H*/