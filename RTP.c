/*
 * Copyright (c) 2017 Liming Shao <lmshao@163.com>
 */

#include "RTP.h"
#include "Media.h"
#include "Network.h"
#include "Utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RTP_VERSION 2
#define RTP_H264 96

static SOCkContext *gsockcontext;


int initRTPMuxContext(RTPMuxContext *ctx)
{
    ctx->seq = 0;
    ctx->timestamp = 0;
    ctx->ssrc = 0x12345678;  // random number
    ctx->aggregation = 0;    // 1 use Aggregation Unit, 0 Single NALU Unit， default 0.
    ctx->buf_ptr = ctx->buf;
    ctx->payload_type = 0;  // 0, H.264/AVC; 1, HEVC/H.265
    return 0;
}


void rtpHeaderInit(struct RtpPacket* rtpPacket, uint8_t csrcLen, uint8_t extension,
    uint8_t padding, uint8_t version, uint8_t payloadType, uint8_t marker,
    uint16_t seq, uint32_t timestamp, uint32_t ssrc)
{
    rtpPacket->rtpHeader.csrcLen = csrcLen;
    rtpPacket->rtpHeader.extension = extension;
    rtpPacket->rtpHeader.padding = padding;
    rtpPacket->rtpHeader.version = version;
    rtpPacket->rtpHeader.payloadType = payloadType;
    rtpPacket->rtpHeader.marker = marker;
    rtpPacket->rtpHeader.seq = seq;
    rtpPacket->rtpHeader.timestamp = timestamp;
    rtpPacket->rtpHeader.ssrc = ssrc;
}
int rtpSendPacketOverTcp(int clientSockfd, struct RtpPacket* rtpPacket, uint32_t dataSize)
{

    rtpPacket->rtpHeader.seq = htons(rtpPacket->rtpHeader.seq);
    rtpPacket->rtpHeader.timestamp = htonl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = htonl(rtpPacket->rtpHeader.ssrc);

    uint32_t rtpSize = RTP_HEADER_SIZE + dataSize;
    char* tempBuf = (char *)malloc(4 + rtpSize);
    tempBuf[0] = 0x24;//$
    tempBuf[1] = 0x00;
    tempBuf[2] = (uint8_t)(((rtpSize) & 0xFF00) >> 8);
    tempBuf[3] = (uint8_t)((rtpSize) & 0xFF);
    memcpy(tempBuf + 4, (char*)rtpPacket, rtpSize);

    int ret = send(clientSockfd, tempBuf, 4 + rtpSize, 0);

    rtpPacket->rtpHeader.seq = ntohs(rtpPacket->rtpHeader.seq);
    rtpPacket->rtpHeader.timestamp = ntohl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = ntohl(rtpPacket->rtpHeader.ssrc);

    free(tempBuf);
    tempBuf = NULL;

    return ret;
}

int rtpSendPacketOverUdp(int serverRtpSockfd, const char* ip, int16_t port, struct RtpPacket* rtpPacket, uint32_t dataSize)
{
    
    struct sockaddr_in addr;
    int ret;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    rtpPacket->rtpHeader.seq = htons(rtpPacket->rtpHeader.seq);//从主机字节顺序转变成网络字节顺序
    rtpPacket->rtpHeader.timestamp = htonl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = htonl(rtpPacket->rtpHeader.ssrc);

    ret = sendto(serverRtpSockfd, (char *)rtpPacket, dataSize + RTP_HEADER_SIZE, 0,
        (struct sockaddr*)&addr, sizeof(addr));

    rtpPacket->rtpHeader.seq = ntohs(rtpPacket->rtpHeader.seq);
    rtpPacket->rtpHeader.timestamp = ntohl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = ntohl(rtpPacket->rtpHeader.ssrc);

    return ret;
}

int udpSend(const SOCkContext *udp, const uint8_t *data, uint32_t len)
{
    ssize_t num = sendto(udp->socketrtpfd, data, len, 0, (struct sockaddr *)&udp->servAddr, sizeof(udp->servAddr));
    if (num != len) {
        LOGE("sendto %s. %d %u socket[%d]\n", strerror(errno), (int)num, len, udp->socketrtpfd);
        return -1;
    }
    printf("send a nal!\n");

    return len;
}

// enc RTP packet
void rtpSendData(RTPMuxContext *ctx, const uint8_t *buf, int len, int mark)
{
    int res = 0;
    /* build the RTP header */
    /*
     *
     *    0                   1                   2                   3
     *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                           timestamp                           |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |           synchronization source (SSRC) identifier            |
     *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
     *   |            contributing source (CSRC) identifiers             |
     *   :                             ....                              :
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *
     **/

    uint8_t *pos = ctx->cache;
    pos[0] = (RTP_VERSION << 6) & 0xff;                            // V P X CC
    pos[1] = (uint8_t)((RTP_H264 & 0x7f) | ((mark & 0x01) << 7));  // M PayloadType
    Load16(&pos[2], (uint16_t)ctx->seq);                           // Sequence number
    Load32(&pos[4], ctx->timestamp);
    Load32(&pos[8], ctx->ssrc);

    /* copy av data */
    memcpy(&pos[12], buf, len);

    res = udpSend(gsockcontext, ctx->cache, (uint32_t)(len + 12));
    if (res <= 0) {
        LOGE("udpSend error %d\n", res);
    }
    // LOG("\n rtpSendData cache [%d]: ", res);
    // for (int i = 0; i < 4; ++i) {
    //     LOG("%.2X ", ctx->cache[i]);
    // }
    // LOG(" timestamp %d\n", ctx->timestamp);

    memset(ctx->cache, 0, RTP_PAYLOAD_MAX + 10);
    ctx->buf_ptr = ctx->buf;  // restore buf_ptr
    ctx->seq = (ctx->seq + 1) & 0xffff;
}

// 拼接NAL头部 在 ctx->buff, 然后ff_rtp_send_data
static void rtpSendNAL(RTPMuxContext *ctx, const uint8_t *nal, int size, int last)
{
    // Single NAL Packet or Aggregation Packets
    if (size <= RTP_PAYLOAD_MAX) {
        // Aggregation Packets
        if (ctx->aggregation) {
            /*
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *  |STAP-A NAL HDR | NALU 1 Size | NALU 1 HDR & Data | NALU 2 Size | NALU 2 HDR & Data | ... |
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *
             * */
            int buffered_size = (int)(ctx->buf_ptr - ctx->buf);  // size of data in ctx->buf
            uint8_t curNRI = (uint8_t)(nal[0] & 0x60);           // NAL NRI

            // The remaining space in ctx->buf is less than the required space
            if (buffered_size + 2 + size > RTP_PAYLOAD_MAX) {
                rtpSendData(ctx, ctx->buf, buffered_size, 0);
                buffered_size = 0;
            }

            /*
             *    STAP-A/AP NAL Header
             *     +---------------+
             *     |0|1|2|3|4|5|6|7|
             *     +-+-+-+-+-+-+-+-+
             *     |F|NRI|  Type   |
             *     +---------------+
             * */
            if (buffered_size == 0) {
                *ctx->buf_ptr++ = (uint8_t)(24 | curNRI);  // 0x18
            } else {
                uint8_t lastNRI = (uint8_t)(ctx->buf[0] & 0x60);
                if (curNRI > lastNRI) {  // if curNRI > lastNRI, use new curNRI
                    ctx->buf[0] = (uint8_t)((ctx->buf[0] & 0x9F) | curNRI);
                }
            }

            // set STAP-A/AP NAL Header F = 1, if this NAL F is 1.
            ctx->buf[0] |= (nal[0] & 0x80);

            // NALU Size + NALU Header + NALU Data
            Load16(ctx->buf_ptr, (uint16_t)size);  // NAL size
            ctx->buf_ptr += 2;
            memcpy(ctx->buf_ptr, nal, size);  // NALU Header & Data
            ctx->buf_ptr += size;

            // meet last NAL, send all buf
            if (last == 1) {
                rtpSendData(ctx, ctx->buf, (int)(ctx->buf_ptr - ctx->buf), 1);
            }
        }
        // Single NAL Unit RTP Packet
        else {
            /*
             *   0 1 2 3 4 5 6 7 8 9
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             *  |F|NRI|  Type   | a single NAL unit ... |
             *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
             * */
            rtpSendData(ctx, nal, size, last);
        }

    } else {  // 分片分组
        /*
         *
         *  0                   1                   2
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * | FU indicator  |   FU header   |   FU payload   ...  |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *
         * */
        if (ctx->buf_ptr > ctx->buf) {
            // if (ctx->buf_ptr < ctx->buf + 10000)
            LOGE("send left data %d", ctx->buf_ptr > ctx->buf);
            rtpSendData(ctx, ctx->buf, (int)(ctx->buf_ptr - ctx->buf), 0);
            ctx->buf_ptr = ctx->buf;  // restore buf_ptr
        }

        int headerSize;
        uint8_t *buff = ctx->buf;
        uint8_t type = nal[0] & 0x1F;
        uint8_t nri = nal[0] & 0x60;

        /*
         *     FU Indicator
         *    0 1 2 3 4 5 6 7
         *   +-+-+-+-+-+-+-+-+
         *   |F|NRI|  Type   |
         *   +---------------+
         * */
        buff[0] = 28;  // FU Indicator; FU-A Type = 28
        buff[0] |= nri;

        /*
         *      FU Header
         *    0 1 2 3 4 5 6 7
         *   +-+-+-+-+-+-+-+-+
         *   |S|E|R|  Type   |
         *   +---------------+
         * */
        buff[1] = type;     // FU Header uses NALU Header
        buff[1] |= 1 << 7;  // S(tart) = 1
        headerSize = 2;
        size -= 1;
        nal += 1;

        while (size + headerSize > RTP_PAYLOAD_MAX) {
            memcpy(&buff[headerSize], nal, (size_t)(RTP_PAYLOAD_MAX - headerSize));
            rtpSendData(ctx, buff, RTP_PAYLOAD_MAX, 0);
            nal += RTP_PAYLOAD_MAX - headerSize;
            size -= RTP_PAYLOAD_MAX - headerSize;
            buff[1] &= ~(1 << 7);  // buff[1] & 0111111, S(tart) = 0
        }
        buff[1] |= 1 << 6;  // buff[1] | 01000000, E(nd) = 1
        memcpy(&buff[headerSize], nal, size);
        rtpSendData(ctx, buff, size + headerSize, last);
    }
}

// 从一段H264流中，查询完整的NAL发送，直到发送完此流中的所有NAL
void rtpSendH264HEVC(RTPMuxContext *ctx, SOCkContext *sockcontext, const uint8_t *buf, int size)
{
    const uint8_t *r;
    const uint8_t *end = buf + size;
    gsockcontext = sockcontext;

    if (NULL == ctx || NULL == sockcontext || NULL == buf || size <= 0) {
        printf("rtpSendH264HEVC param error.\n");
        return;
    }

    r = ff_avc_find_startcode11(buf, end);
    while (r < end) {
        const uint8_t *r1;
        while (!*(r++))
            ;  // skip current startcode

        r1 = ff_avc_find_startcode11(r, end);  // find next startcode
        // send a NALU (except NALU startcode), r1 == end indicates this is the last NALU
        rtpSendNAL(ctx, r, (int)(r1 - r), r1 == end);
        r = r1;
    }
}
void rtpSendH264HEVC_2(RtpPacket *pkt, SOCkContext *sockcontext, const uint8_t *buf, int size)
{
    const uint8_t *r;
    const uint8_t *end = buf + size;
    gsockcontext = sockcontext;

    if (NULL == pkt || NULL == sockcontext || NULL == buf || size <= 0) {
        printf("rtpSendH264HEVC param error.\n");
        return;
    }

    r = ff_avc_find_startcode11(buf, end);
    while (r < end) {
        const uint8_t *r1;
        while (!*(r++))
            ;  // skip current startcode

        r1 = ff_avc_find_startcode11(r, end);  // find next startcode
        // send a NALU (except NALU startcode), r1 == end indicates this is the last NALU
        rtpSendNAL_2(pkt, r, (int)(r1 - r));
        r = r1;
    }
}


int rtpSendNAL_2(struct RtpPacket* rtpPacket,char* frame, uint32_t frameSize)
    {
 
    uint8_t naluType; // nalu第一个字节
    int sendBytes = 0;
    int ret;

    naluType = frame[0];

    printf("frameSize=%d \n", frameSize);

    if (frameSize <= RTP_MAX_PKT_SIZE) // nalu长度小于最大包长：单一NALU单元模式
    {

         //*   0 1 2 3 4 5 6 7 8 9
         //*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         //*  |F|NRI|  Type   | a single NAL unit ... |
         //*  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

        memcpy(rtpPacket->payload, frame, frameSize);
        //ret = rtpSendPacketOverUdp(gsockcontext->socketrtpfd, gsockcontext->srcIp, gsockcontext->srcPort, rtpPacket, frameSize);
        ret = rtpSendPacketOverTcp(gsockcontext->socketrtpfd, rtpPacket, RTP_MAX_PKT_SIZE+2);
        if(ret < 0)
            return -1;

        rtpPacket->rtpHeader.seq++;
        sendBytes += ret;
        if ((naluType & 0x1F) == 7 || (naluType & 0x1F) == 8) // 如果是SPS、PPS就不需要加时间戳
            goto out;
    }
    else // nalu长度小于最大包场：分片模式
    {

         //*  0                   1                   2
         //*  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
         //* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         //* | FU indicator  |   FU header   |   FU payload   ...  |
         //* +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+



         //*     FU Indicator
         //*    0 1 2 3 4 5 6 7
         //*   +-+-+-+-+-+-+-+-+
         //*   |F|NRI|  Type   |
         //*   +---------------+



         //*      FU Header
         //*    0 1 2 3 4 5 6 7
         //*   +-+-+-+-+-+-+-+-+
         //*   |S|E|R|  Type   |
         //*   +---------------+


        int pktNum = frameSize / RTP_MAX_PKT_SIZE;       // 有几个完整的包
        int remainPktSize = frameSize % RTP_MAX_PKT_SIZE; // 剩余不完整包的大小
        int i, pos = 1;

        // 发送完整的包
        for (i = 0; i < pktNum; i++)
        {
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            rtpPacket->payload[1] = naluType & 0x1F;

            if (i == 0) //第一包数据
                rtpPacket->payload[1] |= 0x80; // start
            else if (remainPktSize == 0 && i == pktNum - 1) //最后一包数据
                rtpPacket->payload[1] |= 0x40; // end

            memcpy(rtpPacket->payload+2, frame+pos, RTP_MAX_PKT_SIZE);
            //ret = rtpSendPacketOverUdp(gsockcontext->socketrtpfd, gsockcontext->srcIp, gsockcontext->srcPort, rtpPacket, RTP_MAX_PKT_SIZE+2);
            ret = rtpSendPacketOverTcp(gsockcontext->socketrtpfd, rtpPacket, RTP_MAX_PKT_SIZE+2);
            if(ret < 0)
                return -1;

            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
            pos += RTP_MAX_PKT_SIZE;
        }

        // 发送剩余的数据
        if (remainPktSize > 0)
        {
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            rtpPacket->payload[1] = naluType & 0x1F;
            rtpPacket->payload[1] |= 0x40; //end

            memcpy(rtpPacket->payload+2, frame+pos, remainPktSize+2);
            //ret = rtpSendPacketOverUdp(gsockcontext->socketrtpfd, gsockcontext->srcIp, gsockcontext->srcPort, rtpPacket, remainPktSize+2);
            ret = rtpSendPacketOverTcp(gsockcontext->socketrtpfd, rtpPacket, remainPktSize+2);
            if(ret < 0)
                return -1;

            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
        }
    }
    rtpPacket->rtpHeader.timestamp += 90000 / 25;
    out:

    return sendBytes;
}
