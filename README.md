# LVGL Customizable Widget

A highly customizable widget for LVGL that allows you to create flexible indicator displays with various configurations including different indicator types, layouts, backgrounds, and BMP/logo support.

## Features

### 🎯 **Multiple Indicator Types**
- **Circle Indicators**: Classic round indicators
- **Square Indicators**: Rectangular indicators  
- **LED Indicators**: LED-style indicators with enhanced styling
- **Bar Indicators**: Progress bar style indicators
- **Arc Indicators**: Circular arc progress indicators
- **Custom BMP**: Support for custom bitmap images as indicators

### 📐 **Flexible Layouts**
- **Horizontal**: Indicators arranged in a horizontal line
- **Vertical**: Indicators arranged in a vertical line
- **Grid**: Indicators arranged in a grid pattern (auto-calculated rows/columns)
- **Circular**: Indicators arranged in a circle
- **Custom**: Fully customizable positioning

### 🎨 **Background Customization**
- **Solid Color**: Single color background
- **Gradient**: Two-color gradient background
- **Image**: Custom background image support
- **Pattern**: Custom pattern backgrounds
- **Transparent**: No background for overlay effects

### 🖼️ **Logo/BMP Support**
- Add custom logos or images to the widget
- Configurable position and size
- Show/hide functionality
- Support for various image formats

### ⚡ **Animation Support**
- Smooth indicator state transitions
- Configurable animation duration
- Multiple animation paths (ease-in-out, bounce, etc.)
- Real-time indicator updates

## Quick Start

### 1. Include the Widget

```c
#include "lv_custom_widget.h"
```

### 2. Basic Usage

```c
// Create the widget
lv_obj_t * widget = lv_custom_widget_create(parent);
lv_obj_set_size(widget, 300, 100);

// Configure 5 horizontal indicators
lv_custom_widget_set_indicator_count(widget, 5);
lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_HORIZONTAL);

// Set background
lv_custom_widget_set_background(widget, LV_CUSTOM_WIDGET_BG_GRADIENT,
                               lv_color_hex(0x202020), lv_color_hex(0x404040));

// Activate first 3 indicators
lv_custom_widget_set_indicators_mask(widget, 0x07); // Binary: 00000111
```

## API Reference

### Widget Creation

```c
lv_obj_t * lv_custom_widget_create(lv_obj_t * parent);
```

### Configuration

```c
// Initialize configuration with defaults
void lv_custom_widget_config_init(lv_custom_widget_config_t * config);

// Apply full configuration
void lv_custom_widget_set_config(lv_obj_t * obj, const lv_custom_widget_config_t * config);

// Get current configuration
const lv_custom_widget_config_t * lv_custom_widget_get_config(lv_obj_t * obj);
```

### Indicator Management

```c
// Set number of indicators (1-32)
void lv_custom_widget_set_indicator_count(lv_obj_t * obj, uint8_t count);

// Configure individual indicator
void lv_custom_widget_set_indicator_config(lv_obj_t * obj, uint8_t index, 
                                          const lv_custom_widget_indicator_config_t * config);

// Set individual indicator state
void lv_custom_widget_set_indicator_state(lv_obj_t * obj, uint8_t index, bool active);

// Set multiple indicators using bitmask
void lv_custom_widget_set_indicators_mask(lv_obj_t * obj, uint32_t mask);

// Get indicator state
bool lv_custom_widget_get_indicator_state(lv_obj_t * obj, uint8_t index);
```

### Layout and Styling

```c
// Set layout type
void lv_custom_widget_set_layout(lv_obj_t * obj, lv_custom_widget_layout_t layout);

// Set gap between indicators
void lv_custom_widget_set_indicator_gap(lv_obj_t * obj, lv_coord_t gap);

// Set background
void lv_custom_widget_set_background(lv_obj_t * obj, lv_custom_widget_bg_type_t bg_type,
                                    lv_color_t color_primary, lv_color_t color_secondary);

// Set background image
void lv_custom_widget_set_background_image(lv_obj_t * obj, const void * img_src, lv_img_cf_t img_cf);
```

### Logo/BMP Support

```c
// Set logo
void lv_custom_widget_set_logo(lv_obj_t * obj, const void * logo_src, 
                              lv_coord_t x, lv_coord_t y, lv_coord_t width, lv_coord_t height);

// Show/hide logo
void lv_custom_widget_set_logo_visible(lv_obj_t * obj, bool visible);
```

### Animation

```c
// Set animation properties
void lv_custom_widget_set_animation(lv_obj_t * obj, uint32_t anim_time, lv_anim_path_cb_t path_cb);

// Enable/disable indicator animations
void lv_custom_widget_set_animate_indicators(lv_obj_t * obj, bool enable);
```

## Configuration Structures

### Indicator Configuration

```c
typedef struct {
    lv_custom_widget_indicator_type_t type;    // Indicator type
    lv_color_t color_active;                   // Active state color
    lv_color_t color_inactive;                 // Inactive state color
    lv_coord_t size;                           // Indicator size
    lv_coord_t border_width;                   // Border width
    lv_color_t border_color;                   // Border color
    const void * custom_img_src;              // Custom image source
    bool enabled;                              // Enable/disable indicator
} lv_custom_widget_indicator_config_t;
```

### Widget Configuration

```c
typedef struct {
    // Indicator settings
    uint8_t indicator_count;
    lv_custom_widget_indicator_config_t * indicators;
    lv_custom_widget_layout_t layout;
    lv_coord_t indicator_gap;
    lv_coord_t margin_x;
    lv_coord_t margin_y;
    
    // Background settings
    lv_custom_widget_bg_type_t bg_type;
    lv_color_t bg_color_primary;
    lv_color_t bg_color_secondary;
    const void * bg_img_src;
    
    // Logo settings
    const void * logo_src;
    lv_coord_t logo_x, logo_y;
    lv_coord_t logo_width, logo_height;
    bool logo_visible;
    
    // Animation settings
    uint32_t anim_time;
    lv_anim_path_cb_t anim_path;
    
    // Styling
    lv_coord_t border_width;
    lv_color_t border_color;
    lv_coord_t radius;
    lv_opa_t opa;
} lv_custom_widget_config_t;
```

## Examples

### Example 1: Basic Horizontal LEDs

```c
void create_basic_leds(lv_obj_t * parent) {
    lv_obj_t * widget = lv_custom_widget_create(parent);
    lv_obj_set_size(widget, 300, 60);
    
    lv_custom_widget_set_indicator_count(widget, 8);
    lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_HORIZONTAL);
    lv_custom_widget_set_indicator_gap(widget, 10);
    
    // Configure all indicators as green LEDs
    for(uint8_t i = 0; i < 8; i++) {
        lv_custom_widget_indicator_config_t config = {
            .type = LV_CUSTOM_WIDGET_INDICATOR_CIRCLE,
            .color_active = lv_color_hex(0x00FF00),
            .color_inactive = lv_color_hex(0x003300),
            .size = 20,
            .border_width = 1,
            .border_color = lv_color_hex(0x666666),
            .enabled = true
        };
        lv_custom_widget_set_indicator_config(widget, i, &config);
    }
    
    lv_custom_widget_set_background(widget, LV_CUSTOM_WIDGET_BG_COLOR,
                                   lv_color_hex(0x202020), lv_color_black());
}
```

### Example 2: Circular Status Display

```c
void create_circular_status(lv_obj_t * parent) {
    lv_obj_t * widget = lv_custom_widget_create(parent);
    lv_obj_set_size(widget, 200, 200);
    
    lv_custom_widget_set_indicator_count(widget, 12);
    lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_CIRCULAR);
    
    // Different colors for each indicator
    lv_color_t colors[] = {
        lv_color_hex(0xFF0000), lv_color_hex(0xFF4000), lv_color_hex(0xFF8000),
        lv_color_hex(0xFFFF00), lv_color_hex(0x80FF00), lv_color_hex(0x00FF00),
        lv_color_hex(0x00FF80), lv_color_hex(0x00FFFF), lv_color_hex(0x0080FF),
        lv_color_hex(0x0000FF), lv_color_hex(0x8000FF), lv_color_hex(0xFF00FF)
    };
    
    for(uint8_t i = 0; i < 12; i++) {
        lv_custom_widget_indicator_config_t config = {
            .type = LV_CUSTOM_WIDGET_INDICATOR_CIRCLE,
            .color_active = colors[i],
            .color_inactive = lv_color_hex(0x404040),
            .size = 15,
            .border_width = 1,
            .border_color = lv_color_hex(0x808080),
            .enabled = true
        };
        lv_custom_widget_set_indicator_config(widget, i, &config);
    }
    
    lv_custom_widget_set_background(widget, LV_CUSTOM_WIDGET_BG_GRADIENT,
                                   lv_color_hex(0x001122), lv_color_hex(0x003344));
}
```

### Example 3: Custom BMP Indicators

```c
void create_custom_bmp_widget(lv_obj_t * parent) {
    lv_obj_t * widget = lv_custom_widget_create(parent);
    lv_obj_set_size(widget, 250, 100);
    
    lv_custom_widget_set_indicator_count(widget, 5);
    lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_HORIZONTAL);
    
    // Use custom BMP for indicators
    for(uint8_t i = 0; i < 5; i++) {
        lv_custom_widget_indicator_config_t config = {
            .type = LV_CUSTOM_WIDGET_INDICATOR_CUSTOM_BMP,
            .custom_img_src = &my_custom_icon, // Your BMP data
            .size = 32,
            .enabled = true
        };
        lv_custom_widget_set_indicator_config(widget, i, &config);
    }
    
    // Add logo
    lv_custom_widget_set_logo(widget, &company_logo, 10, 10, 40, 40);
}
```

## Building

### Prerequisites
- LVGL library
- GCC compiler
- Make

### Build Steps

1. **Install LVGL**:
   ```bash
   make install-lvgl
   ```

2. **Build the widget**:
   ```bash
   make
   ```

3. **Create library**:
   ```bash
   make library
   ```

### Integration

To integrate into your LVGL project:

1. Copy `lv_custom_widget.h` and `lv_custom_widget.c` to your project
2. Include the header in your source files
3. Add the source file to your build system
4. Call the widget creation functions

## Memory Usage

- Base widget: ~200 bytes
- Per indicator: ~50-100 bytes (depending on type)
- Configuration: ~100 bytes + indicator configs
- Total for 8 indicators: ~1KB

## Performance

- Optimized for real-time updates
- Minimal redraw operations
- Efficient memory management
- Suitable for embedded systems

## Customization Examples

### Runtime Color Changes
```c
// Change indicator colors at runtime
lv_custom_widget_indicator_config_t config;
config.color_active = lv_color_hex(0xFF0000);  // Red
lv_custom_widget_set_indicator_config(widget, 0, &config);
```

### Dynamic Layout Switching
```c
// Switch from horizontal to circular layout
lv_custom_widget_set_layout(widget, LV_CUSTOM_WIDGET_LAYOUT_CIRCULAR);
lv_custom_widget_refresh(widget);
```

### Animated Sequences
```c
// Create rotating indicator pattern
void rotate_indicators(lv_obj_t * widget) {
    static uint8_t position = 0;
    uint32_t mask = 0x07 << position;  // 3 consecutive indicators
    if(mask > 0xFF) mask = (mask & 0xFF) | ((mask >> 8) & 0xFF);
    lv_custom_widget_set_indicators_mask(widget, mask);
    position = (position + 1) % 8;
}
```

## License

This widget is provided under the same license as LVGL. See the LVGL license for details.

## Contributing

Contributions are welcome! Please feel free to submit issues, feature requests, or pull requests.

## Support

For questions and support, please refer to the LVGL community forums or create an issue in this repository.