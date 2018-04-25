#ifdef WINDOWS
#include <Winsock2.h>
#include <ws2tcpip.h>
#include "realpath.h"
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>
#include <limits.h>
#include <sys/epoll.h>

#include "libhttp.h"
#include "wq.h"
#include "thpool.h"

#define MAX_PATH_SIZE PATH_MAX
#define MAX_RES_BUFF_SIZE 2048

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads = 4;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;

DIR *pdir;

void response_404(int fd)
{
	http_start_response(fd, 404);
	http_send_header(fd, "Content-Type", "text/html");
	http_end_headers(fd);
	http_send_string(fd,
			 "<center>"
			 "<h1> 404 </h1>"
			 "<hr>"
			 "<p>Nothing's here</p>"
			 "</center>");
}

void response_with_file(int fd, char *abs_path, int code)
{
	char buf[MAX_RES_BUFF_SIZE];
	http_start_response(fd, code);
	http_send_header(fd, "Content-Type", http_get_mime_type(abs_path));
	http_end_headers(fd);
	FILE *pfd = fopen(abs_path, "rb");
	if (pfd) {
		size_t nread;
		while ((nread = fread(buf, 1, MAX_RES_BUFF_SIZE, pfd)) > 0) {
			http_send_data(fd, buf, nread);
		}
	}
}

void response_with_list(int fd, char *real_path, char *cur_dir, int code)
{
	struct dirent *de;
	DIR *dir = opendir(real_path);
	if (dir == NULL)
		return;

	http_start_response(fd, code);
	http_send_header(fd, "Content-Type", "text/html");
	http_end_headers(fd);
	http_send_string(fd, "<h1>");
	http_send_string(fd, cur_dir);
	http_send_string(fd, ": </h1>");

	// list all files:
	while((de = readdir(dir)) != NULL) {
		http_send_string(fd, "<li><a href=\"");
		http_send_string(fd, cur_dir);
		int len = strlen(cur_dir);
		if (len > 0 && cur_dir[len-1] != '/')
			http_send_string(fd, "/");
		http_send_string(fd, de->d_name);
		http_send_string(fd, "\">");
		http_send_string(fd, de->d_name);
		http_send_string(fd, "</a></li>");
	}
}

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */
void handle_files_request(int fd)
{

	/*
	 * TODO: Your solution for Task 1 goes here! Feel free to delete/modify *
	 * any existing code.
	 */
	DEBUG_FUNC(printf, ("%lu handling a request\n", pthread_self()));
	struct http_request *request = http_request_parse(fd);

	/* invalid request*/
	if (!request) {
		DEBUG_FUNC(printf, ("%lu gets an invalid request \n", pthread_self()));

		/* get */
	}else if (strcmp(request->method, "GET") == 0) {
		DEBUG_FUNC(printf, ("%lu responsing : %s %s\n", pthread_self(), request->method, request->path));
		//get the abs path
		char *real_path = (char*)malloc(MAX_PATH_SIZE+1);
		real_path[MAX_PATH_SIZE] = 0;
		char *abs_path = (char*)malloc(MAX_PATH_SIZE+1);
		abs_path[MAX_PATH_SIZE] = 0;
		sprintf(abs_path, "%s%s", server_files_directory, request->path);

		realpath(abs_path, real_path);
		DEBUG_FUNC(printf, ("%lu real_path is : %s\n", pthread_self(), real_path));

		struct stat sb;
		if (stat(abs_path, &sb) == -1) {
			response_404(fd);

			/* request a file */
		} else if ((sb.st_mode & S_IFMT) == S_IFREG) {
			response_with_file(fd, abs_path, 200);

			/* request a dir */
		}else if ((sb.st_mode & S_IFMT) == S_IFDIR) {
			// try index
			char* abs_path_indx = (char*)malloc(MAX_PATH_SIZE);
			sprintf(abs_path_indx, "%s/index.html", real_path);
			if (stat(abs_path_indx, &sb) == -1 || (sb.st_mode & S_IFMT) != S_IFREG) {
				//index.html does not exist, response with file list
				response_with_list(fd, real_path, request->path, 200);

			} else { // index exists
				response_with_file(fd, abs_path_indx, 200);
			}

			free(abs_path_indx);
		}

		free(real_path);
		free(abs_path);

		/*other method*/
	} else {
		DEBUG_FUNC(printf, ("%lu responsing an unknown method to fd:%d \n", pthread_self(), fd));
		response_404(fd);
	}

/*
   http_start_response(fd, 200);
   http_send_header(fd, "Content-Type", "text/html");
   http_end_headers(fd);
   http_send_string(fd,
      "<center>"
      "<h1>Welcome to httpserver!</h1>"
      "<hr>"
      "<p>Nothing's here yet.</p>"
      "</center>");
 */
}

static int make_socket_non_blocking (int sfd)
{
  int flags, s;

  flags = fcntl (sfd, F_GETFL, 0);
  if (flags == -1)
    {
      perror ("fcntl");
      return -1;
    }

  flags |= O_NONBLOCK;
  s = fcntl (sfd, F_SETFL, flags);
  if (s == -1)
    {
      perror ("fcntl");
      return -1;
    }

  return 0;
}

void do_proxy(int fd1, int fd2) {
	struct epoll_event ev, events[2];
	int nfds, epollfd;

	if (make_socket_non_blocking(fd1) || make_socket_non_blocking(fd2)) {
		perror("cannot make blocking");
		return;
	}

	epollfd = epoll_create1(0);
	if (epollfd == -1) {
	   perror("epoll_create1");
	   return;
	}

	ev.events = EPOLLIN;
	ev.data.fd = fd1;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd1, &ev) == -1) {
       perror("epoll_ctl: listen_sock");
			 close(epollfd);
       return;
   }
	 ev.data.fd = fd2;
 	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd2, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
 			 close(epollfd);
        return;
    }

		char ok = 1;
		while(ok) {
			nfds = epoll_wait(epollfd, events, 2, -1);
			DEBUG_FUNC(printf, ("epoll wake up\n"));
			if (nfds == -1) {
				return;
			}
			for (int i=0; i<nfds; ++i) {
				DEBUG_FUNC(printf, ("gets an event\n"));
				size_t cnt;
				char buf[512]; // buf for read an write

				if (events[i].data.fd == fd1 && (events[i].events&EPOLLIN)) {
					cnt = read(fd1, buf, sizeof(buf));
					if (cnt == -1) {
						perror("read fd1");
						ok = 0;
						break;
					} else if (cnt == 0) {
						ok = 0;
						break;
					}
					cnt = write(fd2, buf, cnt);
					if (cnt == -1) {
						perror("write to fd2");
						ok = 0;
						break;
					}
					DEBUG_FUNC(printf, ("succesfully send data to fd2\n"));

				} else if (events[i].data.fd == fd2 && (events[i].events&EPOLLIN)){
					cnt = read(fd2, buf, sizeof(buf));
					if (cnt == -1) {
						perror("read fd2");
						ok = 0;
						break;
					} else if (cnt == 0) {
						ok = 0;
						break;
					}
					cnt = write(fd1, buf, cnt);
					if (cnt == -1) {
						perror("write to fd1");
						ok = 0;
						break;
					}
					DEBUG_FUNC(printf, ("succesfully send data to fd1\n"));

				} else {
					DEBUG_FUNC(printf, ("gets an unknown event\n"));
					continue;
				}
			}
		}
		close(epollfd);
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd)
{

	/*
	 * The code below does a DNS lookup of server_proxy_hostname and
	 * opens a connection to it. Please do not modify.
	 */

	struct sockaddr_in target_address;

	memset(&target_address, 0, sizeof(target_address));
	target_address.sin_family = AF_INET;
	target_address.sin_port = htons(server_proxy_port);
#ifdef WINDOWS
    struct hostent *target_dns_entry = gethostbyname(server_proxy_hostname);
#else
	struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);
#endif

	int client_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
	if (client_socket_fd == -1) {
		fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
		exit(errno);
	}

	if (target_dns_entry == NULL) {
		fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
		exit(ENXIO);
	}

	char *dns_address = target_dns_entry->h_addr_list[0];

	memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
	int connection_status = connect(client_socket_fd, (struct sockaddr*)&target_address,
					sizeof(target_address));

	if (connection_status < 0) {
		/* Dummy request parsing, just to be compliant. */
		http_request_parse(fd);

		http_start_response(fd, 502);
		http_send_header(fd, "Content-Type", "text/html");
		http_end_headers(fd);
		http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
		return;

	} else {
		do_proxy(fd, client_socket_fd);
 	 	close(client_socket_fd);
	}
}

void handle_files_request_wraper(void *p_fd)
{
	handle_files_request(*(int*)p_fd);
	close(*(int*)p_fd);
}

void handle_proxy_request_wraper(void *p_fd)
{
	handle_proxy_request(*(int*)p_fd);
	close(*(int*)p_fd);
}


int init_thread_pool(int num_threads, void (*request_handler)(void *))
{
	/*
	 * TODO: Part of your solution for Task 2 goes here!
	 */
	return thpool_init(num_threads, 8192);

}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(void *))
{
#ifdef WINDOWS
	WSADATA wsa;
    printf("\nInitialising Winsock...");
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
    {
        printf("Failed. Error Code : %d",WSAGetLastError());
        return 1;
    }
    printf("Initialised.\n");
#endif

	struct sockaddr_in server_address, client_address;
	size_t client_address_length = sizeof(client_address);
	int client_socket_number;

	*socket_number = socket(PF_INET, SOCK_STREAM, 0);
	if (*socket_number == -1) {
		perror("Failed to create a new socket");
		exit(errno);
	}

#ifdef WINDOWS
    char socket_option = 1;
#else
	int socket_option = 1;
#endif
	if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
		       sizeof(socket_option)) == -1) {
		perror("Failed to set socket options");
		exit(errno);
	}

	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = INADDR_ANY;
	server_address.sin_port = htons(server_port);

	if (bind(*socket_number, (struct sockaddr *)&server_address,
		 sizeof(server_address)) == -1) {
		perror("Failed to bind on socket");
		exit(errno);
	}

	if (listen(*socket_number, 1024) == -1) {
		perror("Failed to listen on socket");
		exit(errno);
	}

	printf("Listening on port %d...\n", server_port);

	if (init_thread_pool(num_threads, request_handler)) {
		perror("Error creating thread pool");
	}else  {
		while (1) {
			client_socket_number = accept(*socket_number,
						      (struct sockaddr *)&client_address,
						      (socklen_t*)&client_address_length);
			if (client_socket_number < 0) {
				perror("Error accepting socket");
				continue;
			}

			printf("Accepted connection from %s on port %d\n",
			       inet_ntoa(client_address.sin_addr),
			       client_address.sin_port);

			// TODO: Change me?
			int *p_fd = (int*)malloc(sizeof(int));
			*p_fd = client_socket_number;
			thpool_add_job(request_handler, p_fd);
		}

		thpool_destroy();
	}

#ifdef WINDOWS
    shutdown(*socket_number, SD_BOTH);
#else
	shutdown(*socket_number, SHUT_RDWR);
#endif
	close(*socket_number);
}

#ifdef WINDOWS
char *strsignal(int sig) {
    static char str_signal_buf[20];
    sprintf(str_signal_buf, "SIG_%d", sig);
    return str_signal_buf;
}
#endif

int server_fd = 0;
void signal_callback_handler(int signum)
{
	printf("Caught signal %d: %s\n", signum, strsignal(signum));
	printf("Closing socket %d\n", server_fd);
	if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
	exit(0);
}

char *USAGE =
	"Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
	"       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage()
{
	fprintf(stderr, "%s", USAGE);
	exit(EXIT_SUCCESS);
}




int main(int argc, char **argv)
{
	signal(SIGINT, signal_callback_handler);
	setbuf(stdout, NULL);

	/* Default settings */
	server_port = 8000;
	void (*request_handler)(void *) = NULL;

	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp("--files", argv[i]) == 0) {
			request_handler = handle_files_request_wraper;
			free(server_files_directory);
			server_files_directory = argv[++i];
			if (!server_files_directory) {
				fprintf(stderr, "Expected argument after --files\n");
				exit_with_usage();
			}
			pdir = opendir(server_files_directory);
			if (pdir == NULL) {
				perror("Directory specified not valid!");
				exit(EXIT_FAILURE);
			}

		} else if (strcmp("--proxy", argv[i]) == 0) {
			request_handler = handle_proxy_request_wraper;

			char *proxy_target = argv[++i];
			if (!proxy_target) {
				fprintf(stderr, "Expected argument after --proxy\n");
				exit_with_usage();
			}

			char *colon_pointer = strchr(proxy_target, ':');
			if (colon_pointer != NULL) {
				*colon_pointer = '\0';
				server_proxy_hostname = proxy_target;
				server_proxy_port = atoi(colon_pointer + 1);
			} else {
				server_proxy_hostname = proxy_target;
				server_proxy_port = 80;
			}
		} else if (strcmp("--port", argv[i]) == 0) {
			char *server_port_string = argv[++i];
			if (!server_port_string) {
				fprintf(stderr, "Expected argument after --port\n");
				exit_with_usage();
			}
			server_port = atoi(server_port_string);
		} else if (strcmp("--num-threads", argv[i]) == 0) {
			char *num_threads_str = argv[++i];
			if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
				fprintf(stderr, "Expected positive integer after --num-threads\n");
				exit_with_usage();
			}
		} else if (strcmp("--help", argv[i]) == 0) {
			exit_with_usage();
		} else {
			fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
			exit_with_usage();
		}
	}

	if (server_files_directory == NULL && server_proxy_hostname == NULL) {
		fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
			"                      \"--proxy [HOSTNAME:PORT]\"\n");
		exit_with_usage();
	}

	serve_forever(&server_fd, request_handler);

	return EXIT_SUCCESS;
}
