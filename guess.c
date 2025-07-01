#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "guess.h"

void play_guess_game() {
    int number, guess, tries = 0;
    srand((unsigned int)time(NULL));
    number = rand() % 100 + 1; // 1-100
    printf("欢迎来到猜数字游戏！我已经想好了一个 1 到 100 之间的数字。\n");
    do {
        printf("请输入你的猜测: ");
        if (scanf("%d", &guess) != 1) {
            printf("输入无效，请输入一个整数。\n");
            while (getchar() != '\n'); // 清空输入缓冲区
            continue;
        }
        tries++;
        if (guess < number) {
            printf("太小了！\n");
        } else if (guess > number) {
            printf("太大了！\n");
        } else {
            printf("恭喜你，猜对了！你总共猜了 %d 次。\n", tries);
        }
    } while (guess != number);
}
