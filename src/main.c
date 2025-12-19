/*
 * main.c
 * 程序入口 + 主循环：菜单 -> 对局/回放；顺带管一下 SDL 的初始化/清理、音频小音效。
 */

#include <stdio.h>   // 用于输入输出（比如 printf 打印文字）
#include <stdlib.h>  // 用于内存管理、随机数等
#include <string.h>  // 用于处理字符串（文字）
#include <time.h>    // 用于获取时间（比如生成随机数种子）
#include <math.h>    // 用于数学计算（比如生成声音的正弦波函数）

/* 
 * 定义圆周率 π（派）
 * 有些编译器可能没有定义 M_PI，所以我们自己定义一个
 * 这个值会用来生成声音（正弦波的计算需要用到 π）
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* 
 * Windows 系统特有的头文件
 * 在 Windows 系统下，我们需要这个来设置控制台编码，防止中文显示乱码
 */

/* ========== 第二部分：引入我们自己写的模块 ========== */

#include "game.h"    // 游戏核心逻辑（棋盘、下棋规则等）
#include "gui.h"     // 图形界面（绘制棋盘、按钮等）
#include "ai.h"      // 人工智能（电脑下棋的逻辑）
#include "fileio.h"  // 文件读写（保存和加载对局记录）
#include "utils.h"   // 小工具函数（一些杂项）

/* 
 * 回放历史对局时，每步之间的延迟时间（单位：毫秒）
 * 300 毫秒 = 0.3 秒，意思是在回放时，每下一步棋等待 0.3 秒再显示下一步
 * 这样用户才能看清楚每一步是怎么下的
 */
static const int PLAYBACK_INTERVAL = 300;

/* ========== 第三部分：全局变量（整个程序都可以用的数据） ========== */

/* 计分板：双人对战和人机对战分开记分（互不影响）。 */
static int score_pvp_black = 0;
static int score_pvp_white = 0;
static int score_ai_black = 0;
static int score_ai_white = 0;

/* 这些变量用来控制游戏的音效（下棋时的"点击"声）；audio_dev： */
static SDL_AudioDeviceID audio_dev = 0;  // 音频设备的 ID
static double click_phase = 0.0;         // 声音相位，用于生成正弦波
static int click_samples_remaining = 0;  // 还剩多少采样点要播放（倒计时）
static const int audio_sample_rate = 48000;  // 采样率：每秒 48000 次

/* ========== 第四部分：音频相关函数 ========== */

/* 音频回调函数 - 生成声音数据；- sin() : 来自 <math.h>，正弦函数，用于生成正弦波（声音波形） */
static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    // 把声音数据缓冲区转换成浮点数数组（这样便于计算）
    float *buf = (float *)stream;
    
    // 计算需要生成多少个采样点
    // 比如缓冲区大小是 4096 字节，每个浮点数是 4 字节，那么需要 1024 个采样点
    int samples = len / sizeof(float);
    
    // 循环生成每个采样点的数据
    for (int i = 0; i < samples; i++) {
        float sample = 0.0f;  // 初始化为 0（静音）
        
        // 如果还有剩余采样点要播放（倒计时还没到 0）
        if (click_samples_remaining > 0) {
            // 设置声音频率为 880 赫兹（Hz）
            // 这个频率比较高，会产生清脆的"滴"声
            //
            double click_freq = 880.0;
            
            // 使用正弦波公式生成声音数据
            // sin 函数会产生 -1 到 1 之间的值
            // 乘以 0.5 是为了降低音量（避免声音太大）
            sample = 0.5f * (float)sin(2.0 * M_PI * click_phase);
            
            // 更新相位
            // 这样下次计算时就会产生下一个位置的值
            click_phase += click_freq / audio_sample_rate;
            
            // 如果相位超过 1.0，就减掉 1.0（让它在 0-1 之间循环）
            // 因为正弦波是周期性重复的，
            if (click_phase >= 1.0) click_phase -= 1.0;
            
            // 倒计时减 1（播放了一个采样点，还剩的要减 1）
            click_samples_remaining--;
        }
        
        // 把生成的声音数据放进缓冲区
        buf[i] = sample;
    }
}

/* 初始化音频设备；- SDL_zero() : SDL 库函数，将结构体清零（类似 memset） */
static int init_audio(void)
{
    // SDL_AudioSpec 是一个结构体，用来描述音频参数
    SDL_AudioSpec want, have;  // want 是我们想要的配置，have 是系统实际提供的配置
    
    // 先把配置单清零（清空所有字段）
    SDL_zero(want);
    
    // 设置我们想要的音频参数：
    want.freq = audio_sample_rate;      // 采样率：每秒 48000 次
    want.format = AUDIO_F32SYS;         // 音频格式：32 位浮点数（精度高，音质好）
    want.channels = 1;                  // 声道数：1 表示单声道（立体声是 2）
    want.samples = 4096;                // 缓冲区大小：4096 个采样点
    want.callback = audio_callback;     // 回调函数：告诉系统"要用这个函数生成声音"
    
    // 尝试打开音频设备
    //
    audio_dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    
    // 如果打开失败（返回 0 或负数），说明音频设备不可用
    if (!audio_dev) {
        fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
        return 1;  // 返回 1 表示失败
    }
    
    // 取消暂停，开始播放音频
    SDL_PauseAudioDevice(audio_dev, 0);
    
    return 0;  // 返回 0 表示成功
}

/* 播放一次短促的点击音效；无（只设置了一个全局变量） */
static void play_click_sound(void)
{
    // 设置要播放 0.05 秒的音效（2400 个采样点）
    // audio_sample_rate / 20 = 48000 / 20 = 2400
    click_samples_remaining = audio_sample_rate / 20;
}

/* 关闭音频设备；- SDL_PauseAudioDevice() : SDL 库函数，暂停音频播放 */
static void close_audio(void)
{
    // 如果音频设备已经打开（audio_dev 不为 0）
    if (audio_dev) {
        // 暂停音频设备
        SDL_PauseAudioDevice(audio_dev, 1);
        
        // 关闭音频设备
        SDL_CloseAudioDevice(audio_dev);
        
        // 把设备 ID 设为 0，表示"没有设备在使用"
        audio_dev = 0;
    }
}

/* ========== 第五部分：游戏核心函数 ========== */

/* 执行一局游戏；- snprintf() : 来自 <stdio.h>，格式化字符串（类似 printf，但写入到缓冲区） */
static void run_game_internal(int mode, const GameState *resume_state, int resume_elapsed)
{
    // 这个变量控制是否继续玩下一局
    // 1 表示"继续玩"，0 表示"不玩了，退出"
    int continuePlaying = 1;

    /* 如果是从“继续上次对局”进来的，只在第一盘用存档状态。 */
    int first_round = 1;

    /* ========== 第一步：创建游戏窗口 ========== */
    
    // 定义两个变量用来存储窗口和渲染器
    // SDL_Window 是窗口
    // SDL_Renderer 是渲染器（用来在窗口上画东西，
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;
    
    // 初始化图形界面（创建窗口和渲染器）
    // gui_init 函数会创建窗口，并把窗口和渲染器的地址存到 win 和 ren 里
    if (gui_init(&win, &ren) != 0) {
        printf("图形界面初始化失败\n");
        return;  // 如果初始化失败，直接退出函数
    }
    
    // 设置窗口标题，显示当前比分
    // 比如："六子棋 - 黑:2 白:1" 表示黑棋赢了 2 局，白棋赢了 1 局
    char titleBuf[64];  // 创建一个字符串数组，用来存放标题文字
    {
        int sb = 0, sw = 0;
        const char *mode_text = "双人";
        if (mode == 1) {
            sb = score_pvp_black;
            sw = score_pvp_white;
            mode_text = "双人";
        } else {
            sb = score_ai_black;
            sw = score_ai_white;
            if (mode == 2) mode_text = "人机-简单";
            if (mode == 3) mode_text = "人机-中级";
            if (mode == 4) mode_text = "人机-困难";
        }
        snprintf(titleBuf, sizeof(titleBuf), "六子棋(%s) - 黑:%d 白:%d", mode_text, sb, sw);
        SDL_SetWindowTitle(win, titleBuf);
    }  // 把标题设置到窗口上
    
    /* 选好“当前模式对应的计分板”——双人和人机分开算。 */
    int *score_black_ptr = (mode == 1) ? &score_pvp_black : &score_ai_black;
    int *score_white_ptr = (mode == 1) ? &score_pvp_white : &score_ai_white;

/* ========== 外层循环：可以连续玩多局游戏 ========== */
    
    // 只要 continuePlaying 是 1（继续玩），就一直循环
    // 每次循环开始新的一局游戏
    while (continuePlaying) {
        /* ========== 初始化新的一局游戏 ========== */
        
        // 创建一个 GameState 结构体，用来存储游戏状态
        //
        GameState game;

        /* 本局计时器：从开局那一刻开始计时（右上角显示 mm:ss） */
        Uint32 start_ticks = SDL_GetTicks();

        /* 第一盘如果有存档，就用存档开局；否则正常新开一盘。 */
        if (first_round && resume_state) {
            game = *resume_state;

            if (resume_elapsed < 0) resume_elapsed = 0;
            start_ticks = SDL_GetTicks() - (Uint32)resume_elapsed * 1000;
        } else {
            init_game(&game);
        }

        
        // 这些变量用来控制游戏流程：
        int running = 1;        // 1 表示游戏正在进行，0 表示这局游戏结束了
        int game_over = 0;      // 0 表示游戏还没结束，1 表示游戏已经结束（有人赢了或平局）

        /* 如果是续玩：把最后一步的位置抓出来（用于高亮），同时决定是不是已经结束。 */
        if (first_round && resume_state) {
            game_over = game.finished;
            /* 极少数情况下，存档时轮到 AI：继续后直接让 AI 走一步。 */
            if (!game_over && mode >= 2 && mode <= 4 && game.current_player == 2) {
                int before = game.moves_count;
                ai_move(&game, mode - 1);

                if (game.moves_count > before) {
                    play_click_sound();
                }

                game_over = game.finished;
            }
        }

        first_round = 0;  /* 只第一盘用存档 */

        /* ========== 内层循环：游戏进行中的每一帧 ========== */
        
        // 只要 running 是 1（游戏还在进行），就一直循环
        // 每次循环处理一帧的画面
        while (running) {
            /* ========== 处理用户输入（事件循环） ========== */
            
            // SDL_Event 是一个结构体，用来存储用户的操作（比如点击鼠标、按键盘、关闭窗口等）
            //
            SDL_Event e;
            
            // SDL_PollEvent 会检查是否有新的事件（比如用户点击了鼠标）
            // 这个循环会把所有待处理的事件都处理完
            while (SDL_PollEvent(&e)) {
                // 如果用户点击了窗口的关闭按钮（右上角的 ×）
                if (e.type == SDL_QUIT) {
                    /* 关窗口也算“中途退出”：帮你把局面存一份，回主菜单就能继续。 */
                    if (!game_over) {
                        int elapsed = (int)((SDL_GetTicks() - start_ticks) / 1000);
                        save_resume_game(&game, mode, elapsed);
                    }

                    running = 0;         // 退出内层循环（这局游戏结束）
                    continuePlaying = 0; // 退出外层循环（不再玩下一局）
                    break;               // 跳出事件处理循环
                }
                /* 键盘：悔棋（U 或 Ctrl+Z） */
                else if (!game_over && e.type == SDL_KEYDOWN) {
                    SDL_Keycode key = e.key.keysym.sym;
                    SDL_Keymod mod = (SDL_Keymod)e.key.keysym.mod;

                    /* ESC：保存并退出到主菜单（以后可以“继续上次对局”）。 */
                    if (key == SDLK_ESCAPE) {
                        int elapsed = (int)((SDL_GetTicks() - start_ticks) / 1000);
                        save_resume_game(&game, mode, elapsed);

                        running = 0;
                        continuePlaying = 0;
                        break;
                    }


                    int want_undo = 0;
                    if (key == SDLK_u) {
                        want_undo = 1;
                    }
                    if (key == SDLK_z && (mod & KMOD_CTRL)) {
                        want_undo = 1;
                    }

                    if (want_undo) {
                        /* 一次按键算一次悔棋 */
                        int did = 0;

                        if (mode >= 2 && mode <= 4) {
                            /* 人机模式：通常希望“退回到人类能下棋的回合”，
                             * 所以可能要撤销 1~2 步。 */
                            did |= undo_last_move(&game);
                            if (did && game.current_player != 1) {
                                did |= undo_last_move(&game);
                            }
                        } else {
                            did = undo_last_move(&game);
                        }

                        if (did) {
                            game.undo_count++;
                        }
                    }
                }
                // 如果用户按下了鼠标左键，并且游戏还没结束
                else if (!game_over && e.type == SDL_MOUSEBUTTONDOWN &&
                         e.button.button == SDL_BUTTON_LEFT) {
                    
                    /* ========== 处理鼠标点击：下棋 ========== */
                    
                    // 将鼠标点击的屏幕坐标（像素坐标）转换为棋盘的行列坐标
                    //   - 返回值：1 表示点击在棋盘范围内，0 表示点击在棋盘外面
                    int row, col;
                    if (pixel_to_cell(e.button.x, e.button.y, &row, &col)) {
                        // 成功转换坐标，说明用户点击了棋盘
                        //   - 这时玩家点击鼠标是无效的，要等电脑下完才能下
                        if ((mode >= 2 && mode <= 4) && game.current_player != 1) {
                            // 忽略这次点击，继续处理下一个事件
                            continue;
                        }
                        
                        // 检查这个位置是否为空（还没有棋子）
                        // CELL_EMPTY 表示空位，可以下棋
                        // CELL_BLACK 表示黑棋，CELL_WHITE 表示白棋，不能重复下
                        if (game.cells[row][col] == CELL_EMPTY) {
                            // 这个位置是空的，可以下棋！
                            // ========== 第一步：在棋盘上放置棋子 ==========
                            
                            // 在棋盘的 [row][col] 位置放置一颗棋子
                            // 注意：这里直接赋值，而不是调用 place_stone 函数
                            game.cells[row][col] = game.current_player;
                            
                            // 记录这一步棋的位置（用于后续检查胜负）
                            
                            // ========== 第二步：记录这一步棋到历史记录 ==========
                            
                            // 把这一步棋存到 moves 数组里（用于回放和保存）
                            // 先检查数组是否还有空间（防止越界）
                            if (game.moves_count < BOARD_SIZE * BOARD_SIZE) {
                                // 获取当前步数的记录位置
                                Move *m = &game.moves[game.moves_count];
                                // 记录这一步的信息
                                m->row = row;      // 行号
                                m->col = col;      // 列号
                                m->player = game.current_player;  // 是哪一方下的
                            }
                            game.moves_count++;  // 步数加 1
                            
                            // ========== 第三步：播放音效 ==========
                            
                            // 播放"滴"的一声，让用户知道已经成功下棋了
                            play_click_sound();
                            
                            // ========== 第四步：检查是否有人赢了 ==========
                            
                            // check_win 函数检查在 (row, col) 位置下棋后，是否形成了连续六子
                            // 返回值：非 0 表示有人赢了，0 表示还没人赢
                            // 如果赢了，函数内部会把 game.winner 设置成赢家的编号（1 或 2）
                            if (check_win(&game, row, col)) {
                                // 有人赢了！游戏结束
                                game_over = 1;
                                game.winner = game.current_player;  // 记录赢家（1=黑, 2=白）
                            } 
                            // 如果没人赢，检查棋盘是否下满了（平局）
                            else if (board_full(&game)) {
                                // 棋盘满了，但是没人赢，那就是平局
                                game_over = 1;
                                game.winner = 0;  // 0 表示平局（没人赢）
                            } 
                            // 如果既没人赢，棋盘也没满，游戏继续
                            else {
                                // ========== 第五步：切换玩家 ==========
                                
                                // 切换当前玩家（黑棋 ↔ 白棋）
                                // switch_player 函数会把 game.current_player 从 1 改成 2，或从 2 改成 1
                                //
                                switch_player(&game);
                                // ========== 第六步：如果是人机模式，让电脑下棋 ==========
                                
                                // 如果是人机模式（mode = 2、3 或 4），并且轮到电脑下棋（current_player == 2）
                                // 那么调用 AI 函数让电脑自动下棋
                                if ((mode >= 2 && mode <= 4) && game.current_player == 2) {
                                    // 调用 AI 函数计算电脑的下一步
                                    // 对应模式：mode-1 即难度等级（2->1 简单，3->2 中级，4->3 困难）
                                    ai_move(&game, mode - 1);
                                    
                                    // ai_move 函数内部已经调用了 place_stone() 并把步数记录到 moves 数组了
                                    // 所以我们这里不需要再记录
                                    
                                    // 取出 AI 最新下的那一步，用于后续检查胜负
                                    // 播放点击声，让用户知道电脑已经下棋了
                                    play_click_sound();
                                    
                                    // 电脑下完棋后，检查是否有人赢了或棋盘满了
                                    // ai_move 函数内部会调用 check_win，如果赢了会把 game.finished 设为 1
                                    if (game.finished) {
                                        game_over = 1;  // 游戏结束
                                        // 注意：winner 已经在 ai_move 内部设置了，这里不需要再设置
                                    } 
                                    // else if (board_full(&game)) {
                                    // }
                                }
                            }
                        }
                    }
                }
            }
            
            /* ========== 渲染画面（把棋盘画到屏幕上） ========== */
            
            // 绘制棋盘和棋子
            //   - 最后一步的标记（通常用圆圈或高亮显示）
            draw_game(ren, &game);
            
            // 绘制计分板（显示黑棋和白棋各赢了多少局）
            // 计分板的分数在"再来一局"时保持不变
            // 只有程序重新启动时才会清零
            draw_scoreboard(ren, *score_black_ptr, *score_white_ptr);

            /* 右上角 HUD：计时器 + 悔棋次数 */
            int elapsed_seconds = (int)((SDL_GetTicks() - start_ticks) / 1000);
            draw_timer(ren, elapsed_seconds);
            draw_undo_count(ren, game.undo_count);
            
            // 把所有绘制的内容显示到窗口上
            // 之前的所有 draw_xxx 函数只是在内存中"画"好了，还没有真正显示
            // SDL_RenderPresent
            SDL_RenderPresent(ren);
            /* ========== 游戏结束后的处理 ========== */
            
            // 如果游戏已经结束（有人赢了或平局）
            if (game_over) {
                // ========== 第一步：更新计分板 ==========
                
                // 根据获胜者更新全局计分板
                //   - 0 表示平局（没人赢）
                if (game.winner == 1) {
                    (*score_black_ptr)++;  // 黑方胜局数加 1
                } else if (game.winner == 2) {
                    (*score_white_ptr)++;  // 白方胜局数加 1
                }
                // 如果是平局（winner == 0），双方都不加分
                
                // 更新窗口标题，显示最新的比分
                // 比如原来显示"六子棋 - 黑:2 白:1"，黑方又赢了一局后变成"六子棋 - 黑:3 白:1"
                char tb[64];
                {
                    int sb = *score_black_ptr;
                    int sw = *score_white_ptr;
                    const char *mode_text = (mode == 1 ? "双人" :
                                             (mode == 2 ? "人机-简单" : (mode == 3 ? "人机-中级" : "人机-困难")));
                    snprintf(tb, sizeof(tb), "六子棋(%s) - 黑:%d 白:%d", mode_text, sb, sw);
                    SDL_SetWindowTitle(win, tb);
                }
                
                // ========== 第二步：保存对局记录到文件 ==========
                
                // 调用 save_record 函数将当前对局保存到文件
                // 保存的信息包括：对局时间、获胜者、每一步的详细记录
                // 这样用户以后可以回放历史对局
                if (save_record(&game)) {
                    // 保存成功
                    printf("对局记录已保存\n");
                } else {
                    // 保存失败（可能是文件权限问题或磁盘空间不足）
                    fprintf(stderr, "警告：保存对局记录失败\n");
                }
                
                /* 这盘已经结束了：续玩存档可以清掉了（避免菜单里一直出现“继续上次对局”）。 */
                clear_resume_game();

                // ========== 第三步：显示胜负结果 ==========

                // 在显示再来一局/退出菜单之前，先显示胜负提示
                draw_game_result(ren, game.winner);
                // 等待一小段时间，让玩家看清胜负信息
                SDL_Delay(1500);

                // ========== 第四步：显示结束菜单 ==========

                // 绘制半透明的覆盖层，并在上面显示两个按钮：
                //   - "再来一局"：重新开始新的一局游戏
                //   - "退出游戏"：退出当前游戏，返回主菜单
                draw_end_menu(ren);
                // ========== 第五步：等待用户选择 ==========

                // waiting 变量控制是否还在等待用户点击按钮
                // 1 表示"还在等待"，0 表示"用户已经做出选择了"
                int waiting = 1;

                // 一直循环，直到用户点击了按钮或关闭窗口
                while (waiting) {
                    SDL_Event ev;
                    // 处理所有事件（鼠标点击、窗口关闭等）
                    while (SDL_PollEvent(&ev)) {
                        // 如果用户关闭窗口
                        if (ev.type == SDL_QUIT) {
                            // 直接退出，不再继续游戏
                            running = 0;
                            continuePlaying = 0;
                            waiting = 0;
                            break;
                        }
                        // 如果用户点击了鼠标左键
                        else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                            // 获取鼠标点击的坐标
                            int mx = ev.button.x;  // 鼠标 x 坐标
                            int my = ev.button.y;  // 鼠标 y 坐标

                            // 定义两个按钮的位置和大小
                            // 这些数值需要和 draw_end_menu 函数中绘制按钮的位置一致
                            int bw = 200;   // 按钮宽度
                            int bh = 80;    // 按钮高度
                            int leftBtnX = (WINDOW_WIDTH / 2) - bw - 20;      // 左侧按钮的 x 坐标
                            int topBtnY  = (WINDOW_HEIGHT / 2) - (bh / 2);    // 按钮的 y 坐标（两个按钮一样）

                            // 定义两个按钮的矩形区域（位置和大小）
                            SDL_Rect replayRect = {leftBtnX, topBtnY, bw, bh};           // "再来一局"按钮
                            SDL_Rect quitRect   = {leftBtnX + bw + 40, topBtnY, bw, bh}; // "退出游戏"按钮（在右边）

                            // 检查用户点击的是否是"再来一局"按钮（左侧按钮）
                            // 判断方法：检查鼠标坐标是否在按钮的矩形区域内
                            if (mx >= replayRect.x && mx <= replayRect.x + replayRect.w &&
                                my >= replayRect.y && my <= replayRect.y + replayRect.h) {
                                // 用户点击了"再来一局"
                                waiting = 0;      // 停止等待
                                running = 0;      // 结束当前局游戏（会重新开始新的一局）
                                break;            // 跳出事件循环
                            }

                            // 检查用户点击的是否是"退出游戏"按钮（右侧按钮）
                            if (mx >= quitRect.x && mx <= quitRect.x + quitRect.w &&
                                my >= quitRect.y && my <= quitRect.y + quitRect.h) {
                                // 用户点击了"退出游戏"
                                waiting = 0;         // 停止等待
                                running = 0;         // 结束当前局游戏
                                continuePlaying = 0; // 不再玩下一局（退出到主菜单）
                                break;               // 跳出事件循环
                            }
                        }
                    }
                    // 稍微延迟一下，避免 CPU 占用过高
                    SDL_Delay(10);  // 延迟 10 毫秒
                }
            }
            
            // 每帧都延迟一下，控制游戏循环的速度
            // 如果没有延迟，循环会运行得非常快，CPU 占用会很高
            SDL_Delay(10);
        }
    }
    
    // ========== 游戏结束，清理资源 ==========
    
    // 关闭窗口和渲染器，释放资源
    //
    gui_quit(win, ren);
}


/* 新开一局：为了避免“继续上次对局”指向旧局面，这里先把 resume.json 清掉。 */
static void run_game(int mode)
{
    clear_resume_game();
    run_game_internal(mode, NULL, 0);
}

/* 继续上次对局：从 resume.json 读出局面、模式、计时器，然后直接开局。 */
static void run_resume_game(void)
{
    GameState game;
    int mode = 1;
    int elapsed = 0;

    if (!load_resume_game(&game, &mode, &elapsed)) {
        printf("没有可继续的存档。\n");
        return;
    }

    run_game_internal(mode, &game, elapsed);
}



/* ========== 第六部分：回放功能 ========== */

/* 从文件读取并播放一局历史对弈；- fopen() : 打开文件（"r" 模式表示只读） */
/* ========== 第六部分：回放功能 ========== */

/* 小工具：判断鼠标点是否落在某个矩形按钮里 */
static int point_in_rect(int x, int y, const SDL_Rect *r)
{
    if (!r) return 0;
    if (x < r->x) return 0;
    if (y < r->y) return 0;
    if (x > r->x + r->w) return 0;
    if (y > r->y + r->h) return 0;
    return 1;
}

/* 播放一局：自动一手一手走，鼠标左键随时退出回放 */
static void playback_one_game(SDL_Renderer *ren, const GameState *game)
{
    if (!ren || !game) return;

    GameState temp;
    init_game(&temp);

    int stop = 0;

    for (int i = 0; i < game->moves_count; i++) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                stop = 1;
                break;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN &&
                ev.button.button == SDL_BUTTON_LEFT) {
                /* 回放中点一下，就当“退出回放” */
                stop = 1;
                break;
            }
        }
        if (stop) break;

        Move m = game->moves[i];

        /* 回放时严格按记录里的落子方来下 */
        temp.current_player = m.player;
        temp.finished = 0;
        place_stone(&temp, m.row, m.col);

        draw_game(ren, &temp);
        SDL_RenderPresent(ren);
        SDL_Delay(PLAYBACK_INTERVAL);
    }

    if (stop) return;

    /* 播放完：展示胜负，点一下回到回放列表 */
    draw_game_result(ren, game->winner);

    int waiting = 1;
    while (waiting) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                waiting = 0;
                break;
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN &&
                ev.button.button == SDL_BUTTON_LEFT) {
                waiting = 0;
                break;
            }
        }
        SDL_Delay(10);
    }
}

/* 图形化的回放入口：列出“第 N 轮”按钮，并带删除按钮 */
static void run_playback(void)
{
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;

    if (gui_init(&win, &ren) != 0) {
        fprintf(stderr, "无法初始化界面，退出回放。\n");
        return;
    }

    const int per_page = 6;
    int page = 0;
    int running = 1;

    while (running) {
        int total = record_count();

        if (total <= 0) {
            draw_playback_empty(ren);
        } else {
            int pages = (total + per_page - 1) / per_page;
            if (pages <= 0) pages = 1;

            if (page < 0) page = 0;
            if (page >= pages) page = pages - 1;

            draw_playback_menu(ren, page, total, per_page);
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
                break;
            }

            if (ev.type == SDL_MOUSEBUTTONDOWN &&
                ev.button.button == SDL_BUTTON_LEFT) {

                int mx = ev.button.x;
                int my = ev.button.y;

                SDL_Rect backRect = { (WINDOW_WIDTH - 240) / 2, WINDOW_HEIGHT - 80, 240, 50 };

                if (point_in_rect(mx, my, &backRect)) {
                    running = 0;
                    break;
                }

                if (total <= 0) {
                    break;
                }

                /* 翻页按钮（只有记录多的时候才显示） */
                SDL_Rect prevRect = { 60, WINDOW_HEIGHT - 80, 120, 50 };
                SDL_Rect nextRect = { WINDOW_WIDTH - 60 - 120, WINDOW_HEIGHT - 80, 120, 50 };

                int pages = (total + per_page - 1) / per_page;
                if (pages < 1) pages = 1;

                if (total > per_page) {
                    if (point_in_rect(mx, my, &prevRect)) {
                        if (page > 0) page--;
                        break;
                    }
                    if (point_in_rect(mx, my, &nextRect)) {
                        if (page < pages - 1) page++;
                        break;
                    }
                }

                /* 逐行判断：点“第 N 轮”就回放；点“删除”就删这一条 */
                int list_w = WINDOW_WIDTH * 3 / 4;
                int left = (WINDOW_WIDTH - list_w) / 2;
                int row_h = 52;
                int gap = 14;
                int top = 110;

                int del_w = 90;
                int play_w = list_w - del_w - 10;

                int start_index = page * per_page;
                int show_count = total - start_index;
                if (show_count > per_page) show_count = per_page;
                if (show_count < 0) show_count = 0;

                int did_action = 0;

                for (int i = 0; i < show_count; i++) {
                    int idx = start_index + i;
                    SDL_Rect playRect = { left, top + i * (row_h + gap), play_w, row_h };
                    SDL_Rect delRect  = { left + play_w + 10, top + i * (row_h + gap), del_w, row_h };

                    if (point_in_rect(mx, my, &delRect)) {
                        delete_record(idx);

                        /* 删完可能页数变少，下一轮循环会自动夹紧 page */
                        did_action = 1;
                        break;
                    }

                    if (point_in_rect(mx, my, &playRect)) {
                        GameState g;
                        if (load_record(idx, &g)) {
                            playback_one_game(ren, &g);
                        }
                        did_action = 1;
                        break;
                    }
                }

                if (did_action) break;
            }
        }

        SDL_Delay(10);
    }

    gui_quit(win, ren);
}


/* ========== 第七部分：主菜单 ========== */

/* 显示主菜单界面并等待用户点击；- SDL_PollEvent() : SDL 库函数，检查并获取事件（鼠标点击等） */
static int show_main_menu(void)
{
    SDL_Window *win = NULL;
    SDL_Renderer *ren = NULL;

    if (gui_init(&win, &ren) != 0) {
        fprintf(stderr, "无法初始化菜单窗口\n");
        return 0;  // 直接当退出
    }

    /* 主菜单：先看看有没有“未结束的存档” */
    int has_resume = has_resume_game();
    draw_main_menu(ren, has_resume);

    /* 菜单状态：0=主菜单；1=人机难度 */
    int state = 0;

    int selection = 0;  // 0 表示“还没选”
    int running = 1;

    /* 主菜单按钮布局（要和 gui.c 里保持一致） */
    const int bw_main = WINDOW_WIDTH * 3 / 4;
    const int bh_main = 60;
    const int spacing_main = 20;
    const int top_main = 80;
    const int left_main = (WINDOW_WIDTH - bw_main) / 2;
    const int main_count = 5;

    /* 人机难度按钮布局（要和 gui.c 里保持一致） */
    const int bw_ai = WINDOW_WIDTH * 3 / 4;
    const int bh_ai = 60;
    const int spacing_ai = 20;
    const int top_ai = 120;
    const int left_ai = (WINDOW_WIDTH - bw_ai) / 2;
    const int ai_count = 4;

    while (running) {
        SDL_Event e;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                selection = 0;
                running = 0;
                break;
            }

            /* AI 菜单里按 ESC 直接回主菜单 */
            if (e.type == SDL_KEYDOWN && state == 1) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    state = 0;
                    has_resume = has_resume_game();
                    draw_main_menu(ren, has_resume);
                }
            }

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x;
                int my = e.button.y;

                if (state == 0) {
                    /* 主菜单：5 个按钮 */
                    for (int i = 0; i < main_count; i++) {
                        int bx = left_main;
                        int by = top_main + i * (bh_main + spacing_main);

                        if (mx >= bx && mx <= bx + bw_main && my >= by && my <= by + bh_main) {
                            if (i == 0) {
                                /* 继续上次对局：没存档就别选了 */
                                if (!has_resume) break;
                                selection = 1;  // 继续
                            } else if (i == 1) {
                                selection = 2;  // 双人
                            } else if (i == 2) {
                                /* 点“人机对战”：切到难度选择 */
                                state = 1;
                                draw_ai_difficulty_menu(ren);
                                break;
                            } else if (i == 3) {
                                selection = 6;  // 回放
                            } else {
                                selection = 0;  // 退出
                            }

                            if (selection != 0) running = 0;
                            else running = 0;  // 退出也要关菜单
                            break;
                        }
                    }
                } else {
                    /* 人机难度菜单：4 个按钮 */
                    for (int i = 0; i < ai_count; i++) {
                        int bx = left_ai;
                        int by = top_ai + i * (bh_ai + spacing_ai);

                        if (mx >= bx && mx <= bx + bw_ai && my >= by && my <= by + bh_ai) {
                            if (i == 0) selection = 3;      // 人机简单
                            else if (i == 1) selection = 4; // 人机中级
                            else if (i == 2) selection = 5; // 人机困难
                            else {
                                /* 返回主菜单 */
                                state = 0;
                                has_resume = has_resume_game();
                                draw_main_menu(ren, has_resume);
                                selection = 0;
                            }

                            if (selection != 0) running = 0;
                            break;
                        }
                    }
                }
            }
        }

        SDL_Delay(10);
    }

    gui_quit(win, ren);
    return selection;
}


/* ========== 第八部分：程序入口 main 函数 ========== */

/* main 函数 - 程序的入口点；- SetConsoleOutputCP() : Windows API，设置控制台输出编码（UTF-8） */
int main(int argc, char *argv[])
{
    // ========== 第一步：处理未使用的参数 ==========
    
    // 这两个参数我们没用，但是 C 语言要求 main 函数必须有这两个参数
    // 加上 (void) 可以告诉编译器"我知道这个参数没用，这是故意的"
    // 这样编译器就不会发出警告了
    (void)argc;
    (void)argv;
    
    // ========== 第二步：Windows 系统特殊设置 ==========
    
    /* Windows 下不再申请控制台窗口：全程只用图形界面 */
    
    // ========== 第三步：初始化随机数生成器 ==========
    
    // 设置随机数种子（用当前时间作为种子）
    // 这样每次运行程序时，AI 的随机下棋行为都会不同
    // 如果不用时间做种子，每次运行程序 AI 的行为都会一模一样（很无聊）
    srand((unsigned int)time(NULL));
    
    // ========== 第四步：初始化计分板 ==========
    
    // 程序启动时，把黑白双方的胜局数都清零
    // 这两个变量是全局变量，在整个程序运行期间都会保存分数
    score_pvp_black = 0;
    score_pvp_white = 0;
    score_ai_black = 0;
    score_ai_white = 0;
    
    // ========== 第五步：初始化 SDL（图形和音频库） ==========
    
    // SDL 是一个库，用来处理图形界面和音频
    // | 是"或"运算符，表示同时启用这两个子系统
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        // 如果初始化失败，打印错误信息并退出程序
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;  // 返回 1 表示程序异常退出
    }
    
    // ========== 第六步：初始化音频系统 ==========
    
    // 初始化音频设备（用来播放下棋时的音效）
    // 如果初始化失败，程序会继续运行，只是没有声音而已
    if (init_audio() != 0) {
        fprintf(stderr, "警告: 音频初始化失败，游戏将没有声音。\n");
        // 注意：这里不退出程序，只是警告一下，游戏还是可以玩的
    }
    
    // ========== 第七步：主循环（游戏的核心循环） ==========
    
    int running = 1;  // 1 表示程序还在运行，0 表示要退出了
    
    // 这个循环会一直运行，直到用户选择退出
    while (running) {
        // 显示主菜单，让用户选择要做什么。
        // show_main_menu 函数会显示菜单界面，等待用户点击，然后返回选择的编号（1-6）。
        int choice = show_main_menu();

        // 根据用户的选择，执行相应的功能
        switch (choice) {
            case 1:  // 继续上次对局
                run_resume_game();
                break;
            case 2:  // 双人对战
                run_game(1);
                break;
            case 3:  // 人机对战（简单）
                run_game(2);
                break;
            case 4:  // 人机对战（中级）
                run_game(3);
                break;
            case 5:  // 人机对战（困难）
                run_game(4);
                break;
            case 6:  // 回放历史对局
                run_playback();
                break;
            default:  // 退出游戏 / 关闭窗口
                running = 0;
                break;
        }
        
        // 注意：如果用户选择 1、2、3 或 4，执行完对应功能后会回到这里
        // 然后循环继续，再次显示主菜单
        // 只有选择 5 或关闭窗口，running 才会变成 0，循环才会结束
    }
    
    // ========== 第八步：清理资源，退出程序 ==========
    
    // 关闭音频设备
    close_audio();
    
    // 释放 SDL 占用的所有资源
    SDL_Quit();
    
    // 返回 0，表示程序正常退出
    return 0;
}