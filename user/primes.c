#include <kernel/types.h>
#include <user/user.h>

/* 为每个素数创建一个进程 */
void primes(int p[2]) {
    /* 管道p是左邻居，管道p1是右邻居 */
    /* 从管道p读出，向管道p1写入 */
    int p1[2];

    int prime;
    int n;

    // 从左邻居读出第一个数(一定是素数)
    close(p[1]);
    if (read(p[0], &prime, 4) == 0) { // int: 4 bytes
        // 递归结束标志: 左邻居无数读出
        close(p[0]);
        exit(0);
    } else {
        printf("prime %d\n", prime);
        // 筛选写入右邻居的数
        pipe(p1);
        if (fork() == 0) {
            /* 子进程 */
            close(p[0]);
            primes(p1); // 递归进入下一个素数的进程
        } else {
            /* 父进程 */
            close(p1[0]);
            while (1) {
                // 从左邻居读出一个数
                if (read(p[0], &n, 4) == 0) {
                    // 全部读出
                    break;
                } else {
                    if (n % prime != 0) {
                        // 排除掉prime的倍数
                        write(p1[1], &n, 4);
                    }
                }
            }
            close(p[0]);
            close(p1[1]);
            wait(0); // 应等待直到所有子孙进程终止 
        }
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc != 1) {
        printf("primes does not need argument!"); //检查参数数量是否正确
        exit(-1);
    }
    int p[2];
    pipe(p);
    if (fork() == 0) {
        /* 子进程 */
        primes(p);
    } else {
        /* 主进程 */
        // 将2~35输入第一个pipe
        close(p[0]);
        for (int i = 2; i <= 35; i++) {
            write(p[1], &i, 4);
        }
        close(p[1]);
        wait(0); // 应等待直到所有子孙进程终止
    }
    exit(0);
}