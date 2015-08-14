#include "threadpool.h"

static void *threadpool_worker(void *arg);
int threadpool_free(threadpool_t *pool);

threadpool_t *threadpool_init(int threadNum)
{
	threadpool_t *pool;
	if((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) return NULL;
	//执行初始化
	pthread_mutex_init(&(pool->lock), NULL);
	pthread_cond_init(&(pool->cond), NULL);
	
	//头指针
	pool->head = (threadpool_task *)malloc(sizeof(threadpool_task));
	pool->head->func = NULL;
	pool->head->arg = NULL;
	pool->head->next = NULL;
	
	pool->queue_size = 0;
	pool->shutdown = false;
	pool->threadNum = threadNum;
	
	pool->threads = (pthread_t *)malloc(threadNum*(sizeof(pthread_t)));
	if(pool->threads == NULL) cerr << "pool->threads" << endl;
	for(int i = 0; i < threadNum; i++)
		pthread_create(&(pool->threads[i]), NULL, threadpool_worker, (void *)pool);		
	return pool;
}
//生产者建立一个任务并添加到任务队列中
int threadpool_add(threadpool_t *pool, void (*func)(void *), void *arg)
{
	threadpool_task *task = (threadpool_task *)malloc(sizeof(threadpool_task)); //分配一个任务对象
	if(task == NULL) cerr << "task" << endl;
	task->func = func;
	task->arg = arg;
	
	pthread_mutex_lock(&(pool->lock));
	//头插法添加任务到任务队列中
	task->next = pool->head->next;
	pool->head->next = task;
	pool->queue_size++;
	pthread_mutex_unlock(&pool->lock);
	//发送信号唤醒工作线程
	pthread_cond_signal(&(pool->cond));
	pthread_mutex_unlock(&pool->lock);
	return 0;
}
//消费者处理线程
static void *threadpool_worker(void *arg) //为什么要声明为static
{
	threadpool_t *pool = (threadpool_t *)arg;
	threadpool_task *task;
	pthread_t tid = pthread_self();
	while(1)
	{
		pthread_mutex_lock(&pool->lock); //加锁
		while(pool->queue_size == 0 && !pool->shutdown) //任务队列为空且没有退出线程的命令
			pthread_cond_wait(&(pool->cond), &(pool->lock)); //解锁-wait-收到信号-加锁-返回
		if(pool->shutdown)
		{
			pthread_mutex_unlock(&(pool->lock)); 
			pthread_exit(NULL);
		}
		task = pool->head->next;
		if(task == NULL) continue;
		pool->head->next = task->next;
		pool->queue_size--;
		
		task->func(task->arg);//如果放在unlock之前，如果这四个线程都在执行函数，如果有新连接来到，执行add函数，分配一个任务后，将阻塞在add的加锁那里，因为此时锁还未释放
		pthread_mutex_unlock(&(pool->lock));
		free(task);	//结束任务后释放该任务所占的空间
	}
	pthread_mutex_unlock(&(pool->lock));
	pthread_exit(NULL);
	return NULL;
}

int threadpool_destroy(threadpool_t* pool) 
{
	if(pool->shutdown) return -1; //线程池所有线程都已经退出
	
	pool->shutdown = true;
	pthread_cond_broadcast(&(pool->cond)); //阻塞在wait那的线程会收到该消息 然后执行while循环条件不满足，pool->shutdown已经置一，故该线会退出
	for(int i = 0; i < pool->threadNum; i++) pthread_join(pool->threads[i], NULL); //等待子线程退出的信息
	
	pthread_mutex_destroy(&(pool->lock));
	pthread_cond_destroy(&(pool->cond));
	
	threadpool_free(pool);
	return 0;
}

int threadpool_free(threadpool_t *pool)
{
	if(pool == NULL) return -1;
	//释放用于存储线程ID的结构
	if(pool->threads) 
	{
		free(pool->threads); 
		pool->threads = NULL;
	}
	//释放任务队列中的任务
	threadpool_task *tmp;
	while(pool->head->next)
	{
		tmp = pool->head->next;
		pool->head->next = tmp->next;
		free(tmp);
	}
	//释放头结点
	free(pool->head);
	
	//释放线程池对象
	free(pool);
	return 0;
}
int threadpool_gettasksize(threadpool_t* pool)
{
	return pool->queue_size;
}
