#include "Game_1.h"
#include "InputHandler.h"
#include "Renderer.h"
#include "PWM.h"
#include "Buzzer.h"
#include <stdio.h>

extern PWM_cfg_t pwm_cfg;
extern Buzzer_cfg_t buzzer_cfg;

// =====================================================================
//  GAME 1 — Bouncing Ball Demo
//  Replace this with your own game logic.
//  This file shows how to use the Renderer API correctly.
//
//  Colour index quick reference (default palette):
//    0=Black  1=White  2=Red    3=Green  4=Blue   5=Orange
//    6=Yellow 7=Pink   8=Purple 9=Navy  10=Gold  11=Violet
//   12=Brown 13=Grey  14=Cyan  15=Magenta
// =====================================================================

// ----- Image / sprite data -----
// Each pixel is a palette index (0-15) or 255 (transparent).
// Stored in Flash as const arrays — row-major order.
#define _ 255
static const uint8_t ball_data[] = {
    _, 1, 1, _,
    1, 6, 6, 1,
    1, 6, 6, 1,
    _, 1, 1, _,
};
static const Image ball_img = IMAGE(4, 4, ball_data);
#undef _

// ----- Game state -----
static int16_t  ball_x, ball_y;
static int16_t  vel_x,  vel_y;

#define BALL_SCALE  4                        // displayed size = 4*4 * 4 = 16x16 px
#define BALL_SIZE   (4 * BALL_SCALE)         // 16 px
#define FRAME_MS    30                        // ~33 FPS

// ----- Internal helpers -----
static void Game1_Init(void) {
    ball_x = SCREEN_WIDTH  / 2 - BALL_SIZE / 2;
    ball_y = SCREEN_HEIGHT / 2 - BALL_SIZE / 2;
    vel_x  = 3;
    vel_y  = 2;

    // Initialise screen: fill navy, push to LCD, set bg-erase colour,
    // and reset dirty-rect tracking so nothing is "dirty" yet.
    Renderer_InitScreen(9, NULL, 0);  // 9 = Navy, no background image

    // Draw static border directly on framebuffer (not dirty-tracked).
    // It will persist unless a dirty-rect clear overlaps it.
    LCD_Draw_Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0);  // 1=White, outline

    Renderer_ResetFrameCount();
}

static void Game1_Update(void) {
    // Move
    ball_x += vel_x;
    ball_y += vel_y;

    // Bounce inside the border (1px margin keeps ball from overlapping border)
    if (ball_x <= 1)                              { ball_x = 1;                              vel_x = -vel_x; }
    if (ball_x >= SCREEN_WIDTH  - BALL_SIZE - 1)  { ball_x = SCREEN_WIDTH  - BALL_SIZE - 1;  vel_x = -vel_x; }
    if (ball_y <= 1)                              { ball_y = 1;                              vel_y = -vel_y; }
    if (ball_y >= SCREEN_HEIGHT - BALL_SIZE - 1)  { ball_y = SCREEN_HEIGHT - BALL_SIZE - 1;  vel_y = -vel_y; }

    // LED brightness tracks horizontal position (0-100 %)
    uint8_t brightness = (uint8_t)((ball_x * 100) / (SCREEN_WIDTH - BALL_SIZE));
    PWM_SetDuty(&pwm_cfg, brightness);
}

static void Game1_Render(void) {
    Renderer_BeginFrame();   // clears only previous dirty rects (to navy)

    // ---- Only draw DYNAMIC elements each frame ----
    // The navy background + white border are already in the framebuffer
    // from Game1_Init and persist automatically.

    // Ball sprite (4× scale)
    Renderer_DrawImageScaled(&ball_img, (uint16_t)ball_x, (uint16_t)ball_y, BALL_SCALE);

    // HUD — title at top, instructions at bottom
    Renderer_DrawTextCentered("GAME 1", 8, 6, 2);    // 6=Yellow, size 2
    Renderer_DrawTextCentered("BT3: Menu", 224, 13, 1); // 13=Grey, size 1

    // Frame counter (debug info)
    char buf[24];
    sprintf(buf, "F:%lu", Renderer_GetFrameCount());
    Renderer_DrawText(buf, 4, 224, 13, 1);            // 13=Grey, size 1

    Renderer_EndFrame(FRAME_MS);   // pushes buffer to LCD, waits for frame time
}

// ----- Public entry point -----
MenuState Game1_Run(void) {
    // Startup beep
    buzzer_tone(&buzzer_cfg, 880, 30);
    buzzer_off(&buzzer_cfg);

    Game1_Init();

    while (1) {
        Input_Read();

        if (current_input.btn3_pressed) {
            PWM_SetDuty(&pwm_cfg, 50);   // reset LED on exit
            return MENU_STATE_HOME;
        }

        Game1_Update();
        Game1_Render();
    }
}
