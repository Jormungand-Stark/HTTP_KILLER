/***********************************************************************
  module:       socket.c
  program:      popclient
  SCCS ID:      @(#)socket.c    1.5  4/1/94
  programmer:   Virginia Tech Computing Center/XIANG-LEE
  compiler:     DEC RISC C compiler (Ultrix 4.1)
  environment:  DEC Ultrix 4.3 
  description:  UNIX sockets code.
 ***********************************************************************/
 
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int Socket(const char *host, int clientPort)
{
    int sock; // socket文件描述符
    unsigned long inaddr; // 保存网络字节序整数形式的 IPv4 地址
    struct sockaddr_in ad; // IPv4 地址结构体
    bzero(&ad, sizeof(ad)); // 内容置零
    struct hostent *hp; // 主机信息结构体
    
    ad.sin_family = AF_INET; // 地址族

    inaddr = inet_addr(host); // 点分十进制字符串转换成网络字节序整数
    if (inaddr != INADDR_NONE) // 转换未失败, *host格式正确
        memcpy(&ad.sin_addr, &inaddr, sizeof(inaddr)); // 将 inaddr 地址中内容拷贝到 ad 结构体的 sin_addr 结构体参数中
        /* inet_pton(AF_INET, host, &ad.sin_addr); */
    else // 转换失败
    {
        hp = gethostbyname(host); // 成功：返回对应于给定主机名的包含主机名字和地址信息的hostent结构指针
        if (hp == NULL) // 失败：返回一个空指针
            return -1;
        memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
    }
    ad.sin_port = htons(clientPort); // 端口号主机字节序（小端）转为网络字节序（大端）
    
    sock = socket(AF_INET, SOCK_STREAM, 0); // 创建socket，文件描述符为sock
    if (sock < 0) return sock; // 创建失败
        
    int con = connect(sock, (struct sockaddr *)&ad, sizeof(ad));
    if (con < 0) return con; // 创建成功但连接失败
        
    return sock;
}
