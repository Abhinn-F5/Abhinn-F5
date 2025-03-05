#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_GROUPS 30
#define MAX_Length 256
#define MAX_CONCURRENT_GROUPS 3

typedef struct{
    long mtype;
    int group_id;
}GroupMsg;


int main(int argc,char *argv[]){
    if(argc != 2){
        printf("Not enough command line args");
        exit(EXIT_FAILURE);
    }
    int testcase_no = atoi(argv[1]);
    char input_path[256];
    char filter_path[256];

    snprintf(input_path,sizeof(input_path),"testcase_%d/input.txt",testcase_no);

    FILE *file = fopen(input_path,"r");
    if(!file){
        perror("Error opening file\n");
        exit(EXIT_FAILURE);
    }
    int num_groups,validation_key,app_key,moderator_key,violation_max;


    fscanf(file,"%d %d %d %d %d",&num_groups,&validation_key,&app_key,&moderator_key,&violation_max);
    char groupPaths[MAX_GROUPS][MAX_Length];
    for(int i=0; i < num_groups; i++){
        fscanf(file,"%s",groupPaths[i]);
    }
    fclose(file);
    int app_msgid = msgget(app_key,IPC_CREAT | 0666);
    if(app_msgid == -1){
        perror("msgget failed in app");
        exit(EXIT_FAILURE);
    }
   
    pid_t group_pids[MAX_GROUPS];
    int active_grps = 0;
    for(int i=0; i < num_groups; i++){
       
        group_pids[i] = fork();

        if(group_pids[i] < 0){
            perror("fork failed");
            exit(EXIT_FAILURE);
        }
        if(group_pids[i]== 0){
            char grp_id_str[10];
            char val_key_str[10];
            char mod_key_str[10];
            char violation_m[10];
            char tc_no[10];
            sprintf(grp_id_str,"%d",i);
            sprintf(val_key_str,"%d",validation_key);
            sprintf(mod_key_str,"%d",moderator_key);
            sprintf(violation_m,"%d",violation_max);
            sprintf(tc_no,"%d",testcase_no);
            char groupP[256];
            snprintf(groupP,sizeof(groupP),"testcase_%d/%s",testcase_no,groupPaths[i]);
            execl("./groups.out","./groups.out", grp_id_str, groupP, val_key_str, mod_key_str,violation_m,tc_no,NULL);
            perror("execl failed");
            exit(EXIT_FAILURE);
        }
        active_grps++;
    }
    int activeGrps = num_groups;
    while(activeGrps > 0){
        GroupMsg msg;
        if(msgrcv(app_msgid,&msg,sizeof(msg)-sizeof(long),0,0) != -1){
            printf("All users terminated. Exiting group process %d \n",msg.group_id);
            activeGrps--;
            int status;
            waitpid(group_pids[msg.group_id],&status,0);
        }


    }
    return 0;
}


