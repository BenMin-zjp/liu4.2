/*
 * fileio.h
 * 对局记录：保存/读取（每局一行 JSON，文件在 liu/data/records.json）。
 */

#ifndef FILEIO_H
#define FILEIO_H

#include "game.h"

/* 保存棋局到记录文件；内部使用以下文件操作函数： */
int save_record(const GameState *game);

/* 读取指定索引的记录（加载历史对局）；内部使用以下文件操作函数： */
int load_record(int index, GameState *game);

/* 返回记录文件中包含的对局数量；内部使用以下文件操作函数： */
int record_count(void);

/* 删除一条对局记录（按编号，从 0 开始）。
 * 成功返回 1，失败返回 0。
 * 说明：records.json 是“每行一条 JSON”的格式，这里通过过滤行来实现删除。
 */
int delete_record(int index);

/* 清空所有对局记录（把 records.json 清空）。成功返回 1，失败返回 0。 */
int clear_records(void);

/* ======= 断点续玩：中途退出时存一份“当前这盘”的状态 ======= */
int has_resume_game(void);
int clear_resume_game(void);
int save_resume_game(const GameState *game, int mode, int elapsed_seconds);
int load_resume_game(GameState *game, int *mode, int *elapsed_seconds);

#endif /* FILEIO_H */