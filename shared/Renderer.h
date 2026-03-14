#ifndef RENDERER_H
#define RENDERER_H

#include <stdint.h>
#include "LCD.h"
#include "ST7789V2_Driver.h"

// ===== Screen Constants =====
#define SCREEN_WIDTH   ST7789V2_WIDTH    // 240
#define SCREEN_HEIGHT  ST7789V2_HEIGHT   // 240

// ===== Transparent Colour =====
#define TRANSPARENT 255  // LCD driver uses 255 as transparent pixel

// Colour indices 0-15 map to the active palette.
// Default palette: 0=Black 1=White 2=Red 3=Green 4=Blue 5=Orange 6=Yellow
//                 7=Pink  8=Purple 9=Navy 10=Gold 11=Violet 12=Brown
//                 13=Grey 14=Cyan  15=Magenta
// Use raw numbers directly. 255 = TRANSPARENT (skip pixel).

// ======================================================================
//  IMAGE / SPRITE SYSTEM
// ======================================================================
//
//  PIXEL FORMAT
//  ~~~~~~~~~~~~
//  Each pixel is stored as a single uint8_t:
//    - 0~15  : colour palette index (mapped to RGB565 by current palette)
//    - 255   : transparent (pixel will not be drawn)
//
//  Pixel data is stored in **row-major** order (left-to-right, top-to-bottom).
//  Total size = width * height bytes.
//
//  HOW TO DEFINE AN IMAGE
//  ~~~~~~~~~~~~~~~~~~~~~~
//  Put image data in a `const uint8_t` array (stored in flash), then
//  create an `Image` with the `IMAGE()` macro:
//
//  ```c
//  // 1) Define pixel data (4x4 heart, stored in flash)
//  static const uint8_t heart_data[] = {
//      255,  3, 255,  3,   // row 0:  _  R  _  R
//       3,   3,  3,   3,   // row 1:  R  R  R  R
//      255,  3,  3, 255,   // row 2:  _  R  R  _
//      255, 255,  3, 255,  // row 3:  _  _  R  _
//  };
//  //  (3 = LCD_COLOUR_3(red by default), 255 = transparent)
//
//  // 2) Create an Image descriptor (also stored in flash)
//  static const Image heart = IMAGE(4, 4, heart_data);
//
//  // 3) Draw it in your Render function
//  Renderer_DrawImage(&heart, 100, 50);            // normal
//  Renderer_DrawImageScaled(&heart, 100, 50, 3);   // 3x scale
//  ```
//
//  DESIGNING LARGER IMAGES
//  ~~~~~~~~~~~~~~~~~~~~~~~
//  For images bigger than a few pixels, you can use the helper macro
//  `_` as a shorthand for transparent:
//
//  ```c
//  #define _ 255
//  static const uint8_t tree_data[] = {
//      _, _, 2, 2, 2, _, _,
//      _, 2, 2, 2, 2, 2, _,
//      2, 2, 2, 2, 2, 2, 2,
//      _, _, _, 6, _, _, _,
//      _, _, _, 6, _, _, _,
//  };
//  #undef _
//  static const Image tree = IMAGE(7, 5, tree_data);
//  ```
//
//  SPRITE SHEETS (ANIMATION)
//  ~~~~~~~~~~~~~~~~~~~~~~~~~
//  A SpriteSheet stores multiple equal-sized frames in one pixel array.
//  Frames are laid out top-to-bottom in the data.
//
//  ```c
//  // 3 frames of 8x8 animation
//  static const uint8_t run_data[] = {
//      // --- frame 0 (8 rows x 8 cols = 64 bytes) ---
//      ...
//      // --- frame 1 ---
//      ...
//      // --- frame 2 ---
//      ...
//  };
//  static const SpriteSheet run_sheet = SPRITESHEET(8, 8, 3, run_data);
//
//  // Draw frame 1 at (50, 80)
//  Renderer_DrawFrame(&run_sheet, 1, 50, 80);
//  ```
// ======================================================================

/**
 * @struct Image
 * @brief Describes a rectangular image stored as palette-indexed pixel data
 */
typedef struct {
    uint16_t        width;    // Image width in pixels
    uint16_t        height;   // Image height in pixels
    const uint8_t  *data;     // Pointer to row-major pixel array (in flash)
} Image;

/**
 * @struct SpriteSheet
 * @brief Multiple equal-sized frames packed into a single pixel array
 */
typedef struct {
    uint16_t        frame_w;      // Width of one frame
    uint16_t        frame_h;      // Height of one frame
    uint16_t        frame_count;  // Total number of frames
    const uint8_t  *data;         // Pointer to all frames (frame_w * frame_h * frame_count bytes)
} SpriteSheet;

/**
 * @brief Helper macro to initialise an Image from a pixel array
 * @param w     Width in pixels
 * @param h     Height in pixels
 * @param arr   Pointer to const uint8_t pixel array
 */
#define IMAGE(w, h, arr)  { .width = (w), .height = (h), .data = (arr) }

/**
 * @brief Helper macro to initialise a SpriteSheet
 * @param fw    Frame width
 * @param fh    Frame height
 * @param n     Number of frames
 * @param arr   Pointer to const uint8_t pixel array
 */
#define SPRITESHEET(fw, fh, n, arr)  { .frame_w = (fw), .frame_h = (fh), .frame_count = (n), .data = (arr) }

// ===== Image Drawing Functions =====

/**
 * @brief Draw an image at (x, y) using its original colours
 * 
 * @param img   Pointer to Image descriptor
 * @param x     X position of top-left corner
 * @param y     Y position of top-left corner
 */
void Renderer_DrawImage(const Image *img, uint16_t x, uint16_t y);

/**
 * @brief Draw an image scaled by an integer factor
 * 
 * @param img   Pointer to Image descriptor
 * @param x     X position
 * @param y     Y position
 * @param scale Integer scale factor (1 = original, 2 = double, etc.)
 */
void Renderer_DrawImageScaled(const Image *img, uint16_t x, uint16_t y, uint8_t scale);

/**
 * @brief Draw an image with all non-transparent pixels replaced by a single colour
 * 
 * Useful for silhouettes, hit-flash effects, or recolouring icons.
 * 
 * @param img     Pointer to Image descriptor
 * @param x       X position
 * @param y       Y position
 * @param colour  Replacement colour index (0-15)
 */
void Renderer_DrawImageColour(const Image *img, uint16_t x, uint16_t y, uint8_t colour);

/**
 * @brief Draw an image scaled and with colour override
 * 
 * @param img     Pointer to Image descriptor
 * @param x       X position
 * @param y       Y position
 * @param colour  Replacement colour index (0-15)
 * @param scale   Integer scale factor
 */
void Renderer_DrawImageColourScaled(const Image *img, uint16_t x, uint16_t y, uint8_t colour, uint8_t scale);

/**
 * @brief Draw an image flipped horizontally (mirror)
 * 
 * @param img   Pointer to Image descriptor
 * @param x     X position of top-left corner (of the output)
 * @param y     Y position of top-left corner
 * @param scale Integer scale factor (1 = original)
 */
void Renderer_DrawImageFlipH(const Image *img, uint16_t x, uint16_t y, uint8_t scale);

/**
 * @brief Draw a single frame from a sprite sheet
 * 
 * @param sheet       Pointer to SpriteSheet descriptor
 * @param frame_idx   Frame index (0-based, clamped to frame_count-1)
 * @param x           X position
 * @param y           Y position
 */
void Renderer_DrawFrame(const SpriteSheet *sheet, uint16_t frame_idx, uint16_t x, uint16_t y);

/**
 * @brief Draw a single frame from a sprite sheet, scaled
 * 
 * @param sheet       Pointer to SpriteSheet descriptor
 * @param frame_idx   Frame index
 * @param x           X position
 * @param y           Y position
 * @param scale       Integer scale factor
 */
void Renderer_DrawFrameScaled(const SpriteSheet *sheet, uint16_t frame_idx, uint16_t x, uint16_t y, uint8_t scale);

/**
 * @brief Draw the appropriate animation frame based on elapsed time
 * 
 * Automatically selects a frame from the sprite sheet based on a counter
 * and the specified frames-per-cycle speed. Loops automatically.
 * 
 * @param sheet          Pointer to SpriteSheet descriptor
 * @param anim_counter   A counter that increments each game frame (e.g. from Renderer_GetFrameCount())
 * @param frames_per_img Number of game frames each sprite frame stays visible
 * @param x              X position
 * @param y              Y position
 * @param scale          Integer scale factor
 */
void Renderer_DrawAnim(const SpriteSheet *sheet, uint32_t anim_counter, uint16_t frames_per_img,
                       uint16_t x, uint16_t y, uint8_t scale);

// ===== Image Utility =====

/**
 * @brief Create a temporary Image view for one frame of a SpriteSheet
 * 
 * Useful if you need to pass a single frame to a function that takes Image*.
 * The returned Image points into the sheet's data - do not free it.
 * 
 * @param sheet       Pointer to SpriteSheet
 * @param frame_idx   Frame index
 * @return Image      Image descriptor for that frame
 */
Image Renderer_GetFrameAsImage(const SpriteSheet *sheet, uint16_t frame_idx);

// ===== Frame Management =====

/**
 * @brief Begin a new frame
 * 
 * Clears the screen buffer and records the frame start time.
 * Call this at the top of each frame iteration.
 */
void Renderer_BeginFrame(void);

/**
 * @brief End the current frame and sync to target frame time
 * 
 * Sends the buffer to the LCD and waits if the frame completed early
 * to maintain a consistent frame rate.
 * 
 * @param target_ms Target frame duration in milliseconds
 *                  (e.g. 30 = ~33 FPS, 16 = ~60 FPS)
 * @return uint32_t Actual frame duration in milliseconds (useful for debugging)
 */
uint32_t Renderer_EndFrame(uint32_t target_ms);

/**
 * @brief Get the actual duration of the last completed frame
 * @return uint32_t Duration in milliseconds
 */
uint32_t Renderer_GetLastFrameTime(void);

/**
 * @brief Get the total number of frames rendered since last reset
 * @return uint32_t Frame count
 */
uint32_t Renderer_GetFrameCount(void);

/**
 * @brief Reset the frame counter to zero
 */
void Renderer_ResetFrameCount(void);

/**
 * @brief Set the background colour used to erase dirty regions at frame start
 *
 * Renderer_BeginFrame() clears only the pixel regions drawn in the previous
 * frame, filling them with this colour. Default is 0 (black).
 *
 * @param colour  Palette colour index (0-15)
 */
void Renderer_SetBgColour(uint8_t colour);

/**
 * @brief Initialise the screen with a background colour and optional background image
 *
 * Fills the entire framebuffer with bg_colour, optionally draws bg_img on top,
 * pushes the result to the LCD, sets the background-erase colour, and resets
 * dirty-rect tracking.
 *
 * Call this once at the start of a game before the main render loop.
 * After this, draw static elements (borders, labels, etc.) directly via
 * LCD_Draw_* functions — they persist in the framebuffer without being tracked
 * as dirty, so they will never be erased between frames.
 *
 * @param bg_colour  Palette colour index used to fill the screen
 *                   (also used to erase dirty rects each frame)
 * @param bg_img     Pointer to a background Image drawn at (0,0) after the fill.
 *                   Pass NULL to use a solid colour only.
 * @param img_scale  Integer scale factor for the background image (1 = original).
 *                   Ignored when bg_img is NULL.
 */
void Renderer_InitScreen(uint8_t bg_colour, const Image *bg_img, uint8_t img_scale);

// ===== Text Drawing Helpers =====

/**
 * @brief Draw text horizontally centered on screen
 * 
 * Automatically calculates x position based on string length and font size.
 * Each character is 5 pixels wide (+1 pixel spacing) at font_size=1.
 * 
 * @param str       Text string to draw
 * @param y         Y position (top of text)
 * @param colour    Colour index (0-15)
 * @param font_size Font scale factor (1, 2, 3, ...)
 */
void Renderer_DrawTextCentered(const char *str, uint16_t y, uint8_t colour, uint8_t font_size);

/**
 * @brief Draw an integer value as text
 * 
 * Converts the integer to a string and renders it at the given position.
 * Handles negative numbers. Max display: 11 digits + sign.
 * 
 * @param value     Integer value to display
 * @param x         X position
 * @param y         Y position
 * @param colour    Colour index (0-15)
 * @param font_size Font scale factor
 */
void Renderer_DrawInt(int32_t value, uint16_t x, uint16_t y, uint8_t colour, uint8_t font_size);

/**
 * @brief Draw a key-value pair, e.g. "Score: 42"
 * 
 * @param label     Label string (e.g. "Score")
 * @param value     Integer value
 * @param x         X position
 * @param y         Y position
 * @param colour    Colour index (0-15)
 * @param font_size Font scale factor
 */
void Renderer_DrawLabelInt(const char *label, int32_t value, uint16_t x, uint16_t y, uint8_t colour, uint8_t font_size);

// ===== UI Components =====

/**
 * @brief Draw a horizontal progress bar
 * 
 * Renders a bordered rectangular bar filled proportionally to current/max.
 * 
 * @param x        X position of the bar (top-left)
 * @param y        Y position of the bar (top-left)
 * @param width    Total width in pixels
 * @param height   Total height in pixels
 * @param current  Current value
 * @param max      Maximum value (if 0, bar is empty)
 * @param colour   Fill colour index (0-15)
 */
void Renderer_DrawProgressBar(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                              int32_t current, int32_t max, uint8_t colour);

/**
 * @brief Draw a rectangular border (outline only)
 * 
 * @param x      X of top-left corner
 * @param y      Y of top-left corner
 * @param w      Width
 * @param h      Height
 * @param colour Colour index (0-15)
 */
void Renderer_DrawBorder(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t colour);

/**
 * @brief Draw a simple HUD bar at the top or bottom of the screen
 * 
 * Shows a label + integer value, useful for persistent score/timer display.
 * 
 * @param label     Label text (e.g. "HP" or "Score")
 * @param value     Integer value to show
 * @param y         Y position of the HUD bar
 * @param colour    Text colour index (0-15)
 * @param bg_colour Background fill colour index (0-15), set same as screen bg to skip
 */
void Renderer_DrawHUD(const char *label, int32_t value, uint16_t y, uint8_t colour, uint8_t bg_colour);

// ===== Screen Transitions =====

/**
 * @brief Fill the entire screen with a colour and refresh immediately
 * 
 * Useful for flash effects or quick screen clears between scenes.
 * This is a blocking call that writes directly to the LCD.
 * 
 * @param colour Colour index (0-15)
 */
void Renderer_FlashScreen(uint8_t colour);

/**
 * @brief Wipe transition: progressively fills the screen from top to bottom
 * 
 * Blocking call that takes approximately duration_ms to complete.
 * Useful for scene transitions.
 * 
 * @param colour      Fill colour index (0-15)
 * @param duration_ms Total duration of the wipe in milliseconds
 */
void Renderer_WipeTransition(uint8_t colour, uint32_t duration_ms);

/**
 * @brief Blink the screen between two colours
 * 
 * Blocking call. Useful for hit effects or alerts.
 * 
 * @param colour1    First colour index
 * @param colour2    Second colour index
 * @param blinks     Number of blink cycles
 * @param interval_ms Duration of each half-blink in milliseconds
 */
void Renderer_BlinkScreen(uint8_t colour1, uint8_t colour2, uint8_t blinks, uint32_t interval_ms);

// ======================================================================
//  DRAWING PRIMITIVES
// ======================================================================

/**
 * @brief Set a single pixel in the buffer
 * @param x      X coordinate (0-239)
 * @param y      Y coordinate (0-239)
 * @param colour Palette index (0-15)
 */
void Renderer_DrawPixel(uint16_t x, uint16_t y, uint8_t colour);

/**
 * @brief Read the colour of a pixel from the buffer
 * @param x X coordinate
 * @param y Y coordinate
 * @return  Palette index (0-15)
 */
uint8_t Renderer_GetPixel(uint16_t x, uint16_t y);

/**
 * @brief Draw a line between two points
 * @param x0     Start X
 * @param y0     Start Y
 * @param x1     End X
 * @param y1     End Y
 * @param colour Palette index (0-15)
 */
void Renderer_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t colour);

/**
 * @brief Draw a rectangle outline (no fill)
 * @param x      Top-left X
 * @param y      Top-left Y
 * @param w      Width in pixels
 * @param h      Height in pixels
 * @param colour Palette index (0-15)
 */
void Renderer_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t colour);

/**
 * @brief Draw a filled rectangle
 * @param x      Top-left X
 * @param y      Top-left Y
 * @param w      Width in pixels
 * @param h      Height in pixels
 * @param colour Palette index (0-15)
 */
void Renderer_DrawRectFilled(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t colour);

/**
 * @brief Draw a circle outline
 * @param cx     Centre X
 * @param cy     Centre Y
 * @param radius Radius in pixels
 * @param colour Palette index (0-15)
 */
void Renderer_DrawCircle(uint16_t cx, uint16_t cy, uint16_t radius, uint8_t colour);

/**
 * @brief Draw a filled circle
 * @param cx     Centre X
 * @param cy     Centre Y
 * @param radius Radius in pixels
 * @param colour Palette index (0-15)
 */
void Renderer_DrawCircleFilled(uint16_t cx, uint16_t cy, uint16_t radius, uint8_t colour);

/**
 * @brief Draw a horizontal line (faster than generic DrawLine)
 * @param x      Start X
 * @param y      Y coordinate
 * @param w      Width in pixels
 * @param colour Palette index (0-15)
 */
void Renderer_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint8_t colour);

/**
 * @brief Draw a vertical line (faster than generic DrawLine)
 * @param x      X coordinate
 * @param y      Start Y
 * @param h      Height in pixels
 * @param colour Palette index (0-15)
 */
void Renderer_DrawVLine(uint16_t x, uint16_t y, uint16_t h, uint8_t colour);

/**
 * @brief Draw a triangle outline
 * @param x0,y0  First vertex
 * @param x1,y1  Second vertex
 * @param x2,y2  Third vertex
 * @param colour Palette index (0-15)
 */
void Renderer_DrawTriangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                           uint16_t x2, uint16_t y2, uint8_t colour);

/**
 * @brief Fill the entire screen buffer with a colour (does not refresh)
 * @param colour Palette index (0-15)
 */
void Renderer_FillScreen(uint8_t colour);

// ======================================================================
//  TEXT DRAWING HELPERS (extended)
// ======================================================================

/**
 * @brief Draw text at a specific position
 * @param str       String to render
 * @param x         X position
 * @param y         Y position
 * @param colour    Palette index (0-15)
 * @param font_size Font scale factor (1, 2, 3 ...)
 */
void Renderer_DrawText(const char *str, uint16_t x, uint16_t y, uint8_t colour, uint8_t font_size);

/**
 * @brief Draw text right-aligned on screen
 * @param str       String to render
 * @param y         Y position
 * @param colour    Palette index (0-15)
 * @param font_size Font scale factor
 */
void Renderer_DrawTextRight(const char *str, uint16_t y, uint8_t colour, uint8_t font_size);

/**
 * @brief Draw an integer value centered on screen
 * @param value     Integer to display
 * @param y         Y position
 * @param colour    Palette index (0-15)
 * @param font_size Font scale factor
 */
void Renderer_DrawIntCentered(int32_t value, uint16_t y, uint8_t colour, uint8_t font_size);

/**
 * @brief Draw a single character
 * @param c      Character to draw
 * @param x      X position
 * @param y      Y position
 * @param colour Palette index (0-15)
 */
void Renderer_DrawChar(char c, uint16_t x, uint16_t y, uint8_t colour);

// ======================================================================
//  ADDITIONAL UI COMPONENTS
// ======================================================================

/**
 * @brief Draw a vertical progress bar (fills bottom-to-top)
 * @param x       Top-left X
 * @param y       Top-left Y
 * @param width   Width in pixels
 * @param height  Height in pixels
 * @param current Current value
 * @param max     Maximum value
 * @param colour  Fill colour index (0-15)
 */
void Renderer_DrawProgressBarVertical(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                                      int32_t current, int32_t max, uint8_t colour);

/**
 * @brief Draw a grid of evenly-spaced lines
 * @param x      Top-left X
 * @param y      Top-left Y
 * @param w      Total width
 * @param h      Total height
 * @param cols   Number of columns
 * @param rows   Number of rows
 * @param colour Line colour index (0-15)
 */
void Renderer_DrawGrid(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t cols, uint16_t rows, uint8_t colour);

/**
 * @brief Draw a dotted / dashed horizontal line
 * @param x      Start X
 * @param y      Y coordinate
 * @param w      Width
 * @param colour Palette index (0-15)
 * @param dash   Dash length in pixels (gap = same length)
 */
void Renderer_DrawDashedHLine(uint16_t x, uint16_t y, uint16_t w, uint8_t colour, uint8_t dash);

// ======================================================================
//  IMAGE TRANSFORMS
// ======================================================================

/**
 * @brief Draw an image flipped vertically (upside-down)
 * @param img   Pointer to Image descriptor
 * @param x     X position of top-left corner (of the output)
 * @param y     Y position of top-left corner
 * @param scale Integer scale factor (1 = original)
 */
void Renderer_DrawImageFlipV(const Image *img, uint16_t x, uint16_t y, uint8_t scale);

// ======================================================================
//  PALETTE MANAGEMENT
// ======================================================================

/**
 * @brief Switch the active colour palette
 * 
 * Palette affects how colour indices 0-15 map to RGB565.
 * Buffer content stays the same, only displayed colours change.
 * 
 * @param palette One of PALETTE_DEFAULT, PALETTE_GREYSCALE, PALETTE_VINTAGE, PALETTE_CUSTOM
 */
void Renderer_SetPalette(LCD_Palette palette);

// ======================================================================
//  COLLISION / GEOMETRY HELPERS (useful for game logic)
// ======================================================================

/**
 * @brief Simple AABB rectangle structure for collision checks
 */
typedef struct {
    int16_t x, y;
    uint16_t w, h;
} Rect;

/**
 * @brief Check if two rectangles overlap (AABB collision)
 * @param a First rectangle
 * @param b Second rectangle
 * @return 1 if overlapping, 0 otherwise
 */
uint8_t Renderer_RectsOverlap(Rect a, Rect b);

/**
 * @brief Check if a point is inside a rectangle
 * @param px Point X
 * @param py Point Y
 * @param r  Rectangle
 * @return 1 if inside, 0 otherwise
 */
uint8_t Renderer_PointInRect(int16_t px, int16_t py, Rect r);

/**
 * @brief Clamp a value between min and max
 * @param val Value to clamp
 * @param min Minimum allowed
 * @param max Maximum allowed
 * @return Clamped value
 */
int16_t Renderer_Clamp(int16_t val, int16_t min, int16_t max);

#endif // RENDERER_H
