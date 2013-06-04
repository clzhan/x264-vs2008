/*****************************************************************************
 * x264.h: h264 encoder library
 * 视讯通讯实验室：http://www.powercam.cc/vclab
 * http://wmnmtm.blog.163.com 上有一些资料
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: x264.h,v 1.1 2004/06/03 19:24:12 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _X264_H
#define _X264_H 1

#if !defined(_STDINT_H) && !defined(_STDINT_H_) && \
    !defined(_INTTYPES_H) && !defined(_INTTYPES_H_)
# ifdef _MSC_VER
#  pragma message("You must include stdint.h or inttypes.h before x264.h")
# else
#  pragma message("warning You must include stdint.h or inttypes.h before x264.h")//zjh
# endif
#endif

#include <stdarg.h>

#define X264_BUILD 49

/* x264_t:
 *      opaque handler for decoder and encoder */
typedef struct x264_t x264_t;

/****************************************************************************
 * Initialisation structure and function.
 ****************************************************************************/
/* CPU flags
 */
#define X264_CPU_MMX        0x000001    /* mmx */
#define X264_CPU_MMXEXT     0x000002    /* mmx-ext*/
#define X264_CPU_SSE        0x000004    /* sse */
#define X264_CPU_SSE2       0x000008    /* sse 2 */
#define X264_CPU_3DNOW      0x000010    /* 3dnow! */
#define X264_CPU_3DNOWEXT   0x000020    /* 3dnow! ext */
#define X264_CPU_ALTIVEC    0x000040    /* altivec */

/* Analyse(分析, 分解, 解释) flags
 */
#define X264_ANALYSE_I4x4       0x0001  /* Analyse i4x4 */
#define X264_ANALYSE_I8x8       0x0002  /* Analyse i8x8 (requires 8x8 transform) */
#define X264_ANALYSE_PSUB16x16  0x0010  /* Analyse p16x8, p8x16 and p8x8 */
#define X264_ANALYSE_PSUB8x8    0x0020  /* Analyse p8x4, p4x8, p4x4 */
#define X264_ANALYSE_BSUB16x16  0x0100  /* Analyse b16x8, b8x16 and b8x8 */
#define X264_DIRECT_PRED_NONE        0
#define X264_DIRECT_PRED_SPATIAL     1
#define X264_DIRECT_PRED_TEMPORAL    2
#define X264_DIRECT_PRED_AUTO        3
#define X264_ME_DIA                  0
#define X264_ME_HEX                  1
#define X264_ME_UMH                  2
#define X264_ME_ESA                  3
#define X264_CQM_FLAT                0
#define X264_CQM_JVT                 1
#define X264_CQM_CUSTOM              2

#define X264_RC_CQP                  0
#define X264_RC_CRF                  1
#define X264_RC_ABR                  2

static const char * const x264_direct_pred_names[] = { "none", "spatial", "temporal", "auto", 0 };
static const char * const x264_motion_est_names[] = { "dia", "hex", "umh", "esa", 0 };
static const char * const x264_overscan_names[] = { "undef", "show", "crop", 0 };
static const char * const x264_vidformat_names[] = { "component", "pal", "ntsc", "secam", "mac", "undef", 0 };
static const char * const x264_fullrange_names[] = { "off", "on", 0 };
static const char * const x264_colorprim_names[] = { "", "bt709", "undef", "", "bt470m", "bt470bg", "smpte170m", "smpte240m", "film", 0 };
static const char * const x264_transfer_names[] = { "", "bt709", "undef", "", "bt470m", "bt470bg", "smpte170m", "smpte240m", "linear", "log100", "log316", 0 };
static const char * const x264_colmatrix_names[] = { "GBR", "bt709", "undef", "", "fcc", "bt470bg", "smpte170m", "smpte240m", "YCgCo", 0 };

/* 
Colorspace type (色彩空间类型)
 */
#define X264_CSP_MASK           0x00ff  /* 只用低8位即可，所以高8位是0x00 后面会按位与进行一些判断 如x264_frame_copy_picture函数中 */
#define X264_CSP_NONE           0x0000  /* Invalid mode     */
#define X264_CSP_I420           0x0001  /* yuv 4:2:0 planar */
#define X264_CSP_I422           0x0002  /* yuv 4:2:2 planar */
#define X264_CSP_I444           0x0003  /* yuv 4:4:4 planar */
#define X264_CSP_YV12           0x0004  /* yuv 4:2:0 planar */
#define X264_CSP_YUYV           0x0005  /* yuv 4:2:2 packed */
#define X264_CSP_RGB            0x0006  /* rgb 24bits       */
#define X264_CSP_BGR            0x0007  /* bgr 24bits       */
#define X264_CSP_BGRA           0x0008  /* bgr 32bits       */
#define X264_CSP_VFLIP          0x1000  /* */

/* 
Slice type (片/条带类型)
 */
#define X264_TYPE_AUTO          0x0000  /* 让x264选择正确遥类型 Let x264 choose the right type */
#define X264_TYPE_IDR           0x0001	/* IDR片 */
#define X264_TYPE_I             0x0002	/* I片 */
#define X264_TYPE_P             0x0003	/* P片 */
#define X264_TYPE_BREF          0x0004  /* Non-disposable(一次性) B-frame(B-帧) ???*/
#define X264_TYPE_B             0x0005	/* B片 */
#define IS_X264_TYPE_I(x) ((x)==X264_TYPE_I || (x)==X264_TYPE_IDR)	/* 帧内类型，I片或者IDR片都算 */
#define IS_X264_TYPE_B(x) ((x)==X264_TYPE_B || (x)==X264_TYPE_BREF)	/* 帧间类型，B片或者BREF */
																	//"||"逻辑或操作符


/* Log level 日志等级
 */
#define X264_LOG_NONE          (-1)	//无日志
#define X264_LOG_ERROR          0	//错误
#define X264_LOG_WARNING        1	//警告
#define X264_LOG_INFO           2	//信息
#define X264_LOG_DEBUG          3	//调试
 
typedef struct
{
    int i_start, i_end;
    int b_force_qp;//是否_强制_qp
    int i_qp;
    float f_bitrate_factor;//float_比特率_因子,系数
} x264_zone_t;//zone:区域

typedef struct
{
    /* (CPU 标志位)*/
    unsigned int cpu;
    int         i_threads;  /* (并行编码多帧) divide(分, 划分) each(每一) frame(帧) into multiple(多个的) slices(片), encode(编码) in parallel(平行的,并行的) */
							//把每一帧分成多个片，并行编码
    /* 视频属性 */
    int         i_width;		/* 宽度*/
    int         i_height;		/* 高度*/
    int         i_csp;			/* (支持i420，色彩空间) CSP of encoded bitstream, only i420 supported */
    int         i_level_idc;	/* level值的设置 */
    int         i_frame_total;	/* 编码帧的总数, 默认 0 */

	/* Vui参数集视频可用性信息视频标准化选项 标准第311页*/
    struct
    {
        /* they will be reduced(减少的,简化的) to be 0 < x <= 65535 and prime(主要的; 基本的) */
        int         i_sar_height;	/* 标准313页 句法元素sar_width表示样点高宽比的垂直尺寸(以任意单位) */
        int         i_sar_width;	/* 标准313页 句法元素sar_height表示样点高宽比的水平尺寸(以与句法元素sar_width相同的任意单位) 设置长宽比 */

        int         i_overscan;    /* 过扫描线，默认"undef"(不设置)，可选项：show(观看)/crop(去除) 0=undef, 1=no overscan, 2=overscan */
        
        /* see h264 annex(附录) E for the values of the following */
        int         i_vidformat;	/* 视频格式，默认"undef"，component/pal/ntsc/secam/mac/undef */
        int         b_fullrange;	/* Specify full range samples setting，默认"off"，可选项：off/on */
        int         i_colorprim;	/* 原始色度格式，默认"undef"，可选项：undef/bt709/bt470m/bt470bg，smpte170m/smpte240m/film */
        int         i_transfer;		/* 转换方式，默认"undef"，可选项：undef/bt709/bt470m/bt470bg/linear,log100/log316/smpte170m/smpte240m */
        int         i_colmatrix;	/* 色度矩阵设置，默认"undef",undef/bt709/fcc/bt470bg,smpte170m/smpte240m/GBR/YCgCo */
        int         i_chroma_loc;   /* 色度样本指定，范围0~5，默认0 both top & bottom */
    } vui;

    int         i_fps_num;
    int         i_fps_den;//i_fps_den
					/*
					这两个参数是由fps帧率确定的，赋值的过程见下：
					{        float fps;      
					if( sscanf( value, "%d/%d", &p->i_fps_num, &p->i_fps_den ) == 2 )
					;
					else if( sscanf( value, "%f", &fps ) )
					{
					p->i_fps_num = (int)(fps * 1000 + .5);
					p->i_fps_den = 1000;
					}
					else
					b_error = 1;
					}
					输入的Value的值就是fps。
					*/



    /* (流参数)Bitstream parameters */
    int         i_frame_reference;  /* 参考帧最大数目 */
    int         i_keyint_max;       /* 在此间隔设置IDR关键帧(每过多少帧设置一个IDR帧) Force an IDR keyframe at this interval */
    int         i_keyint_min;       /* 场景切换少于此值编码为I帧, 而不是 IDR帧Scenecuts closer together than this are coded as I, not IDR. */
    int         i_scenecut_threshold; /* 控制多怎样插入I帧how aggressively to insert extra I frames */
    int         i_bframe;   /* 在两个参考帧之间B帧的数目how many b-frame between 2 references pictures */
    int         b_bframe_adaptive;/* 自适应B帧判定 */
    int         i_bframe_bias;/*控制插入B帧判定，范围-100~+100，越高越容易插入B帧，默认0*/

    int         b_bframe_pyramid;   /* 允许部分B为参考帧,可选值为0，1，2 Keep some B-frames as references */
									//pyramid:金字塔
	/*去方块滤波器需要的参数，alpha和beta是去方块滤波器的参数*/
    int         b_deblocking_filter;			//是否使用去块滤波器
    int         i_deblocking_filter_alphac0;    /* [-6, 6] -6 light(轻的) filter, 6 strong(强烈的) */
    int         i_deblocking_filter_beta;       /* [-6, 6]  idem(同上) ;beta:贝塔,希腊字母中的第二个字母*/

	/*熵编码 */
    int         b_cabac;		//CABAC，基于上下文的自适应二进制算术熵编码
    int         i_cabac_init_idc; //_idc:id cabac

    int         i_cqm_preset;		/* 自定义量化矩阵(CQM),初始化量化模式为flat */ //preset:预置,预先布置,事先调整
    char        *psz_cqm_file;      /* JM format读取JM格式的外部量化矩阵文件，自动忽略其他—cqm 选项 JM format */
    uint8_t     cqm_4iy[16];        /* used only if i_cqm_preset == X264_CQM_CUSTOM */
    uint8_t     cqm_4ic[16];		//???
    uint8_t     cqm_4py[16];
    uint8_t     cqm_4pc[16];
    uint8_t     cqm_8iy[64];
    uint8_t     cqm_8py[64];

    /* Log */
    void        (*pf_log)( void *, int i_level, const char *psz, va_list );/* 函数指针？common.c中h->param.pf_log( h->param.p_log_private, i_level, psz_fmt, arg ) */
    void        *p_log_private;//private:私有的
    int         i_log_level;//前面定义了4个，估计是要用它们来赋值(X264_LOG_NONE X264_LOG_ERROR X264_LOG_WARNING  X264_LOG_INFO X264_LOG_DEBUG)
    int         b_visualize;//visualize:设想, 想像, 构想

    /* 编码分析参数Encoder analyser parameters */
    struct
    {
        unsigned int intra;     /* 帧间分区intra partitions */
        unsigned int inter;     /* 帧内分区inter partitions */

        int          b_transform_8x8;		/* 帧间分区 */
        int          b_weighted_bipred;		/* 为b帧隐式加权implicit weighting for B-frames */
        int          i_direct_mv_pred;		/* 时间空间队运动预测spatial vs temporal mv prediction */
        int          i_chroma_qp_offset;	/*色度量化步长偏移量 */

        int          i_me_method;		/* motion(运动) estimation(估计) algorithm(算法,运算法则) to use (X264_ME_*) */
        int          i_me_range;		/* integer pixel motion estimation search range (from predicted(预测的) mv) 整像素运动估计搜索范围，从预测的mv*/
        int          i_mv_range;		/* maximum length of a mv (in pixels) */
        int          i_subpel_refine;	/* subpixel(子像素) motion(运动) estimation(估计) quality(质量) */
        int          b_bidir_me;		/* jointly(共同地,联合地) optimize(使最优化) both MVs in B-frames */
        int          b_chroma_me;		/* chroma(色度) ME(运动估计) for subpel(子像素) and mode(模式) decision(判断,结果) in P-frames */
        int          b_bframe_rdo;		/* RD based mode decision for B-frames */
        int          b_mixed_references; /* allow each mb partition in P-frames to have it's own reference number */
        int          i_trellis;			/* trellis RD quantization */
        int          b_fast_pskip;		/* early SKIP detection on P-frames */
        int          b_dct_decimate;	/* transform coefficient thresholding on P-frames */
        int          i_noise_reduction; /* adaptive pseudo-deadzone (假的-死的区域) ;zero:区域 ;reduction:减少, 缩小*/

        int          b_psnr;			/* 计算和打印PSNR信息 || Do we compute PSNR stats雕塑 (save a few % of cpu) */
    } analyse;//分析, 分解, 解释
	/*
	--deadzone-inter <整数>  设置inter模式下，亮度死区量化值，范围0~32，默认21。
	--deadzone-intra <整数>  设置intra模式下，亮度死区量化值，范围0~32，默认11。

	http://bbs.chinavideo.org/viewthread.php?tid=6769&extra=page%3D3
	量化公式：z＝（Y*MF+f）>>qbits；其中，Y为变换后的残差值，MF为量化系数，f为量化死区系数，经验值f＝1<<qbits /3(intra)或1<<qbits/6（inter），而21和11正是根据这个经验值转换而来。
	只是X264把51个qp等级下的量化值，用查表的方式，提前计算好，将量化公式与x264量化公式对照，就可知道21、11的来历
	*/


    /* Rate control parameters 速率控制参数 */
    struct
    {
        int         i_rc_method;    /* X264_RC_* method:方法, 办法 */

        int         i_qp_constant;  /* 0-51 constant:常数,常量*/
        int         i_qp_min;       /* 允许的最小量化值 || min allowed QP value */
        int         i_qp_max;       /* 允许的最大量化值 || max allowed QP value */
        int         i_qp_step;      /* 帧间最大量化步长 || max QP step between frames */

        int         i_bitrate;		//设置平均码率
        int         i_rf_constant;  /* 1pass VBR, nominal(名义上的) QP */ //constant:常数
        float       f_rate_tolerance;//tolerance:偏差
        int         i_vbv_max_bitrate;/*平均码率模式下，最大瞬时码率，默认0(与-B设置相同) */
        int         i_vbv_buffer_size;/*码率控制缓冲区的大小，单位kbit，默认0 */
        float       f_vbv_buffer_init;/* <=1: fraction of buffer_size. >1: kbit码率控制缓冲区数据保留的最大数据量与缓冲区大小之比，范围0~1.0，默认0.9*/
        float       f_ip_factor;//factor:因子, 因数,要素
        float       f_pb_factor;

        /* 2pass */
        int         b_stat_write;   /* Enable(使能够) stat writing in psz_stat_out */
        char        *psz_stat_out;
        int         b_stat_read;    /* Read stat from psz_stat_in and use it */
        char        *psz_stat_in;

        /* 2pass params (same as ffmpeg ones) */
        char        *psz_rc_eq;     /* 2 pass rate control equation 等式 */
        float       f_qcompress;    /* 0.0 => cbr, 1.0 => constant qp */
        float       f_qblur;        /* 时间上模糊量化 || temporally blur quants(数量，量化) */
        float       f_complexity_blur; /* 时间上模糊复杂性 || temporally(时间的) blur(模糊) complexity(复杂性) */
        x264_zone_t *zones;         /* ratecontrol overrides(忽略；覆盖) */
        int         i_zones;        /* sumber(总数) of zone_t's */
        char        *psz_zones;     /* 指定区的另一种方法 || alternate(代替的) method(方法) of specifying(指定) zones(区域) */
    } rc;

    /* Muxing parameters */
    int b_aud;                  /* 生成访问单元分隔符 generate(生成) access(访问,入口，入门，入径；通道；使用途径；) unit(单元) delimiters(定界符,分隔符) */
    int b_repeat_headers;       /* 在每个关键帧前放置SPS/PPS || put SPS/PPS before each keyframe */
    int i_sps_id;               /*  SPS 和 PPS id 号 || SPS and PPS id number */
} x264_param_t;

typedef struct {
    int level_idc;
    int mbps;        // 最大宏块处理速度，单位是 宏块数/秒 max macroblock processing(处理) rate (macroblocks/sec)
    int frame_size;  // 最大帧尺寸，以宏块为单位 max frame size (macroblocks)
    int dpb;         // 最大解码图像缓存区，单位是比特 max decoded picture buffer (bytes)//DPB 解码图像缓存区
    int bitrate;     // 最大比特率 k比特/秒 max bitrate (kbit/sec)
    int cpb;         // 最大vbv缓冲，单位kbit max vbv buffer (kbit)
    int mv_range;    // max vertica(垂直)l mv component(分量) range范围 (pixels)
    int mvs_per_2mb; // max mvs per 2 consecutive(连续的) mbs.
    int slice_rate;  // rate:比率,率,速度; 进度
    int bipred8x8;   // limit bipred to >=8x8	//双向预测8x8
    int direct8x8;   // limit b_direct to >=8x8	//直接预测8x8
    int frame_only;  // forbid(禁止) interlacing(隔行,间行)		//
} x264_level_t;

/* all of the levels defined(规定, 确定) in the standard, terminated(结束) by .level_idc=0 */
extern const x264_level_t x264_levels[];

/* x264_param_default:
 *      fill(填充) x264_param_t with default values and do CPU detection(发觉; 侦查；探测；察觉；发现) 
 *		用默认值填充结构体x264_param_t
 */
void    x264_param_default( x264_param_t * );

/* x264_param_parse:
 *		parse:解析
 *      set one parameter by name.
 *      returns 0 on success(返回0表示成功), or returns one of the following errors(返回1表示后面的错误).
 *      note: bad value occurs(重现) only if it can't even(甚至, 即使) parse the value,
 *      numerical(数字的, 用数字表示的, 数值的) range(范围) is not checked(检查, 核对) until(到…为止,在…以前) x264_encoder_open() or x264_encoder_reconfig(). */
#define X264_PARAM_BAD_NAME  (-1)
#define X264_PARAM_BAD_VALUE (-2)
int x264_param_parse( x264_param_t *, const char *name, const char *value );

/****************************************************************************
 * Picture structures and functions.
 * 用read_frame_yuv函数从文件中读取的内容放到这个结构体的
 ****************************************************************************/
typedef struct
{
    int     i_csp;			//Csp: color space parameter 色彩空间参数 X264只支持I420

    int     i_plane;		//i_Plane 代表色彩空间的个数。一般为3，YUV，初始化为
    int     i_stride[4];	//跨距。表面的跨距，有时也称为间距，指的是表面的宽度，以字节数表示。对于一个表面原点位于左上角的表面来说，跨距总是正数
    uint8_t *plane[4];		//typedef unsigned char   uint8_t //sizeof(unsigned char) = 1
} x264_image_t;

typedef struct
{
    /* In: force picture type (if not auto) XXX: ignored (忽视) for now
     * Out: type of the picture encoded */
    int i_type;		//I_type 指明被编码图像的类型，有X264_TYPE_AUTO X264_TYPE_IDR X264_TYPE_I X264_TYPE_P X264_TYPE_BREF X264_TYPE_B可供选择，初始化为AUTO，说明由x264在编码过程中自行控制。
    /* In: force (强制) quantizer (量化器) for > 0 */
    int i_qpplus1;	//I_qpplus1 ：此参数减1代表当前画面的量化参数值
    /* In: user pts, Out: pts of encoded picture (user)*/
    int64_t i_pts;	//I_pts ：program time stamp 程序时间戳，指示这幅画面编码的时间戳。

    /* In: raw(原始的) data */
    x264_image_t img;//Img :存放真正一副图像的原始数据。////x264_picture_t->img.plane[0] x264_picture_t->img.plane[1] x264_picture_t->img.plane[2] 这里面存原始图像
} x264_picture_t;
//x264_picture_t和x264_frame_t的区别.前者是说明一个视频序列中每帧的特点.后者存放每帧实际的象素值.注意区分

/* x264_picture_alloc:
 *  alloc data for a picture. You must call x264_picture_clean on it. */
/*  分配数据给一个图片，必须调用x264_picture_clean清除 */
void x264_picture_alloc( x264_picture_t *pic, int i_csp, int i_width, int i_height );

/* x264_picture_clean:
 *  free associated resource for a x264_picture_t allocated with
 *  x264_picture_alloc ONLY 
 * 释放由x264_picture_alloc分配的资源
*/
void x264_picture_clean( x264_picture_t *pic );

/****************************************************************************
 * NAL structure and functions:
 ****************************************************************************/
/* nal */
//毕厚杰：表6.20，第159页，应该是0至12,13至23,24至31
enum nal_unit_type_e
{
    NAL_UNKNOWN = 0,		//未使用
    NAL_SLICE   = 1,		//不分区、非IDR图像的片
    NAL_SLICE_DPA   = 2,	//片分区A
    NAL_SLICE_DPB   = 3,	//片分区B
    NAL_SLICE_DPC   = 4,	//片分区C
    NAL_SLICE_IDR   = 5,    /* ref_idc != 0 nal_unit_type=5时，表示当前NAL是IDR图像的一个片，在这种情况下，IDR图像中的每个片的nal_unit_type都应该等于5。注意片分区不可用于IDR图像 ，当nal_unit_type=5时，nal_ref_idc大于0，nal_unit_type等于6,9,10,11,12时，nal_ref_idc等于0*/
    NAL_SEI         = 6,    /* ref_idc == 0 */
    NAL_SPS         = 7,	//序列参数集
    NAL_PPS         = 8,	//图像参数集
    NAL_AUD         = 9,	//分界符
    /* ref_idc == 0 for 6,9,10,11,12 */
};
enum nal_priority_e//priority:优先权,优先级
{
    NAL_PRIORITY_DISPOSABLE = 0,//NAL_优先级_可丢弃的
    NAL_PRIORITY_LOW        = 1,//NAL_优先级_低
    NAL_PRIORITY_HIGH       = 2,//NAL_优先级_高
    NAL_PRIORITY_HIGHEST    = 3,//NAL_优先级_最高
};	//[毕厚杰：Page159 nal_ref_idc] 指示当前NAL的优先级，取值范围0-3，值越高，表示当前NAL越重要，越需要优先受到保护，H.264规定如果当前NAL是一个序列参数集，或一个图像参数集，或属于参考图像的片或片分区等重要的数据单位时，本句法元素必须大于0。但在大于0时具体该取何值，并没有进一步的规定。

typedef struct
{
    int i_ref_idc;			/* nal_priority_e (nal优先级) */
    int i_type;				/* nal_unit_type_e (nal单元类型) */

    /* This data are raw(原始的) payload(载荷) */
    int i_payload;			/* 负载的大小 Size of payload in bytes */
							/* 
							If param->b_annexb is set, Annex-B bytestream with 4-byte startcode.
							* Otherwise, startcode is replaced with a 4-byte size.
							* This size is the size used in mp4/similar muxing; it is equal to i_payload-4 
							*/
    uint8_t *p_payload;		/* 指向有效荷载的起始地址 */
} x264_nal_t;

/* x264_nal_encode:
 *      encode a nal into a buffer, setting the size.
 *		编码一个nal至一个缓冲区，设置尺寸
 *      if b_annexeb then a long synch(同时;同步;同步信号) work is added
 *      XXX: it currently doesn't check for overflow 
 *		b_annexeb:是否加起始码
 */
int x264_nal_encode( void *, int *, int b_annexeb, x264_nal_t *nal );

/* x264_nal_decode:
 *      decode a buffer nal into a x264_nal_t 
 *      解码一个缓冲区 nal 到 一个 x264_nal_t 结构体
*/
int x264_nal_decode( x264_nal_t *nal, void *, int );

/****************************************************************************
 * Encoder functions:
 * x264_encoder_open创建一个编码器，返回类型为x264_t
 * 后面的多个函数，第一个参数均为x264_t，x264_t就代表、指定一个编码器
 ****************************************************************************/

/* x264_encoder_open:
 *      create a new encoder handler, all parameters from x264_param_t are copied 
 *      建立一个新的编码器 ，所有参数从结构体x264_param_t拷贝
*/
x264_t *x264_encoder_open   ( x264_param_t * );

/* x264_encoder_reconfig:
 *      change encoder options while encoding,
 *      analysis(解析)-related(相关的) parameters from x264_param_t are copied 
 *      编码过程中改变编码器选项
 *		主要对x264_t结构体的x264_param_t子结构进行设置
*/
int     x264_encoder_reconfig( x264_t *, x264_param_t * );

/* x264_encoder_headers:
 *      return the SPS and PPS that will be used for the whole stream 
 *		返回 SPS 和 pps ，将被用在整个流中
*/
int     x264_encoder_headers( x264_t *, x264_nal_t **, int * );

/* x264_encoder_encode:
 *      encode one picture 
 *		编码一个图片
*/
int     x264_encoder_encode ( x264_t *, x264_nal_t **, int *, x264_picture_t *, x264_picture_t * );

/* x264_encoder_close:
 *      close an encoder handler 
 *		关闭一个编码处理器
*/
void    x264_encoder_close  ( x264_t * );

/* XXX: decoder isn't working so no need to export it */

/****************************************************************************
 * Private((私有的) stuff(材料，东西，把…装进) for internal(内部的) usage(用法):
 ****************************************************************************/
#ifdef __X264__
#   ifdef _MSC_VER
#       define inline __inline
#       define DECLARE_ALIGNED( type, var, n ) __declspec(align(n)) type var
#		define strncasecmp(s1, s2, n) strnicmp(s1, s2, n)	/* 比较字符串s1和s2的前n个字符但不区分大小写 */
#   else
#       define DECLARE_ALIGNED( type, var, n ) type var __attribute__((aligned(n)))
#   endif
#endif

#endif
