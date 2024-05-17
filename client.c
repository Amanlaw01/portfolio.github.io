#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>

#define MAXRCVLEN 200
#define STRLEN 200
#define SERVERPORTNUM 27428
#define HOSTPORTNUM 27429
#define PEERPORTNUM 27429
#define PORT 8080
#define BUFFER_SIZE 1024

char name[STRLEN];
char opponent[STRLEN];
pthread_t tid;
int yours = 0;
int oppos = 0;

void playgame(int socket, char *buffer, int playerID);
void *serverthread(void *parm);
void *peerthread(char *ip);
void *receive_messages(void *arg);
void print_board(char board[3][3]);
void update_board(char board[3][3], int move, char mark);

int serversocket, peersocket, mysocket, consocket;
int ingame, turn;

void playgame(int socket, char *buffer, int playerID) {
    int len, datasocket = socket;

    printf("\nSTARTING GAME\n");

    int i = 0;
    int player = 0;
    int go = 0;
    int row = 0;
    int column = 0;
    int line = 0;
    int winner = 0;
    char board[3][3] = {
        {'1', '2', '3'},
        {'4', '5', '6'},
        {'7', '8', '9'}
    };

    for (i = (0 + turn); i < (9 + turn) && winner == 0; i++) {
        print_board(board);
        printf(" %c | %c | %c\n", board[0][0], board[0][1], board[0][2]);
        printf("---+---+---\n");
        printf(" %c | %c | %c\n", board[1][0], board[1][1], board[1][2]);
        printf("---+---+---\n");
        printf(" %c | %c | %c\n", board[2][0], board[2][1], board[2][2]);

        player = i % 2 + 1;

        do {
            if (player == playerID) {
                printf("\n%s, please enter the number of the square "
                       "where you want to place your %c: ", name, (player == 1) ? 'X' : 'O');
                scanf("%d", &go);
                send(datasocket, &go, sizeof(go), 0);
            } else {
                printf("\nWaiting for %s...\n", opponent);
                len = recv(datasocket, &go, MAXRCVLEN, 0);
                printf("%s chose %d\n", opponent, go);
            }

            row = --go / 3;
            column = go % 3;
        } while (go < 0 || go > 9 || board[row][column] > '9');

        board[row][column] = (player == 1) ? 'X' : 'O';

        if ((board[0][0] == board[1][1] && board[0][0] == board[2][2]) ||
            (board[0][2] == board[1][1] && board[0][2] == board[2][0]))
            winner = player;
        else
            for (line = 0; line <= 2; line++)
                if ((board[line][0] == board[line][1] && board[line][0] == board[line][2]) ||
                    (board[0][line] == board[1][line] && board[0][line] == board[2][line]))
                    winner = player;
    }

    printf("\n\n");
    printf(" %c | %c | %c\n", board[0][0], board[0][1], board[0][2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c\n", board[1][0], board[1][1], board[1][2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c\n", board[2][0], board[2][1], board[2][2]);

    if (winner == 0)
        printf("\nHow boring, it is a draw.\n");
    else if (winner == playerID) {
        printf("\nCongratulations %s, YOU ARE THE WINNER!\n", name);
        yours++;
    } else {
        oppos++;
        printf("\n%s wins this round...\n", opponent);
    }
    printf("\nYour Score: %d", yours);
    printf("\nOpponent's Score: %d", oppos);

    if (turn == 0)
        turn++;
    else
        turn--;

    printf("\nPlay another round? (y/n) ");
    fgetc(stdin);
    fgets(buffer, sizeof buffer, stdin);
    buffer[strlen(buffer) - 1] = '\0';
    printf("\nWating for %s to acknowledge...\n", opponent);

    ingame = 0;
    if (strcmp(buffer, "y") == 0)
        ingame = 1;
    send(datasocket, buffer, strlen(buffer), 0);
    len = recv(datasocket, buffer, MAXRCVLEN, 0);
    buffer[len] = '\0';
    if (strcmp(buffer, "y") != 0 && ingame == 1) {
        printf("\n%s has declined...\n", opponent);
        ingame = 0;
        yours = 0;
        oppos = 0;
    }
}

void *serverthread(void *obj) {
    bool checkflag = false;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    char buffer[MAXRCVLEN + 1];
    int len;

    printf("\nUsage: \n\n join {name}\n list\n invite {player}\n leave\n\n");

    while (1) {
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strlen(buffer) - 1] = '\0';
        char arg1[STRLEN], arg2[STRLEN];
        int n = sscanf(buffer, "%s %s", arg1, arg2);

        if (strcmp(buffer, "list") == 0) {
            send(serversocket, buffer, strlen(buffer), 0);
            len = recv(serversocket, buffer, MAXRCVLEN, 0);
            buffer[len] = '\0';
            printf("\n%s\n", buffer);
        } else if (strcmp(arg1, "join") == 0 && n > 1) {
            if (name == NULL || strlen(name) < 1) {
                send(serversocket, buffer, strlen(buffer), 0);
                len = recv(serversocket, buffer, MAXRCVLEN, 0);
                buffer[len] = '\0';
                printf("\n%s\n", buffer);
                strcpy(name, arg2);
            } else {
                printf("\nYou are already joined by the following name: %s\n", name);
            }
        } else if (strcmp(arg1, "invite") == 0 && n > 1) {
            if (name == NULL || strlen(name) < 1) {
                printf("\nYou must join before inviting...\n");
            } else {
                strcpy(opponent, arg2);
                send(serversocket, buffer, strlen(buffer), 0);
                len = recv(serversocket, buffer, MAXRCVLEN, 0);
                buffer[len] = '\0';
                pthread_create(&tid, NULL, (void *)peerthread, buffer);
                pthread_exit(0);
            }
        } else if (strcmp(buffer, "leave") == 0) {
            name[0] = '\0';
            printf("\nGoodbye!\n");
            close(mysocket);
            close(consocket);
            close(peersocket);
            close(serversocket);
            exit(1);
        } else {
            if (!checkflag) {
                checkflag = true;
            } else {
                printf("\nUsage: \n\n join {name}\n list\n invite {player}\n leave\n\n");
            }
        }
    }
}

void *peerthread(char *ip) {
    char buffer[MAXRCVLEN + 1];
    int len;

    struct sockaddr_in dest;
    peersocket = socket(AF_INET, SOCK_STREAM, 0);
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = htonl(inet_network(ip));
    dest.sin_port = htons(PEERPORTNUM);
    printf("\nSending invite...\n");
    connect(peersocket, (struct sockaddr *)&dest, sizeof(struct sockaddr));

    send(peersocket, name, strlen(name), 0);

    len = recv(peersocket, buffer, MAXRCVLEN, 0);
    buffer[len] = '\0';
    if (strcmp(buffer, "y") == 0) {
        yours = 0;
        oppos = 0;
        printf("\nYour Score: %d", yours);
        printf("\nOpponent's Score: %d", oppos);
        ingame = 1;
    } else {
        yours = 0;
        oppos = 0;
        ingame = 0;
        printf("\n%s declined...\n", opponent);
    }

    turn = 0;
    int playerID = 1;
    while (ingame)
        playgame(peersocket, buffer, playerID);

    yours = 0;
    oppos = 0;
    opponent[0] = '\0';
    close(peersocket);
    pthread_create(&tid, NULL, serverthread, (void *)&serversocket);
    pthread_exit(0);
}

void print_board(char board[3][3]) {
    printf(" %c | %c | %c\n", board[0][0], board[0][1], board[0][2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c\n", board[1][0], board[1][1], board[1][2]);
    printf("---+---+---\n");
    printf(" %c | %c | %c\n", board[2][0], board[2][1], board[2][2]);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in addr;
    struct sockaddr_in client;
    char buffer[BUFFER_SIZE];
    socklen_t client_len = sizeof(client);
    int len;

    serversocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serversocket < 0) {
        perror("socket() failed");
        exit(1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(serversocket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind() failed");
        close(serversocket);
        exit(1);
    }

    if (listen(serversocket, 10) < 0) {
        perror("listen() failed");
        close(serversocket);
        exit(1);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        consocket = accept(serversocket, (struct sockaddr *)&client, &client_len);
        if (consocket < 0) {
            perror("accept() failed");
            continue;
        }

        len = recv(consocket, buffer, BUFFER_SIZE, 0);
        buffer[len] = '\0';
        printf("Received: %s\n", buffer);

        pthread_create(&tid, NULL, serverthread, (void *)&consocket);
        pthread_detach(tid);
    }

    close(serversocket);
    return 0;
}

