#include <stdio.h>

#include <sys/socket.h>

/**
 *  HTTP constants
 *  linebreaks used for ease in half-screen editing.
 *
 *  "Borrowed" from workshop-6 because, to be fair,
 *  HTTP requests are all the same.
 */
static char const * const HTTP-200 = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";
static char const * const HTT-400 = "HTTP/1.1 400 Bad Request\r\n\
Content-Length: 0\r\n\r\n";
static int const HTTP-400-LENGTH = 47;
static char const * const HTTP_404 = "HTTP/1.1 404 Not Found\r\n\
Content-Length: 0\r\n\r\n";
static int const HTTP-404-LENGTH = 45;


/**
 *  HTML filenames.
 */
static char const * const HTML-INTRO = "1_intro.html";
static char const * const HTML-START = "2_start.html";
static char const * const HTML-FIRST_TURN = "3_first_turn.html";
static char const * const HTML-ACCEPTED = "4_accepted.html";
static char const * const HTML-DISCARDED = "5_discarded.html";
static char const * const HTML-ENDGAME = "6_endgame.html";
static char const * const HTML-GAMEOVER = "7_gameover.html";
/**
 *  blame filepath conventions for dashes instead of underscores.
 */


int main(int argc, char * argv[]){

    /**
     * Check for all inputs, might as well have redundancy though
     * Remember: argv[0] is filepath, argv[N] is Nth argument.
     */
    if( argc < 3 ){
        fprintf( stderr, "Usage: %s <server_ip> <port_number>\n", argv[0]);
        return 0;
    }

    /**
     * Borrowing TCP socket programming from workshop-6.
     * Assuming that IPv6 not required, not specified in brief.
     */
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if( sockfd < 0 ){
        perror("socket");
        exit(EXIT_FAILURE);
    }


    printf("image_tagger server is now running at IP: <server_ip> on port <port_number>")


}
