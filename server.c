#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include "words.h"

#define NoOfClients 2

void sendWordsToPlay();
void writeToClientPipe(char * wordToWrite, char * pid);
void notifyAllPlayers();
int getRandomNumber();

bool gameOver = false;

bool rgistrationDone = true;

int clientExited = 0;

int serverFifoOpened;

typedef struct clientInfo
{
    char *pid;
    char *word;
    int correctSpells;
} Client_Info;

Client_Info *clients[NoOfClients];

char serverFifo[] = "./server_pipe";
char clientFifoName[] = "./client_";
char *clientFifo;
int fifoStatus = -1;
int serverFifoStatus = -1;
char client_response[15];
void handler(int signal) { 
	printf("Server Interrupted.\n");
    unlink(serverFifo);
    exit(0);
}

void killer(int signal) {
	printf("Server Killed.\n");
	unlink(serverFifo);
	exit(0);
}


int main(int argc, const char *argv[])
{
    signal(SIGINT, handler);
	signal(SIGTERM, killer);


    int i;
    char client_pid[10];

    printf("Server is listening for input in the player response fifo\n");

    for (i = 0; i < NoOfClients; i++)
    {
        clients[i] = malloc(sizeof(Client_Info));

        if (clients[i] != NULL)
        {
            clients[i]->pid = (char *)malloc(sizeof(char) * 6);
            clients[i]->word = (char *)malloc(sizeof(char) * 6);
            clients[i]->correctSpells = 0;
        }
        else
        {
            printf("There is not enough memory. \n");
            return EXIT_FAILURE;
        }
    }

    serverFifoStatus = mkfifo(serverFifo, 0666);
    if (serverFifoStatus < 0)
    {
        printf("Failed to create new fifo, the FIFO might already be existed! \n");
    }
    serverFifoOpened = open(serverFifo, O_RDONLY);
    if (serverFifoOpened < 0)
    {
        printf("Player response FIFO is unavailable for read access\n");
        return EXIT_FAILURE;
    }
    else
    {
        printf("Player Response FIFO Opened \n \n");
    }

    char length;

    int countClients = 0;
    while (countClients < NoOfClients)
    {
        printf("\nWatiting for player for their pid.\n");
        int reads = read(serverFifoOpened, &length, 1);
        if(reads <= 0)
        {
            perror("ERROR: Error reading from.\n");
        }
        else
        {
            read(serverFifoOpened, &client_response, length);
            if (&client_response[0] != NULL)
            {
                client_response[(int)length] = '\0';
                clients[countClients]->pid = strdup(client_response);
                printf("Registering the Player: %s\n", clients[countClients]->pid);
            }
            else
            {
                printf("Not enought players registered. Sorry! \n");
                rgistrationDone = false;
                close(reads);
            }
        }
        
        countClients++;
    }
    printf("All Players are registered correctly!\n");
    sendWordsToPlay();
    printf("Game over...\n");
    unlink(serverFifo);
    return EXIT_SUCCESS;
}

void sendWordsToPlay()
{
    int i;
    int clientFifoOpened;
    char *wordToSpell;

    for (i = 0; i < NoOfClients; i++)
    {
        char welcomeMessage[200];
        wordToSpell = words[getRandomNumber()];
        clients[i]->word = wordToSpell;
        //strcat(welcomeMessage, "You are registered to the game");
        writeToClientPipe("1", clients[i]->pid);
        writeToClientPipe(wordToSpell, clients[i]->pid);
    }

    while (!gameOver)
    {
        char* wordTyped;
        char* currentPID;
        bool foundWinner = false;
        int pidIndex = 0;

        char length;
        printf("\nWaiting for player to type the word\n");
        int reads = read(serverFifoOpened, &length, 1);

        if(reads <= 0)
        {
            printf("Error reading Server. Might be closed.\n");
            gameOver = true;
        }
        else
        {
            if ('\a' == length)
            {
                clientExited++;
            }
            if(NoOfClients - clientExited == 0)
            {
                printf("All players exited the game.\n");
                gameOver = true;
            }
            else
            {
                read(serverFifoOpened, client_response, length);
                client_response[strlen(client_response)] = '\0';
                currentPID = strtok(client_response, " ");
                wordTyped  = strtok(NULL, " ");
                printf("Player \"%s\" typed: %s\n", currentPID, wordTyped);

                for(i = 0; i < NoOfClients; i++)
                {
                    if(strcmp(currentPID, clients[i]->pid) == 0)
                    {
                        pidIndex = i; 
                        if(strcmp(wordTyped, clients[i]->word) == 0)
                        {
                            (clients[i]->correctSpells)++;
                        }

                        if(clients[i]->correctSpells >= 10)
                        {
                            foundWinner = true;
                            gameOver = true;
                        }
                    }
                }

                if(foundWinner == true)
                {
                    notifyAllPlayers();
                }
                else
                {
                    wordToSpell =  words[getRandomNumber()];
                    clients[pidIndex]->word = wordToSpell;
                    writeToClientPipe(wordToSpell, currentPID);
                } 
            }
        }  
    }
}
void notifyAllPlayers()
{
    int i;
    for(i = 0; i < NoOfClients; i++)
    {
        if(clients[i]->correctSpells >= 10)
        {
            writeToClientPipe("wins", clients[i]->pid);
        }
        else
        {
            writeToClientPipe("lose", clients[i]->pid);
        }
    }

}
void writeToClientPipe(char * wordToWrite, char * pid)
{
    clientFifo = (char *)malloc(sizeof(clientFifoName) + sizeof(pid) + 1);
    strcat(clientFifo, clientFifoName);
    strcat(clientFifo, pid);

    int clientFifoOpened = open(clientFifo, O_WRONLY);
    if (clientFifoOpened <= 0)
    {
        printf("Client FIFO is unavailable for write access\n");
        return;
    }
    else
    {
        char length;
        length = (char)strlen(wordToWrite);
        write(clientFifoOpened, &length, 1);
        write(clientFifoOpened, wordToWrite, length);
    }
    free(clientFifo);
}

int getRandomNumber()
{
    int lengthOfWords = sizeof(words) / sizeof(char *);
    int randomNumber = 0;

    srand(time(NULL));
    int r = rand();

    randomNumber = r % lengthOfWords;
    //printf("The random number is: %d", randomNumber);

    return randomNumber;
}