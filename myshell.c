#define _POSIX_C_SOURCE 199309L
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#define TRUE 1
#define MAX_INPUT 512
#define MAX_TOK 32

// Prototypes to avoid compile warnings
char* strdup(const char *s);
//char* strtok_r(char* str, char *delim, char** saveptr);

char*** parse(char input[]) {
        char*** result = (char***) malloc(MAX_INPUT * sizeof(char**));
        char* str = strdup(input);
        char* ptr1, *ptr2;
        char* row = strtok_r(str, "|", &ptr1);
        int row_idx = 0;

        // Parse until pipe
        while(row != NULL) {
                result[row_idx] = (char**) malloc(MAX_TOK * sizeof(char*));
                char* tok = strtok_r(row, " \t\n", &ptr2);
                int tok_idx = 0;
                char* special = NULL;

                // Parse tokens until pipe
                while (tok != NULL) {
                        if (strcmp(tok, "<")==0) special = "<";
                        else if (strcmp(tok, ">")==0) special = ">";

                        // Handle < >
                        if (special) {
                                row_idx++;
                                result[row_idx] = (char**) malloc(MAX_TOK * sizeof(char*));
                                result[row_idx][0] = special;
                                tok_idx = 1;
                                tok = strtok_r(NULL, " \t\n", &ptr2);
                                if (tok == NULL) break;
                        }

                        result[row_idx][tok_idx] = strdup(tok);
                        tok_idx++;
                        tok = strtok_r(NULL, " \t\n", &ptr2);
                }

                result[row_idx][tok_idx] = NULL;
                row_idx++;
                row = strtok_r(NULL, "|", &ptr1);
        }

        result[row_idx] = NULL;
        free(str);
        return result;
}


// Signal handler for zombie processes
void sigchld_handler(int signum) {
        while (waitpid(-1, NULL, WNOHANG) > 0) { // wait for child processes, no exit status, return 
                printf("\nmy_shell$ ");
        }
}


int main(int argc, char* argv[]) {

         while (TRUE) {

                // Prompt user
                char input[MAX_INPUT];
                if (!(argc > 1 && strcmp(argv[1], "-n")==0)) printf("my_shell$ ");
                if (fgets(input, sizeof(input), stdin)==NULL) {
                        printf("\n");
                        exit(0);
                }

                // Add necessary spaces for parser
                char new_input[3*strlen(input)+1];
                int j=0;
                for (int i=0;i<strlen(input);i++) {
                        if ((input[i]=='|' || input[i]=='<' || input[i]=='>') && input[i+1]!=' ') {
                                new_input[j++] = input[i];
                                new_input[j++] = ' ';
                        }
                        else if (input[i]!=' ' && (input[i+1]=='|' || input[i+1]=='<' || input[i+1]=='<')) {
                                new_input[j++] = input[i];
                                new_input[j++] = ' ';
                        }

                        else if (input[i]=='&' && input[i-1] != ' ') {
                                new_input[j++] = ' ';
                                new_input[j++] = input[i];
                        }

                        else {
                                new_input[j++] = input[i];
                        }

                }
                new_input[j] = '\0';

                // Parse new input and set up
                char*** parsed = parse(new_input);
                int status;
                //fflush(stdout);


                // Signal handler initialization and mapping
                struct sigaction sa;
                sa.sa_handler = sigchld_handler;
                memset(&sa, 0, sizeof(sa));
                //sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
                sigemptyset(&sa.sa_mask);

                if (sigaction(SIGCHLD, &sa, NULL)==-1) {
                        perror("ERROR: Sigaction error");
                        exit(EXIT_FAILURE);
                }



                // Get numebr of commands - separated by only pipes
                int num_commands=0;
                while (parsed[num_commands] && strcmp(parsed[num_commands][0],">")!=0 && strcmp(parsed[num_commands][0], "<")!=0) num_commands++;

                // 2D int array to store file descriptors
                int** pipes = malloc((num_commands-1)*sizeof(int*));
                for (int k=0; k<num_commands-1;k++) {
                        pipes[k] = malloc(2*sizeof(int));
                        if (pipe(pipes[k]) == -1) {
                                perror("ERROR: Pipe set up failed");
                                exit(EXIT_FAILURE);
                        }
                }

                // Set up for redirection
                int fd_in = NULL, fd_out = NULL, num_rows = 0;
                char* file_in= NULL, *file_out = NULL;
                while (parsed[num_rows]) num_rows++;

                // Get file for output redirection (>)
                if (num_rows>1 && (strcmp(parsed[num_rows-1][0], ">")==0)) {
                        file_out = parsed[num_rows-1][1];
                }

                // Get file for input redirection (<)
                if (parsed[1] && (strcmp(parsed[1][0], "<")==0)) {
                        file_in = parsed[1][1];
                }

                // Check for & and remove from parsed array
                int background = 0;
                if (num_rows==1) {
                        for (int p=0; parsed[0][p]!=NULL; p++) {
                                if (strcmp(parsed[0][p], "&")==0) {
                                        background = 1;
                                        parsed[0][p] = NULL;
                                }
                        }
                } else {
                        for (int p=0; parsed[num_rows-1][p]!=NULL; p++) {
                                if (strcmp(parsed[num_rows-1][p], "&")==0) {
                                        background = 1;
                                        parsed[num_rows-1][p] = NULL;
                                }
                        }
                }

                // Forking
                for (int j=0; j<num_commands; j++) {
                        if (fork()==0) {


                                // Handling input redirection (<)
                                if (j==0 && file_in) {
                                        fd_in = open(file_in, O_RDONLY);
                                        dup2(fd_in, STDIN_FILENO);
                                        close(fd_in);
                                }

                                // If not first comand, redirect stdin
                                if (j!=0) {
                                        dup2(pipes[j-1][0], STDIN_FILENO);
                                        close(pipes[j-1][0]);
                                        close(pipes[j-1][1]);
                                }

                                // If not last command, redirect stdout
                                if (j != num_commands-1) {
                                        dup2(pipes[j][1], STDOUT_FILENO);
                                        close(pipes[j][0]);
                                        close(pipes[j][1]);
                                }
                                // Handling output redirection (>)
                                else if (file_out) {
                                        fd_out = open(file_out, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                                        dup2(fd_out, STDOUT_FILENO);
                                        close(fd_out);
                                }

                                // Close other pipes            
                                for (int t=0; t<num_commands-1; t++) {
                                        if (t!=j-1 && t!=j) {
                                                close(pipes[t][0]);
                                                close(pipes[t][1]);
                                        }
                                }

                                // Execute command
                                int err = execvp(parsed[j][0], parsed[j]);
                                if (err == -1) {
                                        perror("ERROR: Execvp failed");
                                        exit(err);
                                }
                        }
                }

                // Close all parent pipes
                for (int w=0; w<num_commands-1;w++) {
                        close(pipes[w][0]);
                        close(pipes[w][1]);
                        free(pipes[w]);
                }

                free(pipes);

                // Wait for child processes to terminate - account for backgrounding
                if (!background) {
                        for (int q=0; q<num_commands; q++) {
                                wait(&status);
                                //waitpid(pid, &status, 0);
                        }
                }
                fflush(stdout);
                input[0] = '\0';
                new_input[0] = '\0';
        }
        return 0;
}