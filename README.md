##一款linux下支持高并发的轻量级web服务器

###lemur的特点
- 采用时间循环+非阻塞IO+线程池的解决方案
- 支持GET/POST/HEAD请求
- 支持CGI功能

####编译

环境：ubuntu10.04
```
g++ -o lemur.cpp -c lemur.o
g++ -o http_request.cpp -c http_request.o
g++ -o threadpool.cpp -c threadpool.o
g++ -o lemur lemur.o http_request.o threadpool.o -lpthread
```
####运行
```
./lemur
```

