#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/wait.h>

#define MAX_LINE		80 /* 80 chars per line, per command */
#define READ_END		0
#define WRITE_END		1

int main(void)
{
	char *args[MAX_LINE/2 + 1];	/* command line (of 80) has max of 40 arguments */
    int should_run = 1; //indicates that exit is not entered
	int buffer_size = 0;
	int args_size = 0;
	char * buffer; 
	buffer = (char *)malloc((MAX_LINE+1)*sizeof(char));
	int saved = 0;
	int concurrent;

    while (should_run){
        printf("osh>");
        fflush(stdout);
       
        char * s; //declaration of string that is inputted
        s = (char *)malloc((MAX_LINE+1)*sizeof(char));


        ssize_t buf_size = MAX_LINE;
        getline(&s, &buf_size, stdin); //getting input
        
        
        //checking if it's !! that is history
        if (strcmp(s,"!!\n")==0){
            if (saved == 1) //saved is 1 when there is a command in the buffer
            {
                strcpy(s, buffer); //s gets updated with the content of the buffer
                printf("%s\n", s); //the command is printed
                fflush(stdout);
            }
            else
                printf("No commands in history\n"); //if the buffer is empty
        }
        else {
            strcpy(buffer,s); //if not !! then copy s into buffer to create history
            saved = 1; //assign 1 to saved
        }
        
        //String parsing
        char delim[] = " \n\a\r\t";
        char *ptr = strtok(s, delim);
        
        
        //if input is only the delimiting characters
        while (ptr == NULL){
            printf("osh>");
            fflush(stdout);
            getline(&s, &buf_size, stdin);
            
            if (strcmp(s,"!!\n")==0){
                if (saved == 1)    {
                    strcpy(s, buffer);
                    printf("%s\n", s);
                    fflush(stdout);
                }
                else
                    printf("No commands in history\n");
            } else {
                strcpy(buffer,s);
                saved = 1;
            }
            ptr = strtok(s, delim);
        }
        
        
        int i = 0;
        while (ptr != NULL){
            args[i] = ptr;
            ptr = strtok(NULL, delim);
            i = i + 1;
        }
        
        //checking if parent will run concurrent with child (no waiting)
        if (strcmp(args[i-1],"&") == 0){
            concurrent = 0;
            i = i - 1; //to set to NULL
        } else
            concurrent = 1;

        args[i] = NULL; //set last element to null
        args_size = i;

        if (strcmp(args[0],"exit") == 0) //if exit is inputted, terminate
        {
            should_run=0;
            return 0;
        }
        
        //creating child
        pid_t pid;
        pid = fork();
        
        if (pid <0) //if fails to create child
        {
            fprintf(stderr, "Fork Failed");
            return 1;
        }
        else if (pid == 0) //if child is created
        {
            fflush(stdout);
            int j = 0;
            while (j < args_size)
            {
                int comp = args_size - 1;
                if (strcmp (args[j], "<")==0) //checking for redirection
                {
                    fflush(stdout);
                    if(j == comp) //if redirection character is last character (validation)
                    {
                        printf("Syntax error\n");
                        fflush(stdout);
                        args[0] = NULL;
                    }
                    else if( (strcmp(args[j+1], ">") == 0) | (strcmp(args[j+1], "<") == 0)) //if two redicretion characters followed by each other (validation)
                    {
                        printf("Syntax error\n");
                        fflush(stdout);
                        args[0] = NULL;
                    }
                    else //redirecting input to file
                    {
                        int fd = open(args[j + 1], O_RDONLY);
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                        args[j] = NULL;
                        args[j + 1] = NULL;
                    }
                    fflush(stdout);
                    j = 100;
                }
                else if (strcmp(args[j], ">")==0) //checking for redirection
                {
                    if (j == comp) //if redirection character is last character (validation)
                    {
                        printf("Syntax error\n");
                        fflush(stdout);
                        args[0] = NULL;
                    }
                    if((strcmp(args[j+1], "<") == 0) |(strcmp(args[j+1], ">") == 0)) //if two redicretion characters followed by each other (validation)
                    {
                        printf("Syntax error\n");
                        fflush(stdout);
                        args[0] = NULL;
                    }
                    else //redirecting output to file
                    {
                        int fd = open(args[j + 1], O_WRONLY | O_CREAT, 0644);
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                        args[j] = NULL;
                        args[j + 1] = NULL;
                    }
                    j = 100;
                }
                else if (strcmp(args[j], "|")==0) //checking for pipe
                {
                    args[j] = NULL;
                    int fd[2];
                    if (pipe(fd) == -1)
                    {
                        fprintf(stderr, "Pipe failed");
                    }
                    
                    //creating child
                    pid_t pid1 = fork();
                    char *args2[MAX_LINE/2 + 1];
                    int x = 0;
                    
                    //creating args2(executed by grandchild) and args(executed by child)
                    while (x < j)
                    {
                        args2[x] = args[x];
                        x = x + 1;
                    }
                    args2[j] = NULL;
                    x = j+1;
                    while(x <= args_size){
                        args[x-j-1] = args[x];
                        x = x + 1;
                    }
                    
                    //grandchild failed to be created
                    if (pid1 < 0)
                    {
                        fprintf(stderr, "Fork Failed");
                        return 1;
                    }
                    else if (pid1 == 0) //grandchild was created
                    {
                        close(fd[READ_END]); //close read end of pipe
                        dup2(fd[WRITE_END], STDOUT_FILENO); //write to write end of pipe
                        close(fd[WRITE_END]);
                        if (execvp(args2[0], args2) == -1){
                            printf("Error executing instruction\n");
                            return 1;
                        }
                    }
                    else
                    {
                        waitpid(pid1, NULL, 0); //child wait for grandchild
                        close(fd[WRITE_END]); //close write end of pipe
                        dup2(fd[READ_END], STDIN_FILENO); //read from read end if pipe
                        close(fd[READ_END]);

                    }
                    j = 100;
                }
                j = j + 1;
            }
            

            if (execvp(args[0], args) == -1) //child executes
            {
                printf("Error executing instruction\n");
                return 1;
            }
        }
        else if (concurrent == 1) //if parent and concurrent = 1 wait for child
        {
            waitpid(pid, NULL, 0);
        }
    }
	return 0;
}
