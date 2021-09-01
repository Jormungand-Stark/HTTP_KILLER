#include"Socket.c"
#include<unistd.h>
#include<stdio.h>
#include<sys/param.h>
#include<rpc/types.h>
#include<getopt.h>
#include<strings.h>
#include<time.h>
#include<signal.h>
#include<string.h>
#include<error.h>

/* values */
volatile int time_alarm=0;
int speed=0;
int failed=0;
int bytes=0;

/* HTTP version */
int http_v=1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */

/* HTTP request method. Allow: GET, HEAD, OPTIONS, TRACE */
#define HTTP_GET 0
#define HTTP_HEAD 1
#define HTTP_OPTIONS 2
#define HTTP_TRACE 3
#define PROGRAM_VERSION "1.5"

/* 相关参数选项的默认值 */
int method=HTTP_GET; // 管理HTTP请求模式的指针
int clients=1; // 创建的客户端数量
int force=0; // 是否等待服务器响应，0：yes 1：no
int force_reload=0; // 是否重新请求加载 0：no 1：yes
int proxy_port=80; // HTTP服务默认访问80端口
char *proxy_host=NULL; // 默认无代理服务器，因此初值为空
int request_time=30; // 默认模拟请求时间

/* internal */
int mypipe[2]; // 父子进程通信管道
char host[MAXHOSTNAMELEN]; // 存放目标服务器的网络地址
#define REQUEST_SIZE 2048 // 2000个字符以内的URI可以满足所有主流浏览器的要求
char request[REQUEST_SIZE]; // 存放请求报文的字节流

/* short option */
/*
   struct option 
   { 
   const char *name;  
   int         has_arg;  
   int        *flag;  
   int         val;  
   };

   (1)name:表示选项的名称,比如daemon,dir,out等。

   (2)has_arg:表示选项后面是否携带参数。该参数有三个不同值，如下：
a:  no_argument(或者是0)时   ——参数后面不跟参数值，eg: --version,--help
b: required_argument(或者是1)时 ——参数输入格式为：--参数 值 或者 --参数=值。eg:--dir=/home
c: optional_argument(或者是2)时  ——参数输入格式只能为：--参数=值

(3)flag:这个参数有两个意思，空或者非空。
a:如果参数为空NULL，那么当选中某个长选项的时候，getopt_long将返回val值。
eg，可执行程序 --webbench，getopt_long的返回值为h.             
b:如果参数不为空，那么当选中某个长选项的时候，getopt_long将返回0，并且将flag指针参数指向val值。
eg: 可执行程序 --http-proxy=127.0.0.1:80 那么getopt_long返回值为0，并且lopt值为1。

(4)val：表示指定函数找到该选项时的返回值，或者当flag非空时指定flag指向的数据的值val。

全局变量：

(1)optarg：表示当前选项对应的参数值。

(2)optind：表示的是下一个将被处理到的参数在argv中的下标值。

(3)opterr：如果opterr = 0，在getopt、getopt_long、getopt_long_only遇到错误将不会输出错误信息到标准输出流。opterr在非0时，向屏幕输出错误。

*/ 
// 建立长选项和短选项的映射关系
static const struct option long_options[]=
{
        {"force", 0, &force, 1},
        {"reload", 0, &force_reload, 1},
        {"time", 1, NULL, 't'},
        {"help", 0, NULL, '?'},
        {"http09", 0, NULL, '9'},
        {"http10", 0, NULL, '1'},
        {"http11", 0, NULL,'2'},
        {"get", 0, &method, HTTP_GET},
        {"head", 0, &method, HTTP_HEAD},
        {"options", 0, &method, HTTP_OPTIONS},
        {"trace", 0, &method, HTTP_TRACE},
        {"version", 0, NULL, 'V'},
        {"version", 0, NULL, 'v'},
        {"proxy", 1, NULL, 'p'},
        {"clients", 1, NULL, 'c'},
        {NULL, 0, NULL, 0}
};

/*  Function declaration */
static void Killcore(const char* host,const int port, const char *request);
static int Kill(void); // fork创建子进程开始测压
static void build_request(const char *url); // 构造请求报文
static void alarm_solve(int signal) // 闹钟信号处理函数
{
        time_alarm=1;
}

/* 使用手册 */
static void usage_man(void)
{
        fprintf(stderr,
                        "webbench [option]... URL\n"
                        "  -f| --force               不等待服务器响应\n"
                        "  -r| --reload              重新请求加载（无缓存）\n"
                        "  -t| --time <sec>          运行时间，单位：秒，默认为30秒\n"
                        "  -p| --proxy <server:port> 使用代理服务器发送请求\n"
                        "  -c| --clients <n>         创建多少个客户端，默认为1个\n"
                        "  -9| --http09              Use HTTP/0.9 style requests.\n"
                        "  -1| --http10              Use HTTP/1.0 protocol.\n"
                        "  -2| --http11              Use HTTP/1.1 protocol.\n"
                        "  --get                     Use GET request method.\n"
                        "  --head                    Use HEAD request method.\n"
                        "  --options                 Use OPTIONS request method.\n"
                        "  --trace                   Use TRACE request method.\n"
                        "  -?| -h| --help            This information.\n"
                        "  -V| -v| --version         Display program version.\n"
               );
}

int main(int argc, char* argv[])
{
        int option_res = 0; // 输入选项
        int option_key = 0; // 参数对应下标
        char* cp = NULL; //

        // 没有输入选项
        if(argc == 1){
                usage_man();
                return 1;
        }

        // 有输入选项
        while((option_res = getopt_long(argc, argv, "frt:?912Vhvp:c:", long_options, &option_key)) != EOF){
                //printf("op_res: %d \n", option_res);
                switch(option_res){
                        case 'f': force = 1; break;
                        case 'r': force_reload = 1; break;
                        case '9': http_v = 0; break;
                        case '1': http_v = 1; break;
                        case '2': http_v = 2; break;
                        case 'V': 
                        case 'v': printf("The version of HTTP_KILLER is 1.0!\n"); exit(0);
                        case 't': request_time = atoi(optarg); break; // optarg 指向选项后的参数
                        case 'c': clients = atoi(optarg); break; 
                        case 'p': // 使用代理服务器，设置其代理网络号和端口号，格式：-p server:port
                                  cp = strrchr(optarg, ':'); break; //查找':'在optarg中最后出现的位置
                                  proxy_host = optarg;
                                  // 成功，返回该字符以及其后面的字符，如果失败，则返回 NULL。
                                  if(cp == NULL){ // 没有输入参数
                                          break;
                                  }
                                  if(cp == optarg){ // 端口号在optarg最开头,说明缺失主机名
                                          fprintf(stderr, "选项参数(--proxy)错误, %s:缺失主机名\n", optarg);
                                          break;
                                  }
                                  if(cp == optarg + strlen(optarg) - 1){ // ':'在optarg末位，说明缺少端口号
                                          fprintf(stderr, "选项参数(--proxy)错误, %s 缺少端口号\n", optarg);
                                  }
                                  *cp = '\0';
                                  proxy_port = atoi(cp + 1); // 把代理服务器端口号设置好
                                  break;
                        case '?': 
                        case 'h': usage_man(); exit(0);
                        default: printf("default!\n");usage_man(); return 2; break;
                }
        }

        // 选项参数解析完毕后，此时argv[optind]指向URL
        if(optind == argc){ // 没有输入URL
                fprintf(stderr, "缺少URL参数\n");
                usage_man();
                return 2;
        }

        if(clients==0) clients=1;
        if(request_time==0) request_time=30;

        fprintf(stderr, "HTTP_KILLER: 一款轻巧的网站测压工具 1.0 VERSION!\nGPL Open Source Software\n");

        /* 解析完命令行后，首先先构造http请求报文 */
        build_request(argv[optind]); // 参数是URL
        /* 请求报文构造好了 */
        printf("method: %d\n", method);
        printf("request: %s\n", request);
        printf("proxy_host: %s\n", proxy_host);

        /* 开始测压 */
        printf("\n       Try Killing：......\n");

        switch(method)
        {
                default:
                case HTTP_GET: printf("GET"); break;
                case HTTP_OPTIONS: printf("OPTIONS"); break;
                case HTTP_HEAD: printf("HEAD"); break;
                case HTTP_TRACE: printf("TRACE"); break;
        }

        printf(" %s", argv[optind]);

        switch(http_v)
        {
                case 0: printf(" (using HTTP/0.9)"); break;
                case 1: printf(" (using HTTP/1.0)"); break;
                case 2: printf(" (using HTTP/1.1)"); break;
        }

        printf("\n");

        printf("%d 个客户端", clients);

        printf(",running %d s", request_time);

        if (force) printf(",选择提前关闭连接");

        if (proxy_host != NULL) printf(",经由代理服务器 %s:%d   ", proxy_host, proxy_port);

        if (force_reload) printf(",选择无缓存");

        printf(".\n");
        /* 换行不能少！库函数是默认行缓冲，子进程会复制整个缓冲区
         * 若不换行刷新缓冲区，子进程会把缓冲区的的也打出来！
         * 而换行后缓冲区就刷新了，子进程的标准库函数的缓冲区就不会有前面的这些了 */

        /* 开始压力测试 */
        return Kill();
}

void build_request(const char* url) /* 构造请求报文 */
{
        printf("-----build_request-----\n");
        char carr[10];
        int i = 0;

        memset(host, 0, MAXHOSTNAMELEN); // 目标服务器网络地址清零
        memset(request, 0, REQUEST_SIZE); // 请求报文清零

        if (force_reload && proxy_host != NULL && http_v < 1) http_v = 1; // HEAD请求是http1.0后才有
        if (method == HTTP_HEAD && http_v < 1) http_v = 1; // HEAD请求是http1.0后才有
        if (method == HTTP_OPTIONS && http_v < 2) http_v = 2; // HEAD请求是http1.0后才有
        if (method == HTTP_TRACE && http_v < 2) http_v = 2;

        /* 开始填写http请求 */
        // 填写请求方法
        switch (method)
        {
                default:
                case HTTP_GET: strcpy(request, "GET"); break;
                case HTTP_HEAD: strcpy(request, "HEAD"); break;
                case HTTP_OPTIONS: strcpy(request, "OPTIONS"); break;
                case HTTP_TRACE: strcpy(request, "TRACE"); break;
        }

        strcat(request, " ");

        /* 判断URL的合法性 http://<host>:<port>/<path>?<searchpart> */
        // 1.URL中没有"://"
        if (strstr(url, "://") == NULL)
        {
                fprintf(stderr, "\n%s:是一个不合法的URL\n", url);
                exit(2);
        }

        // 2.URL过长
        if (strlen(url) > 1500)
        {
                fprintf(stderr, "URL is too long!\n");
                exit(2);
        }

        // 3.没有代理服务器
        if (proxy_host == NULL)   // 若无代理
        {
                if (strncasecmp("http://", url, 7) != 0)  // 忽略大小写比较前7位
                {
                        fprintf(stderr, "\n仅直接支持HTTP协议，其他协议需要选择代理服务器.\n");
                        usage_man();
                        exit(2);
                }
        }

        /* 定位url中主机名开始的位置 */
        // 比如  http://<host>:<port>/<path>?<searchpart>    
        i = strstr(url, "://") - url + 3; // 若str2是str1的子串，则返回str2在str1的首次出现的地址；如果str2不是str1的子串，则返回NULL。
        printf("i: %d\n", i); // i=7

        // 4.在主机名开始的位置找是否有'/'，若没有则非法
        if (strchr(url + i, '/') == NULL)
        {
                fprintf(stderr, "\nURL非法：<host>:<port>没有以'/'结尾\n");
                exit(2);
        }

        // 判断完URL合法性后继续填写URL到请求行

        // 无代理时
        if (proxy_host == NULL)
        {
                printf("---未设置代理服务器 proxy_host=NULL ---\n");
                // 有端口号时，填写端口号
                if (index(url + i, ':') != NULL && index(url, ':') < index(url, '/')) // 如果找到指定的字符则返回该字符所在地址，否则返回0.
                {
                        // 设置域名或IP
                        strncpy(host, url + i, strchr(url + i, ':') - url - i);

                        memset(carr, 0, 10);
                        strncpy(carr, index(url + i, ':') + 1, strchr(url + i, '/') - strchr(url + i, ':') - 1);

                        // 设置端口号
                        proxy_port = atoi(carr);
                        if (proxy_port == 0) proxy_port = 80; // 避免写了':'却没写端口号
                        printf("proxy_port: %d\n", proxy_port);
                }
                else    // 无端口号
                {
                        strncpy(host, url + i, strcspn(url + i, "/"));   // 将url+i到第一个”/"之间的字符复制到host中
                }
                printf("host: %s\n", host);
        }
        else    // 有代理服务器就简单了，直接填就行，不用自己处理
        {
                strcat(request, url);
        }

        // 将HTTP版本连接到request后面
        if (http_v == 1) strcat(request, " HTTP/1.0");
        if (http_v == 2) strcat(request, " HTTP/1.1");

        strcat(request, "\r\n");

        if (http_v > 0) strcat(request, "User-Agent:HTTP_KILLER 1.0\r\n"); // 添加程序版本号

        // 填写域名或IP
        if (proxy_host == NULL && http_v > 0)
        {
                strcat(request, "Host: ");
                strcat(request, host);
                strcat(request, "\r\n");
        }

        // 若选择强制重新加载，则填写无缓存
        if (force_reload && proxy_host != NULL)
        {
                strcat(request, "Pragma: no-cache\r\n");
        }

        // 我们目的是构造请求给网站，不需要传输任何内容，当然不必用长连接
        // 否则太多的连接维护会造成太大的消耗，大大降低可构造的请求数与客户端数
        // http1.1后是默认keep-alive的
        if (http_v > 1) strcat(request, "Connection: close\r\n");

        //填入空行后就构造完成了
        if (http_v > 0) strcat(request, "\r\n");
        // printf("request: %s \n",request);
}

static int Kill()
{
        printf("-----kill-----\n");
        int i = 0, j = 0; // j:用于辅助 参数failed 累加服务器每次发回的失败请求数
        long long k = 0; // 用于辅助 参数bytes 累加服务器每次发回的总字节数
        pid_t pid = 0;
        FILE* files = NULL; // 读/写端文件描述符

        // 尝试建立连接一次,i:socket文件描述符
        i = Socket(proxy_host == NULL ? host : proxy_host, proxy_port);  

        if (i < 0)
        {
                fprintf(stderr, "\n连接服务器失败，中断测试!a\n");
                return 3;
        }

        close(i); // 尝试连接成功了，关闭该连接

        // 建立父子进程通信的管道
        if (pipe(mypipe))
        {
                perror("通信管道建立失败");
                return 3;
        }

        // 让子进程去测试，建立多少个子进程进行连接由参数clients决定
        for (i = 0; i < clients; i++)
        {
                pid = fork();
                if (pid <= 0)
                {
                        sleep(1);
                        break;  
        // 失败或者子进程执行本语句都结束循环，否则该子进程可能继续fork了，显然不可以
                }
        }

        // 处理fork失败的情况
        if (pid < 0)
        {
                fprintf(stderr, "第 %d 子进程创建失败", i);
                perror("创建子进程失败");
                return 3;
        }

        // 子进程执行流
        if (pid == 0)
        {
                // 由子进程来发出请求报文
                Killcore(proxy_host == NULL ? host : proxy_host, proxy_port, request);

                // 子进程获得管道写端的文件描述符
                files = fdopen(mypipe[1], "w");
                if (files == NULL)
                {
                        perror("管道写端打开失败");
                        return 3;
                }

                // 向管道中写入该子进程在一定时间内 请求成功的次数 失败的次数 读取到的服务器回复的总字节数
                // fprintf(stderr,"Child - %d %d\n",speed,failed);
                fprintf(files, "%d %d %lld\n", speed, failed, bytes);
                fclose(files);  //关闭写端
                return 0;
        }
        else // 父进程执行流
        {
                // 父进程获得管道读端的文件描述符
                files = fdopen(mypipe[0], "r");

                if (files == NULL)
                {
                        perror("管道读端打开失败");
                        return 3;
                }

                // fopen标准IO函数是自带缓冲区的，
                // 我们的输入数据非常短，并且要求数据要及时，
                // 因此没有缓冲是最合适的
                // 我们不需要缓冲区
                // 因此把缓冲类型设置为_IONBF
                setvbuf(files, NULL, _IONBF, 0);

                speed = 0;  // 连接成功的总次数，后面除以时间可以得到速度
                failed = 0; // 失败的请求数
                bytes = 0;  // 服务器回复的总字节数

                // 唯一的父进程不停的读
                while (1)
                {
                        pid = fscanf(files, "%d %d %lld", &i, &j, &k); // 得到成功读入的参数个数
                        if (pid < 3)
                        {
                                fprintf(stderr, "某个子进程死亡\n");
                                break;
                        }

                        speed += i;
                        failed += j;
                        bytes += k;

                        // 我们创建了clients,正常情况下要读clients次
                        // fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid);
                        if (--clients == 0) break;
                }

                fclose(files);

                //统计处理结果
                printf("\n速度：%d pages/min,%d bytes/s.\n请求：%d 成功,%d 失败\n", \
                                (int)((speed + failed) / (request_time / 60.0f)), \
                                (int)(bytes / (float)request_time), \
                                speed, failed);
        }

        return i;
}

void Killcore(const char *host,const int port,const char *req)
{
        printf("-------killore-------\n");
        int rlen; // 记录req长度
        char buff[1500];
        int s, i; // s:socket文件描述符
        struct sigaction sa;

        /*  安装闹钟信号的处理函数 
         *  struct sigaction {
         *       void (*sa_handler)(int);
         *       void (*sa_sigaction)(int, siginfo_t *, void *);
         *       sigset_t sa_mask;
         *       int sa_flags;
         *       void (*sa_restorer)(void);
         *   };
         *   sa_handler此参数和signal()的参数handler相同，代表新的信号处理函数，其他意义请参考signal()。
         *   sa_mask 用来设置在处理该信号时暂时将sa_mask 指定的信号集搁置。
         *   sa_restorer 此参数没有使用。
         *   sa_flags 用来设置信号处理的其他相关操作，下列的数值可用:
         *      SA_RESETHAND：当调用信号处理函数时，将信号的处理函数重置为缺省值SIG_DFL
         *      SA_RESTART：如果信号中断了进程的某个系统调用，则系统自动启动该系统调用
         *      SA_NODEFER ：一般情况下， 当信号处理函数运行时，内核将阻塞该给定信号。但是如果设置了 SA_NODEFER标记， 那么在该信号处理函数运行时，内核将不会阻塞该信号
         */

        sa.sa_handler=alarm_solve;
        sa.sa_flags=0;
        if(sigaction(SIGALRM, &sa, NULL)) exit(3);

        alarm(request_time); // after request_time,then exit
        rlen=strlen(req);

        nextwork:while(1)
        {
                // 只有在收到闹钟信号后会使 time = 1
                // 即该子进程的工作结束了
                if(time_alarm)
                {
                        if(failed>0)
                        {
                                /* fprintf(stderr,"Correcting failed by signal\n"); */
                                failed--;
                        }
                        return;
                }

                // 建立到目标网站服务器的tcp连接，发送http请求
                s = Socket(host, port);
                if(s < 0) { failed++; continue;} // 请求失败数增加
                
                //发送请求报文, 但是未能把所有请求都写入
                if(rlen != write(s, req, rlen)) {failed++; close(s); continue;} 
                // 写操作结束记得关闭套接字

                /* http0.9的特殊处理
                 * 因为http0.9是在服务器回复后自动断开连接的，不keep-alive
                 * 在此可以提前彻底关闭套接字的写的一半，如果失败了那么肯定是个不正常的状态,
                 * 如果关闭成功则继续往后，因为可能还有需要接收服务器的回复内容
                 * 但是写这一半是一定可以关闭了，作为客户端进程上不需要再写了
                 * 因此我们主动破坏套接字的写端，但是这不是关闭套接字，关闭还是得close
                 * 事实上，关闭写端后，服务器没写完的数据也不会再写了，不过这个就不考虑了
                 */
                if(http_v == 0)
                        if(shutdown(s, 1)) {failed++; close(s); continue;}
                if(force == 0) // -f没有设置时, 默认等待服务器的回复
                {
                        /* read all available data from socket */
                        while(1)
                        {
                                if(time_alarm) break;
                                i = read(s, buff, 1500); // 读服务器发回的数据到buff中
                                /* fprintf(stderr,"%d\n",i); */
                                if(i<0) // 读操作失败
                                {
                                        failed++; // 请求失败数增加
                                        close(s); // 失败后一定要关闭套接字，不然失败个数多时会严重浪费资源
                                        goto nextwork; // 这次失败了那么继续下一次连接，与发出请求
                                }
                                else
                                        if(i==0) break; // 读完了
                                        else bytes+=i; // 统计服务器回复的字节数
                        }
                }
                //如果之前已经执行过close那么显然此时就是重复关闭了，会返回-1
                if(close(s)) {failed++; continue;}
                speed++; // 之前没有执行则返回0
        }
}
