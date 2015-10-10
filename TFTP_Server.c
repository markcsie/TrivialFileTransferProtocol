#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <arpa/inet.h>

#define MAXBUFSIZE 4096
// 1 - file not found
// 4 - illegal TFTP operation
// 6 - file already exists
// flag: GET = 0 PUT = 1 ELSE = 2
char upload_buf[516];
int end;

int rdt_send (int server_socket, char *buf, struct sockaddr_in addr, int size, int flag) {
	socklen_t addr_len = sizeof(addr);
	char buf_for_ACK[4];
	int select_return;
	struct timeval timeout;
	
	int maxfd = getdtablesize();
	fd_set readfds;
	fd_set masterfds;
	FD_ZERO(&readfds);
	FD_ZERO(&masterfds);
	
	int nbytes = 0;
	int count = 0;
	
	FD_SET(server_socket, &masterfds);
	//fprintf(stderr, "Client's IP:%s\n", inet_ntoa(addr.sin_addr));
	sendto (server_socket, buf, size, 0, (struct sockaddr *)&addr, sizeof(addr));
	if (end == 1)
		return nbytes;
	while (1) {
		readfds = masterfds;
		timeout.tv_sec =  2;
		timeout.tv_usec = 0;
		
		select_return = select(maxfd, &readfds, NULL, NULL, &timeout); 
		if (select_return < 0)
			fprintf(stderr, "Select error!!!\n");
		else if (select_return == 0) {
			fprintf(stderr, "Timeout 0.2 seconds\nResending...\n");
			// resend
			fprintf(stderr, "Data block: %d\n", buf[2] * 128 + buf[3]);
			sendto (server_socket, buf, size, 0, (struct sockaddr *)&addr, sizeof(addr));
			if (end == 1) {   // for uploading // sending the last ACK	
				//break;
				count++;
				if (count == 5)
					break;
			}
		}
		else {
			if (flag == 1) { // for uploading
				nbytes = recvfrom(server_socket, upload_buf, 516, 0, (struct sockaddr *)&addr, &addr_len); // receiving uploaded chunk
				fprintf(stderr, "Get a chuck of file\n");
				fprintf(stderr, "ACK block: %d\n", buf[2] * 128 + buf[3]);
				fprintf(stderr, "Chunk: %d\n", upload_buf[2] * 128 + upload_buf[3]);
				if ( buf[2] * 128 + buf[3] + 1 != upload_buf[2] * 128 + upload_buf[3])
					continue;
			}
			else if (flag == 0){ // for downloading
				nbytes = recvfrom(server_socket, buf_for_ACK, 4, 0, (struct sockaddr *)&addr, &addr_len); // receiving ACK
				fprintf(stderr, "Get an ACK %d\n", buf_for_ACK[2] * 128 + buf_for_ACK[3]);
				if (buf[2] * 128 + buf[3] != buf_for_ACK[2] * 128 + buf_for_ACK[3])
					continue;
			}
			else { // for error message
				nbytes = recvfrom(server_socket, buf_for_ACK, 4, 0, (struct sockaddr *)&addr, &addr_len); // receiving ACK
				fprintf(stderr, "Get an ACK\n");
			}
			
			break;
		}
	}
	return nbytes;
}

int main (int argc, char** argv) {
	int server_socket = 0;
	int server_socket_d = 0;
	
	int file_fd = 0; 
	struct sockaddr_in server_addr;
	struct sockaddr_in server_addr_d;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	char server_buf[MAXBUFSIZE];
	char client_buf[516];
	char ACK_buf[4]; 
	char ERROR_buf[100]; 
	char file[FILENAME_MAX]; // file to open
	int nbytes_read;
	unsigned short int block_count = 1;
	//FILE *uploaded_file;
	int upload_fd;
	struct stat sb;
	
	int nbytes_received;
	
	server_socket = socket(AF_INET, SOCK_DGRAM, 0 ); //create a server socket UDP
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET; //IPv4 protocol
	server_addr.sin_port = htons( (unsigned short) atoi( argv[1] ) ); //port number
	server_addr.sin_addr.s_addr = htonl (INADDR_ANY); //for any addr to connect to the server
	
	bind(server_socket, (struct sockaddr*) & server_addr, sizeof(server_addr) ); //create a listener
	
	while (1) {
		memset(server_buf, 0, sizeof(server_buf));
		memset(client_buf, 0, sizeof(client_buf));
		memset(ACK_buf, 0, sizeof(ACK_buf));
		memset(ERROR_buf, 0, sizeof(ERROR_buf));
		memset(file, 0, sizeof(file));
		memset(upload_buf, 0, sizeof(upload_buf));
		
		block_count = 0;
		end = 0;
		client_addr_len = sizeof(client_addr);
		
		recvfrom(server_socket, server_buf, MAXBUFSIZE, 0, (struct sockaddr *)&client_addr, &client_addr_len); 
		// request from client through UDP connection
		
		// new channel for transferring data
		server_socket_d = socket(AF_INET, SOCK_DGRAM, 0 ); //create a server socket UDP
		bzero(&server_addr_d, sizeof(server_addr_d));
		server_addr_d.sin_family = AF_INET; //IPv4 protocol
		server_addr_d.sin_port = htons( 5487 ); //port number
		server_addr_d.sin_addr.s_addr = htonl (INADDR_ANY); //for any addr to connect to the server
		bind(server_socket_d, (struct sockaddr*) & server_addr_d, sizeof(server_addr_d) ); //create a listener
		//////////////////////////////////////////////////////
		// handling the request //
		if ( server_buf[0] == 0 && server_buf[1] == 0 ) { // GET
			// send data to client
			fprintf(stderr, "Get a new request!!\n");
			fprintf (stderr, "Opcode:%d\n", (int)server_buf[1]);
			block_count = 1;
			strcpy (file, &server_buf[2]);
			fprintf (stderr, "Requested file:%s\n", file);
			client_buf[0] = 0;
			client_buf[1] = 2;
			
			file_fd = open(file, O_RDONLY);
			if (file_fd > 0) { // opening successes 
				stat( file, &sb ); //get the information of the file
				// read and write the requested data
				while ( ( nbytes_read = read( file_fd, &client_buf[4], 512 ) ) > 0 ) {
					client_buf[2] = block_count / 128;
					client_buf[3] = block_count % 128;
					fprintf(stderr, "Sending the requested file...\n");
					//fprintf(stderr, " %d %d\n", client_buf[2], client_buf[3]);
					rdt_send (server_socket_d, client_buf, client_addr, nbytes_read + 4, 0);
					block_count++;
				}
				fprintf(stderr, "End of the requested file\n");
				if (sb.st_size % 512 == 0) {
					client_buf[2] = block_count / 128;
					client_buf[3] = block_count % 128;
					strncpy (&client_buf[4], "", strlen("")); // empty chunk
					rdt_send (server_socket_d, client_buf, client_addr, 4, 0);
				}
				close(file_fd);
			}
			else {
				//////////////// FILE NOT FOUND ///////////////
				fprintf(stderr, "Error 1: File not found\n");
				ERROR_buf[0] = 0;
				ERROR_buf[1] = 4;
				strcpy(&ERROR_buf[4], "Error 1: File not found");
				// send error message to client
				rdt_send (server_socket, ERROR_buf, client_addr, 100, 2); 
			}
		}
		else if ( server_buf[0] == 0 && server_buf[1] == 1 ) { //put
			fprintf (stderr, "Get a new request!!\n");
			fprintf (stderr, "Opcode:%d\n", (int)server_buf[1]);
			strcpy (file, &server_buf[2]);
			fprintf (stderr, "Uploaded file:%s\n", file);
			file_fd = open(file, O_WRONLY | O_APPEND);
			if (file_fd > 0) {
				// File already exists
				close (file_fd);
				fprintf(stderr, "Error 6: File already exists\n");
				ERROR_buf[0] = 0;
				ERROR_buf[1] = 4;
				strcpy(&ERROR_buf[4], "Error 6: File already exists");
				// Send error message to client
				rdt_send ( server_socket, ERROR_buf, client_addr, 100, 2); 
			}
			else {
				// Send ACK 0
				//uploaded_file = fopen (file, "a+");
				upload_fd = open (file, O_WRONLY | O_CREAT , S_IRUSR | S_IWUSR | S_IROTH);
				ACK_buf[0] = 0;
				ACK_buf[1] = 3;
				ACK_buf[2] = 0;
				ACK_buf[3] = 0;
				fprintf(stderr, "Sending ACK 0...\n");
				nbytes_received = rdt_send ( server_socket_d, ACK_buf, client_addr, 4, 1);
				
				
				fprintf(stderr, "Receive data %d bytes\n", nbytes_received);
				write (upload_fd, &upload_buf[4], nbytes_received - 4);
				block_count++;  // block_count == 1
				if (nbytes_received - 4 < 512) { // filesize < 512
					end = 1;
					ACK_buf[0] = 0;
					ACK_buf[1] = 3;
					ACK_buf[2] = (int)block_count / 128;
					ACK_buf[3] = (int)block_count % 128;
					fprintf(stderr, "End of the uploaded file\n");
					fprintf(stderr, "Sending ACK %d...\n", block_count);
					
					nbytes_received = rdt_send ( server_socket_d, ACK_buf, client_addr, 4, 1);
					
					continue;
				}
				while (1) {
					ACK_buf[0] = 0;
					ACK_buf[1] = 3;
					ACK_buf[2] = (int)block_count / 128;
					ACK_buf[3] = (int)block_count % 128;
					fprintf(stderr, "Sending ACK %d...\n", block_count);
					//sendto (server_socket_d, ACK_buf, 4, 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
					//fprintf(stderr, "Sending ACK %d...\n", block_count);
					//nbytes_received = recvfrom(server_socket_d, upload_buf, 516, 0, (struct sockaddr *)&client_addr, &client_addr_len); // receiving uploaded chunk
					//fprintf(stderr, "Sending ACK %d...\n", block_count);
					nbytes_received = rdt_send ( server_socket_d, ACK_buf, client_addr, 4, 1);
					if (block_count + 1 == upload_buf[2] * 128 + upload_buf[3]) {
						fprintf(stderr, "Receive data %d bytes\n", nbytes_received - 4);
						write (upload_fd, &upload_buf[4], nbytes_received - 4);
						block_count++;
					}
					else {
						continue;
					}

					if (nbytes_received - 4 < 512) { // end of file
						end = 1;
						ACK_buf[0] = 0;
						ACK_buf[1] = 3;
						ACK_buf[2] = (int)block_count / 128;
						ACK_buf[3] = (int)block_count % 128;
						fprintf(stderr, "End of the uploaded file\n");
						fprintf(stderr, "Sending ACK %d...\n", block_count);
						nbytes_received = rdt_send ( server_socket_d, ACK_buf, client_addr, 4, 1);
						break;
					}
				}
			}
		}
		close (server_socket_d);
	}
	return 0;
}
