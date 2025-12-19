/*
 * ai.c
 *
 * 提供电脑落子策略的简单实现。难度 1 为随机落子，难度 2 为根据周围局势评分选择。
 */

#include "ai.h"
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

/* 计算某个位置的评分：越高表示此位置越值得落子；无（只使用了基本的循环和计算） */
static int evaluate_pos(const GameState *game, int row, int col, int player)
{
    int score = 0;
    /* 临时放一个棋子，分别统计四个方向上连续己子和连续对手 */
    int directions[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
    int self_type  = (player == 1 ? CELL_BLACK : CELL_WHITE);
    int opp_type   = (player == 1 ? CELL_WHITE : CELL_BLACK);
    for (int d = 0; d < 4; d++) {
        int dr = directions[d][0];
        int dc = directions[d][1];
        /* 统计己方连续数 */
        int self_cnt = 1; /* 包含假设落子 */
        int r = row + dr;
        int c = col + dc;
        while (within_board(r, c) && game->cells[r][c] == self_type) {
            self_cnt++;
            r += dr; c += dc;
        }
        r = row - dr; c = col - dc;
        while (within_board(r, c) && game->cells[r][c] == self_type) {
            self_cnt++;
            r -= dr; c -= dc;
        }
        /* 对手连续数 */
        int opp_cnt = 1;
        r = row + dr; c = col + dc;
        while (within_board(r, c) && game->cells[r][c] == opp_type) {
            opp_cnt++;
            r += dr; c += dc;
        }
        r = row - dr; c = col - dc;
        while (within_board(r, c) && game->cells[r][c] == opp_type) {
            opp_cnt++;
            r -= dr; c -= dc;
        }
        /* 简单的加权：更大的连续数权重更高 */
        if (self_cnt >= WIN_LENGTH) {
            score += 100000; /* 赢棋 */
        } else {
            score += self_cnt * self_cnt * 10;
        }
        if (opp_cnt >= WIN_LENGTH) {
            score += 90000; /* 阻止对手胜利 */
        } else {
            score += opp_cnt * opp_cnt * 9;
        }
    }
    return score;
}

/* 随机挑选一个可落子的空位；- rand() : 来自 <stdlib.h>，生成随机整数（返回 0 到 RAND_MAX 之间的随机数） */
static int random_move(GameState *game)
{
    int empty_count = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->cells[r][c] == CELL_EMPTY) {
                empty_count++;
            }
        }
    }
    if (empty_count == 0) return 0;
    int pick = rand() % empty_count;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->cells[r][c] == CELL_EMPTY) {
                if (pick == 0) {
                    place_stone(game, r, c);
                    return 1;
                }
                pick--;
            }
        }
    }
    return 0;
}

/* AI 落子实现（电脑下棋）；- srand() : 来自 <stdlib.h>，设置随机数生成器的种子 */
void ai_move(GameState *game, int difficulty)
{
    if (!game || game->finished) return;
    /* 确保随机数种子只初始化一次 */
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
    if (difficulty <= 1) {
        /* 简单随机 */
        random_move(game);
        return;
    }
    if (difficulty == 2) {
        /* 中级难度：按照简单的估值函数选择最佳位置 */
        int best_row = -1, best_col = -1;
        int best_score = -1;
        for (int r = 0; r < BOARD_SIZE; r++) {
            for (int c = 0; c < BOARD_SIZE; c++) {
                if (game->cells[r][c] == CELL_EMPTY) {
                    int score = evaluate_pos(game, r, c, game->current_player);
                    /* 加一点随机性，避免千篇一律 */
                    score += rand() % 5;
                    if (score > best_score) {
                        best_score = score;
                        best_row = r;
                        best_col = c;
                    }
                }
            }
        }
        if (best_row >= 0 && best_col >= 0) {
            place_stone(game, best_row, best_col);
        } else {
            random_move(game);
        }
        return;
    }
    /* 困难难度：先判断是否存在能立即获胜的落子；
     * 如果有，直接下在该处；否则检查是否需要阻挡对手即将取胜；
     * 接着检查对手是否有形成长连（例如 4 子或以上）的潜在威胁，如果有则优先堵住；
     * 如果这些都没有，再使用估值函数选择最佳位置。 */
    int win_row = -1, win_col = -1;
    int block_row = -1, block_col = -1;
    /* 记录潜在威胁位置及威胁连子长度 */
    int threat_row = -1, threat_col = -1;
    int threat_len = 0;
    int self = game->current_player;
    int opp  = (self == 1 ? 2 : 1);
    GameState temp;
    /* 搜索所有空位，寻找立即取胜的落点和必须阻挡的对手取胜点 */
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->cells[r][c] != CELL_EMPTY) continue;
            /* 检查是否能直接获胜 */
            temp = *game;
            place_stone(&temp, r, c);
            if (temp.winner == self) {
                win_row = r;
                win_col = c;
                break;
            }
            /* 检查是否需要阻挡对手立即获胜 */
            temp = *game;
            temp.current_player = opp;
            place_stone(&temp, r, c);
            if (temp.winner == opp) {
                block_row = r;
                block_col = c;
            }
        }
        if (win_row != -1) break;
    }
    /* 如果能立即获胜，直接下此处 */
    if (win_row != -1) {
        place_stone(game, win_row, win_col);
        return;
    }
    /* 如果对手下一步能获胜，则阻挡 */
    if (block_row != -1) {
        place_stone(game, block_row, block_col);
        return;
    }
    /* 检查潜在威胁：若对手某个空位形成较长连续棋子（如 4 子及以上），优先堵住 */
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->cells[r][c] != CELL_EMPTY) continue;
            /* 计算如果对手在该位置落子，形成的最大连续长度 */
            int max_len = 1;
            int directions[4][2] = {{1,0},{0,1},{1,1},{1,-1}};
            for (int d = 0; d < 4; d++) {
                int dr = directions[d][0];
                int dc = directions[d][1];
                int cnt = 1;
                /* 正向统计对手连子 */
                int rr = r + dr;
                int cc = c + dc;
                while (within_board(rr, cc) && game->cells[rr][cc] == (opp == 1 ? CELL_BLACK : CELL_WHITE)) {
                    cnt++;
                    rr += dr;
                    cc += dc;
                }
                /* 反向统计对手连子 */
                rr = r - dr;
                cc = c - dc;
                while (within_board(rr, cc) && game->cells[rr][cc] == (opp == 1 ? CELL_BLACK : CELL_WHITE)) {
                    cnt++;
                    rr -= dr;
                    cc -= dc;
                }
                if (cnt > max_len) {
                    max_len = cnt;
                }
            }
            /* 更新潜在威胁位置 */
            if (max_len > threat_len) {
                threat_len = max_len;
                threat_row = r;
                threat_col = c;
            }
        }
    }
    /* 威胁阈值：阻挡对手即将形成 WIN_LENGTH-2（例如 4 连）的情况 */
    int threat_threshold = (WIN_LENGTH > 2 ? WIN_LENGTH - 2 : 2);
    if (threat_len >= threat_threshold && threat_row >= 0 && threat_col >= 0) {
        place_stone(game, threat_row, threat_col);
        return;
    }
    /* 如果没有立即获胜或阻挡对手或者长连威胁，则使用估值函数选择最佳位置 */
    int best_row = -1, best_col = -1;
    int best_score = -1;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (game->cells[r][c] == CELL_EMPTY) {
                int score = evaluate_pos(game, r, c, game->current_player);
                /* 加一些随机性避免过于死板 */
                score += rand() % 3;
                if (score > best_score) {
                    best_score = score;
                    best_row = r;
                    best_col = c;
                }
            }
        }
    }
    if (best_row >= 0 && best_col >= 0) {
        place_stone(game, best_row, best_col);
    } else {
        random_move(game);
    }
}