#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<limits.h>
#include<fcntl.h>
#include<string.h>
#include"scanner.h"

#define CMD_MAX 10

extern int yylex();
extern int yyleng;
extern char* yytext;

char curWorkDir[PATH_MAX];

void change_directory(char** filePath);
void preprocess_command(char** command);
int main(){
    //get current working directory
    if(getcwd(curWorkDir,PATH_MAX) == NULL){
        printf("getcwd failed.\n");
        exit(0);
    }
    while(1){
        char* command[CMD_MAX][CMD_MAX];
        int row,col,cur;
        int i;
        //print current working directory
        printf("%s$ ",curWorkDir);
        //read in the comment
        //initial
        for(row =0;row<CMD_MAX;row++)
            for(col=0;col<CMD_MAX;col++)
                command[row][col] = 0;
        row = col =0;
        while((cur = yylex())!=NEXT_LINE){
            if(cur == PIPE){
                row++;
                col = 0;
                continue;
            }
            if( yytext[0] == '\"' &&strlen(yytext)>2 && yytext[strlen(yytext)-1]== '\"'){
                command[row][col] = (char*)calloc(strlen(yytext)-2,sizeof(char));
                strncpy(command[row][col],yytext+1,strlen(yytext)-2);
            }else{
            command[row][col] = (char*)calloc(strlen(yytext)+1,sizeof(char));
            strcpy(command[row][col],yytext);
            }//printf("Command[%d][%d]%d: %s\n",row,col,cur,command[row][col]);
            col++;
        }
        if(row == 0 && col ==0)//without command
            continue;
        else if(row == 0&&col!=0){              //only one line command
            if(!strcmp(command[0][0],"cd")){
                change_directory(command[0]);
            }else{
                pid_t pid;
                if((pid = fork())<0){
                    printf("Fork process error.\n");
                    for(i =0;i<col;i++)
                        free(command[0][i]);
                }
                else if(pid == 0){
                    preprocess_command(command[0]);
                    if(execvp(command[0][0],command[0])==-1){
                        exit(0);
                    }
                }
                else {
                    wait(NULL);
                    for(i =0;i<col;i++)
                        free(command[0][i]);
                }

            }
        }
        else{                                   //need piping
            //create pipe
            int** fd = (int**)calloc(row,sizeof(int*));
            for(i=0;i<row;i++){
                fd[i] = (int*)calloc(2,sizeof(int));
            }
            //fork the process
            for(i=0;i<=row;i++){
                pid_t pid;
                if(i<row){
                    if(pipe(fd[i])==-1){
                        printf("Pipe allocate failed.\n");
                        exit(0);
                    }
                }
                if(i>1){
                    close(fd[i-2][0]);
                    close(fd[i-2][1]);
                }
                //fork process;
                if((pid = fork())<0){
                    printf("fork process failed.\n");
                    break;
                }
                else if(pid == 0){      //child
                    //process current pipeline
                    if(i==0){
                        dup2(fd[i][1],STDOUT_FILENO);
                        close(fd[i][0]);
                    }
                    else if(i == row){
                        dup2(fd[i-1][0],STDIN_FILENO);
                        close(fd[i-1][1]);
                    }
                    else{
                        dup2(fd[i-1][0],STDIN_FILENO);
                        dup2(fd[i][1],STDOUT_FILENO);
                        close(fd[i-1][1]);
                        close(fd[i][0]);
                    }
                    preprocess_command(command[i]);
                    if(!strcmp(command[i][0],"cd")){
                        change_directory(command[i]);
                        exit(0);
                    }
                    else {
                        if(execvp(command[i][0],command[i])==-1){
                            printf("excute error.\n");
                            exit(0);
                        }
                        
                    }
                }
            }
            for(i=0;i<row;i++){
                close(fd[i][0]);
                close(fd[i][1]);
                free(fd[i]);
            }
            free(fd);
            for(i=0;i<=row;i++){
                wait(NULL);
            }
            
        }
    }
    return 0;
}

/*------------------------------------------------*/
int open_file(const char* file,int flag){
    int fd;
    if(file == 0){
        printf("Without file name.\n");
        return -1;
    }
    if((fd=open(file,flag,0777))==-1){
        printf("Open %s failed.\n",file);
        return -1;
    }
    return fd;
}

void preprocess_command(char** command){
    int i;
    int cur;
    for(i=0,cur=0;command[i];i++){
        if(!strcmp(command[i],"<")){
            int fd;
            //to next command
            i++;
            fd = open_file(command[i],O_RDONLY);
            if(fd!=-1){
                dup2(fd,STDIN_FILENO);
            }
            free(command[i-1]);
            free(command[i]);
            command[i-1] = command[i] = 0;
        }
        else if(!strcmp(command[i],"<<")){
            int fd[2];
            char tmp[10000]={0};
            i++;
            while(1){
                char in[100];
                printf("> ");
                scanf("%s",in);
                if(!strcmp(in,command[i])){
                    break;
                }
                strcat(tmp,in);
                strcat(tmp,"\n");
            }
            pipe(fd);
            dup2(fd[0],STDIN_FILENO);
            write(fd[1],(void*)tmp,10000);
            close(fd[1]);
            free(command[i-1]);
            free(command[i]);
            command[i-1] = command[i] = 0;
        }
        else if(!strcmp(command[i],">")||!strcmp(command[i],">>")){
            int fd;
            if(!strcmp(command[i],">")){        //open for >
                i++;
                fd = open_file(command[i],O_WRONLY|O_CREAT|O_TRUNC);
            }
            else {                              //open for >>
                i++;
                fd = open_file(command[i],O_WRONLY|O_CREAT|O_APPEND);
            }
            if(fd!=-1){
                dup2(fd,STDOUT_FILENO);
            }
            free(command[i-1]);
            free(command[i]);
            command[i-1] = command[i] = 0;
        }
        else if(!strcmp(command[i],"2>")||!strcmp(command[i],"2>>")){
            int fd;
            if(!strcmp(command[i],"2>")){        //open for >
                i++;
                fd = open_file(command[i],O_WRONLY|O_CREAT|O_TRUNC);
            }
            else {                              //open for >>
                i++;
                fd = open_file(command[i],O_WRONLY|O_CREAT|O_APPEND);
            }
            if(fd!=-1){
                dup2(fd,STDERR_FILENO);;
            }
            free(command[i-1]);
            free(command[i]);
            command[i-1] = command[i] = 0;
        }
        else {//normal command
            if(i!=cur){
                command[cur] = (char*)calloc(strlen(command[i])+1,sizeof(char));
                strcpy(command[cur],command[i]);
                free(command[i]);
                command[i] = 0;
            }
            cur++;
        }

    }

}
/*------------------------------------------------*/
void change_directory(char** command){
    int fd_in = dup(STDIN_FILENO);
    int fd_out = dup(STDOUT_FILENO);
    int fd_err = dup(STDERR_FILENO);
    //change dir
    preprocess_command(command);
    strcat(curWorkDir,"/");
    strcat(curWorkDir,command[1]);
    if(-1 == chdir(curWorkDir)){
        printf("Path:%s not found.\n",curWorkDir);
    }
    if(getcwd(curWorkDir,PATH_MAX) == NULL){
        printf("getcwd failed.\n");
        exit(0);
    }
    //recovery
    dup2(fd_in,STDIN_FILENO);
    dup2(fd_out,STDOUT_FILENO);
    dup2(fd_err,STDERR_FILENO);
    close(fd_in);
    close(fd_out);
    close(fd_err);
}
