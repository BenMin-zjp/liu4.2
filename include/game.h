/*
 * game.h
 * 游戏核心数据结构 + 规则函数（棋盘、落子、胜负判断）。
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h>

/* ========== 游戏配置常量 ========== */

/* 棋盘大小（棋盘是 BOARD_SIZE × BOARD_SIZE 的格子）；可以调整这个值来改变棋盘大小，比如改成 19 就是 19×19 的棋盘 */
/*
 * 棋盘大小（棋盘是 BOARD_SIZE × BOARD_SIZE 的格子）
 * 将原来的 15 改为 19，使得棋盘更大。注意界面绘制函数会根据
 * BOARD_SIZE 自动调整格子间距，无需额外修改。
 */
#define BOARD_SIZE 19

/* 胜利条件：连续多少个同色棋子算赢；比如 WIN_LENGTH = 6 表示连续 6 个子算胜利 */
#define WIN_LENGTH 6

/* ========== 数据结构定义 ========== */

/* 表示棋盘上每个格子的状态；枚举类型（enum），表示一个格子可能是： */
//定义了一个名为Cell的枚举类型来代替enum，枚举类型用于将一组整数值命名
typedef enum {
    CELL_EMPTY = 0,
    CELL_BLACK = 1,  /* 先手 */
    CELL_WHITE = 2   /* 后手 */
} Cell;
/* 保存一次落子的记录；结构体（struct），用来记录每一步棋的信息： */
//把结构体命名为Move，以后就不用写struct Move而是直接写Move
typedef struct {
    int row;      // 行号（0 到 BOARD_SIZE-1）
    int col;      // 列号（0 到 BOARD_SIZE-1）
    int player;   // 玩家编号：1 = 黑子, 2 = 白子
} Move;

/* 游戏整体状态结构；这个结构体包含了整个游戏的所有状态信息： */
//把结构体命名为GameState，以后就不用写struct GameState而是直接写GameState
typedef struct {
    Cell cells[BOARD_SIZE][BOARD_SIZE];  // 棋盘：二维数组，每个元素是一个格子
    int current_player;                   // 当前落子方: 1 或 2
    int finished;                         // 游戏是否结束: 0=进行中, 1=已结束
    int winner;                           // 胜利者: 0=无人/平局, 1=黑, 2=白
    int undo_count;                       // 这局里一共按了多少次“悔棋”（按一次算一次）
    int moves_count;                      // 已下的步数
    Move moves[BOARD_SIZE * BOARD_SIZE];  // 所有落子步骤的历史记录
} GameState;

/* ========== 函数声明 ========== */

/* 初始化棋局状态；- memset() : 来自 <string.h>，将内存区域清零 */
void init_game(GameState *game);

/* 在指定位置落子；内部调用其他游戏函数： */
int place_stone(GameState *game, int row, int col);

/* 判断最近一手是否形成连续六子（检查是否有人赢了）；无（只使用了基本的循环和条件判断） */
int check_win(GameState *game, int last_row, int last_col);

/* 切换当前玩家；无（只使用了简单的条件运算符） */
void switch_player(GameState *game);

/* 判断棋盘是否已下满（无空位）；无（只使用了基本的比较运算） */
int board_full(const GameState *game);

/* 判断坐标是否在棋盘范围内；无（只使用了基本的比较运算符） */
int within_board(int row, int col);

/* 悔棋：撤销最后一步。
 * 返回 1 表示撤销成功；返回 0 表示没法撤销（例如还没下棋）。
 * 注意：这个函数只负责“把棋盘和当前玩家状态回退一步”，
 * UI 里怎么提示、要不要一次撤两步（人机模式）由调用者决定。
 */
int undo_last_move(GameState *game);

#endif /* GAME_H */