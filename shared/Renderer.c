#include "Renderer.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <string.h>

// LCD config from main.c
extern ST7789V2_cfg_t cfg0;

// ===== Internal State =====
static uint32_t frame_start_tick = 0;
static uint32_t last_frame_time  = 0;
static uint32_t frame_count      = 0;

// Character width: 5px glyph + 1px spacing = 6px per char at scale 1
#define CHAR_WIDTH_BASE 6

// ===== Dirty Rect Tracking =====
#define MAX_DIRTY_RECTS 32

typedef struct {
    uint16_t x, y, w, h;
} DirtyRect_t;

static DirtyRect_t curr_dirty[MAX_DIRTY_RECTS];  // rects drawn in the current frame
static uint8_t     curr_dirty_n    = 0;
static uint8_t     dirty_overflow  = 0;           // fell back to full-screen clear
static uint8_t     frame_bg_colour = 0;           // palette index used to erase dirty rects

static void mark_dirty(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (dirty_overflow) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT || w == 0 || h == 0) return;
    if ((uint32_t)x + w > SCREEN_WIDTH)  w = (uint16_t)(SCREEN_WIDTH  - x);
    if ((uint32_t)y + h > SCREEN_HEIGHT) h = (uint16_t)(SCREEN_HEIGHT - y);
    if (curr_dirty_n >= MAX_DIRTY_RECTS) {
        dirty_overflow = 1;
        return;
    }
    curr_dirty[curr_dirty_n++] = (DirtyRect_t){ x, y, w, h };
}

// ===== Image Drawing Functions =====

void Renderer_DrawImage(const Image *img, uint16_t x, uint16_t y) {
    if (!img || !img->data || img->width == 0 || img->height == 0) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    mark_dirty(x, y, img->width, img->height);
    LCD_Draw_Sprite(x, y, img->height, img->width, img->data);
}

void Renderer_DrawImageScaled(const Image *img, uint16_t x, uint16_t y, uint8_t scale) {
    if (!img || !img->data || scale == 0 || img->width == 0 || img->height == 0) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    mark_dirty(x, y, (uint16_t)img->width * scale, (uint16_t)img->height * scale);
    LCD_Draw_Sprite_Scaled(x, y, img->height, img->width, img->data, scale);
}

void Renderer_DrawImageColour(const Image *img, uint16_t x, uint16_t y, uint8_t colour) {
    if (!img || !img->data || img->width == 0 || img->height == 0) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    mark_dirty(x, y, img->width, img->height);
    LCD_Draw_Sprite_Colour(x, y, img->height, img->width, img->data, colour);
}

void Renderer_DrawImageColourScaled(const Image *img, uint16_t x, uint16_t y, uint8_t colour, uint8_t scale) {
    if (!img || !img->data || scale == 0 || img->width == 0 || img->height == 0) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    mark_dirty(x, y, (uint16_t)img->width * scale, (uint16_t)img->height * scale);
    LCD_Draw_Sprite_Colour_Scaled(x, y, img->height, img->width, img->data, colour, scale);
}

void Renderer_DrawImageFlipH(const Image *img, uint16_t x, uint16_t y, uint8_t scale) {
    if (!img || !img->data || scale == 0 || img->width == 0 || img->height == 0) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    mark_dirty(x, y, (uint16_t)img->width * scale, (uint16_t)img->height * scale);

    for (uint16_t row = 0; row < img->height; row++) {
        uint16_t by = y + (uint16_t)(row * scale);
        if (by >= SCREEN_HEIGHT) break;
        for (uint16_t col = 0; col < img->width; col++) {
            uint8_t pixel = img->data[row * img->width + col];
            if (pixel == TRANSPARENT) continue;
            uint16_t dst_col = img->width - 1 - col;
            uint16_t bx = x + (uint16_t)(dst_col * scale);
            if (bx >= SCREEN_WIDTH) break;
            for (uint8_t dy = 0; dy < scale; dy++) {
                if (by + dy >= SCREEN_HEIGHT) break;
                for (uint8_t dx = 0; dx < scale; dx++) {
                    if (bx + dx < SCREEN_WIDTH)
                        LCD_Set_Pixel(bx + dx, by + dy, pixel);
                }
            }
        }
    }
}

void Renderer_DrawFrame(const SpriteSheet *sheet, uint16_t frame_idx, uint16_t x, uint16_t y) {
    Renderer_DrawFrameScaled(sheet, frame_idx, x, y, 1);
}

void Renderer_DrawFrameScaled(const SpriteSheet *sheet, uint16_t frame_idx, uint16_t x, uint16_t y, uint8_t scale) {
    if (!sheet || !sheet->data || sheet->frame_count == 0 || scale == 0) return;
    if (sheet->frame_w == 0 || sheet->frame_h == 0) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;

    if (frame_idx >= sheet->frame_count) {
        frame_idx = sheet->frame_count - 1;
    }

    const uint8_t *frame_data = sheet->data + (uint32_t)frame_idx * sheet->frame_w * sheet->frame_h;
    mark_dirty(x, y, (uint16_t)sheet->frame_w * scale, (uint16_t)sheet->frame_h * scale);
    LCD_Draw_Sprite_Scaled(x, y, sheet->frame_h, sheet->frame_w, frame_data, scale);
}

void Renderer_DrawAnim(const SpriteSheet *sheet, uint32_t anim_counter, uint16_t frames_per_img,
                       uint16_t x, uint16_t y, uint8_t scale) {
    if (!sheet || sheet->frame_count == 0 || frames_per_img == 0 || scale == 0) return;

    uint16_t frame_idx = (anim_counter / frames_per_img) % sheet->frame_count;
    Renderer_DrawFrameScaled(sheet, frame_idx, x, y, scale);
}

Image Renderer_GetFrameAsImage(const SpriteSheet *sheet, uint16_t frame_idx) {
    Image img = { 0, 0, 0 };
    if (!sheet || !sheet->data || sheet->frame_count == 0) return img;

    if (frame_idx >= sheet->frame_count) {
        frame_idx = sheet->frame_count - 1;
    }

    img.width  = sheet->frame_w;
    img.height = sheet->frame_h;
    img.data   = sheet->data + (uint32_t)frame_idx * sheet->frame_w * sheet->frame_h;
    return img;
}

// ===== Frame Management =====

void Renderer_BeginFrame(void) {
    frame_start_tick = HAL_GetTick();

    // Erase the regions drawn in the *previous* frame (stored in curr_dirty
    // which hasn't been reset yet).  This ensures old sprites are removed
    // in the very next frame, not one frame late.
    if (dirty_overflow) {
        // Previous frame overflowed → whole screen is dirty.
        // LCD_Fill_Buffer is a fast byte-level fill and is far cheaper than
        // 57600 individual LCD_Set_Pixel calls via LCD_Draw_Rect.
        LCD_Fill_Buffer(frame_bg_colour);
    } else {
        // Clear each dirty rect with a tight pixel loop.
        // This is faster than LCD_Draw_Rect(fill) which goes through
        // LCD_Draw_Line → LCD_Set_Pixel with extra overhead per pixel.
        for (uint8_t i = 0; i < curr_dirty_n; i++) {
            const DirtyRect_t *r = &curr_dirty[i];
            uint16_t x_end = r->x + r->w;
            uint16_t y_end = r->y + r->h;
            if (x_end > SCREEN_WIDTH)  x_end = SCREEN_WIDTH;
            if (y_end > SCREEN_HEIGHT) y_end = SCREEN_HEIGHT;
            for (uint16_t row = r->y; row < y_end; row++) {
                for (uint16_t col = r->x; col < x_end; col++) {
                    LCD_Set_Pixel(col, row, frame_bg_colour);
                }
            }
        }
    }

    // Reset for the new frame.
    curr_dirty_n   = 0;
    dirty_overflow = 0;
}

uint32_t Renderer_EndFrame(uint32_t target_ms) {
    LCD_Refresh(&cfg0);

    uint32_t elapsed = HAL_GetTick() - frame_start_tick;

    if (target_ms > 0 && elapsed < target_ms) {
        HAL_Delay(target_ms - elapsed);
    }

    last_frame_time = HAL_GetTick() - frame_start_tick;
    frame_count++;
    return last_frame_time;
}

uint32_t Renderer_GetLastFrameTime(void) {
    return last_frame_time;
}

uint32_t Renderer_GetFrameCount(void) {
    return frame_count;
}

void Renderer_ResetFrameCount(void) {
    frame_count = 0;
}

void Renderer_SetBgColour(uint8_t colour) {
    frame_bg_colour = colour;
}

void Renderer_InitScreen(uint8_t bg_colour, const Image *bg_img, uint8_t img_scale) {
    LCD_Fill_Buffer(bg_colour);
    if (bg_img && bg_img->data && bg_img->width > 0 && bg_img->height > 0 && img_scale > 0) {
        if (img_scale == 1) {
            LCD_Draw_Sprite(0, 0, bg_img->height, bg_img->width, bg_img->data);
        } else {
            LCD_Draw_Sprite_Scaled(0, 0, bg_img->height, bg_img->width, bg_img->data, img_scale);
        }
    }
    LCD_Refresh(&cfg0);
    frame_bg_colour  = bg_colour;
    curr_dirty_n     = 0;
    dirty_overflow   = 0;
    frame_start_tick = HAL_GetTick();
}

// ===== Text Drawing Helpers =====

void Renderer_DrawTextCentered(const char *str, uint16_t y, uint8_t colour, uint8_t font_size) {
    if (!str) return;

    uint16_t len = (uint16_t)strlen(str);
    uint16_t text_width = len * CHAR_WIDTH_BASE * font_size;

    int16_t x = ((int16_t)SCREEN_WIDTH - (int16_t)text_width) / 2;
    if (x < 0) x = 0;

    mark_dirty((uint16_t)x, y, text_width, (uint16_t)(7 * font_size));
    LCD_printString(str, (uint16_t)x, y, colour, font_size);
}

void Renderer_DrawInt(int32_t value, uint16_t x, uint16_t y, uint8_t colour, uint8_t font_size) {
    char buf[16];
    sprintf(buf, "%ld", (long)value);
    mark_dirty(x, y, (uint16_t)(strlen(buf) * CHAR_WIDTH_BASE * font_size),
                     (uint16_t)(7 * font_size));
    LCD_printString(buf, x, y, colour, font_size);
}

void Renderer_DrawLabelInt(const char *label, int32_t value, uint16_t x, uint16_t y, uint8_t colour, uint8_t font_size) {
    char buf[32];
    sprintf(buf, "%s: %ld", label, (long)value);
    mark_dirty(x, y, (uint16_t)(strlen(buf) * CHAR_WIDTH_BASE * font_size),
                     (uint16_t)(7 * font_size));
    LCD_printString(buf, x, y, colour, font_size);
}

// ===== UI Components =====

void Renderer_DrawProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                              int32_t current, int32_t max, uint8_t colour) {
    if (width < 2 || height < 2) return;
    mark_dirty(x, y, width, height);
    // Draw outer border
    Renderer_DrawBorder(x, y, width, height, colour);

    // Calculate fill width (leave 1px border on each side)
    if (max <= 0 || current <= 0) return;
    if (current > max) current = max;

    uint16_t inner_w = width - 2;
    uint16_t fill_w = (uint16_t)((uint32_t)inner_w * (uint32_t)current / (uint32_t)max);

    if (fill_w > 0) {
        LCD_Draw_Rect(x + 1, y + 1, fill_w, height - 2, colour, 1);
    }
}

void Renderer_DrawBorder(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t colour) {
    // Mark 4 thin edge rects instead of one large bounding box.
    // This avoids creating a huge dirty rect that forces full-screen clearing.
    mark_dirty(x, y, w, 1);             // Top edge
    mark_dirty(x, y + h - 1, w, 1);     // Bottom edge
    mark_dirty(x, y, 1, h);             // Left edge
    mark_dirty(x + w - 1, y, 1, h);     // Right edge
    // Top edge
    LCD_Draw_Line(x, y, x + w - 1, y, colour);
    // Bottom edge
    LCD_Draw_Line(x, y + h - 1, x + w - 1, y + h - 1, colour);
    // Left edge
    LCD_Draw_Line(x, y, x, y + h - 1, colour);
    // Right edge
    LCD_Draw_Line(x + w - 1, y, x + w - 1, y + h - 1, colour);
}

void Renderer_DrawHUD(const char *label, int32_t value, uint16_t y, uint8_t colour, uint8_t bg_colour) {
    mark_dirty(0, y, SCREEN_WIDTH, 14);
    // Draw background bar
    LCD_Draw_Rect(0, y, SCREEN_WIDTH, 14, bg_colour, 1);
    // Draw label + value
    Renderer_DrawLabelInt(label, value, 4, y + 2, colour, 1);
}

// ===== Screen Transitions =====

void Renderer_FlashScreen(uint8_t colour) {
    mark_dirty(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    LCD_Fill_Buffer(colour);
    LCD_Refresh(&cfg0);
}

void Renderer_WipeTransition(uint8_t colour, uint32_t duration_ms) {
    if (SCREEN_HEIGHT == 0) return;
    mark_dirty(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Number of steps (each step draws a band of rows)
    const uint16_t steps = 24;
    uint16_t rows_per_step = SCREEN_HEIGHT / steps;
    uint32_t delay_per_step = duration_ms / steps;

    for (uint16_t s = 0; s < steps; s++) {
        uint16_t y_start = s * rows_per_step;
        uint16_t band_h  = (s == steps - 1) ? (SCREEN_HEIGHT - y_start) : rows_per_step;

        LCD_Draw_Rect(0, y_start, SCREEN_WIDTH, band_h, colour, 1);
        LCD_Refresh(&cfg0);

        if (delay_per_step > 0) {
            HAL_Delay(delay_per_step);
        }
    }
}

void Renderer_BlinkScreen(uint8_t colour1, uint8_t colour2, uint8_t blinks, uint32_t interval_ms) {
    mark_dirty(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    for (uint8_t i = 0; i < blinks; i++) {
        LCD_Fill_Buffer(colour1);
        LCD_Refresh(&cfg0);
        HAL_Delay(interval_ms);

        LCD_Fill_Buffer(colour2);
        LCD_Refresh(&cfg0);
        HAL_Delay(interval_ms);
    }
}

// ======================================================================
//  DRAWING PRIMITIVES
// ======================================================================

void Renderer_DrawPixel(uint16_t x, uint16_t y, uint8_t colour) {
    mark_dirty(x, y, 1, 1);
    LCD_Set_Pixel(x, y, colour);
}

uint8_t Renderer_GetPixel(uint16_t x, uint16_t y) {
    return LCD_Get_Pixel(x, y);
}

void Renderer_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t colour) {
    uint16_t bx = x0 < x1 ? x0 : x1;
    uint16_t by = y0 < y1 ? y0 : y1;
    mark_dirty(bx, by,
               (uint16_t)((x0 > x1 ? x0 - x1 : x1 - x0) + 1),
               (uint16_t)((y0 > y1 ? y0 - y1 : y1 - y0) + 1));
    LCD_Draw_Line(x0, y0, x1, y1, colour);
}

void Renderer_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t colour) {
    mark_dirty(x, y, w, h);
    LCD_Draw_Rect(x, y, w, h, colour, 0);
}

void Renderer_DrawRectFilled(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t colour) {
    mark_dirty(x, y, w, h);
    LCD_Draw_Rect(x, y, w, h, colour, 1);
}

void Renderer_DrawCircle(uint16_t cx, uint16_t cy, uint16_t radius, uint8_t colour) {
    mark_dirty(cx > radius ? cx - radius : 0,
               cy > radius ? cy - radius : 0,
               2 * radius + 1, 2 * radius + 1);
    LCD_Draw_Circle(cx, cy, radius, colour, 0);
}

void Renderer_DrawCircleFilled(uint16_t cx, uint16_t cy, uint16_t radius, uint8_t colour) {
    mark_dirty(cx > radius ? cx - radius : 0,
               cy > radius ? cy - radius : 0,
               2 * radius + 1, 2 * radius + 1);
    LCD_Draw_Circle(cx, cy, radius, colour, 1);
}

void Renderer_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint8_t colour) {
    if (w == 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    if ((uint32_t)x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    mark_dirty(x, y, w, 1);
    LCD_Draw_Line(x, y, x + w - 1, y, colour);
}

void Renderer_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint8_t colour) {
    if (h == 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    if ((uint32_t)y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    mark_dirty(x, y, 1, h);
    LCD_Draw_Line(x, y, x, y + h - 1, colour);
}

void Renderer_DrawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           uint16_t x2, uint16_t y2, uint8_t colour) {
    uint16_t bx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    uint16_t by = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    uint16_t ex = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    uint16_t ey = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    mark_dirty(bx, by, ex - bx + 1, ey - by + 1);
    LCD_Draw_Line(x0, y0, x1, y1, colour);
    LCD_Draw_Line(x1, y1, x2, y2, colour);
    LCD_Draw_Line(x2, y2, x0, y0, colour);
}

void Renderer_FillScreen(uint8_t colour) {
    mark_dirty(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    LCD_Fill_Buffer(colour);
}

// ======================================================================
//  TEXT DRAWING HELPERS (extended)
// ======================================================================

void Renderer_DrawText(const char *str, uint16_t x, uint16_t y, uint8_t colour, uint8_t font_size) {
    if (!str) return;
    mark_dirty(x, y, (uint16_t)(strlen(str) * CHAR_WIDTH_BASE * font_size),
                     (uint16_t)(7 * font_size));
    LCD_printString(str, x, y, colour, font_size);
}

void Renderer_DrawTextRight(const char *str, uint16_t y, uint8_t colour, uint8_t font_size) {
    if (!str) return;

    uint16_t len = (uint16_t)strlen(str);
    uint16_t text_width = len * CHAR_WIDTH_BASE * font_size;

    int16_t x = (int16_t)SCREEN_WIDTH - (int16_t)text_width - 2;
    if (x < 0) x = 0;

    mark_dirty((uint16_t)x, y, text_width, (uint16_t)(7 * font_size));
    LCD_printString(str, (uint16_t)x, y, colour, font_size);
}

void Renderer_DrawIntCentered(int32_t value, uint16_t y, uint8_t colour, uint8_t font_size) {
    char buf[16];
    sprintf(buf, "%ld", (long)value);
    Renderer_DrawTextCentered(buf, y, colour, font_size);
}

void Renderer_DrawChar(char c, uint16_t x, uint16_t y, uint8_t colour) {
    mark_dirty(x, y, 5, 7);
    LCD_printChar(c, x, y, colour);
}

// ======================================================================
//  ADDITIONAL UI COMPONENTS
// ======================================================================

void Renderer_DrawProgressBarVertical(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                      int32_t current, int32_t max, uint8_t colour) {
    if (width < 2 || height < 2) return;
    mark_dirty(x, y, width, height);
    Renderer_DrawBorder(x, y, width, height, colour);

    if (max <= 0 || current <= 0) return;
    if (current > max) current = max;

    uint16_t inner_h = height - 2;
    uint16_t fill_h = (uint16_t)((uint32_t)inner_h * (uint32_t)current / (uint32_t)max);

    if (fill_h > 0) {
        // Fill from bottom upward
        uint16_t fill_y = y + 1 + (inner_h - fill_h);
        LCD_Draw_Rect(x + 1, fill_y, width - 2, fill_h, colour, 1);
    }
}

void Renderer_DrawGrid(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t cols, uint16_t rows, uint8_t colour) {
    if (cols == 0 || rows == 0 || w == 0 || h == 0) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    if ((uint32_t)x + w > SCREEN_WIDTH)  w = SCREEN_WIDTH  - x;
    if ((uint32_t)y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;

    // Outer border
    Renderer_DrawBorder(x, y, w, h, colour);

    // Vertical lines
    for (uint16_t c = 1; c < cols; c++) {
        uint16_t lx = x + (uint16_t)((uint32_t)w * c / cols);
        LCD_Draw_Line(lx, y, lx, y + h - 1, colour);
    }
    // Horizontal lines
    for (uint16_t r = 1; r < rows; r++) {
        uint16_t ly = y + (uint16_t)((uint32_t)h * r / rows);
        LCD_Draw_Line(x, ly, x + w - 1, ly, colour);
    }
}

void Renderer_DrawDashedHLine(uint16_t x, uint16_t y, uint16_t w, uint8_t colour, uint8_t dash) {
    if (dash == 0) dash = 1;
    if (w == 0 || y >= SCREEN_HEIGHT || x >= SCREEN_WIDTH) return;
    if ((uint32_t)x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    mark_dirty(x, y, w, 1);
    uint8_t draw = 1;
    for (uint16_t i = 0; i < w; i++) {
        if (draw) {
            LCD_Set_Pixel(x + i, y, colour);
        }
        if ((i + 1) % dash == 0) {
            draw = !draw;
        }
    }
}

// ======================================================================
//  IMAGE TRANSFORMS
// ======================================================================

void Renderer_DrawImageFlipV(const Image *img, uint16_t x, uint16_t y, uint8_t scale) {
    if (!img || !img->data || scale == 0 || img->width == 0 || img->height == 0) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    mark_dirty(x, y, (uint16_t)img->width * scale, (uint16_t)img->height * scale);

    for (uint16_t row = 0; row < img->height; row++) {
        uint16_t dst_row = img->height - 1 - row;
        uint16_t by = y + (uint16_t)(dst_row * scale);
        if (by >= SCREEN_HEIGHT) continue;
        for (uint16_t col = 0; col < img->width; col++) {
            uint8_t pixel = img->data[row * img->width + col];
            if (pixel == TRANSPARENT) continue;
            uint16_t bx = x + (uint16_t)(col * scale);
            if (bx >= SCREEN_WIDTH) continue;
            for (uint8_t dy = 0; dy < scale; dy++) {
                if (by + dy >= SCREEN_HEIGHT) break;
                for (uint8_t dx = 0; dx < scale; dx++) {
                    if (bx + dx < SCREEN_WIDTH)
                        LCD_Set_Pixel(bx + dx, by + dy, pixel);
                }
            }
        }
    }
}

// ======================================================================
//  PALETTE MANAGEMENT
// ======================================================================

void Renderer_SetPalette(LCD_Palette palette) {
    LCD_Set_Palette(palette);
}

// ======================================================================
//  COLLISION / GEOMETRY HELPERS
// ======================================================================

uint8_t Renderer_RectsOverlap(Rect a, Rect b) {
    if (a.x + (int16_t)a.w <= b.x) return 0;
    if (b.x + (int16_t)b.w <= a.x) return 0;
    if (a.y + (int16_t)a.h <= b.y) return 0;
    if (b.y + (int16_t)b.h <= a.y) return 0;
    return 1;
}

uint8_t Renderer_PointInRect(int16_t px, int16_t py, Rect r) {
    return (px >= r.x && px < r.x + (int16_t)r.w &&
            py >= r.y && py < r.y + (int16_t)r.h);
}

int16_t Renderer_Clamp(int16_t val, int16_t min, int16_t max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}
