/**
 * @file lv_custom_widget.c
 * @brief Customizable LVGL Widget Implementation
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_custom_widget.h"
#include <stdlib.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lv_custom_widget_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_custom_widget_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_custom_widget_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lv_custom_widget_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void draw_indicators(lv_obj_t * obj, lv_draw_ctx_t * draw_ctx);
static void draw_background(lv_obj_t * obj, lv_draw_ctx_t * draw_ctx);
static void draw_logo(lv_obj_t * obj, lv_draw_ctx_t * draw_ctx);
static void calculate_indicator_positions(lv_obj_t * obj);
static void create_indicator_objects(lv_obj_t * obj);
static void update_indicator_object(lv_obj_t * obj, uint8_t index);
static void anim_indicator_cb(void * var, int32_t v);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lv_custom_widget_class = {
    .constructor_cb = lv_custom_widget_constructor,
    .destructor_cb = lv_custom_widget_destructor,
    .event_cb = lv_custom_widget_event,
    .width_def = LV_CUSTOM_WIDGET_DEF_WIDTH,
    .height_def = LV_CUSTOM_WIDGET_DEF_HEIGHT,
    .instance_size = sizeof(lv_custom_widget_t),
    .base_class = &lv_obj_class
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_custom_widget_create(lv_obj_t * parent)
{
    LV_LOG_INFO("begin");
    lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
    lv_obj_class_init_obj(obj);
    return obj;
}

void lv_custom_widget_config_init(lv_custom_widget_config_t * config)
{
    LV_ASSERT_NULL(config);
    
    memset(config, 0, sizeof(lv_custom_widget_config_t));
    
    /* Default indicator settings */
    config->indicator_count = 4;
    config->layout = LV_CUSTOM_WIDGET_LAYOUT_HORIZONTAL;
    config->indicator_gap = 10;
    config->margin_x = 10;
    config->margin_y = 10;
    
    /* Default background settings */
    config->bg_type = LV_CUSTOM_WIDGET_BG_COLOR;
    config->bg_color_primary = lv_color_hex(0x404040);
    config->bg_color_secondary = lv_color_hex(0x606060);
    
    /* Default logo settings */
    config->logo_visible = false;
    config->logo_x = 0;
    config->logo_y = 0;
    config->logo_width = 0;
    config->logo_height = 0;
    
    /* Default animation settings */
    config->anim_time = 200;
    config->anim_path = lv_anim_path_ease_in_out;
    
    /* Default styling */
    config->border_width = 2;
    config->border_color = lv_color_hex(0x808080);
    config->radius = 5;
    config->opa = LV_OPA_COVER;
}

void lv_custom_widget_set_config(lv_obj_t * obj, const lv_custom_widget_config_t * config)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    LV_ASSERT_NULL(config);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    /* Copy configuration */
    memcpy(&widget->config, config, sizeof(lv_custom_widget_config_t));
    
    /* Allocate indicator configurations if needed */
    if(config->indicator_count > 0 && config->indicators) {
        size_t indicators_size = sizeof(lv_custom_widget_indicator_config_t) * config->indicator_count;
        widget->config.indicators = lv_mem_alloc(indicators_size);
        if(widget->config.indicators) {
            memcpy(widget->config.indicators, config->indicators, indicators_size);
        }
    }
    
    /* Create indicator objects */
    create_indicator_objects(obj);
    
    /* Apply styling */
    lv_obj_set_style_bg_color(obj, config->bg_color_primary, 0);
    lv_obj_set_style_border_width(obj, config->border_width, 0);
    lv_obj_set_style_border_color(obj, config->border_color, 0);
    lv_obj_set_style_radius(obj, config->radius, 0);
    lv_obj_set_style_opa(obj, config->opa, 0);
    
    /* Refresh the widget */
    lv_custom_widget_refresh(obj);
}

const lv_custom_widget_config_t * lv_custom_widget_get_config(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    return &widget->config;
}

void lv_custom_widget_set_indicator_count(lv_obj_t * obj, uint8_t count)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    if(count > 32) count = 32; /* Limit to 32 indicators */
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    /* Clean up existing indicators */
    if(widget->indicator_objs) {
        for(uint8_t i = 0; i < widget->config.indicator_count; i++) {
            if(widget->indicator_objs[i]) {
                lv_obj_del(widget->indicator_objs[i]);
            }
        }
        lv_mem_free(widget->indicator_objs);
        widget->indicator_objs = NULL;
    }
    
    if(widget->config.indicators) {
        lv_mem_free(widget->config.indicators);
        widget->config.indicators = NULL;
    }
    
    /* Set new count */
    widget->config.indicator_count = count;
    
    /* Allocate new indicator configurations with defaults */
    if(count > 0) {
        size_t indicators_size = sizeof(lv_custom_widget_indicator_config_t) * count;
        widget->config.indicators = lv_mem_alloc(indicators_size);
        
        if(widget->config.indicators) {
            for(uint8_t i = 0; i < count; i++) {
                widget->config.indicators[i].type = LV_CUSTOM_WIDGET_INDICATOR_CIRCLE;
                widget->config.indicators[i].color_active = lv_color_hex(0x00FF00);
                widget->config.indicators[i].color_inactive = lv_color_hex(0x404040);
                widget->config.indicators[i].size = 20;
                widget->config.indicators[i].border_width = 1;
                widget->config.indicators[i].border_color = lv_color_hex(0x808080);
                widget->config.indicators[i].custom_img_src = NULL;
                widget->config.indicators[i].enabled = true;
            }
        }
    }
    
    /* Create new indicator objects */
    create_indicator_objects(obj);
    lv_custom_widget_refresh(obj);
}

void lv_custom_widget_set_indicator_config(lv_obj_t * obj, uint8_t index, 
                                          const lv_custom_widget_indicator_config_t * config)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    LV_ASSERT_NULL(config);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    if(index >= widget->config.indicator_count || !widget->config.indicators) {
        LV_LOG_WARN("Invalid indicator index: %d", index);
        return;
    }
    
    /* Copy configuration */
    memcpy(&widget->config.indicators[index], config, sizeof(lv_custom_widget_indicator_config_t));
    
    /* Update the indicator object */
    update_indicator_object(obj, index);
}

void lv_custom_widget_set_indicator_state(lv_obj_t * obj, uint8_t index, bool active)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    if(index >= widget->config.indicator_count) {
        LV_LOG_WARN("Invalid indicator index: %d", index);
        return;
    }
    
    if(active) {
        widget->active_indicators |= (1 << index);
    } else {
        widget->active_indicators &= ~(1 << index);
    }
    
    /* Update the specific indicator */
    update_indicator_object(obj, index);
}

void lv_custom_widget_set_indicators_mask(lv_obj_t * obj, uint32_t mask)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    widget->active_indicators = mask;
    
    /* Update all indicators */
    for(uint8_t i = 0; i < widget->config.indicator_count; i++) {
        update_indicator_object(obj, i);
    }
}

bool lv_custom_widget_get_indicator_state(lv_obj_t * obj, uint8_t index)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    if(index >= widget->config.indicator_count) {
        LV_LOG_WARN("Invalid indicator index: %d", index);
        return false;
    }
    
    return (widget->active_indicators & (1 << index)) != 0;
}

void lv_custom_widget_set_layout(lv_obj_t * obj, lv_custom_widget_layout_t layout)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    widget->config.layout = layout;
    
    calculate_indicator_positions(obj);
    lv_custom_widget_refresh(obj);
}

void lv_custom_widget_set_indicator_gap(lv_obj_t * obj, lv_coord_t gap)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    widget->config.indicator_gap = gap;
    
    calculate_indicator_positions(obj);
    lv_custom_widget_refresh(obj);
}

void lv_custom_widget_set_background(lv_obj_t * obj, lv_custom_widget_bg_type_t bg_type,
                                    lv_color_t color_primary, lv_color_t color_secondary)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    widget->config.bg_type = bg_type;
    widget->config.bg_color_primary = color_primary;
    widget->config.bg_color_secondary = color_secondary;
    
    /* Apply background styling */
    switch(bg_type) {
        case LV_CUSTOM_WIDGET_BG_COLOR:
            lv_obj_set_style_bg_color(obj, color_primary, 0);
            lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
            break;
            
        case LV_CUSTOM_WIDGET_BG_GRADIENT:
            lv_obj_set_style_bg_color(obj, color_primary, 0);
            lv_obj_set_style_bg_grad_color(obj, color_secondary, 0);
            lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
            break;
            
        case LV_CUSTOM_WIDGET_BG_NONE:
            lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
            break;
            
        default:
            break;
    }
    
    lv_obj_invalidate(obj);
}

void lv_custom_widget_set_background_image(lv_obj_t * obj, const void * img_src, lv_img_cf_t img_cf)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    widget->config.bg_type = LV_CUSTOM_WIDGET_BG_IMAGE;
    widget->config.bg_img_src = img_src;
    widget->config.bg_img_cf = img_cf;
    
    /* Set background image */
    lv_obj_set_style_bg_img_src(obj, img_src, 0);
    lv_obj_invalidate(obj);
}

void lv_custom_widget_set_logo(lv_obj_t * obj, const void * logo_src, 
                              lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    widget->config.logo_src = logo_src;
    widget->config.logo_x = x;
    widget->config.logo_y = y;
    widget->config.logo_width = width;
    widget->config.logo_height = height;
    widget->config.logo_visible = (logo_src != NULL);
    
    /* Create or update logo object */
    if(widget->logo_obj) {
        lv_obj_del(widget->logo_obj);
        widget->logo_obj = NULL;
    }
    
    if(logo_src && widget->config.logo_visible) {
        widget->logo_obj = lv_img_create(obj);
        lv_img_set_src(widget->logo_obj, logo_src);
        lv_obj_set_pos(widget->logo_obj, x, y);
        
        if(width > 0 && height > 0) {
            lv_obj_set_size(widget->logo_obj, width, height);
        }
    }
}

void lv_custom_widget_set_logo_visible(lv_obj_t * obj, bool visible)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    widget->config.logo_visible = visible;
    
    if(widget->logo_obj) {
        if(visible) {
            lv_obj_clear_flag(widget->logo_obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widget->logo_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void lv_custom_widget_set_animation(lv_obj_t * obj, uint32_t anim_time, lv_anim_path_cb_t path_cb)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    widget->config.anim_time = anim_time;
    widget->config.anim_path = path_cb;
}

void lv_custom_widget_set_animate_indicators(lv_obj_t * obj, bool enable)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    if(!enable) {
        lv_anim_del(&widget->anim, NULL);
    }
}

void lv_custom_widget_refresh(lv_obj_t * obj)
{
    LV_ASSERT_OBJ(obj, MY_CLASS);
    
    calculate_indicator_positions(obj);
    lv_obj_invalidate(obj);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lv_custom_widget_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    /* Initialize configuration with defaults */
    lv_custom_widget_config_init(&widget->config);
    
    /* Initialize other members */
    widget->indicator_objs = NULL;
    widget->bg_obj = NULL;
    widget->logo_obj = NULL;
    widget->active_indicators = 0;
    
    /* Set default object properties */
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    
    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_custom_widget_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
    LV_UNUSED(class_p);
    LV_TRACE_OBJ_CREATE("begin");
    
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    /* Clean up indicator objects */
    if(widget->indicator_objs) {
        for(uint8_t i = 0; i < widget->config.indicator_count; i++) {
            if(widget->indicator_objs[i]) {
                lv_obj_del(widget->indicator_objs[i]);
            }
        }
        lv_mem_free(widget->indicator_objs);
    }
    
    /* Clean up indicator configurations */
    if(widget->config.indicators) {
        lv_mem_free(widget->config.indicators);
    }
    
    /* Clean up animations */
    lv_anim_del(&widget->anim, NULL);
    
    LV_TRACE_OBJ_CREATE("finished");
}

static void lv_custom_widget_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
    LV_UNUSED(class_p);
    
    lv_res_t res = lv_obj_event_base(MY_CLASS, e);
    if(res != LV_RES_OK) return;
    
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);
    
    if(code == LV_EVENT_DRAW_MAIN) {
        lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
        draw_background(obj, draw_ctx);
        draw_indicators(obj, draw_ctx);
        draw_logo(obj, draw_ctx);
    }
    else if(code == LV_EVENT_SIZE_CHANGED) {
        calculate_indicator_positions(obj);
    }
}

static void draw_background(lv_obj_t * obj, lv_draw_ctx_t * draw_ctx)
{
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);
    
    lv_draw_rect_dsc_t bg_dsc;
    lv_draw_rect_dsc_init(&bg_dsc);
    
    switch(widget->config.bg_type) {
        case LV_CUSTOM_WIDGET_BG_COLOR:
            bg_dsc.bg_color = widget->config.bg_color_primary;
            bg_dsc.bg_opa = LV_OPA_COVER;
            break;
            
        case LV_CUSTOM_WIDGET_BG_GRADIENT:
            bg_dsc.bg_color = widget->config.bg_color_primary;
            bg_dsc.bg_grad.stops[0].color = widget->config.bg_color_primary;
            bg_dsc.bg_grad.stops[1].color = widget->config.bg_color_secondary;
            bg_dsc.bg_grad.dir = LV_GRAD_DIR_VER;
            bg_dsc.bg_opa = LV_OPA_COVER;
            break;
            
        case LV_CUSTOM_WIDGET_BG_PATTERN:
            /* Custom pattern drawing could be implemented here */
            bg_dsc.bg_color = widget->config.bg_color_primary;
            bg_dsc.bg_opa = LV_OPA_50;
            break;
            
        case LV_CUSTOM_WIDGET_BG_NONE:
        default:
            return; /* No background to draw */
    }
    
    bg_dsc.radius = widget->config.radius;
    bg_dsc.border_width = widget->config.border_width;
    bg_dsc.border_color = widget->config.border_color;
    
    lv_draw_rect(draw_ctx, &bg_dsc, &obj_coords);
}

static void draw_indicators(lv_obj_t * obj, lv_draw_ctx_t * draw_ctx)
{
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    if(!widget->config.indicators || widget->config.indicator_count == 0) return;
    
    /* Indicators are drawn as child objects, so this function mainly handles 
       custom drawing that can't be done with standard LVGL objects */
}

static void draw_logo(lv_obj_t * obj, lv_draw_ctx_t * draw_ctx)
{
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    if(!widget->config.logo_visible || !widget->config.logo_src) return;
    
    /* Logo is handled as a child image object, so custom drawing is minimal */
}

static void calculate_indicator_positions(lv_obj_t * obj)
{
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    if(!widget->indicator_objs || widget->config.indicator_count == 0) return;
    
    lv_coord_t obj_width = lv_obj_get_width(obj);
    lv_coord_t obj_height = lv_obj_get_height(obj);
    lv_coord_t content_width = obj_width - 2 * widget->config.margin_x;
    lv_coord_t content_height = obj_height - 2 * widget->config.margin_y;
    
    for(uint8_t i = 0; i < widget->config.indicator_count; i++) {
        if(!widget->indicator_objs[i]) continue;
        
        lv_coord_t x = widget->config.margin_x;
        lv_coord_t y = widget->config.margin_y;
        
        switch(widget->config.layout) {
            case LV_CUSTOM_WIDGET_LAYOUT_HORIZONTAL: {
                lv_coord_t total_width = widget->config.indicator_count * widget->config.indicators[i].size + 
                                       (widget->config.indicator_count - 1) * widget->config.indicator_gap;
                x = widget->config.margin_x + (content_width - total_width) / 2 + 
                    i * (widget->config.indicators[i].size + widget->config.indicator_gap);
                y = widget->config.margin_y + (content_height - widget->config.indicators[i].size) / 2;
                break;
            }
            
            case LV_CUSTOM_WIDGET_LAYOUT_VERTICAL: {
                lv_coord_t total_height = widget->config.indicator_count * widget->config.indicators[i].size + 
                                        (widget->config.indicator_count - 1) * widget->config.indicator_gap;
                x = widget->config.margin_x + (content_width - widget->config.indicators[i].size) / 2;
                y = widget->config.margin_y + (content_height - total_height) / 2 + 
                    i * (widget->config.indicators[i].size + widget->config.indicator_gap);
                break;
            }
            
            case LV_CUSTOM_WIDGET_LAYOUT_GRID: {
                uint8_t cols = (uint8_t)lv_sqrt(widget->config.indicator_count);
                if(cols == 0) cols = 1;
                uint8_t rows = (widget->config.indicator_count + cols - 1) / cols;
                
                uint8_t col = i % cols;
                uint8_t row = i / cols;
                
                lv_coord_t grid_width = cols * widget->config.indicators[i].size + (cols - 1) * widget->config.indicator_gap;
                lv_coord_t grid_height = rows * widget->config.indicators[i].size + (rows - 1) * widget->config.indicator_gap;
                
                x = widget->config.margin_x + (content_width - grid_width) / 2 + 
                    col * (widget->config.indicators[i].size + widget->config.indicator_gap);
                y = widget->config.margin_y + (content_height - grid_height) / 2 + 
                    row * (widget->config.indicators[i].size + widget->config.indicator_gap);
                break;
            }
            
            case LV_CUSTOM_WIDGET_LAYOUT_CIRCULAR: {
                lv_coord_t center_x = obj_width / 2;
                lv_coord_t center_y = obj_height / 2;
                lv_coord_t radius = LV_MIN(content_width, content_height) / 2 - widget->config.indicators[i].size / 2;
                
                int32_t angle = (360 * i) / widget->config.indicator_count;
                x = center_x + (radius * lv_trigo_sin(angle)) / LV_TRIGO_SIN_MAX - widget->config.indicators[i].size / 2;
                y = center_y - (radius * lv_trigo_cos(angle)) / LV_TRIGO_SIN_MAX - widget->config.indicators[i].size / 2;
                break;
            }
            
            default:
                break;
        }
        
        lv_obj_set_pos(widget->indicator_objs[i], x, y);
    }
}

static void create_indicator_objects(lv_obj_t * obj)
{
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    /* Clean up existing indicator objects */
    if(widget->indicator_objs) {
        for(uint8_t i = 0; i < widget->config.indicator_count; i++) {
            if(widget->indicator_objs[i]) {
                lv_obj_del(widget->indicator_objs[i]);
            }
        }
        lv_mem_free(widget->indicator_objs);
        widget->indicator_objs = NULL;
    }
    
    if(widget->config.indicator_count == 0) return;
    
    /* Allocate array for indicator objects */
    widget->indicator_objs = lv_mem_alloc(sizeof(lv_obj_t *) * widget->config.indicator_count);
    if(!widget->indicator_objs) return;
    
    /* Create indicator objects */
    for(uint8_t i = 0; i < widget->config.indicator_count; i++) {
        widget->indicator_objs[i] = NULL;
        
        if(!widget->config.indicators || !widget->config.indicators[i].enabled) continue;
        
        lv_custom_widget_indicator_config_t * ind_config = &widget->config.indicators[i];
        
        switch(ind_config->type) {
            case LV_CUSTOM_WIDGET_INDICATOR_CIRCLE:
            case LV_CUSTOM_WIDGET_INDICATOR_SQUARE:
            case LV_CUSTOM_WIDGET_INDICATOR_LED: {
                widget->indicator_objs[i] = lv_obj_create(obj);
                lv_obj_set_size(widget->indicator_objs[i], ind_config->size, ind_config->size);
                
                if(ind_config->type == LV_CUSTOM_WIDGET_INDICATOR_CIRCLE) {
                    lv_obj_set_style_radius(widget->indicator_objs[i], LV_RADIUS_CIRCLE, 0);
                } else {
                    lv_obj_set_style_radius(widget->indicator_objs[i], 0, 0);
                }
                
                lv_obj_set_style_border_width(widget->indicator_objs[i], ind_config->border_width, 0);
                lv_obj_set_style_border_color(widget->indicator_objs[i], ind_config->border_color, 0);
                break;
            }
            
            case LV_CUSTOM_WIDGET_INDICATOR_BAR: {
                widget->indicator_objs[i] = lv_bar_create(obj);
                lv_obj_set_size(widget->indicator_objs[i], ind_config->size, ind_config->size / 4);
                lv_bar_set_range(widget->indicator_objs[i], 0, 100);
                lv_bar_set_value(widget->indicator_objs[i], 0, LV_ANIM_OFF);
                break;
            }
            
            case LV_CUSTOM_WIDGET_INDICATOR_ARC: {
                widget->indicator_objs[i] = lv_arc_create(obj);
                lv_obj_set_size(widget->indicator_objs[i], ind_config->size, ind_config->size);
                lv_arc_set_range(widget->indicator_objs[i], 0, 100);
                lv_arc_set_value(widget->indicator_objs[i], 0);
                break;
            }
            
            case LV_CUSTOM_WIDGET_INDICATOR_CUSTOM_BMP: {
                if(ind_config->custom_img_src) {
                    widget->indicator_objs[i] = lv_img_create(obj);
                    lv_img_set_src(widget->indicator_objs[i], ind_config->custom_img_src);
                    if(ind_config->size > 0) {
                        lv_obj_set_size(widget->indicator_objs[i], ind_config->size, ind_config->size);
                    }
                }
                break;
            }
            
            default:
                break;
        }
        
        if(widget->indicator_objs[i]) {
            lv_obj_clear_flag(widget->indicator_objs[i], LV_OBJ_FLAG_SCROLLABLE);
            update_indicator_object(obj, i);
        }
    }
}

static void update_indicator_object(lv_obj_t * obj, uint8_t index)
{
    lv_custom_widget_t * widget = (lv_custom_widget_t *)obj;
    
    if(index >= widget->config.indicator_count || !widget->indicator_objs || !widget->indicator_objs[index]) {
        return;
    }
    
    bool is_active = (widget->active_indicators & (1 << index)) != 0;
    lv_custom_widget_indicator_config_t * ind_config = &widget->config.indicators[index];
    lv_obj_t * ind_obj = widget->indicator_objs[index];
    
    lv_color_t color = is_active ? ind_config->color_active : ind_config->color_inactive;
    
    switch(ind_config->type) {
        case LV_CUSTOM_WIDGET_INDICATOR_CIRCLE:
        case LV_CUSTOM_WIDGET_INDICATOR_SQUARE:
        case LV_CUSTOM_WIDGET_INDICATOR_LED:
            lv_obj_set_style_bg_color(ind_obj, color, 0);
            lv_obj_set_style_bg_opa(ind_obj, LV_OPA_COVER, 0);
            break;
            
        case LV_CUSTOM_WIDGET_INDICATOR_BAR:
            lv_obj_set_style_bg_color(ind_obj, color, LV_PART_INDICATOR);
            lv_bar_set_value(ind_obj, is_active ? 100 : 0, widget->config.anim_time > 0 ? LV_ANIM_ON : LV_ANIM_OFF);
            break;
            
        case LV_CUSTOM_WIDGET_INDICATOR_ARC:
            lv_obj_set_style_arc_color(ind_obj, color, LV_PART_INDICATOR);
            lv_arc_set_value(ind_obj, is_active ? 100 : 0);
            break;
            
        case LV_CUSTOM_WIDGET_INDICATOR_CUSTOM_BMP:
            lv_obj_set_style_img_opa(ind_obj, is_active ? LV_OPA_COVER : LV_OPA_30, 0);
            break;
            
        default:
            break;
    }
}

static void anim_indicator_cb(void * var, int32_t v)
{
    lv_obj_t * obj = (lv_obj_t *)var;
    lv_obj_invalidate(obj);
}