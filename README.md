# HTTP_KILLER
HTTP_KILLER是一个在Linux下使用的非常简单的网站压测工具。它使用fork()模拟多个客户端同时访问我们设定的URL，测试网站在压力下工作的性能，最多可以模拟3万个并发连接去测试网站的负载能力。

---
# 开发语言
C

# 开发环境
Debian 5.10.13-1kali1、vim、gcc、gdb、git

# 技术栈
套接字socket、HTTP协议、TCP/IP、fork多进程、getopt_long命令行解析函数、sigaction信号处理、write/read操作

# 选项参数

| 短参        | 长参数           | 作用   |
| ------------- |:-------------:| -----:|
|-f     |--force                |不等待服务器响应                 | 
|-r     |--reload               |发送重新加载请求（无缓存）        |
|-t     |--time <sec>           |运行时间，单位：秒，默认为30秒    |
|-p     |--proxy <server:port>  |使用代理服务器发送请求	          |
|-c     |--clients <n>          |创建多少个客户端，默认1个         |
|-9     |--http09               |使用 HTTP/0.9 协议来构造请求     |
|-1     |--http10               |使用 HTTP/1.0 协议来构造请求     |
|-2     |--http11               |使用 HTTP/1.1 协议来构造请求     |
|       |--get                  |使用 GET 请求方法                |
|       |--head                 |使用 HEAD 请求方法               |
|       |--options              |使用 OPTIONS 请求方法            |
|       |--trace                |使用 TRACE 请求方法              |
|-?/-h  |--help                 |打印帮助信息                     |
|-V/-v  |--version              |显示版本                        |
