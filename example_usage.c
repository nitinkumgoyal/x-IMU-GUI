/**
 * @file example_usage.c
 * @brief Example usage of the customizable LVGL widget
 * 
 * This file demonstrates various ways to configure and use the custom widget
 */

#include "lv_custom_widget.h"

/* Example BMP data (placeholder - replace with actual image data) */
LV_IMG_DECLARE(example_logo);
LV_IMG_DECLARE(custom_indicator_bmp);

/**
 * Example 1: Basic horizontal indicator widget
 */
void example_basic_horizontal_widget(lv_obj_t * parent)
{
    /* Create the widget */
    lv_obj_t * widget = lv_custom_widget_create(parent);
    lv_obj_set_size(widget, 300, 100);
    lv_obj_center(widget);
    
    /* Configure basic horizontal layout with 5 LED indicators */
    lv_custom_widget_set_indicator_count(widget, 5);
    lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_HORIZONTAL);
    lv_custom_widget_set_indicator_gap(widget, 15);
    
    /* Configure individual indicators */
    for(uint8_t i = 0; i < 5; i++) {
        lv_custom_widget_indicator_config_t ind_config = {
            .type = LV_CUSTOM_WIDGET_INDICATOR_CIRCLE,
            .color_active = lv_color_hex(0x00FF00),
            .color_inactive = lv_color_hex(0x404040),
            .size = 25,
            .border_width = 2,
            .border_color = lv_color_hex(0x808080),
            .enabled = true
        };
        lv_custom_widget_set_indicator_config(widget, i, &ind_config);
    }
    
    /* Set background */
    lv_custom_widget_set_background(widget, LV_CUSTOM_WIDGET_BG_GRADIENT,
                                   lv_color_hex(0x202020), lv_color_hex(0x404040));
    
    /* Activate first 3 indicators */
    lv_custom_widget_set_indicators_mask(widget, 0x07); /* Binary: 00000111 */
}

/**
 * Example 2: Circular layout with different indicator types
 */
void example_circular_mixed_indicators(lv_obj_t * parent)
{
    /* Create the widget */
    lv_obj_t * widget = lv_custom_widget_create(parent);
    lv_obj_set_size(widget, 250, 250);
    lv_obj_align(widget, LV_ALIGN_TOP_LEFT, 20, 20);
    
    /* Configure circular layout with 8 indicators */
    lv_custom_widget_set_indicator_count(widget, 8);
    lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_CIRCULAR);
    
    /* Configure different indicator types */
    lv_custom_widget_indicator_config_t configs[8] = {
        /* Circle indicators */
        {LV_CUSTOM_WIDGET_INDICATOR_CIRCLE, lv_color_hex(0xFF0000), lv_color_hex(0x404040), 20, 1, lv_color_hex(0x808080), NULL, true},
        {LV_CUSTOM_WIDGET_INDICATOR_CIRCLE, lv_color_hex(0x00FF00), lv_color_hex(0x404040), 20, 1, lv_color_hex(0x808080), NULL, true},
        /* Square indicators */
        {LV_CUSTOM_WIDGET_INDICATOR_SQUARE, lv_color_hex(0x0000FF), lv_color_hex(0x404040), 18, 1, lv_color_hex(0x808080), NULL, true},
        {LV_CUSTOM_WIDGET_INDICATOR_SQUARE, lv_color_hex(0xFFFF00), lv_color_hex(0x404040), 18, 1, lv_color_hex(0x808080), NULL, true},
        /* LED indicators */
        {LV_CUSTOM_WIDGET_INDICATOR_LED, lv_color_hex(0xFF00FF), lv_color_hex(0x404040), 22, 2, lv_color_hex(0x808080), NULL, true},
        {LV_CUSTOM_WIDGET_INDICATOR_LED, lv_color_hex(0x00FFFF), lv_color_hex(0x404040), 22, 2, lv_color_hex(0x808080), NULL, true},
        /* Arc indicators */
        {LV_CUSTOM_WIDGET_INDICATOR_ARC, lv_color_hex(0xFFA500), lv_color_hex(0x404040), 25, 0, lv_color_hex(0x808080), NULL, true},
        {LV_CUSTOM_WIDGET_INDICATOR_ARC, lv_color_hex(0x800080), lv_color_hex(0x404040), 25, 0, lv_color_hex(0x808080), NULL, true}
    };
    
    for(uint8_t i = 0; i < 8; i++) {
        lv_custom_widget_set_indicator_config(widget, i, &configs[i]);
    }
    
    /* Set dark background with border */
    lv_custom_widget_set_background(widget, LV_CUSTOM_WIDGET_BG_COLOR,
                                   lv_color_hex(0x1a1a1a), lv_color_black());
    
    /* Activate alternating indicators */
    lv_custom_widget_set_indicators_mask(widget, 0xAA); /* Binary: 10101010 */
}

/**
 * Example 3: Grid layout with logo and custom BMP indicators
 */
void example_grid_with_logo(lv_obj_t * parent)
{
    /* Create the widget */
    lv_obj_t * widget = lv_custom_widget_create(parent);
    lv_obj_set_size(widget, 200, 200);
    lv_obj_align(widget, LV_ALIGN_TOP_RIGHT, -20, 20);
    
    /* Configure grid layout with 9 indicators (3x3) */
    lv_custom_widget_set_indicator_count(widget, 9);
    lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_GRID);
    lv_custom_widget_set_indicator_gap(widget, 8);
    
    /* Configure indicators - mix of regular and custom BMP */
    for(uint8_t i = 0; i < 9; i++) {
        lv_custom_widget_indicator_config_t ind_config;
        
        if(i % 3 == 0) {
            /* Every 3rd indicator uses custom BMP */
            ind_config.type = LV_CUSTOM_WIDGET_INDICATOR_CUSTOM_BMP;
            ind_config.custom_img_src = &custom_indicator_bmp;
            ind_config.size = 24;
        } else {
            /* Regular circle indicators */
            ind_config.type = LV_CUSTOM_WIDGET_INDICATOR_CIRCLE;
            ind_config.color_active = lv_color_hex(0x00AAFF);
            ind_config.color_inactive = lv_color_hex(0x333333);
            ind_config.size = 20;
            ind_config.border_width = 1;
            ind_config.border_color = lv_color_hex(0x666666);
        }
        
        ind_config.enabled = true;
        lv_custom_widget_set_indicator_config(widget, i, &ind_config);
    }
    
    /* Set gradient background */
    lv_custom_widget_set_background(widget, LV_CUSTOM_WIDGET_BG_GRADIENT,
                                   lv_color_hex(0x003366), lv_color_hex(0x001122));
    
    /* Add logo in center */
    lv_custom_widget_set_logo(widget, &example_logo, 75, 75, 50, 50);
    
    /* Activate corner indicators */
    lv_custom_widget_set_indicators_mask(widget, 0x145); /* Binary: 101000101 */
}

/**
 * Example 4: Vertical bar indicators with animation
 */
void example_vertical_bars_animated(lv_obj_t * parent)
{
    /* Create the widget */
    lv_obj_t * widget = lv_custom_widget_create(parent);
    lv_obj_set_size(widget, 300, 150);
    lv_obj_align(widget, LV_ALIGN_BOTTOM_MID, 0, -20);
    
    /* Configure vertical layout with 6 bar indicators */
    lv_custom_widget_set_indicator_count(widget, 6);
    lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_HORIZONTAL);
    lv_custom_widget_set_indicator_gap(widget, 12);
    
    /* Configure bar indicators with different colors */
    lv_color_t colors[] = {
        lv_color_hex(0xFF0000), lv_color_hex(0xFF8000),
        lv_color_hex(0xFFFF00), lv_color_hex(0x80FF00),
        lv_color_hex(0x00FF00), lv_color_hex(0x00FF80)
    };
    
    for(uint8_t i = 0; i < 6; i++) {
        lv_custom_widget_indicator_config_t ind_config = {
            .type = LV_CUSTOM_WIDGET_INDICATOR_BAR,
            .color_active = colors[i],
            .color_inactive = lv_color_hex(0x404040),
            .size = 35,
            .border_width = 1,
            .border_color = lv_color_hex(0x808080),
            .enabled = true
        };
        lv_custom_widget_set_indicator_config(widget, i, &ind_config);
    }
    
    /* Set transparent background to show parent */
    lv_custom_widget_set_background(widget, LV_CUSTOM_WIDGET_BG_NONE,
                                   lv_color_black(), lv_color_black());
    
    /* Enable animations */
    lv_custom_widget_set_animation(widget, 300, lv_anim_path_ease_in_out);
    lv_custom_widget_set_animate_indicators(widget, true);
    
    /* Start with no indicators active */
    lv_custom_widget_set_indicators_mask(widget, 0x00);
}

/**
 * Example 5: Custom configuration structure usage
 */
void example_full_configuration(lv_obj_t * parent)
{
    /* Create the widget */
    lv_obj_t * widget = lv_custom_widget_create(parent);
    lv_obj_set_size(widget, 400, 300);
    lv_obj_center(widget);
    
    /* Create full configuration structure */
    lv_custom_widget_config_t config;
    lv_custom_widget_config_init(&config);
    
    /* Configure widget properties */
    config.indicator_count = 12;
    config.layout = LV_CUSTOM_WIDGET_LAYOUT_CIRCULAR;
    config.indicator_gap = 5;
    config.margin_x = 20;
    config.margin_y = 20;
    
    /* Background settings */
    config.bg_type = LV_CUSTOM_WIDGET_BG_GRADIENT;
    config.bg_color_primary = lv_color_hex(0x001a33);
    config.bg_color_secondary = lv_color_hex(0x003366);
    
    /* Logo settings */
    config.logo_src = &example_logo;
    config.logo_x = 175;
    config.logo_y = 125;
    config.logo_width = 50;
    config.logo_height = 50;
    config.logo_visible = true;
    
    /* Animation settings */
    config.anim_time = 500;
    config.anim_path = lv_anim_path_bounce;
    
    /* Styling */
    config.border_width = 3;
    config.border_color = lv_color_hex(0x0066cc);
    config.radius = 15;
    config.opa = LV_OPA_90;
    
    /* Allocate and configure indicators */
    config.indicators = lv_mem_alloc(sizeof(lv_custom_widget_indicator_config_t) * config.indicator_count);
    
    for(uint8_t i = 0; i < config.indicator_count; i++) {
        config.indicators[i].type = (i % 4 == 0) ? LV_CUSTOM_WIDGET_INDICATOR_ARC : LV_CUSTOM_WIDGET_INDICATOR_CIRCLE;
        config.indicators[i].color_active = lv_color_hsv_to_rgb(i * 30, 100, 100);
        config.indicators[i].color_inactive = lv_color_hex(0x333333);
        config.indicators[i].size = (i % 4 == 0) ? 30 : 18;
        config.indicators[i].border_width = 1;
        config.indicators[i].border_color = lv_color_hex(0x666666);
        config.indicators[i].enabled = true;
    }
    
    /* Apply configuration */
    lv_custom_widget_set_config(widget, &config);
    
    /* Clean up allocated memory */
    lv_mem_free(config.indicators);
    
    /* Set initial state - clock pattern */
    lv_custom_widget_set_indicators_mask(widget, 0x249); /* 12, 6, 3, 9 o'clock positions */
}

/**
 * Animation timer callback to demonstrate dynamic indicator updates
 */
static void indicator_animation_timer_cb(lv_timer_t * timer)
{
    static uint32_t counter = 0;
    lv_obj_t * widget = (lv_obj_t *)timer->user_data;
    
    /* Rotate active indicators */
    uint32_t mask = 0x07 << (counter % 6); /* 3 consecutive indicators rotating */
    if(mask > 0xFF) mask = (mask & 0xFF) | ((mask >> 8) & 0xFF);
    
    lv_custom_widget_set_indicators_mask(widget, mask);
    counter++;
}

/**
 * Main example function - creates all example widgets
 */
void lv_custom_widget_examples(void)
{
    /* Get the active screen */
    lv_obj_t * scr = lv_scr_act();
    
    /* Create different example widgets */
    example_basic_horizontal_widget(scr);
    example_circular_mixed_indicators(scr);
    example_grid_with_logo(scr);
    example_vertical_bars_animated(scr);
    
    /* Create a timer for demonstration */
    lv_obj_t * animated_widget = lv_custom_widget_create(scr);
    lv_obj_set_size(animated_widget, 200, 50);
    lv_obj_align(animated_widget, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    
    lv_custom_widget_set_indicator_count(animated_widget, 8);
    lv_custom_widget_set_layout(animated_widget, LV_CUSTOM_WIDGET_LAYOUT_HORIZONTAL);
    
    /* Create timer for animation */
    lv_timer_t * timer = lv_timer_create(indicator_animation_timer_cb, 200, animated_widget);
}

/**
 * Example of runtime indicator state changes
 */
void example_runtime_changes(lv_obj_t * widget)
{
    /* Change individual indicator states */
    lv_custom_widget_set_indicator_state(widget, 0, true);
    lv_custom_widget_set_indicator_state(widget, 2, true);
    lv_custom_widget_set_indicator_state(widget, 4, false);
    
    /* Change layout dynamically */
    lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_VERTICAL);
    
    /* Change gap between indicators */
    lv_custom_widget_set_indicator_gap(widget, 20);
    
    /* Change background */
    lv_custom_widget_set_background(widget, LV_CUSTOM_WIDGET_BG_GRADIENT,
                                   lv_color_hex(0x4a0080), lv_color_hex(0x8000ff));
    
    /* Toggle logo visibility */
    lv_custom_widget_set_logo_visible(widget, false);
    
    /* Refresh widget to apply changes */
    lv_custom_widget_refresh(widget);
}