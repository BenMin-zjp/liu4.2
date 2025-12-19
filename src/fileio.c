/*
 * fileio.c
 *
 * 提供简单的记录保存与读取功能。
 * 数据格式为 NDJSON，每行一个 JSON 对象，包含时间、胜者、以及每一步的行列与落子方。
 */

#include "fileio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

/* 记录文件路径 */
static const char *RECORD_FILE = "liu/data/records.json";

/* 确保 data 目录存在（如果不存在则创建）；- stat() : 来自 <sys/stat.h>，检查文件或目录是否存在 */
static void ensure_data_dir(void)
{
    struct stat st;
    // 先检查并创建 liu 目录
    if (stat("liu", &st) != 0) {
        #ifdef _WIN32
        mkdir("liu");
        #else
        mkdir("liu", 0755);
        #endif
    }
    // 再检查并创建 liu/data 目录
    if (stat("liu/data", &st) != 0) {
        #ifdef _WIN32
        mkdir("liu\\data");  // Windows 使用反斜杠，但也可以使用正斜杠
        #else
        mkdir("liu/data", 0755);
        #endif
    }
}

/* 保存游戏记录到文件；- fopen()  : 打开文件（"a" 模式表示追加写入，在文件末尾添加内容） */
int save_record(const GameState *game)
{
    if (!game) return 0;
    ensure_data_dir();
    FILE *fp = fopen(RECORD_FILE, "a");
    if (!fp) {
        // 输出错误信息到控制台，方便调试
        fprintf(stderr, "错误：无法打开文件 %s 进行写入\n", RECORD_FILE);
        perror("fopen records.json");
        return 0;
    }
    /* 时间戳字符串 */
    char timestr[32];
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (lt) {
        strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", lt);
    } else {
        strcpy(timestr, "unknown");
    }
    /* 写入 JSON 对象（每局一行，方便追加/删除）
     * 说明：undo 字段是后来加的，旧记录里可能没有；读的时候要能兼容。
     */
    fprintf(fp, "{\"time\":\"%s\",\"winner\":%d,\"undo\":%d,\"moves\":[",
            timestr, game->winner, game->undo_count);
    for (int i = 0; i < game->moves_count; i++) {
        const Move *m = &game->moves[i];
        fprintf(fp, "{\"p\":%d,\"r\":%d,\"c\":%d}", m->player, m->row, m->col);
        if (i != game->moves_count - 1) {
            fputc(',', fp);
        }
    }
    fprintf(fp, "]}\n");
    fclose(fp);
    return 1;
}

/* 计算记录条数（统计文件中有多少条对局记录）；- fopen() : 打开文件（"r" 模式表示只读） */
int record_count(void)
{
    FILE *fp = fopen(RECORD_FILE, "r");
    if (!fp) return 0;

    int count = 0;
    int ch;
    int saw_any = 0;

    while ((ch = fgetc(fp)) != EOF) {
        saw_any = 1;
        if (ch == '\n') {
            count++;
        }
    }

    /* 保险：文件有内容但末尾没换行 */
    if (saw_any && count == 0) {
        count = 1;
    }

    fclose(fp);
    return count;
}

/* 解析一行 JSON 中的 moves 数组并填充游戏状态；- strstr() : 来自 <string.h>，在字符串中查找子串（如查找 "\"moves\":["） */
static void parse_moves(const char *line, GameState *game)
{
    const char *p = strstr(line, "\"moves\":[");
    if (!p) return;
    p = strchr(p, '[');
    if (!p) return;
    p++; /* skip '[' */
    init_game(game);
    /* 读取数组中的对象 */
    while (*p && *p != ']') {
        int player = 0, row = 0, col = 0;
        /* 找到数字 */
        if (sscanf(p, "{\"p\":%d,\"r\":%d,\"c\":%d}", &player, &row, &col) == 3) {
            if (within_board(row, col)) {
                game->cells[row][col] = (player == 1 ? CELL_BLACK : CELL_WHITE);
                if (game->moves_count < BOARD_SIZE * BOARD_SIZE) {
                    Move *m = &game->moves[game->moves_count];
                    game->moves_count++;
                    m->player = player;
                    m->row = row;
                    m->col = col;
                }
            }
            /* 移动到下一个 } */
            p = strchr(p, '}');
            if (!p) break;
            p++;
            /* 跳过逗号 */
            if (*p == ',') p++;
        } else {
            /* 无法解析，跳出 */
            break;
        }
    }
    /* 读取胜者 winner 字段 */
    int winner = 0;
    const char *w = strstr(line, "\"winner\":");
    if (w) {
        sscanf(w + 9, "%d", &winner);
    }
    /* 读取 undo 字段（如果没有就默认为 0） */
    int undo_count = 0;
    const char *u = strstr(line, "\"undo\":");
    if (u) {
        sscanf(u + 7, "%d", &undo_count);
    }
    game->undo_count = undo_count;
    game->finished = 1;
    game->winner = winner;
    /* 当前玩家设置为赢家反方，方便回放时下一手颜色正确 */
    if (game->moves_count % 2 == 0) {
        game->current_player = 1;
    } else {
        game->current_player = 2;
    }
}

/* 按索引读取历史记录到游戏状态；- fopen()  : 打开文件（"r" 模式表示只读） */
int load_record(int index, GameState *game)
{
    if (!game) return 0;
    FILE *fp = fopen(RECORD_FILE, "r");
    if (!fp) return 0;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int current = 0;
    int found = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        if (current == index) {
            /* 解析这一行 */
            parse_moves(line, game);
            found = 1;
            break;
        }
        current++;
    }
    if (line) free(line);
    fclose(fp);
    return found;
}

/* 删除指定编号的一条记录（0 开始）。
 * 做法：把 records.json 逐行读出来，除了 index 这行，其他写到临时文件；
 * 最后用临时文件替换原文件。
 */
int delete_record(int index)
{
    if (index < 0) return 0;

    FILE *in = fopen(RECORD_FILE, "r");
    if (!in) return 0;

    /* 临时文件放在同目录，避免跨盘 rename 的坑 */
    const char *tmp = "liu/data/records.tmp";
    FILE *out = fopen(tmp, "w");
    if (!out) {
        fclose(in);
        return 0;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int cur = 0;
    int removed = 0;

    while ((read = getline(&line, &len, in)) != -1) {
        if (cur == index) {
            removed = 1;
        } else {
            fputs(line, out);
        }
        cur++;
    }

    if (line) free(line);
    fclose(in);
    fclose(out);

    if (!removed) {
        /* 没找到这条，临时文件删掉就行 */
        remove(tmp);
        return 0;
    }

    /* 替换原文件 */
    remove(RECORD_FILE);
    if (rename(tmp, RECORD_FILE) != 0) {
        /* 失败就尽量恢复（别把用户的数据整没了） */
        return 0;
    }

    return 1;
}

/* 清空所有记录：直接把 records.json 截断为 0 字节 */
int clear_records(void)
{
    ensure_data_dir();
    FILE *fp = fopen(RECORD_FILE, "w");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

/* ======= 断点续玩：中途退出时存一份“当前这盘”的状态 ======= */
static const char *RESUME_FILE = "liu/data/resume.json";

/* 是否存在 resume.json（并且不是空文件） */
int has_resume_game(void)
{
    struct stat st;
    if (stat(RESUME_FILE, &st) != 0) return 0;
    if (st.st_size <= 4) return 0; /* 太小基本就是空的 */
    return 1;
}

/* 删除 resume.json（不存在也当成功） */
int clear_resume_game(void)
{
    if (remove(RESUME_FILE) == 0) return 1;
    /* 如果文件本来就没有，也不算失败 */
    return 1;
}

/* 保存当前棋局：mode + 已用时间 + 当前走子方 + 悔棋次数 + moves[] */
int save_resume_game(const GameState *game, int mode, int elapsed_seconds)
{
    if (!game) return 0;
    ensure_data_dir();

    FILE *fp = fopen(RESUME_FILE, "w");
    if (!fp) {
        fprintf(stderr, "错误：无法打开文件 %s 进行写入\n", RESUME_FILE);
        perror("fopen resume.json");
        return 0;
    }

    if (elapsed_seconds < 0) elapsed_seconds = 0;

    fprintf(fp, "{\"mode\":%d,\"elapsed\":%d,\"current\":%d,\"undo\":%d,\"moves\":[",
            mode, elapsed_seconds, game->current_player, game->undo_count);

    for (int i = 0; i < game->moves_count; i++) {
        const Move *m = &game->moves[i];
        fprintf(fp, "{\"p\":%d,\"r\":%d,\"c\":%d}", m->player, m->row, m->col);
        if (i != game->moves_count - 1) fputc(',', fp);
    }

    fprintf(fp, "]}\n");
    fclose(fp);
    return 1;
}

/* 解析 resume.json 的 moves 数组 */
static void parse_resume_moves(const char *buf, GameState *game)
{
    const char *p = strstr(buf, "\"moves\":[");
    if (!p) return;
    p = strchr(p, '[');
    if (!p) return;
    p++; /* skip '[' */

    init_game(game);

    while (*p && *p != ']') {
        int player = 0, row = 0, col = 0;
        if (sscanf(p, "{\"p\":%d,\"r\":%d,\"c\":%d}", &player, &row, &col) == 3) {
            if (within_board(row, col)) {
                game->cells[row][col] = (player == 1 ? CELL_BLACK : CELL_WHITE);
                if (game->moves_count < BOARD_SIZE * BOARD_SIZE) {
                    Move *m = &game->moves[game->moves_count];
                    game->moves_count++;
                    m->player = player;
                    m->row = row;
                    m->col = col;
                }
            }
            p = strchr(p, '}');
            if (!p) break;
            p++;
            if (*p == ',') p++;
        } else {
            break;
        }
    }
}

/* 读取 resume.json */
int load_resume_game(GameState *game, int *mode, int *elapsed_seconds)
{
    if (!game) return 0;

    FILE *fp = fopen(RESUME_FILE, "r");
    if (!fp) return 0;

    /* 读整文件 */
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0 || sz > 2000000) { /* 防呆：太小或太大都直接当失败 */
        fclose(fp);
        return 0;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return 0;
    }

    size_t got = fread(buf, 1, (size_t)sz, fp);
    buf[got] = '\0';
    fclose(fp);

    int local_mode = 1;
    int local_elapsed = 0;
    int local_current = 1;
    int local_undo = 0;

    const char *m = strstr(buf, "\"mode\":");
    if (m) sscanf(m + 7, "%d", &local_mode);

    const char *el = strstr(buf, "\"elapsed\":");
    if (el) sscanf(el + 10, "%d", &local_elapsed);

    const char *cur = strstr(buf, "\"current\":");
    if (cur) sscanf(cur + 10, "%d", &local_current);

    const char *u = strstr(buf, "\"undo\":");
    if (u) sscanf(u + 7, "%d", &local_undo);

    parse_resume_moves(buf, game);

    game->undo_count = local_undo;
    game->finished = 0;
    game->winner = 0;

    if (local_current == 1 || local_current == 2) {
        game->current_player = local_current;
    } else {
        /* 兜底：按步数奇偶推一下当前玩家 */
        game->current_player = (game->moves_count % 2 == 0) ? 1 : 2;
    }

    if (mode) *mode = local_mode;
    if (elapsed_seconds) *elapsed_seconds = local_elapsed;

    free(buf);
    return 1;
}
