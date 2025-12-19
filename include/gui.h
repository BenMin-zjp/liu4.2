/*
 * gui.h
 * SDL2 绘制相关：棋盘/棋子/菜单/比分等。
 */

#ifndef GUI_H
#define GUI_H

#include <SDL2/SDL.h>
#include "game.h"

/* ========== 窗口尺寸配置 ========== */

/* 窗口宽度（像素）；为了在多个源文件中共享统一的窗口宽度，这里定义宏。 */
#ifndef WINDOW_WIDTH
#define WINDOW_WIDTH 640
#endif

/* 窗口高度（像素）；为了在多个源文件中共享统一的窗口高度，这里定义宏。 */
#ifndef WINDOW_HEIGHT
#define WINDOW_HEIGHT 640
#endif

/* ========== 函数声明 ========== */

/* 初始化 SDL 窗口和渲染器；内部使用 SDL 库函数： */
int gui_init(SDL_Window **win, SDL_Renderer **ren);

/* 关闭并释放 SDL 和字体相关资源；内部使用以下函数： */
void gui_quit(SDL_Window *win, SDL_Renderer *ren);

/* 根据当前棋局绘制棋盘和棋子；内部使用 SDL 库函数： */
void draw_game(SDL_Renderer *ren, const GameState *game);

/* 将屏幕坐标（像素）转换为棋盘行列坐标；无（只使用了基本的数学运算） */
int pixel_to_cell(int x, int y, int *row, int *col);

/* 绘制游戏结束时的遮罩层；内部使用 SDL 库函数： */
void draw_game_over(SDL_Renderer *ren, int winner);

/* 绘制游戏结果界面（显示胜负/平局的一屏）。 */
void draw_game_result(SDL_Renderer *ren, int winner);

/* 绘制主菜单界面；内部使用 SDL 库函数： */
void draw_main_menu(SDL_Renderer *ren, int has_resume);

/* 人机难度选择菜单（从“人机对战”按钮点进去）。 */
void draw_ai_difficulty_menu(SDL_Renderer *ren);

/* 回放菜单：列出历史对局（第 N 轮）并提供删除按钮 */
void draw_playback_menu(SDL_Renderer *ren, int page, int total, int per_page);

/* 回放菜单：没有任何记录时的提示界面 */
void draw_playback_empty(SDL_Renderer *ren);

/* 绘制游戏结束后的菜单（再来一局/退出游戏）；内部使用 SDL 库函数： */
void draw_end_menu(SDL_Renderer *ren);

/* 在窗口顶部绘制黑白双方的比分；内部使用以下函数： */
void draw_scoreboard(SDL_Renderer *ren, int score_black, int score_white);

/* 右上角计时器（本局用时）。elapsed_seconds：从 0 开始累计的秒数 */
void draw_timer(SDL_Renderer *ren, int elapsed_seconds);

/* 显示悔棋次数（按一次算一次）。一般放在计时器旁边或下面。 */
void draw_undo_count(SDL_Renderer *ren, int undo_count);

#endif /* GUI_H */