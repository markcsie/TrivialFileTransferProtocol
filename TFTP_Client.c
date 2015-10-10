#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define MAXBUFSIZE 4096

int main (int argc, char** argv) {
    int i;
    int server_socket;
    int client_socket;
    int file_fd; 
    struct stat sb;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t clilen = sizeof(client_addr);
    char buf[MAXBUFSIZE]; 
    int buflen;
    char file[100]; // file to open
    int nbytes_read;
    time_t now; // present time
    char timebuf[100]; 
    char content_length_buf[100]; 
    char content_type[30]; // content_type string
    char content_type_buf[100];
    server_socket = socket(AF_INET, SOCK_STREAM, 0 ); //create a server socket
	
    server_addr.sin_family = AF_INET; //IPv4 protocol
    server_addr.sin_port = htons( (unsigned short) atoi( argv[1] ) ); //port number
    server_addr.sin_addr.s_addr = htonl (INADDR_ANY); //for any addr to connect to the server
	
    bind(server_socket, (struct sockaddr*) & server_addr, sizeof(server_addr) ); //create a listener
    listen(server_socket, 100); //start to listen
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*) &client_addr, &clilen ); // waiting to accept a client //TCP connection
        nbytes_read = read(client_socket, buf, MAXBUFSIZE); // read request from client
        strcpy(file, ""); // empty string
        for (i = 4; i < MAXBUFSIZE; i++ ) //split the "GET /xxx" string 
            if(buf[i] == ' ') {
                buf[i] = 0;
                break;
            }
        /* handling the requested file string*/
        if ( buf[ strlen(buf) - 5] == '.' || buf[ strlen(buf) - 4] == '.') {
            strcpy (file, "htdocs/");
            strcat(file, &buf[5] );
            file_fd = open( file, O_RDONLY );
        }
        else {
            if (buf[5] != '\0') {
                strcpy (file, "htdocs/");
                strcat(file, &buf[5] );
            }
            if (buf[ strlen(buf) - 1] == '/')
                strcat(file, "index.html\0");
            else
                strcat(file, "/index.html\0");
            file_fd = open( file, O_RDONLY );
        }
        /**************************************/
		
        for (i=0; i < strlen(file); i++) //get the content type
            if (file[i] == '.') {
                strcpy(content_type, &file[i+1]);
                break;
            }
		
        fprintf(stderr, "%s\n", file); //print the opened file
        if (file_fd > 0) { // opening successes
            stat( file, &sb ); //get the information of the file
            /**************** 200 OK header **********************/
            write(client_socket, "HTTP/1.1 200 OK\r\n", strlen("HTTP/1.1 200 OK\r\n") );
            // get and write the present time
            now = time( (time_t*) 0 );
            (void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );  
            write(client_socket,"Date: ", strlen("Date: ") );
            write(client_socket, timebuf, strlen(timebuf) );
            // get and write the present time
			
            // get and write the Last-Modified time
            (void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &sb.st_mtime ) );
            write(client_socket,"\r\nLast-Modified: ", strlen("\r\nLast-Modified: ") );
            write(client_socket, timebuf, strlen(timebuf) );
            // get and write the Last-Modified time
            
            // write the Content-Type
            if ( strcmp(content_type,"jpg") == 0 ||
				strcmp(content_type,"png") == 0 ||
				strcmp(content_type,"bmp") == 0 ||
				strcmp(content_type,"gif") == 0 
				)
                buflen = snprintf( content_type_buf, sizeof(content_type_buf), "\r\nContent-Type: image/%s", content_type );
            else if (strcmp(content_type,"txt") == 0)
                buflen = snprintf( content_type_buf, sizeof(content_type_buf), "\r\nContent-Type: text/%s", "plain" );
            else
                buflen = snprintf( content_type_buf, sizeof(content_type_buf), "\r\nContent-Type: text/%s", "html" );
            write(client_socket, content_type_buf, buflen );
            // write the Content-Type
            
            // write the Content-Length
            buflen = snprintf( content_length_buf, sizeof(content_length_buf), "\r\nContent-Length: %ld", (int64_t) sb.st_size );
            write(client_socket, content_length_buf, buflen );
            // write the Content-Length
            write(client_socket, "\r\n\r\n", strlen("\r\n\r\n") ); // end of header
            /********************************************************/
            
            while ( ( nbytes_read = read( file_fd, buf, MAXBUFSIZE ) ) > 0 ) // read and write the requested data
                write(client_socket, buf, nbytes_read );       
            close(file_fd);
        }
        else {
            /**************** 404 NOT FOUND header **********************/
            write(client_socket, "HTTP/1.1 404 Not Found\r\n", strlen("HTTP/1.1 404 Not Found\r\n") );
            
            // get and write the present time
            now = time( (time_t*) 0 );
            (void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
            write(client_socket,"Date: ", strlen("Date: ") );
            write(client_socket, timebuf, strlen(timebuf) );
            // get and write the present time
            
            write(client_socket, "\r\n\r\n", strlen("\r\n\r\n") ); // end of header
            /*************************************************************/
            write(client_socket, "404 error : File not found!!\n", strlen("404 error : File not found!!\n") ); // indicate that file not found
        }
        close(client_socket);
    }
    return 0;
}
