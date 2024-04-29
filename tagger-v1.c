/**
 * Image_Tagger V1, adjusted from COMP30023 workshop 6: http-server.c
 * As presented for comp30023-2019-project-1.
 * Work by Nicholas Gurban, Student ID 757235.
 */

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


// represents the types of method
typedef enum
{
    GET,
    POST,
    UNKNOWN
} METHOD;

static bool handle_http_request(int sockfd)
{
    // try to read the request
    char buff[2049];
    int n = read(sockfd, buff, 2049);
    if (n <= 0)
    {
        if (n < 0)
            perror("read");
        else
            printf("socket %d close the connection\n", sockfd);
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
    while (*curr == '.' || *curr == '/')
        ++curr;
    // assume the only valid request URI is "/" but it can be modified to accept more files
    if (*curr == ' ')
        if (method == GET)
        {
            // get the size of the file
            struct stat st;
            stat(1_INTRO, &st);
            n = sprintf(buff, HTTP_200_FORMAT, st.st_size);
            // send the header first
            if (write(sockfd, buff, n) < 0)
            {
                perror("write");
                return false;
            }
            // send the file
            int filefd = open(1_INTRO, O_RDONLY);
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
        }
        else if (method == POST)
        {
            printf(buff);
            printf("\nEND OF MESSAGE\n\n");

            if ( strstr(buff, "user=") ){
                printf("username given\n")
            }

            // locate the username, it is safe to do so in this sample code, but usually the result is expected to be
            // copied to another buffer using strcpy or strncpy to ensure that it will not be overwritten.

            /**
            char * username = strstr(buff, "user=") + 5;
            int username_length = strlen(username);
            // the length needs to include the ", " before the username
            long added_length = username_length + 2;

            // get the size of the file
            struct stat st;
            stat("lab6-POST.html", &st);
            // increase file size to accommodate the username
            long size = st.st_size + added_length;
            n = sprintf(buff, HTTP_200_FORMAT, size);
            // send the header first
            if (write(sockfd, buff, n) < 0)
            {
                perror("write");
                return false;
            }
            // read the content of the HTML file
            int filefd = open("lab6-POST.html", O_RDONLY);
            n = read(filefd, buff, 2048);
            if (n < 0)
            {
                perror("read");
                close(filefd);
                return false;
            }
            close(filefd);
            // move the trailing part backward
            int p1, p2;
            for (p1 = size - 1, p2 = p1 - added_length; p1 >= size - 25; --p1, --p2)
                buff[p1] = buff[p2];
            ++p2;
            // put the separator
            buff[p2++] = ',';
            buff[p2++] = ' ';
            // copy the username
            strncpy(buff + p2, username, username_length);
            if (write(sockfd, buff, size) < 0)
            {
                perror("write");
                return false;
            }
            */
        }
        else
            // never used, just for completeness
            fprintf(stderr, "no other methods supported");
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
int main(int argc, char * argv[])
{
    if (argc < 3) //nothing to change here really
    {
        fprintf(stderr, "usage: %s ip port\n", argv[0]);
        return 0; // maybe change this to EXIT_FAILURE
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

    // listen on the socket
    listen(sockfd, 5); // standard expression

    // initialise an active file descriptors set
    fd_set masterfds;
    FD_ZERO(&masterfds);
    FD_SET(sockfd, &masterfds);
    // record the maximum socket number
    int maxfd = sockfd;
        // seems standard


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
