/*
 * gui.c
 *
 * 封装 SDL 相关的绘制函数，用于绘制棋盘、棋子等。
 * 由于没有使用额外的字体库，这里的结束提示采用控制台输出告知胜负。
 */

#include "gui.h"
#include <math.h>
#include <stdio.h>
#include <ctype.h>          // 为 toupper 函数补的头文件
#include <string.h>
#include <SDL2/SDL_ttf.h>   // 为 TTF_* 函数补的头文件

/* 菜单/结束界面用的宋体字体，全局只维护这一份 */
static TTF_Font *g_font_menu = NULL;

/* ========== 菜单背景图 ========== */
/*
 * SDL2 自带的图片加载接口 SDL_LoadBMP() 只支持 BMP。
 * 你提供的是 image/yinghua.jpg，所以我额外生成了一份 bmp：
 *     image/menu_bg.bmp
 *
 * 这里用“懒加载”：第一次画菜单时才读文件并创建纹理，后续直接复用。
 */
static SDL_Texture *g_menu_bg_tex = NULL;
static const char *MENU_BG_PATH = "image/menu_bg.bmp";

/* 确保菜单背景纹理已加载好。加载失败时返回 0（菜单会退回纯色背景）。 */
static int ensure_menu_background(SDL_Renderer *ren)
{
    if (!ren) return 0;
    if (g_menu_bg_tex) return 1;

    SDL_Surface *surf = SDL_LoadBMP(MENU_BG_PATH);
    if (!surf) {
        /* 这一步失败不致命：只是没背景图而已 */
        fprintf(stderr, "SDL_LoadBMP(%s) error: %s\n", MENU_BG_PATH, SDL_GetError());
        return 0;
    }

    g_menu_bg_tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_FreeSurface(surf);
    if (!g_menu_bg_tex) {
        fprintf(stderr, "SDL_CreateTextureFromSurface error: %s\n", SDL_GetError());
        return 0;
    }

    /* 背景图别太“抢戏”：略微调透明，让按钮和文字更舒服 */
    SDL_SetTextureBlendMode(g_menu_bg_tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(g_menu_bg_tex, 220);

    return 1;
}

/* 把背景图铺满整个窗口。 */
static void draw_menu_background(SDL_Renderer *ren)
{
    if (!ensure_menu_background(ren)) return;

    SDL_Rect dst = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderCopy(ren, g_menu_bg_tex, NULL, &dst);
}

/* 给背景盖一层半透明“雾”，让按钮文字更清楚。 */
static void draw_menu_fog(SDL_Renderer *ren, Uint8 alpha)
{
    if (!ren) return;
    SDL_Rect full = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, alpha);
    SDL_RenderFillRect(ren, &full);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
}



/* 保证字体已经初始化并加载；- TTF_WasInit() : SDL_ttf 库函数，检查 TTF 子系统是否已初始化 */
static int ensure_menu_font(void)
{
    if (!TTF_WasInit()) {
        if (TTF_Init() != 0) {
            fprintf(stderr, "TTF_Init error: %s\n", TTF_GetError());
            return 0;
        }
    }
    if (!g_font_menu) {
        const char *path = "C:\\\\Windows\\\\Fonts\\\\simsun.ttc"; /* 注意双反斜杠 */
        g_font_menu = TTF_OpenFont(path, 26);   /* 26 像素大小，你可以自己调 */
        if (!g_font_menu) {
            fprintf(stderr, "TTF_OpenFont error: %s\n", TTF_GetError());
            return 0;
        }
    }
    return 1;
}

/* 在给定矩形中居中绘制一行 UTF-8 文字；- TTF_RenderUTF8_Blended() : SDL_ttf 库函数，将 UTF-8 字符串渲染成带透明度的图像（Surface） */
static void draw_menu_text_center(SDL_Renderer *ren,
                                  const SDL_Rect *rect,
                                  const char *utf8,
                                  SDL_Color color)
{
    if (!ensure_menu_font()) return;

    SDL_Surface *surf = TTF_RenderUTF8_Blended(g_font_menu, utf8, color);
    if (!surf) {
        fprintf(stderr, "TTF_RenderUTF8_Blended error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }
    int tw, th;
    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);

    SDL_Rect dst;
    dst.w = tw;
    dst.h = th;
    dst.x = rect->x + (rect->w - tw) / 2;
    dst.y = rect->y + (rect->h - th) / 2;

    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}



/* 一些可调节的常量。
 * 窗口宽度和高度由 gui.h 定义的宏 WINDOW_WIDTH/WINDOW_HEIGHT 提供，
 * 这里不再定义独立的静态常量，以免与宏值不一致。BOARD_MARGIN 控制棋盘边缘留白。 */
static const int BOARD_MARGIN = 40;

/* 计算每个格子的间距（像素）；无（只使用了基本的数学运算） */
static int cell_size()
{
    return (WINDOW_WIDTH - 2 * BOARD_MARGIN) / (BOARD_SIZE - 1);
}

/* SDL 窗口和渲染器初始化；- SDL_CreateWindow() : SDL 库函数，创建窗口 */
int gui_init(SDL_Window **win, SDL_Renderer **ren)
{
    /*
     * 初始化窗口和渲染器。
     *
     * 由于 SDL 的初始化在 main 中统一完成，这里不再调用 SDL_Init。
     * 调用者必须确保在调用此函数之前已经通过 SDL_Init 初始化了视频子系统。
     */
    if (!win || !ren) return 1;
    *win = SDL_CreateWindow("六子棋", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!*win) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        return 1;
    }
    *ren = SDL_CreateRenderer(*win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!*ren) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(*win);
        return 1;
    }
    return 0;
}

/* 释放 SDL 和字体资源；- TTF_CloseFont() : SDL_ttf 库函数，关闭字体对象（释放字体资源） */
void gui_quit(SDL_Window *win, SDL_Renderer *ren)
{
    /* 先释放字体，再关 TTF 子系统 */
    if (g_font_menu) {
        TTF_CloseFont(g_font_menu);
        g_font_menu = NULL;
    }

    /* 菜单背景图纹理也要手动释放（纹理属于 renderer 的资源） */
    if (g_menu_bg_tex) {
        SDL_DestroyTexture(g_menu_bg_tex);
        g_menu_bg_tex = NULL;
    }
    if (TTF_WasInit()) {
        TTF_Quit();
    }

    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
}


/* 绘制填充圆（实心圆）；- sqrt() : 来自 <math.h>，计算平方根（用于计算圆的半径） */
static void draw_filled_circle(SDL_Renderer *ren, int cx, int cy, int r, SDL_Color color)
{
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    for (int dy = -r; dy <= r; dy++) {
        int dx_max = (int)sqrt((double)r * r - dy * dy);
        SDL_RenderDrawLine(ren, cx - dx_max, cy + dy, cx + dx_max, cy + dy);
    }
}

/* 绘制棋盘和棋子；- SDL_SetRenderDrawColor() : SDL 库函数，设置绘制颜色 */
void draw_game(SDL_Renderer *ren, const GameState *game)
{
    if (!ren || !game) return;
    int csize = cell_size();
    /* 背景色：略带木纹色调 */
    SDL_SetRenderDrawColor(ren, 240, 217, 181, 255);
    SDL_RenderClear(ren);

    /* 绘制网格线 */
    SDL_SetRenderDrawColor(ren, 80, 60, 40, 255);
    int start = BOARD_MARGIN;
    int end   = BOARD_MARGIN + csize * (BOARD_SIZE - 1);
    for (int i = 0; i < BOARD_SIZE; i++) {
        int pos = start + i * csize;
        /* 横线 */
        SDL_RenderDrawLine(ren, start, pos, end, pos);
        /* 竖线 */
        SDL_RenderDrawLine(ren, pos, start, pos, end);
    }

    /* 绘制棋子 */
    int radius = csize / 2 - 2;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->cells[r][c] != CELL_EMPTY) {
                int cx = start + c * csize;
                int cy = start + r * csize;
                SDL_Color col;
                if (game->cells[r][c] == CELL_BLACK) {
                    col.r = 20; col.g = 20; col.b = 20; col.a = 255;
                } else {
                    col.r = 230; col.g = 230; col.b = 230; col.a = 255;
                }
                draw_filled_circle(ren, cx, cy, radius, col);
            }
        }
    }

    /* 高亮最后一步落子 */
    if (game->moves_count > 0) {
        Move last = game->moves[game->moves_count - 1];
        int lx = start + last.col * csize;
        int ly = start + last.row * csize;
        SDL_Color red = {200, 30, 30, 255};
        draw_filled_circle(ren, lx, ly, radius / 4, red);
    }

    /* SDL_RenderPresent 将由调用者负责，以便在绘制棋盘之后再绘制计分板或其他元素 */
}

/* 坐标转换：将屏幕坐标（像素）映射到棋盘行列；无（只使用了基本的数学运算） */
int pixel_to_cell(int x, int y, int *row, int *col)
{
    int csize = cell_size();
    int start = BOARD_MARGIN;
    /* 使用半格宽度作为容差，以便点击靠近线条也能生效 */
    int half = csize / 2;
    int rel_x = x - start;
    int rel_y = y - start;
    if (rel_x < -half || rel_y < -half) return 0;
    int c = (rel_x + half) / csize;
    int r = (rel_y + half) / csize;
    if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) return 0;
    if (row) *row = r;
    if (col) *col = c;
    return 1;
}

/* 绘制游戏结束时的遮罩层；- SDL_SetRenderDrawColor() : SDL 库函数，设置绘制颜色（这里设置半透明黑色） */
void draw_game_over(SDL_Renderer *ren, int winner)
{
    if (!ren) return;
    SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    /* 半透明遮罩 */
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 128);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(ren, &overlay);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    /* 不再走控制台提示：全程只用图形界面 */
}

/* 绘制游戏结果界面。；在棋局结束后先调用此函数，用半透明遮罩覆盖棋盘，并在 */
void draw_game_result(SDL_Renderer *ren, int winner)
{
    if (!ren) return;
    /* 绘制半透明遮罩层 */
    SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_RenderFillRect(ren, &overlay);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    /* 根据获胜者选择提示文本 */
    const char *msg;
    if (winner == 1) {
        msg = "黑方获胜！";
    } else if (winner == 2) {
        msg = "白方获胜！";
    } else {
        msg = "平局！";
    }
    /* 文本区域 */
    SDL_Rect rect;
    rect.w = WINDOW_WIDTH * 3 / 4;
    rect.h = 80;
    rect.x = (WINDOW_WIDTH - rect.w) / 2;
    rect.y = (WINDOW_HEIGHT - rect.h) / 2;
    SDL_Color color = {255, 255, 255, 255};
    draw_menu_text_center(ren, &rect, msg, color);

    /* 再补一句提示：回到菜单用鼠标点一下就行 */
    SDL_Rect rect2 = rect;
    rect2.y += 60;
    rect2.h = 50;
    SDL_Color color2 = {230, 230, 230, 255};
    draw_menu_text_center(ren, &rect2, "(鼠标左键返回)", color2);
    SDL_RenderPresent(ren);
}

/* 使用七段显示样式绘制 0-9 的数字；- SDL_SetRenderDrawColor() : SDL 库函数，设置绘制颜色 */
static void draw_segment_digit(SDL_Renderer *ren, int x, int y, int w, int h,
                               int digit, SDL_Color color)
{
    if (!ren) return;
    /* 定义每个数字需要点亮的段 */
    static const int segments[10][7] = {
        /* a b c d e f g (a=0) */
        {1,1,1,1,1,1,0}, /* 0 */
        {0,1,1,0,0,0,0}, /* 1 */
        {1,1,0,1,1,0,1}, /* 2 */
        {1,1,1,1,0,0,1}, /* 3 */
        {0,1,1,0,0,1,1}, /* 4 */
        {1,0,1,1,0,1,1}, /* 5 */
        {1,0,1,1,1,1,1}, /* 6 */
        {1,1,1,0,0,0,0}, /* 7 */
        {1,1,1,1,1,1,1}, /* 8 */
        {1,1,1,1,0,1,1}  /* 9 */
    };
    int thick = w / 10;
    if (thick < 2) thick = 2;
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    /* 计算段坐标 */
    SDL_Rect seg;
    /* 顶部横段 a */
    if (segments[digit][0]) {
        seg.x = x;
        seg.y = y;
        seg.w = w;
        seg.h = thick;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 右上纵段 b */
    if (segments[digit][1]) {
        seg.x = x + w - thick;
        seg.y = y;
        seg.w = thick;
        seg.h = h / 2;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 右下纵段 c */
    if (segments[digit][2]) {
        seg.x = x + w - thick;
        seg.y = y + h / 2;
        seg.w = thick;
        seg.h = h / 2;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 底部横段 d */
    if (segments[digit][3]) {
        seg.x = x;
        seg.y = y + h - thick;
        seg.w = w;
        seg.h = thick;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 左下纵段 e */
    if (segments[digit][4]) {
        seg.x = x;
        seg.y = y + h / 2;
        seg.w = thick;
        seg.h = h / 2;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 左上纵段 f */
    if (segments[digit][5]) {
        seg.x = x;
        seg.y = y;
        seg.w = thick;
        seg.h = h / 2;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 中间横段 g */
    if (segments[digit][6]) {
        seg.x = x;
        seg.y = y + h / 2 - thick / 2;
        seg.w = w;
        seg.h = thick;
        SDL_RenderFillRect(ren, &seg);
    }
}

/* 使用七段显示样式绘制数字或字母；- toupper() : 来自 <ctype.h>，将字符转换为大写字母 */
static void draw_segment_char(SDL_Renderer *ren, int x, int y, int w, int h,
                              char ch, SDL_Color color)
{
    if (!ren) return;
    /* 定义 7 段灯的状态: 0:off 1:on */
    int pattern[7] = {0};
    if (ch >= '0' && ch <= '9') {
        /* 使用数字的既有模式 */
        static const int segs[10][7] = {
            {1,1,1,1,1,1,0}, /*0*/
            {0,1,1,0,0,0,0}, /*1*/
            {1,1,0,1,1,0,1}, /*2*/
            {1,1,1,1,0,0,1}, /*3*/
            {0,1,1,0,0,1,1}, /*4*/
            {1,0,1,1,0,1,1}, /*5*/
            {1,0,1,1,1,1,1}, /*6*/
            {1,1,1,0,0,0,0}, /*7*/
            {1,1,1,1,1,1,1}, /*8*/
            {1,1,1,1,0,1,1}  /*9*/
        };
        for (int i = 0; i < 7; i++) pattern[i] = segs[ch - '0'][i];
    } else {
        /* 定义常见字母的 7 段模式 */
        switch (ch) {
        case 'A': pattern[0]=1; pattern[1]=1; pattern[2]=1; pattern[4]=1; pattern[5]=1; pattern[6]=1; break;
        case 'B': pattern[2]=1; pattern[3]=1; pattern[4]=1; pattern[5]=1; pattern[6]=1; break; /* 近似 b */
        case 'C': pattern[0]=1; pattern[3]=1; pattern[4]=1; pattern[5]=1; break;
        case 'D': pattern[1]=1; pattern[2]=1; pattern[3]=1; pattern[4]=1; pattern[6]=1; break; /* 近似 d */
        case 'E': pattern[0]=1; pattern[3]=1; pattern[4]=1; pattern[5]=1; pattern[6]=1; break;
        case 'F': pattern[0]=1; pattern[4]=1; pattern[5]=1; pattern[6]=1; break;
        case 'H': pattern[1]=1; pattern[2]=1; pattern[4]=1; pattern[5]=1; pattern[6]=1; break;
        case 'I': pattern[1]=1; pattern[2]=1; break;
        case 'L': pattern[3]=1; pattern[4]=1; pattern[5]=1; break;
        case 'N': pattern[2]=1; pattern[4]=1; pattern[5]=1; pattern[6]=1; break;
        case 'O': pattern[0]=1; pattern[1]=1; pattern[2]=1; pattern[3]=1; pattern[4]=1; pattern[5]=1; break;
        case 'P': pattern[0]=1; pattern[1]=1; pattern[4]=1; pattern[5]=1; pattern[6]=1; break;
        case 'R': pattern[0]=1; pattern[1]=1; pattern[2]=1; pattern[4]=1; pattern[5]=1; pattern[6]=1; break;
        case 'U': pattern[1]=1; pattern[2]=1; pattern[3]=1; pattern[4]=1; pattern[5]=1; break;
        case 'V': pattern[1]=1; pattern[2]=1; pattern[3]=1; pattern[4]=1; pattern[5]=1; break; /* 近似 U/V */
        case 'Y': pattern[1]=1; pattern[2]=1; pattern[3]=1; pattern[5]=1; pattern[6]=1; break;
        case 'Z': pattern[0]=1; pattern[1]=1; pattern[3]=1; pattern[4]=1; pattern[6]=1; break;
        case 'T': pattern[0]=1; pattern[1]=1; pattern[2]=1; break;
        case 'Q': pattern[0]=1; pattern[1]=1; pattern[2]=1; pattern[3]=1; pattern[5]=1; pattern[6]=1; break;
        default:
            /* 未支持的字符不绘制 */
            return;
        }
    }
    SDL_Rect seg;
    int thick = w / 6;
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    /* 顶部水平段 a */
    if (pattern[0]) {
        seg.x = x;
        seg.y = y;
        seg.w = w;
        seg.h = thick;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 右上垂直段 b */
    if (pattern[1]) {
        seg.x = x + w - thick;
        seg.y = y;
        seg.w = thick;
        seg.h = h / 2;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 右下垂直段 c */
    if (pattern[2]) {
        seg.x = x + w - thick;
        seg.y = y + h / 2;
        seg.w = thick;
        seg.h = h / 2;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 底部水平段 d */
    if (pattern[3]) {
        seg.x = x;
        seg.y = y + h - thick;
        seg.w = w;
        seg.h = thick;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 左下垂直段 e */
    if (pattern[4]) {
        seg.x = x;
        seg.y = y + h / 2;
        seg.w = thick;
        seg.h = h / 2;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 左上垂直段 f */
    if (pattern[5]) {
        seg.x = x;
        seg.y = y;
        seg.w = thick;
        seg.h = h / 2;
        SDL_RenderFillRect(ren, &seg);
    }
    /* 中间水平段 g */
    if (pattern[6]) {
        seg.x = x;
        seg.y = y + h / 2 - thick / 2;
        seg.w = w;
        seg.h = thick;
        SDL_RenderFillRect(ren, &seg);
    }
}

/* 使用七段显示绘制一串字符；- toupper() : 来自 <ctype.h>，将字符转换为大写 */
static void draw_segment_text(SDL_Renderer *ren, int x, int y, int w, int h,
                              const char *text, SDL_Color color)
{
    if (!ren || !text) return;
    int posX = x;
    for (const char *p = text; *p; p++) {
        /* 七段字库里没有“:”，这里手动画两个小点出来当分隔符 */
        if (*p == ':') {
            int dot = w / 4;
            if (dot < 2) dot = 2;
            SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
            SDL_Rect d1 = {posX + w / 2 - dot / 2, y + h / 3 - dot / 2, dot, dot};
            SDL_Rect d2 = {posX + w / 2 - dot / 2, y + (h * 2) / 3 - dot / 2, dot, dot};
            SDL_RenderFillRect(ren, &d1);
            SDL_RenderFillRect(ren, &d2);
        } else {
            draw_segment_char(ren, posX, y, w, h, (char)toupper((unsigned char)*p), color);
        }
        /* 在字符之间加入一点间隔（别挤成一坨） */
        posX += w + (w / 4);
    }
}

/* 绘制主菜单界面；- SDL_SetRenderDrawColor() : SDL 库函数，设置绘制颜色 */
void draw_main_menu(SDL_Renderer *ren, int has_resume)
{
    if (!ren) return;

    /* 先画背景图（如果加载失败，会退化成纯色底） */
    SDL_SetRenderDrawColor(ren, 240, 240, 240, 255);
    SDL_RenderClear(ren);
    draw_menu_background(ren);

    /* 盖一层浅色雾面：背景再花也不怕，按钮/文字会更清楚 */
    draw_menu_fog(ren, 110);

    /* 按钮布局：五个竖着排的长矩形 */
    int bw = WINDOW_WIDTH * 3 / 4;
    int bh = 60;
    int spacing = 20;

    int left = (WINDOW_WIDTH - bw) / 2;
    int top  = 80;

    const char *labels[5] = {
        has_resume ? "1. 继续上次对局" : "1. 继续上次对局（暂无存档）",
        "2. 双人对战",
        "3. 人机对战",
        "4. 回放历史",
        "5. 退出游戏"
    };

    for (int i = 0; i < 5; i++) {
        SDL_Rect rect = {left, top + i * (bh + spacing), bw, bh};

        /* 按钮：统一用偏粉的半透明底色，跟背景更搭一点 */
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        if (i == 0 && !has_resume) {
            /* 没有存档时，把“继续”按钮画成灰一点，避免误点 */
            SDL_SetRenderDrawColor(ren, 210, 210, 210, 140);
        } else {
            /* 轻微做点深浅变化，让列表看起来不那么“板” */
            Uint8 g = (Uint8)(185 - i * 6);
            Uint8 b = (Uint8)(210 - i * 5);
            SDL_SetRenderDrawColor(ren, 255, g, b, 170);
        }

        SDL_RenderFillRect(ren, &rect);

        /* 边框也别太硬，稍微淡一点 */
        SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
        SDL_RenderDrawRect(ren, &rect);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        SDL_Color textColor = {20, 20, 20, 255};
        if (i == 0 && !has_resume) {
            textColor.r = 90; textColor.g = 90; textColor.b = 90;
        }
        draw_menu_text_center(ren, &rect, labels[i], textColor);
    }

    SDL_RenderPresent(ren);
}

/* 人机难度选择菜单：从“人机对战”点进去后显示 */
void draw_ai_difficulty_menu(SDL_Renderer *ren)
{
    if (!ren) return;

    /* 和主菜单保持一致：同一张背景图 + 雾面 */
    SDL_SetRenderDrawColor(ren, 245, 245, 245, 255);
    SDL_RenderClear(ren);
    draw_menu_background(ren);
    draw_menu_fog(ren, 110);

    int bw = WINDOW_WIDTH * 3 / 4;
    int bh = 60;
    int spacing = 20;

    int left = (WINDOW_WIDTH - bw) / 2;
    int top  = 120;

    const char *labels[4] = {
        "1. 简单",
        "2. 中级",
        "3. 困难",
        "4. 返回"
    };

    for (int i = 0; i < 4; i++) {
        SDL_Rect rect = {left, top + i * (bh + spacing), bw, bh};

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        /* 难度按钮也用同一套“粉色半透明”风格 */
        Uint8 g = (Uint8)(185 - i * 8);
        Uint8 b = (Uint8)(210 - i * 8);
        SDL_SetRenderDrawColor(ren, 255, g, b, 170);
        SDL_RenderFillRect(ren, &rect);

        SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
        SDL_RenderDrawRect(ren, &rect);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        SDL_Color textColor = {20, 20, 20, 255};
        draw_menu_text_center(ren, &rect, labels[i], textColor);
    }

    SDL_RenderPresent(ren);
}


/* 绘制游戏结束后的菜单（再来一局/退出游戏）；- SDL_SetRenderDrawColor() : SDL 库函数，设置绘制颜色 */

/* ========== 回放菜单：列出历史对局（鼠标点选） ========== */
void draw_playback_menu(SDL_Renderer *ren, int page, int total, int per_page)
{
    if (!ren) return;

    SDL_SetRenderDrawColor(ren, 240, 240, 240, 255);
    SDL_RenderClear(ren);
    draw_menu_background(ren);

    /* 背景雾面稍微淡一点：让图片更“显眼”些 */
    draw_menu_fog(ren, 110);

    /* 标题 */
    SDL_Rect title = {0, 20, WINDOW_WIDTH, 60};
    SDL_Color titleColor = {60, 40, 55, 255};
    draw_menu_text_center(ren, &title, "对局回放", titleColor);

    int list_w = WINDOW_WIDTH * 3 / 4;
    int left = (WINDOW_WIDTH - list_w) / 2;
    int row_h = 52;
    int gap = 14;
    int top = 110;

    int start_index = page * per_page;
    int show_count = total - start_index;
    if (show_count > per_page) show_count = per_page;
    if (show_count < 0) show_count = 0;

    /* 每行：左边“第 N 轮”按钮，右边“删除”小按钮 */
    int del_w = 90;
    int play_w = list_w - del_w - 10;

    for (int i = 0; i < show_count; i++) {
        int idx = start_index + i;
        SDL_Rect playRect = {left, top + i * (row_h + gap), play_w, row_h};
        SDL_Rect delRect  = {left + play_w + 10, top + i * (row_h + gap), del_w, row_h};

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        /* 回放按钮 */
        SDL_SetRenderDrawColor(ren, 255, 180, 210, 170);
        SDL_RenderFillRect(ren, &playRect);
        SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
        SDL_RenderDrawRect(ren, &playRect);

        /* 删除按钮：稍微深一点，别点错 */
        SDL_SetRenderDrawColor(ren, 255, 155, 190, 180);
        SDL_RenderFillRect(ren, &delRect);
        SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
        SDL_RenderDrawRect(ren, &delRect);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        char label[64];
        snprintf(label, sizeof(label), "第 %d 轮", idx + 1);

        SDL_Color textColor = {40, 30, 40, 255};
        draw_menu_text_center(ren, &playRect, label, textColor);

        SDL_Color delText = {40, 30, 40, 255};
        draw_menu_text_center(ren, &delRect, "删除", delText);
    }

    /* 翻页提示 */
    if (total > per_page) {
        char pbuf[64];
        int pages = (total + per_page - 1) / per_page;
        snprintf(pbuf, sizeof(pbuf), "第 %d/%d 页", page + 1, pages);

        SDL_Rect pageRect = {0, WINDOW_HEIGHT - 120, WINDOW_WIDTH, 40};
        SDL_Color pc = {70, 60, 70, 255};
        draw_menu_text_center(ren, &pageRect, pbuf, pc);
    }


    /* 翻页按钮（记录多时才显示） */
    if (total > per_page) {
        SDL_Rect prevRect = { 60, WINDOW_HEIGHT - 80, 120, 50 };
        SDL_Rect nextRect = { WINDOW_WIDTH - 60 - 120, WINDOW_HEIGHT - 80, 120, 50 };

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        SDL_SetRenderDrawColor(ren, 255, 180, 210, 170);
        SDL_RenderFillRect(ren, &prevRect);
        SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
        SDL_RenderDrawRect(ren, &prevRect);

        SDL_SetRenderDrawColor(ren, 255, 180, 210, 170);
        SDL_RenderFillRect(ren, &nextRect);
        SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
        SDL_RenderDrawRect(ren, &nextRect);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

        SDL_Color t = {40, 30, 40, 255};
        draw_menu_text_center(ren, &prevRect, "上一页", t);
        draw_menu_text_center(ren, &nextRect, "下一页", t);
    }

    /* 底部按钮：返回主菜单 */
    SDL_Rect backRect = { (WINDOW_WIDTH - 240) / 2, WINDOW_HEIGHT - 80, 240, 50 };
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 255, 180, 210, 170);
    SDL_RenderFillRect(ren, &backRect);
    SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
    SDL_RenderDrawRect(ren, &backRect);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_Color backText = {40, 30, 40, 255};
    draw_menu_text_center(ren, &backRect, "返回", backText);

    SDL_RenderPresent(ren);
}

void draw_playback_empty(SDL_Renderer *ren)
{
    if (!ren) return;

    SDL_SetRenderDrawColor(ren, 240, 240, 240, 255);
    SDL_RenderClear(ren);
    draw_menu_background(ren);
    draw_menu_fog(ren, 110);

    SDL_Rect title = {0, 20, WINDOW_WIDTH, 60};
    SDL_Color titleColor = {60, 40, 55, 255};
    draw_menu_text_center(ren, &title, "对局回放", titleColor);

    SDL_Rect msg = {0, 150, WINDOW_WIDTH, 60};
    SDL_Color mc = {70, 60, 70, 255};
    draw_menu_text_center(ren, &msg, "暂无对局记录", mc);

    SDL_Rect backRect = { (WINDOW_WIDTH - 240) / 2, WINDOW_HEIGHT - 80, 240, 50 };
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 255, 180, 210, 170);
    SDL_RenderFillRect(ren, &backRect);
    SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
    SDL_RenderDrawRect(ren, &backRect);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    SDL_Color backText = {40, 30, 40, 255};
    draw_menu_text_center(ren, &backRect, "返回", backText);

    SDL_RenderPresent(ren);
}


void draw_end_menu(SDL_Renderer *ren)
{
    if (!ren) return;

    /* 半透明遮罩：不完全挡住棋盘，稍微柔一点 */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 140);
    SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(ren, &overlay);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

    int bw = 220;
    int bh = 80;
    int gap = 40;

    int left = (WINDOW_WIDTH - (2 * bw + gap)) / 2;
    int top  = WINDOW_HEIGHT / 2 - bh / 2;

    SDL_Color white = {255, 255, 255, 255};

    /* 左侧：再来一局 */
    SDL_Rect r1 = {left, top, bw, bh};
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 255, 185, 210, 180);
    SDL_RenderFillRect(ren, &r1);
    SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
    SDL_RenderDrawRect(ren, &r1);
    draw_menu_text_center(ren, &r1, "再来一局", white);

    /* 右侧：返回主菜单 */
    SDL_Rect r2 = {left + bw + gap, top, bw, bh};
    SDL_SetRenderDrawColor(ren, 255, 165, 195, 180);
    SDL_RenderFillRect(ren, &r2);
    SDL_SetRenderDrawColor(ren, 90, 60, 80, 140);
    SDL_RenderDrawRect(ren, &r2);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
    draw_menu_text_center(ren, &r2, "返回主菜单", white);

    SDL_RenderPresent(ren);
}


/* 在窗口顶部绘制黑白双方的比分；- snprintf() : 来自 <stdio.h>，格式化字符串（将整数转换为字符串） */
void draw_scoreboard(SDL_Renderer *ren, int score_black, int score_white)
{
    if (!ren) return;
    /* 定义计分板区域起点 */
    int x = 10;
    int y = 10;
    /* 为了防止计分板图标被误认为棋子，这里将图标半径调小并使用不同颜色 */
    int radius = 6;
    /* 黑方图标及分数 */
    /* 使用深灰色而非纯黑色，避免和棋子颜色混淆 */
    SDL_Color blackColor = {60, 60, 60, 255};
    /* 画黑棋图标 */
    draw_filled_circle(ren, x + radius, y + radius, radius, blackColor);
    /* 黑方分数的颜色保持一致 */
    SDL_Color digitColor = {60, 60, 60, 255};
    /* 分数可能是多位数字，将其分解为字符 */
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", score_black);
    draw_segment_text(ren, x + radius * 2 + 5, y, 12, 16, buf, digitColor);
    /* 白方图标及分数 */
    int offsetX = 120;
    /* 使用浅灰色代替纯白色 */
    SDL_Color whiteColor = {200, 200, 200, 255};
    draw_filled_circle(ren, x + offsetX + radius, y + radius, radius, whiteColor);
    snprintf(buf, sizeof(buf), "%d", score_white);
    /* 白色数字用深灰色描绘以便可见 */
    SDL_Color whiteDigitColor = {80, 80, 80, 255};
    draw_segment_text(ren, x + offsetX + radius * 2 + 5, y, 12, 16, buf, whiteDigitColor);
}

/* 右上角计时器：显示本局用时（mm:ss）。 */
void draw_timer(SDL_Renderer *ren, int elapsed_seconds)
{
    if (!ren) return;

    if (elapsed_seconds < 0) elapsed_seconds = 0;
    int mm = elapsed_seconds / 60;
    int ss = elapsed_seconds % 60;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);

    /* 估算一下宽度，把它贴在右上角 */
    int char_w = 12;
    int gap = char_w / 4;
    int total_w = 5 * (char_w + gap); /* "mm:ss" 共 5 个字符 */
    int x = WINDOW_WIDTH - total_w - 10;
    int y = 10;

    SDL_Color color = {40, 40, 40, 255};
    draw_segment_text(ren, x, y, char_w, 18, buf, color);
}

/* 显示悔棋次数（按一次算一次）。 */
void draw_undo_count(SDL_Renderer *ren, int undo_count)
{
    if (!ren) return;
    if (undo_count < 0) undo_count = 0;

    char buf[16];
    snprintf(buf, sizeof(buf), "U%d", undo_count);

    int char_w = 12;
    int gap = char_w / 4;
    int total_w = (int)strlen(buf) * (char_w + gap);
    int x = WINDOW_WIDTH - total_w - 10;
    int y = 32; /* 在计时器下面一点 */

    SDL_Color color = {60, 60, 60, 255};
    draw_segment_text(ren, x, y, char_w, 18, buf, color);
}