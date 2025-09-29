/**
 * webserver.c -- A webserver written in C
 * 
 * Test with curl (if you don't have it, install it):
 * 
 *    curl -D - http://localhost:3490/
 *    curl -D - http://localhost:3490/d20
 *    curl -D - http://localhost:3490/date
 * 
 * You can also test the above URLs in your browser! They should work!
 * 
 * Posting Data:
 * 
 *    curl -D - -X POST -H 'Content-Type: text/plain' -d 'Hello, sample data!' http://localhost:3490/save
 * 
 * (Posting data is harder to test from a browser.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include "net.h"
#include "file.h"
#include "mime.h"
#include "cache.h"

#define PORT "3490"  // the port users will be connecting to

#define SERVER_FILES "./serverfiles"
#define SERVER_ROOT "./serverroot"

/**
 * Send an HTTP response
 *
 * header:       "HTTP/1.1 404 NOT FOUND" or "HTTP/1.1 200 OK", etc.
 * content_type: "text/plain", etc.
 * body:         the data to send.
 * 
 * Return the value from the send() function.
 */
int send_response(int fd, char *header, char *content_type, void *body, int content_length)
{
    const int max_response_size = 262144;
    char response[max_response_size];
    
    //Build Date
    //gmt buffer to store the date string
    char gmt_buf[64];
    {
        time_t now = time(0); // get time
        struct tm tm = *gmtime(&now); //gmt struct
        strftime(gmt_buf, sizeof gmt_buf, "%a, %d %b %Y %H:%M:%S %Z", &tm); //place time into gmt_buf
    }

    // Build Content-Length header
    char content_length_header[64];
    snprintf(content_length_header, sizeof content_length_header, "Content-Length: %d", content_length);

    // Hardcoded connection header
    char connection[] = "Connection: close";

    // Build the headers into the response buffer
    int response_length = snprintf(response, max_response_size,
        "%s\r\nDate: %s\r\n%s\r\n%s\r\nContent-Type: %s\r\n\r\n",
        header, gmt_buf, connection, content_length_header, content_type);
    
        //check for errors, or if length is bigger than max size
    if (response_length < 0 || response_length >= max_response_size) {
        fprintf(stderr, "Header too large to fit in response buffer\n");
        return -1;
    }
    //copy body into response buffer.
    memcpy(response + response_length, body, content_length);
    
    // printf("\n%s\n", response);


    /*
    pseudo format for response
    {header}\n
    Date: {get_date}\n
    Connection: {connection (close)}\n
    Content-Length: {content_length}\n
    Content-Type: {content_type}\n
    \n
    {body}
    
    */


    /*
    Example output:

    HTTP/1.1 200 OK
    Date: Wed Dec 20 13:05:11 PST 2017
    Connection: close
    Content-Length: 41749
    Content-Type: text/html

    <!DOCTYPE html><html><head><title>Lambda School ...
    
    
    */


    // Send it all!
    int rv = send(fd, response, response_length+content_length, 0);

    if (rv < 0) {
        perror("send");
    }

    return rv;
}


/**
 * Send a /d20 endpoint response
 */
void get_d20(int fd)
{
    int N = 20;
    // Generate a random number between 1 and 20 inclusive
    int d20 = rand() % (N + 1);
    void* body_buf[64];
    int len = snprintf(body_buf, sizeof(body_buf), "%d", d20);

    char mime_type[] = "text/html";//"text/plain";

    send_response(fd, "HTTP/1.1 200 OK", mime_type, body_buf, len); //prevent sending garbage to browser

}

/**
 * Send a 404 response
 */
void resp_404(int fd)
{
    char filepath[32];
    struct file_data *filedata = NULL; 
    char *mime_type;

    // Fetch the 404.html file
    int filepath_len = snprintf(filepath, sizeof filepath, "%s/404.html", SERVER_FILES);
    // printf("%s\n", filepath);
    
    filedata = file_load(filepath);
    // printf("\nFile Loaded.\n");
    if (filedata == NULL) {
        // TODO: make this non-fatal
        fprintf(stderr, "cannot find system 404 file\n");
        exit(3);
    }

    mime_type = mime_type_get(filepath);
    // printf("\n%s\n", mime_type);
    send_response(fd, "HTTP/1.1 404 NOT FOUND", mime_type, filedata->data, filedata->size);

    file_free(filedata);
}

/**
 * Read and return a file from disk or cache
 */
void get_file(int fd, struct cache *cache, char *request_path)
{

    char filepath[64];
    struct file_data *filedata = NULL; 
    char *mime_type;

    // Fetch the 404.html file
    int filepath_len = snprintf(filepath, sizeof filepath, "%s%s", SERVER_ROOT, request_path);
    // printf("%s\n", filepath);
    
    filedata = file_load(filepath);
    // printf("\nFile Loaded.\n");
    if (filedata == NULL) {
        resp_404(fd);
    }

    mime_type = mime_type_get(filepath);
    // printf("\n%s\n", mime_type);
    send_response(fd, "HTTP/1.1 200 OK", mime_type, filedata->data, filedata->size);

    file_free(filedata);

    //check cache for the file
    printf("\n%s\n", request_path);
    // Not in cache -> check disk

    //else return 404. Not found
}

/**
 * Search for the end of the HTTP header
 * 
 * "Newlines" in HTTP can be \r\n (carriage return followed by newline) or \n
 * (newline) or \r (carriage return).
 */
char *find_start_of_body(char *header)
{
    //return the poninter that finds the 2 new lines in a row
    ///////////////////
    // IMPLEMENT ME! // (Stretch)
    ///////////////////
}

/**
 * Handle HTTP request and send response
 */
void handle_http_request(int fd, struct cache *cache)
{
    const int request_buffer_size = 65536; // 64K
    char request[request_buffer_size];

    // Read request
    int bytes_recvd = recv(fd, request, request_buffer_size - 1, 0);

    if (bytes_recvd < 0) {
        perror("recv");
        return;
    }
    //tokenize using strtok
    char delim[] = " ";
    char* token;

    token = strtok(request, delim);
    printf("\n%s\n", token);
    //first token
    if (strcmp(token, "GET") == 0) {
        
        token = strtok(NULL, delim);        
        if (strcmp(token, "/d20") == 0) {
            get_d20(fd);
        } else if (strcmp(token, "/") == 0) {
            get_file(fd, cache, "/index.html");
        }   else {
            get_file(fd, cache, token);
        }

    } else if (strcmp(token, "POST")) {
        //
    } else {
        resp_404(fd);
    }
    // resp_404(fd);


    // printf("\n%s\n", request);

    ///////////////////
    // IMPLEMENT ME! //
    ///////////////////

    // Read the first two components of the first line of the request 
 
    // If GET, handle the get endpoints

    //    Check if it's /d20 and handle that special case
    //    Otherwise serve the requested file by calling get_file()


    // (Stretch) If POST, handle the post request
}

/**
 * Main
 */
int main(void)
{
    int newfd;  // listen on sock_fd, new connection on newfd
    struct sockaddr_storage their_addr; // connector's address information
    char s[INET6_ADDRSTRLEN];

    struct cache *cache = cache_create(10, 0);

    // Get a listening socket
    //port 3490
    int listenfd = get_listener_socket(PORT);

    if (listenfd < 0) {
        fprintf(stderr, "webserver: fatal error getting listening socket\n");
        exit(1);
    }

    printf("webserver: waiting for connections on port %s...\n", PORT);

    // This is the main loop that accepts incoming connections and
    // responds to the request. The main parent process
    // then goes back to waiting for new connections.
    
    while(1) {
        socklen_t sin_size = sizeof their_addr;

        // Parent process will block on the accept() call until someone
        // makes a new connection:
        newfd = accept(listenfd, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }

        // Print out a message that we got the connection
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        // newfd is a new socket descriptor for the new connection.
        // listenfd is still listening for new connections.
        // get_d20(newfd);
        // resp_404(newfd);
        handle_http_request(newfd, cache);

        close(newfd);
    }

    // Unreachable code

    return 0;
}

