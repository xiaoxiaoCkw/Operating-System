#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

int main(int argc, char* argv[]) {
    char buffer[512]; // 512 bytes的输入缓冲区
    char *argv_sub[MAXARG]; // 需要每行运行的命令的参数
    int i;

    //检查参数数量是否正确
    if (argc < 2) {
        printf("xargs: too few arguments\n"); 
        exit(-1);
    }
    if (argc > MAXARG) {
        printf("xargs: too many arguments\n");
        exit(-1);
    }

    // 复制除了xargs命令以外的参数
    for (i = 1; i < argc; i++) {
        argv_sub[i-1] = argv[i];
    }
    argv_sub[argc] = 0; // 最后一个参数为0

    // 读取输入
    while (1) {
        i = 0;
        // 一次读取一行
        while (1) {
            if (read(0, &buffer[i], 1) <= 0 || buffer[i] == '\n') // 读取错误或者遇到换行符结束
                break;
            i++;
        }
        if (i == 0)
            break;
        buffer[i] = 0; // buffer相当于每行运行命令的参数，最后一个参数为0
        argv_sub[argc-1] = buffer;
        if (fork() == 0) {
            /* 子进程 */
            exec(argv_sub[0], argv_sub);
            exit(0);
        } else {
            /* 父进程 */
            wait(0); // 等待子进程结束
        }
    }
    exit(0);
}