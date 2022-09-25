// 参照ls.c的逻辑实现

#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user.h"

char* fmtname(char *path) {
    /* 找到最后一个“/”后的第一个字符 */
    /* 返回指向该字符的指针 */
    char *p;
    
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;
    return p;
}

void find(char *path, char *filename) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // 打开文件
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 获取文件状态信息
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type) {
        case T_FILE: // fd是文件
            if (strcmp(fmtname(path), filename) == 0) {
                printf("%s\n", path);
            }
            break;
        case T_DIR: // fd是目录
            if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            // 递归下降到子目录
            while (read(fd, &de, sizeof(de)) == sizeof(de)) {
                if ((de.inum == 0) || strcmp(de.name, ".") ==0 || strcmp(de.name, "..") == 0) // 不要递归进入.和..
                    continue;
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                find(buf, filename);
            }
            break;
    }
    close(fd);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("find needs three arguments!\n"); //检查参数数量是否正确
        exit(-1);
    }
    find(argv[1], argv[2]);
    exit(0);
}