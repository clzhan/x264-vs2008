#ifndef _X264_VFW_H
#define _X264_VFW_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <windows.h>
#include <vfw.h>

#include <x264.h>

#include "resource.h"

/* Name */
#define X264_NAME_L     L"x264"
#define X264_DESC_L     L"x264 - H264/AVC encoder"

/* Codec fcc */
#define FOURCC_X264 mmioFOURCC('X','2','6','4')		
/*
为了简化RIFF文件中的4字符标识的读写与比较，Windows SDK在多媒体头文件mmsystem.h中定义了类型FOURCC(Four-Character Code四字符代码)：
typedef DWORD FOURCC;
及其构造宏(用于将4个字符转换成一个FOURCC数据)
FOURCC mmioFOURCC(CHAR ch0, CHAR ch1, CHAR ch2, CHAR ch3);
其定义为MAKEFOURCC宏：
#define mmioFOURCC(ch0, ch1, ch2, ch3) MAKEFOURCC(ch0, ch1, ch2, ch3);
而MAKEFOURCC宏定义为：
#define MAKEFOURCC(ch0, ch1, ch2, ch3) ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 )); 

*/

/* yuv 4:2:0 planar */
#define FOURCC_I420 mmioFOURCC('I','4','2','0')		/* 将4个字符转换成一个FOURCC数据 */
#define FOURCC_IYUV mmioFOURCC('I','Y','U','V')		/* (Four-Character Code四字符代码) */
#define FOURCC_YV12 mmioFOURCC('Y','V','1','2')		/*  */

/* yuv 4:2:2 packed */
#define FOURCC_YUY2 mmioFOURCC('Y','U','Y','2')
#define FOURCC_YUYV mmioFOURCC('Y','U','Y','V')

#define X264_WEBSITE    "http://videolan.org/x264.html"

/* CONFIG: vfw config
 */
typedef struct
{
    /********** ATTENTION(注意) **********/
    int mode;                   /* Vidomi directly accesses these vars */
    int bitrate;
    int desired_size;           /* please try to avoid(避免) modifications(修改) here(这儿) */
    char stats[MAX_PATH];
    /*******************************/
    int i_2passbitrate;
    int i_pass;

    int b_fast1pass;    /* turns off(关掉) some flags during(在…期间, 当…之时) 1st pass */    
    int b_updatestats;  /* updates the statsfile (统计数据) during 2nd pass */

    int i_frame_total;	/* 数据帧数量 */

    /* Our config */
    int i_refmax;
    int i_keyint_max;
    int i_keyint_min;
    int i_scenecut_threshold;
    int i_qp_min;
    int i_qp_max;
    int i_qp_step;

    int i_qp;
    int b_filter;

    int b_cabac;

    int b_i8x8;
    int b_i4x4;
    int b_psub16x16;
    int b_psub8x8;
    int b_bsub16x16;
    int b_dct8x8;
    int b_mixedref;

    int i_bframe;
    int i_subpel_refine;
    int i_me_method;
    int i_me_range;
    int b_chroma_me;
    int i_direct_mv_pred;
    int i_threads;

    int i_inloop_a;
    int i_inloop_b;

    int b_b_refs;
    int b_b_wpred;
    int i_bframe_bias;
    int b_bframe_adaptive;

    int i_key_boost;
    int i_b_red;
    int i_curve_comp;

    int i_sar_width;
    int i_sar_height;

    int i_log_level;

    /* vfw interface(接口) */
    int b_save;
    /* fourcc used */
    char fcc[4+1];
    int i_encoding_type;
    int i_trellis;
    int b_bidir_me;
    int i_noise_reduction;	/* noise:噪声 reduction:减少*/
} CONFIG;

/* 
 * CODEC: vfw codec instance(实例)
 * 
 * 
*/
typedef struct
{
    CONFIG config;

    /* handle */
    x264_t *h;

    /* error console(控制台) handle(处理) */
    HWND *hCons;

    /* XXX: needed ? */
    unsigned int fincr;
    unsigned int fbase;
} CODEC;

/* Compress(压缩) functions(函数) */
LRESULT compress_query(CODEC *, BITMAPINFO *, BITMAPINFO *);		/* query:查询 */
LRESULT compress_get_format(CODEC *, BITMAPINFO *, BITMAPINFO *);	/* 压缩_取_格式 */
LRESULT compress_get_size(CODEC *, BITMAPINFO *, BITMAPINFO *);		/* 压缩_取_尺寸 */
LRESULT compress_frames_info(CODEC *, ICCOMPRESSFRAMES *);			/* 压缩_帧_信息 */
LRESULT compress_begin(CODEC *, BITMAPINFO *, BITMAPINFO *);		/* 压缩_开始 */
LRESULT compress_end(CODEC *);										/* 压缩_结束 */
LRESULT compress(CODEC *, ICCOMPRESS *);							/* 压缩 */

/* config(设置) functions(函数) */
void config_reg_load( CONFIG * config );	/* 注册_载入 */
void config_reg_save( CONFIG * config );	/* 注册_保存 */
static void tabs_enable_items( HWND hDlg, CONFIG * config );	/* enable:使能够 */
static void tabs_update_items( HWND hDlg, CONFIG * config );	/*  */

/* Dialog(对话框) callbacks(回调) */
BOOL CALLBACK callback_main ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK callback_tabs( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK callback_about( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK callback_err_console( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );

/* Dll instance(例子, 实例) */
extern HINSTANCE g_hInst;

#if defined(_DEBUG)
	#include <stdio.h> /* vsprintf */
	#define DPRINTF_BUF_SZ  1024
	static __inline void DPRINTF(char *fmt, ...)
	{
		va_list args;
		char buf[DPRINTF_BUF_SZ];

		va_start(args, fmt);
		vsprintf(buf, fmt, args);
		OutputDebugString(buf);
	}
#else
	static __inline void DPRINTF(char *fmt, ...) { }
#endif


#endif

