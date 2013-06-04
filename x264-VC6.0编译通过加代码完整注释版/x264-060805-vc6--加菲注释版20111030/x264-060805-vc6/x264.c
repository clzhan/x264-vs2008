/*****************************************************************************
 * x264: h264 encoder/decoder testing program.
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: x264.c,v 1.1 2004/06/03 19:24:12 fenrir Exp $
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

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <signal.h>
#define _GNU_SOURCE
#include <getopt.h>		/* 其中有一个变量extern char *optarg;实际是在getopt.c中定义的 */

#ifdef _MSC_VER
#include <io.h>			/* _setmode() */
#include <fcntl.h>		/* _O_BINARY  */
#endif

#ifndef _MSC_VER
#include "config.h"		/* 文件夹下无此文件 */
#endif

#include "common/common.h"	/*  */
#include "x264.h"
#include "muxers.h"			/* 文件操作，如mkv,mp4,avi,yuv,... */

#define DATA_MAX 3000000	//数据的最大长度
uint8_t data[DATA_MAX];		//实际存放数据的数组 unsigned char data[3000000] ，即3000000*8比特

/* Ctrl+C 组合键 */
static int     b_ctrl_c = 0;				/* CTRL+C ，按下此组合键时，更新此变量为1 */
static int     b_exit_on_ctrl_c = 0;		/* 是否要在 CTRL+C 组合键 按下时退出程序 */	
static void    SigIntHandler( int a )		/* 信号捕捉函数 ，在main()中用到，它捕捉CTRL+C组合键 */
{
    if( b_exit_on_ctrl_c )	/* 如果启用CTRL+C组合键 */				
        exit(0);			/* 整个程序的结束 */
    b_ctrl_c = 1;			/* 我按下了CTRL+C */
}							//在main(...)中，注册了这个信号捕捉函数，是否在按下CTRL+C时退出，取决于变量b_exit_on_ctrl_c

typedef struct {
    int b_progress;		//是否显示编码进度。取值为0,1
    int i_seek;			//表示开始从哪一帧编码
    hnd_t hin;			//Hin 指向输入yuv文件的指针	//void *在C语言里空指针是有几个特性的，他是一个一般化指针，可以指向任何一种类型，但却不能解引用，需要解引用的时候，需要进行强制转换。采用空指针的策略，应该是为了声明变量的简便和统一。
    hnd_t hout;			//Hout 指向编码过后生成的文件的指针
    FILE *qpfile;		//Qpfile 是一个指向文件类型的指针，他是文本文件，其每一行的格式是framenum frametype QP,用于强制指定某些帧或者全部帧的帧类型和QP(quant param量化参数)的值

} cli_opt_t;	/* 此结构体是记录一些与编码关系较小的设置信息的 */

/* 
 * 输入文件操作的函数指针  input file operation function pointers 
 * 打开输入文件
 * 取总帧数
 * 读帧内容
 * 关闭输入文件
*/

int (*p_open_infile)( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param );
int (*p_get_frame_total)( hnd_t handle );
int (*p_read_frame)( x264_picture_t *p_pic, hnd_t handle, int i_frame );	/* !!!!!! */
int (*p_close_infile)( hnd_t handle );

/* 
 * 输出文件操作的函数指针  output file operation function pointers 
 * 打开 输出文件
 * 设置 输出文件参数
 * 写   nalu
 * 设置 
 * 关闭 输出文件
*/
static int (*p_open_outfile)( char *psz_filename, hnd_t *p_handle );
static int (*p_set_outfile_param)( hnd_t handle, x264_param_t *p_param );
static int (*p_write_nalu)( hnd_t handle, uint8_t *p_nal, int i_size );
static int (*p_set_eop)( hnd_t handle, x264_picture_t *p_picture );
static int (*p_close_outfile)( hnd_t handle );

static void Help( x264_param_t *defaults, int b_longhelp );//帮助
static int  Parse( int argc, char **argv, x264_param_t *param, cli_opt_t *opt );//解析命令行参数
static int  Encode( x264_param_t *param, cli_opt_t *opt );//编码


/****************************************************************************
 * main: 程序入口点
 * 参数：
 * http://wmnmtm.blog.163.com
 *
 ****************************************************************************/
int main( int argc, char **argv )
{
	//定义两个结构体
    x264_param_t param;
    cli_opt_t opt;	/*一点设置*/

	#ifdef _MSC_VER							//stdin在STDIO.H，是系统定义的
    _setmode(_fileno(stdin), _O_BINARY);	//_setmode(_fileno(stdin), _O_BINARY)功能是将stdin流(或其他文件流）从文本模式   <--切换-->   二进制模式 就是stdin流(或其他文件流）从文本模式   <--切换-->   二进制模式
    _setmode(_fileno(stdout), _O_BINARY);
	#endif

	//对编码器参数进行设定，初始化结构体对象
    x264_param_default( &param );	//(common/common.c中定义)

    /* 解析命令行,完成文件打开 */
    if( Parse( argc, argv, &param, &opt ) < 0 )	/* 就是把用户通过命令行提供的参数保存到两个结构体中，未提供的参数还以x264_param_default函数设置的值为准 */
        return -1;

    /* 用函数signal注册一个信号捕捉函数 实现“Ctrl+C”退出程序之功能 */
    signal( SIGINT/*要捕捉的信号*/, SigIntHandler/*信号捕捉函数*/ );//用函数signal注册一个信号捕捉函数，第1个参数signum表示要捕捉的信号，第2个参数是个函数指针，表示要对该信号进行捕捉的函数，该参数也可以是SIG_DEF(表示交由系统缺省处理，相当于白注册了)或SIG_IGN(表示忽略掉该信号而不做任何处理)。signal如果调用成功，返回以前该信号的处理函数的地址，否则返回 SIG_ERR。
									//sighandler_t是信号捕捉函数，由signal函数注册，注册以后，在整个进程运行过程中均有效，并且对不同的信号可以注册同一个信号捕捉函数。该函数只有一个参数，表示信号值。
	printf("\n");
	printf("************************************");	printf("\n");
	printf("**   http://wmnmtm.blog.163.com   **");	printf("\n");
	printf("************************************");	printf("\n");
	/* 开始编码*/
    return Encode( &param, &opt );	//把两个参数提供给Encode，而它们已经保存上了命令行的参数,此函数在 x264.c 中定义 
									//Encode内部循环调用Encode_frame对帧编码
}

/* 取字符串的第i个字符 */
static char const *strtable_lookup( const char * const table[], int index )//static静态函数，只在定义它的文件内有效
{
    int i = 0; 
	while( table[i] )	/* 当table[i]为0时，即遇到字符串末尾时退出 */
		i++;			/* 计数字符串中字符的个数 */
    return ( ( index >= 0 && index < i ) ? table[ index ] : "???" ); 
						/*
						index >=0 ，是说调用本函数时指定的index必须合情合理
						index < i，是保证指定的index不能超出字符串中字符的个数
						当前提的这两个条件满足时，反回table[index]，即一个字符'*'
						如果table[index]为非，则返回"???"
						*/
}

/*****************************************************************************

 *****************************************************************************/
/*
 * Help:
 * D:\test>x2641 --longhelp
   用--longhelp参数时，上面的H0和H1全部输出
   用--longhelp参数时，上面的H1不输出，仅输出H0
*/
static void Help( x264_param_t *defaults, int b_longhelp )
{
	#define H0 printf							//int printf(const char *format,[argument]); 
	#define H1 if(b_longhelp) printf
    H0( "x264 core:%d%s\n"																											/* x264 core:49 */
        "Syntax: x264 [options] -o outfile infile [宽x高]\n"																	/* Syntax: x264 [options] -o outfile infile [widthxheight] */
        "\n"
        "Infile can be raw YUV 4:2:0 (in which case resolution is required),\n"														/* Infile can be raw YUV 4:2:0 (in which case resolution is required),*/
        "  or YUV4MPEG 4:2:0 (*.y4m),\n"																							/*  or YUV4MPEG 4:2:0 (*.y4m), */
        "  or AVI or Avisynth if compiled with AVIS support (%s).\n"																/*  or AVI or Avisynth if compiled with AVIS support (no). */
        "Outfile type is selected by filename:\n"																					/* Outfile type is selected by filename: */
        " .264 -> Raw bytestream\n"																									/* .264 -> Raw bytestream */
        " .mkv -> Matroska\n"																										/* .mkv -> Matroska */
        " .mp4 -> MP4 if compiled with GPAC support (%s)\n"																			/* .mp4 -> MP4 if compiled with GPAC support (no) */
        "\n"																														/* 换行 */
        "Options:\n"																												/* Options: */
        "\n"																														/* 换行 */
        "  -h, --help                  List the more commonly used options\n"														/*   -h, --help                  List the more commonly used options */
        "      --longhelp              List all options\n"																			/*       --longhelp              List all options */
        "\n",																														/* 换行 */
        X264_BUILD, X264_VERSION,																									/* ??? */
		#ifdef AVIS_INPUT
				"yes",
		#else
				"no",
		#endif
		#ifdef MP4_OUTPUT
				"yes"
		#else
				"no"
		#endif
      );
    H0( "Frame-type options:\n" );				/* Frame-type options: */
    H0( "\n" );
    H0( "  -I, --keyint <integer>      Maximum GOP size [%d]\n", defaults->i_keyint_max );											/* [H0] */  /*  -I, --keyint <integer>      Maximum GOP size [250] */
    H1( "  -i, --min-keyint <integer>  Minimum GOP size [%d]\n", defaults->i_keyint_min );											/* [H1] */  /*  -i, --min-keyint <integer>  Minimum GOP size [25] */
    H1( "      --scenecut <integer>    How aggressively to insert extra I-frames [%d]\n", defaults->i_scenecut_threshold );			/* [H1] */  /*      --scenecut <integer>    How aggressively to insert extra I-frames [40] */
    H0( "  -b, --bframes <integer>     Number of B-frames between I and P [%d]\n", defaults->i_bframe );							/* [H0] */  /*  -b, --bframes <integer>     Number of B-frames between I and P [0] */
    H1( "      --no-b-adapt            Disable adaptive B-frame decision\n" );														/* [H1] */  /*      --no-b-adapt            Disable adaptive B-frame decision */
    H1( "      --b-bias <integer>      Influences how often B-frames are used [%d]\n", defaults->i_bframe_bias );					/* [H1] */  /*      --b-bias <integer>      Influences how often B-frames are used [0] */
    H0( "      --b-pyramid             Keep some B-frames as references\n" );														/* [H0] */  /*      --b-pyramid             Keep some B-frames as references */
    H0( "      --no-cabac              Disable CABAC\n" );																			/* [H0] */  /*      --no-cabac              Disable CABAC */
    H0( "  -r, --ref <integer>         Number of reference frames [%d]\n", defaults->i_frame_reference );							/* [H0] */  /*  -r, --ref <integer>         Number of reference frames [1] */
    H1( "      --nf                    Disable loop filter\n" );																	/* [H1] */  /*      --b-pyramid             Keep some B-frames as references */
    H0( "  -f, --filter <alpha:beta>   Loop filter AlphaC0 and Beta parameters [%d:%d]\n",											/* [H0] */  /*  -f, --filter <alpha:beta>   Loop filter AlphaC0 and Beta parameters [0:0] */
                                       defaults->i_deblocking_filter_alphac0, defaults->i_deblocking_filter_beta );					/* ??? */
    H0( "\n" );																														/* 换行 */
    H0( "Ratecontrol:\n" );																											/* [H0] */  /*Ratecontrol: */
    H0( "\n" );																														/* 换行 */
    H0( "  -q, --qp <integer>          Set QP (0=lossless) [%d]\n", defaults->rc.i_qp_constant );									/* [H0] */  /*  -q, --qp <integer>          Set QP (0=lossless) [26] */
    H0( "  -B, --bitrate <integer>     Set bitrate (kbit/s)\n" );																	/* [H0] */  /*  -B, --bitrate <integer>     Set bitrate (kbit/s) */
    H0( "      --crf <integer>         Quality-based VBR (nominal QP)\n" );															/* [H0] */  /*      --crf <integer>         Quality-based VBR (nominal QP) */
    H1( "      --vbv-maxrate <integer> Max local bitrate (kbit/s) [%d]\n", defaults->rc.i_vbv_max_bitrate );						/* [H1] */  /*       --vbv-maxrate <integer> Max local bitrate (kbit/s) [0] */
    H0( "      --vbv-bufsize <integer> Enable CBR and set size of the VBV buffer (kbit) [%d]\n", defaults->rc.i_vbv_buffer_size );	/* [H0] */  /*      --vbv-bufsize <integer> Enable CBR and set size of the VBV buffer (kbit) [0] */
    H1( "      --vbv-init <float>      Initial VBV buffer occupancy [%.1f]\n", defaults->rc.f_vbv_buffer_init );					/* [H1] */  /*       --vbv-init <float>      Initial VBV buffer occupancy [0.9] */
    H1( "      --qpmin <integer>       Set min QP [%d]\n", defaults->rc.i_qp_min );													/* [H1] */  /*       --qpmin <integer>       Set min QP [10] */
    H1( "      --qpmax <integer>       Set max QP [%d]\n", defaults->rc.i_qp_max );													/* [H1] */  /*       --qpmax <integer>       Set max QP [51] */
    H1( "      --qpstep <integer>      Set max QP step [%d]\n", defaults->rc.i_qp_step );											/* [H1] */  /*       --qpstep <integer>      Set max QP step [4] */
    H0( "      --ratetol <float>       Allowed variance of average bitrate [%.1f]\n", defaults->rc.f_rate_tolerance );				/* [H0] */  /*      --ratetol <float>       Allowed variance of average bitrate [1.0] */
    H0( "      --ipratio <float>       QP factor between I and P [%.2f]\n", defaults->rc.f_ip_factor );								/* [H0] */  /*      --ipratio <float>       QP factor between I and P [1.40] */
    H0( "      --pbratio <float>       QP factor between P and B [%.2f]\n", defaults->rc.f_pb_factor );								/* [H0] */  /*      --pbratio <float>       QP factor between P and B [1.30] */
    H1( "      --chroma-qp-offset <integer>  QP difference between chroma and luma [%d]\n", defaults->analyse.i_chroma_qp_offset );	/* [H1] */  /*       --chroma-qp-offset <integer>  QP difference between chroma and luma [0] */
    H0( "\n" );																														/* 换行 */
    H0( "  -p, --pass <1|2|3>          Enable multipass ratecontrol\n"																/* [H0] */  /*  -p, --pass <1|2|3>          Enable multipass ratecontrol */
        "                                  - 1: First pass, creates stats file\n"													/* [H0] */  /*                                  - 1: First pass, creates stats file */
        "                                  - 2: Last pass, does not overwrite stats file\n"											/* [H0] */  /*                                  - 2: Last pass, does not overwrite stats file */
        "                                  - 3: Nth pass, overwrites stats file\n" );												/* [H0] */  /*                                  - 3: Nth pass, overwrites stats file */
    H0( "      --stats <string>        Filename for 2 pass stats [\"%s\"]\n", defaults->rc.psz_stat_out );							/* [H0] */  /*      --stats <string>        Filename for 2 pass stats ["x264_2pass.log"] */
    H1( "      --rceq <string>         Ratecontrol equation [\"%s\"]\n", defaults->rc.psz_rc_eq );									/* [H1] */  /*       --rceq <string>         Ratecontrol equation ["blurCplx^(1-qComp)"] */
    H0( "      --qcomp <float>         QP curve compression: 0.0 => CBR, 1.0 => CQP [%.2f]\n", defaults->rc.f_qcompress );			/* [H0] */  /*      --qcomp <float>         QP curve compression: 0.0 => CBR, 1.0 => CQP [0.60] */
    H1( "      --cplxblur <float>      Reduce fluctuations in QP (before curve compression) [%.1f]\n", defaults->rc.f_complexity_blur );	/* [H1]      --cplxblur <float>      Reduce fluctuations in QP (before curve compression) [20.0] */
    H1( "      --qblur <float>         Reduce fluctuations in QP (after curve compression) [%.1f]\n", defaults->rc.f_qblur );		/* [H1] */  /*       --qblur <float>         Reduce fluctuations in QP (after curve compression) [0.5] */
    H0( "      --zones <zone0>/<zone1>/...  Tweak the bitrate of some regions of the video\n" );									/* [H0] */  /*      --zones <zone0>/<zone1>/...  Tweak the bitrate of some regions of the video */
    H1( "                              Each zone is of the form\n"																	/* [H1] */  /*                               Each zone is of the form */
        "                                  <start frame>,<end frame>,<option>\n"													/* [H1] */  /*                                   <start frame>,<end frame>,<option> */
        "                                  where <option> is either\n"																/* [H1] */  /*                                   where <option> is either */
        "                                      q=<integer> (force QP)\n"															/* [H1] */  /*                                       q=<integer> (force QP) */
        "                                  or  b=<float> (bitrate multiplier)\n" );													/* [H1] */  /*                                   or  b=<float> (bitrate multiplier) */
    H1( "      --qpfile <string>       Force frametypes and QPs\n" );																/* [H1] */  /*       --qpfile <string>       Force frametypes and QPs */
    H0( "\n" );																														/* 换行 */
    H0( "Analysis:\n" );																											/* [H0] */  /* Analysis: */
    H0( "\n" );																														/* 换行 */
    H0( "  -A, --analyse <string>      Partitions to consider [\"p8x8,b8x8,i8x8,i4x4\"]\n"											/* [H0] */  /*  -A, --analyse <string>      Partitions to consider ["p8x8,b8x8,i8x8,i4x4"] */
        "                                  - p8x8, p4x4, b8x8, i8x8, i4x4\n"														/* [H0] */  /*                                  - p8x8, p4x4, b8x8, i8x8, i4x4 */
        "                                  - none, all\n"																			/* [H0] */  /*                                  - none, all */
        "                                  (p4x4 requires p8x8. i8x8 requires --8x8dct.)\n" );										/* [H0] */  /*                                  (p4x4 requires p8x8. i8x8 requires --8x8dct.) */
    H0( "      --direct <string>       Direct MV prediction mode [\"%s\"]\n"														/* [H0] */  /*      --direct <string>       Direct MV prediction mode ["spatial"] */
        "                                  - none, spatial, temporal, auto\n",														/* [H0] */  /*                                  - none, spatial, temporal, auto */
                                       strtable_lookup( x264_direct_pred_names, defaults->analyse.i_direct_mv_pred ) );				/* ??? */
    H0( "  -w, --weightb               Weighted prediction for B-frames\n" );														/* [H0] */  /*  -w, --weightb               Weighted prediction for B-frames */
    H0( "      --me <string>           Integer pixel motion estimation method [\"%s\"]\n",											/* [H0] */  /*      --me <string>           Integer pixel motion estimation method ["hex"] */
                                       strtable_lookup( x264_motion_est_names, defaults->analyse.i_me_method ) );					/* [H0] */  /*                                  - dia, hex, umh */
    H1( "                                  - dia: diamond search, radius 1 (fast)\n"												/* [H1] */  /*                                   - dia: diamond search, radius 1 (fast) */
        "                                  - hex: hexagonal search, radius 2\n"														/* [H1] */  /*                                   - hex: hexagonal search, radius 2 */
        "                                  - umh: uneven multi-hexagon search\n"													/* [H1] */  /*                                   - umh: uneven multi-hexagon search */
        "                                  - esa: exhaustive search (slow)\n" );													/* [H1]  */  /*                                  - esa: exhaustive search (slow) */
    else			/*这个else好像不起作用似的，它上面和下面的都会同时输出，当然--help不输出H1，--longhelp时，else上面和下面的都会输出，else没任何影响*/																												/* else */
	H0( "                                  - dia, hex, umh\n" );																	/* [H0] */  /*                                  - dia, hex, umh */
    H0( "      --merange <integer>     Maximum motion vector search range [%d]\n", defaults->analyse.i_me_range );					/* [H0] */  /*      --merange <integer>     Maximum motion vector search range [16] */
    H0( "  -m, --subme <integer>       Subpixel motion estimation and partition\n"													/* [H0] */  /*  -m, --subme <integer>       Subpixel motion estimation and partition */
        "                                  decision quality: 1=fast, 7=best. [%d]\n", defaults->analyse.i_subpel_refine );			/* [H0] */  /*                                  decision quality: 1=fast, 7=best. [5] */
    H0( "      --b-rdo                 RD based mode decision for B-frames. Requires subme 6.\n" );									/* [H0] */  /*      --b-rdo                 RD based mode decision for B-frames. Requires subme 6. */
    H0( "      --mixed-refs            Decide references on a per partition basis\n" );												/* [H0] */  /*      --mixed-refs            Decide references on a per partition basis */
    H1( "      --no-chroma-me          Ignore chroma in motion estimation\n" );														/* [H1] */  /*       --no-chroma-me          Ignore chroma in motion estimation */
    H1( "      --bime                  Jointly optimize both MVs in B-frames\n" );													/* [H1] */  /*       --bime                  Jointly optimize both MVs in B-frames */
    H0( "  -8, --8x8dct                Adaptive spatial transform size\n" );														/* [H0] */  /*  -8, --8x8dct                Adaptive spatial transform size */
    H0( "  -t, --trellis <integer>     Trellis RD quantization. Requires CABAC. [%d]\n"												/* [H0] */  /*  -t, --trellis <integer>     Trellis RD quantization. Requires CABAC. [0] */
        "                                  - 0: disabled\n"																			/* [H0] */  /*                                  - 0: disabled */
        "                                  - 1: enabled only on the final encode of a MB\n"											/* [H0] */  /*                                  - 1: enabled only on the final encode of a MB */
        "                                  - 2: enabled on all mode decisions\n", defaults->analyse.i_trellis );					/* [H0] */  /*                                  - 2: enabled on all mode decisions */
    H0( "      --no-fast-pskip         Disables early SKIP detection on P-frames\n" );												/* [H0] */  /*      --no-fast-pskip         Disables early SKIP detection on P-frames */
    H0( "      --no-dct-decimate       Disables coefficient thresholding on P-frames\n" );											/* [H0] */  /*      --no-dct-decimate       Disables coefficient thresholding on P-frames */
    H0( "      --nr <integer>          Noise reduction [%d]\n", defaults->analyse.i_noise_reduction );								/* [H0] */  /*      --nr <integer>          Noise reduction [0] */
    H1( "\n" );																														/* 换行 */
    H1( "      --cqm <string>          Preset quant matrices [\"flat\"]\n"		/*预置量化矩阵*/									/* [H1] */  /*       --cqm <string>          Preset quant matrices ["flat"] */
        "                                  - jvt, flat\n" );																		/* [H1] */  /*                                   - jvt, flat */
    H0( "      --cqmfile <string>      Read custom quant matrices from a JM-compatible file\n" );									/* [H0] */  /*       --cqmfile <string>      Read custom quant matrices from a JM-compatible file */
    H1( "                                  Overrides any other --cqm* options.\n" );												/* [H1] */  /*                                   Overrides any other --cqm* options. */
    H1( "      --cqm4 <list>           Set all 4x4 quant matrices\n"																/* [H1] */  /*       --cqm4 <list>           Set all 4x4 quant matrices */
        "                                  Takes a comma-separated list of 16 integers.\n" );										/* [H1] */  /*                                  Takes a comma-separated list of 16 integers. */
    H1( "      --cqm8 <list>           Set all 8x8 quant matrices\n"																/* [H1] */  /*       --cqm8 <list>           Set all 8x8 quant matrices */
        "                                  Takes a comma-separated list of 64 integers.\n" );										/* [H1] */  /*                                  Takes a comma-separated list of 64 integers. */
    H1( "      --cqm4i, --cqm4p, --cqm8i, --cqm8p\n"																				/* [H1] */  /*       --cqm4i, --cqm4p, --cqm8i, --cqm8p */
        "                              Set both luma and chroma quant matrices\n" );												/* [H1] */  /*                               Set both luma and chroma quant matrices */
    H1( "      --cqm4iy, --cqm4ic, --cqm4py, --cqm4pc\n"																			/* [H1] */  /*       --cqm4iy, --cqm4ic, --cqm4py, --cqm4pc */
        "                              Set individual quant matrices\n" );															/* [H1] */  /*                               Set individual quant matrices */
    H1( "\n" );																														/* 换行 */
    H1( "Video Usability Info (Annex E):\n" );																						/* [H1] */  /* Video Usability Info (Annex E): */
    H1( "The VUI settings are not used by the encoder but are merely suggestions to\n" );											/* [H1] */  /* The VUI settings are not used by the encoder but are merely suggestions to */
    H1( "the playback equipment. See doc/vui.txt for details. Use at your own risk.\n" );											/* [H1] */  /* the playback equipment. See doc/vui.txt for details. Use at your own risk. */
    H1( "\n" );
    H1( "      --overscan <string>     Specify crop overscan setting [\"%s\"]\n"													/* [H1] */  /*       --overscan <string>     Specify crop overscan setting ["undef"] */
        "                                  - undef, show, crop\n",																	/* [H1] */  /*                                   - undef, show, crop */
                                       strtable_lookup( x264_overscan_names, defaults->vui.i_overscan ) );							/* [H1] */
    H1( "      --videoformat <string>  Specify video format [\"%s\"]\n"																/* [H1] */  /*       --videoformat <string>  Specify video format ["undef"] */
        "                                  - component, pal, ntsc, secam, mac, undef\n",											/* [H1] */  /*                                   - component, pal, ntsc, secam, mac, undef */
                                       strtable_lookup( x264_vidformat_names, defaults->vui.i_vidformat ) );						/* [H1] */
    H1( "      --fullrange <string>    Specify full range samples setting [\"%s\"]\n"												/* [H1] */  /*       --fullrange <string>    Specify full range samples setting ["off"] */
        "                                  - off, on\n",																			/* [H1] */  /*                                   - off, on */
                                       strtable_lookup( x264_fullrange_names, defaults->vui.b_fullrange ) );						/* [H1] */
    H1( "      --colorprim <string>    Specify color primaries [\"%s\"]\n"															/* [H1] */  /*       --colorprim <string>    Specify color primaries ["undef"] */
        "                                  - undef, bt709, bt470m, bt470bg\n"														/* [H1] */  /*                                   - undef, bt709, bt470m, bt470bg */
        "                                    smpte170m, smpte240m, film\n",															/* [H1] */  /*                                     smpte170m, smpte240m, film */
                                       strtable_lookup( x264_colorprim_names, defaults->vui.i_colorprim ) );						/* [H1] */
    H1( "      --transfer <string>     Specify transfer characteristics [\"%s\"]\n"													/* [H1] */  /*       --transfer <string>     Specify transfer characteristics ["undef"] */
        "                                  - undef, bt709, bt470m, bt470bg, linear,\n"												/* [H1] */  /*                                   - undef, bt709, bt470m, bt470bg, linear, */
        "                                    log100, log316, smpte170m, smpte240m\n",												/* [H1] */  /*                                     log100, log316, smpte170m, smpte240m */
                                       strtable_lookup( x264_transfer_names, defaults->vui.i_transfer ) );							/* [H1]  */
    H1( "      --colormatrix <string>  Specify color matrix setting [\"%s\"]\n"														/* [H1] */  /*       --colormatrix <string>  Specify color matrix setting ["undef"] */
        "                                  - undef, bt709, fcc, bt470bg\n"															/* [H1] */  /*                                   - undef, bt709, fcc, bt470bg */
        "                                    smpte170m, smpte240m, GBR, YCgCo\n",													/* [H1] */  /*                                     smpte170m, smpte240m, GBR, YCgCo */
                                       strtable_lookup( x264_colmatrix_names, defaults->vui.i_colmatrix ) );						/* [H1] */
    H1( "      --chromaloc <integer>   Specify chroma sample location (0 to 5) [%d]\n",												/* [H1] */  /*       --chromaloc <integer>   Specify chroma sample location (0 to 5) [0] */
                                       defaults->vui.i_chroma_loc );																/* [H1] */
    H0( "\n" );																														/* 换行 */
    H0( "Input/Output:\n" );																										/* [H0] */  /* Input/Output: */
    H0( "\n" );																														/* 换行 */
    H0( "  -o, --output                Specify output file\n" );																	/* [H0] */  /*   -o, --output                Specify output file */
    H0( "      --sar width:height      Specify Sample Aspect Ratio\n" );															/* [H0] */  /*       --sar width:height      Specify Sample Aspect Ratio */
    H0( "      --fps <float|rational>  Specify framerate\n" );																		/* [H0] */  /*       --fps <float|rational>  Specify framerate */
    H0( "      --seek <integer>        First frame to encode\n" );																	/* [H0] */  /*       --seek <integer>        First frame to encode */
    H0( "      --frames <integer>      Maximum number of frames to encode\n" );														/* [H0] */  /*       --frames <integer>      Maximum number of frames to encode */
    H0( "      --level <string>        Specify level (as defined by Annex A)\n" );													/* [H0] */  /*       --level <string>        Specify level (as defined by Annex A) */
    H0( "\n" );																														/* 换行 */
    H0( "  -v, --verbose               Print stats for each frame\n" );																/* [H0] */  /*   -v, --verbose               Print stats for each frame */
    H0( "      --progress              Show a progress indicator while encoding\n" );												/* [H0] */  /*       --progress              Show a progress indicator while encoding */
    H0( "      --quiet                 Quiet Mode\n" );																				/* [H0] */  /*       --quiet                 Quiet Mode */
    H0( "      --no-psnr               Disable PSNR computation\n" );																/* [H0] */  /*       --no-psnr               Disable PSNR computation */
    H0( "      --threads <integer>     Parallel encoding (uses slices)\n" );														/* [H0] */  /*       --threads <integer>     Parallel encoding (uses slices) */
    H0( "      --thread-input          Run Avisynth in its own thread\n" );															/* [H0] */  /*       --thread-input          Run Avisynth in its own thread */
    H1( "      --no-asm                Disable all CPU optimizations\n" );															/* [H1] */  /*       --no-asm                Disable all CPU optimizations */
    H1( "      --visualize             在被编码的视频上显示覆盖的宏块类型Show MB types overlayed on the encoded video\n" );											/* [H1] */  /*       --visualize             Show MB types overlayed on the encoded video */
    H1( "      --sps-id <integer>      Set SPS and PPS id numbers [%d]\n", defaults->i_sps_id );									/* [H1] */  /*       --sps-id <integer>      Set SPS and PPS id numbers [0] */
    H1( "      --aud                   Use access unit delimiters\n" );																/* [H1] */  /*       --aud                   Use access unit delimiters */
    H0( "\n" );																														/* 换行 */
}


/*****************************************************************************
 * Parse				：解析命令行参数
 * 参数					：
 * int argc				：参数个数
 * char **argv			：参数串
 * x264_param_t *param	：从参数中取到值后存到这里面，如：param->i_log_level = X264_LOG_NONE;
 * cli_opt_t *opt		：从参数中取到值后存到这里面，如：opt->i_seek = atoi( optarg );
 *****************************************************************************/
static int  Parse( int argc, char **argv,
                   x264_param_t *param, cli_opt_t *opt )
{
    char *psz_filename = NULL;			/* 用来存解析到的文件名 */
    x264_param_t defaults = *param;		/* 本函数内部定义一个对象，生命期只限本函数内部 main(...)：{x264_param_t param;x264_param_default( &param );Parse( argc, argv, &param, &opt );} */
    char *psz;							/*  */
    int b_avis = 0;						/* ? .avi或者是.avs */
    int b_y4m = 0;						/* ? .y4m */
    int b_thread_input = 0;				/*  */

    memset( opt, 0, sizeof(cli_opt_t) );/* 内存块设置00000 (opt指针所指对象实际已经分配了内存，这儿只是初始化一下这块内存) */

    /* 
	Default input file driver 
	=左侧的都是函数指针，在x264.c(即本文件)开始处定义的
	=右侧的都是在“\muxers.c中定义的函数，此处用法依据：函数名即函数指针”
	*/
    p_open_infile = open_file_yuv;				/*函数指针赋值*/
    p_get_frame_total = get_frame_total_yuv;	//p_get_frame_total是一个函数指针，右侧的get_frame_total_yuv是一个函数名称；//muxers.c(82):int get_frame_total_yuv( hnd_t handle )
    p_read_frame = read_frame_yuv;				/*函数指针赋值*/
    p_close_infile = close_file_yuv;			/*函数指针赋值*/

    /*
	Default output file driver 
	输出文件的处理函数，后面会对.mkv和.mp4另外指定处理函数，这儿是默认的普遍函数，未另指定的后缀格式，都用这些来处理http://wmnmtm.blog.163.com/blog/static/38245714201181824344687/
	*/
    p_open_outfile = open_file_bsf;				/*函数指针赋值*/
    p_set_outfile_param = set_param_bsf;		/*函数指针赋值*/
    p_write_nalu = write_nalu_bsf;				/*函数指针赋值*/
    p_set_eop = set_eop_bsf;					/*函数指针赋值*/
    p_close_outfile = close_file_bsf;			/*函数指针赋值*/
	//printf("\n (x264.c\Parse(...) 系列函数指针赋值：) p_open_outfile p_close_outfile");//zjh

    /* 解析命令行参数 Parse command line options */
    for( ;; )//for循环中的"初始化"、"条件表达式"和"增量"都是选择项, 即可以缺省, 但";"不能缺省。省略了初始化, 表示不对循环控制变量赋初值。 省略了条件表达式, 则不做其它处理时便成为死循环。省略了增量, 则不对循环控制变量进行操作, 这时可在语句体中加入修改循环控制变量的语句。
    {
        int b_error = 0;
        int long_options_index = -1;

		#define OPT_FRAMES 256
		#define OPT_SEEK 257
		#define OPT_QPFILE 258
		#define OPT_THREAD_INPUT 259
		#define OPT_QUIET 260
		#define OPT_PROGRESS 261
		#define OPT_VISUALIZE 262
		#define OPT_LONGHELP 263

		/*option类型的结构体数组*/ /*struct定义的是一种类型，并不分配内存空间*/
        static struct option long_options[] =				/* option本身是一个结构体类型，它有4个字段，所以下面每对花括号中有4项 */
        {
			/* 选项名称	 要求参数?          flag   值  */

            { "help",    no_argument,       NULL, 'h' },	/* {参数名称,是否有参数,标记,值} */	
            { "longhelp",no_argument,       NULL, OPT_LONGHELP },
            { "version", no_argument,       NULL, 'V' },	/* argument:参数 */
            { "bitrate", required_argument, NULL, 'B' },	/* # define required_argument	1 */
            { "bframes", required_argument, NULL, 'b' },	/* # define no_argument		0	  */
            { "no-b-adapt", no_argument,    NULL, 0 },		/* # define optional_argument	2 */
            { "b-bias",  required_argument, NULL, 0 },
            { "b-pyramid", no_argument,     NULL, 0 },
            { "min-keyint",required_argument,NULL,'i' },
            { "keyint",  required_argument, NULL, 'I' },
            { "scenecut",required_argument, NULL, 0 },
            { "nf",      no_argument,       NULL, 0 },
            { "filter",  required_argument, NULL, 'f' },
            { "no-cabac",no_argument,       NULL, 0 },
            { "qp",      required_argument, NULL, 'q' },
            { "qpmin",   required_argument, NULL, 0 },
            { "qpmax",   required_argument, NULL, 0 },
            { "qpstep",  required_argument, NULL, 0 },			//argument:参数
            { "crf",     required_argument, NULL, 0 },			/* crf */
            { "ref",     required_argument, NULL, 'r' },
            { "no-asm",  no_argument,       NULL, 0 },
            { "sar",     required_argument, NULL, 0 },
            { "fps",     required_argument, NULL, 0 },
            { "frames",  required_argument, NULL, OPT_FRAMES },	/* #define OPT_FRAMES 256 */
            { "seek",    required_argument, NULL, OPT_SEEK },	/* #define OPT_SEEK 257 */
            { "output",  required_argument, NULL, 'o' },
            { "analyse", required_argument, NULL, 'A' },
            { "direct",  required_argument, NULL, 0 },
            { "weightb", no_argument,       NULL, 'w' },
            { "me",      required_argument, NULL, 0 },
            { "merange", required_argument, NULL, 0 },
            { "subme",   required_argument, NULL, 'm' },
            { "b-rdo",   no_argument,       NULL, 0 },
            { "mixed-refs", no_argument,    NULL, 0 },
            { "no-chroma-me", no_argument,  NULL, 0 },
            { "bime",    no_argument,       NULL, 0 },
            { "8x8dct",  no_argument,       NULL, '8' },
            { "trellis", required_argument, NULL, 't' },
            { "no-fast-pskip", no_argument, NULL, 0 },
            { "no-dct-decimate", no_argument, NULL, 0 },
            { "level",   required_argument, NULL, 0 },
            { "ratetol", required_argument, NULL, 0 },
            { "vbv-maxrate", required_argument, NULL, 0 },
            { "vbv-bufsize", required_argument, NULL, 0 },
            { "vbv-init", required_argument,NULL,  0 },
            { "ipratio", required_argument, NULL, 0 },
            { "pbratio", required_argument, NULL, 0 },
            { "chroma-qp-offset", required_argument, NULL, 0 },
            { "pass",    required_argument, NULL, 'p' },
            { "stats",   required_argument, NULL, 0 },
            { "rceq",    required_argument, NULL, 0 },
            { "qcomp",   required_argument, NULL, 0 },
            { "qblur",   required_argument, NULL, 0 },
            { "cplxblur",required_argument, NULL, 0 },
            { "zones",   required_argument, NULL, 0 },
            { "qpfile",  required_argument, NULL, OPT_QPFILE },
            { "threads", required_argument, NULL, 0 },
            { "thread-input", no_argument,  NULL, OPT_THREAD_INPUT },
            { "no-psnr", no_argument,       NULL, 0 },
            { "quiet",   no_argument,       NULL, OPT_QUIET },
            { "verbose", no_argument,       NULL, 'v' },
            { "progress",no_argument,       NULL, OPT_PROGRESS },
            { "visualize",no_argument,      NULL, OPT_VISUALIZE },
            { "sps-id",  required_argument, NULL, 0 },
            { "aud",     no_argument,       NULL, 0 },
            { "nr",      required_argument, NULL, 0 },
            { "cqm",     required_argument, NULL, 0 },
            { "cqmfile", required_argument, NULL, 0 },
            { "cqm4",    required_argument, NULL, 0 },
            { "cqm4i",   required_argument, NULL, 0 },
            { "cqm4iy",  required_argument, NULL, 0 },
            { "cqm4ic",  required_argument, NULL, 0 },
            { "cqm4p",   required_argument, NULL, 0 },
            { "cqm4py",  required_argument, NULL, 0 },
            { "cqm4pc",  required_argument, NULL, 0 },
            { "cqm8",    required_argument, NULL, 0 },
            { "cqm8i",   required_argument, NULL, 0 },
            { "cqm8p",   required_argument, NULL, 0 },
            { "overscan", required_argument, NULL, 0 },
            { "videoformat", required_argument, NULL, 0 },
            { "fullrange", required_argument, NULL, 0 },
            { "colorprim", required_argument, NULL, 0 },
            { "transfer", required_argument, NULL, 0 },
            { "colormatrix", required_argument, NULL, 0 },
            { "chromaloc", required_argument, NULL, 0 },
            {0, 0, 0, 0}
        };	/* 此数组告诉后面，需要解析这么多"参数名称"，并且指明了每个参数是否要求"参数" */

        int c = getopt_long( argc, argv, "8A:B:b:f:hI:i:m:o:p:q:r:t:Vvw",/* 被冒号分成13项 */
                             long_options, &long_options_index);
							//解析入口地址的向量，最后c 得到的是 运行参数（“-o test.264 foreman.yuv  352x288”）中前面“-o”中“o”的ASCII值 即 c = 111 。可通过VC Debug查看。 getopt_long() 定义在getopt.c中。其中用到 getopt_internal(nargc, nargv, options)也定义在extras/getopt.c中，解析入口地址向量。
							/* 在getopt.c中定义 */
							/* 前两个参数是main从操作系统得到的，一个是int一个是字符串 */
		//printf("\n(x264.c) getopt_long的返回值c = %c \n",c);//zjh
		//printf("%d \n",c);//zjh

        if( c == -1 )//
        {
            break;//退出for
        }

		

        switch( c )	//int c //参数o最复杂，放在最后处理
        {
            case 'h':	/* 帮助基本 */
                Help( &defaults, 0 );		//显示帮助信息
                exit(0);					//exit函数中的实参是返回给操作系统，表示程序是成功运行结束还是失败运行结束。对于程序本身的使用没有什么太实际的差别。习惯上，一般使用正常结束程序exit(0)。exit(非0）：非正常结束程序运行
            case OPT_LONGHELP:	/* 帮助详细 */
                Help( &defaults, 1 );		//显示帮助信息(较多的)
                exit(0);					//STDLIB.H里声明
            case 'V':	/* 版本信息 */
				#ifdef X264_POINTVER
					printf( "x264 "X264_POINTVER"\n" );		//产生格式化输出的函数(定义在 stdio.h 中)。
				#else
					printf( "x264 0.%d.X\n", X264_BUILD );	/* x264 0.49.X ||(#define X264_BUILD 49)*/
				#endif
                exit(0);
            case OPT_FRAMES:	/* 256 */	//--frames <整数> 最大编码帧数
                param->i_frame_total = atoi( optarg );//atoi(const char *);把字符串转换成整型数 函数说明: 参数nptr字符串，如果第一个非空格字符不存在或者不是数字也不是正负号则返回零，否则开始做类型转换，之后检测到非数字或结束符 \0 时停止转换，返回整型数。
                break;
            case OPT_SEEK://命令行：--seek <整数> 设定起始帧  //

                opt->i_seek = atoi( optarg );
                break;
            case 'o':			//对运行参数(“-o test.264 foreman.yuv  352x288”)由c = 111 ,程序跳转到case 'o'，执行p_open_outfile ( optarg, &opt->hout )，即进入函数open_file_bsf（），功能为以二进制写的方式打开输出文件test.264，函数在nuxers.c中
								/*
								命令行示例：
								x264 --crf 22 --profile main --tune animation --preset medium --b-pyramid none 
								-o test.mp4 oldFile.avi
								如上，-o后有两个文件名，第一个是输出的新文件，第二个是原始文件
								*/
				//mp4文件
                if( !strncasecmp(optarg + strlen(optarg) - 4, ".mp4", 4) )	/* strncasecmp:比较字符串s1和s2的前n个字符但不区分大小写 */
                {
					#ifdef MP4_OUTPUT
						p_open_outfile = open_file_mp4;//muxers.c
						p_write_nalu = write_nalu_mp4;
						p_set_outfile_param = set_param_mp4;
						p_set_eop = set_eop_mp4;
						p_close_outfile = close_file_mp4;
						printf("\n (x264.c\case 'o')");//zjh
					#else
						fprintf( stderr, "x264 [error]: not compiled with MP4 output support\n" );
						return -1;
					#endif
                }
				//MKV文件
                else if( !strncasecmp(optarg + strlen(optarg) - 4, ".mkv", 4) )//int strncasecmp(const char *s1, const char *s2, size_t n) 用来比较参数s1和s2字符串前n个字符，比较时会自动忽略大小写的差异, 若参数s1和s2字符串相同则返回0; s1若大于s2则返回大于0的值; s1若小于s2则返回小于0的值
                {	
                    p_open_outfile = open_file_mkv;//muxers.c
                    p_write_nalu = write_nalu_mkv;
                    p_set_outfile_param = set_param_mkv;
                    p_set_eop = set_eop_mkv;
                    p_close_outfile = close_file_mkv;
                }
                if( !strcmp(optarg, "-") )
                    opt->hout = stdout;
                else if( p_open_outfile( optarg, &opt->hout ) )	/* 打开文件，接口是这个函数指针，实际调用适当的打开文件的函数 */
                {
                    fprintf( stderr, "x264 [error]: can't open output file `%s'\n", optarg );
                    return -1;
                }
                break;
            case OPT_QPFILE:
                opt->qpfile = fopen( optarg, "r" );
                if( !opt->qpfile )
                {
                    fprintf( stderr, "x264 [error]: can't open `%s'\n", optarg );
                    return -1;
                }
                param->i_scenecut_threshold = -1;//threshold:阈值  场景切换阈值=-1
                param->b_bframe_adaptive = 0;	//b帧适应=0 adaptive:适应的
                break;
            case OPT_THREAD_INPUT:
                b_thread_input = 1;
                break;
            case OPT_QUIET:
                param->i_log_level = X264_LOG_NONE;
                param->analyse.b_psnr = 0;
                break;
            case 'v':
                param->i_log_level = X264_LOG_DEBUG;
                break;
            case OPT_PROGRESS:
                opt->b_progress = 1;
                break;
            case OPT_VISUALIZE:				//VISUALIZE:设想
				#ifdef VISUALIZE
					param->b_visualize = 1;	
					b_exit_on_ctrl_c = 1;
				#else
					fprintf( stderr, "x264 [warning]: not compiled with visualization support\n" );
				#endif
                break;
            default:
            {
                int i;
                if( long_options_index < 0 )
                {
                    for( i = 0; long_options[i].name; i++ )
                        if( long_options[i].val == c )
                        {
                            long_options_index = i;	/* long_options_index在这儿实现了++ */
                            break;
                        }
                    if( long_options_index < 0 )
                    {
                        /* getopt_long already printed an error message */
                        return -1;
                    }
                }

                b_error |= x264_param_parse( param, long_options[long_options_index].name, optarg ? optarg : "true" );	/* 从数组里读出.name，然后解析 common/common.c中定义*/
							/*
							main()
							{
							...
							x264_param_t param;
							...
							}
														

							Parse()
							{
							int x264_param_parse( x264_param_t *p, const char *name, const char *value )
							}

							*/
            }
        }

        if( b_error )
        {
            const char *name = long_options_index > 0 ? long_options[long_options_index].name : argv[optind-2];
            fprintf( stderr, "x264 [error]: invalid argument: %s = %s\n", name, optarg );
            return -1;
        }
    }

    /* 获取文件名 Get the file name */
    if( optind > argc - 1 || !opt->hout )
    {
        fprintf( stderr, "x264 [error]: No %s file. Run x264 --help for a list of options.\n",
                 optind > argc - 1 ? "input" : "output" );
				/*
				D:\test>x2641 --no-b-adapt
				x264 [error]: No input file. Run x264 --help for a list of options.
				*/
        return -1;
    }
    psz_filename = argv[optind++];	/* 文件名 */

    /* check demuxer(匹配) type (muxer是合并将视频文件、音频文件和字幕文件合并为某一个视频格式。如，可将a.avi, a.mp3, a.srt用muxer合并为mkv格式的视频文件。demuxer是拆分这些文件的。)*/
    psz = psz_filename + strlen(psz_filename) - 1;	/* 计算字符串s的(unsigned int型)长度,不包括'\0'在内, strlen所作的仅仅是一个计数器的工作，它从内存的某个位置（可以是字符串开头，中间某个位置，甚至是某个不确定的内存区域）开始扫描，直到碰到第一个字符串结束符'\0'为止，然后返回计数器值。 */
    while( psz > psz_filename && *psz != '.' ) //psz_filename + strlen(psz_filename) - 1 > psz_filename
	{
        psz--;	/* psz是什么类型啊，怎么--了 */
	}//从后续代码看，这个while循环能取出文件名的后缀来，不过这个while循环是看不懂
	/* 实际验证，psz到这儿已经是后缀了，验证过程：
	http://wmnmtm.blog.163.com/blog/static/382457142011813638733/ 
	http://wmnmtm.blog.163.com/blog/static/3824571420118151053445/
	http://wmnmtm.blog.163.com/blog/static/3824571420118194830397/  abc100x200def.yuv 100x200.yuv
	*/
    if( !strncasecmp( psz, ".avi", 4 ) || !strncasecmp( psz, ".avs", 4 ) )	/* 后缀：.avi或.avs AVS是AviSynth的简称。意思是AVI合成器，就是一个把影像文件从一个程序转换到另外一个程序的过程 , 其间没有临时文件或中介文件产生。对于Rip的人来说，AVS就是基础，就是关键。像VirtualDubMod、MeGuid之类的强大压缩软件都必须通过AVS载入。它的工作过程：新建个文本文件，然后将后缀改为.avs（文件名可任意，但后缀必须是.avs）如：01.txt->01.avs。AVS文件中包含的是一行行的特定命令的文本，称之为"脚本"。*/
        b_avis = 1;	/* 是.avi或.avs后缀 */									/* .avs: http://baike.baidu.com/view/63605.htm */
    if( !strncasecmp( psz, ".y4m", 4 ) )	/* .y4m也是一种文件格式 另：http://media.xiph.org/video/derf 有很多y4m的序列 */
        b_y4m = 1;	/* 是.y4m后缀 */		/* 布尔类型的变量 */
	/*
　　表头文件：#include <string.h> 　　
	函数定义：int strncasecmp(const char *s1, const char *s2, size_t n)
　　函数说明：strncasecmp()用来比较参数s1和s2字符串前n个字符，比较时会自动忽略大小写的差异 　　
	返回值 ：若参数s1和s2字符串相同则返回0 s1若大于s2则返回大于0的值 s1若小于s2则返回小于0的值
	*/
    if( !(b_avis || b_y4m) ) /* 两个变量都==0，即不是.avi，也不是.avs ，也不是.y4m */ // raw(天然的; 未加工过的) yuv
    {						
        if( optind > argc - 1 ) /* ??? */
        {
            /* 
			try to parse the file name 
			尝试解析文件名
			一般的for格式：for (int i=0;i<10;i++)
			for( psz = psz_filename; *psz; psz++ )
			for( psz = psz_filename; *psz不为0; psz++ )，当*psz为假(即*psz==0 字符串结尾是\0)时退出循环

			C/C++ char*类型字符串的结尾符可以赋值为如下两个字符，一个是'\0'，而另一个是数字的0，请问这两个在计算机或者一些特殊的环境下有区别没有呢？至于特殊环境我也不知道，什么称为特殊环境，只是想知道他们之间到底有没有却别。谢谢。
			正如我之前的邮件中，所提到的当把整数0强行赋值给char是，在编译器内部隐式的进行了一次类型转换（char*）0, 这样就把整数0转变为一个地址为空的指针。这就是使得’\0’与0 在数值上相等。所以得到的结果相同
			*/
            for( psz = psz_filename; *psz; psz++ )	/* psz在此处重新赋值了，又变成了完整的文件名xxxx.yuv */
            {
                if( *psz >= '0' && *psz <= '9'
                    && sscanf( psz, "%ux%u", &param->i_width, &param->i_height ) == 2 )	/* 验证：http://wmnmtm.blog.163.com/blog/static/3824571420118175830667/ */
                {
                    if( param->i_log_level >= X264_LOG_INFO )
                        fprintf( stderr, "x264 [info]: file name gives %dx%d\n", param->i_width, param->i_height );
                    break;
                }
            }
        }
        else
        {
            sscanf( argv[optind++], "%ux%u", &param->i_width, &param->i_height );	/* sscanf() - 从一个字符串中读进与指定格式相符的数据。*/


        }
    }
        
    if( !(b_avis || b_y4m) && ( !param->i_width || !param->i_height ) )	/* !(b_avis || b_y4m)表示不是这几种后缀，即代表是yuv ；( !param->i_width || !param->i_height )表示前面没有取到这两个变量值 */
    {
        fprintf( stderr, "x264 [error]: Rawyuv input requires a resolution(分辨率).\n" );
        return -1;
    }

    /* open the input
	打开输入
	*/
    {
        if( b_avis )	/* .avi或.avs */
        {
		#ifdef AVIS_INPUT
            p_open_infile = open_file_avis;				/* muxers.c中的函数 int open_file_avis( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param ) */
            p_get_frame_total = get_frame_total_avis;	/* muxers.c中的函数 int get_frame_total_avis( hnd_t handle ) */
            p_read_frame = read_frame_avis;				/* muxers.c中的函数 int read_frame_avis( x264_picture_t *p_pic, hnd_t handle, int i_frame ) */
            p_close_infile = close_file_avis;			/* muxers.c中的函数 int close_file_avis( hnd_t handle ) */
		#else
            fprintf( stderr, "x264 [error]: not compiled(编译) with AVIS input support\n" );
            return -1;
		#endif
        }
        if ( b_y4m )	/* .y4m */
        {
            p_open_infile = open_file_y4m;				/* muxers.c中的函数 int open_file_y4m( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param ) */
            p_get_frame_total = get_frame_total_y4m;	/* muxers.c中的函数 int get_frame_total_y4m( hnd_t handle ) */
            p_read_frame = read_frame_y4m;				/* muxers.c中的函数 int read_frame_y4m( x264_picture_t *p_pic, hnd_t handle, int i_frame ) */
            p_close_infile = close_file_y4m;			/* muxers.c中的函数 int close_file_y4m(hnd_t handle) */
        }

        if( p_open_infile( psz_filename, &opt->hin, param ) )	/* p_open_infile是一个函数指针，可能指向不同的函数，也就是说，这句可能是不同的函数的调用 */
        {
            fprintf( stderr, "x264 [error]: could not open input file '%s'\n", psz_filename );
            return -1;
        }//到这儿，已经打开视频文件了
    }

	#ifdef HAVE_PTHREAD
    if( b_thread_input || param->i_threads > 1 )
    {
        if( open_file_thread( NULL, &opt->hin, param ) )	/* muxers.c中的函数 int open_file_thread( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param ) */
        {
            fprintf( stderr, "x264 [warning]: threaded input failed\n" );
        }
        else
        {
            p_open_infile = open_file_thread;				/* muxers.c中的函数 int open_file_thread( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param ) */
            p_get_frame_total = get_frame_total_thread;		/* muxers.c中的函数 int get_frame_total_thread( hnd_t handle ) */
            p_read_frame = read_frame_thread;				/* muxers.c中的函数 void read_frame_thread_int( thread_input_arg_t *i ) 和 int read_frame_thread( x264_picture_t *p_pic, hnd_t handle, int i_frame ) */
            p_close_infile = close_file_thread;				/* muxers.c中的函数 int close_file_thread( hnd_t handle ) */
        }
    }
	#endif

    return 0;
}

/*
 * 
 * 调用代码示例：
 * parse_qpfile( opt, &pic, i_frame + opt->i_seek );//在x264.c 之 Encode(...)内部
 * 
*/
static void parse_qpfile( cli_opt_t *opt, x264_picture_t *pic, int i_frame )
{

    int num = -1, qp;		/* 2个int */
    char type;				/* 1个char ,'a'之类的 */

    while( num < i_frame )	/* ??? */
    {
        int ret = fscanf( opt->qpfile, "%d %c %d\n", &num, &type, &qp );
		printf("\n (x264.c:parse_qpfile)num=%d",num);printf("\n\n");//zjh//运行效果：我用的命令这儿没执行过
		
		//system("pause");//暂停，任意键继续
		/* 
		%d %c %d 十进制整数 一个字符 十进制整数 
		ret 参数个数,3
		*/
		/*
	　　函数名: fscanf
	　　功 能: 从一个流中执行格式化输入
	　　用 法: int fscanf(FILE *stream, char *format,[argument...]);
	　　int fscanf(文件指针，格式字符串，输入列表); 
	　　返回值：整型，数值等于[argument...]的个数	

	　　常用基本参数对照： 
	　　%d：读入一个十进制整数.
	　　%i ：读入十进制，八进制，十六进制整数，与%d类似，但是在编译时通过数据前置来区分进制，如加入“0x”则是十六进制，加入“0”则为八进制。例如串“031”使用%d时会被算作31，但是使用%i时会算作25. 
	　　%u：读入一个无符号十进制整数. 
	　　%f %F %g %G : 用来输入实数，可以用小数形式或指数形式输入. 
	　　%x %X： 读入十六进制整数.
	　　%o': 读入八进制整数. 
	　　%s : 读入一个字符串，遇空格结束。
	　　%c : 读入一个字符。无法读入空值。空格可以被读入。

	　　附加格式说明字符表修饰符说明
	　　L/l 长度修饰符 输入"长"数据
	　　h 长度修饰符 输入"短"数据	

		*/
        if( num < i_frame )
            continue;
        pic->i_qpplus1 = qp+1;

		/* 片类型 */
        if     ( type == 'I' ) pic->i_type = X264_TYPE_IDR;		/* x264.h: #define X264_TYPE_IDR 0x0001 */
        else if( type == 'i' ) pic->i_type = X264_TYPE_I;		/*  */
        else if( type == 'P' ) pic->i_type = X264_TYPE_P;		/*  */
        else if( type == 'B' ) pic->i_type = X264_TYPE_BREF;	/*  */
        else if( type == 'b' ) pic->i_type = X264_TYPE_B;		/* 片类型 */
        else ret = 0;
        if( ret != 3 || qp < 0 || qp > 51 || num > i_frame )	/* num,type,qp 从文件里读到的,ret是参数个数3  */
        {	/*
			量化值qp必须在[0,51]之间，
			
			*/
            fprintf( stderr, "x264 [error]: can't parse qpfile for frame %d\n", i_frame );
            fclose( opt->qpfile );			/* 关闭文件:STDIO.H:fclose(FILE *); */
            opt->qpfile = NULL;				/* 更新结构体字段 */
            pic->i_type = X264_TYPE_AUTO;	/* x264.c:#define X264_TYPE_AUTO	0x0000 */
            pic->i_qpplus1 = 0;				/*  */
            break;
        }
    }
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/


static void x264_frame_dump( x264_t *h, x264_frame_t *fr, char *name,int k )
{
	FILE *f = fopen( name, "wb+" );
    int i, y;
    //printf("\n x264.c \ x264_frame_dump");//zjh
    if( !f )
	{
        return;
	};
    fseek( f, fr->i_frame * h->param.i_height * h->param.i_width * 3 / 2, SEEK_SET );
	//printf("(\n(x264.c-x264_frame_dump) h->param.i_height=%d, param.i_width=%d ) ",h->param.i_height,h->param.i_width);//zjh;
	//printf("\n((x264.c-x264_frame_dump)) fr->i_plane = %d",fr->i_plane);//zjh;
	
	//printf("\n\n");
	//system("pause");//暂停，任意键继续

	//另一种暂停，用回车键继续
	/*
	if (1)
	{
		int c;
		while ((c = getchar()) != '\n') 
		{
		printf("%c", c);
		}
	}
	*/

	for( i = 0; i < fr->i_plane; i++ )
	{
		for( y = 0; y < h->param.i_height / ( i == 0 ? 1 : 2 ); y++ )
		{
			fwrite( &fr->plane[i][y*fr->i_stride[i]], 1, h->param.i_width / ( i == 0 ? 1 : 2 ), f );
		}
	}

    fclose( f );

}


/*
Encode_frame
这个函数将输入每帧的YUV数据，然后输出nal包。编码码流的具体工作交由API x264_encoder_encode
*/

static int  Encode_frame( x264_t *h, hnd_t hout, x264_picture_t *pic )
{

    x264_picture_t pic_out;
    x264_nal_t *nal;	//字段包括：类型，优先级，负载大小，负载起始地址
						//实际用的是输出流h->out那块内存
    int i_nal, i;
    int i_file = 0;
	printf("\n***********************************Encode_frame(...)：in****************");//zjh

	//printf("\n i_frame=%d \n",h->i_frame);//zjh



	/*	printf("\n i_frame_offset = %d \n", h->i_frame_offset );
	 printf("\n i_frame_num = %d \n", h->i_frame_num );
	printf("\n i_poc_msb = %d \n", h->i_poc_msb );
	printf("\n i_poc_lsb = %d \n", h->i_poc_lsb );
	printf("\n i_poc = %d \n", h->i_poc );
	printf("\n i_thread_num = %d \n", h->i_thread_num );
	printf("\n i_nal_ref_idc = %d \n", h->i_nal_ref_idc );
	*/

	/*
	 * x264_encoder_encode内部有如下代码：
	 int     x264_encoder_encode( x264_t *h,
                             x264_nal_t **pp_nal,
							 int *pi_nal,	
                             x264_picture_t *pic_in,
                             x264_picture_t *pic_out )
	   {
	   *pp_nal = h->out.nal;
	   *pi_nal = h->out.i_nal;
	   }
	 */
	//输入图像，进行编码
	//把输出流h->out.nal告诉&nal
    if( x264_encoder_encode( h, &nal, &i_nal, pic/* 待编码 */, &pic_out ) < 0 )//264_encoder_encode每次会以参数送入一帧待编码的帧pic_in(第3个参数)
    {
        fprintf( stderr, "x264 [error]: x264_encoder_encode failed\n" );
    }

	/*
	if (1)
	{
		int tpi;
		tpi = x264_encoder_encode( h, &nal, &i_nal, pic, &pic_out );
		printf("\n (x264.t:encoder_frame) x264_encoder_encode的返回值：%d ",tpi);//zjh 打印结果：每次均为0
	}
	*/
	printf("\n (x264.c Encode_frame(){x264_encoder_encode()后}:)i_nal=%d",i_nal);//zjh//http://wmnmtm.blog.163.com/blog/static/38245714201181104551687/
	//printf("\n~~~~~~~~~~~~~~下面将开始for循环~~~~~~~~~~~~~~~~~~~~~~~~~\n");//zjh
	for( i = 0; i < i_nal; i++ )
    {
        int i_size;
        int i_data;

        i_data = DATA_MAX;	//右边这个是3百万，本文件最上面给了
		/*
		 * 网络打包编码
		 * 进行了如下操作：
		 * 在开头加0x 00 00 00 01
		 * 加入NAL头，即一字节{禁止位,优先级,类型}
		 * 检查原始流，遇上可能冲突的加入0x03
		 */
        if( ( i_size = x264_nal_encode( data, &i_data, 1, &nal[i] ) ) > 0 )//nalu 打包，加入前缀,第一个参数是一个全局变量，是个数组
        {
			//把网络包写入到输出文件中去
            i_file += p_write_nalu( hout, data, i_size );//这个是函数指针的形式，//p_write_nalu = write_nalu_mp4;p_write_nalu = write_nalu_mkv;p_write_nalu = write_nalu_bsf;
					
			//printf("\n(for内部) i_size=%d",i_size);//zjh
			//printf("\n(for内部) i_data=%d",i_data);//zjh
			//printf("\n(for内部) i_file=%d",i_file);//zjh
			//printf("\n\n");//zjh
			//system("pause");//暂停，任意键继续


        }
        else if( i_size < 0 )
        {
            fprintf( stderr, "x264 [error]: need to increase buffer size (size=%d)\n", -i_size );
        }
		
		
    }//system("pause");//暂停，任意键继续

	//把第一帧存成二进制文件
//	if (i_nal == 4)
//	{
//		x264_frame_t   *frame_psnr = h->fdec;
//		x264_frame_dump(h,frame_psnr,"d:\\testfile1.test",(int)1);//zjh
//	}


    if (i_nal)
        p_set_eop( hout, &pic_out );
	
	//system("pause");//暂停，任意键继续
	printf("\n***********************************Encode_frame(...)：out****************\n");//zjh
	
    return i_file;
}

/*****************************************************************************
 * Encode: 编码
 * 这个函数执行完，全部编码工作就结束了
 * 
 * 
 * 
 * 
 * 
 *****************************************************************************/
static int  Encode( x264_param_t *param, cli_opt_t *opt )
{
    x264_t *h;				//定义一个结构体的指针
    x264_picture_t pic;		//这是一个结构体的对象，所以已经分配了内存空间，不用再动态分配了，本结构体里有一个数组，是用来存图像的，但它只是存的指针，所以后面还得分配实际存图像的内存，然后用这个数组记下这块内存的地址

    int     i_frame=0, i_frame_total;		//x264_param_t结构的字段里也有个i_frame_total字段，默认值为0
    int64_t i_start, i_end;				//从i_start开始编码，直到i_end结束
										/* i_start = x264_mdate();  i_end = x264_mdate(); */
    int64_t i_file;						//好像是计数编码过的长度的
    int     i_frame_size;				//后面有一句，是用编码一帧的返回值赋值的
    int     i_progress;					//记录进度

    i_frame_total = p_get_frame_total( opt->hin );	/* 得到输入文件的总帧数 由于p_get_frame_total = get_frame_total_yuv（见Parse（）函数），所以调用函数int get_frame_total_yuv( hnd_t handle )，在文件muxers.c中*/
    i_frame_total -= opt->i_seek;					/* 要编码的总帧数= 源文件总帧数-开始帧(不一定从第1帧开始编码) */
    if( ( i_frame_total == 0 || param->i_frame_total < i_frame_total ) && param->i_frame_total > 0 )	/* 对这个待编码的总帧数进行一些判断 */
        i_frame_total = param->i_frame_total;		/* 保证i_frame_total是有效的 */
    param->i_frame_total = i_frame_total;			/* 获取要求编码的帧数param->i_frame_total 根据命令行输入参数计算得到的总帧数，存进结构体的字段*/

    if( ( h = x264_encoder_open( param ) ) == NULL )	/* 在文件encoder.c中，创建编码器，并对不正确的264_t结构体(h的类型是264_t * )参数进行修改，并对各结构体参数、编码、预测等需要的参数进行初始化 */
    {
        fprintf( stderr, "x264 [error]: x264_encoder_open failed\n" );		/* 打印错误信息 */
        p_close_infile( opt->hin );					/* 关闭输入文件 */
        p_close_outfile( opt->hout );				/* 关闭输出文件 */
        return -1;									/* 程序结束 */
    }

    if( p_set_outfile_param( opt->hout, param ) )	/* 设置输出文件格式 */
    {
        fprintf( stderr, "x264 [error]: can't set outfile param\n" );//输出：不能设置输出文件参数
        p_close_infile( opt->hin );	/* 关闭YUV文件 关闭输入文件 */
        p_close_outfile( opt->hout );	/* 关闭码流文件 关闭输出文件 */
        return -1;
    }

    /* 创建一帧图像空间 Create a new pic (这个图像分配好的内存首地址等填充到了pic结构)*/
    x264_picture_alloc( &pic, X264_CSP_I420, param->i_width, param->i_height );	/* #define X264_CSP_I420  0x0001  /* yuv 4:2:0 planar */ 

    i_start = x264_mdate();	/* 返回当前日期，开始和结束时间求差，可以计算总的耗时和每秒的效率 return the current date in microsecond */


    /* (循环编码每一帧) Encode frames */
    for( i_frame = 0, i_file = 0, i_progress = 0; b_ctrl_c == 0 && (i_frame < i_frame_total || i_frame_total == 0); )
    {
		printf("\n Encode() : i_frame=%d \n",i_frame);//这样输出时i_frame是0,1,2,...,300
        if( p_read_frame( &pic, opt->hin, i_frame + opt->i_seek ) )/*读取一帧，并把这帧设为prev; 读入第i_frame帧，从起始帧算，而不是视频的第0帧 */
            break;

        pic.i_pts = (int64_t)i_frame * param->i_fps_den; //字段：I_pts ：program time stamp 程序时间戳，指示这幅画面编码的时间戳
														//帧率*帧数，好像也能算出时间戳来
														//param->i_fps_den好像是序列的已编码帧数，因为我编码一个300帧的序列，此参数打印结果为0,1,2,...,298,299

//		printf("\npic.i_pts=%d;  \n",pic.i_pts);//zjh//http://wmnmtm.blog.163.com/blog/static/382457142011810113418553/
//		printf("\npic.i_pts=%d;     i_frame=%d;      param->i_fps_den=%d;   \n",\
			pic.i_pts,(int64_t)i_frame,param->i_fps_den);//zjh//http://wmnmtm.blog.163.com/blog/static/382457142011810115333413/

        if( opt->qpfile )//这儿可能是一个什么设置，或许是存在一个配置文件
            ;//parse_qpfile( opt, &pic, i_frame + opt->i_seek );//parse_qpfile() 为从指定的文件中读入qp的值留下的接口，qpfile为文件的首地址
        else//没有配置的话，就全自动了，呵呵
        {
            /* 未强制任何编码参数Do not force any parameters */
            pic.i_type = X264_TYPE_AUTO;
            pic.i_qpplus1 = 0;//I_qpplus1 ：此参数减1代表当前画面的量化参数值
        }

		/*编码一帧图像，h编码器句柄，hout码流文件，pic预编码帧图像，实际就是上面从视频文件中读到的东西*/
        i_file += Encode_frame( h, opt->hout, &pic );//进入核心编码层

        i_frame++;//已编码的帧数统计(递增1)


//		printf("\n i_frame=%d;i_file=%d;\n",i_frame,i_file);//zjh//http://wmnmtm.blog.163.com/blog/static/382457142011810115333413/
		//??i_frame打印结果，为什么一直是0//把i_frame和i_file互换一下就又变了
		
//		printf("\n pic.i_type=%d;pic.i_qpplus1=%d;\n",pic.i_type,pic.i_qpplus1);//pic.i_type=0;pic.i_qpplus1=0;


        /* 更新数据行，用于显示整个编码过程的进度 update status line (up to 1000 times per input file) */
		
		if( opt->b_progress && param->i_log_level < X264_LOG_DEBUG &&		// #define X264_LOG_DEBUG 3 调试 //
            ( i_frame_total ? i_frame * 1000 / i_frame_total > i_progress	//
                            : i_frame % 10 == 0 ) )		
        {
            int64_t i_elapsed = x264_mdate() - i_start;//elapsed:(时间)过去;开始至现在的时间差
            double fps = i_elapsed > 0 ? i_frame * 1000000. / i_elapsed : 0;
            if( i_frame_total )
            {
                int eta = i_elapsed * (i_frame_total - i_frame) / ((int64_t)i_frame * 1000000);//已用的时间*(总共要编码的帧数-已编码的帧数=待编码的帧数)/(已编码的帧数*1000000)，看上去就象是预计剩余时间，也就是估计的倒计时，根据过去的效率动态的估计这个时间
                i_progress = i_frame * 1000 / i_frame_total;
                fprintf( stderr, "已编码的帧数:: %d/%d (%.1f%%), %.2f fps, eta %d:%02d:%02d  \r",	
                         i_frame, i_frame_total, (float)i_progress / 10, fps,
                         eta/3600, (eta/60)%60, eta%60 );//ETA是Estimated Time of Arrival的英文缩写，指 预计到达时间// 状态提示 FPS（Frames Per Second）：每秒传输帧数,每秒钟填充图像的帧数（帧/秒)
            }
            else
                fprintf( stderr, "已编码的帧数: %d, %.2f fps   \r", i_frame, fps );	// 状态提示,共编码了...//
            fflush( stderr ); // needed in windows
       }

    }

    /* Flush delayed(延迟) B-frames */
//    do {
//        i_file +=
//        i_frame_size = Encode_frame( h, opt->hout, NULL );//上面调用的是i_file += Encode_frame( h, opt->hout, &pic );
//    } while( i_frame_size );
//printf("p_write_nalu=%s",p_write_nalu);
    i_end = x264_mdate();		/* 返回当前时间return the current date in microsecond//前面有句： i_start = x264_mdate(); */
    x264_picture_clean( &pic );	/* common.c:void x264_picture_clean( x264_picture_t *pic ){free()} x264_picture_t.x264_image_t.plane[4]用来存放实际的图像 */
    x264_encoder_close( h );	/* 关闭一个编码处理器 ncoder.c:void x264_encoder_close  ( x264_t *h ) */
    fprintf( stderr, "\n" );

    if( b_ctrl_c )	/* 按下了ctrl+c组合键 编码中止，做一些收尾工作 */
        fprintf( stderr, "aborted(失败的) at input frame %d\n", opt->i_seek + i_frame );	/* 输入帧时失败 */

    p_close_infile( opt->hin );		//关闭输入文件
    p_close_outfile( opt->hout );	//关闭输出文件

    if( i_frame > 0 )
    {
        double fps = (double)i_frame * (double)1000000 / (double)( i_end - i_start );/* 每秒多少帧 */

        fprintf( stderr, "encoded %d frames, %.2f fps, %.2f kb/s\n", i_frame, fps,
                 (double) i_file * 8 * param->i_fps_num /
                 ( (double) param->i_fps_den * i_frame * 1000 ) );	/* 在窗口显示encoded 26 frames等进度提示 */
    }

    return 0;
}


/*直接传输像素值是IPCM模式

  FME:fast motion estimation 估计
  \tools\q_matrix_jvt.cfg 矩阵配置文件 matrix:矩阵
*/