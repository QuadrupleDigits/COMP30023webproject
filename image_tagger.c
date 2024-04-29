/**
 * Image_Tagger V9.
 * As presented for comp30023-2019-project-1.
 * Work by Nicholas Gurban, Student ID 757235.
 * 
 * Parent code from COMP30023 workshop 6: http-server.c
 */
 
static char const * const VERSION_NAME = "image_tagger";


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
    char wordsUsed[24][24];
    char playerName[24];
};
struct PlayerInfo players[24];
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


// serve_welcome: adds a username to the start screen.
static bool serve_welcome(int sockfd){

    if ( !strcmp( players[sockfd].playerName, "" ) ){
        if(debug){printf("%d has no name, redirected to regular %s page.\n", sockfd, START);}
        return serve_html(sockfd, START);
    }

    if(debug){printf("Page %s is being altered for %d.\n", START, sockfd);}
    // increasing buffer size, just in case.
    char buff[2049];

    // first we're gonna make a string to be added.
    char added_text[1025];
    bzero(added_text, 1025);
    strcat(added_text, "<p>Welcome, ");
    strcat(added_text, players[sockfd].playerName);
    strcat(added_text, ".</p> ");
    // then, find added length of all words.
    long text_length = strlen(added_text);
    struct stat st;
    stat(START, &st);
    // increase the file size
    long size = st.st_size + text_length;
    int n = sprintf(buff, HTTP_200_FORMAT, size);
    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return false;
    }

    // then read contents of HTML file to be doctored
    int filefd = open(START, O_RDONLY);
    n = read(filefd, buff, 2048);
    if (n < 0)
    {
        perror("read");
        close(filefd);
        return false;
    }
    close(filefd);

    // make insertion into the html file
    char * trailing_text = strstr(buff, "<form method=\"GET\">") - 1;
    strcat(added_text, trailing_text);
    // need to update text_length
    text_length = strlen(added_text);
    strncpy(trailing_text, added_text, text_length);

    // and write out, nice and easy
    if (write(sockfd, buff, size) < 0)
    {
        perror("write");
        return false;
    }
    if(debug){printf("Altered page %s has been sent to %d.\n", START, sockfd);}
    return true;
}


// serve_guesses: just for processing accepted guesses.
static bool serve_guesses(int sockfd){
    if(debug){printf("Page %s is being altered for %d.\n", ACCEPTED, sockfd);}
    // increasing buffer size, just in case.
    char buff[4097];

    // first we're gonna make a string to be added.
    char added_text[2049];
    bzero(added_text, 2049);
    strcat(added_text, "<p> ");
    for (int i = 0; i < players[sockfd].numWords; ++i){
        if ( i > 0){
            strcat(added_text, ", ");
        }
        strcat(added_text, players[sockfd].wordsUsed[i]);
    }
    strcat(added_text, " </p>");
    // then, find added length of all words.
    long text_length = strlen(added_text);
    struct stat st;
    stat(ACCEPTED, &st);
    // increase the file size
    long size = st.st_size + text_length;
    int n = sprintf(buff, HTTP_200_FORMAT, size);
    // send the header first
    if (write(sockfd, buff, n) < 0)
    {
        perror("write");
        return false;
    }

    // then read contents of HTML file to be doctored
    int filefd = open(ACCEPTED, O_RDONLY);
    n = read(filefd, buff, 4096);
    if (n < 0)
    {
        perror("read");
        close(filefd);
        return false;
    }
    close(filefd);

    // make insertion into the html file
    char * trailing_text = strstr(buff, "</p>") + 4;
    strcat(added_text, trailing_text);
    // need to update text_length
    text_length = strlen(added_text);
    strncpy(trailing_text, added_text, text_length);

    // and write out, nice and easy
    if (write(sockfd, buff, size) < 0)
    {
        perror("write");
        return false;
    }
    if(debug){printf("Altered page %s has been sent to %d.\n", ACCEPTED, sockfd);}
    return true;
}


// to reset player guesses
static void wipe_words(int sockfd){
    if(!players[sockfd].numWords){
        if(debug){printf("Wiping past guesses of %d.\n", sockfd);}
        for (int i = 0; i < players[sockfd].numWords; ++i){
            if(debug){printf("Wiping past guesses of %d: %s\n", sockfd, players[sockfd].wordsUsed[i] );}
            bzero(players[sockfd].wordsUsed[i], 24);
        }
        players[sockfd].numWords = 0;
    }
    return;
}


// quitter code. Part is for the eventuality of cookies.
static void player_quit(int sockfd){
    if(debug){printf("Game Over for %d.\n", sockfd);}
    serve_html(sockfd, GAMEOVER);
    players[sockfd].gameState = 1;
    if ( players[sockfd].otherSock >= 0 ){
        players[players[sockfd].otherSock].gameState = -1;
        if(debug){
            printf("%d is going to lose.\n", players[sockfd].otherSock);
            printf("%d is losing friends.\n", sockfd);
            printf("%d and %d are no longer friends.\n", sockfd, players[sockfd].otherSock);}
        players[players[sockfd].otherSock].otherSock = -1;
        players[sockfd].otherSock = -1;
    }
    // need to clean up past guesses in case of rematch
    wipe_words(sockfd);
    return;
}

// and another one for winning
static void win_game(int sockfd){
    if(debug){printf("Game won for %d.\n", sockfd);}
    serve_html(sockfd, ENDGAME);
    /** player[sockfd].gameState needs to be set to 0 to be able to exit afterwards,
     *  since win/loss detection is first option for GET/POST processing and if done
     *  otherwise will result in an ENDGAME loop despite pressing other buttons.
     */
    players[sockfd].gameState = 1;
    if ( players[sockfd].otherSock >= 0 ){
        players[players[sockfd].otherSock].gameState = 6;
        if(debug){
            printf("%d is going to win.\n", players[sockfd].otherSock);
            printf("%d is losing friends.\n", sockfd);
            printf("%d and %d are no longer friends.\n", sockfd, players[sockfd].otherSock);}
        players[players[sockfd].otherSock].otherSock = -1;
        players[sockfd].otherSock = -1;
    }
    // might as well run cleanup
    wipe_words(sockfd);
    return;
}

// Finds a friend to play against. Designed for multiple simultaneous games I guess.
static bool find_a_friend(int sockfd){
    /**  I know the brief said that the program only needs to find friends at the start,
     *  but the way I've designed this may have inadvertently resulted in a server capable
     *  of multiple simultaneous games. Because of this design and whatever complications
     *  it causes, it's just easier to call this function every time the 'start' or 'guess'
     *  calls are made.
     */
    if ( players[sockfd].otherSock >= 0 ){
        if(debug){printf("%d is playing with %d.\n", sockfd, players[sockfd].otherSock);}
        return true;
    } else {
        if(debug){printf("%d is searching for other players.\n", sockfd);}
        for (int i = 0; i < maxfd; ++i){
            if ( (i != sockfd) && (players[i].otherSock < 0) && (1 < players[i].gameState) && (players[i].gameState < 6) ){
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
        if(debug){printf("Both players ready.\n");}
        return true;
    } else if ( players[otherPlayer].gameState == -1 ){
        players[sockfd].gameState = -1;
    }
    if(debug){printf("%d not ready.\n", otherPlayer);}
    return false;
}

// works out guesses
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
    if(debug){printf("%s added to %d's guesses.\n", guess, sockfd);}
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
            if(debug){printf("socket %d close the connection\n", sockfd);}
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

        // game end conditions, if they have been triggered by the friend.
        if ( players[sockfd].gameState == -1 ){
            player_quit(sockfd);
        } else if ( players[sockfd].gameState == 6 ){
            win_game(sockfd);
        }

        // GET request processing
        else if (method == GET) {
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

        // POST request processing
        else if (method == POST)
        {
            if ( strstr(curr, "user=") ){
                char * username = strstr(buff, "user=") + 5;
                bzero(players[sockfd].playerName, 24);
                strcpy( players[sockfd].playerName, username );
                if(debug){printf("%d adopts username %s.\n", sockfd,players[sockfd].playerName);}
                serve_welcome(sockfd);
                players[sockfd].gameState = 2;
                find_a_friend(sockfd);
            // and here, the guess protocol.
          } else if ( strstr(curr, "keyword=") ){
                if ( find_a_friend(sockfd) ){
                    if(debug){printf("%d is attempting a guess.\n", sockfd);}
                    if ( friend_status(sockfd) ){
                        char * tempGuess = strstr(buff, "keyword=") + 8;
                        /** Two magic numbers here.
                         *  8 = strlen("keyword="), removing the classifier.
                         *  Next, each GUESS Post is subscripted by "&guess=Guess".
                         *  12 = strlen("&guess=Guess")
                         */
                        int guess_length = strlen(tempGuess) - 12;
                        char guess[24];
                        strncpy(guess, tempGuess, guess_length);
                        guess[guess_length] = 0;
                        if ( guess_protocol(sockfd, guess) ){
                            win_game(sockfd);
                        } else {
                            serve_guesses(sockfd);
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
            if(debug){fprintf(stderr, "Not GET or POST, and no other methods supported.\n");}
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
        fprintf(stderr, "usage: %s <ip> <port>\nAdd \"debug\" to input to start the server in debug mode.\n", argv[0]);
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
                        if(debug){printf(
                            "new connection from %s on socket %d\n",
                            // convert to human readable string
                            inet_ntop(cliaddr.sin_family, &cliaddr.sin_addr, ip, INET_ADDRSTRLEN),
                            newsockfd
                        );}

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

