/*****************************************************************************
 * common.c: h264 library
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: common.c,v 1.1 2004/06/03 19:27:06 fenrir Exp $
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "common.h"
#include "cpu.h"

/*
设置缺省日志参数
*/
static void x264_log_default( void *, int, const char *, va_list );


/****************************************************************************
 * x264_test_my1:(简单的测试函数)
 ****************************************************************************/

void    x264_test_my1()
{
//	AfxMessageBox("x264_test_my1");
}

/****************************************************************************
 * x264_test_my2:(简单的测试函数)
 ****************************************************************************/

void    x264_test_my2( char * name)
{

//	AfxMessageBox(name);
}

/****************************************************************************
 * x264_param_default:(为此结构体初始化一些默认值)
 设置缺省参数

 ****************************************************************************/
void    x264_param_default( x264_param_t *param )
{
    /* 对已分配好的内存空间初始化为0 */
    memset( param, 0, sizeof( x264_param_t ) );

    /* CPU autodetect(自动侦测) */
    param->cpu = x264_cpu_detect();		/* 好象有取CPU序列号的操作在里头 detect:检测*/
    param->i_threads = 1;				//并行编码多帧

    /* 视频属性 */
    param->i_csp           = X264_CSP_I420;		/* 色彩空间 */	
    param->i_width         = 0;		/* 宽 */
    param->i_height        = 0;		/* 高 */
    param->vui.i_sar_width = 0;		/* 标准313页 句法元素sar_width表示样点高宽比的水平尺寸(以任意单位) */
    param->vui.i_sar_height= 0;		/* 标准313页 句法元素sar_height表示样点高宽比的垂直尺寸(以与句法元素sar_width相同的任意单位) */
    param->vui.i_overscan  = 0;		/* undef 过扫描线，默认"undef"(不设置)，可选项：show(观看)/crop(去除) 0=undef, 1=no overscan, 2=overscan */
    param->vui.i_vidformat = 5;		/* undef 标准314 句法元素video_format 表示图像在依本建议书编码前的制式，见表E-2的规定，当video_format语法元素不存在，video_format的值应被推定为5*/
    param->vui.b_fullrange = 0;		/* off 标准314页 video_full_rang_flag表示黑电平和亮度与色度信号的范围由E'y，和E'pr或...模拟信号分量得到*/
    param->vui.i_colorprim = 2;		/* undef 标准314，句法元素colour_primaries表示最初的原色的色度坐标，按照EIE 1931的规定(表E-3)，x和y的定义由SO/CIE 10527规定，当colour_primaries语法元素不存在时，colour_primaries的值应被推定为等于2(色度未定义或由应用决定)*/
    param->vui.i_transfer  = 2;		/* undef 标准315，语法元素transfer_characteristics表示源图像的光电转换特性，模拟量范围从0到1的线性光强度输入Lc的函数，见表E4，当transfer_characteristics语法元素不存在时，transfer_characteristics的值应被推定为等于2(转换特性未定义或由应用决定)*/
    param->vui.i_colmatrix = 2;		/* undef 标准316，语法元素matrix_coefficients描述用于根据红、绿、蓝三原色得到亮度和色度信号的矩阵系数，见表E-5。matrix_coefficients不应等于0，除非下面两个条件都为真... ...*/
    param->vui.i_chroma_loc= 0;		/* left center 语法元素chroma_loc_info_present_flag等于1表示chroma_sample_loc_type_top_field和chroma_sample_loc_type_bottom_field存在。chroma_loc_info_present_flag等于0表示它们不存在 */
    param->i_fps_num       = 25;	/* 视频数据帧率 */
    param->i_fps_den       = 1;		/* 用两个整型的数的比值，来表示帧率 */
    param->i_level_idc     = 51;	/* as close to "unrestricted" as we can get */

    /* (编码参数)Encoder parameters */
    param->i_frame_reference = 1;			/* 参考帧最大数目 */
    param->i_keyint_max = 250;				/* 在此间隔设置IDR关键帧 */
    param->i_keyint_min = 25;				/* 场景切换少于次值编码位I, 而不是 IDR */
    param->i_bframe = 0;					/* 两个相关图像间P帧的数目 */
    param->i_scenecut_threshold = 40;		/* 如何积极地插入额外的I帧 scene:背景, 现场 threshold:阈；界限；起始点 */
    param->b_bframe_adaptive = 1;			/* adaptive:适应 */
    param->i_bframe_bias = 0;				/* 控制插入B帧判定，范围-100~+100，越高越容易插入B帧，默认0 bias: 偏见, 偏心 */
    param->b_bframe_pyramid = 0;			/* pyramid: 金字塔,棱锥(体), 角锥(体) */

    param->b_deblocking_filter = 1;			/* 去块滤波 */
    param->i_deblocking_filter_alphac0 = 0;	/*  */
    param->i_deblocking_filter_beta = 0;	/* 希腊字母第二字[B, β] */

    param->b_cabac = 1;						/* 基于上下文自适应二进制算术熵编码 */
    param->i_cabac_init_idc = 0;			/*  */

	/*码率控制*/
    param->rc.i_rc_method = X264_RC_CQP;	/*恒定码率*/
    param->rc.i_bitrate = 0;				/*设置平均码率大小*/
    param->rc.f_rate_tolerance = 1.0;		/* rate:比率, 率;tolerance:宽容, 容忍，偏差, 公差,失真*/
    param->rc.i_vbv_max_bitrate = 0;		/*平均码率模式下，最大瞬时码率，默认0(与-B设置相同) */
    param->rc.i_vbv_buffer_size = 0;		/*码率控制缓冲区的大小，单位kbit，默认0 */
    param->rc.f_vbv_buffer_init = 0.9;		/* <=1: fraction of buffer_size. >1: kbit码率控制缓冲区数据保留的最大数据量与缓冲区大小之比，范围0~1.0，默认0.9*/
    param->rc.i_qp_constant = 26;			/*最小qp值*/
    param->rc.i_rf_constant = 0;			/*  */
    param->rc.i_qp_min = 10;				/*允许的最小量化值 */
    param->rc.i_qp_max = 51;				/*允许的最大量化值*/
    param->rc.i_qp_step = 4;				/*帧间最大量化步长 */
    param->rc.f_ip_factor = 1.4;				/* factor: 因素, 要素*/
    param->rc.f_pb_factor = 1.3;				/*  */

    param->rc.b_stat_write = 0;					/*  */
    param->rc.psz_stat_out = "x264_2pass.log";	/*  */
    param->rc.b_stat_read = 0;					/*  */
    param->rc.psz_stat_in = "x264_2pass.log";	/*  */
    param->rc.psz_rc_eq = "blurCplx^(1-qComp)";	/*  */
    param->rc.f_qcompress = 0.6;				/* compress: 压缩*/
    param->rc.f_qblur = 0.5;					/*时间上模糊量化 */
    param->rc.f_complexity_blur = 20;			/* 时间上模糊复杂性 complexity:复杂性 */
    param->rc.i_zones = 0;						/* zone: (划分出来的)地区, 区域, 地带*/

    /* 日志 */
    param->pf_log = x264_log_default;			/* 貌似函数指针 common.c:static void x264_log_default( void *p_unused, int i_level, const char *psz_fmt, va_list arg ) */
    param->p_log_private = NULL;				/*  */
    param->i_log_level = X264_LOG_INFO;			/* 日志级别 */

    /* 分析 analyse:分析, 分解*/
    param->analyse.intra = X264_ANALYSE_I4x4 | X264_ANALYSE_I8x8;	/* 0x0001 | 0x0002 */
    param->analyse.inter = X264_ANALYSE_I4x4 | X264_ANALYSE_I8x8	/* 0x0001 | 0x0002 */	
                         | X264_ANALYSE_PSUB16x16 | X264_ANALYSE_BSUB16x16;	/* 0x0010 | 0x0100 */
    param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_SPATIAL;	/* 时间空间队运动预测 #define X264_DIRECT_PRED_SPATIAL	1 */
    param->analyse.i_me_method = X264_ME_HEX;	/* 运动估计算法 (X264_ME_*) */
    param->analyse.i_me_range = 16;				/* 运动估计范围 */
    param->analyse.i_subpel_refine = 5;			/* 亚像素运动估计质量 */
    param->analyse.b_chroma_me = 1;				/* 亚像素色度运动估计和P帧的模式选择 */
    param->analyse.i_mv_range = -1; 			/* 运动矢量最大长度(in pixels). -1 = auto, based on level */ // set from level_idc
    param->analyse.i_chroma_qp_offset = 0;		/* 色度量化步长偏移量 */
    param->analyse.b_fast_pskip = 1;			/* 快速P帧跳过检测 */		
    param->analyse.b_dct_decimate = 1;			/* 在P-frames转换参数域 */
    param->analyse.b_psnr = 1;					/*是否显示PSNR 计算和打印PSNR信息*/

	/*量化*/
    param->i_cqm_preset = X264_CQM_FLAT;		/*自定义量化矩阵(CQM),初始化量化模式为flat 0*/
    memset( param->cqm_4iy, 16, 16 );			/*  */
    memset( param->cqm_4ic, 16, 16 );			/*  */
    memset( param->cqm_4py, 16, 16 );			/*  */
    memset( param->cqm_4pc, 16, 16 );			/*  */
    memset( param->cqm_8iy, 16, 64 );			/*  */
    memset( param->cqm_8py, 16, 64 );			/*开辟空间*/

	/*muxing*/									/* repeat:重复 */
    param->b_repeat_headers = 1;				/* 在每个关键帧前放置SPS/PPS*/
    param->b_aud = 0;							/*生成访问单元分隔符*/
}

/* 
 * 解析枚举
 * const char *arg				:值
 * const char * const *names	:枚举
 * int *dst						:目标变量
 * 
 * 调用示例：
 * parse_enum( value, x264_overscan_names, &p->vui.i_overscan ); //static const char * const x264_overscan_names[] = { "undef", "show", "crop", 0 }; //如此所示，第二个参数是一个枚举
*/
static int parse_enum( const char *arg, const char * const *names, int *dst )
{
    int i;
    for( i = 0; names[i]; i++ )
        if( !strcmp( arg, names[i] ) )
        {
            *dst = i;	/* 把原值改变了 */
            return 0;
        }
    return -1;
}

static int parse_cqm( const char *str, uint8_t *cqm, int length )
{
    int i = 0;
    do {
        int coef;
        if( !sscanf( str, "%d", &coef ) || coef < 1 || coef > 255 )	/* 有效值范围[1,255] */
            return -1;
        cqm[i++] = coef;	/* i在这儿递增了 */
    } while( i < length && (str = strchr( str, ',' )) && str++ );	/* length指明要读多长  */
    return (i == length) ? 0 : -1;	/* 循环次数i与length相等，返回0，否则返回1；返回0，说明读到了指定长度的东西 */
}
										/* 
										从一个字符串中读进与指定格式相符的数据 
									　　int sscanf( const char *, const char *, ...);
									　　int sscanf(const char *buffer,const char *format[,argument ]...);
									　　buffer 存储的数据
									　　format 格式控制字符串
									　　argument 选择性设定字符串
									　　sscanf会从buffer里读进数据，依照argument的设定将数据写回。
										sscanf与scanf类似，都是用于输入的，只是后者以键盘(stdin)为输入源，前者以固定字符串为输入源。
										第一个参数可以是一个或多个 {%[*] [width] [{h | l | I64 | L}]type | ' ' | '\t' | '\n' | 非%符号}
										　　注：
										　　1、 * 亦可用于格式中, (即 %*d 和 %*s) 加了星号 (*) 表示跳过此数据不读入. (也就是不把此数据读入参数中)
										　　2、{a|b|c}表示a,b,c中选一，[d],表示可以有d也可以没有d。
										　　3、width表示读取宽度。
										　　4、{h | l | I64 | L}:参数的size,通常h表示单字节size，I表示2字节 size,L表示4字节size(double例外),l64表示8字节size。
										　　5、type :这就很多了，就是%s,%d之类。
										　　6、特别的：%*[width] [{h | l | I64 | L}]type 表示满足该条件的被过滤掉，不会向目标参数中写入值
										　　失败返回0 ，否则返回格式化的参数个数
										*/

										/*
											extern char *strchr(const char *s,char c);
										　　const char *strchr(const char* _Str,int _Val)
										　　char *strchr(char* _Str,int _Ch)
										　　头文件：#include <string.h>
										　　功能：查找字符串s中首次出现字符c的位置
										　　说明：返回首次出现c的位置的指针，如果s中不存在c则返回NULL。
										　　返回值：Returns the address of the first occurrence of the character in the string if successful, or NULL otherwise
										*/
/*
 * 将字符串转为布尔类型
 * "1","true","yes" => 1
 * "0","false","no" => 0
*/
static int atobool( const char *str )
{
	/* "1","true","yes" => 1 */
    if( !strcmp(str, "1") || !strcmp(str, "true") || !strcmp(str, "yes") )	/* strcmp(const char *s1,const char * s2); 当s1=s2时，返回值=0 ;  */
        return 1;
	/* "0","false","no" => 0 */
    if( !strcmp(str, "0") ||			/* !strcmp(str, "0")为真，即strcmp(str, "0")=0，即str="0" */
        !strcmp(str, "false") || 
        !strcmp(str, "no") )
        return 0;
    return -1;
}

#define atobool(str) ( (i = atobool(str)) < 0 ? (b_error = 1) : i )

/*
 * x264_param_parse
 * x264.c中的调用代码：
 * b_error |= x264_param_parse( param, long_options[long_options_index].name, optarg ? optarg : "true" );
 * 在进入main函数时，定义了param，所以param的生存期是很长的，这儿把命令行参数进行分析，存到这个param结构体对象的各字段供整个程序使用
 * p用来往结构体的字段中存值
 * name是参数名称
 * value是参数值
 * 在上级函数中，循环提供不同的参数调用本函数，将输入的所有参数的值存到结构体中
*/
int x264_param_parse( x264_param_t *p, const char *name, const char *value )
{
    int b_error = 0;
    int i;

    if( !name )
	{
        return X264_PARAM_BAD_NAME;	/* #define X264_PARAM_BAD_NAME  (-1) x264.h */
	}
    if( !value )
	{
        return X264_PARAM_BAD_VALUE;	/* #define X264_PARAM_BAD_VALUE (-2) x264.h  */
	}

    if( value[0] == '=' )
	{
        value++;
	}

    if( (!strncmp( name, "no-", 3 ) && (i = 3)) || (!strncmp( name, "no", 2 ) && (i = 2)) )	/* ?i在这儿怎么会大于0呢? */
    {
        name += i;
        value = atobool(value) ? "false" : "true";
    }

	#define OPT(STR) else if( !strcmp( name, STR ) )		/* strcmp(const char *s1,const char * s2); 比较字符串s1和s2。 说明： 当s1<s2时，返回值<0 ; 当s1=s2时，返回值=0 ; 当s1>s2时，返回值>0 , 即：两个字符串自左向右逐个字符相比（按ASCII值大小相比较），直到出现不同的字符或遇'\0'为止。 */
    if(0);	/* http://wmnmtm.blog.163.com/blog/static/3824571420117303531845/ */
    OPT("asm")												/* else if( !strcmp( name, "asm" ) )  */
        p->cpu = atobool(value) ? x264_cpu_detect() : 0;	/*  */
    OPT("threads")											/*  */
    {
        if( !strcmp(value, "auto") )						/*  */
            p->i_threads = 0;
        else
            p->i_threads = atoi(value);						/*  */
    }
    OPT("level")							/*  */
    {
        if( atof(value) < 6 )
            p->i_level_idc = (int)(10*atof(value)+.5);		/*  */
        else
            p->i_level_idc = atoi(value);					/*  */
    }
    OPT("sar")							/*  */
    {
        b_error = ( 2 != sscanf( value, "%d:%d", &p->vui.i_sar_width, &p->vui.i_sar_height ) &&
                    2 != sscanf( value, "%d/%d", &p->vui.i_sar_width, &p->vui.i_sar_height ) );/* 从value里内容到两个变量里 */
    }
    OPT("overscan")							/* 默认值：undef；如何处理溢出扫描（overscan）。溢出扫描的意思是装置只显示影像的一部分。可用的值：undef：未定义。show：指示要显示整个影像。理论上如果有设定则必须被遵守。crop：指示此影像适合在有溢出扫描功能的装置上播放。不一定被遵守。建议：在编码之前裁剪（Crop），然后如果装置支援则使用show，否则不理会 */		/* else if( !strcmp( name, "overscan" ) ) */
        b_error |= parse_enum( value, x264_overscan_names, &p->vui.i_overscan );
    OPT("vidformat")							/* 只找到个videoformat：默认值：undef；指示此视讯在编码／数位化（digitizing）之前是什么格式。可用的值：component, pal, ntsc, secam, mac, undef 建议：来源视讯的格式，或者未定义 */	/* else if( !strcmp( name, "vidformat" ) ) */
        b_error |= parse_enum( value, x264_vidformat_names, &p->vui.i_vidformat );
    OPT("fullrange")							/* 默认值：off；指示是否使用亮度和色度层级的全范围。如果设为off，则会使用有限范围。建议：如果来源是从类比视讯数位化，将此设为off。否则设为on */
        b_error |= parse_enum( value, x264_fullrange_names, &p->vui.b_fullrange );
    OPT("colourprim")							/* 只找到个colorprim：默认值：undef；设定以什么色彩原色转换成RGB。可用的值：undef, bt709, bt470m, bt470bg, smpte170m, smpte240m, film；建议：默认值，除非你知道来源使用什么色彩原色 */
        b_error |= parse_enum( value, x264_colorprim_names, &p->vui.i_colorprim );
    OPT("transfer")							/* 默认值：undef；设定要使用的光电子（opto-electronic）传输特性（设定用于修正的色差补正(gamma)曲线）。可用的值：undef, bt709, bt470m, bt470bg, linear, log100, log316, smpte170m, smpte240m；建议：默认值，除非你知道来源使用什么传输特性 */
        b_error |= parse_enum( value, x264_transfer_names, &p->vui.i_transfer );
    OPT("colourmatrix")
        b_error |= parse_enum( value, x264_colmatrix_names, &p->vui.i_colmatrix );
    OPT("chromaloc")						/* 默认值：0;设定色度采样位置（如ITU-T规格的附录E所定义）.可用的值：0~5；建议：如果是从正确次采样4:2:0的MPEG1转码，而且没有做任何色彩空间转换，则应该将此选项设为1。如果是从正确次采样4:2:0的MPEG2转码，而且没有做任何色彩空间转换，则应该将此选项设为0。如果是从正确次采样4:2:0的MPEG4转码，而且没有做任何色彩空间转换，则应该将此选项设为0。否则，维持默认值。 */
    {
        p->vui.i_chroma_loc = atoi(value);	/* chroma:色度 */
        b_error = ( p->vui.i_chroma_loc < 0 || p->vui.i_chroma_loc > 5 );
    }
    OPT("fps")												/* 指定固定帧率 */
    {
        float fps;
        if( sscanf( value, "%d/%d", &p->i_fps_num, &p->i_fps_den ) == 2 )
            ;
        else if( sscanf( value, "%f", &fps ) )
        {
            p->i_fps_num = (int)(fps * 1000 + .5);			/*  */
            p->i_fps_den = 1000;							/*  */
        }
        else
            b_error = 1;
    }
    OPT("ref")												/* 控制解码图片缓冲（DPB：Decoded Picture Buffer）的大小。范围是从0到16。总之，此值是每个P帧可以使用先前多少帧作为参照帧的数目;（B帧可以使用的数目要少一或两个，取决于它们是否作为参照帧）。可以被参照的最小ref数是1。 还要注意的是，H.264规格限制了每个level的DPB大小 http://nmm-hd.org/doc/X264%E8%A8%AD%E5%AE%9A#bitrate */
        p->i_frame_reference = atoi(value);					/*  */
    OPT("keyint")											/* 设定x264输出的资料流之最大IDR帧（亦称为关键帧）间隔。可以指定infinite让x264永远不要插入非场景变更的IDR帧 */
    {
        p->i_keyint_max = atoi(value);						/*  */
        if( p->i_keyint_min > p->i_keyint_max )				/*  */
            p->i_keyint_min = p->i_keyint_max;				/*  */
    }
    OPT("min-keyint")										/* 设定IDR帧之间的最小长度,此选项限制在每个IDR帧之后，要有多少帧才可以再有另一个IDR帧的最小长度,min-keyint的最大允许值是--keyint/2+1,过小的keyint范围会导致“不正确的”IDR帧位置（例如闪屏场景）。 else if( !strcmp( name, "min-keyint" ) ) */
    {
        p->i_keyint_min = atoi(value);						/*  */
        if( p->i_keyint_max < p->i_keyint_min )
            p->i_keyint_max = p->i_keyint_min;				/*  */
    }
    OPT("scenecut")											/* 设定I/IDR帧位置的阈值（场景变更侦测）。 x264为每一帧计算一个度量值，来估计与前一帧的不同程度。如果该值低于scenecut，则算侦测到一个“场景变更”。如果此时与最近一个IDR帧的距离低于--min-keyint，则放置一个I帧，否则放置一个IDR帧。越大的scenecut值会增加侦测到场景变更的数目。场景变更是如何比较的详细资讯可以参阅http://forum.doom9.org/showthread.php?t=121116 */
        p->i_scenecut_threshold = atoi(value);				/* scene:背景,现场;threshold:阈,界限,起始点 将scenecut设为0相当于设定--no-scenecut */
    OPT("bframes")											/* 默认值：3 设定x264可以使用的最大并行B帧数,没有B帧时，一个典型的x264资料流有着像这样的帧类型：IPPPPP...PI。当设了--bframes 2时，最多两个连续的P帧可以被B帧取代，就像：IBPBBPBPPPB...PI,B帧类似于P帧，除了B帧还能从它之后的帧做动态预测（motion prediction）。就压缩比来说效率会大幅提高。它们的平均品质是由--pbratio所控制,有趣的事：x264还区分两种不同种类的B帧。"B"是代表一个被其他帧作为参照帧的B帧（参阅--b-pyramid），而"b"则代表一个不被其他帧作为参照帧的B帧。如果看到一段混合的"B"和"b"，原因通常与上述有关。当差别并不重要时，通常就以"B"代表所有B帧;x264是如何为每个候选帧选定为P帧或B帧的详细资讯可以参阅http://article.gmane.org/gmane.comp.video.ffmpeg.devel/29064。在此情况下，帧类型看起来会像这样（假设--bframes 3）：IBBBPBBBPBPI */
        p->i_bframe = atoi(value);							/*  */
    OPT("b-adapt")											/* 默认值：1;设定弹性B帧位置决策算法。此设定控制x264如何决定要放置P帧或B帧;0：停用，总是挑选B帧。这与旧的no-b-adapt设定相同作用;1：“快速”算法，较快，越大的--bframes值会稍微提高速度。当使用此模式时，基本上建议搭配--bframes 16使用。2：“最佳”算法，较慢，越大的--bframes值会大幅降低速度。 */
        p->b_bframe_adaptive = atobool(value);//原值				/* B帧_自适应 */
    OPT("b-bias")											/* 默认值：0;控制使用B帧而不使用P帧的可能性。大于0的值增加偏向B帧的加权，而小于0的值则相反。范围是从-100到100。100并不保证全是B帧（要全是B帧该使用--b-adapt 0），而-100也不保证全是P帧。仅在你认为能比x264做出更好的位元率控制决策时才使用此选项。 */
        p->i_bframe_bias = atoi(value);						/*  */
    OPT("b-pyramid")										/* 默认值：normal;允许B帧作为其他帧的参照帧。没有此设定时，帧只能参照I/P帧。虽然I/P帧因其较高的品质作为参照帧更有价值，但B帧也是很有用的。作为参照帧的B帧会得到一个介于P帧和普通B帧之间的量化值。b-pyramid需要至少两个以上的--bframes才会运作。如果是在为蓝光编码，须使用none或strict。none：不允许B帧作为参照帧。strict：每minigop允许一个B帧作为参照帧，这是蓝光标准强制执行的限制。normal：每minigop允许多个B帧作为参照帧。 */
        p->b_bframe_pyramid = atobool(value);				/*  */
    OPT("nf")
        p->b_deblocking_filter = 0;							/* 去块滤波 */
    OPT("filter")											/*  */
    {
        int count;
        if( 0 < (count = sscanf( value, "%d:%d", &p->i_deblocking_filter_alphac0, &p->i_deblocking_filter_beta )) ||
            0 < (count = sscanf( value, "%d,%d", &p->i_deblocking_filter_alphac0, &p->i_deblocking_filter_beta )) )
        {
            p->b_deblocking_filter = 1;						/*  */
            if( count == 1 )
                p->i_deblocking_filter_beta = p->i_deblocking_filter_alphac0;	/*  */
        }
        else
            p->b_deblocking_filter = atobool(value);		/* 去块滤波 */
    }
    OPT("cabac")											/* 只找到个no-cabac：默认值：无；停用弹性内容的二进制算数编码（CABAC：Context Adaptive Binary Arithmetic Coder）资料流压缩，切换回效率较低的弹性内容的可变长度编码（CAVLC：Context Adaptive Variable Length Coder）系统。大幅降低压缩效率（通常10~20%）和解码的硬件需求。 */
        p->b_cabac = atobool(value);						/*  */
    OPT("cabac-idc")										/*  */
        p->i_cabac_init_idc = atoi(value);					/*  */
    OPT("cqm")												/* 默认值：flat；设定所有自订量化矩阵（custom quantization matrices）为内建的默认之一。内建默认有flat和JVT。建议：默认值 */
    {
        if( strstr( value, "flat" ) )
            p->i_cqm_preset = X264_CQM_FLAT;				/*  */
        else if( strstr( value, "jvt" ) )
            p->i_cqm_preset = X264_CQM_JVT;					/*  */
        else
            b_error = 1;
    }
    OPT("cqmfile")											/* 默认值：无：从一个指定的JM相容档案来设定自订量化矩阵。覆写所有其他--cqm开头的选项。建议：默认值 */
        p->psz_cqm_file = strdup(value);					/*  */
    OPT("cqm4")												/* 设定所有4x4量化矩阵。需要16个以逗号分隔的整数清单。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/* #define X264_CQM_JVT	1 x264.h*/
        b_error |= parse_cqm( value, p->cqm_4iy, 16 );
        b_error |= parse_cqm( value, p->cqm_4ic, 16 );
        b_error |= parse_cqm( value, p->cqm_4py, 16 );
        b_error |= parse_cqm( value, p->cqm_4pc, 16 );
    }
	/*
	cqm4* / cqm8*默认值：无 

	--cqm4：设定所有4x4量化矩阵。需要16个以逗号分隔的整数清单。 
	--cqm8：设定所有8x8量化矩阵。需要64个以逗号分隔的整数清单。 
	--cqm4i、--cqm4p、--cqm8i、--cqm8p：设定亮度和色度量化矩阵。 
	--cqm4iy、--cqm4ic、--cqm4py、--cqm4pc：设定个别量化矩阵。 
	建议：默认值 
	资料：http://nmm-hd.org/doc/X264%E8%A8%AD%E5%AE%9A#no-8x8dct	
	*/
    OPT("cqm8")												/* 设定所有8x8量化矩阵。需要64个以逗号分隔的整数清单。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/*  */
        b_error |= parse_cqm( value, p->cqm_8iy, 64 );
        b_error |= parse_cqm( value, p->cqm_8py, 64 );
    }
    OPT("cqm4i")											/* 设定亮度和色度量化矩阵。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/*  */
        b_error |= parse_cqm( value, p->cqm_4iy, 16 );
        b_error |= parse_cqm( value, p->cqm_4ic, 16 );
    }
    OPT("cqm4p")											/* 设定亮度和色度量化矩阵。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/*  */
        b_error |= parse_cqm( value, p->cqm_4py, 16 );
        b_error |= parse_cqm( value, p->cqm_4pc, 16 );
    }
    OPT("cqm4iy")											/* 设定个别量化矩阵。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/*  */
        b_error |= parse_cqm( value, p->cqm_4iy, 16 );
    }
    OPT("cqm4ic")											/* 设定个别量化矩阵。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/*  */
        b_error |= parse_cqm( value, p->cqm_4ic, 16 );
    }
    OPT("cqm4py")											/* 设定个别量化矩阵。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/*  */
        b_error |= parse_cqm( value, p->cqm_4py, 16 );
    }
    OPT("cqm4pc")											/* 设定个别量化矩阵。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/*  */
        b_error |= parse_cqm( value, p->cqm_4pc, 16 );
    }
    OPT("cqm8i")											/* 设定亮度和色度量化矩阵。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/*  */
        b_error |= parse_cqm( value, p->cqm_8iy, 64 );
    }
    OPT("cqm8p")											/* 设定亮度和色度量化矩阵。 */
    {
        p->i_cqm_preset = X264_CQM_CUSTOM;					/*  */
        b_error |= parse_cqm( value, p->cqm_8py, 64 );
    }
    OPT("log")												/*  */
        p->i_log_level = atoi(value);						/*  */
    OPT("analyse")											/* analyse:分析, 分解, 解释 */
    {
        p->analyse.inter = 0;								/*  */
        if( strstr( value, "none" ) )  p->analyse.inter =  0;					/*  */
        if( strstr( value, "all" ) )   p->analyse.inter = ~0;					/*  */

        if( strstr( value, "i4x4" ) )  p->analyse.inter |= X264_ANALYSE_I4x4;	/*  */
        if( strstr( value, "i8x8" ) )  p->analyse.inter |= X264_ANALYSE_I8x8;	/*  */
        if( strstr( value, "p8x8" ) )  p->analyse.inter |= X264_ANALYSE_PSUB16x16;	/*  */
        if( strstr( value, "p4x4" ) )  p->analyse.inter |= X264_ANALYSE_PSUB8x8;	/*  */
        if( strstr( value, "b8x8" ) )  p->analyse.inter |= X264_ANALYSE_BSUB16x16;	/*  */
    }
    OPT("8x8dct")										/* 只找到个no-8x8dct;默认值：无弹性8x8离散余弦转换（Adaptive 8x8 DCT）使x264能够智慧弹性地使用I帧的8x8转换。此选项停用该功能。建议：默认值 */
        p->analyse.b_transform_8x8 = atobool(value);	/*  */
    OPT("weightb")										/* 只找到个no-weightb：默认值：无；H.264允许“加权”B帧的参照，它允许变更每个参照影响预测图片的程度。此选项停用该功能。建议：默认值 */
        p->analyse.b_weighted_bipred = atobool(value);	/*  */
    OPT("direct")										/* 默认值：spatial;设定"direct"动态向量（motion vectors）的预测模式。有两种模式可用：spatial和temporal。可以指定none来停用direct动态向量，和指定auto来允许x264在两者之间切换为适合的模式。如果设为auto，x264会在编码结束时输出使用情况的资讯。auto最适合用于两阶段编码，但也可用于一阶段编码。在第一阶段auto模式，x264持续记录每个方法执行到目前为止的好坏，并从该记录挑选下一个预测模式。注意，仅在第一阶段有指定auto时，才应该在第二阶段指定auto；如果第一阶段不是指定auto，第二阶段将会默认为temporal。none模式会浪费位元数，因此强烈不建议。建议：auto */
        b_error |= parse_enum( value, x264_direct_pred_names, &p->analyse.i_direct_mv_pred );	/*  */
    OPT("chroma-qp-offset")								/* 默认值：0;在编码时增加色度平面量化值的偏移。偏移可以为负数。当使用psy-rd或psy-trellis时，x264自动降低此值来提高亮度的品质，其后降低色度的品质。这些设定的默认值会使chroma-qp-offset再减去2。注意：x264仅在同一量化值编码亮度平面和色度平面，直到量化值29。在此之后，色度逐步以比亮度低的量被量化，直到亮度在q51和色度在q39为止。此行为是由H.264标准所要求。 */
        p->analyse.i_chroma_qp_offset = atoi(value);	/*  */
    OPT("me")											/* 默认值：hex;设定全像素（full-pixel）动态估算（motion estimation）的方法。有五个选项：dia（diamond）：最简单的搜寻方法，起始于最佳预测器（predictor），检查上、左、下、右方一个像素的动态向量，挑选其中最好的一个，并重复此过程直到它不再找到任何更好的动态向量为止。hex（hexagon）：由类似策略组成，除了它使用周围6点范围为2的搜寻，因此叫做六边形。它比dia更有效率且几乎没有变慢，因此作为一般用途的编码是个不错的选择。umh（uneven multi-hex）：比hex更慢，但搜寻复杂的多六边形图样以避免遗漏难以找到的动态向量。不像hex和dia，merange参数直接控制umh的搜寻半径，允许增加或减少广域搜寻的大小。esa（exhaustive）：一种在merange内整个动态搜寻空间的高度最佳化智慧搜寻。虽然速度较快，但数学上相当于搜寻该区域每个单一动态向量的暴力（bruteforce）方法。不过，它仍然比UMH还要慢，而且没有带来很大的好处，所以对于日常的编码不是特别有用。tesa（transformed exhaustive）：一种尝试接近在每个动态向量执行Hadamard转换法比较的效果之算法，就像exhaustive，但效果好一点而速度慢一点。 */
        b_error |= parse_enum( value, x264_motion_est_names, &p->analyse.i_me_method );			/*  */
    OPT("merange")										/*  */
        p->analyse.i_me_range = atoi(value);			/*  */
    OPT("mvrange")										/* 默认值：-1 (自动)；设定动态向量的最大（垂直）范围（单位是像素）。默认值依level不同：Level 1/1b：64; Level 1.1~2.0：128; Level 2.1~3.0：256; Level 3.1+：512。注意：如果想要手动覆写mvrange，在设定时从上述值减去0.25（例如--mvrange 127.75）。 建议：默认值 */
        p->analyse.i_mv_range = atoi(value);			/*  */
    OPT("subme")										/* 默认值：7；设定子像素((subpixel)估算复杂度。值越高越好。层级1~5只是控制子像素细分（refinement）强度。层级6为模式决策启用RDO，而层级8为动态向量和内部预测模式启用RDO。RDO层级明显慢于先前的层级。使用小于2的值不但会启用较快且品质较低的lookahead模式，而且导致较差的--scenecut决策，因此不建议。可用的值：0：Fullpel only;1：QPel SAD 1 iteration;2：QPel SATD 2 iterations;3：HPel on MB then QPel;4：Always QPel;5：Multi QPel + bi-directional motion estimation;6：RD on I/P frames;7：RD on all frames;8：RD refinement on I/P frames;9：RD refinement on all frames;10：QP-RD (requires --trellis=2, --aq-mode>0); */
        p->analyse.i_subpel_refine = atoi(value);		/*  */
    OPT("bime")											/*  */
        p->analyse.b_bidir_me = atobool(value);			/*  */
    OPT("chroma-me")									/* 只找到个no-chroma-me：默认值：无；通常，亮度（luma）和色度（chroma）两个平面都会做动态估算。此选项停用色度动态估算来提高些微速度。建议：默认值 */
        p->analyse.b_chroma_me = atobool(value);		/*  */
    OPT("b-rdo")										/*  */
        p->analyse.b_bframe_rdo = atobool(value);		/*  */
    OPT("mixed-refs")									/* 只找到个no-mixed-refs：默认值：无;混合参照会以每个8x8分割为基础来选取参照，而不是以每个宏区块为基础。当使用多个参照帧时这会改善品质，虽然要损失一些速度。设定此选项会停用该功能。建议：默认值; */
        p->analyse.b_mixed_references = atobool(value);	/*  */
    OPT("trellis")										/* 默认值：1：执行Trellis quantization来提高效率。0：停用。1：只在一个宏区块的最终编码上启用。2：在所有模式决策上启用。在宏区块时提供了速度和效率之间的良好平衡。在所有决策时则更加降低速度。建议：默认值注意：需要--cabac  */
        p->analyse.i_trellis = atoi(value);				/*  */
    OPT("fast-pskip")									/* 只找到个no-fast-pskip：默认值：无；停用P帧的早期略过侦测（early skip detection）。非常轻微地提高品质，但要损失很多速度。建议：默认值 */
        p->analyse.b_fast_pskip = atobool(value);		/*  */
    OPT("dct-decimate")									/* 只找到个no-dct-decimate:默认值：无;DCT Decimation会舍弃它认为“不必要的”DCT区块。这会改善编码效率，而降低的品质通常微不足道。设定此选项会停用该功能。建议：默认值 */
        p->analyse.b_dct_decimate = atobool(value);		/*  */
    OPT("nr")											/*  */
        p->analyse.i_noise_reduction = atoi(value);		/*  */
    OPT("bitrate")										/* 默认值：无;三种位元率控制方法之二。以目标位元率模式来编码视讯。目标位元率模式意味着最终档案大小是已知的，但最终品质则未知。x264会尝试把给定的位元率作为整体平均值来编码视讯。参数的单位是千位元/秒（8位元=1字节）。注意，1千位元(kilobit)是1000位元，而不是1024位元。此设定通常与--pass在两阶段（two-pass）编码一起使用。此选项与--qp和--crf互斥。 */
    {
        p->rc.i_bitrate = atoi(value);					/*  */
        p->rc.i_rc_method = X264_RC_ABR;				/*  */
    }
    OPT("qp")											/* 默认值：无;三种位元率控制方法之一。设定x264以固定量化值（Constant Quantizer）模式来编码视讯。这里给的值是指定P帧的量化值。I帧和B帧的量化值则是从--ipratio和--pbratio中取得。CQ模式把某个量化值作为目标，这意味着最终档案大小是未知的（虽然可以透过一些方法来准确地估计）。将值设为0会产生无失真输出。对于相同视觉品质，qp会比--crf产生更大的档案。qp模式也会停用弹性量化，因为按照定义“固定量化值”意味着没有弹性量化。此选项与--bitrate和--crf互斥。虽然qp不需要lookahead来执行因此速度较快，但通常应该改用--crf */
    {
        p->rc.i_qp_constant = atoi(value);	/*  */
        p->rc.i_rc_method = X264_RC_CQP;	/*  */
    }
    OPT("crf")		/* 默认值：23.0;最后一种位元率控制方法：固定位元率系数（Constant Ratefactor）。当qp是把某个量化值作为目标，而bitrate是把某个档案大小作为目标时，crf则是把某个“品质”作为目标。构想是让crf n提供的视觉品质与qp n相同，只是档案更小一点。crf值的度量单位是“位元率系数（ratefactor）”。CRF是借由降低“较不重要”的帧之品质来达到此目的。在此情况下，“较不重要”是指在复杂或高动态场景的帧，其品质不是很耗费位元数就是不易察觉，所以会提高它们的量化值。从这些帧里所节省下来的位元数被重新分配到可以更有效利用的帧。CRF花费的时间会比两阶段编码少，因为两阶段编码中的“第一阶段”被略过了。另一方面，要预测CRF编码的最终位元率是不可能的。根据情况哪种位元率控制模式更好是由你来决定。 此选项与--qp和--bitrate互斥。 */
    {
        p->rc.i_rf_constant = atoi(value);	/*  */
        p->rc.i_rc_method = X264_RC_CRF;	/*  */
    }
    OPT("qpmin")				/* 默认值：0;定义x264可以使用的最小量化值。量化值越小，输出就越接近输入。到了一定的值，x264的输出看起来会跟输入一样，即使它并不完全相同。通常没有理由允许x264花费比这更多的位元数在任何特定的宏区块上。当弹性量化启用时（默认启用），不建议提高qpmin，因为这会降低帧里面平面背景区域的品质。 */
        p->rc.i_qp_min = atoi(value);		/*  */
    OPT("qpmax")				/* 默认值：51;定义x264可以使用的最大量化值。默认值51是H.264规格可供使用的最大量化值，而且品质极低。此默认值有效地停用了qpmax。如果想要限制x264可以输出的最低品质，可以将此值设小一点（通常30~40），但通常并不建议调整此值。 */
        p->rc.i_qp_max = atoi(value);		/*  */
    OPT("qpstep")							/* 默认值：4;设定两帧之间量化值的最大变更幅度。 */
        p->rc.i_qp_step = atoi(value);		/*  */
    OPT("ratetol")				/*  */
        p->rc.f_rate_tolerance = !strncmp("inf", value, 3) ? 1e9 : atof(value);		/*  */
    OPT("vbv-maxrate")							/* 默认值：0;设定重新填满VBV缓冲的最大位元率。VBV会降低品质，所以必要时才使用。 */
        p->rc.i_vbv_max_bitrate = atoi(value);	/*  */
    OPT("vbv-bufsize")							/* 默认值：0;设定VBV缓冲的大小（单位是千位元）。 VBV会降低品质，所以必要时才使用。 */
        p->rc.i_vbv_buffer_size = atoi(value);	/*  */
    OPT("vbv-init")								/* 默认值：0.9;设定VBV缓冲必须填满多少才会开始播放。如果值小于1，初始的填满量是：vbv-init * vbv-bufsize。否则该值即是初始的填满量（单位是千位元）。 */
        p->rc.f_vbv_buffer_init = atof(value);	/*  */
    OPT("ipratio")								/* 默认值：1.40;修改I帧量化值相比P帧量化值的目标平均增量。越大的值会提高I帧的品质。 */
        p->rc.f_ip_factor = atof(value);		/*  */
    OPT("pbratio")								/* 默认值：1.30;修改B帧量化值相比P帧量化值的目标平均减量。越大的值会降低B帧的品质。当mbtree启用时（默认启用），此设定无作用，mbtree会自动计算最佳值。 */
        p->rc.f_pb_factor = atof(value);		/*  */
    OPT("pass")									/* 默认值：无;此为两阶段编码的一个重要设定。它控制x264如何处理--stats档案。有三种设定：1：建立一个新的统计资料档案。在第一阶段使用此选项。2：读取统计资料档案。在最终阶段使用此选项。3：读取统计资料档案并更新。统计资料档案包含每个输入帧的资讯，可以输入到x264以改善输出。构想是执行第一阶段来产生统计资料档案，然后第二阶段将建立一个最佳化的视讯编码。改善的地方主要是从更好的位元率控制中获益。 */
    {
        int i = x264_clip3( atoi(value), 0, 3 );	/*  */
        p->rc.b_stat_write = i & 1;					/*  */
        p->rc.b_stat_read = i & 2;					/*  */
    }
    OPT("stats")									/* 默认值："x264_2pass.log";设定x264读取和写入统计资料档案的位置。 */
    {
        p->rc.psz_stat_in = strdup(value);			/*  */
        p->rc.psz_stat_out = strdup(value);			/*  */
    }
    OPT("rceq")									/*  */
        p->rc.psz_rc_eq = strdup(value);		/*  */
    OPT("qcomp")								/* 默认值：0.60;量化值曲线压缩系数。0.0是固定位元率，1.0则是固定量化值。当mbtree启用时，它会影响mbtree的强度（qcomp越大，mbtree越弱）。 建议：默认值 */
        p->rc.f_qcompress = atof(value);		/*  */
    OPT("qblur")								/* 默认值：0.5;在曲线压缩之后，以给定的半径范围套用高斯模糊于量化值曲线。不怎么重要的设定 */
        p->rc.f_qblur = atof(value);			/*  */
    OPT("cplxblur")								/* 默认值：20.0;以给定的半径范围套用高斯模糊（gaussian blur）于量化值曲线。这意味着分配给每个帧的量化值会被它的邻近帧模糊掉，以此来限制量化值波动 */
        p->rc.f_complexity_blur = atof(value);	/*  */
    OPT("zones")								/* 默认值：无;调整视讯的特定片段之设定。可以修改每区段的大多数x264选项;一个单一区段的形式为<起始帧>,<结束帧>,<选项>。 多个区段彼此以"/"分隔。范例：0,1000,b=2/1001,2000,q=20,me=3,b-bias=-1000;建议：默认值 */
        p->rc.psz_zones = strdup(value);		/*  */
    OPT("psnr")									/*  */
        p->analyse.b_psnr = atobool(value);		/*  */
    OPT("aud")									/*  */
        p->b_aud = atobool(value);				/*  */
    OPT("sps-id")								/*  */
        p->i_sps_id = atoi(value);				/*  */
    OPT("repeat-headers")						/*  */
        p->b_repeat_headers = atobool(value);	/*  */
    else
        return X264_PARAM_BAD_NAME;
#undef OPT
#undef atobool

    return b_error ? X264_PARAM_BAD_VALUE : 0;
}

/****************************************************************************
 * x264_log:定义log级别

 ****************************************************************************/
void x264_log( x264_t *h, int i_level, const char *psz_fmt, ... )
{
    if( i_level <= h->param.i_log_level )
    {
        va_list arg;	/* 在函数体中声明一个va_list，然后用va_start函数来获取参数列表中的参数，使用完毕后调用va_end()结束。 */
        va_start( arg, psz_fmt );	/* start:必须以va_start开始，并以va_end结尾 */
        h->param.pf_log( h->param.p_log_private, i_level, psz_fmt, arg );/* 貌似函数指针？ */
        va_end( arg );	/* end	:必须以va_end结尾，把ap指针清为NULL */
    }
}

/*
 * 日志_默认 (把日志写入了文件)
 * x264 error ...
 * x264 warning ...
 * x264 info ...
 * x264 debug ...
 * x264 unknown ...
*/
static void x264_log_default( void *p_unused, int i_level, const char *psz_fmt, va_list arg )
{
    char *psz_prefix;
    switch( i_level )
    {
        case X264_LOG_ERROR:
            psz_prefix = "error";	//prefix:前缀
            break;
        case X264_LOG_WARNING:
            psz_prefix = "warning";	/* 警告 */
            break;
        case X264_LOG_INFO:
            psz_prefix = "info";	/* 信息 */
            break;
        case X264_LOG_DEBUG:
            psz_prefix = "debug";	/* 调试 */
            break;
        default:
            psz_prefix = "unknown";	/* 未知 */
            break;
    }
    fprintf( stderr, "x264 [%s]: ", psz_prefix );	/* fprintf()函数根据指定的format(格式)(格式)发送信息(参数)到由stream(流)指定的文件. fprintf()只能和printf()一样工作. fprintf()的返回值是输出的字符数,发生错误时返回一个负值. http://baike.baidu.com/view/656682.htm */
    vfprintf( stderr, psz_fmt, arg );	/* vfprintf()会根据参数format字符串来转换并格式化数据，然后将结果输出到参数stream指定的文件中，直到出现字符串结束（‘\0’）为止。http://baike.baidu.com/view/1081188.htm */
}

/****************************************************************************
 * x264_picture_alloc:设置picture参数,根据输出图像格式分配空间
 ****************************************************************************/
void x264_picture_alloc( x264_picture_t *pic, int i_csp, int i_width, int i_height )
{
	//对传入的第一个参数，就是一个结构体的字段赋值
    pic->i_type = X264_TYPE_AUTO;//#define X264_TYPE_AUTO  0x0000  /* Let x264 choose the right type */
    pic->i_qpplus1 = 0;
    pic->img.i_csp = i_csp; //x264_picture_t有四个字段，最后一个字段是个指针数组，用来存放动态分配的内存的地址

    switch( i_csp & X264_CSP_MASK )	//根据色彩空间区分 //实际就是0x0001 & 0x00ff 按位与
    {
        case X264_CSP_I420://0x0001
        case X264_CSP_YV12://0x0004
            pic->img.i_plane = 3;
			//分配内存，把返回的地址保存到字段数组中
            pic->img.plane[0] = x264_malloc( 3 * i_width * i_height / 2 );	/* 分配了内存，返回首地址 */ //像素总数 * 1.5
            pic->img.plane[1] = pic->img.plane[0] + i_width * i_height;		/* 首地址 + 像素总数 ->另一段内存地址 */
            pic->img.plane[2] = pic->img.plane[1] + i_width * i_height / 4; /* 再向后移动地址 */
            pic->img.i_stride[0] = i_width;
            pic->img.i_stride[1] = i_width / 2;
            pic->img.i_stride[2] = i_width / 2;
            break;

        case X264_CSP_I422://0x0002
            pic->img.i_plane = 3;
            pic->img.plane[0] = x264_malloc( 2 * i_width * i_height );
            pic->img.plane[1] = pic->img.plane[0] + i_width * i_height;
            pic->img.plane[2] = pic->img.plane[1] + i_width * i_height / 2;
            pic->img.i_stride[0] = i_width;
            pic->img.i_stride[1] = i_width / 2;
            pic->img.i_stride[2] = i_width / 2;
            break;

        case X264_CSP_I444:
            pic->img.i_plane = 3;
            pic->img.plane[0] = x264_malloc( 3 * i_width * i_height );
            pic->img.plane[1] = pic->img.plane[0] + i_width * i_height;
            pic->img.plane[2] = pic->img.plane[1] + i_width * i_height;
            pic->img.i_stride[0] = i_width;
            pic->img.i_stride[1] = i_width;
            pic->img.i_stride[2] = i_width;
            break;

        case X264_CSP_YUYV://YUY2（和YUYV）格式为每个像素保留Y分量，而UV分量在水平方向上每两个像素采样一次
            pic->img.i_plane = 1;
            pic->img.plane[0] = x264_malloc( 2 * i_width * i_height );//像素总数*2
            pic->img.i_stride[0] = 2 * i_width;
            break;

        case X264_CSP_RGB:
        case X264_CSP_BGR://RGB->BGR：把 R 和 B 的位置换一下就行了
            pic->img.i_plane = 1;
            pic->img.plane[0] = x264_malloc( 3 * i_width * i_height );//像素总数*3
            pic->img.i_stride[0] = 3 * i_width;
            break;

        case X264_CSP_BGRA:
            pic->img.i_plane = 1;
            pic->img.plane[0] = x264_malloc( 4 * i_width * i_height );//像素总数*4
            pic->img.i_stride[0] = 4 * i_width;
            break;

        default:
            fprintf( stderr, "invalid CSP\n" );
            pic->img.i_plane = 0;
            break;
    }//在平面格式中，Y、U 和 V 组件作为三个单独的平面进行存储。 
}

/****************************************************************************
 * x264_picture_clean:释放分配的图像空间
 ****************************************************************************/
void x264_picture_clean( x264_picture_t *pic )
{
    x264_free( pic->img.plane[0] );//

    /* 
	just to be safe
	仅仅为了安全使用内存
	*/
    memset( pic, 0, sizeof( x264_picture_t ) );
}

/****************************************************************************
 * x264_nal_encode:nal单元编码
 * 编码一个nal至一个缓冲区，设置尺寸
 * 参数b_annexeb:是否加起始码
 * nalu 打包，加入前缀，header, 数据中加入0x03
 * 最后那个参数是个结构体，有四个字段，分别是：优先级，类别，x，有效载荷
 ****************************************************************************/
int x264_nal_encode( void *p_data, int *pi_data, int b_annexeb, x264_nal_t *nal )
{
    uint8_t *dst = p_data;
    uint8_t *src = nal->p_payload;//指向有效荷载的起始地址
    uint8_t *end = &nal->p_payload[nal->i_payload];//最后一个字节的地址？
					//取出数组的最后一个元素，对该元素取地址，得到数组的最后地址
    int i_count = 0;

    /* FIXME this code doesn't check overflow */

	//nalu surfix(后缀) 0x 00 00 00 01//这句网上搜的注释[毕厚杰：Page158]，起始码
    if( b_annexeb )//annex:附件，附加
    {
        /* long nal start code (we always use long ones)
		 * 当数据流是存储在介质上时，在每个NAL 前添加起始码：0x000001
		 * 在某些类型的介质上，为了寻址的方便，要求数据流在长度上对齐，或必须是某个常数的倍数。考
		 * 虑到这种情况，H.264 建议在起始码前添加若干字节的0 来填充，直到该NAL 的长度符合要求。
		 * 在这样的机制下，解码器在码流中检测起始码，作为一个NAL 的起始标识，当检测到下一个
		 * 起始码时当前NAL 结束。H.264 规定当检测到0x000000 时也可以表征当前NAL 的结束，这是因为
		 * 连着的三个字节的0 中的任何一个字节的0 要么属于起始码要么是起始码前面添加的0。
		*/
        *dst++ = 0x00;//++后置，先使用dst，本句执行完后，再执行递增
        *dst++ = 0x00;//*dst=0x00;dst++;
        *dst++ = 0x00;
        *dst++ = 0x01;
		//dst实际指向的就是x264.c:data[3000000]
    }
	//data现在先存入了: 0x 00 00 00 01
	//下面实现nal头的存入
	//第1位禁止位
	//第2、3位是优先级
	//其余5位是类型
	//以下是先移到相应位置，再按位或，就把它位放到一个字节里了
    /* nal header [毕厚杰：Page 145 181] 头部1字节，包括禁止位1bit优先级2bit和NAL类型5bit，后面紧跟有效载RBSP*/
    *dst++ = ( 0x00 << 7 ) | ( nal->i_ref_idc << 5 ) | nal->i_type;//按位或C++primer第三版Page136页位操作符
							//递增操作符的后置形式，先用当前值，然后才递增C++primer第三版Page127递增和递减操作符
							//左移7位，正好把第一个隐藏位空下了，
							//左移5位，正好是类型用的5位，七位和五位中间正好是二位，用来放优先级
							//最右边的5位，正好可以表示32，用来放类型
	//insert 0x03//这儿看来好象是见2个字节就插入03吗，这样不是插了很多吗

	//防止原始内容与起始码冲突
    while( src < end )//从首地址循环到尾地址
    {
        if( i_count == 2 && *src <= 0x03 )//毕厚杰：图6.6 page158，4个字节序列都要加0x03
        {									//0x01,0x02,0x03
            *dst++ = 0x03;
            i_count = 0;//插过0x03后计数归0
        }
		/*
		如果检测到这些序列存在，编码器将在最后一个字节前插入一个新的字节：0x03
		0x 00 00 00 -> 0x 00 00 03 00
		0x 00 00 01 -> 0x 00 00 03 01
		0x 00 00 02 -> 0x 00 00 03 02
		0x 00 00 03 -> 0x 00 00 03 03
		*/
        if( *src == 0 )	//第一个0x00
        {
            i_count++;
        }
        else
        {
            i_count = 0;
        }
        *dst++ = *src++;
    }

	//count nalu length in byte//统计出nalu的长度，单位是字节
    *pi_data = dst - (uint8_t*)p_data;//p_data是函数参数，没动过，dst一直在移动,pi_data就是专门用来保存这个新长度的

    return *pi_data;
}

/****************************************************************************
 * x264_nal_decode:nal单元解码
 * 解码一个缓冲区 nal (p_data) 到 一个 x264_nal_t 结构体 (*nal)
 * 思路:网络传输过来的是一个字节,并不是如结构体x264_nal_t定义的用int表示的一个个结构体字段
 *      我们要把传来的第一个字节拆开用结构体中的int字段来表示,把前3比特放到x264_nal_t.i_ref_idc
 *      后5比特放到x264_nal_t.i_type
 *		并计算出x264_nal_t的其它字段值（x264_nal_t共4个字段，从NAL第一字节已得到其中的前2个）
 ****************************************************************************/
int x264_nal_decode( x264_nal_t *nal, void *p_data, int i_data )
{
    uint8_t *src = p_data;				//缓冲区起始地址,   unsigned char,就是一个8位的char,一个字节
    uint8_t *end = &src[i_data];		//缓冲区结束地址,
    uint8_t *dst = nal->p_payload;		//指向有效荷载的起始地址

	/*
	 * 0 1 2 3 4 5 6 7 第一字节的8比特
	 * | | | + + + + + 前3比特是优先级;后5比特是nal单元类型
	 * --3-- ----5----
	*/
    nal->i_type    = src[0]&0x1f;		//类型占本字节的5位 0x1f-> 0001 1111 本字节与0001 1111按位与，取出nal的类型值
    nal->i_ref_idc = (src[0] >> 5)&0x03;//第1个字节右移5，得到优先级,占本字节的2位

    src++;	//第2个字节及后面的字节

    while( src < end )	//循环第2及以后字节
    {
        if( src < end - 3 && src[0] == 0x00 && src[1] == 0x00  && src[2] == 0x03 )
			//开始地址 < 结束地址 并且 第1字节 == 0x00
			//					  并且 第2字节 == 0x00
			//					  并且 第3字节 == 0x03
        {
            *dst++ = 0x00;//dst后移1字节
            *dst++ = 0x00;//同上

            src += 3;	//src=src+3
						/*
						 * src   最初的src地址是起始的第一字节
						 * src++ 移到了第2字节(while前的那句)
						 * src += 3 即 src = src + 3
						 * 比如起始地址为0, src = 0 + 3
						 * 0 1 2 3  
						 * 可以看到,3实际是第4个字节了
						 * 这样,0x 00 00 03 01 虽然只是比较了前3字节,但是
						 * src += 3 ,实际上把后面的01也跳过了
						*/
            continue;
        }
        *dst++ = *src++;//左边是dst的值(*dst)++,右侧是(*src)++
						//先结合* ,然后指针自增1
						// 这样理解，++后置的情况下，先用它的值，最后才++
						//是*src给*dst
						/* 等价于如下的三行代码：
						* *dst = *src
						* dst++;
						* src++;
						* -----------------
						* *j++的理解还应该是 *（j++）
						* *(j++)就是*j
						* 注意：不是先计算括号里面的++
						*/
    }//去掉添加的0x00 00 03 01 (4字节)

    nal->i_payload = dst - (uint8_t*)p_data;//i_payload是有效载荷的长度,以字节为单位
					/* 直观的理解，负载的长度为 nal->p_payload 的地址 减
					 * 1字节的Nal Header 减
					 * 0x00 00 03 01 (4字节)(如果有的话) 减					 * 
					 * NAL起始地址p_data
					 * -----------------------
					 * dst经过几次移动
					 * 第一次，uint8_t *dst = nal->p_payload;		//指向有效荷载的起始地址
					 * 第二次，*dst++ = 0x00;
					 * 第三次，*dst++ = 0x00;
					 * 第四次，*dst++ = *src++;
					*/
					
    return 0;
}



/****************************************************************************
 * x264_malloc:X264内部定义的内存分配

 ****************************************************************************/
void *x264_malloc( int i_size )
{
	#ifdef SYS_MACOSX
		/* Mac OS X always returns 16 bytes aligned memory */
		return malloc( i_size );
	#elif defined( HAVE_MALLOC_H )
		return memalign( 16, i_size );//void *memalign( size_t alignment, size_t size );  其中alignment指定对齐的字节数,必须是2的正整数次幂,size是请求内存的尺寸,返回的内存首地址保证是对齐alignment的
	#else
		uint8_t * buf;
		uint8_t * align_buf;
		buf = (uint8_t *) malloc( i_size + 15 + sizeof( void ** ) + sizeof( int ) );
		align_buf = buf + 15 + sizeof( void ** ) + sizeof( int );	/* 字节对齐，在后面跟着加了块 */
		align_buf -= (long) align_buf & 15;	//align_buf = align_buf - ((long) align_buf & 15)
		*( (void **) ( align_buf - sizeof( void ** ) ) ) = buf;
		*( (int *) ( align_buf - sizeof( void ** ) - sizeof( int ) ) ) = i_size;
		return align_buf;	/* uint8_t * align_buf; */
	#endif
}

/****************************************************************************
 * x264_free:X264内存释放

 ****************************************************************************/
void x264_free( void *p )
{
    if( p )
    {
	#if defined( HAVE_MALLOC_H ) || defined( SYS_MACOSX )
		free( p );//C语言的库函数，释放已分配的块
	#else
		free( *( ( ( void **) p ) - 1 ) );
	#endif
    }
}

/****************************************************************************
 * x264_realloc:X264重新分配图像空间

 ****************************************************************************/
void *x264_realloc( void *p, int i_size )
{
#ifdef HAVE_MALLOC_H
    return realloc( p, i_size );
#else
    int       i_old_size = 0;
    uint8_t * p_new;
    if( p )
    {
        i_old_size = *( (int*) ( (uint8_t*) p ) - sizeof( void ** ) -
                         sizeof( int ) );	/* ?????? */
    }
    p_new = x264_malloc( i_size );		/* 新开辟一个内存块，大小由传入的参数确定 */
    if( i_old_size > 0 && i_size > 0 )
    {
        memcpy( p_new, p, ( i_old_size < i_size ) ? i_old_size : i_size );	/* 把旧内存块中的内容拷过来 */
    }
    x264_free( p );		/* 释放旧的内存块 */
    return p_new;		/* 把新内存块的地址返回去 */
#endif
}

/****************************************************************************
 * x264_reduce_fraction:分数化简
 * reduce:将…概括[简化]
 * fraction:分数
 * 比如4/12，可能最后是1/3
 * http://wmnmtm.blog.163.com/blog/static/38245714201173073922482/
 ****************************************************************************/
void x264_reduce_fraction( int *n, int *d )
{
    int a = *n;
    int b = *d;
    int c;
    if( !a || !b )	/* 即a和b中，只要有 0 存在 */
        return;
    c = a % b;
    while(c)
    {
	a = b;
	b = c;
	c = a % b;
    }
    *n /= b;	/* 把原值改掉了 */
    *d /= b;	/* 把原值改掉了 */
}

/****************************************************************************
 * x264_slurp_file:将文件读入分配的缓存区
 * slurp:啜食
 * 
 ****************************************************************************/
char *x264_slurp_file( const char *filename )
{
    int b_error = 0;
    int i_size;
    char *buf;
    FILE *fh = fopen( filename, "rb" );			/* r 以只读方式打开文件，该文件必须存在，上述的形态字符串都可以再加一个b字符，如rb、w+b或ab＋等组合，加入b 字符用来告诉函数库打开的文件为二进制文件，而非纯文字文件。不过在POSIX系统，包含Linux都会忽略该字符。 */
    if( !fh )									/* if(fp==NULL) //如果失败了 */
        return NULL;
    b_error |= fseek( fh, 0, SEEK_END ) < 0;	/* http://baike.baidu.com/view/656699.htm */
    b_error |= ( i_size = ftell( fh ) ) <= 0;	/* 函数 ftell() 用于得到文件位置指针当前位置相对于文件首的偏移字节数。在随机方式存取文件时，由于文件位置频繁的前后移动，程序不容易确定文件的当前位置。调用函数ftell()就能非常容易地确定文件的当前位置。 */
    b_error |= fseek( fh, 0, SEEK_SET ) < 0;	/* ftell(fp);利用函数 ftell() 也能方便地知道一个文件的长。如以下语句序列： fseek(fp, 0L,SEEK_END); len =ftell(fp); 首先将文件的当前位置移到文件的末尾，然后调用函数ftell()获得当前位置相对于文件首的位移，该位移值等于文件所含字节数。 */
    if( b_error )								/* fseek( fh, 0, SEEK_SET );定位到文件开头 */
        return NULL;
    buf = x264_malloc( i_size+2 );				/* ?+2 */
    if( buf == NULL )
        return NULL;
    b_error |= fread( buf, 1, i_size, fh ) != i_size;
    if( buf[i_size-1] != '\n' )					/* !+2 */
        buf[i_size++] = '\n';
    buf[i_size] = 0;							/* 已有了'\n'，为什么又弄个0 */
    fclose( fh );								/* 关闭一个流。注意：使用fclose()函数就可以把缓冲区内最后剩余的数据输出到磁盘文件中，并释放文件指针和有关的缓冲区。 */
    if( b_error )								/* 如果出错才清除buf */
    {
        x264_free( buf );						/*  */
        return NULL;
    }
    return buf;									/* 返回缓冲区(已存有文件内容) */
}

/****************************************************************************
 * x264_param2string:转换参数为字符串,返回字符串存放的地址

 ****************************************************************************/
char *x264_param2string( x264_param_t *p, int b_res )
{
    char *buf = x264_malloc( 1000 );
    char *s = buf;

    if( b_res )
    {
        s += sprintf( s, "%dx%d ", p->i_width, p->i_height );	/* sprintf:把格式化的数据写入某个字符串 http://baike.baidu.com/view/1295144.htm */
        s += sprintf( s, "fps=%d/%d ", p->i_fps_num, p->i_fps_den );
    }

    s += sprintf( s, "cabac=%d", p->b_cabac );
    s += sprintf( s, " ref=%d", p->i_frame_reference );
    s += sprintf( s, " deblock=%d:%d:%d", p->b_deblocking_filter,
                  p->i_deblocking_filter_alphac0, p->i_deblocking_filter_beta );
    s += sprintf( s, " analyse=%#x:%#x", p->analyse.intra, p->analyse.inter );
    s += sprintf( s, " me=%s", x264_motion_est_names[ p->analyse.i_me_method ] );
    s += sprintf( s, " subme=%d", p->analyse.i_subpel_refine );
    s += sprintf( s, " brdo=%d", p->analyse.b_bframe_rdo );
    s += sprintf( s, " mixed_ref=%d", p->analyse.b_mixed_references );
    s += sprintf( s, " me_range=%d", p->analyse.i_me_range );
    s += sprintf( s, " chroma_me=%d", p->analyse.b_chroma_me );
    s += sprintf( s, " trellis=%d", p->analyse.i_trellis );
    s += sprintf( s, " 8x8dct=%d", p->analyse.b_transform_8x8 );
    s += sprintf( s, " cqm=%d", p->i_cqm_preset );
    s += sprintf( s, " chroma_qp_offset=%d", p->analyse.i_chroma_qp_offset );
    s += sprintf( s, " slices=%d", p->i_threads );
    s += sprintf( s, " nr=%d", p->analyse.i_noise_reduction );
    s += sprintf( s, " decimate=%d", p->analyse.b_dct_decimate );

    s += sprintf( s, " bframes=%d", p->i_bframe );
    if( p->i_bframe )
    {
        s += sprintf( s, " b_pyramid=%d b_adapt=%d b_bias=%d direct=%d wpredb=%d bime=%d",
                      p->b_bframe_pyramid, p->b_bframe_adaptive, p->i_bframe_bias,
                      p->analyse.i_direct_mv_pred, p->analyse.b_weighted_bipred,
                      p->analyse.b_bidir_me );
    }

    s += sprintf( s, " keyint=%d keyint_min=%d scenecut=%d",
                  p->i_keyint_max, p->i_keyint_min, p->i_scenecut_threshold );

    s += sprintf( s, " rc=%s", p->rc.i_rc_method == X264_RC_ABR ?
                               ( p->rc.b_stat_read ? "2pass" : p->rc.i_vbv_buffer_size ? "cbr" : "abr" )
                               : p->rc.i_rc_method == X264_RC_CRF ? "crf" : "cqp" );
    if( p->rc.i_rc_method == X264_RC_ABR || p->rc.i_rc_method == X264_RC_CRF )
    {
        if( p->rc.i_rc_method == X264_RC_CRF )
            s += sprintf( s, " crf=%d", p->rc.i_rf_constant );
        else
            s += sprintf( s, " bitrate=%d ratetol=%.1f",
                          p->rc.i_bitrate, p->rc.f_rate_tolerance );
        s += sprintf( s, " rceq='%s' qcomp=%.2f qpmin=%d qpmax=%d qpstep=%d",
                      p->rc.psz_rc_eq, p->rc.f_qcompress,
                      p->rc.i_qp_min, p->rc.i_qp_max, p->rc.i_qp_step );
        if( p->rc.b_stat_read )
            s += sprintf( s, " cplxblur=%.1f qblur=%.1f",
                          p->rc.f_complexity_blur, p->rc.f_qblur );
        if( p->rc.i_vbv_buffer_size )
            s += sprintf( s, " vbv_maxrate=%d vbv_bufsize=%d",
                          p->rc.i_vbv_max_bitrate, p->rc.i_vbv_buffer_size );
    }
    else if( p->rc.i_rc_method == X264_RC_CQP )
        s += sprintf( s, " qp=%d", p->rc.i_qp_constant );
    if( !(p->rc.i_rc_method == X264_RC_CQP && p->rc.i_qp_constant == 0) )
    {
        s += sprintf( s, " ip_ratio=%.2f", p->rc.f_ip_factor );
        if( p->i_bframe )
            s += sprintf( s, " pb_ratio=%.2f", p->rc.f_pb_factor );
        if( p->rc.i_zones )
            s += sprintf( s, " zones" );
    }

    return buf;
}

