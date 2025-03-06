#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>

#include "Network.h"
#include "RTP.h"
#include "Utils.h"
#include "sample_comm.h"
#include "comm.h"
#include "rtsp_demo.h"

#ifdef HI_FPGA
    #define PIC_SIZE   PIC_1080P
#else
    #define PIC_SIZE   PIC_3840x2160
#endif
static HI_U32 g_u32DefaultClkCfg = 0;
static HI_U64 g_u64VeduClkAddr = 0x04510164;

typedef enum {
    MODE_FILE,
    MODE_RTP
} RunMode;

typedef struct {
    RunMode mode;                // -m
    int frameRate;               // -f
    int bitRate;                 // -b
    char ip[16];                 // -i
    PAYLOAD_TYPE_E videoFormat;  // -e
    PIC_SIZE_E videoSize;        // -s
} ParamOption;
rtsp_demo_handle demo = NULL;
rtsp_session_handle session= NULL;
int ts;
ParamOption gParamOption;
static RTPMuxContext gRTPCtx;
static SOCkContext gUDPCtx;
struct RtpPacket* rtpPacket = NULL;
static pthread_t gMediaProcPid;
static SAMPLE_VENC_GETSTREAM_PARA_S gMediaProcPara;

extern HI_S32 SAMPLE_SYS_SetReg(HI_U64 u64Addr, HI_U32 u32Value);
extern HI_S32 SAMPLE_SYS_GetReg(HI_U64 u64Addr, HI_U32 *pu32Value);

VENC_GOP_MODE_E SAMPLE_VENC_GetGopMode(void);
SAMPLE_RC_E SAMPLE_VENC_GetRcMode(void);
HI_S32 SAMPLE_VENC_SYS_Init(HI_U32 u32SupplementConfig,SAMPLE_SNS_TYPE_E  enSnsType);
HI_S32 SAMPLE_VENC_CheckSensor(SAMPLE_SNS_TYPE_E   enSnsType,SIZE_S  stSize);
HI_S32 SAMPLE_VENC_VI_Init( SAMPLE_VI_CONFIG_S *pstViConfig, HI_BOOL bLowDelay, HI_U32 u32SupplementConfig);
HI_S32 SAMPLE_VENC_VPSS_Init(VPSS_GRP VpssGrp, HI_BOOL* pabChnEnable, DYNAMIC_RANGE_E enDynamicRange,PIXEL_FORMAT_E enPixelFormat,SIZE_S stSize,SAMPLE_SNS_TYPE_E enSnsType);
HI_S32 SAMPLE_VENC_H264(void);
HI_S32 Hilive_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, char *szFilePostfix);
HI_S32 Hilive_COMM_VENC_StartGetStream(VENC_CHN VeChn);
HI_VOID *Hilive_COMM_VENC_GetVencStreamProc(HI_VOID *p);
HI_S32 Hilive_COMM_VENC_SaveStream(FILE *pFd, VENC_STREAM_S *pstStream);
HI_S32 Hilive_RTPSendVideo(VENC_STREAM_S *pstStream);

int Hilive_ParseParam(int argc, char **argv)
{
    gParamOption.mode = MODE_RTP;
    gParamOption.frameRate = 30;  // fps
    gParamOption.bitRate = 0;     // kbps
    sprintf(gParamOption.ip, "%s", "192.168.1.122");
    gParamOption.videoSize = PIC_400x400;
    gParamOption.videoFormat = PT_H264;  // H.264
    return 0;

}

VENC_GOP_MODE_E SAMPLE_VENC_GetGopMode(void)
{
    char c;
    VENC_GOP_MODE_E enGopMode = 0;

Begin_Get:

    printf("please input choose gop mode!\n");
    printf("\t 0) NORMALP.\n");
    printf("\t 1) DUALP.\n");
    printf("\t 2) SMARTP.\n");
    printf("\t 3) BIPREDB\n");

    while((c = getchar()) != '\n' && c != EOF)
    switch(c)
    {
        case '0':
            enGopMode = VENC_GOPMODE_NORMALP;
            break;
        case '1':
            enGopMode = VENC_GOPMODE_DUALP;
            break;
        case '2':
            enGopMode = VENC_GOPMODE_SMARTP;
            break;
        case '3':
            enGopMode = VENC_GOPMODE_BIPREDB;
            break;
        default:
            SAMPLE_PRT("input rcmode: %c, is invaild!\n",c);
            goto Begin_Get;
    }

    return enGopMode;
}


SAMPLE_RC_E SAMPLE_VENC_GetRcMode(void)
{
    char c;
    SAMPLE_RC_E  enRcMode = 0;

Begin_Get:

    printf("please input choose rc mode!\n");
    printf("\t c) cbr.\n");
    printf("\t v) vbr.\n");
    printf("\t a) avbr.\n");
    printf("\t f) fixQp\n");

    while((c = getchar()) != '\n' && c != EOF)
    switch(c)
    {
        case 'c':
            enRcMode = SAMPLE_RC_CBR;
            break;
        case 'v':
            enRcMode = SAMPLE_RC_VBR;
            break;
        case 'a':
            enRcMode = SAMPLE_RC_AVBR;
            break;
        case 'f':
            enRcMode = SAMPLE_RC_FIXQP;
            break;
        default:
            SAMPLE_PRT("input rcmode: %c, is invaild!\n",c);
            goto Begin_Get;
    }
    return enRcMode;
}


HI_S32 SAMPLE_VENC_SYS_Init(HI_U32 u32SupplementConfig,SAMPLE_SNS_TYPE_E  enSnsType)
{
    HI_S32 s32Ret;
    HI_U64 u64BlkSize;
    VB_CONFIG_S stVbConf;
    PIC_SIZE_E enSnsSize;
    SIZE_S     stSnsSize;

    memset(&stVbConf, 0, sizeof(VB_CONFIG_S));

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    u64BlkSize = COMMON_GetPicBufferSize(stSnsSize.u32Width, stSnsSize.u32Height, PIXEL_FORMAT_YVU_SEMIPLANAR_422, DATA_BITWIDTH_8, COMPRESS_MODE_SEG,DEFAULT_ALIGN);
    stVbConf.astCommPool[0].u64BlkSize   = u64BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt    = 15;

    u64BlkSize = COMMON_GetPicBufferSize(1920, 1080, PIXEL_FORMAT_YVU_SEMIPLANAR_422, DATA_BITWIDTH_8, COMPRESS_MODE_SEG,DEFAULT_ALIGN);
    stVbConf.astCommPool[1].u64BlkSize   = u64BlkSize;
    stVbConf.astCommPool[1].u32BlkCnt    = 15;

    stVbConf.u32MaxPoolCnt = 2;

    if(0 == u32SupplementConfig)
    {
        s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    }
    else
    {
        s32Ret = SAMPLE_COMM_SYS_InitWithVbSupplement(&stVbConf,u32SupplementConfig);
    }
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    return HI_SUCCESS;
}


HI_S32 SAMPLE_VENC_VI_Init( SAMPLE_VI_CONFIG_S *pstViConfig, HI_BOOL bLowDelay, HI_U32 u32SupplementConfig)
{
    HI_S32              s32Ret;
    SAMPLE_SNS_TYPE_E   enSnsType;
    ISP_CTRL_PARAM_S    stIspCtrlParam;
    HI_U32              u32FrameRate;


    enSnsType = pstViConfig->astViInfo[0].stSnsInfo.enSnsType;

    pstViConfig->as32WorkingViId[0]                           = 0;
    //pstViConfig->s32WorkingViNum                              = 1;

    pstViConfig->astViInfo[0].stSnsInfo.MipiDev            = SAMPLE_COMM_VI_GetComboDevBySensor(pstViConfig->astViInfo[0].stSnsInfo.enSnsType, 0);

    //pstViConfig->astViInfo[0].stDevInfo.ViDev              = ViDev0;
    pstViConfig->astViInfo[0].stDevInfo.enWDRMode          = WDR_MODE_NONE;

    if(HI_TRUE == bLowDelay)
    {
        pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode     = VI_ONLINE_VPSS_ONLINE;
    }
    else
    {
        pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode     = VI_OFFLINE_VPSS_OFFLINE;
    }
    s32Ret = SAMPLE_VENC_SYS_Init(u32SupplementConfig,enSnsType);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Init SYS err for %#x!\n", s32Ret);
        return s32Ret;
    }

    //if(8k == enSnsType)
    //{
    //    pstViConfig->astViInfo[0].stPipeInfo.enMastPipeMode       = VI_PARALLEL_VPSS_OFFLINE;
    //}

    //pstViConfig->astViInfo[0].stPipeInfo.aPipe[0]          = ViPipe0;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[1]          = -1;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[2]          = -1;
    pstViConfig->astViInfo[0].stPipeInfo.aPipe[3]          = -1;

    //pstViConfig->astViInfo[0].stChnInfo.ViChn              = ViChn;
    //pstViConfig->astViInfo[0].stChnInfo.enPixFormat        = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    //pstViConfig->astViInfo[0].stChnInfo.enDynamicRange     = enDynamicRange;
    pstViConfig->astViInfo[0].stChnInfo.enVideoFormat      = VIDEO_FORMAT_LINEAR;
    pstViConfig->astViInfo[0].stChnInfo.enCompressMode     = COMPRESS_MODE_SEG;//COMPRESS_MODE_SEG;
    s32Ret = SAMPLE_COMM_VI_SetParam(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_SetParam failed with %d!\n", s32Ret);
        return s32Ret;
    }

    SAMPLE_COMM_VI_GetFrameRateBySensor(enSnsType, &u32FrameRate);

    s32Ret = HI_MPI_ISP_GetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_GetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }
    stIspCtrlParam.u32StatIntvl  = u32FrameRate/30;

    s32Ret = HI_MPI_ISP_SetCtrlParam(pstViConfig->astViInfo[0].stPipeInfo.aPipe[0], &stIspCtrlParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_ISP_SetCtrlParam failed with %d!\n", s32Ret);
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_VI_StartVi(pstViConfig);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_COMM_SYS_Exit();
        SAMPLE_PRT("SAMPLE_COMM_VI_StartVi failed with %d!\n", s32Ret);
        return s32Ret;
    }

    return HI_SUCCESS;
}


HI_S32 SAMPLE_VENC_VPSS_Init(VPSS_GRP VpssGrp, HI_BOOL* pabChnEnable, DYNAMIC_RANGE_E enDynamicRange,PIXEL_FORMAT_E enPixelFormat,SIZE_S stSize,SAMPLE_SNS_TYPE_E enSnsType)
{
    HI_S32 i;
    HI_S32 s32Ret;
    PIC_SIZE_E      enSnsSize;
    SIZE_S          stSnsSize;
    VPSS_GRP_ATTR_S stVpssGrpAttr = {0};
    VPSS_CHN_ATTR_S stVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }


    stVpssGrpAttr.enDynamicRange = enDynamicRange;
    stVpssGrpAttr.enPixelFormat  = enPixelFormat;
    stVpssGrpAttr.u32MaxW        = stSnsSize.u32Width;
    stVpssGrpAttr.u32MaxH        = stSnsSize.u32Height;
    stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
    stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
    stVpssGrpAttr.bNrEn = HI_TRUE;


    for(i=0; i<VPSS_MAX_PHY_CHN_NUM; i++)
    {
        if(HI_TRUE == pabChnEnable[i])
        {
            stVpssChnAttr[i].u32Width                     = stSize.u32Width;
            stVpssChnAttr[i].u32Height                    = stSize.u32Height;
            stVpssChnAttr[i].enChnMode                    = VPSS_CHN_MODE_USER;
            stVpssChnAttr[i].enCompressMode               = COMPRESS_MODE_NONE;//COMPRESS_MODE_SEG;
            stVpssChnAttr[i].enDynamicRange               = enDynamicRange;
            stVpssChnAttr[i].enPixelFormat                = enPixelFormat;
            stVpssChnAttr[i].stFrameRate.s32SrcFrameRate  = -1;
            stVpssChnAttr[i].stFrameRate.s32DstFrameRate  = -1;
            stVpssChnAttr[i].u32Depth                     = 0;
            stVpssChnAttr[i].bMirror                      = HI_FALSE;
            stVpssChnAttr[i].bFlip                        = HI_FALSE;
            stVpssChnAttr[i].enVideoFormat                = VIDEO_FORMAT_LINEAR;
            stVpssChnAttr[i].stAspectRatio.enMode         = ASPECT_RATIO_NONE;
        }
    }

    s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, pabChnEnable,&stVpssGrpAttr,stVpssChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("start VPSS fail for %#x!\n", s32Ret);
    }

    return s32Ret;
}


HI_S32 SAMPLE_VENC_CheckSensor(SAMPLE_SNS_TYPE_E   enSnsType,SIZE_S  stSize)
{
    HI_S32 s32Ret;
    SIZE_S          stSnsSize;
    PIC_SIZE_E      enSnsSize;

    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(enSnsType, &enSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_GetSizeBySensor failed!\n");
        return s32Ret;
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSnsSize, &stSnsSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    if((stSnsSize.u32Width < stSize.u32Width) || (stSnsSize.u32Height < stSize.u32Height))
    {
        SAMPLE_PRT("Sensor size is (%d,%d), but encode chnl is (%d,%d) !\n",
            stSnsSize.u32Width,stSnsSize.u32Height,stSize.u32Width,stSize.u32Height);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}


HI_S32 SAMPLE_VENC_H264(void)
{
    HI_S32 s32Ret;
    SIZE_S stSize;
    PIC_SIZE_E enSize = PIC_400x400;//6946 PIC_400X400
    VENC_CHN VencChn = 0;
    HI_U32 u32Profile = 0;  // H.264: 0:baseline; 1:MP; 2:HP; 3:SVC-T ; H.265: 0:MP; 1:Main 10 [0 1];
    PAYLOAD_TYPE_E enPayLoad = PT_H264;//PT_H264
    VENC_GOP_MODE_E enGopMode;
    VENC_GOP_ATTR_S stGopAttr;
    SAMPLE_RC_E enRcMode;
    HI_BOOL bRcnRefShareBuf = HI_TRUE;

    VI_DEV ViDev = 0;
    VI_PIPE ViPipe = 0;
    VI_CHN ViChn = 0;
    SAMPLE_VI_CONFIG_S stViConfig;

    VPSS_GRP VpssGrp = 0;
    VPSS_CHN VpssChn = 1;  // use chn 1 for zoom-out, chn 0 for zoom-in only
    HI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = { 1,1, 0, 0 };

    HI_U32 u32SupplementConfig = HI_FALSE;
    
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        return s32Ret;
    }

    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    if(SAMPLE_SNS_TYPE_BUTT == stViConfig.astViInfo[0].stSnsInfo.enSnsType)
    {
        SAMPLE_PRT("Not set SENSOR%d_TYPE !\n",0);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_VENC_CheckSensor(stViConfig.astViInfo[0].stSnsInfo.enSnsType,stSize);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Check Sensor err!\n");
        return HI_FAILURE;
    }

    stViConfig.s32WorkingViNum = 1;
    stViConfig.astViInfo[0].stDevInfo.ViDev = ViDev;
    stViConfig.astViInfo[0].stPipeInfo.aPipe[0] = ViPipe;
    stViConfig.astViInfo[0].stChnInfo.ViChn = ViChn;
    stViConfig.astViInfo[0].stChnInfo.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stViConfig.astViInfo[0].stChnInfo.enPixFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;

    s32Ret = SAMPLE_VENC_VI_Init(&stViConfig, HI_FALSE,u32SupplementConfig);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("Init VI err for %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = SAMPLE_VENC_VPSS_Init(VpssGrp,abChnEnable,DYNAMIC_RANGE_SDR8,PIXEL_FORMAT_YVU_SEMIPLANAR_420,stSize,stViConfig.astViInfo[0].stSnsInfo.enSnsType);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Init VPSS err for %#x!\n", s32Ret);
        goto EXIT_VI_STOP;
    }

    s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("VI Bind VPSS err for %#x!\n", s32Ret);
        goto EXIT_VPSS_STOP;
    }

    /******************************************
    start stream venc
    ******************************************/

    enRcMode = SAMPLE_RC_CBR;
    enGopMode = VENC_GOPMODE_NORMALP;
    s32Ret = SAMPLE_COMM_VENC_GetGopAttr(enGopMode,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Get GopAttr for %#x!\n", s32Ret);
        goto EXIT_VI_VPSS_UNBIND;
    }

    /***encode h.264 **/
    s32Ret = SAMPLE_COMM_VENC_Start(VencChn, enPayLoad, enSize, enRcMode,u32Profile,&stGopAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc Start failed for %#x!\n", s32Ret);
        goto EXIT_VENC_H265_UnBind;
    }

    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn,VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Venc bind Vpss failed for %#x!\n", s32Ret);
        goto EXIT_VENC_H264_STOP;
    }

    /******************************************
     stream save process
    ******************************************/
    s32Ret = Hilive_COMM_VENC_StartGetStream(VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        goto EXIT_VENC_H264_UnBind;
    }

    printf("please press twice ENTER to exit this sample\n");
    getchar();
    getchar();

    /******************************************
     exit process
    ******************************************/
    SAMPLE_COMM_VENC_StopGetStream();

EXIT_VENC_H264_UnBind:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp,VpssChn,VencChn);
EXIT_VENC_H264_STOP:
    SAMPLE_COMM_VENC_Stop(VencChn);
EXIT_VENC_H265_UnBind:
    SAMPLE_COMM_VPSS_UnBind_VENC(VpssGrp,VpssChn,VencChn);
EXIT_VENC_H265_STOP:
    SAMPLE_COMM_VENC_Stop(VencChn);
EXIT_VI_VPSS_UNBIND:
    SAMPLE_COMM_VI_UnBind_VPSS(ViPipe,ViChn,VpssGrp);
EXIT_VPSS_STOP:
    SAMPLE_COMM_VPSS_Stop(VpssGrp,abChnEnable);
EXIT_VI_STOP:
    SAMPLE_COMM_VI_StopVi(&stViConfig);
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}


/******************************************************************************
 * funciton : get file postfix according palyload_type.
 ******************************************************************************/
HI_S32 Hilive_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, char *szFilePostfix)
{
    if (PT_H264 == enPayload) {
        strcpy(szFilePostfix, ".h264");
    } else if (PT_H265 == enPayload) {
        strcpy(szFilePostfix, ".h265");
    } else if (PT_JPEG == enPayload) {
        strcpy(szFilePostfix, ".jpg");
    } else if (PT_MJPEG == enPayload) {
        strcpy(szFilePostfix, ".mjp");
    } else {
        SAMPLE_PRT("payload type err!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}



/******************************************************************************
 * funciton : start get venc stream process thread
 ******************************************************************************/
HI_S32 Hilive_COMM_VENC_StartGetStream(VENC_CHN VeChn)
{
    gMediaProcPara.bThreadStart = HI_TRUE;
    gMediaProcPara.s32Cnt = 1;
    gMediaProcPara.VeChn[0] = VeChn;
    return pthread_create(&gMediaProcPid, 0, Hilive_COMM_VENC_GetVencStreamProc, (HI_VOID *)&gMediaProcPara);
}

/******************************************************************************
 * funciton : get stream from each channels and save them
 ******************************************************************************/
HI_VOID *Hilive_COMM_VENC_GetVencStreamProc(HI_VOID *p)
{
    HI_S32 i;
    HI_S32 s32ChnTotal;
    VENC_CHN_ATTR_S stVencChnAttr;
    SAMPLE_VENC_GETSTREAM_PARA_S *pstPara;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_U32 u32PictureCnt[VENC_MAX_CHN_NUM] = { 0 };
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE *pFile[VENC_MAX_CHN_NUM];
    char szFilePostfix[10];
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
    VENC_STREAM_BUF_INFO_S stStreamBufInfo[VENC_MAX_CHN_NUM];

    prctl(PR_SET_NAME, "GetVencStream", 0, 0, 0);

    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S *)p;
    s32ChnTotal = pstPara->s32Cnt;
    /******************************************
     step 1:  check & prepare save-file & venc-fd
    ******************************************/
    if (s32ChnTotal >= VENC_MAX_CHN_NUM) {
        SAMPLE_PRT("input count invaild\n");
        return NULL;
    }
    for (i = 0; i < s32ChnTotal; i++) {
        /* decide the stream file name, and open file to save stream */
        VencChn = pstPara->VeChn[i];
        s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", VencChn, s32Ret);
            return NULL;
        }
        enPayLoadType[i] = stVencChnAttr.stVencAttr.enType;

        s32Ret = Hilive_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
        if (s32Ret != HI_SUCCESS) {
            SAMPLE_PRT("HisiLive_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", stVencChnAttr.stVencAttr.enType, s32Ret);
            return NULL;
        }
        if (PT_JPEG != enPayLoadType[i] && gParamOption.mode == MODE_FILE) {
            snprintf(aszFileName[i], 32, "stream_chn%d%s", i, szFilePostfix);

            pFile[i] = fopen(aszFileName[i], "wb");
            if (!pFile[i]) {
                SAMPLE_PRT("open file[%s] failed!\n", aszFileName[i]);
                return NULL;
            }
        }
        /* Set Venc Fd. */
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0) {
            SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n", VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i]) {
            maxfd = VencFd[i];
        }

        s32Ret = HI_MPI_VENC_GetStreamBufInfo(i, &stStreamBufInfo[i]);
        if (HI_SUCCESS != s32Ret) {
            SAMPLE_PRT("HI_MPI_VENC_GetStreamBufInfo failed with %#x!\n", s32Ret);
            return (void *)HI_FAILURE;
        }
    }

    /******************************************
     step 2:  Start to get streams of each channel.
    ******************************************/
    while (HI_TRUE == pstPara->bThreadStart) {
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++) {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0) {
            SAMPLE_PRT("select failed!\n");
            break;
        } else if (s32Ret == 0) {
            SAMPLE_PRT("get venc stream time out, exit thread\n");
            continue;
        } else {
            for (i = 0; i < s32ChnTotal; i++) {
                if (FD_ISSET(VencFd[i], &read_fds)) {
                    // SAMPLE_PRT("get channel [%d] data.\n", i);
                    /*******************************************************
                     step 2.1 : query how many packs in one-frame stream.
                    *******************************************************/
                    memset(&stStream, 0, sizeof(stStream));

                    s32Ret = HI_MPI_VENC_QueryStatus(i, &stStat);
                    if (HI_SUCCESS != s32Ret) {
                        SAMPLE_PRT("HI_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", i, s32Ret);
                        break;
                    }

                    /*******************************************************
                    step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
                     if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
                     {
                        SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                     }
                    *******************************************************/
                    if (0 == stStat.u32CurPacks) {
                        SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                        continue;
                    }
                    /*******************************************************
                     step 2.3 : malloc corresponding number of pack nodes.
                    *******************************************************/
                    stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack) {
                        SAMPLE_PRT("malloc stream pack failed!\n");
                        break;
                    }

                    /*******************************************************
                     step 2.4 : call mpi to get one-frame stream
                    *******************************************************/
                    stStream.u32PackCount = stStat.u32CurPacks;
                    s32Ret = HI_MPI_VENC_GetStream(i, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                        break;
                    }

                    /*******************************************************
                     step 2.5 : save frame to file
                    *******************************************************/
                    if (PT_JPEG == enPayLoadType[i]) {
                        snprintf(aszFileName[i], 32, "stream_chn%d_%d%s", i, u32PictureCnt[i], szFilePostfix);
                        pFile[i] = fopen(aszFileName[i], "wb");
                        if (!pFile[i]) {
                            SAMPLE_PRT("open file err!\n");
                            return NULL;
                        }
                    }


                        s32Ret = Hilive_RTPSendVideo(&stStream);

                    if (HI_SUCCESS != s32Ret) {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("save stream failed!\n");
                        break;
                    }
                    /*******************************************************
                     step 2.6 : release stream
                     *******************************************************/
                    s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (HI_SUCCESS != s32Ret) {
                        SAMPLE_PRT("HI_MPI_VENC_ReleaseStream failed!\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }

                    /*******************************************************
                     step 2.7 : free pack nodes
                    *******************************************************/
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    u32PictureCnt[i]++;
                    if (PT_JPEG == enPayLoadType[i]) {
                        fclose(pFile[i]);
                    }
                }
            }
        }
    }
    /*******************************************************
     * step 3 : close save-file
     *******************************************************/
    for (i = 0; i < s32ChnTotal; i++) {
        if (PT_JPEG != enPayLoadType[i]) {
            fclose(pFile[i]);
        }
    }
    return NULL;
}


/******************************************************************************
 * funciton : save stream
 ******************************************************************************/
HI_S32 Hilive_COMM_VENC_SaveStream(FILE *pFd, VENC_STREAM_S *pstStream)
{
    HI_S32 i;

    for (i = 0; i < pstStream->u32PackCount; i++) {
        fwrite(pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,
               pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset, 1, pFd);

        fflush(pFd);
    }

    return HI_SUCCESS;
}



HI_S32 Hilive_RTPSendVideo(VENC_STREAM_S *pstStream)
{
    int i;
    gRTPCtx.payload_type = (gParamOption.videoFormat == PT_H264) ? 0 : 1;

    static uint64_t packets = 0;
    ++packets;
    int count10s = gParamOption.frameRate * 10;

    for (i = 0; i < pstStream->u32PackCount; i++) {
        // // LOG("packet %d / %d, %lld\n", i + 1, pstStream->u32PackCount, pstStream->pstPack[i].u64PTS);
        // gRTPCtx.timestamp = (HI_U32)(pstStream->pstPack[i].u64PTS / 100 * 9);  // (μs / 10^6) * (90 * 10^3)
        // if (packets % count10s == 0) {                                         // debug once every 10 seconds
        //     LOGD("packet pts %llu, rtp ts %u\n", pstStream->pstPack[i].u64PTS, gRTPCtx.timestamp);
        // }
        //rtpSendH264HEVC(&gRTPCtx, &gUDPCtx,
                       // pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,  // stream ptr
                       // pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset);  // stream length
    
        // rtpSendH264HEVC_2(&rtpPacket,  &gUDPCtx, pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,  // stream ptr
        //                 pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset);
        ts=rtsp_get_reltime();
		rtsp_tx_video(session,(pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset), pstStream->pstPack[i].u32Len , ts);
		rtsp_do_event(demo);

    }

    return 0;
}


void doClient(SOCkContext *sockcontext)
{
    char method[40];
    char url[100];
    char version[40];
    int CSeq;

    int clientRtpPort, clientRtcpPort;
    char *rbuf=(char*)malloc(10000);
    char *sbuf=(char*)malloc(10000);

    while(1)
    {
        int recvLen;
        recvLen = recv(sockcontext->clientfd,rbuf,2000,0);
        if(recvLen<=0){
            printf("NO message received!\n");
            break;
        }
        rbuf[recvLen]='\0';
        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
        printf("%s rBuf = %s \n",__FUNCTION__,rbuf);
        const char* sep = "\n";
        char* line = strtok(rbuf, sep); //strtok()用来将字符串分割成一个个片段。参数s指向欲分割的字符串，参数delim则为分割字符串中包含的所有字符。
                                        //当strtok()在参数s的字符串中发现参数delim中包含的分割字符时,则会将该字符改为\0 字符。
                                        //在第一次调用时，strtok()必需给予参数s字符串，往后的调用则将参数s设置成NULL。每次调用成功则返回指向被分割出片段的指针。
        while(line)
        {
            //strstr(str1,str2) 函数用于判断字符串str2是否是str1的子串
            if (strstr(line, "OPTIONS") ||
                strstr(line, "DESCRIBE") ||
                strstr(line, "SETUP") ||
                strstr(line, "PLAY")) 
            {

                if (sscanf(line, "%s %s %s\r\n", method, url, version) != 3) 
                {
                    // error
                }
            }
                else if (strstr(line, "CSeq")) 
            {
                if (sscanf(line, "CSeq: %d\r\n", &CSeq) != 1) {
                    // error
                }
            }
            //strncmp：是C语言的字符串比较函数，用于比较两个字符串的前 n 个字符
            else if (!strncmp(line, "Transport:", strlen("Transport:"))) 
            {
                // Transport: RTP/AVP/UDP;unicast;client_port=13358-13359
                // Transport: RTP/AVP;unicast;client_port=13358-13359

                if (sscanf(line, "Transport: RTP/AVP/UDP;unicast;client_port=%d-%d\r\n",
                    &clientRtpPort, &clientRtcpPort) != 2) {
                    // error
                    printf("parse Transport error \n");
                }
            }
            line = strtok(NULL, sep);
        }
         if (!strcmp(method, "OPTIONS")) {
            if (handleCmd_OPTIONS(sbuf, CSeq))
            {
                printf("failed to handle options\n");
                break;
            }
        }
        else if (!strcmp(method, "DESCRIBE")) {
            if (handleCmd_DESCRIBE(sbuf, CSeq, url))
            {
                printf("failed to handle describe\n");
                break;
            }
       
        }
        else if (!strcmp(method, "SETUP")) {
            if (handleCmd_SETUP(sbuf, CSeq, clientRtpPort))
            {
                printf("failed to handle setup\n");
                break;
            }
            sockcontext->socketrtpfd = createSocket(IPPROTO_UDP);
            sockcontext->socketrtcpfd= createSocket(IPPROTO_UDP);

            if (sockcontext->socketrtpfd < 0 || sockcontext->socketrtcpfd < 0)
            {
                printf("failed to create udp socket\n");
                break;
            }

            if (bindSocketAddr(sockcontext->socketrtpfd , "0.0.0.0", SERVER_RTP_PORT) < 0 ||
                bindSocketAddr(sockcontext->socketrtcpfd, "0.0.0.0", SERVER_RTCP_PORT) < 0)
            {
                printf("failed to bind addr\n");
                break;
            }
        }
        else if (!strcmp(method, "PLAY")) {
            if (handleCmd_PLAY(sbuf, CSeq))
            {
                printf("failed to handle play\n");
                break;
            }
            printf("sbuf = %s \n", sbuf);
            printf("%s sbuf = %s \n", __FUNCTION__, sbuf);

            send(sockcontext->clientfd, sbuf, strlen(sbuf), 0);
        }

        else{
            printf("unsupported method = %s\n",method);
            break;
        }
        printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
        printf("%s sBuf = %s \n", __FUNCTION__, sbuf);

        send(sockcontext->clientfd, sbuf, strlen(sbuf), 0);


        
if (!strcmp(method, "PLAY")) 
        {
            rtpPacket = (struct RtpPacket*)malloc(500000);
            rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_H264, 0,
                0, 0, 0x88923423);
            printf("start play\n");
            printf("client ip:%s\n", sockcontext->srcIp);
            printf("client port:%d\n", sockcontext->srcPort);
            SAMPLE_VENC_H264();
            free(rtpPacket);
        }

        memset(method,0,sizeof(method)/sizeof(char));
        memset(url,0,sizeof(url)/sizeof(char));
        CSeq = 0;
    
    }
#if defined(WIN32) || defined(_WIN32) 
    closesocket(sockcontext->clientfd);
#else
    close(sockcontext->clientfd);
#endif
    free(rbuf);
    free(sbuf);
}



int main(int argc, char *argv[])
{
    HI_S32 s32Ret;
    char logo[200] = { 0 };
    sprintf(logo, "+-------------------------+\n|         HisiLive        |\n|  %s %s   |\n+-------------------------+\n", __DATE__,
            __TIME__);
    GREEN("%s\n", logo);

    writeFile("log.txt", logo, strlen(logo), 0);
    demo = create_rtsp_demo(554);//rtsp sever socket
    session = create_rtsp_session(demo, "/live.sdp");//对应rtsp session 

    SAMPLE_VENC_H264();
    // if (Hilive_ParseParam(argc, argv)) {
    //     return -1;
    // }
    // strcpy(gUDPCtx.dstIp, gParamOption.ip);
    // gUDPCtx.dstPort = SERVER_PORT ;

    // if (gParamOption.mode == MODE_RTP)
    // {

    //     initRTPMuxContext(&gRTPCtx);
    // }
    // createSocket_ctx(IPPROTO_TCP,&gUDPCtx);
    // if (bindSocketAddr_ctx(&gUDPCtx) < 0)
    // {
    //     printf("failed to bind addr\n");
    //     return -1;
    // }
    // if (listen(gUDPCtx.socketfd, 10) < 0)
    // {
    //     printf("failed to listen\n");
    //     return -1;
    // }
    // printf("%s rtsp://127.0.0.1:%d\n", __FILE__, SERVER_PORT);
    
    // while (1) {


    //     gUDPCtx.clientfd = acceptClient_ctx(&gUDPCtx);
    //     if (gUDPCtx.socketfd < 0)
    //     {
    //         printf("failed to accept client\n");
    //         return -1;
    //     }

    //     printf("accept client;client ip:%s,client port:%d\n", gUDPCtx.srcIp, gUDPCtx.srcPort);

    //     doClient(&gUDPCtx);
    // }
    if (HI_SUCCESS == s32Ret) {
        LOGD("program exit normally!\n");
    } else {
        LOGD("program exit abnormally!\n");
    }

    exit(s32Ret);
}

HI_VOID VENC_RTSP_THREAD(HI_VOID* arg)
{
    rtsp_demo_handle thread_rtsp_demo;
    rtsp_session_handle thread_session;
    thread_rtsp_demo = create_rtsp_demo(8554); 
    thread_session = create_rtsp_session(thread_rtsp_demo, "/live.sdp");   
    SAMPLE_VENC_H264();
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
