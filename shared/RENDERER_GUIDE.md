# Renderer API 使用指南

> 适用于 ELEC2645 Group Project — STM32L476 游戏机  
> 屏幕：ST7789V2，240×240 像素，RGB565（通过 4-bit 调色板访问）

---

## 目录

1. [快速开始](#1-快速开始)
2. [颜色系统](#2-颜色系统)
3. [帧管理](#3-帧管理)
4. [静态背景工作流](#4-静态背景工作流)
5. [绘图基元](#5-绘图基元)
6. [文本绘制](#6-文本绘制)
7. [图片与精灵](#7-图片与精灵)
8. [动画精灵表](#8-动画精灵表)
9. [UI 组件](#9-ui-组件)
10. [屏幕过渡特效](#10-屏幕过渡特效)
11. [调色板管理](#11-调色板管理)
12. [碰撞与几何工具](#12-碰撞与几何工具)
13. [完整游戏循环示例](#13-完整游戏循环示例)

---

## 1. 快速开始

在游戏文件顶部包含头文件：

```c
#include "Renderer.h"
```

每帧的标准结构：

```c
// --- 初始化时调用一次 ---
void Game_Init(void) {
    Renderer_InitScreen(0, NULL, 0);    // 0 = Black 背景，无背景图
    // 可在此用 LCD_Draw_* 绘制静态元素（边框等）
    Renderer_ResetFrameCount();
}

// --- 每帧调用 ---
void Game_Render(void) {
    Renderer_BeginFrame();          // 清除上帧脏区，记录帧开始时间

    // --- 只在这里绘制动态内容 ---
    Renderer_DrawTextCentered("Hello World", 110, 1, 2);   // 1 = White

    Renderer_EndFrame(30);          // 刷新到 LCD，并维持 30ms 帧时（约 33 FPS）
}
```

> **规则：所有绘制函数必须在 `Renderer_BeginFrame()` 之后、`Renderer_EndFrame()` 之前调用。不变的背景只在初始化时画一次。**

> **补充（与当前实现一致）：若在动态渲染中直接调用 `LCD_Draw_*`，这些像素不会被 Renderer 脏区系统记录，下一帧不会被自动擦除。**

---

## 2. 颜色系统

### 2.1 调色板原理

LCD 驱动使用 **4-bit 调色板**：每个像素用 0~15 的索引存储，刷新时由调色板映射为真实 RGB565 颜色。  
这意味着你只能**同时**在屏幕上显示最多 16 种颜色，但切换调色板可以整体改变风格。

### 2.2 默认调色板颜色索引

直接在函数中传入 **0~15** 整数，以下是对应关系：

| 索引 | 颜色名    | RGB565 (approximate) |
|:----:|:----------|:---------------------|
| 0    | Black     | #000000              |
| 1    | White     | #FFFFFF              |
| 2    | Red       | #F80000              |
| 3    | Green     | #07E000              |
| 4    | Blue      | #0000F8              |
| 5    | Orange    | #FD2000              |
| 6    | Yellow    | #FFE000              |
| 7    | Pink      | #FC18A0              |
| 8    | Purple    | #780F10              |
| 9    | Navy      | #000F00              |
| 10   | Gold      | #FEA000              |
| 11   | Violet    | #915C60              |
| 12   | Brown     | #A14540              |
| 13   | Grey      | #841084              |
| 14   | Cyan      | #07FFE0              |
| 15   | Magenta   | #F81FE0              |

> 切换调色板（`Renderer_SetPalette`）后同一索引对应不同 RGB 颜色。

### 2.3 透明色

```c
#define TRANSPARENT 255   // 用于图片/精灵数据，该像素不会被绘制
```

### 2.4 屏幕尺寸常量

```c
SCREEN_WIDTH   // 240
SCREEN_HEIGHT  // 240
```

---

## 3. 帧管理

### `Renderer_BeginFrame()`

清除**上一帧被修改过的区域**（脏区块），将它们还原为背景色，并记录当前时间作为帧起点。

> **注意**：`BeginFrame` 只清除上一帧绘制过的区域，不会清空整个屏幕。  
> 这是避免闪屏的关键——全屏清除会导致整帧黑屏闪烁。

```c
Renderer_BeginFrame();
```

### `Renderer_EndFrame(target_ms)`
将缓冲区推送到 LCD，如帧时不足则等待以维持目标帧率。

```c
uint32_t actual_ms = Renderer_EndFrame(30);  // 目标 30ms ≈ 33 FPS
```

| target_ms | FPS（近似） |
|:---------:|:-----------:|
| 16        | ~60 FPS     |
| 30        | ~33 FPS     |
| 50        | ~20 FPS     |
| 0         | 不限速      |

> 当前实现中 `Renderer_EndFrame()` 每帧调用 `LCD_Refresh(&cfg0)`。LCD 驱动会根据行变更标记决定实际发送的行数。

### `Renderer_SetBgColour(colour)`

设置背景擦除色——`BeginFrame` 清除脏区块时用该颜色填充。默认为 `0`（黑色）。

```c
Renderer_SetBgColour(9);  // 9 = Navy，背景擦除为深蓝色
```

### 帧计数与计时

```c
uint32_t Renderer_GetFrameCount(void);      // 从上次重置起的总帧数
uint32_t Renderer_GetLastFrameTime(void);   // 上一帧实际耗时（ms）
void     Renderer_ResetFrameCount(void);    // 重置帧计数为 0
```

**用途示例：**

```c
// 每 60 帧切换一次动画
uint32_t frame = Renderer_GetFrameCount();
uint8_t is_even_second = (frame / 60) % 2;

// 显示 FPS（调试用）
Renderer_DrawLabelInt("FPS", 1000 / Renderer_GetLastFrameTime(), 4, 4, 1, 1);  // 1 = White
```

---

## 4. 静态背景工作流

对于有固定背景的游戏，使用 `Renderer_InitScreen` 可以大幅减少每帧的像素操作，消除闪屏。

### `Renderer_InitScreen(bg_colour, bg_img, img_scale)`

在游戏初始化时调用一次：
1. 用 `bg_colour` 填充整个帧缓冲
2. 如果提供了 `bg_img`，将其绘制在 (0, 0) 处（透明像素显示 `bg_colour`）
3. 立即推送到 LCD
4. 设置背景擦除色（`BeginFrame` 将用该色填充脏区）
5. 重置脏区块状态

参数细节（与 `Renderer.c` 一致）：
- `bg_img == NULL`：仅使用纯色背景，`img_scale` 被忽略。
- `bg_img != NULL` 且 `img_scale == 0`：不会绘制背景图。
- `bg_img != NULL` 且 `img_scale >= 1`：按给定倍率在 `(0,0)` 绘制背景图。

```c
// 纯色背景（无背景图）
Renderer_InitScreen(9, NULL, 0);          // 9 = Navy

// 带背景图（原始大小）
Renderer_InitScreen(0, &bg_image, 1);     // 0 = Black 作为透明像素的填充色

// 带背景图（2倍放大）
Renderer_InitScreen(0, &bg_image, 2);
```

### 静态元素（初始化时绘制，永久保留）

调用 `Renderer_InitScreen` 之后，用底层 `LCD_Draw_*` 绘制静态元素（边框、地图、标题等）。这些元素**不被脏区跟踪**，永远不会被 `BeginFrame` 擦除：

```c
void MyGame_Init(void) {
    Renderer_InitScreen(9, NULL, 0);  // Navy 背景

    // 静态元素：写入帧缓冲后永久保留
    LCD_Draw_Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0); // 白色边框
    LCD_printString("MY GAME", 72, 8, 6, 2);                // 固定标题

    Renderer_ResetFrameCount();
}

void MyGame_Render(void) {
    Renderer_BeginFrame();          // 只清除上帧的动态脏区

    // 只在这里画每帧变化的内容（精灵、分数、动画等）
    Renderer_DrawImageScaled(&player, (uint16_t)px, (uint16_t)py, 2);
    Renderer_DrawInt(score, 4, 4, 1, 1);

    Renderer_EndFrame(30);
}
```

### 每帧开销对比

| 方式 | LCD_Set_Pixel 调用（约） | SPI 传输行数 |
|---|---|---|
| ❌ 旧方式：每帧全屏 DrawRectFilled | ~120,000 次 | 240 行（全屏） |
| ✅ 新方式：InitScreen + 只画动态元素 | ~3,000 次 | 仅脏区所在行 |

> **规则：不变的内容在 `Init()` 里画一次；只把每帧会移动或变化的元素放到 `Render()` 里。**

---

## 5. 绘图基元

所有坐标以左上角 (0, 0) 为原点，向右为 X+，向下为 Y+。

### 像素

```c
Renderer_DrawPixel(x, y, colour);         // 画单个像素
uint8_t c = Renderer_GetPixel(x, y);      // 读取像素颜色索引
```

### 直线

```c
Renderer_DrawLine(x0, y0, x1, y1, colour);   // 任意两点直线
Renderer_DrawHLine(x, y, w, colour);          // 水平线（更快）
Renderer_DrawVLine(x, y, h, colour);          // 垂直线（更快）
```

### 矩形

```c
Renderer_DrawRect(x, y, w, h, colour);        // 矩形边框（空心）
Renderer_DrawRectFilled(x, y, w, h, colour);  // 填充矩形（实心）
Renderer_DrawBorder(x, y, w, h, colour);      // 同 DrawRect，语义更清晰
```

### 圆形

```c
Renderer_DrawCircle(cx, cy, radius, colour);         // 圆形边框
Renderer_DrawCircleFilled(cx, cy, radius, colour);   // 填充圆形
```

### 三角形

```c
Renderer_DrawTriangle(x0, y0, x1, y1, x2, y2, colour);  // 三角形边框
```

### 虚线

```c
// dash=3 表示：画3像素，空3像素，循环
Renderer_DrawDashedHLine(x, y, w, colour, dash);
```

### 填充整屏（不刷新）

```c
Renderer_FillScreen(0);   // 0=Black，只写缓冲区，需配合 EndFrame 或 FlashScreen 使用
```

---

## 6. 文本绘制

字体为内置 5×7 像素点阵，每个字符实际占 6×7 像素（含1像素间距）。

### 基础绘制

```c
// 指定位置绘制
Renderer_DrawText("Hello", x, y, 1, font_size);       // 1 = White

// 水平居中
Renderer_DrawTextCentered("GAME OVER", y, 2, 3);       // 2 = Red

// 右对齐
Renderer_DrawTextRight("100", y, 1, 2);                // 1 = White

// 单个字符
Renderer_DrawChar('A', x, y, 6);                       // 6 = Yellow
```

### 数字绘制

```c
// 绘制整数
Renderer_DrawInt(score, x, y, 1, 2);                  // 1 = White

// 居中整数
Renderer_DrawIntCentered(score, y, 1, 2);

// 标签 + 数字（如 "Score: 42"）
Renderer_DrawLabelInt("Score", score, x, y, 1, 1);
```

### font_size 参考

| font_size | 字符像素尺寸 | 一行最多字符数 |
|:---------:|:------------:|:--------------:|
| 1         | 5×7          | 40 个          |
| 2         | 10×14        | 20 个          |
| 3         | 15×21        | 13 个          |
| 4         | 20×28        | 10 个          |

---

## 7. 图片与精灵

### 7.1 像素格式

图片数据存为 `const uint8_t` 数组（存放在 Flash），**行主序**排列：

```
data[row * width + col]
```

- **0~15**：调色板颜色索引
- **255**：透明，绘制时跳过

### 7.2 定义图片

```c
// 用 #define _ 255 简化透明像素书写
#define _ 255

static const uint8_t coin_data[] = {
    _, 6, 6, _,   // row 0
    6, 1, 1, 6,   // row 1
    6, 1, 1, 6,   // row 2
    _, 6, 6, _,   // row 3
};
// 6=Yellow, 1=White, _=透明(255)

#undef _

static const Image coin = IMAGE(4, 4, coin_data);
```

宏 `IMAGE(宽, 高, 数组)` 展开为 `Image` 结构体初始化。

### 7.3 绘制图片

```c
Renderer_DrawImage(&coin, x, y);                          // 原始大小
Renderer_DrawImageScaled(&coin, x, y, 3);                 // 3倍放大
Renderer_DrawImageColour(&coin, x, y, 2);          // 2=Red，单色剪影
Renderer_DrawImageColourScaled(&coin, x, y, 2, 2); // 单色 + 放大
Renderer_DrawImageFlipH(&coin, x, y, 1);                  // 水平翻转
Renderer_DrawImageFlipV(&coin, x, y, 1);                  // 垂直翻转
```

### 7.4 设计较大的图片

推荐使用 `#define _ 255` + `#undef _` 提高可读性：

```c
#define _ 255
static const uint8_t spaceship_data[] = {
    // 11×9 像素飞船
    _, _, _, _, _, 1, _, _, _, _, _,
    _, _, _, _, 1, 1, 1, _, _, _, _,
    _, _, _, 1, 1, 1, 1, 1, _, _, _,
    _, 1, 1, 1, 1, 1, 1, 1, 1, 1, _,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    _, 1, 1, 1, 1, 1, 1, 1, 1, 1, _,
    _, _, _, 1, _, _, _, 1, _, _, _,
    _, _, 2, _, _, _, _, _, 2, _, _,
    _, 2, _, _, _, _, _, _, _, 2, _,
};
#undef _
static const Image spaceship = IMAGE(11, 9, spaceship_data);
```

---

## 8. 动画精灵表

### 8.1 定义精灵表

多帧动画的数据**从上到下**顺序排列（每帧 = frame_w × frame_h 字节）：

```c
#define _ 255
static const uint8_t walk_data[] = {
    // 帧 0（8×8）
    _, 1, 1, 1, 1, 1, 1, _,
    _, _, 1, 1, 1, 1, _, _,
    // ... 共 8 行
    
    // 帧 1（8×8）
    _, 1, 1, 1, 1, 1, 1, _,
    // ...
    
    // 帧 2（8×8）
    // ...
};
#undef _

static const SpriteSheet walk_sheet = SPRITESHEET(8, 8, 3, walk_data);
//                                                  宽  高  帧数  数据
```

### 8.2 绘制指定帧

```c
Renderer_DrawFrame(&walk_sheet, 0, x, y);           // 绘制第 0 帧（原始大小）
Renderer_DrawFrameScaled(&walk_sheet, 1, x, y, 2);  // 绘制第 1 帧，2倍
```

### 8.3 自动循环动画

```c
// 每 5 个游戏帧切换一次精灵帧，自动循环
Renderer_DrawAnim(&walk_sheet, Renderer_GetFrameCount(), 5, x, y, 2);
//                              ↑帧计数器              ↑每帧持续游戏帧数
```

| frames_per_img | 动画速度 (at 33FPS) |
|:--------------:|:-------------------:|
| 2              | 很快 (~16FPS)      |
| 5              | 适中 (~6.6FPS)     |
| 10             | 缓慢 (~3.3FPS)     |

### 8.4 将某帧提取为 Image

```c
Image frame1 = Renderer_GetFrameAsImage(&walk_sheet, 1);
Renderer_DrawImage(&frame1, x, y);  // 像普通图片一样使用
```

---

## 9. UI 组件

### 水平进度条（如血量、能量）

```c
// Renderer_DrawProgressBar(x, y, 总宽, 总高, 当前值, 最大值, 颜色)
Renderer_DrawProgressBar(10, 220, 100, 8, hp, max_hp, 2);  // 2 = Red
```

外观：`[████████░░░░]`

### 垂直进度条

```c
// 从底部向上填充（适合显示充能、能量槽等）
Renderer_DrawProgressBarVertical(5, 50, 10, 80, energy, max_energy, 14); // 14 = Cyan
```

### HUD 状态栏

```c
// 在指定 Y 位置绘制一条横向 HUD 栏，包含标签和数值
Renderer_DrawHUD("HP",    player_hp, 0,   1, 9);  // 1=White text, 9=Navy bg, 顶部
Renderer_DrawHUD("Score", score,     226, 1, 9);  // 底部
```

### 矩形边框

```c
Renderer_DrawBorder(x, y, w, h, 1);   // 1 = White，仅边框
```

### 网格

```c
// 绘制 4列 × 4行 的格子（如俄罗斯方块、棋盘）
Renderer_DrawGrid(0, 0, 240, 240, 4, 4, 13);  // 13 = Grey
```

> 当前实现中，`Renderer_DrawGrid()` 仅通过 `Renderer_DrawBorder()` 记录边框脏区，内部网格线由 `LCD_Draw_Line()` 直接绘制。建议将网格作为静态元素在 `Init()` 阶段绘制。

### 虚线分割线

```c
// 虚线 dash=4（4像素实 + 4像素空）
Renderer_DrawDashedHLine(0, 120, SCREEN_WIDTH, 13, 4);  // 13 = Grey
```

---

## 10. 屏幕过渡特效

> 所有过渡函数均为**阻塞调用**，执行期间游戏暂停。

### 闪屏（即时）

```c
Renderer_FlashScreen(1);   // 1 = White，瞬间填充并刷新
```

### 擦除过渡（从上到下）

```c
// 约 300ms 完成，逐行用黑色覆盖屏幕
Renderer_WipeTransition(0, 300);   // 0 = Black
```

### 闪烁特效

```c
// 在两种颜色间以 100ms 间隔闪烁 3 次（如受击效果）
Renderer_BlinkScreen(1, 0, 3, 100);   // 1=White ↔ 0=Black
```

**典型用法：**

```c
// 游戏结束时
Renderer_BlinkScreen(2, 0, 5, 80);   // 2=Red ↔ 0=Black
Renderer_WipeTransition(0, 400);      // 0=Black
// 然后显示结算界面...
```

---

## 11. 调色板管理

切换调色板会改变 0~15 号颜色对应的实际 RGB 显示，不影响缓冲区数据。

```c
Renderer_SetPalette(PALETTE_DEFAULT);    // 默认：高对比度彩色
Renderer_SetPalette(PALETTE_GREYSCALE);  // 16 级灰度
Renderer_SetPalette(PALETTE_VINTAGE);    // 复古游戏机风格
Renderer_SetPalette(PALETTE_CUSTOM);     // 自定义调色板（在 LCD.c 中修改）
```

**用途示例：**

```c
// 游戏暂停时切换为灰度，恢复时切换回彩色
if (paused) {
    Renderer_SetPalette(PALETTE_GREYSCALE);
} else {
    Renderer_SetPalette(PALETTE_DEFAULT);
}
```

---

## 12. 碰撞与几何工具

### `Rect` 结构体

```c
typedef struct {
    int16_t  x, y;    // 左上角坐标（可为负数）
    uint16_t w, h;    // 宽度和高度
} Rect;
```

### AABB 矩形碰撞检测

```c
Rect player = { player_x, player_y, 8, 8 };
Rect enemy  = { enemy_x,  enemy_y,  12, 12 };

if (Renderer_RectsOverlap(player, enemy)) {
    // 发生碰撞！
    player_hp--;
}
```

### 点是否在矩形内

```c
Rect button = { 80, 100, 80, 30 };
if (Renderer_PointInRect(cursor_x, cursor_y, button)) {
    // 光标在按钮上
}
```

### 数值钳制

```c
// 将玩家坐标限制在屏幕边界内
player_x = Renderer_Clamp(player_x, 0, SCREEN_WIDTH  - PLAYER_W);
player_y = Renderer_Clamp(player_y, 0, SCREEN_HEIGHT - PLAYER_H);
```

---

## 13. 完整游戏循环示例

下面是一个含图片、动画、HUD 和碰撞检测的完整示例：

```c
#include "Renderer.h"

// ===== 资源定义（放在文件顶部，存于 Flash）=====

#define _ 255

// 玩家角色（8×8，面朝右）
static const uint8_t player_data[] = {
    _, 1, 1, 1, 1, 1, _, _,
    1, 1, 3, 1, 1, 1, 1, _,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 6, 6, 6, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 3, 1, 1, 1, 1, _,
    _, 1, 1, 1, 1, 1, _, _,
    _, _, 1, _, _, 1, _, _,
};
static const Image player_img = IMAGE(8, 8, player_data);

// 硬币动画（6×6，2帧）
static const uint8_t coin_frames[] = {
    // 帧 0
    _, 6, 6, 6, 6, _,
    6, 1, 6, 6, 1, 6,
    6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6,
    6, 1, 6, 6, 1, 6,
    _, 6, 6, 6, 6, _,
    // 帧 1（压扁效果）
    _, _, _, _, _, _,
    6, 6, 6, 6, 6, 6,
    6, 1, 6, 6, 1, 6,
    6, 6, 6, 6, 6, 6,
    _, _, _, _, _, _,
    _, _, _, _, _, _,
};
static const SpriteSheet coin_sheet = SPRITESHEET(6, 6, 2, coin_frames);

#undef _

// ===== 游戏状态 =====
static int16_t px = 100, py = 100;  // 玩家位置
static int16_t cx = 50,  cy = 80;   // 硬币位置
static int32_t score = 0;
static int32_t hp = 5, max_hp = 5;

// ===== 游戏初始化（调用一次）=====
void Game_Init(void) {
    // Navy 背景 + 无背景图
    Renderer_InitScreen(9, NULL, 0);  // 9 = Navy

    // 静态元素：直接写入帧缓冲，永久保留
    LCD_Draw_Rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 1, 0);  // 白色边框

    Renderer_ResetFrameCount();
}

// ===== 游戏更新 =====
void Game_Update(InputState *input) {
    const int speed = 3;

    // 移动
    if (input->joystick == JOYSTICK_LEFT)  px -= speed;
    if (input->joystick == JOYSTICK_RIGHT) px += speed;
    if (input->joystick == JOYSTICK_UP)    py -= speed;
    if (input->joystick == JOYSTICK_DOWN)  py += speed;

    // 边界限制（在边框内 1px）
    px = Renderer_Clamp(px, 1, SCREEN_WIDTH  - 8 - 1);
    py = Renderer_Clamp(py, 15, SCREEN_HEIGHT - 8 - 15);

    // 碰撞检测：玩家 vs 硬币
    Rect pr = { px, py, 8, 8 };
    Rect cr = { cx, cy, 6, 6 };
    if (cx >= 0 && Renderer_RectsOverlap(pr, cr)) {
        score++;
        cx = -1;  // 隐藏硬币（已拾取）
    }
}

// ===== 游戏渲染 =====
void Game_Render(void) {
    Renderer_BeginFrame();    // 清除上帧脏区（还原为 Navy）

    // ---- 只画动态内容 ----
    // 静态边框已在 Game_Init() 中写入帧缓冲，无需重绘

    // 绘制硬币（带动画）
    if (cx >= 0) {
        Renderer_DrawAnim(&coin_sheet, Renderer_GetFrameCount(), 8, (uint16_t)cx, (uint16_t)cy, 2);
    }

    // 绘制玩家（向左移动时水平翻转）
    static int16_t last_dx = 1;
    if (last_dx >= 0) {
        Renderer_DrawImageScaled(&player_img, (uint16_t)px, (uint16_t)py, 2);
    } else {
        Renderer_DrawImageFlipH(&player_img, (uint16_t)px, (uint16_t)py, 2);
    }

    // HUD：顶部得分、底部血量
    Renderer_DrawHUD("Score", score, 0,                 1, 9); // 1=White, 9=Navy
    Renderer_DrawHUD("HP",    hp,    SCREEN_HEIGHT - 14, 1, 9);
    Renderer_DrawProgressBar(80, 2, 156, 10, hp, max_hp, 2);  // 2 = Red

    Renderer_EndFrame(30);
}
```

---

## 附录 A：函数速查表

### 帧管理
| 函数 | 说明 |
|---|---|
| `Renderer_BeginFrame()` | 清除上帧脏区，记录帧起点 |
| `Renderer_EndFrame(ms)` | 刷新 LCD，帧率控制 |
| `Renderer_SetBgColour(c)` | 设置脏区擦除背景色 |
| `Renderer_InitScreen(c,img,s)` | 初始化屏幕（背景色 + 可选背景图） |
| `Renderer_GetFrameCount()` | 获取总帧数 |
| `Renderer_GetLastFrameTime()` | 获取上帧耗时(ms) |
| `Renderer_ResetFrameCount()` | 重置帧计数 |

### 基本图形
| 函数 | 说明 |
|---|---|
| `Renderer_DrawPixel(x,y,c)` | 画像素 |
| `Renderer_GetPixel(x,y)` | 读像素 |
| `Renderer_DrawLine(x0,y0,x1,y1,c)` | 直线 |
| `Renderer_DrawHLine(x,y,w,c)` | 水平线 |
| `Renderer_DrawVLine(x,y,h,c)` | 垂直线 |
| `Renderer_DrawRect(x,y,w,h,c)` | 矩形边框 |
| `Renderer_DrawRectFilled(x,y,w,h,c)` | 填充矩形 |
| `Renderer_DrawBorder(x,y,w,h,c)` | 矩形边框（同 DrawRect） |
| `Renderer_DrawCircle(cx,cy,r,c)` | 圆形边框 |
| `Renderer_DrawCircleFilled(cx,cy,r,c)` | 填充圆形 |
| `Renderer_DrawTriangle(x0..x2,c)` | 三角形边框 |
| `Renderer_DrawDashedHLine(x,y,w,c,d)` | 水平虚线 |
| `Renderer_FillScreen(c)` | 填充整屏缓冲 |

### 文本
| 函数 | 说明 |
|---|---|
| `Renderer_DrawText(str,x,y,c,sz)` | 指定位置文字 |
| `Renderer_DrawTextCentered(str,y,c,sz)` | 居中文字 |
| `Renderer_DrawTextRight(str,y,c,sz)` | 右对齐文字 |
| `Renderer_DrawChar(ch,x,y,c)` | 单字符 |
| `Renderer_DrawInt(val,x,y,c,sz)` | 整数 |
| `Renderer_DrawIntCentered(val,y,c,sz)` | 居中整数 |
| `Renderer_DrawLabelInt(lbl,val,x,y,c,sz)` | 标签+整数 |

### 图片/精灵
| 函数 | 说明 |
|---|---|
| `Renderer_DrawImage(img,x,y)` | 绘制图片 |
| `Renderer_DrawImageScaled(img,x,y,s)` | 缩放绘制 |
| `Renderer_DrawImageColour(img,x,y,c)` | 单色剪影 |
| `Renderer_DrawImageColourScaled(img,x,y,c,s)` | 单色+缩放 |
| `Renderer_DrawImageFlipH(img,x,y,s)` | 水平翻转 |
| `Renderer_DrawImageFlipV(img,x,y,s)` | 垂直翻转 |
| `Renderer_DrawFrame(sheet,idx,x,y)` | 绘制指定帧 |
| `Renderer_DrawFrameScaled(sheet,idx,x,y,s)` | 缩放绘制帧 |
| `Renderer_DrawAnim(sheet,ctr,spd,x,y,s)` | 自动循环动画 |
| `Renderer_GetFrameAsImage(sheet,idx)` | 帧转 Image |

### UI 组件
| 函数 | 说明 |
|---|---|
| `Renderer_DrawProgressBar(x,y,w,h,cur,max,c)` | 水平进度条 |
| `Renderer_DrawProgressBarVertical(x,y,w,h,cur,max,c)` | 垂直进度条 |
| `Renderer_DrawHUD(lbl,val,y,c,bg)` | HUD 状态栏 |
| `Renderer_DrawGrid(x,y,w,h,cols,rows,c)` | 网格 |

### 过渡特效
| 函数 | 说明 |
|---|---|
| `Renderer_FlashScreen(c)` | 瞬间填色并刷新 |
| `Renderer_WipeTransition(c,ms)` | 擦除过渡 |
| `Renderer_BlinkScreen(c1,c2,n,ms)` | 闪烁效果 |

### 实现约束（建议阅读）
| 项目 | 当前实现行为 |
|---|---|
| 脏区数量上限 | 每帧最多记录 32 个脏区（`MAX_DIRTY_RECTS=32`） |
| 脏区溢出处理 | 若超过上限，下一次 `Renderer_BeginFrame()` 会退化为整屏背景清除 |
| 动态内容最佳实践 | 尽量合并绘制区域、避免大量碎片化小绘制调用 |
| 静态内容最佳实践 | 边框/背景/网格等不变化元素在 `Init()` 用 `LCD_Draw_*` 画一次 |

### 调色板
| 函数 | 说明 |
|---|---|
| `Renderer_SetPalette(p)` | 切换调色板 |

### 碰撞工具
| 函数 | 说明 |
|---|---|
| `Renderer_RectsOverlap(a,b)` | 矩形碰撞检测 |
| `Renderer_PointInRect(px,py,r)` | 点在矩形内？ |
| `Renderer_Clamp(val,min,max)` | 数值钳制 |
