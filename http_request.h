#include <stdlib.h>

#define MAX_URL_LENGTH 256
#define doc_root "/root/lemur/html"
#define HEADER_IF_MODIFIED_SINCE "If-Modified-Since: "
#define RFC1123_DATE_FMT "%a, %d %b %Y %H:%M:%S %Z"
#define header_end "\r\n"

#define header_400 "HTTP/1.1 400 Bad Request\r\nServer: clowwindyserver/1.0\r\n" \
    "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Bad request</h1>"
#define header_404 "HTTP/1.1 404 Not Found\r\nServer: clowwindyserver/1.0\r\n" \
    "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Not found</h1>"
#define header_200_start "HTTP/1.1 200 OK\r\nServer: clowwindyserver/1.0\r\n" \
    "Content-Type: text/html\r\nConnection: Close\r\n"
#define header_304_start "HTTP/1.1 304 Not Modified\r\nServer: clowwindyserver/1.0\r\n" \
    "Content-Type: text/html\r\nConnection: Close\r\n"

struct http_request_t{
	static const int BUF_SIZE = 4096;
	int test;
	int sock;
	//int status;
	int response_code;
	int fd;
	int read_pos;
	int write_pos;
	int total_length;
	char buf[BUF_SIZE];                                    
};

void init_request_t(http_request_t *request, int fd);
void reset_request_t(http_request_t *request);
void cleanup_request_t(http_request_t *request);
void handle_error(http_request_t *request, const char *error_string);
void handle_response_code_400(http_request_t *request);
void handle_response_code_404(http_request_t *request);
void do_request(void *arg);
void do_response(void *arg);
int write_all(http_request_t *rt, char *buf, int n);
void send_response(http_request_t *rt);
int get_index_file(char *filename, struct stat *pstat);
void write_to_header(http_request_t *request, const char *str);
