/*
 * Copyright (c) 2017 Liming Shao <lmshao@163.com>
 */

#include "Network.h"
#include "Utils.h"



int createSocket_ctx(int protocol,SOCkContext *sockcontext)
{
    int on=1;
    if(protocol==IPPROTO_TCP)
    {
        sockcontext->socketfd=socket(AF_INET,SOCK_STREAM,protocol);
        if(sockcontext->socketfd<0)
         {
            printf("LINE:%d,create socketfd failed!\n",__LINE__);
            return -1;
        }
    }
    else if(protocol==IPPROTO_UDP)
    {
        sockcontext->socketfd=socket(AF_INET,SOCK_DGRAM,protocol);
        if(sockcontext->socketfd<0)
         {
            printf("LINE:%d,create socketfd failed!\n",__LINE__);
            return -1;
        }
    }
    setsockopt(sockcontext->socketfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return 0;
}

int bindSocketAddr_ctx(SOCkContext *sockcontext)
{
    struct sockaddr_in addr;
    addr.sin_family= AF_INET;//使用ipv4
    addr.sin_port= htons(sockcontext->dstPort);//htones将主机u_short转化为ip端口
    addr.sin_addr.s_addr=inet_addr(sockcontext->dstIp);

    if(bind(sockcontext->socketfd,(struct sockaddr*)&addr,sizeof(struct sockaddr))<0)
    {
        printf("LINE:%d,bind socketfd to addr failed!\n",__LINE__);
        return -1;
    }
    return 0;
}

int acceptClient_ctx(SOCkContext *sockcontext)
{   
    socklen_t len = 0;
    memset(&sockcontext->servAddr,0,sizeof(sockcontext->servAddr));
    len = sizeof(sockcontext->servAddr);
    sockcontext->clientfd = accept(sockcontext->socketfd, (struct sockaddr *)&sockcontext->servAddr, &len);
    if(sockcontext->clientfd < 0)
    {
        printf("LINE:%d,accept client failed!\n",__LINE__);
        return -1;
    }
    strcpy(sockcontext->srcIp,inet_ntoa(sockcontext->servAddr.sin_addr));//从机地址
    sockcontext->srcPort = ntohs(sockcontext->servAddr.sin_port);//从机端口
    sockcontext->servAddr.sin_family = AF_INET;
    sockcontext->servAddr.sin_port = htons(sockcontext->srcPort);
    inet_aton(sockcontext->srcIp, &sockcontext->servAddr.sin_addr);
    return 0;

}

int createSocket(int protocol)
{
    int socketfd;
    int on=1;
    if(protocol==IPPROTO_TCP)
    {
        socketfd=socket(AF_INET,SOCK_STREAM,protocol);
        if(socketfd<0)
         {
            printf("LINE:%d,create socketfd failed!\n",__LINE__);
            return -1;
        }
    }
    else if(protocol==IPPROTO_UDP)
    {
        socketfd=socket(AF_INET,SOCK_DGRAM,protocol);
        if(socketfd<0)
         {
            printf("LINE:%d,create socketfd failed!\n",__LINE__);
            return -1;
        }
    }
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return socketfd;
}

int bindSocketAddr(int socketfd, const char *ip,int port)
{
    struct sockaddr_in addr;
    addr.sin_family= AF_INET;//使用ipv4
    addr.sin_port= htons(port);//htones将主机u_short转化为ip端口
    addr.sin_addr.s_addr=inet_addr(ip);

    if(bind(socketfd,(struct sockaddr*)&addr,sizeof(struct sockaddr))<0)
    {
        printf("LINE:%d,bind socketfd to addr failed!\n",__LINE__);
        return -1;
    }
    return 0;
}

int acceptClient(int socketfd, char*ip, int*port)
{
    int clientfd;
    socklen_t len = 0;
    struct sockaddr_in addr;
    
    memset(&addr,0,sizeof(addr));
    len = sizeof(addr);

    clientfd =accept(socketfd, (struct sockaddr*)&addr, &len);
    if(clientfd<0)
    {
        printf("LINE:%d,accept client failed!\n",__LINE__);
        return -1;
    }

    strcpy(ip,inet_ntoa(addr.sin_addr)); //客户端地址
    *port =ntohs(addr.sin_port);//客户端端口
    return clientfd;
}
/*sample*/
/*___________________________________________________
*|C>>>>>>>>>>>>>>>>>>>>>>>>>S START
*|Request: OPTIONS rtsp://127.0.0.1:8554 RTSP/1.0\r\n
*|CSeq: 1\r\n
*|User-Agent: Lavf61.9.100\r\n
*|\r\n
*|C>>>>>>>>>>>>>>>>>>>>>>>>>S END
*▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
*___________________________________________________
*|S>>>>>>>>>>>>>>>>>>>>>>>>>>C START
*|Response: RTSP/1.0 200 OK\r\n
*|CSeq: 1\r\n
*|CMD list:OPTIONS,DESCRIBE,SETUP,PLAY\r\n
*|\r\n
*|S>>>>>>>>>>>>>>>>>>>>>>>>>>C END
*▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
*/
int handleCmd_OPTIONS(char* result,int cseq)
{
    sprintf(result,"RTSP/1.0 200 OK\r\n"
            "CSeq: %d\r\n"
            "CMD list:OPTIONS,DESCRIBE,SETUP,PLAY\r\n"
            "\r\n",
            cseq);
    return 0;
}
/*sample*/
/*___________________________________________________
*|C>>>>>>>>>>>>>>>>>>>>>>>>>S START
*|Request: DESCRIBE rtsp://127.0.0.1:8554 RTSP/1.0\r\n
*|Accept: application/sdp\r\n
*|CSeq: 2\r\n
*|User-Agent: Lavf61.9.100\r\n
*|\r\n
*|C>>>>>>>>>>>>>>>>>>>>>>>>>S END
*▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
*___________________________________________________
*|S>>>>>>>>>>>>>>>>>>>>>>>>>>C START
*|Response: RTSP/1.0 200 OK\r\n
*|CSeq: 2\r\n
*|Content-Base: rtsp://127.0.0.1:8554\r\n
*|Content-type: application/sdp
*|Content-length: 125
*|\r\n
*|
*|
*|Session Description Protocol Version (v): 0
*|Owner/Creator, Session Id (o): - 91729845456 1 IN IP4 127.0.0.1
*|Time Description, active time (t): 0 0
*|Session Attribute (a): control:*
*|Media Description, name and address (m): video 0 RTP/AVP 96
*|Media Attribute (a): rtpmap:96 H264/90000
*|Media Attribute (a): control:track0
*|S>>>>>>>>>>>>>>>>>>>>>>>>>>C END
*▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
*/
int handleCmd_DESCRIBE(char* result,int cseq,char* url)
{
    char sdp[500];
    char localIp[100];

    sscanf(url, "rtsp://%[^:]:", localIp);

    sprintf(sdp, "v=0\r\n"
        "o=- 9%ld 1 IN IP4 %s\r\n"
        "t=0 0\r\n"
        "a=control:*\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=control:track0\r\n",
        time(NULL), localIp);

    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
        "Content-Base: %s\r\n"
        "Content-type: application/sdp\r\n"
        "Content-length: %zu\r\n\r\n"
        "%s",
        cseq,
        url,
        strlen(sdp),
        sdp);

    return 0;
}
/*sample*/
/*___________________________________________________
*|C>>>>>>>>>>>>>>>>>>>>>>>>>S START
*|Request: SETUP rtsp://127.0.0.1:8554/track0 RTSP/1.0\r\n
*|Transport: RTP/AVP/UDP;unicast;client_port=12216-12217
*|CSeq: 3\r\n
*|User-Agent: Lavf61.9.100\r\n
*|\r\n
*|C>>>>>>>>>>>>>>>>>>>>>>>>>S END
*▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
*___________________________________________________
*|S>>>>>>>>>>>>>>>>>>>>>>>>>>C START
*|Response: RTSP/1.0 200 OK\r\n
*|CSeq: 3\r\n
*|Transport: RTP/AVP;unicast;client_port=12216-12217;server_port=55532-55533
*|Session: 66334873
*|\r\n
*|S>>>>>>>>>>>>>>>>>>>>>>>>>>C END
*▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
*/
int handleCmd_SETUP(char* result, int cseq, int clientRtpPort)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
        "Session: 66334873\r\n"
        "\r\n",
        cseq,
        clientRtpPort,
        clientRtpPort + 1,
        SERVER_RTP_PORT,
        SERVER_RTCP_PORT);

    return 0;
}
/*sample*/
/*___________________________________________________
*|C>>>>>>>>>>>>>>>>>>>>>>>>>S START
*|Request: PLAY rtsp://127.0.0.1:8554 RTSP/1.0\r\n
*|CSeq: 4\r\n
*|User-Agent: Lavf61.9.100\r\n
*|Session: 66334873
*|\r\n
*|C>>>>>>>>>>>>>>>>>>>>>>>>>S END
*▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
*___________________________________________________
*|S>>>>>>>>>>>>>>>>>>>>>>>>>>C START
*|Response: RTSP/1.0 200 OK\r\n
*|CSeq: 4\r\n
*|Range: npt=0.000-\r\n
*|Session: 66334873; timeout=10
*|\r\n
*|S>>>>>>>>>>>>>>>>>>>>>>>>>>C END
*▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
*/
int handleCmd_PLAY(char* result, int cseq)
{
    sprintf(result, "RTSP/1.0 200 OK\r\n"
        "CSeq: %d\r\n"
        "Range: npt=0.000-\r\n"
        "Session: 66334873; timeout=10\r\n\r\n",
        cseq);

    return 0;
}


