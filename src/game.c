/*
 * game.c
 *
 * 实现六子棋游戏的基本逻辑，包括初始化、落子、胜负判断等功能。
 * 为了保证代码易于理解和维护，尽量保持函数简单直观
 */

#include "game.h"
#include <string.h>

/* 初始化棋局状态；- memset() : 来自 <string.h>，用于将内存区域清零 */
void init_game(GameState *game)
{
    if (!game) return;
    /*
     * 先将整个 GameState 结构体置零，确保没有任何脏数据残留。
     * 这样可以避免棋盘上出现莫名其妙的棋子或者状态。
     * 注意：清零后我们会重新设置 current_player 等字段。
     */
    memset(game, 0, sizeof(GameState));
    /* 将所有棋盘格子标记为空 */
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            game->cells[r][c] = CELL_EMPTY;
        }
    }
    /* 黑棋先手 */
    game->current_player = 1;
    /* 游戏尚未结束 */
    game->finished = 0;
    game->winner = 0;
    /* 这局“悔棋”次数（按一次算一次） */
    game->undo_count = 0;
    /* 重置步数计数 */
    game->moves_count = 0;
}

/* 撤销最后一步（悔棋）。
 * 这个函数只负责“把状态退回一步”，不处理 UI 交互。
 *
 * 规则：
 *   - 如果还没下过棋，直接返回 0
 *   - 清空最后一步落子位置
 *   - current_player 回到“刚才下子的人”（也就是撤销后轮到他下）
 *   - finished / winner 清掉，让对局继续
 */
int undo_last_move(GameState *game)
{
    if (!game) return 0;
    if (game->moves_count <= 0) return 0;

    Move last = game->moves[game->moves_count - 1];

    if (within_board(last.row, last.col)) {
        game->cells[last.row][last.col] = CELL_EMPTY;
    }

    game->moves_count--;

    /* 撤销后应该轮到“刚才下棋的那一方”继续下 */
    game->current_player = last.player;
    if (game->moves_count <= 0) {
        game->current_player = 1; /* 开局默认黑先 */
    }

    game->finished = 0;
    game->winner = 0;
    return 1;
}

/* 判断坐标是否在棋盘范围内；无（只使用了基本的比较运算符） */
int within_board(int row, int col)
{
    return row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE;
}

/* 放置一颗棋子；无（只调用其他游戏函数） */
int place_stone(GameState *game, int row, int col)
{
    if (!game || game->finished) {
        return 0;
    }
    if (!within_board(row, col)) {
        return 0;
    }
    if (game->cells[row][col] != CELL_EMPTY) {
        return 0;
    }
    /* 在棋盘上标记 */
    game->cells[row][col] = (game->current_player == 1 ? CELL_BLACK : CELL_WHITE);

    /* 记录本次落子 */
    //棋盘没有满
    if (game->moves_count < (BOARD_SIZE * BOARD_SIZE)) {
        Move *m = &game->moves[game->moves_count];
        game->moves_count++;
        m->row = row;
        m->col = col;
        m->player = game->current_player;
    }
    /* 判断胜负 */
    if (check_win(game, row, col)) {
        game->finished = 1;
        game->winner = game->current_player;
    } else if (game->moves_count == BOARD_SIZE * BOARD_SIZE) {
        /* 平局 */
        game->finished = 1;
        game->winner = 0;
    } else {
        /* 切换玩家 */
        switch_player(game);
    }
    return 1;
}

/* 判断最近一手是否形成连续六子；无（只使用了基本的循环和条件判断） */
int check_win(GameState *game, int last_row, int last_col)
{
    if (!game) return 0;

    Cell me = game->cells[last_row][last_col];
    if (me == CELL_EMPTY) return 0;

    // 四个方向：横、竖、右下斜、右上斜
    int dr[4] = {0, 1, 1, -1};
    int dc[4] = {1, 0, 1,  1};

    for (int k = 0; k < 4; k++) {
        int cnt = 1;

        int r = last_row + dr[k];
        int c = last_col + dc[k];
        while (r >= 0 && r < BOARD_SIZE &&
               c >= 0 && c < BOARD_SIZE &&
               game->cells[r][c] == me) {
            cnt++;
            r += dr[k];
            c += dc[k];
        }

        r = last_row - dr[k];
        c = last_col - dc[k];
        while (r >= 0 && r < BOARD_SIZE &&
               c >= 0 && c < BOARD_SIZE &&
               game->cells[r][c] == me) {
            cnt++;
            r -= dr[k];
            c -= dc[k];
        }

        if (cnt >= WIN_LENGTH) {
            game->winner = me;
            return 1;   // 有人赢了
        }
    }

    return 0;           // 还没分出胜负
}


/* 切换当前玩家；无（只使用了简单的条件运算符） */
void switch_player(GameState *game)
{
    if (!game) return;
    game->current_player = (game->current_player == 1 ? 2 : 1);
}

/* 判断棋盘是否已下满（无空位）；无（只使用了基本的比较运算） */
int board_full(const GameState *game)
{
    if (!game) return 0;
    return game->moves_count >= BOARD_SIZE * BOARD_SIZE;
}