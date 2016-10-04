#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#ifdef __DEBUG__
	#define DEBUG(...) printf(__VA_ARGS__)
#else
	#define DEBUG(format, ...) {} 
#endif

#define BUFSIZE 4096

struct {
	char *ext;
	char *filetype;
} extentions [] = {
	{"gif" , "image/gif" },
	{"jpg" , "image/jpeg"},
	{"jpeg", "image/jpeg"},
	{"png" , "image/png" },
	{"htm" , "text/html" },
	{"html", "text/html" },
	{"mp4" , "video/mp4" },
	{"ogg" , "audio/ogg" },
	{"exe" , "text/plain"},
	{0,0} 
};

void handle_socket(int);
void unknown_cmd(int);
void not_found(int);
void inaccessible(int);
void serve_file(int, char *, int);
void move_permanently(int, char *, char *);
void handle_directory(int, char *);
void handle_object(int, char *, char *);
void access_object(int, char *);
void generate_index(int, char *);
void print_detailed(int, char *, struct stat);

/*
 * Unsupport command.
 */
void unknown_cmd(int client_sock)
{
	return ;
}

/*
 * Requested object without permission.
 */
void inaccessible(int client_sock)
{

	char buf[1024];

	sprintf(buf, "HTTP/1.0 403 FORBIDDEN\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "Content-Type: text/html\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "<HTML><TITLE>FORBIDDEN</TITLE>\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "<BODY><h1>403 FORBIDDEN</h1>Requested resource is forbidden</BODY></HTML>\r\n");
	write(client_sock, buf, strlen(buf));

	return ;
}

/*
 * Requested object do not found on web server.
 */
void not_found(int client_sock)
{
	
	char buf[1024];

	sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "Content-Type: text/html\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "<BODY><h1>404 NOT FOUND</h1>Requested resource is not found</BODY></HTML>\r\n");
	write(client_sock, buf, strlen(buf));

	return ;
}

/*
 * Send requested object back to client.
 */
void serve_file(int client_sock, char *fstr, int file_fd)
{

	int ret;
	char buf[1024];

	DEBUG("Now serve file...\n");

	sprintf(buf, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
	write(client_sock, buf, strlen(buf));

	while ((ret = read(file_fd, buf, 1024))) {
		write(client_sock, buf, ret);
	}	

	return ;
}

/*
 * Requested object are move permanently.
 */
void move_permanently(int client_sock, char *buffer, char *hostname)
{
	char buf[1024];

	sprintf(buf, "HTTP/1.0 301 MOVE PERMANENTLY\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "Content-Type: text/html\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "<HTML><TITLE>Move Permanently</TITLE>\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "<BODY><h1>301 MOVE PERMANENTLY</h1>you should access <a href=\"http://%s/%s/\">here</a></BODY></HTML>\r\n", hostname, buffer);
	write(client_sock, buf, strlen(buf));

	return ;
}

void print_detailed(int client_sock, char *buf, struct stat s)
{
	char mdate[30];

	if (s.st_mode & S_IFIFO)
		buf[0] = 'p';
	else if (s.st_mode & S_IFCHR)
		buf[0] = 'c';
	else if (s.st_mode & S_IFDIR)
		buf[0] = 'd';
	else if (s.st_mode & S_IFBLK)
		buf[0] = 'b';
	else
		buf[0] = '-';

	buf[1] = (s.st_mode & S_IRUSR) ? 'r' : '-';
	buf[2] = (s.st_mode & S_IWUSR) ? 'w' : '-';
	buf[3] = (s.st_mode & S_IXUSR) ? 'x' : '-';
	buf[4] = (s.st_mode & S_IRGRP) ? 'r' : '-';
	buf[5] = (s.st_mode & S_IWGRP) ? 'w' : '-';
	buf[6] = (s.st_mode & S_IXGRP) ? 'x' : '-';
	buf[7] = (s.st_mode & S_IROTH) ? 'r' : '-';
	buf[8] = (s.st_mode & S_IWOTH) ? 'w' : '-';
	buf[9] = (s.st_mode & S_IXOTH) ? 'x' : '-';

	buf[10] = '\0';
	write(client_sock, buf, strlen(buf));

	sprintf(buf, "%3d", s.st_nlink);
	write(client_sock, buf, strlen(buf));

	sprintf(buf, "%9d", s.st_uid);
	write(client_sock, buf, strlen(buf));

	sprintf(buf, "%8d", s.st_gid);
	write(client_sock, buf, strlen(buf));

	sprintf(buf, "%8d", (int)s.st_size);
	write(client_sock, buf, strlen(buf));

	strcpy(mdate, (char *)ctime(&s.st_mtime));
	mdate[16] = (char)0;
	sprintf(buf, " %s ", mdate);
	write(client_sock, buf, strlen(buf));

	return ;
}

/*
 * Generate index page if index page are not exist.
 */ 
void generate_index(int client_sock, char *buffer)
{
	DIR *dir;
	struct dirent *entry;
	struct stat s;
	char buf[1024];

	sprintf(buf, "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "<HTML><HEAD><TITLE>Generate Index</TITLE>\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "<style> body { font-family: monospace; while-space: pre; } </style></HEAD>\r\n");
	write(client_sock, buf, strlen(buf));
	sprintf(buf, "<BODY><hr/><pre>\r\n");
	write(client_sock, buf, strlen(buf));

	DEBUG("Opendir %s\n", buffer);
	if (!(dir = opendir(buffer))) {
		perror("opendir");
		exit(1);
	}

	while ((entry = readdir(dir)) != NULL) {
		DEBUG("%s\n", entry->d_name);
		stat(entry->d_name, &s);
		print_detailed(client_sock, buf, s);
		sprintf(buf, "<a href=\"%s\">%s</a>\r\n", entry->d_name, entry->d_name);
		write(client_sock, buf, strlen(buf));
	}

	sprintf(buf, "</pre><hr/></BODY><HTML>\r\n");
	write(client_sock, buf, strlen(buf));

	return ;
}

/*
 * Handle directory, automatically search index.html under requested directory.
 *
 * This function doing following thing:
 *
 * 	1. Check index.html status:
 * 		1. If exist, check permission:
 * 			1. With permission, call access_object.
 * 			2. Without permission, call inacessible.
 * 		2. If not exist, check directory permission:
 * 			1. If directory not exist, call not found.
 * 			2. With permission, call generate_index .
 *			3. Without permission, call inaccessible.
 */
void handle_directory(int client_sock, char *buffer)
{
	struct stat s;
	char *tmp;

	DEBUG("Now handling directory...\n");

	if (*buffer == (char)0)
		strcpy(buffer, "index.html\0");

	tmp = &buffer[strlen(buffer)-1];
	if (*tmp == '/')
		strcpy(tmp, "/index.html\0");
	DEBUG("Trying to access %s\n", buffer);

	if (stat(buffer, &s) == 0) {
		if (s.st_mode & S_IRUSR) {
			DEBUG("Successful access...\n");
			access_object(client_sock, buffer);
		} else {
			DEBUG("Access object without permission\n");
			inaccessible(client_sock);
		}	
	} else {

		DEBUG("File not found: %s\n", buffer);

		tmp = strrchr(buffer, '/');
		if (tmp)
			*tmp = (char)0;

		if (stat(buffer, &s) == 0) {
			if (s.st_mode & S_IRUSR) {
				DEBUG("Generate index.html\n");
				generate_index(client_sock, buffer);
			} else {
				inaccessible(client_sock);
			}
		} else {
			DEBUG("Directory not found: %s\n", buffer);
			not_found(client_sock);
		}
		
	}

	return ;
}

/*
 * Requested object is available, set content type then serve client.
 */
void access_object(int client_sock, char *buffer)
{

	int i, buflen;
	char *fstr = 0;
	int file_fd;	

	buflen = strlen(buffer);

	for (i=0; extentions[i].ext!=0; i++) {
		int len = strlen(extentions[i].ext);
		if (!strncmp(&buffer[buflen - len], extentions[i].ext, len)) {
			fstr = extentions[i].filetype;
			break;
		}
	}

	if (fstr == 0)
		fstr = extentions[i-1].filetype;
	DEBUG("File Type: %s\n",fstr);


	if ((file_fd = open(buffer, O_RDONLY)) == -1) {
		perror("open");
	}

	DEBUG("Starting server...\n");
	serve_file(client_sock, fstr, file_fd);

	return ;
}

/*
 * Handle object, check object status and call related function.
 *
 * This function doing following thing:
 *
 * 	1. Check object status.
 * 		1. If not found, call not_found.
 * 		2. If object is directory, call move_permanently.
 * 		3. If object is regular file, check permission:
 * 			1. Without permission, call inaccessible.
 * 			2. With permission, call access_object.
 */
void handle_object(int client_sock, char *buffer, char *hostname)
{

	struct stat s;

	DEBUG("Now handling object...\n");	

	DEBUG("Trying to access %s\n", buffer);

	if (stat(buffer, &s) == 0) {
		if (s.st_mode & S_IFDIR) {
			DEBUG("Access directory without slash\n");
			move_permanently(client_sock, buffer, hostname);
		} else if (s.st_mode & S_IFREG) {
			if (s.st_mode & S_IRUSR) {
				DEBUG("Successfully Access...\n");
				access_object(client_sock, buffer);		
			} else {
				DEBUG("Access object without permission\n");
				inaccessible(client_sock);
			}	
		} else {
			DEBUG("Access unknown object file\n");
		}
	} else {
		not_found(client_sock);
	}

	return ;
}

/*
 * Handle socket, determine requested object an object or a directory.
 *
 * This function doing following thing.
 *
 * 	1. Read from socket.
 * 	2. Check HTTP method and host name.
 * 	3. Check user try to access a object or a directory.
 * 		1. if directory, call handle_directory.
 * 		2. if object, call handle_object.
 */
void handle_socket(int client_sock)
{

	int ret, i;
	static char buffer[BUFSIZE + 1];
	char *tmp, *tmp_end;
	char hostname[80]={0};

	ret = read(client_sock, buffer, BUFSIZE);

	if (ret == -1 || ret == 0) {
		perror("read");
		exit(1);
	}

	if (ret > 0 && ret < BUFSIZE)
		buffer[ret] = 0;
	else
		buffer[0] = 0;

	for (i=0; i<ret; i++) {
		if (buffer[i] == '\r' && buffer[i+1] == '\n' && buffer[i+2] == '\r' && buffer[i+3] == '\n')
			buffer[i] = 0;
	}

	DEBUG("Command: %s\n", buffer);
	if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
		DEBUG("Unknown command: %s\n",buffer);
		unknown_cmd(client_sock);
		exit(1);
	}

	tmp = strstr(buffer, "Host: ");
	tmp_end = strstr(tmp, "\n");
	if (tmp)
		strncpy(hostname, tmp+6, tmp_end-tmp-6);
	DEBUG("Host name: %s\n",hostname);

	for (i=4; i<BUFSIZE; i++) {
		if (buffer[i] == ' ') {
			buffer[i] = 0;
			break;
		}
	}

	tmp = strrchr(buffer, '?');
	if (tmp)
		*tmp = (char)0;

	if (buffer[strlen(buffer)-1] == '/')
		handle_directory(client_sock, &buffer[5]);
	else
		handle_object(client_sock, &buffer[5], hostname);

	exit(1);
}

/*
 * Web server startup routine.
 *
 * This function doing following thing:
 * 	
 * 	1. Check parameter are available.
 * 	2. Bind and listen to certain port.
 * 	3. When accept a connection, webserver will fork a child process to handle socket.
 * 	4. Wait for next connection. 
 */
int main(int argc, char *argv[])
{

	int pid;
	int length;
	int server_sock = -1;
	int client_sock = -1;
	static struct sockaddr_in server_addr;
	static struct sockaddr_in client_addr;
	int port = 0;
	char *path = NULL;

	if (argc < 3) {
		printf("Usage: ./webserver port path\n");
		exit(1);
	}

	sscanf (argv[1], "%d", &port);
	DEBUG ("Port Number: %d\n",port);

	path = argv[2];
	DEBUG ("Root Directory: %s\n", path);
	if (chdir(path) == -1) {
		perror("chdir error");
		exit(1);
	}

	if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(port);

	if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind");
		exit(1);
	}

	if (listen(server_sock, 64) < 0) {
		perror("listen");
		exit(1);
	}

	DEBUG ("Starting Web Server...\n");
	while (1) {
		length = sizeof(client_addr);

		if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &length)) < 0) {
			perror("accept");
			exit(1);
		}

		if ((pid = fork()) < 0) {
			perror("fork");
			exit(1);
		} else {
			if (pid == 0) {
				close(server_sock);
				handle_socket(client_sock);	
			} else {
				close(client_sock);
			}
		}

	}

	return 0;

}
