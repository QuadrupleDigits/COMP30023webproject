/**
 * Image_Tagger V2, adjusted from COMP30023 workshop 6: http-server.c
 * As presented for comp30023-2019-project-1.
 * Work by Nicholas Gurban, Student ID 757235.
 */
static char const * const VERSION_NAME = "TAGGER-V6";


// libraries:
#include <errno.h>        // used for particular error messages and associated numbers
#include <stdbool.h>      // extends [bool true/false] to include [int 1/0]
#include <stdio.h>        // file information and others
#include <stdlib.h>       // basic utility
#include <string.h>       // string utility

#include <arpa/inet.h>    // types in_port_t and in_addr_t
#include <fcntl.h>        // file control options
#include <netdb.h>        // network database operations
#include <netinet/in.h>   // internet address family. kinda have to have.
#include <strings.h>      // string operations. possibly legacy but include anyway
#include <sys/select.h>   // select types, also time
#include <sys/sendfile.h> // for sending files? documentation not found.
#include <sys/socket.h>   // internet protocol family. Must have for socket programming, which this is.
#include <sys/stat.h>     // for stat(), to get file status. very useful.
#include <sys/types.h>    // data types. who knows exactly how far this rabbit hole goes.
#include <unistd.h>       // standard symbolic constants and types. also auto-include.

// constants
static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";
static char const * const HTTP_400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_400_LENGTH = 47;
static char const * const HTTP_404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_404_LENGTH = 45;

// html filepaths
static char const * const INTRO = "1_intro.html";
static char const * const START = "2_start.html";
static char const * const FIRST_TURN = "3_first_turn.html";
static char const * const ACCEPTED = "4_accepted.html";
static char const * const DISCARDED = "5_discarded.html";
static char const * const ENDGAME = "6_endgame.html";
static char const * const GAMEOVER = "7_gameover.html";

// keeping track of player info
int maxfd;
struct PlayerInfo {
    int otherSock;
    int gameState;
    int numWords;
    char wordsUsed[128][128];
};
struct PlayerInfo players[128];
/** Probably don't need more than 10 players, but why not.
 * And I know global variables are frowned upon but they make it so much easier.
 */

// represents the types of method
typedef enum
{
    GET,
    POST,
    UNKNOWN
} METHOD;
// for error checking
bool debug = false;

// serve_html: processes all the easy HTML file transfers.
static bool serve_html(int sockfd, const char * fileID){
    if(debug){printf("Page %s is being served to %d.\n", fileID, sockfd);}
    char buff[2049];
    struct stat st;
    stat(fileID, &st);
    int n = sprintf(buff, HTTP_200_FORMAT, st.st_size);
    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return false;
    }
    // send the file
    int filefd = open(fileID, O_RDONLY);
    do
    {
        n = sendfile(sockfd, filefd, NULL, 2048);
    }
    while (n > 0);
    if (n < 0)
    {
        perror("sendfile");
        close(filefd);
        return false;
    }
    close(filefd);
    return true;
}

// quitter code. Part is for the eventuality of cookies.
static void player_quit(int sockfd){
    if(debug){printf("Game Over for %d.\n", sockfd);}
    serve_html(sockfd, GAMEOVER);
    players[sockfd].gameState = 1;
    if ( players[sockfd].otherSock >= 0 ){
        players[players[sockfd].otherSock].gameState = -1;
        players[sockfd].otherSock = -1;
    }
    return;
}

// and another one for winning
static void win_game(int sockfd){
    if(debug){printf("Game won for %d.\n", sockfd);}
    serve_html(sockfd, ENDGAME);
    players[sockfd].gameState = 6;
    if ( players[sockfd].otherSock >= 0 ){
        players[players[sockfd].otherSock].gameState = 6;
        players[sockfd].otherSock = -1;
    }
    return;
}

// finds a friend to play against. Designed for multiple simultaneous games I guess.
static bool find_a_friend(int sockfd){
    if ( players[sockfd].otherSock >= 0 ){
        if(debug){printf("%d is playing with %d.\n", sockfd, players[sockfd].otherSock);}
        return true;
    } else {
        if(debug){printf("%d is searching for other players.\n", sockfd);}
        for (int i = 0; i < maxfd; ++i){
            if ( players[i].otherSock < 0 && 1 < players[i].gameState ){
                players[i].otherSock = sockfd;
                players[sockfd].otherSock = i;
                if(debug){printf("%d is now playing with %d.\n", sockfd, players[sockfd].otherSock);}
                return true;
            }
        }
    }
    if(debug){printf("No other suitable players found.\n");}
    return false;
}

// works out friend status
static bool friend_status(int sockfd){
    int otherPlayer = players[sockfd].otherSock;
    if(debug){printf("%d requesting readiness of %d\n", sockfd, otherPlayer);}
    if ( players[otherPlayer].gameState > 3 ){
        if(debug){printf("Both players ready.");}
        return true;
    } else if ( players[otherPlayer].gameState == -1 ){
        players[sockfd].gameState = -1;
    }
    if(debug){printf("%d not ready.\n", otherPlayer);}
    return false;
}

//
static bool guess_protocol(int sockfd, char * guess){
    int otherPlayer = players[sockfd].otherSock;
    if(debug){printf("%d guesses: %s\n", sockfd, guess);}
    for (int i = 0; i < players[otherPlayer].numWords; ++i){
        if(debug){printf("Searching past guesses of %d: %s\n", otherPlayer, players[otherPlayer].wordsUsed[i] );}
        if ( !strcmp(players[otherPlayer].wordsUsed[i], guess) ){
            if(debug){printf("Match detected, ending game.\n");}
            return true;
        }
    }
    strcpy( players[sockfd].wordsUsed[players[sockfd].numWords], guess );
    players[sockfd].numWords += 1;
    if(debug){printf("No matches. %d has taken %d guesses.\n", sockfd, players[sockfd].numWords);}
    return false;
}


static bool handle_http_request(int sockfd) {

    // just to keep track of things
    if(debug){printf("Action taken on socket %d.\n", sockfd);}

    // try to read the request
    char buff[2049];
    int n = read(sockfd, buff, 2049);
    if (n <= 0) {
        if (n < 0) {
            perror("read");
        } else {
            printf("socket %d close the connection\n", sockfd);
        }
        return false;
    }

    // terminate the string
    buff[n] = 0;
    char * curr = buff;

    // parse the method
    METHOD method = UNKNOWN;
    if (strncmp(curr, "GET ", 4) == 0)
    {
        curr += 4;
        method = GET;
    }
    else if (strncmp(curr, "POST ", 5) == 0)
    {
        curr += 5;
        method = POST;
    }
    else if (write(sockfd, HTTP_400, HTTP_400_LENGTH) < 0)
    {
        perror("write");
        return false;
    }

    // sanitise the URI
    while (*curr == '.' || *curr == '/'){
        ++curr;
    }
    // assume the only valid request URI is "/" but it can be modified to accept more files
    /**
     * Added '?' to valid requests.
     * This took me way too long to work out.
     */
    if (*curr == ' ' || *curr == '?'){
        if ( players[sockfd].gameState == -1 ){
            player_quit(sockfd);
        } else if ( players[sockfd].gameState == 6 ){
            win_game(sockfd);
        } else if (method == GET) {
            if ( strstr(curr, "start=") ){
                if(debug){printf("%d has entered the game.\n", sockfd);}
                serve_html(sockfd, FIRST_TURN);
                players[sockfd].gameState = 4;
                find_a_friend(sockfd);
            } else {
                if(debug){printf("Welcome, %d.\n", sockfd);}
                serve_html(sockfd, INTRO);
                players[sockfd].gameState = 1;
            }
        }

        else if (method == POST)
        {
            if ( strstr(curr, "user=") ){
                if(debug){printf("%d has chosen a username, for the little good it will do them.\n", sockfd);}
                serve_html(sockfd, START);
                players[sockfd].gameState = 2;
                find_a_friend(sockfd);
            // and here, the guess protocol
          } else if ( strstr(curr, "keyword=") ){
                if ( find_a_friend(sockfd) ){
                    if(debug){printf("%d is attempting a guess.\n", sockfd);}
                    if ( friend_status(sockfd) ){
                        char * tempGuess = strstr(buff, "keyword=") + 8;
                        int guess_length = strlen(tempGuess) - 12; // magic number is length of &guess=Guess
                        char guess[128];
                        for ( int i = 0; i < guess_length; ++i){
                            guess[i] = tempGuess[i];
                        }
                        guess[guess_length] = 0;
                        if ( guess_protocol(sockfd, guess) ){
                            win_game(sockfd);
                        } else {
                            serve_html(sockfd, ACCEPTED);
                        }
                    } else {
                        if(debug){printf("%d's friend is not ready, and the guess is discarded.\n", sockfd);}
                        serve_html(sockfd, DISCARDED);
                    }
                } else {
                    if(debug){printf("No-one is listening to %d, and the guess is discarded.\n", sockfd);}
                    serve_html(sockfd, DISCARDED);
                }
            // and another quit handler.
            } else if ( strstr(curr, "quit=") ){
                player_quit(sockfd);
            }
        }
        else
            // never used, just for completeness
            fprintf(stderr, "no other methods supported");
    }
    // send 404
    else if (write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0)
    {
        perror("write");
        return false;
    }

    return true;
}

// this is the main thing to analyse, see what I can do better.
// turns out to be pretty average
int main(int argc, char * argv[]){
    if (argc < 3) //nothing to change here really
    {
        fprintf(stderr, "usage: %s ip port\n", argv[0]);
        return 0; // maybe change this to EXIT_FAILURE
    }

    // turns on debug messages
    if ( argc > 3 && !strcmp(argv[3], "debug") ){
        debug = true;
        printf("Debug mode on. Prepare for a lot of text.\nNote that all users are referred to by their Socket Number.\n");
    }

    // create TCP socket which only accept IPv4
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // regular expression
    if (sockfd < 0) // also regular expression
    {
        perror("socket"); // feel free to customise perrors
        exit(EXIT_FAILURE); // use errno
    }

    // reuse the socket if possible
    int const reuse = 1; // seems to also be a regular expression
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // create and initialise address we will listen on
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // if ip parameter is not specified
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]); // need this for customised address
    serv_addr.sin_port = htons(atoi(argv[2])); // need this for customised port
    // otherwise all standard expressions, just rearranged

    // bind address to socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    } // standard expression

    // status message and initialising
    printf("%s server is now running at IP: %s on port %s\n", VERSION_NAME, argv[1], argv[2]);
    players[sockfd].otherSock = sockfd;
    players[sockfd].gameState = -1;

    // listen on the socket
    listen(sockfd, 5); // standard expression

    // initialise an active file descriptors set
    fd_set masterfds;
    FD_ZERO(&masterfds);
    FD_SET(sockfd, &masterfds);
    // record the maximum socket number
    maxfd = sockfd;
    // seems standard

    // used this to find the max number of sockets
    //printf("%d\n", maxfd);



    while (1)
    {
        // monitor file descriptors
        fd_set readfds = masterfds;
        if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("select"); // still seems standard
            exit(EXIT_FAILURE);
        }

        // loop all possible descriptor
        for (int i = 0; i <= maxfd; ++i)
            // determine if the current file descriptor is active
            if (FD_ISSET(i, &readfds))
            {
                // create new socket if there is new incoming connection request
                if (i == sockfd)
                {
                    struct sockaddr_in cliaddr;
                    socklen_t clilen = sizeof(cliaddr);
                    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
                    if (newsockfd < 0)
                        perror("accept");
                    else
                    {
                        // add the socket to the set
                        FD_SET(newsockfd, &masterfds);
                        // update the maximum tracker
                        if (newsockfd > maxfd)
                            maxfd = newsockfd;
                        // print out the IP and the socket number
                        char ip[INET_ADDRSTRLEN];
                        printf(
                            "new connection from %s on socket %d\n",
                            // convert to human readable string
                            inet_ntop(cliaddr.sin_family, &cliaddr.sin_addr, ip, INET_ADDRSTRLEN),
                            newsockfd
                        );

                        // Add new player as identified by their socket
                        players[newsockfd].gameState = 1;
                        players[newsockfd].otherSock = -1;
                    }

                }
                // a request is sent from the client
                else if (!handle_http_request(i))
                {
                    close(i);
                    FD_CLR(i, &masterfds);
                }
            }
    }

    return 0;
}
