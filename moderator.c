#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_USERS 50
#define MAX_FILTER_WORDS 50
#define MAX_Length 256
#define MAX_GROUPS 30
#define MAX_TEXT_SIZE 256


typedef struct{
    long mtype;
    int timestamp;
    int user;
    char mtext[MAX_TEXT_SIZE];
    int modifyingGroup;
} Message;


char filter_words[MAX_FILTER_WORDS][MAX_Length];
int num_filter_words = 0;

int violation_threshold;
bool banned_users[MAX_GROUPS*10][MAX_USERS*10] = {false};

void to_lowercase(char* str){
    for(int i=0; str[i] != '\0'; i++){
        str[i] = tolower(str[i]);
    }
}


void load_filtered_words(const char* filename){
    FILE* file = fopen(filename,"r");
    if(!file){
        perror("Error opening filtered words file");
        exit(EXIT_FAILURE);
    }


    while(fscanf(file,"%s",filter_words[num_filter_words]) != EOF){
        to_lowercase(filter_words[num_filter_words]);
        num_filter_words++;
        if(num_filter_words >= MAX_FILTER_WORDS) break;
    }
    fclose(file);
}


int count_violations(char* message) {
    int violations = 0;
    char lower_message[MAX_TEXT_SIZE];
    strncpy(lower_message, message, MAX_TEXT_SIZE - 1);
    lower_message[MAX_TEXT_SIZE - 1] = '\0';

    to_lowercase(lower_message);

    bool matched[MAX_FILTER_WORDS] = {false};

    for (int i = 0; i < num_filter_words; i++) {
        if (!matched[i] && strstr(lower_message, filter_words[i]) != NULL) {
            violations++;
            matched[i] = true;
        }
    }
    return violations;
}


int main(int argc, char* argv[]){

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <moderator_key> <violation_threshold>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
   
    int test_case_num = atoi(argv[1]);

    char input_path[256];
    char filter_path[256];

    snprintf(input_path,sizeof(input_path),"testcase_%d/input.txt",test_case_num);
    snprintf(filter_path,sizeof(input_path),"testcase_%d/filtered_words.txt",test_case_num);
 
    int user_violation[MAX_GROUPS*10][MAX_USERS*10];
    for(int i=0; i < MAX_GROUPS*10; i++){
        for(int j=0; j < MAX_USERS*10; j++){
            user_violation[i][j] = 0;
        }
    }

    FILE* file = fopen(input_path,"r");
    if(!file){
        perror("Error opening input.txt");
        exit(EXIT_FAILURE);
    }
   
    int num_groups,validation_key,app_key,moderator_key;
    fscanf(file,"%d %d %d %d %d",&num_groups,&validation_key,&app_key,&moderator_key,&violation_threshold);
    fclose(file);

   
    load_filtered_words(filter_path);

    int msgid = msgget(moderator_key,IPC_CREAT | 0666);
    if(msgid == -1){
        perror("Msgget failed in moderator\n");
        exit(EXIT_FAILURE);
    }
    Message msg;
    while(1){
        if(msgrcv(msgid,&msg,sizeof(msg)-sizeof(long),4,IPC_NOWAIT) != -1){
            if(banned_users[msg.modifyingGroup][msg.user]){
                continue;
            }
            int violations = count_violations(msg.mtext);
             //printf("Recieved from group %d, user %d:%s\n",msg.modifyingGroup,msg.user,msg.mtext);
             //printf("Violations detected: %d\n",violations);
           
            user_violation[msg.modifyingGroup][msg.user] += violations;
            // printf("%d violations committed my %d\n",user_violation[msg.modifyingGroup][msg.user],msg.user);
            if(user_violation[msg.modifyingGroup][msg.user] >= violation_threshold){
                printf("User %d from group %d has been removed due to %d violations. \n",msg.user,msg.modifyingGroup,user_violation[msg.modifyingGroup][msg.user]);
                Message removal_msg;
                if(user_violation[msg.modifyingGroup][msg.user] > violation_threshold){
                    removal_msg.timestamp = 1;
                }
                else{
                    removal_msg.timestamp = 0;
                }
                removal_msg.mtype = msg.modifyingGroup + 10;
                removal_msg.user = msg.user;
                msgsnd(msgid,&removal_msg,sizeof(removal_msg)-sizeof(long),0);
            }
        }
     
    }
    return 0;
}

