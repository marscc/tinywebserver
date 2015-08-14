#include "http_request.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>

#define INDEX_FILE "index.htm"
extern struct epoll_event event;
extern int epollfd;

const char *index_file = "index.htm";

void init_request_t(http_request_t *request, int fd)
{
	request->sock = fd;
	request->read_pos = request->write_pos = 0;
	request->fd = -1;
	request->total_length = 0;
	request->test = 0;
}

void reset_request_t(http_request_t *request)
{
	request->read_pos = 0;
	request->write_pos = 0;
}
void cleanup_request_t(http_request_t *rt)
{
	int s;
	if(rt->sock != -1)
	{
		s = close(rt->sock);
		if(s == -1) cerr << "close sock" << endl;
	}
	if(rt->fd != -1)
	{
		s = close(rt->fd);
		if(s == -1) cerr << "close file" << endl;
	}
	rt->sock = -1;
	rt->fd = -1;
	reset_request_t(rt);
}
void handle_error(http_request_t *request, const char *error_string)
{
	cleanup_request_t(request);
}
void handle_response_code_400(http_request_t *request)
{
	request->response_code = 400;
	//request->status = STATUS_SEND_RESPONSE_HEADER;
	strncpy(request->buf, header_400, sizeof(header_400));
	do_response(request);
	return;
}
void handle_response_code_404(http_request_t *request)
{
	request->response_code = 404;
	//request->status = STATUS_SEND_RESPONSE_HEADER;
	strncpy(request->buf, header_404, sizeof(header_404));
	do_response(request);
	cerr << "not found" << endl;
	return;
}
void do_request(void *arg) //do_request完成后释不释放http_request_t结构，分长连接和短连接
{
	http_request_t *rt = (http_request_t *)arg;
	int sock = rt->sock;
	int s;
	char *buf = rt->buf;
	bool read_complete = false;
	int n;
	while(1)
	{
		n = read(sock, buf + rt->read_pos, rt->BUF_SIZE - rt->read_pos);
		cout << "已读字节数: " << n << endl;
		if(n == 0) //客户端关闭连接
		{
			cleanup_request_t(rt);
		}
		else if(n == -1)
		{
			if(errno != EAGAIN)
			{
				handle_error(rt, "read error");
				return;
			}
			else break;
		}
		else if(n > 0)
		{
			rt->read_pos += n;		
		}
	}
	int header_length = rt->read_pos;
	if(header_length > rt->BUF_SIZE)
	{
		handle_response_code_400(rt);
		return;
	}
	buf[header_length] = '\0';
	read_complete = ((strstr(buf, "\n\n") != 0) || (strstr(buf, "\r\n\r\n")) != 0);
	if(read_complete)
	{
		reset_request_t(rt);
		if(!strncmp(buf, "GET", 3) == 0)//非GET请求方式
		{
			
		}
		const char *n_loc = strchr(buf, '\n');
		const char *space_loc = strchr(buf + 4, ' ');
		if(n_loc <= space_loc)
		{
			handle_response_code_400(rt);
			return;
		}
		int path_len = space_loc - buf - 4;
		if(path_len > MAX_URL_LENGTH)
		{
			handle_response_code_400(rt);
			return;
		}
		
		char path[255];
		strncpy(path, buf + 4, path_len);
		path[path_len] = 0;
		
		struct stat filestat;
		char fullname[256];
		const char *prefix = doc_root;
		strncpy(fullname, prefix, strlen(prefix) + 1);
		strncpy(fullname + strlen(prefix), path, strlen(path) + 1);
		
		s = get_index_file(fullname, &filestat);
		if(s == -1)
		{
			handle_response_code_404(rt);
			return;		
		}
		int fd = open(fullname, O_RDONLY);
		rt->fd = fd;
		if(fd < 0)
		{
			handle_response_code_404(rt);
			return;	
		}
		else rt->response_code = 200;
		char tempstring[256];
		char *c = strstr(buf, HEADER_IF_MODIFIED_SINCE);
		if(c == NULL) cout << "not find HEADER_IF_MODIFIED_SINCE" << endl;
		if(c != NULL)
		{
			char *rn = strchr(c, '\r');
			if(rn == NULL)
			{
				rn = strchr(c, '\n');
				if(rn == NULL)
				{
					handle_response_code_400(rt);
					return;
				}
			}
			int time_len = rn - c - sizeof(HEADER_IF_MODIFIED_SINCE) + 1;
			strncpy(tempstring, c + sizeof(HEADER_IF_MODIFIED_SINCE) - 1, time_len);
			tempstring[time_len] = '\0';
			
			tm tm;
			time_t t;
			strptime(tempstring, RFC1123_DATE_FMT, &tm);
			tzset();
			t = mktime(&tm);
			t -= timezone;
			gmtime_r(&t, &tm);
			if(t >= filestat.st_mtime) rt->response_code = 304;      
		}
		//开始header
		rt->buf[0] = 0; //此时写buf
		if(rt->response_code == 304) write_to_header(rt, header_304_start);
		else write_to_header(rt, header_200_start);
		
		cout << "rt->buf: " << rt->buf << endl;
		
		rt->total_length = filestat.st_size;
		
		//写入当前时间
		tm tm_t;
		tm *tm;
		time_t tmt;
		tmt = time(NULL);
		tm = gmtime_r(&tmt, &tm_t);
		strftime(tempstring, sizeof(tempstring), RFC1123_DATE_FMT, tm);
		write_to_header(rt, "Date: ");
		write_to_header(rt, tempstring);
		write_to_header(rt, "\r\n");
		
		//写入文件修改时间
		tm = gmtime_r(&filestat.st_mtime, &tm_t); //st_mtime表示最后一次修改时间
		strftime(tempstring, sizeof(tempstring), RFC1123_DATE_FMT, tm);
		write_to_header(rt, "Last-Modified: ");
		write_to_header(rt, tempstring);
		write_to_header(rt, "\r\n");
		
		if(rt->response_code == 200)
		{	
			snprintf(tempstring, sizeof(tempstring),"Content-Length: %ld\r\n", filestat.st_size);
			write_to_header(rt, tempstring);
		}
	}
	//结束header
	write_to_header(rt, header_end);
	//将该http请求对象的状态变成发送响应头
	//rd->status = STATUS_SEND_RESPNSE_HEADER;
	//修改connfd的监听状态，改为监视写状态
	//event.events = EPOLLOUT | EPOLLET;
	//event.data.ptr = (void *)rt;
	//s = epoll_ctl(epollfd, EPOLL_CTL_MOD, rt->sock, &event);
	//if(s == -1)
	//{
	//	cerr << "epoll ctl" << endl;
	//	abort();
	//}
	void *arg1 =  (void *)rt;
	do_response(arg1);
}

void do_response(void *arg)
{
	http_request_t *rt = (http_request_t *)arg;
	if(rt->response_code != 200)
	{
		int bytes_written = write_all(rt, rt->buf + rt->write_pos, strlen(rt->buf) - rt->write_pos);
		if(bytes_written == (int)strlen(rt->buf) + rt->write_pos) cleanup_request_t(rt);
		else rt->write_pos += bytes_written;
	}
	else
	{
		int bytes_written = write_all(rt, rt->buf + rt->write_pos, strlen(rt->buf) - rt->write_pos);
		if(bytes_written == (int)strlen(rt->buf) + rt->write_pos)
		{
			//rt->status = STATUS_SEND_RESPONSE;
			send_response(rt);
		}
		else rt->write_pos += bytes_written;
	}
}

int write_all(http_request_t *rt, char *buf, int n)
{
	int done_write = 0;
	int total_bytes_write = 0;
	while(!done_write && total_bytes_write != n)
	{
		int bytes_write = write(rt->sock, buf + total_bytes_write, n - total_bytes_write);
		if(bytes_write == -1)
		{
			if(errno != EAGAIN)
			{
				handle_error(rt, "write");
				return 0;
			}
			else return total_bytes_write; //写缓冲区满
		}
		else total_bytes_write += bytes_write;
	}
	return total_bytes_write;
}

void send_response(http_request_t *rt)
{
	while(1)
	{
		off_t offset = rt->read_pos;
		int s = sendfile(rt->sock, rt->fd, &offset, rt->total_length - rt->read_pos);
		rt->read_pos = offset;
		if(s == -1)
		{
			if(errno != EAGAIN)
			{
				handle_error(rt, "sendfile");
				return;
			}
			else return; //写入缓冲区已满
		}
		if(rt->read_pos == rt->total_length)
		{
			cleanup_request_t(rt);
			return;
		}
	}	
}

int get_index_file(char *filename, struct stat *pstat)
{
	struct stat stat_buf;
	int s = lstat(filename, &stat_buf);
	if(s == -1) return -1;
	if(S_ISDIR(stat_buf.st_mode)) //是目录
	{
		strncpy(filename+strlen(filename), INDEX_FILE, sizeof(INDEX_FILE)); //注意为什么不能用常量字符串index_file
		s = lstat(filename, &stat_buf);
		if(s == -1 || S_ISDIR(stat_buf.st_mode))
		{
			int len = strlen(filename);
			filename[len] = 'l';
			filename[len + 1] = '\0';
			s = lstat(filename, &stat_buf);
			if(s == -1 || S_ISDIR(stat_buf.st_mode)) return -1;
		}
	}
	*pstat = stat_buf;
	return 0;
}

void write_to_header(http_request_t *request, const char *str)
{
	strncpy(request->buf + strlen(request->buf), str, strlen(str) + 1);
}
