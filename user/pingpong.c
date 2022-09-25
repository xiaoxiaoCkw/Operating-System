#include <kernel/types.h>
#include <user/user.h>

int main(int argc, char* argv[]) {
    if (argc != 1) {
        printf("pingpong does not need argument!"); //检查参数数量是否正确
        exit(-1);
    }
    /* 两个管道 */
    int p1[2]; // pipe1: 子进程读，父进程写
    int p2[2]; // pipe2: 子进程写，父进程读

    char buffer1[] = "ping"; // 父进程传给子进程的字符数组
    char buffer2[] = "pong"; // 子进程传给父进程的字符数组

    // n1和n2用于比较read和write返回值是否正确
    int n1 = sizeof(buffer1);
    int n2 = sizeof(buffer2);

    pipe(p1);
    pipe(p2);

    int ret = fork();
    if (ret == 0) {
        /* 子进程 */
        // 使用pipe1读端，关闭pipe1写端
        // 读"ping"
        close(p1[1]);
        if (read(p1[0], buffer1, n1) != n1) {
            printf("Child read failed!");
            exit(-1);
        } else {
            printf("%d: received %s\n", getpid(), buffer1);
        }
        // 使用pipe2写端，关闭pipe2读端
        // 写"pong"
        close(p2[0]);
        if (write(p2[1], buffer2, n2) != n2) {
            printf("Child write failed!");
            exit(-1);
        }
        exit(0);
    } else {
        /* 父进程 */
        // 使用pipe1写端，关闭pipe1读端
        // 写"ping"
        close(p1[0]);
        if (write(p1[1], buffer1, n1) != n1) {
            printf("Parent write failed!");
            exit(-1);
        }
        // 使用pipe2读端，关闭pipe2写端
        // 读"pong"
        close(p2[1]);
        if (read(p2[0], buffer2, n2) != n2) {
            printf("Parent read failed!");
            exit(-1);
        } else {
            printf("%d: received %s\n", getpid(), buffer2);
        }

        wait(0); // 等待子进程结束
        exit(0);
    }
}