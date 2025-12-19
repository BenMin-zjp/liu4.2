/*
 * utils.c
 *
 * 实现一些常用的小工具函数。
 */

#include "utils.h"
#include <stdio.h>

/* 等待用户按回车键；直接调用，不需要传参数： */
void wait_for_key(void)
{
    int ch;

    printf("(按 Enter 继续)\n");

    /* 先把上一轮 scanf/getchar 留下的东西吃掉，不然会“秒过”。 */
    do {
        ch = getchar();
        if (ch == EOF) return;
    } while (ch != '\n');

    /* 再等用户真按一次回车 */
    getchar();
}