/*
 * utils.h
 * 一些零散小工具函数（目前主要是控制台暂停）。
 */

#ifndef UTILS_H
#define UTILS_H

/* 等待用户按回车键后继续（控制台暂停）；内部使用 <stdio.h> 中的函数： */
void wait_for_key(void);

#endif /* UTILS_H */