
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/file.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include "queue.h"

void summary(int line_count, int file_count, float time);
void *worker(void *);
int file_count = 0;
int line_count = 0;
char **keyword;
pthread_mutex_t lock;
queue_t q;
char *data[1024];
char *new_task[1024];
int ind_new = 0;
int ind = 0;
clock_t start, end;
void handler(int sig)
{
    end = clock();
    summary(line_count, file_count, (float)(end - start) / CLOCKS_PER_SEC);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, handler);
    pthread_mutex_init(&lock, NULL);
    start = clock();
    char *dir;
    int num = 0;         //옵션 관련 인자
    int opt;             //옵션 인자받기
    extern char *optarg; //옵션 뒤에 오는 인자
    extern int optind;   //옵션이 끝나는 index위치

    //option 인자 받기
    while ((opt = getopt(argc, argv, "t:")) != -1)
    {
        switch (opt)
        {
        case 't': // child process 갯수 정하기
            num = atoi(optarg);
            if (num < 1 || num > 16)
            {
                perror("the number of worker process greater than 0 and less than 17.\n");
                exit(1);
            }
            break;
        case '?':
            perror("Unknow argument.\n");
            exit(1);
        }
    }
    //dir 입력했는지 확인
    if ((dir = argv[optind]) == NULL)
    {
        printf("There is no directory path\n");
        return 0;
    }
    //만약 keyword가 없으면 종료
    if (argc - optind == 1 || argc - optind > 9)
    {
        printf("the number of keyword greater than 0 and less than 10.\n");
        return 0;
    }

    //keyword 입력받기
    optind++; //위에 두개 if문 통과. 즉, dir과 keyword 다 존재. index올려주기
    keyword = (char **)malloc(sizeof(char *) * (argc - optind));
    for (int i = 0; i < argc - optind; i++)
    {
        keyword[i] = (char *)malloc(sizeof(char) * strlen(argv[optind + i]));
    }
    for (int i = 0; i < argc - optind; i++)
    {
        keyword[i] = argv[optind + i];
        printf("keyword check(m): %s\n", keyword[i]);
    }

    //checking
    printf("check precondition.\n");
    printf("directory is : %s\n", dir);
    printf("check process number %d\n", num);
    printf("---------------------------\n");
    /////////////전처리 끝

    Queue_Init(&q);
    Enqueue(&q, dir);

    int available_task; //1임
    pthread_t worker_thread[num];
    while (1)
    {
        size_t len1 = 0, len2 = 0, bs2 = 0;
        // worker의 갯수 정해주기
        available_task = q.count;
        if (available_task > num) //worker보다 일이 많으면
            available_task = num; // 일은 worker의 수로 맞춘다
        for (int i = 0; i < available_task; i++)
        {
            pthread_create(&worker_thread[i], NULL, worker, NULL);
        }
        for (int i = 0; i < available_task; i++)
        {
            pthread_join(worker_thread[i], NULL);
        }
        available_task = q.count;
        //-----------------------
        if (IsEmpty(&q))
            break;
        printf("\n\n");
    } ///여기가 제일 큰 무한루프 끝
    end = clock();
    summary(line_count, file_count, (float)(end - start) / CLOCKS_PER_SEC);
    for (int i = 0; i < argc - optind; i++)
    {
        free(keyword[i]);
    }
    free(keyword);
    return 0;
}

void *worker(void *arg)
{
    DIR *dp = NULL;
    FILE *check;
    struct dirent *file = NULL;
    char task[1024];
    char *filename;
    char buff[1024];
    size_t len1 = 0;
    //char *data = NULL;
    pthread_mutex_lock(&lock);
    if ((data[ind] = Dequeue(&q)) == NULL)
    {
        perror("failed to dequeue\n");
        exit(1);
    }
    sprintf(task, "%s", data[ind]);
    ind++;
    pthread_mutex_unlock(&lock);
    len1 = strlen(task);
    if (task[len1 - 1] != '/')
    {
        strcat(task, "/");
        len1++;
    }
    //===========================================update
    if ((dp = opendir(task)) == NULL)
    {
        perror("fail to open dir\n");
        exit(1);
    }

    for (int i = 0; (file = readdir(dp)) != NULL; i++)
    {
        //filename 정하기
        filename = file->d_name;
        //. 이랑 .. 무시하기
        if (!strcmp(filename, "..") || !strcmp(filename, "."))
        {
            continue;
        }
        char command[1024];
        sprintf(command, "%s %s%s", "file", task, filename);
        if ((check = popen(command, "r")) == NULL)
        {
            perror("failed to open file.\n");
            exit(1);
        }
        if (fgets(buff, 1024, check) == NULL)
        {
            perror("failed to read buffer\n");
            exit(1);
        }
        if (strstr(buff, "text"))
        {
            file_count++;
            int line = 1;
            char path[1024];
            char str[1024];
            sprintf(path, "%s%s", task, filename);
            FILE *fd = fopen(path, "r");
            if (fd == NULL)
            {
                perror("Error to open regular file.\n");
                exit(1);
            }
            while (!feof(fd))
            {
                int all_keyword_check = 1;
                char *p = fgets(str, 1024, fd);
                for (int j = 0; keyword[j] != NULL; j++)
                {
                    if (!strstr(str, keyword[j]))
                        all_keyword_check = all_keyword_check * 0;
                }
                if (all_keyword_check)
                { // 찾은 줄 출력하기
                    printf("%s:%d:%s\n", path, line, str);
                    line_count++;
                }
                line++;
            }
            fclose(fd);
        }
        else if (strstr(buff, "directory"))
        {
            char task_temp[1024];
            sprintf(task_temp, "%s%s%s", task, filename, "/");
            pthread_mutex_lock(&lock);
            new_task[ind_new] = malloc(sizeof(char) * strlen(task_temp));
            strcpy(new_task[ind_new], task_temp); //이걸로 포인터문제 해결함!!
            Enqueue(&q, new_task[ind_new]);
            pthread_mutex_unlock(&lock);
            ind_new++;
        }
        else
            continue;
    }
    return NULL;
}

void summary(int line_count, int file_count, float time)
{
    printf("\n\n\ntotal number of found lines : %d\n", line_count);
    printf("total number of explored files : %d\n", file_count);
    printf("total execute time : %fsec\n", time);
    return;
}