#include <pthread.h>
#include <stdlib.h>

struct threadpool_task{
	void (*func)(void *); //函数指针
	void *arg; //参数
	struct threadpool_task *next; //指向下一个任务
};
struct threadpool_t{
	pthread_mutex_t lock; //互斥锁
	pthread_cond_t cond; //条件变量
	
	pthread_t* threads; //线程ID指针
	bool shutdown;//线程退出标志	
	threadpool_task *head; //指向任务队列头指针	
	int queue_size;	
	int threadNum;
};

threadpool_t *threadpool_init(int threadNum); //初始化
int threadpool_add(threadpool_t *pt, void (*func)(void*), void *arg); //生产者
int threadpool_free(threadpool_t *pool);
int threadpool_destroy(threadpool_t *pt);
int threadpool_gettasksize(threadpool_t* pool);
