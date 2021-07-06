/* #################################################### */
/*                    21600786                          */
/*                      홍승훈                            */
/* #################################################### */
/*           ./pfind [<option>] <dir> [<keyword>]       */
/* [<option>] -p<N>(work process갯수지정)  -c(대소문자 무시) -a(절대경로인쇄) */
/* ./pfind -p<8> -c -a /home/s21600786/totaldir keyword  */  //example
/* ./manager p4 -c /home/s21600786/OSHW3 Hello name */

#define _CRT_SECURE_NO_WARNINGS
#define TRUE 1
#define FALSE 0

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

//강제종료(ctrl+C)가 넘어오면 manager가 worker들을 kill
//kill signal이 넘어오면 pipe로 현재 가진정보를 모두 리턴해야함.
//모든 결과들을 hadnler가 책임지고 출력한 후 manager process가 종료되야함. 
void handler(int sig);

void Eliminate(char *str, char ch);

int pipes_1[2];

struct work {
    char dir[300];
    char redir[300];
    pid_t p_num;
    int check;
    int temp;
    size_t length;
};

int read_bytes(int fd, void* a, int len);
int write_bytes(int fd, void* a, int len);
int pipes_1[2];



int main(int argc, char **argv) { //START
    
    struct timespec start1, end1;
    double times;
    clock_gettime(CLOCK_MONOTONIC, &start1);
    int opt;      //getopt variables
    int case_check = 0;
    int Absolute = 0;
    extern char *optarg;
    extern int optind;
    char keyword[100];
    char option[30];
    char *directory;
    int procNum = 2; //default proc value
    char *wheretree = "/usr/bin/tree";
    char *wherework = "/home/s21600786/OSHW3/worker";
    char subdir[500][300];
    int counts=0;
    

    //get optional arguments from command line
    while ((opt = getopt(argc, argv, "p:ca")) != -1)
    {
        switch (opt)
        {
        case 'p':
            procNum = atoi(optarg);
            if( procNum > 7 || procNum < 1 )
            {
                perror("Process number error\n");
            }
            printf("Using %d Process\n", procNum);
            break;
        case 'c':
            //case sensitive disabled
            case_check = 1;
            printf("Case Insensitive\n");
            break;
        case 'a':
            Absolute = 1;
            printf("Absolute Path\n");
            break;
        case '?':
            printf("Non-option argument");
            exit(1);
        default:
            printf("error in getopt from default case");
            break;
        }
    }
    if((directory = argv[optind]) == NULL)
    {
        printf("NO DIRECTORY\n");
        return -1;
    }
    if(argc - optind == 1)
    {
        printf("NO KEYWORD \n");
        return -1;
    }
    optind++; //dir와 keyword가 둘다 존재할 때. -> index를 2개 up
    sprintf(keyword, "%s", argv[optind]);
    for(int i = 1; i < argc - optind; i++)
        sprintf(keyword, "%s %s", keyword, argv[i + optind]);
    //option 분류
    if(case_check == 1 && Absolute ==1)
        strcpy(option, "AC");
    else if(case_check == 1)
        strcpy(option, "C");
    else if(Absolute == 1)
        strcpy(option, "A");
    else
        strcpy(option, "NONE");
    
    printf("keyword = %s\n", keyword);
    //Directory 받아오기
    pid_t tree;
    if (pipe(pipes_1) != 0)
    {
        perror("Error");
        exit(1);
    }
    printf("input directory %s\n", directory);
    tree = fork();
    if(tree == 0) //child  -> execl tree -> stdout(dup2) -> manager가 받아와
    {
        dup2(pipes_1[1], 1); /* standard output*/
        execl(wheretree, "tree", directory, "-d", "-fi", (char *)NULL);
    }
    else
    {
        char buf[1000001];
        ssize_t s;
        close(pipes_1[1]); /* close standard output*/
        while ((s = read(pipes_1[0], buf, 1000000)) > 0)
            buf[s + 1] = 0x0;
        char *ptr = strtok(buf, "\n");
        while(ptr != NULL)
        {
            strcpy(subdir[counts], ptr);
            ptr = strtok(NULL, "\n");
            counts ++;  
        }
        wait(NULL);
        //모든 subdir를 subdir에 counts-1만큼 가져옴
    }
    char exx[300];
    for(int i=1; i<=counts-1; i++){
        for(int j=0; j<counts-i; j++)
            {
                if( strlen(subdir[j]) > strlen(subdir[j+1]) )
                {
                    strcpy(exx, subdir[j+1]);
                    strcpy(subdir[j+1], subdir[j]);
                    strcpy(subdir[j], exx);
                }
            }
    }
    printf("counts = %d\n", counts-2);
    // 구현 -> 일단 subdir를 기준으로 key를 잡는다
    //processor 갯수를 받아서 해당 갯수만큼 fork()가 필요. ->pid값을 기준으로 
    struct work work_arr[counts-1];
    for(int i=0; i<counts-1; i++)
    {
        strcpy(work_arr[i].dir, subdir[i]);
        work_arr[i].check = 0;
        printf("%s\n", work_arr[i].dir);
    }
    pid_t work_pid[procNum];
    int condition = 1;
    
    for(int i=0; i<procNum; i++)
    {
        work_pid[i] = fork();
        if (work_pid[i] == 0)
        {
            printf("Start %d's worker\n", i);
            execl(wherework, "worker", keyword, option, (char *)NULL);
        }
    }
    sleep(2);

    if(mkfifo("manager_to_worker", 0666))  //manager to worker 채널생성
    {
        if (errno != EEXIST)
        {
            perror("fail to open file: ");
            exit(1);
        }
    }
    int fd_manager_to_work = open("manager_to_worker", O_WRONLY | O_SYNC);
    printf("=========== manager to worker channel open ===========\n");
    usleep(5000);
    int tp = 0;
    while (condition) // 일단 모든 dirtory를 보내!
    {
        char s[300];
        if(tp < counts-1)
        {
            usleep(5000);
            tp++;
            printf("%d's transfer------ \n",tp);
            size_t len = strlen(work_arr[tp].dir);
            if (write_bytes(fd_manager_to_work, &len, sizeof(len)) != sizeof(len))
                break;
            if(write_bytes(fd_manager_to_work, work_arr[tp].dir,len) != len)
                break;
            len = 0;
            
        }
        else
        {   //모든 directory들을 전송하면 fork된 프로세스 갯수만큼 특수문자조합(#DONE#)을 보내
            //더이상 JOB이 없음을 알린다.
            char done[300] = "##########################################################################################################################################################################################################################################################################################################################################";
            size_t done_len = 300;
            //printf("done_len = %d\n", done_len);
            for(int z=0; z<procNum; z++)
            {
            if(write_bytes(fd_manager_to_work, &done_len, sizeof(done_len)) != sizeof(done_len))
                break;
            if(write_bytes(fd_manager_to_work, done, done_len) != done_len)
                break;
            }
            done_len = 0;
            condition = 0; // 모든 job들을 send하면 while문을 종료시킴.
        }

    }
    close(fd_manager_to_work);
    if(mkfifo("worker_to_manager", 0666))
    {
        if(errno != EEXIST)
        {
            perror("fail to open fifo");
            exit(1);
        }
    }
    int fd_worker_to_manager = open("worker_to_manager", O_RDONLY | O_SYNC);

    char reading[5500000];
    size_t len, bs;
    while(1)
    {
        flock(fd_worker_to_manager, LOCK_EX);
        printf(" ========== manager read ========== \n");
        if (read_bytes(fd_worker_to_manager, &len, sizeof(len)) != sizeof(len))
        {
            flock(fd_worker_to_manager, LOCK_UN);
            printf("Wrong length\n");
        }
        bs = read_bytes(fd_worker_to_manager, reading, len);
        flock(fd_worker_to_manager, LOCK_UN);
        for (int i = 0; i < bs; i++)
            putchar(reading[i]);
        reading[bs] = 0;
        if (bs != len)
            printf("reading len wrong");
        len = 0;
        bs = 0;

        break;
    }
    printf("\n");
    unlink("worker_to_manager");
    unlink("manager_to_worker");

    pid_t killing;
    killing = fork();
    if(killing == 0)
    {
        execl("/usr/bin/killall", "killall", "-9", "-q", "worker", (char *)NULL);
    }
    else
    {

        wait(NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end1);
    times = ((end1.tv_sec - start1.tv_sec) * 1.0e3) + ((end1.tv_nsec - start1.tv_nsec) / 1.0e6);
    printf("Execution time: %lf (msec)\n", times);
    printf("====================== ALL DONE ======================\n\n");
    return 0; //END
}


//FUNCTIONS 
int read_bytes (int fd, void* a, int len)
{
    char *s = (char *) a;  //type casting (역참조)
    int i;
    for (i = 0; i < len;)  //len만큼 읽음
    {
        int b;  
        b = read(fd, s + i, len - i); 
        if( b == 0)
            break;
        i += b;
    }
    return i; // 정상적으로 읽으면 i == len이 되고 int i를 리턴
}

int write_bytes (int fd, void* a, int len) 
{
    char *s = (char *) a; //type casting (역참조)

    int i = 0;
    while( i < len )  //len만큼 보냄
    {
        int b;
        b = write(fd, s + i, len - i);
        if ( b == 0)
            break;
        i += b;
    }
    return i; //정상적으로 보내면 i == len이 되고 int i를 리턴
}