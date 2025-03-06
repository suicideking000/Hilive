/*
 * Copyright (c) 2017 Liming Shao <lmshao@163.com>
 */

#ifndef HISILIVE_NETWORK_H
#define HISILIVE_NETWORK_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>



#if defined(WIN32) || defined(_WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>


#elif defined(__linux) || defined(__linux__) 
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#else
#endif


#define SERVER_PORT      8554
#define SERVER_RTP_PORT  55532
#define SERVER_RTCP_PORT 55533


typedef struct SOCkContext{
    char dstIp[16];//主机ip    
    int dstPort;//主机端口
    char srcIp[16];//从机ip
    int srcPort;//从机端口
    struct sockaddr_in servAddr;//从机地址
    int socketfd;//主机sockfd
    int socketrtpfd;//主机rtpsockfd
    int socketrtcpfd;//主机rtcpsockfd
    int clientfd;//从机sockfd
} SOCkContext;

//创建socket套接字
int createSocket_ctx(int protocol,SOCkContext *sockcontext);
//绑定socket到地址
int bindSocketAddr_ctx(SOCkContext *sockcontext);
//接受客户端的连接请求
int acceptClient_ctx(SOCkContext *sockcontext);
//创建socket套接字
int createSocket(int protocol);
//绑定socket到地址
int bindSocketAddr(int socketfd, const char *ip,int port);
//接受客户端的连接请求
int acceptClient(int socketfd, char*ip, int*port);

/*对客户端各种请求的处理*/
/*
*①C->S OPTION request           //询问S有哪些方法可用
* S->C OPTION response          //S回应信息的public头字段中包括提供的所有可用方法
*②C->S DESCRIBE request         //要求得到S提供的媒体描述信息
* S->C DESCRIBE response        //S回应媒体描述信息，一般是sdp信息
*③C->S SETUP request            //通过Transport头字段列出可接受的传输选项，请求S建立会话
* S->C SETUP response           //S建立会话，通过Transport头字段返回选择的具体转输选项，并返回建立的Session ID;
*④C->S PLAY request             //C请求S开始发送数据
* S->C PLAY response            //S回应该请求的信息
*⑤S->C 发送流媒体数据            //通过RTP协议传送数据
*⑥C->S EARDOWN request          //C请求关闭会话
* S->C TEARDOWN response        //S回应该请求
*/
//OPTION
int handleCmd_OPTIONS(char* result,int cseq);
//DESCRIBE
int handleCmd_DESCRIBE(char* result,int cseq,char* url);
//SETUP
int handleCmd_SETUP(char* result, int cseq, int clientRtpPort);
//PLAY
int handleCmd_PLAY(char* result, int cseq);


void doClient(SOCkContext *sockcontext);


#endif  // HISILIVE_NETWORK_H
