#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>


#define MAX_USERS 50
#define MAX_LENGTH 256
#define MAX_GROUPS 30
#define MAX_TEXT_SIZE 256


typedef struct {
    long mtype;
    int timestamp;
    int user;
    char mtext[256];
    int modifyingGroup;
} Message;


typedef struct {
    int timestamp;
    int user;
    char mtext[MAX_LENGTH];
} ChatMessage;


typedef struct{
    ChatMessage heap[MAX_USERS*5000];
    int size;
}MinHeap;




int group_id,validation_key,moderation_key,app_key,violation_threshold,testcaseNo,GROUP_NO;
int num_users,active_users;
int removed_due_to_violations=0;
bool group_terminated = false;


int user_pipes[MAX_USERS][2];
pid_t user_pids[MAX_USERS];


int user_ids[MAX_USERS*100];
int user_index[MAX_USERS*100];
ChatMessage msg_q[MAX_USERS*200];
bool active[MAX_USERS*100] = {false};


MinHeap* heap;


int message_count = 0;
int extractGrpno(char *s);
void remove_users(int user_id,int violation_flag,int validation_msgid);
void send_across(int msgid, long mtype, int group_id, int user_id, int timestamp, char *text) ;
void initialize_users(char* group_file,int validation_msgid);
void extract_timestamp_message(char* input,int* timestamp,char* message);
void handle_user_message(int user_no,int moderator_msgid,int validation_msgid);
int compare_messages(const void* a,const void* b);
void send_sorted_messages(int validation_msgid);
void process_heap_messages(MinHeap* heap, int moderator_msgid,int validation_msgid);




void swap(ChatMessage* a, ChatMessage* b) {
    ChatMessage temp = *a;
    *a = *b;
    *b = temp;
}


void heapify(MinHeap* heap, int i) {
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;


    if (left < heap->size && heap->heap[left].timestamp < heap->heap[smallest].timestamp)
        smallest = left;
    if (right < heap->size && heap->heap[right].timestamp < heap->heap[smallest].timestamp)
        smallest = right;


    if (smallest != i) {
        swap(&heap->heap[i], &heap->heap[smallest]);
        heapify(heap, smallest);
    }
}


void insertHeap(MinHeap* heap, ChatMessage msg) {
    if (heap->size >= MAX_USERS*1000) {
        printf("Heap is full, cannot insert\n");
        return;
    }


    int i = heap->size;
    heap->heap[i] = msg;
    heap->size++;


    while (i > 0 && heap->heap[(i - 1) / 2].timestamp > heap->heap[i].timestamp) {
        swap(&heap->heap[i], &heap->heap[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
}


ChatMessage extractMin(MinHeap* heap) {
    if (heap->size <= 0) {
        ChatMessage empty = {-1, -1, ""};
        return empty; 
    }


    ChatMessage min = heap->heap[0];
    heap->heap[0] = heap->heap[heap->size - 1];
    heap->size--;


    heapify(heap, 0);
    return min;
}


void buildHeap(MinHeap* heap, ChatMessage messages[], int n) {
    heap->size = n;
    for (int i = 0; i < n; i++) {
        heap->heap[i] = messages[i];
    }


    for (int i = (n / 2) - 1; i >= 0; i--) {
        heapify(heap, i);
    }
}






int extractGrpno(char *s) {
    const char *underscore = strrchr(s, '_');
    const char *dot = strstr(s, ".txt");
    if (!underscore || !dot || underscore > dot) {
        printf("Error: No valid group number in filename: %s\n",s);
        return -1;
    }
    int length = dot - (underscore + 1);
    if (length <= 0 || length > 2) {
        printf("Error: Invalid number format. %d\n", length);
        return -1;
    }
    char numStr[3];
    strncpy(numStr, underscore + 1, length);
    numStr[length] = '\0';
    return atoi(numStr);
}


void send_across(int msgid, long mtype, int group_id, int user_id, int timestamp, char *text) {
    Message msg;
    msg.mtype = mtype;
    msg.timestamp = timestamp;
    msg.modifyingGroup = group_id;
    msg.user = user_id;
    if (text) {
        strncpy(msg.mtext, text, MAX_LENGTH-1);
        msg.mtext[MAX_LENGTH-1] = '\0';
    }
    else{
        msg.mtext[0] = '\0';
    }
    //printf("What is getting sent %d from %d is sending at %d\n",msg.user,msg.modifyingGroup,msg.timestamp);
    if(msgsnd(msgid, &msg, sizeof(Message) - sizeof(long), 0)==-1){
        if(errno == EINVAL){
            printf("WARNING MSGQ DOESNT EXIST for group: %d, user:%d, timestamp:%d\n",msg.modifyingGroup,msg.user,msg.timestamp);
            exit(EXIT_FAILURE);
        }
       
        perror("Error in msgsnd\n");
        exit(EXIT_FAILURE);
    }
};


void initialize_users(char* group_file,int validation_msgid){
    FILE* file= fopen(group_file,"r");
    if(!file){
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }


    fscanf(file,"%d",&num_users);
    active_users = num_users;
    for(int i=0; i < num_users; i++){
       char user_file[MAX_LENGTH];
       fscanf(file,"%s",user_file);


       int m = 0;
       while(user_file[m] != '.'){
        m++;
       }
       user_file[m+4] = '\0';
       int userNo = extractGrpno(user_file);
       
       user_ids[i] = userNo;
       user_index[userNo] = i;
       active[userNo] = true;
       char pathToUser[256];


       snprintf(pathToUser,sizeof(pathToUser),"testcase_%d/%s",testcaseNo,user_file);


       if(pipe(user_pipes[i])==-1){
        perror("Pipe creation failed\n");
        exit(EXIT_FAILURE);
       }
       user_pids[i]=fork();
       sleep(1);
       if(user_pids[i] < 0){
        perror("fork failed");
        exit(EXIT_FAILURE);
       }


       if(user_pids[i] == 0){
        close(user_pipes[i][0]);
       
        // int m = 0;
        // while(pathToUser[m] != '.'){
        //     m++;
        // }
        FILE* user_fp= fopen(pathToUser,"r");


        if(!user_fp){
            perror("Error opening user_file");
            exit(EXIT_FAILURE);
        }


        char buffer[MAX_TEXT_SIZE];
        while(fgets(buffer,MAX_TEXT_SIZE,user_fp)){
            char padded_buffer[MAX_TEXT_SIZE] = {0};
            //printf("%s : %d %d buffer\n",buffer,userNo,GROUP_NO);
            strncpy(padded_buffer,buffer,MAX_TEXT_SIZE-1);
            write(user_pipes[i][1],padded_buffer,MAX_TEXT_SIZE);    
        }


        fclose(user_fp);
        close(user_pipes[i][1]);
        exit(0);
       }


       close(user_pipes[i][1]);
       //printf("User: %d added to group %d\n",userNo,GROUP_NO);
       send_across(validation_msgid,2,GROUP_NO,userNo,0,NULL);


    }
    fclose(file);
}


void extract_timestamp_message(char* input,int* timestamp,char* message){
    char temp[MAX_TEXT_SIZE];
    strncpy(temp,input,MAX_TEXT_SIZE-1);
    temp[MAX_TEXT_SIZE-1] ='\0';


    char* token = strtok(temp," ");
    if(token == NULL) return;


    *timestamp = atoi(token);


    token = strtok(NULL,"\n");
    if(token) {
        strncpy(message,token,MAX_TEXT_SIZE-1);
        message[MAX_TEXT_SIZE-1] = '\0';
    }
    else{
        message[0]='\0';
    }
}


void handle_user_message(int user_id, int moderator_msgid, int validation_msgid) {
    int user_no = user_ids[user_id];
    if(group_terminated) return;


    if (user_pipes[user_id][0] == -1) return;


    char buffer[MAX_TEXT_SIZE];


    while (1) {
        int bytes_read = read(user_pipes[user_id][0], buffer, MAX_TEXT_SIZE);


        if (bytes_read == -1) {
            perror("Error occurred reading bytes");
            return;
        }
        if (bytes_read == 0) {  
            close(user_pipes[user_id][0]);  
            user_pipes[user_id][0] = -1;
            //remove_users(user_id,0,validation_msgid);
            return;
        }


        buffer[bytes_read] = '\0';
        int timestamp;
        char extracted_message[MAX_TEXT_SIZE];


        extract_timestamp_message(buffer, &timestamp, extracted_message);


        bool is_duplicate = false;
        for (int i = 0; i < message_count; i++) {
            if (msg_q[i].timestamp == timestamp && msg_q[i].user == user_no) {
                is_duplicate = true;
                break;
            }
        }
        if (is_duplicate) continue;


       // send_across(moderator_msgid, 4, GROUP_NO, user_no, timestamp, extracted_message);


        // Message msg;
        // if(msgrcv(moderator_msgid,&msg,sizeof(msg)-sizeof(long),GROUP_NO,IPC_NOWAIT) != -1){
        //     remove_users(msg.user,1,validation_msgid);
        //     return;
        // }


           
        msg_q[message_count].timestamp = timestamp;
        msg_q[message_count].user = user_no;
        strncpy(msg_q[message_count].mtext, extracted_message, MAX_TEXT_SIZE - 1);
        msg_q[message_count].mtext[MAX_TEXT_SIZE - 1] = '\0';  
        message_count++;


        ChatMessage new_msg;
        new_msg.timestamp = timestamp;
        new_msg.user = user_no;
        strncpy(new_msg.mtext, extracted_message, MAX_TEXT_SIZE - 1);
       
        insertHeap(heap,new_msg);
    }
}


void remove_users(int user_id,int violation_flag,int validation_msgid){
    printf("User %d removed from group %d\n",user_id,GROUP_NO);
    int ind = -1;
   
    for (int j = 0; j < num_users; j++) {
        if (user_ids[j] == user_id) {
            ind = j;
            break;
        }
    }


    if(ind != -1){
        active[user_id] = false;
        close(user_pipes[ind][0]);
        user_pipes[ind][0] = -1;
        user_ids[ind] = -2;
    }
   
   
    if(violation_flag){
        removed_due_to_violations++;
    }

    active_users--;


    if(active_users < 2){
        send_across(validation_msgid,3,GROUP_NO,removed_due_to_violations,0,NULL);
        printf("No of active users in group %d is %d\n",GROUP_NO,active_users);
        group_terminated = true;
        printf("All users terminated, exiting group %d\n",GROUP_NO);
        exit(0);
    }
}




void process_heap_messages(MinHeap* heape,int moderator_msgid,int validation_msgid){
    bool has_more_messages[MAX_USERS*1000] = {false};


    for(int i=0; i < heape->size; i++){
        has_more_messages[heape->heap[i].user] = true;
    }

    int m=0;
   
    while(heape->size > 0){
        ChatMessage msg = extractMin(heape);
        int user_no = msg.user;
       // printf("user %d indexed %d from group %d is being processed\n",user_ids[user_index[user_no]],user_index[user_no],GROUP_NO);
        if(user_ids[user_index[user_no]] == -2){
            printf("Skipping message from banned user %d\n",user_no);
            continue;
        }
        Message mod_response;
       
        send_across(moderator_msgid,4,GROUP_NO,user_no,msg.timestamp,msg.mtext);
       
        send_across(validation_msgid,GROUP_NO+MAX_GROUPS,GROUP_NO,user_no,msg.timestamp,msg.mtext);

        usleep(1000);
        if(msgrcv(moderator_msgid,&mod_response,sizeof(mod_response)-sizeof(long),GROUP_NO+10,IPC_NOWAIT) != -1){
            printf("User %d banned in group %d while processing message at %d. removing.. \n",user_no,GROUP_NO,msg.timestamp);
            remove_users(mod_response.user,1,validation_msgid);
            continue;
        }
     
        printf("%d did user from group %d count %d\n",user_no,GROUP_NO,msg.timestamp);
        m++;

        //printf("Sent message from user %d at timestamp %d to validation process\n", user_no, msg.timestamp);
   
        has_more_messages[user_no] = false;
        for(int i=0; i < heape->size; i++){
            if(heape->heap[i].user == user_no){
                has_more_messages[user_no] = true;
                break;
            }
        }
        if(!has_more_messages[user_no]){
            remove_users(user_no,0,validation_msgid);
        }
    }
}


int main(int argc, char* argv[]){
    if(argc != 7){
        fprintf(stderr,"usage reductio");
        exit(EXIT_FAILURE);
    }
    group_id = atoi(argv[1]);
    validation_key = atoi(argv[3]);
    moderation_key = atoi(argv[4]);
    violation_threshold = atoi(argv[5]);
    testcaseNo = atoi(argv[6]);
    GROUP_NO = extractGrpno(argv[2]);


    int validation_msgid = msgget(validation_key, IPC_CREAT | 0666);
    int moderator_msgid = msgget(moderation_key, IPC_CREAT | 0666);


    heap = (MinHeap*)malloc(sizeof(MinHeap));
    if(!heap){
        perror("Memory alloc failed for heap");
        exit(EXIT_FAILURE);
    }
    heap->size = 0;


    send_across(validation_msgid,1,GROUP_NO,0,0,NULL);
    initialize_users(argv[2],validation_msgid);


    //printf("%d active users in group %d\n",active_users,GROUP_NO);
    int count = 0;
    while(active_users > 1){
        for(int i=0; i < num_users; i++){
            int user_no = user_ids[i];
            if(user_pipes[i][0] != -1){
                handle_user_message(i,moderator_msgid,validation_msgid);
            }
        }
        process_heap_messages(heap,moderator_msgid,validation_msgid);
        //printf("No of active users in group %d is %d\n",GROUP_NO,active_users);
        count++;
    }
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            printf("Child process %d done\n", pid);
        }
        if(errno == EINVAL){
            printf("WARNING: Message queue deleted..\n");
            exit(EXIT_FAILURE);
        }
}


