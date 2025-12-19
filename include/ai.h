/*
 * ai.h
 * 电脑下棋接口：difficulty 控制强度/策略。
 */

#ifndef AI_H
#define AI_H

#include "game.h"

/* 电脑落子（AI 下棋）；内部会调用以下函数： */
void ai_move(GameState *game, int difficulty);

#endif /* AI_H */