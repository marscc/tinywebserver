#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include "threadpool.h"
#include "http_request.h"

#include <iostream>

using namespace std;

#define MAX_PROCESS 100
#define MAXEVENTS 1024
#define SERV_PORT 3000
#define MAXCONN 128

struct epoll_event event;
int epollfd;
struct epoll_event *events;

int main(int argc, char *argv[])
{
	int listenfd, connectfd;
	int flags, s;
	
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
	
	http_request_t *ptr =  (http_request_t *)malloc(sizeof(http_request_t)); 
	init_request_t(ptr, listenfd);
	event.data.ptr = (void *)ptr; //用了event.data.ptr就不要再用event.data.fd
	event.events = EPOLLIN | EPOLLET;
	
	epollfd = epoll_create1(0);//创建epoll实例
	
	epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event); //注册事件
	
	events = (epoll_event *)calloc(MAXEVENTS, sizeof(epoll_event));//calloc分配内存并初始化events数组
	
	//创建线程池对象，并初始化 pt指向堆中分配的threadpool_t对象
	threadpool_t *pt = threadpool_init(4);
	
	cout << "listenfd: " << listenfd << endl;
	while(1)
	{
		int n = epoll_wait(epollfd, events, MAXEVENTS, -1);
		//cout << "n: " << n << endl;
		int sock;
		for(int i = 0; i < n; i++)
		{
			http_request_t *r = (http_request_t *)events[i].data.ptr;
			sock = r->sock;
			//cout << "r->sock: " << r->sock << endl;
			if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
			{
				cerr << "epoll error" << endl;
				close(sock);
				continue;
			}
			else if(sock == listenfd) //新连接来到
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
					
					//需不需要设置监视sock的读取状态函数setsockopt ( infd, SOL_TCP, TCP_CORK, &on, sizeof ( on ) );	
					
					//给已连接的请求头分配空间
					http_request_t *request = (http_request_t *)malloc(sizeof(http_request_t));
					if(request == NULL) cerr << "request alloc error" << endl;
					
					init_request_t(request, connfd);					
					
					//将要添加事件的时候关联自定义数据 event.data.fd不要管
					event.data.ptr = (void *)request;
					event.events = EPOLLIN | EPOLLET;
					epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &event);
					
					cout << "accept..." << endl;		
				}
			}
			else if(events[i].events & EPOLLIN) 
			{
				printf("fd: %d\n", (*((http_request_t *)events[i].data.ptr)).sock);
				threadpool_add(pt, do_request, events[i].data.ptr);
			}
			//else if(events[i].events & EPOLLOUT)
			//{
			//	printf("fd: %d\n", (*((http_request_t *)events[i].data.ptr)).sock);
			//	threadpool_add(pt, do_response, events[i].data.ptr);
			//} 
		}
	}
	threadpool_destroy(pt);
	free(ptr);
	free(events);
	close(listenfd);
	return 0;	
}
