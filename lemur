#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <iostream>

using namespace std;

#define STATUS_READ_REQUEST_HEADER 0
#define STATUS_SEND_RESPONSE_HEADER 1
#define STATUS_SEND_RESPONSE 2
#define MAX_PROCESS 100
#define MAXEVENTS 100
#define SERV_PORT 3000
#define BUF_SIZE 4024
#define MAXCONN 128

struct process{
	int sock;
	int status;
	int response_code;
	int fd;
	int read_pos;
	int write_pos;
	int total_length;
	char buf[BUF_SIZE];                                    
};
static process processes[MAX_PROCESS];

void init_processes()
{
	for(int i = 0; i < MAX_PROCESS; i++) processes[i].sock = -1;
}

void read_request(process* process)
{}

process* find_process_by_sock_slow(int sock)
{
	for(int i = 0; i < MAX_PROCESS; i++)
		if(processes[i].sock == sock) return &processes[i];
	return 0;
}

process* find_process_by_sock(int sock)
{
	if(sock<MAX_PROCESS && sock >=0 && processes[sock].sock == sock) return &processes[sock];
	else return find_process_by_sock_slow(sock); //慢查找 O(n)时间复杂度 能否降低？？
}

process* find_empty_process_for_sock(int sock) {
  if (sock < MAX_PROCESS && sock >= 0 && processes[sock].sock == -1) {
    return &processes[sock];
  } else {
    return find_process_by_sock_slow(-1);
  }
}

void reset_process(process* process) {
  process->read_pos = 0;
  process->write_pos = 0;
}

int main(int argc, char *argv[])
{
	int listenfd, connectfd, epollfd;
	int flags, s;
	
	init_processes();
	struct sockaddr_in servaddr;
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT);
	
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	
	bind(listenfd, (sockaddr*)&servaddr, sizeof(servaddr));
	
	//设置监听套接字非阻塞
	flags = fcntl(listenfd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	flags = fcntl(listenfd, F_SETFL, flags);
	
	listen(listenfd, MAXCONN);
	
	struct epoll_event event;
	struct epoll_event *events;
	event.data.fd = listenfd;
	event.events = EPOLLIN | EPOLLET;
	
	epollfd = epoll_create1(0);//创建epoll实例
	
	epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event); //注册事件
	
	events = (epoll_event *)calloc(MAXEVENTS, sizeof(epoll_event));//calloc分配内存并初始化
	
	//创建线程池对象，并初始化
	//threadpool_t *pt = threadpool_init(4);
	
	while(1)
	{
		int n = epoll_wait(epollfd, events, MAXEVENTS, -1);
		for(int i = 0; i < n; i++)
		{
			if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
			{
				cerr << "epoll error" << endl;
				close(events[i].data.fd);
				continue;
			}
			else if(events[i].data.fd == listenfd) //新连接来到
			{
				while(1) //因为监听套接字也是边缘触发 故要读完所有已完成的连接，必须采用while
				{
					int connfd;
					struct sockaddr_in cliaddr;
					socklen_t len = sizeof(cliaddr);
					connfd = accept(listenfd, (sockaddr*)&cliaddr, &len);
					if(connfd == -1)
					{
						if((errno == EAGAIN) || (errno == EWOULDBLOCK)) break; //已经处理完所有来的连接
						else
						{
							cerr << "accept error" << endl;
							break;
						}
					}
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
					s = getnameinfo((sockaddr*)&cliaddr, len, hbuf, sizeof hbuf, sbuf, sizeof sbuf,
					            NI_NUMERICHOST | NI_NUMERICSERV);//<netdb.h>中
					if(s == 0)
						cout << "accepted connection on descriptor "<< connfd << " (host=" << hbuf
							 << ", port=" << sbuf << ")" << endl;
					//将普通已连接端口设置成非阻塞            
					flags = fcntl(connfd, F_GETFL, 0);
					flags |= O_NONBLOCK;
					flags = fcntl(connfd, F_SETFL, flags);
					
					// 需不需要设置监视sock的读取状态函数setsockopt ( infd, SOL_TCP, TCP_CORK, &on, sizeof ( on ) );
					
					event.data.fd = connfd;
					event.events = EPOLLIN | EPOLLET;
					epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &event);	
					
					process* process = find_empty_process_for_sock(connfd);	
					reset_process(process);
					process->sock = connfd;
					process->fd = -1;
					process->status = STATUS_READ_REQUEST_HEADER;	
				}
				continue;
			}
			else //普通的已连接描述符变为可读, 则将该描述符添加到任务队列中
			{
				//threadpool_add(tp, do_request, events[i].data.fd);
				process* process = find_process_by_sock(events[i].data.fd);
				if(process != NULL)
				{
					switch(process->status)
					{
						case STATUS_READ_REQUEST_HEADER:
							read_request(process);
							break;
						case STATUS_SEND_RESPONSE_HEADER:
							//send_response_header(process);
							break;
						case STATUS_SEND_RESPONSE:
							//send_response(process);
						default: break;
					}
				}
			}
		}
		//threadpool_destroy(tp);
	}
	free(events);
	close(listenfd);
	return 0;	
}
