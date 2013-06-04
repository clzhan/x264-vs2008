/*****************************************************************************
 * common.h: h264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: common.h,v 1.1 2004/06/03 19:27:06 fenrir Exp $
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

#ifndef _COMMON_H
#define _COMMON_H 1

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif
#include <stdarg.h>
#include <stdlib.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#define X264_VERSION "" // no configure script for msvc
#endif

/* threads */
#ifdef __WIN32__
#include <windows.h>
#define pthread_t               HANDLE
#define pthread_create(t,u,f,d) *(t)=CreateThread(NULL,0,f,d,0,NULL)
#define pthread_join(t,s)       { WaitForSingleObject(t,INFINITE); \
                                  CloseHandle(t); } 
#define HAVE_PTHREAD 1

#elif defined(SYS_BEOS)
#include <kernel/OS.h>
#define pthread_t               thread_id
#define pthread_create(t,u,f,d) { *(t)=spawn_thread(f,"",10,d); \
                                  resume_thread(*(t)); }
#define pthread_join(t,s)       { long tmp; \
                                  wait_for_thread(t,(s)?(long*)(s):&tmp); }
#define HAVE_PTHREAD 1

#elif defined(HAVE_PTHREAD)
#include <pthread.h>
#endif

/****************************************************************************
 * Macros
 ****************************************************************************/
#define X264_MIN(a,b) ( (a)<(b) ? (a) : (b) )//求最小值
#define X264_MAX(a,b) ( (a)>(b) ? (a) : (b) )//求最大值
#define X264_MIN3(a,b,c) X264_MIN((a),X264_MIN((b),(c)))//三个中求最小值
#define X264_MAX3(a,b,c) X264_MAX((a),X264_MAX((b),(c)))//三个中求最大值
#define X264_MIN4(a,b,c,d) X264_MIN((a),X264_MIN3((b),(c),(d)))//4个中求最小值
#define X264_MAX4(a,b,c,d) X264_MAX((a),X264_MAX3((b),(c),(d)))//4个中求最大值
#define XCHG(type,a,b) { type t = a; a = b; b = t; }//交换a和b
#define FIX8(f) ((int)(f*(1<<8)+.5))//fix:取整数，修理

#if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 0)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

#define CHECKED_MALLOC( var, size )\
{\
    var = x264_malloc( size );\
    if( !var )\
    {\
        x264_log( h, X264_LOG_ERROR, "malloc failed\n" );\
        goto fail;\
    }\
}

#define X264_BFRAME_MAX 16	//B帧最大数为16
#define X264_SLICE_MAX 4	//片/条带最大数为4
#define X264_NAL_MAX (4 + X264_SLICE_MAX)	//4 + 4

/****************************************************************************
 * Includes
 ****************************************************************************/
#include "x264.h"
#include "bs.h"
#include "set.h"
#include "predict.h"
#include "pixel.h"
#include "mc.h"
#include "frame.h"
#include "dct.h"
#include "cabac.h"
#include "csp.h"
#include "quant.h"

/****************************************************************************
 * Generals functions
 ****************************************************************************/
/* x264_malloc : will do or emulate(模仿) a memalign(内存对齐?)
 * XXX you HAVE TO(必须) use x264_free(函数) for buffer(缓存) allocated(已分配的)
 * with x264_malloc
 * 你必须用专用的函数x264_free来释放，那些用x264_malloc函数分配的缓存
 */
void *x264_malloc( int );
void *x264_realloc( void *p, int i_size );
void  x264_free( void * );

/* x264_slurp_file: malloc space for the whole file and read it */
char *x264_slurp_file( const char *filename );

/* mdate: return the current date in microsecond( 一百万分之一秒,微秒) */
int64_t x264_mdate( void );

/* x264_param2string: return a (malloced) string containing(包含) most of
 * the encoding options */
char *x264_param2string( x264_param_t *p, int b_res );

/* log */
void x264_log( x264_t *h, int i_level, const char *psz_fmt, ... );

void x264_reduce_fraction( int *n, int *d );

/*
 * 名称：
 * 参数：
 * 返回：
 * 功能：v在区间[i_min,i_max]的左边时，返回区间左边界,在区间右边时，返回区间右边界，其它情况下，返回v
 * 思路：实际上就是控制了v的值不能超出区间[i_min,i_max]
 * 资料：
			i_min.........i_max
		v	v		v		v	v   (v的几种可能位置示意)

 * 
*/
static inline int x264_clip3( int v, int i_min, int i_max )
{
    return ( (v < i_min) ? i_min : (v > i_max) ? i_max : v );
}
//x264_clip3f是x264_clip3的float版本，功能一样，只是输入参数的类型为float，返回值也是float
static inline float x264_clip3f( float v, float f_min, float f_max )//float版的限制v的值不越出边界
{
    return ( (v < f_min) ? f_min : (v > f_max) ? f_max : v );
}

/*
 * 名称：
 * 功能：x264_median(...) 在给出的三个数中，找出中间的那个数 
 * 参数：
 * 返回：
 * 资料：
 * 思路：借助于临时的min和max，存下每次比较的最小与最大，最后一步return时，直接把三个数加起来，减去最小和最大，求出中间的来
 * 
*/
static inline int x264_median( int a, int b, int c )//median:在中间的,通过中点的
{
    int min = a, max =a;
    if( b < min )
        min = b;
    else
        max = b;    /* no need to do 'b > max' (more consuming than always doing affectation) */
	//上面是在a和b中比较大小，小的那个存在min里

	//把第3个数c，与上步中的最小和最大比较
    if( c < min )
        min = c;
    else if( c > max )
        max = c;

    return a + b + c - min - max;	//a + b + c - 最小的 - 最大的 -> 中间的
}


/****************************************************************************
 *片_类型//毕厚杰书，第164页
 ****************************************************************************/
enum slice_type_e
{
    SLICE_TYPE_P  = 0,//P片
    SLICE_TYPE_B  = 1,//B片
    SLICE_TYPE_I  = 2,//I片
    SLICE_TYPE_SP = 3,//SP片
    SLICE_TYPE_SI = 4//SI片
};

static const char slice_type_to_char[] = { 'P', 'B', 'I', 'S', 'S' };

typedef struct
{
    x264_sps_t *sps;//序列参数集
    x264_pps_t *pps;//图像参数集

    int i_type;		//[毕厚杰：Page 164] 相当于slice_type，指明片的类型；0123456789对应P、B、I、SP、SI 
    int i_first_mb;	//[毕厚杰：Page 164] 相当于 first_mb_in_slice 片中的第一个宏块的地址，片通过这个句法元素来标定它自己的地址。要注意的是，在帧场自适应模式下，宏块都是成对出现的，这时本句法元素表示的是第几个宏块对，对应的第一个宏块的真实地址应该是2*---
    int i_last_mb;	//最后一个宏块

    int i_pps_id;	//[毕厚杰：Page 164] 相当于 pic_parameter_set_id ; 图像参数集的索引号，取值范围[0,255]

    int i_frame_num;	//[毕厚杰：Page 164] 相当于 frame_num ; 只有当这个图像是参考帧时，它所携带的这个句法元素在解码时才有意义，
						//[毕厚杰：Page 164] 相当于 frame_num ; 每个参考帧都有一个依次连续的frame_num作为它们的标识，这指明了各图像的解码顺序。frame_num所能达到的最大值由前文序列参数集中的句法元素推出。毕P164
						//Frame Num指示图像列的编码顺序，POC指示了图像的显示顺序 //视频序列里每一幅图像都要按照顺序进行编码，由于H．264中存在双向预测，这个顺序就不一定是显示顺序。为了使解码器能正常地解码并显示，H．264中使用Frame Num指示图像列的编码顺序，POC指示了图像的显示顺序。
    int b_field_pic;	//[毕厚杰：Page 165] 相当于 field_pic_flag; 这是在片层标识图像编码模式的唯一一个句法元素。所谓的编码模式是指的帧编码、场编码、帧场自适应编码。当这个句法元素取值为1时属于场编码，0时为非场编码。
    int b_bottom_field;	//[毕厚杰：Page 166] 相当于 bottom_field_flag ;  =1时表示当前的图像是属于底场，=0时表示当前图像属于顶场

    int i_idr_pic_id;   /* -1 if nal_type != 5 */ 
						//[毕厚杰：Page 164] 相当于idr_pic_id ;IDR图像的标识。不同的IDR图像有不同的idr_pic_id值。值得注意的是，IDR图像不等价于I图像，只有在作为IDR图像的I帧才有这个句法元素。在场模式下，IDR帧的两个场有相同的idr_pic_id值。idr_pic_id的取值范围是[0,65536]，和frame_num类似，当它的值超过这个范围时，它会以循环的方式重新开始计数。

    int i_poc_lsb;		//[毕厚杰：Page 166] 相当于 pic_order_cnt_lsb，在POC的第一种算法中本句法元素来计算POC值，在POC的第一种算法中是显式地传递POC的值，而其他两种算法是通过frame_num来映射POC的值，注意这个句法元素的读取函数是u(v)，这个v的值是序列参数集的句法元素--+4个比特得到的。取值范围是...
    int i_delta_poc_bottom;	//[毕厚杰：Page 166] 相当于 delta_pic_order_cnt_bottom，如果是在场模式下，场对中的两个场各自被构造为一个图像，它们有各自的POC算法来分别计算两个场的POC值，也就是一个场对拥有一对POC值；而在帧模式或帧场自适应模式下，一个图像只能根据片头的句法元素计算出一个POC值。根据H.264的规定，在序列中可能出现场的情况，即frame_mbs_only_flag不为1时，每个帧或帧场自适应的图像在解码完后必须分解为两个场，以供后续图像中的场作为参考图像。所以当时frame_mb_only_flag不为1时，帧或帧场自适应

    int i_delta_poc[2]; //[毕厚杰：Page 167] 相当于 delta_pic_order_cnt[0] , delta_pic_order_cnt[1]
    int i_redundant_pic_cnt;	//[毕厚杰：Page 167] 相当于 redundant_pic_cnt;冗余片的id号，取值范围[0,127]

    int b_direct_spatial_mv_pred;	//[毕厚杰：Page 167] 相当于 direct_spatial_mv_pred；指出在B图像的直接预测模式下，用时间预测还是用空间预测，1表示空间预测，0表示时间预测。直接预测在书上94页

    int b_num_ref_idx_override;		//[毕厚杰：Page 167] 相当于 num_ref_idx_active_override_flag，在图像参数集中，有两个句法元素指定当前参考帧队列中实际可用的参考帧的数目。在片头可以重载这对句法元素，以给某特定图像更大的灵活度。这个句法元素就是指明片头是否会重载，如果该句法元素等于1，下面会出现新的num_ref_idx_10_active_minus1和...11...
    int i_num_ref_idx_l0_active;	//[毕厚杰：Page 167] 相当于 num_ref_idx_10_active_minus1
    int i_num_ref_idx_l1_active;	//[毕厚杰：Page 167] 相当于 num_ref_idx_11_active_minus1

    int b_ref_pic_list_reordering_l0;//短期参考图像列表重排序?
    int b_ref_pic_list_reordering_l1;//长期参考图像列表重排序?
    struct {
        int idc;
        int arg;
    } ref_pic_list_order[2][16];	 

    int i_cabac_init_idc;//[毕厚杰：Page 167] 相当于 cabac_init_idc，给出cabac初始化时表格的选择，取值范围为[0,2]

    int i_qp;//
    int i_qp_delta;//[毕厚杰：Page 167] 相当于 slice_qp_delta，指出用于当前片的所有宏块的量化参数的初始值
    int b_sp_for_swidth;////[毕厚杰：Page 167] 相当于 sp_for_switch_flag
    int i_qs_delta;//[毕厚杰：Page 167] 相当于 slice_qs_delta，与slice_qp_delta语义相似，用在SI和SP中，由下式计算：...

    /* deblocking filter */
    int i_disable_deblocking_filter_idc;//[毕厚杰：Page 167] 相当于 disable_deblocking_filter_idc，H.264规定一套可以在解码器端独立地计算图像中各边界的滤波强度进行
    int i_alpha_c0_offset;
    int i_beta_offset;
	/*
	http://bbs.chinavideo.org/viewthread.php?tid=10671&extra=page%3D2
	disable_deblocking_filter_idc = 0，所有边界全滤波
	disable_deblocking_filter_idc = 1，所有边界都不滤波
	disable_deblocking_filter_idc = 2，片边界不滤波
	*/
} x264_slice_header_t;	//x264_片_头_
//x264这块的变量命名，与毕厚杰书上说的变量有一些出入，x264加了类型符号，比如i_、b_，另外省掉了_flag，进行了缩写，如_poc_，实际是pic_order_cnt_lsb，_pps_，实际为pic_parameter_set_id
//几种结构体的定义格式的区别：http://wmnmtm.blog.163.com/blog/static/38245714201181744856220/

/* From ffmpeg
 */
#define X264_SCAN8_SIZE (6*8)	//
#define X264_SCAN8_0 (4+1*8)	//

/*
这是一个坐标变换用的查找表,是个数组,将0--23变换为一个8x8矩阵中的4x4 和2个2x2 的块扫描
http://wmnmtm.blog.163.com/blog/static/3824571420118175553244/
也就是说，这个表里的元素的值，将被作为序号或者下标，以它们为指引读出来的，正好是一个宏块里的亮度部分和色度部分，而且很完整，但是，是以4x4块为单位的。
那么，它要去哪里读呢？待续
*/
static const int x264_scan8[16+2*4] =	//16+8=24
{
    /* Luma亮度 */
    4+1*8, 5+1*8, 4+2*8, 5+2*8,	/* 12, 13, 20, 21 */
    6+1*8, 7+1*8, 6+2*8, 7+2*8,	/* 14, 15, 22, 23 */
    4+3*8, 5+3*8, 4+4*8, 5+4*8,	/* 28, 29, 36, 37 */
    6+3*8, 7+3*8, 6+4*8, 7+4*8,	/* 30, 31, 38, 39 */

    /* Cb */
    1+1*8, 2+1*8, /*  9, 10 */
    1+2*8, 2+2*8, /* 17, 18 */

    /* Cr */
    1+4*8, 2+4*8, /* 33, 34 */
    1+5*8, 2+5*8, /* 41, 42 */
};
/*
变换矩阵如下,先luma亮度,然后chroma(色度) b chroma(色度) r,都是从一个2x2小块开始,raster(光栅) scan
其中L块先第一44块raster scane 再第二44块 scane
每一个块的左边和上边是空出来的,用来存放left和top mb的 4x4小块的intra pre mode
*/

/*
   0 1 2 3 4 5 6 7
 0
 1   B B   L L L L
 2   B B   L L L L
 3         L L L L
 4   R R   L L L L
 5   R R
*/

typedef struct x264_ratecontrol_t   x264_ratecontrol_t;//ratecontrol.c(85):struct x264_ratecontrol_t
typedef struct x264_vlc_table_t     x264_vlc_table_t;

//x264_t结构体维护着CODEC的诸多重要信息
struct x264_t
{
    /* encoder parameters ( 编码器参数 )*/
    x264_param_t    param;

	//一个片对应一个x264_t(必须定义HAVE_PTHREAD时才启用多线程好像)
    x264_t *thread[X264_SLICE_MAX];	//#define X264_SLICE_MAX 4	//片/条带最大数为4
									//在x264_encoder_open中会动态分配结构体内存，并将地址存在此数组
									//    h->thread[0] = h;
									//	h->i_thread_num = 0;
									//	for( i = 1; i < h->param.i_threads; i++ )
									//		h->thread[i] = x264_malloc( sizeof(x264_t) );//为其它
    /* bitstream output ( bit流输出 ) 
	 * x264_encoder_open根据i_bitstream动态分配一大块内存，首地址存在p_bitstream
	 * 写sps时，先调用x264_nal_start( h, NAL_SPS, NAL_PRIORITY_HIGHEST );
	*/
    struct
    {
        int         i_nal;				//out里有x264_nal_t对象数组，i_nal指示现在操作第几个数组元素
        x264_nal_t  nal[X264_NAL_MAX]; 	//#define X264_NAL_MAX (4 + X264_SLICE_MAX)
        int         i_bitstream;        /* 1000000或更大 size of p_bitstream */
        uint8_t     *p_bitstream;       /* 将存放所有nal的数据  will hold包含 data for all nal
										 * 有x264_encoder_open中动态分配内存，地址存在它里
										*/
        bs_t        bs;	                /* common/bs.h里定义，存储和读取数据的一个结构体,5个字段，指明了开始和结尾地址，当前读取位置等 */
    } out;

    /* frame number/poc ( 帧序号 )*/
    int             i_frame;

    int             i_frame_offset; /* decoding only */
    int             i_frame_num;    /* decoding only */	//用作图像标识符，如果当前图像是一个IDR图像， frame_num 应等于0，标准第76页
    int             i_poc_msb;      /* decoding only */	//MSB 最高有效位
    int             i_poc_lsb;      /* decoding only */	//LSB 最低有效位
    int             i_poc;          /* decoding only */

    int             i_thread_num;   /* threads only */
    int             i_nal_type;     /* threads only */	//指明当前NAL_unit的类型。0未使用，1不分区、非IDR图像的片，2、3、4片分区A、B、C。5IDR图像中的片，6补充增强信息单元(SEI)，7序列参数集，8图像参数集，9分界符，10，序列结束，11码流结束，12填充。毕厚杰159页
    int             i_nal_ref_idc;  /* threads only */ //nal_ref_idc，指示当前NAL的优先级，取值范围[0,1,2,3]，值越高，表示当前NAL越重要，越需要优先受到保护，H.264规定如果当前NAL是一个序列参数集，或者是一个图像参数集，或者属于参考图像的片或片分区等重要的数据单位时，本句法元素必须大于0。但是大于0时具体该取何值，并没有进一步的规定，通信双方可以灵活地制定策略。当nal_unit_type等于5时，nal_ref_idc大于0；nal_unit_type等于6、9、10、11或12时，nal_ref_idc等于0。毕厚杰159页

    /* We use only one SPS(序列参数集) and one PPS(图像参数集) 
	   我们只使用一个序列参数集和一个图像参数集
	*/
    x264_sps_t      sps_array[1];	//结构体的数组。set.h里定义的结构体
    x264_sps_t      *sps;
    x264_pps_t      pps_array[1];	//结构体的数组。set.h里定义的结构体
    x264_pps_t      *pps;
    int             i_idr_pic_id;

    int             (*dequant4_mf[4])[4][4]; /* [4][6][4][4] */
    int             (*dequant8_mf[2])[8][8]; /* [2][6][8][8] */
    int             (*quant4_mf[4])[4][4];   /* [4][6][4][4] */
    int             (*quant8_mf[2])[8][8];   /* [2][6][8][8] */
    int             (*unquant4_mf[4])[16];   /* [4][52][16] */
    int             (*unquant8_mf[2])[64];   /* [2][52][64] */

    uint32_t        nr_residual_sum[2][64];	/* residual:残余 */
    uint32_t        nr_offset[2][64];		/* offset:偏移 */
    uint32_t        nr_count[2];

    /* Slice header (片头部) */
    x264_slice_header_t sh;

    /* cabac(适应性二元算术编码) context */
    x264_cabac_t    cabac;	/* 结构体,cabac.h里定义 */

    struct
    {
        /* Frames to be encoded (whose types have been decided(明确的) ) */
        x264_frame_t *current[X264_BFRAME_MAX+3];//current是已经准备就绪可以编码的帧，其类型已经确定
        /* Temporary(临时的) buffer (frames types not yet decided) */
        x264_frame_t *next[X264_BFRAME_MAX+3];//next是尚未确定类型的帧
        /* 未使用的帧Unused frames */
        x264_frame_t *unused[X264_BFRAME_MAX+3];//unused用于回收不使用的frame结构体以备今后再次使用
        /* For adaptive(适应的) B decision(决策) */
        x264_frame_t *last_nonb;

        /* frames used for reference +1 for decoding + sentinels (发射器,标记) */
        x264_frame_t *reference[16+2+1+2];//指明当前正在编码的图像的参考图像，最大可以有21个参考图像？

        int i_last_idr; /* 最后的IDR图像的序号，当新编码了一个IDR时，会更新此变量 */

        int i_input;    /* 当前输入的帧的总数(比如从文件中读取给编码150帧了) */

        int i_max_dpb;  /* Number of frames allocated (分配) in the decoded picture buffer */
        int i_max_ref0;
        int i_max_ref1;
        int i_delay;    //i_delay设置为由B帧个数（线程个数）确定的帧缓冲延迟，在多线程情况下为i_delay = i_bframe + i_threads - 1。而判断B帧缓冲填充是否足够则通过条件判断：h->frames.i_input <= h->frames.i_delay + 1 - h->param.i_threads。 /* Number of frames buffered for B reordering (重新排序) */
        int b_have_lowres;  /* Whether 1/2 resolution (分辨率) luma(亮度) planes(平面) are being used */
    } frames;//指示和控制帧编码过程的结构

    /* 正被编码的当前帧 current frame being encoded */
    x264_frame_t    *fenc;//存放的是YCbCr(420)格式的画面 http://wmnmtm.blog.163.com/blog/static/3824571420118188540701/

    /* 正在被重建的帧 frame being reconstructed(重建的) */
    x264_frame_t    *fdec;////存放的是YCbCr(420)格式的画面 http://wmnmtm.blog.163.com/blog/static/3824571420118188540701/

    /* references(参考) lists */
    int             i_ref0;
    x264_frame_t    *fref0[16+3];     /* ref list 0 */
    int             i_ref1;
    x264_frame_t    *fref1[16+3];     /* ref list 1 */
    int             b_ref_reorder[2];



    /* Current MB DCT coeffs(估计系数) */
    struct
    {
        DECLARE_ALIGNED( int, luma16x16_dc[16], 16 );
        DECLARE_ALIGNED( int, chroma_dc[2][4], 16 );
        // FIXME merge(合并) with union(结合,并集)
        DECLARE_ALIGNED( int, luma8x8[4][64], 16 );
        union
        {
            DECLARE_ALIGNED( int, residual_ac[15], 16 );
            DECLARE_ALIGNED( int, luma4x4[16], 16 );
        } block[16+8];
    } dct;

    /* MB table and cache(高速缓冲存储器) for current frame/mb */
    struct
    {
        int     i_mb_count;	/* number of mbs in a frame */ /* 在一帧中的宏块总数 */

        /* Strides (跨,越) */
        int     i_mb_stride;//以16x16宏块为宽度的图像的宽度 （就是图像一行有几个宏块）
        int     i_b8_stride;
        int     i_b4_stride;

        /* 
		Current index 当前宏块的索引
		资料：http://wmnmtm.blog.163.com/blog/static/382457142011885411173/
		*/
        int     i_mb_x;//当前宏块在X坐标轴上的位置
        int     i_mb_y;//当前宏块在Y坐标轴上的位置
        int     i_mb_xy;//当前宏块是第几个宏块
        int     i_b8_xy;//以8X8的时候是第几个宏块 （目的是为了计算出他相邻的宏块的位置，找到相邻宏块的相关信息，以此来估计当前宏块的处理方式）
        int     i_b4_xy;//（同上）
        
        /* Search parameters (搜索参数) */
        int     i_me_method;			//luma运动估计方法
        int     i_subpel_refine;		//亚像素运动估计质量 运动估计的精度
        int     b_chroma_me;			//色度是否进行运动估计
        int     b_trellis;				//trellis：格子
        int     b_noise_reduction;		//是否降噪 noise:噪声;reduction:减少 /*自适应伪盲区 */

        /* Allowed qpel(四分之一映像点) MV range to stay (继续,停留) within the picture + emulated(模拟) edge (边) pixels */
        int     mv_min[2];	//MV的范围
        int     mv_max[2];
        /* Subpel亚像素 MV运动 range范围 for motion运动 search搜索.
         * same mv_min/max but includes levels' i_mv_range. */
        int     mv_min_spel[2];	//半像素的范围
        int     mv_max_spel[2];
        /* Fullpel MV range for motion search 全像素mv范围，运动搜索*/
        int     mv_min_fpel[2];	//整像素的搜索范围
        int     mv_max_fpel[2];

        /* neighboring MBs 相邻宏块的信息 是否可用*/
        unsigned int i_neighbour;
        unsigned int i_neighbour8[4];       /* ??? neighbours邻近 of each每个 8x8 or 4x4 block块 that are available可用的 */
        unsigned int i_neighbour4[16];      /* ??? at the time the block is coded编码 */
        int     i_mb_type_top;				/* 宏块类型：上 */
        int     i_mb_type_left;				/* 宏块类型：左 */		
        int     i_mb_type_topleft; 			/* 宏块类型：左上 */
        int     i_mb_type_topright; 		/* 宏块类型：右上 */

		/*		
		1 2 3 
		4 C 5
		6 7 8
		C为当前宏块，在它的周围的宏块数共有8个
		上、左、左上、右上		
		*/

        /* mb table 宏块表 */
        int8_t  *type;                      /* 指向一个存储其他已经确定宏块类型的宏块类型表格 mb type */
        int8_t  *qp;                        /* 同上，不过这里是存储的量化参数 mb qp */
        int16_t *cbp;                       /* 同上 这里是宏块残差的编码方式 mb cbp: 0x0?: luma, 0x?0: chroma, 0x100: luma dc, 0x0200 and 0x0400: chroma dc  (all set for PCM)*/
        int8_t  (*intra4x4_pred_mode)[7];   /* 每个宏块预测模式 intra4x4 pred mode. for non I4x4 set to I_PRED_4x4_DC(2) */
        uint8_t (*non_zero_count)[16+4+4];  /* 每个宏块的非零系数 nzc. for I_PCM set to 16 */
        int8_t  *chroma_pred_mode;          /* 每个宏块色度的预测模式 chroma_pred_mode. cabac only. for non intra I_PRED_CHROMA_DC(0) */
        int16_t (*mv[2])[2];                /* 每个实际的mv   mb mv. set to 0 for intra mb */
        int16_t (*mvd[2])[2];               /* 每个 残差 mb mv difference with predict. set to 0 if intra. cabac only */
        int8_t   *ref[2];                   /* 参考列表的指针 mb ref. set to -1 if non used (intra or Lx only) */
        int16_t (*mvr[2][16])[2];           /* 16x16 mv for each每个 possible可能的 ref参考 */
        int8_t  *skipbp;                    /* block pattern形式 for SKIP跳过; or DIRECT直接 (sub)mbs. B-frames + cabac前文参考之适应性二元算术编码 only */
        int8_t  *mb_transform_size;         /* (变换)transform_size_8x8_flag of each mb */
		/* 以上是每个宏块都有的 */

        /* current value 当前值 */
        int     i_type;				//当前宏块的类型
        int     i_partition;		//当前宏块的划分模式
        int     i_sub_partition[4];	//子宏块的划分模式
        int     b_transform_8x8;	//是否8X8变化

		//CBP详解：http://wmnmtm.blog.163.com/blog/static/38245714201181611143750/ cbp用于表示当前宏块是否存在非零值,cbp一共6bit，高2bit表示cbpc(2：cb、cr中至少一个4x4块的AC系数不全为0；1：cb、cr中至少一个2x2的DC系数不全为0；0：所有色度系数全0）,低4bit分别表示4个8x8亮度块，其中从最低一位开始的4位分别对应00，10，01，11位置的8*8亮度块。如果某位为1，表示该对应8*8块的4个4*4块中至少有一个的系数不全为0。

        int     i_cbp_luma;		//当前宏块的亮度数据 残差编码模式
        int     i_cbp_chroma;	//当前宏块的色度数据 残差编码模式

        int     i_intra16x16_pred_mode;//当前宏块帧内预测模式 帧内16x16预测模式
        int     i_chroma_pred_mode;//色度预测模式

        struct
        {
            /* space for p_fenc and p_fdec 空间for当前编码帧和重建帧*/
#define FENC_STRIDE 16
#define FDEC_STRIDE 32
            DECLARE_ALIGNED( uint8_t, fenc_buf[24*FENC_STRIDE], 16 );
            DECLARE_ALIGNED( uint8_t, fdec_buf[27*FDEC_STRIDE], 16 );

            /* pointer over mb of the frame to be compressed */
            uint8_t *p_fenc[3];

            /* pointer over mb of the frame to be reconstrucated  */
            uint8_t *p_fdec[3];

            /* pointer over mb of the references */
            uint8_t *p_fref[2][16][4+2]; /* last: lN, lH, lV, lHV, cU, cV */
            uint16_t *p_integral[2][16];

            /* fref stride 参考帧跨度？*/
            int     i_stride[3];
        } pic;

        /* cache 进行预测模式选择的时候需要的缓存空间 */
        struct
        {
            /* real intra4x4_pred_mode if I_4X4 or I_8X8, I_PRED_4x4_DC if mb available, -1 if not */
            int     intra4x4_pred_mode[X264_SCAN8_SIZE];

            /* i_non_zero_count if availble else 0x80 */
            int     non_zero_count[X264_SCAN8_SIZE];

            /* -1 if unused, -2 if unavaible难以获得的 */
            int8_t  ref[2][X264_SCAN8_SIZE];

            /* 0 if non avaible */
            int16_t mv[2][X264_SCAN8_SIZE][2];
            int16_t mvd[2][X264_SCAN8_SIZE][2];

            /* 1 if SKIP跳过 or DIRECT直接的. set only for B-frames + CABAC */
            int8_t  skip[X264_SCAN8_SIZE];

            int16_t direct_mv[2][X264_SCAN8_SIZE][2];
            int8_t  direct_ref[2][X264_SCAN8_SIZE];

            /* number of neighbors (top and left) that used 8x8 dct */
            int     i_neighbour_transform_size;	//
            int     b_transform_8x8_allowed;	//
        } cache;

        /* 码率控制 量化参数 */
        int     i_qp;			/* current qp */
        int     i_last_qp;		/* 上一量化参数 last qp */
        int     i_last_dqp;		/* 量化参数差 last delta qp */
        int     b_variable_qp;	/*  whether是否 qp is allowed to vary变化 per根据 macroblock */
        int     b_lossless;		//是否无损编码
        int     b_direct_auto_read; /* take stats for --direct auto from the 2pass log */
        int     b_direct_auto_write; /* analyse解析 direct直接 modes模式, to use and/or save */

        /* B_direct and weighted加权的 prediction预测 */
        int     dist_scale_factor[16][16];//加权预测
        int     bipred_weight[16][16];
        /* maps fref1[0]'s ref indices into the current list0 */
        int     map_col_to_list0_buf[2]; // for negative拒绝 indices(索引,index的复数)
        int     map_col_to_list0[16];
    } mb;

    /* 速率控制编码使用 rate control encoding only */
    x264_ratecontrol_t *rc;//struct x264_ratecontrol_t,ratecontrol.c中定义

    /* stats */
    struct
    {
        /* Current frame stats */
        struct
        {
            /* Headers bits (MV+Ref+MB Block Type */
            int i_hdr_bits;
            /* Texture bits (Intra/Predicted) */
            int i_itex_bits;
            int i_ptex_bits;
            /* ? */
            int i_misc_bits;
            /* MB type counts */
            int i_mb_count[19];
            int i_mb_count_i;
            int i_mb_count_p;
            int i_mb_count_skip;
            int i_mb_count_8x8dct[2];//宏块数_8x8DCT
            int i_mb_count_size[7];
            int i_mb_count_ref[16];
            /* Estimated (SATD) cost as Intra/Predicted frame */
            /* XXX: both omit the cost of MBs coded as P_SKIP */
            int i_intra_cost;
            int i_inter_cost;
            /* Adaptive direct mv pred */
            int i_direct_score[2];
        } frame;

        /* Cummulated stats */

        /* per(每个) slice info */
        int     i_slice_count[5];
        int64_t i_slice_size[5];
        int     i_slice_qp[5];
        /* */
        int64_t i_sqe_global[5];
        float   f_psnr_average[5];
        float   f_psnr_mean_y[5];
        float   f_psnr_mean_u[5];
        float   f_psnr_mean_v[5];
        /* */
        int64_t i_mb_count[5][19];
        int64_t i_mb_count_8x8dct[2];
        int64_t i_mb_count_size[2][7];
        int64_t i_mb_count_ref[2][16];
        /* */
        int     i_direct_score[2];//score:得分,成绩,大量；很多
        int     i_direct_frames[2];

    } stat;

    /* CPU functions dependants(家属) */
    x264_predict_t      predict_16x16[4+3];//predict:预测
    x264_predict_t      predict_8x8c[4+3];
    x264_predict8x8_t   predict_8x8[9+3];
    x264_predict_t      predict_4x4[9+3];

    x264_pixel_function_t pixf;
    x264_mc_functions_t   mc;
    x264_dct_function_t   dctf;
    x264_csp_function_t   csp;//x264_csp_function_t是一个结构体，在common/csp.h中定义
    x264_quant_function_t quantf;//在common/quant.h中定义
    x264_deblock_function_t loopf;//在common/frame.h中定义

    /* vlc table for decoding(解码) purpose(1. 目的; 意图2. 作用; 用途; 效果) only */
    x264_vlc_table_t *x264_coeff_token_lookup[5];
    x264_vlc_table_t *x264_level_prefix_lookup;
    x264_vlc_table_t *x264_total_zeros_lookup[15];
    x264_vlc_table_t *x264_total_zeros_dc_lookup[3];
    x264_vlc_table_t *x264_run_before_lookup[7];

#if VISUALIZE
    struct visualize_t *visualize;//visualize:设想, 想像, 构想
#endif
};

void    x264_test_my1();
void    x264_test_my2(char * name);

//在最后包含它是因为它需要x264_t
// included at the end because it needs x264_t
#include "macroblock.h"

#endif

