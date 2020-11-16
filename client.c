#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include<stdbool.h>

char serverFifo[] = "./server_pipe";
char clientFifoName[] = "./client_";
bool firstWord = true;
char *clientFifo;
int fifoStatus = -1;
int serverFifoStatus = -1;
char client_response[15];
char newWord[100];
char word_typed[10];
bool gameOver = false;
char client_pid[10];

bool signaled = false;
pthread_mutex_t signalLock;
pthread_cond_t signalAvailable;

void * listenToStdin(void * arg);
void * listenToFifo(void * arg);
void handler(int signal) { 
	printf("Server Interrupted. %s\n", clientFifo);
    unlink(clientFifo);
    exit(0);
}

void killer(int signal) {
	printf("Server Killed. %s\n", clientFifo);
    unlink(clientFifo);
	exit(0);
}


int main(int argc, const char * argv[])
{
    signal(SIGINT, handler);
	signal(SIGTERM, killer);

    pthread_t threadToListenFifo;
    pthread_t threadToListenStdin;
    pthread_cond_init(&signalAvailable, NULL);
    pthread_mutex_init(&signalLock, NULL);
    

    // Create Client FIFO
    printf("Creating client's FIFO!\n");

    sprintf(client_pid, "%d", getpid());

    clientFifo = (char *) malloc (sizeof(clientFifoName) + sizeof(client_pid) + 1);
    strcat(clientFifo, clientFifoName);
    strcat(clientFifo, client_pid);

    printf("Client creating a named pipe %s\n", clientFifo);

    //creating fifo
    fifoStatus = mkfifo(clientFifo, 0666);
    if (fifoStatus < 0) {
        printf("could not create a new client Fifo, the Fifo might be existed already.ddd\n");
    
    } else {
        printf("Client FIFO was created successful!\n");
    }

    serverFifoStatus = open(serverFifo, O_WRONLY);
    if (serverFifoStatus < 0) {
        printf("Could not open server fifo.\n");
        return EXIT_FAILURE;
    }

    char pid_length = (char) strlen(client_pid);  
    printf("\nThe Player ID is = %s\n", client_pid);

    write(serverFifoStatus, &pid_length, 1);           
    write(serverFifoStatus, &client_pid, (char) strlen(client_pid)); 

    pthread_create(&threadToListenFifo, NULL, &listenToFifo, (void *) clientFifo);
    pthread_create(&threadToListenStdin, NULL, &listenToStdin, NULL);
    pthread_join(threadToListenFifo, NULL);
    pthread_join(threadToListenStdin, NULL);

    //unlink(serverFifo);
    unlink(clientFifo);
    return EXIT_SUCCESS;
}

//This thread works fine
void * listenToFifo(void * arg)
{
    char* client_pipe = (char *) arg;
    char incomingLength;
    int read_New_Word = open(client_pipe, O_RDONLY);
    if (read_New_Word < 0) {
        printf("Client FIFO could not be opened properly!\n");
        pthread_exit(NULL);
    }
    while(!gameOver)
    {
        fifoStatus = read(read_New_Word, &incomingLength, 1);
        
        if(fifoStatus <= 0)
        {
            printf("\nServer not found.\n Game over.!!!!!!\nPress any key to exit.\n");
            gameOver = true;
            //pthread_exit(NULL);
        }
        else
        {
            read(read_New_Word, &newWord, incomingLength);

            if(strcmp(newWord, "1") == 0)
            {
                printf("\nYou are registered in the game. Let's play\n");
            }
            else
            {
                if(strcmp(newWord, "wins") == 0)
                {
                    printf("\nCongratulations!! You won the game.\n");
                    gameOver = true;
                    //pthread_exit(NULL);
                }
                else if(strcmp(newWord, "lose") == 0)
                {
                    printf("\nSorry!! One of budy won this time !! Good luck for the next time. Press key to exit the game\n");
                    gameOver = true;
                    //pthread_exit(NULL);
                }
                pthread_mutex_lock(&signalLock);
                signaled = true;
                pthread_cond_signal(&signalAvailable);
                pthread_mutex_unlock(&signalLock);
            }
        }
        
    }
    pthread_exit(NULL);
}
void * listenToStdin(void * arg)
{
    char * result;
    while(!gameOver)
    {
        pthread_mutex_lock(&signalLock);
        while(!signaled)
        {
            pthread_cond_wait(&signalAvailable, &signalLock);
        }
        signaled = false;
        pthread_mutex_unlock(&signalLock);

        if(gameOver == true)
        {
            pthread_exit(NULL);
        }
        else
        {
            printf("\nType the word \"%s\":", newWord);
            result = fgets((char*)&word_typed, 10, stdin);
            word_typed[strlen(word_typed) - 1] = '\0';
            
            serverFifoStatus = open(serverFifo, O_WRONLY);
            if (serverFifoStatus <= 0) {
                pthread_exit(NULL);
            }

            else
            {
                char *word_responded = (char *)malloc(strlen(client_pid) + strlen(word_typed) + 2);
                if(strcmp(word_typed, "exit") == 0)
                {
                    write(serverFifoStatus, "\a", 1);
                    gameOver = true;
                    pthread_exit(NULL);
                }
                else
                {
                    printf("You typed: \"%s\"\n", word_typed);
                    strcat(word_responded, client_pid);
                    strcat(word_responded, " ");
                    strcat(word_responded, word_typed);

                    char word_typed_length = (char) strlen(word_responded);
                    int resultofwritting = write(serverFifoStatus, &word_typed_length, 1);           
                    write(serverFifoStatus, word_responded, (char) strlen(word_responded));

                    if(resultofwritting < 0)
                    {
                        printf("Could not write into server fifo.\n");
                    }
                }
                free(word_responded);
            }
            
        }
    }
    pthread_exit(NULL);
}