/* #################################################### */
/*                    21600786                          */
/*                      홍승훈                            */
/* #################################################### */
/*           ./pfind [<option>] <dir> [<keyword>]       */
/* [<option>] -p<N>(work process갯수지정)  -c(대소문자 무시) -a(절대경로인쇄) */
/* ./pfind -p<8> -c -a /home/s21600786/totaldir keyword  */ //example

/* Work Flow         
1. manager가 dir와 keyword를 넘긴다( named pipe - FIFO )
2. option, dir, keyword가 넘어오면 work는 searching한 후 pipe를 통해 결과들을 리턴한다.
*/
#define _CRT_SECURE_NO_WARNINGS
#define MAX_LENGTH 100000
#define TRUE 1
#define FALSE 0
#define Character 1
#define Absolute 2
#define Char_Abs 3
#define NONE 0

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

int pipes_1[2];
int pipes_2[2];

char* wherefile = "/usr/bin/file";
char* Asc = "ASCII";

//강제종료(ctrl+C)가 넘어오면 manager가 kill을 할 예정
//kill signal이 넘어오면 pipe로 현재 가진정보를 모두 리턴해야함.
void handler(int sig);
void Eliminate(char *str, char ch);
int read_bytes (int fd_read, void* a, int len);
int write_bytes (int fd_write, void* a, int len);

//DRIVER CODE
int main(int argc, char **argv)
{ //START
    //1. manager가 dir를 넘긴다( named pipe - FIFO - read by manager_to_worker pipe)
    
    //2. option, dir, keyword가 넘어오면 work는 searching한 후 pipe를 통해 결과들을 manager에게 리턴해야한다.
    // pipe를 넘길때 pipe를 flock

    //temp directory, keyword 지정
    char DONE[300] = "##########################################################################################################################################################################################################################################################################################################################################";
    char* temp_keyword = argv[1];
    char keyword[10][100];
    int count = 0;
    int option = 0; // 0 == none, 1 = character, 2 == abs path, 3 == char + abs path 
    char *ptr = strtok(temp_keyword, " ");
    while (ptr != NULL)
    {
        strcpy(keyword[count], ptr);
        ptr = strtok(NULL, "\n");
        count++;
    }
    //printf("count = %d\n", count);
    // for(int a = 0; a <count; a++)
    //     printf("keyword = %s\n", keyword[a]);
    char *temp_option = argv[2];
    if(strcmp(temp_option, "AC") == 0) option = Char_Abs;
    else if(strcmp(temp_option, "C") == 0 ) option = Character;
    else if(strcmp(temp_option, "A") == 0) option = Absolute;
    else option = NONE;
    char contents[MAX_LENGTH] = {0,};

    //이제 채널에서 읽어올껀데 flock걸어서 2번 읽은 후 flock풀어줘!
    //만약 읽은 데이터가 #DONE#면 종합된 결과들을 모두 worker_to_manager를 통해 manager에게 넘겨.
    // #DONE#이 아닐때 까지 계속해서 반복해야해!
    if (mkfifo("manager_to_worker", 0666)) //worker to manager 채널생성
    {
        if (errno != EEXIST)
        {
            perror("fail to open file: ");
            exit(1);
        }
    }
    int condition = 1;
    int fd_manager_to_worker = open("manager_to_worker", O_RDONLY|O_SYNC);
    char directory[300];
    sleep(1);
    while(condition)
    {
        if(condition != 0)
        {
            //printf("--------- WORKER: READ manager to worker pipe ---------\n");
        }
        while(1)
        {
            size_t len, bs;
            flock(fd_manager_to_worker, LOCK_EX);

            if(read_bytes(fd_manager_to_worker, &len, sizeof(len)) != sizeof(len))
            {
                flock(fd_manager_to_worker, LOCK_UN);
                break;
            }
            bs = read_bytes(fd_manager_to_worker, directory, len);
            //printf("Dir  len = %d\n", len);
            flock(fd_manager_to_worker, LOCK_UN);
            //printf("TEST 1: %s\n", directory);
            if(bs != len) break;
            break;
        }
        

        //printf("TEST 2: %s\n", directory);
        if(strcmp(DONE, directory) == 0)
        {
            
            condition = 0;
            close(fd_manager_to_worker);
            //unlink("manager_to_worker");
            //여기서 부터 manager에게 보내기
            //printf("WORK ALL DONE\n");


            exit(0);
            return 0;
        }

        //char directory[300] = "/home/s21600786/OSHW3";
        DIR *dp = NULL;
        struct dirent *file = NULL;
        char list[100][20];
        int i = 0;
        int numbers;

        if ((dp = opendir(directory)) == NULL) /*opendir*/
        {
            return -1;
        }
        else
        {
            while ((file = readdir(dp)) != NULL) /*fill list[]*/
            {
                if (strcmp(file->d_name, ".") && strcmp(file->d_name, "..") && (file->d_type == 8))
                {
                    strcpy(list[i], file->d_name);
                    //printf("list %s", list[i]);
                    i++;
                }
            }
            numbers = i;
        }
        closedir(dp);
        
        pid_t temp[numbers];
        pid_t test_child;
        strcat(directory, "/");
        test_child = fork();
        if (test_child == 0)
        { /*make a pipe*   ->   run the exe_file*/
            for (int i = 0; i < numbers; i++)
            {
                if (pipe(pipes_1) != 0)
                {
                    perror("Error");
                    exit(1);
                }

                temp[i] = fork();
                if (temp[i] == 0) //child
                {
                    dup2(pipes_1[1], 1); /* standard output*/
                    close(pipes_1[0]);
                    execl(wherefile, "file", strcat(directory, list[i]), (char *)NULL);
                }
                else //parent
                {
                    char buf[1001];
                    ssize_t s;
                    close(pipes_1[1]); /* close standard output*/
                    while ((s = read(pipes_1[0], buf, 1000)) > 0)
                    {
                        buf[s + 1] = 0x0;
                    }
                    close(pipes_1[0]);
                    //buf[]에는 TEMP.txt: ASCII text라고 정보가 들어가있음.
                    //여기서 부터 ASCII가 들어가면 해당 파일을 열고 keyword를 찾아내야함.
                    if (strstr(buf, Asc) != NULL)
                    {
                        /*parent -> worker by dup2() stdout*/
                        //dup2(pipes_2[1], 1);

                        //printf로 결과값 보내면 worker가 buf로 받음.
                        //line_num = 몇번째줄에서 발견됬나
                        //p = keyword가 공통적으로 발견된 문장.
                        /*sarching algorithm*/
                        int line_num = 0;
                        char path[10001];
                        int maxline = 100000;
                        char line[maxline];
                        char *pline;
                        int for_check;
                        strcpy(path, directory);
                        strcat(path, list[i]);
                        FILE *fd = fopen(path, "r");
                        //flock(fd,LOCK_EX);
                        if (fd == NULL)
                        {
                            perror("ERROR\n");
                            exit(1);
                        }
                        while (!feof(fd))
                        {
                            for_check = 1;
                            pline = fgets(line, maxline, fd);
                            line_num++;
                            for (int i = 0; i < (sizeof(keyword) / sizeof(keyword[0])); i++)
                                if (!strstr(pline, keyword[i]))
                                    for_check = for_check * 0;

                            if (for_check)
                            {
                                FILE *fp2 = fopen("RESULTS.txt", "a");
                                flock(fp2, LOCK_EX);
                                fprintf(fp2,"%s:%d:%s", path, line_num, pline); // -> pipe로 넘겨야 할 정보
                                flock(fp2, LOCK_UN);
                                fclose(fp2);
                            }
                        }
                        //flock(fd, LOCK_UN);
                        fclose(fd);
                        //searching multiple words function.
                        //returns # of line, whole sentence.
                    }
                     wait(NULL);
                }
            }
        }
        else
        { //test_child의 wait() -> 즉 worker 본체
            // char buf2[MAX_LENGTH];
            // ssize_t s;
            // close(pipes_2[1]); /* close standard output*/
            // while ((s = read(pipes_2[0], buf2, MAX_LENGTH - 1)) > 0)
            // {
            //     buf2[s + 1] = 0x0;
            // }
            // printf("%s\n", buf2);

            //printf("test\n");
            pid_t term_pid;
            int exit_code;
            term_pid = wait(&exit_code);
            //printf("p_id = %d\n", term_pid);
            
        }

        //return 0; //END
    }
    //printf("test\n");
    return 0;
}

//FUNCTIONS
int read_bytes (int fd_read, void* a, int len)
{
    char *s = (char *) a;  //type casting (역참조)
    int i;
    for (i = 0; i < len;)  //len만큼 읽음
    {
        int b;  
        b = read(fd_read, s + i, len - i); 
        if( b == 0)
            break;
        i += b;
    }
    return i; // 정상적으로 읽으면 i == len이 되고 int i를 리턴
}

int write_bytes (int fd_write, void* a, int len) 
{
    char *s = (char *) a; //type casting (역참조)

    int i = 0;
    while( i < len )  //len만큼 보냄
    {
        int b;
        b = write(fd_write, s + i, len - i);
        if ( b == 0)
            break;
        i += b;
    }
    return i; //정상적으로 보내면 i == len이 되고 int i를 리턴
}
