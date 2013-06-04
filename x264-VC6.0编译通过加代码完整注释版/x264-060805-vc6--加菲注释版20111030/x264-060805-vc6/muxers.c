/*****************************************************************************
 * muxers.c: h264 file i/o plugins
 * muxer:匹配
 *****************************************************************************
 * Copyright (C) 2003-2006 x264 project
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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "common/common.h"
#include "x264.h"
#include "matroska.h"
#include "muxers.h"

#ifndef _MSC_VER
#include "config.h"
#endif

#ifdef AVIS_INPUT
#include <windows.h>
#include <vfw.h>
#endif

#ifdef MP4_OUTPUT
#include <gpac/isomedia.h>
#endif

typedef struct {
    FILE *fh;			//输入文件的文件指针
    int width, height;	//宽和高，应该是指视频画面的尺寸
    int next_frame;		//下一帧
} yuv_input_t;

/* 
raw 420 yuv file operation(操作) 
打开一个文件，已经文件路径
*/
int open_file_yuv( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param )
{
    yuv_input_t *h = malloc(sizeof(yuv_input_t));//申请一块内存给yuv_input_t结构体用，内存地址存到h
	/*对结构体字段赋值*/
	h->width = p_param->i_width;
    h->height = p_param->i_height;
    h->next_frame = 0;

	printf("\n(muxer.c) open_file_yuv(...) done");//zjh，当测试序列后缀为.yuv时显示此句

    if( !strcmp(psz_filename, "-") )
        h->fh = stdin;//这句应该是代表文件已经打开，这儿只需接收这个句柄/地址
    else
        h->fh = fopen(psz_filename, "rb");/* 打开一个文件 文件顺利打开后，指向该流的文件指针就会被返回。如果文件打开失败则返回NULL，并把错误代码存在errno 中 */
											//rb 以只读方式打开文件，该文件必须存在,加入b 字符用来告诉函数库打开的文件为二进制文件，而非纯文字文件
    if( h->fh == NULL )//文件打开失败
        return -1;

    *p_handle = (hnd_t)h;//把此处打开的文件的指针传出去，左边的p_handle在调本函数时以参数方式提供过来

    //printf("文件名：%s\n",psz_filename);//zjh
	return 0;
}

/*
得到帧的总数

这儿传的参数，就是上面open_file_yuv结束前传出的那个文件指针
这儿通过文件的总长度/每帧尺寸来计算得到总帧数，可以看到，文件必须是yuv420的，如果是其它格式或者包含音频的，是不能用此函数计算的
*/
int get_frame_total_yuv( hnd_t handle )
{
    yuv_input_t *h = handle;
    int i_frame_total = 0;	/* 数据帧数量 int get_frame_total_yuv( hnd_t handle ) { return i_frame_total }*/

    if( !fseek( h->fh, 0, SEEK_END ) )//重定位流(数据流/文件)上的文件内部位置指针;函数设置文件指针stream的位置。如果执行成功，stream将指向以fromwhere（偏移起始位置：文件头0，当前位置1，文件尾2）为基准，偏移offset（指针偏移量）个字节的位置。如果执行失败(比如offset超过文件自身大小)，则不改变stream指向的位置。
    {
        uint64_t i_size = ftell( h->fh );//函数 ftell() 用于得到文件位置指针当前位置相对于文件首的偏移字节数。在随机方式存取文件时，由于文件位置频繁的前后移动，程序不容易确定文件的当前位置。调用函数ftell()就能非常容易地确定文件的当前位置
        fseek( h->fh, 0, SEEK_SET );//重定位流(数据流/文件)上的文件内部位置指针,注意：不是定位文件指针，文件指针指向文件/流。位置指针指向文件内部的字节位置，随着文件的读取会移动，文件指针如果不重新赋值将不会改变指向别的文件
									//其中fseek中第一个参数为文件指针，第二个参数为偏移量，起始位置，SEEK_END 表示文件尾，SEEK_SET 表示文件头。
        i_frame_total = (int)(i_size / ( h->width * h->height * 3 / 2 ));//计算总的帧数, 这里乘以1.5是因为一个编码单位是一个亮度块加2个色度块，大小上等于1.5个亮度块
    }		/* i_frame_total:数据帧数量 */

    return i_frame_total;	/* i_frame_total */
}

/*
读取帧，yuv格式的

这里支持的yuv存储格式为:先存一帧的全部亮度值，再存一帧的全部Cb值，再存一帧的全部Cr值，下一帧也是如此；其它存储方式，这儿不支持
*/
int read_frame_yuv( x264_picture_t *p_pic, hnd_t handle, int i_frame )
{
    yuv_input_t *h = handle;

    if( i_frame != h->next_frame )
        if( fseek( h->fh, (uint64_t)i_frame * h->width * h->height * 3 / 2, SEEK_SET ) )//定位文件
            return -1;

    if( fread( p_pic->img.plane[0], 1, h->width * h->height, h->fh ) <= 0	//fread:从一个流中读数据//读Y亮度
            || fread( p_pic->img.plane[1], 1, h->width * h->height / 4, h->fh ) <= 0//读Cb
            || fread( p_pic->img.plane[2], 1, h->width * h->height / 4, h->fh ) <= 0 )//读Cr
        return -1;

    h->next_frame = i_frame+1;//记住下一帧是第几帧，存到了yuv_input_t结构的最后一个字段里

    return 0;
}

/*
	fread 　　
	功 能: 从一个流中读数据
	函数原型: size_t fread( void *buffer, size_t size, size_t count, FILE *stream );
	参 数：
	1.用于接收数据的地址（指针）（buffer）
	2.单个元素的大小（size） ：单位是字节而不是位，例如读取一个整型数就是2个字节
	3.元素个数（count）
	4.提供数据的文件指针（stream）
	返回值：成功读取的元素个数
*/




/*

关闭YUV文件
*/
int close_file_yuv(hnd_t handle)
{
    yuv_input_t *h = handle;
    if( !h || !h->fh )
        return 0;
    return fclose(h->fh);
}

/* YUV4MPEG2 raw(原始的) 420 yuv file operation */
typedef struct {
    FILE *fh;			//文件指针，用来保存一个已打开的文件的指针
    int width, height;	//宽、高
    int next_frame;		//下一帧，计数用	
    int seq_header_len, frame_header_len;	/*  */
    int frame_size;		//帧尺寸
} y4m_input_t;			/*  */

#define Y4M_MAGIC "YUV4MPEG2"
#define MAX_YUV4_HEADER 80
#define Y4M_FRAME_MAGIC "FRAME"
#define MAX_FRAME_HEADER 80


/*
 * 名称：
 * 功能：打开文件:"*.y4m"
 * 参数：文件名，typedef void *hnd_t,x264_param_t
 * 注意：
 * 资料：YUV4MPEG2这种文件格式是一种以头文件存储视频规格的未压缩视频序列。简单来说，在原始的yuv序列的起始和每一帧的头部都加入了纯文字形式的视频参数信息，包括分辨率、帧率、逐行/隔行扫描方式、高宽比(aspect ratio)，以及每一帧起始的”FRAME ”标志位。
 *		 y4m到yuv的转换 了解了y4m的封装形式后，我们的工作就变得相当机械。只要把头文件和每帧的标志位去除即可，剩下的生肉既是原封不动的yuv数据，如果是4:2:0也不需要进一步的转换修整工作。
*/
int open_file_y4m( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param )	/* typedef void *hnd_t; */
{
    int  i, n, d;
    int  interlaced;						/* 隔行扫描,交错 */
    char header[MAX_YUV4_HEADER+10];		/* #define MAX_YUV4_HEADER 80 ，比头部的长度多申请了10个字节空间*/
    char *tokstart, *tokend, *header_end;	/*  */
    y4m_input_t *h = malloc(sizeof(y4m_input_t));	/* 结构体字段包括文件指针、长和宽、下一帧、 */

	printf("\n(muxer.c) open_file_y4m(...) done\n");//zjh，当源测试序列后缀名为.y4m时显示
    h->next_frame = 0;		/*  */

    if( !strcmp(psz_filename, "-") )	/* !strcmp(psz_filename, "-") 为真，即strcmp(psz_filename, "-")必须返回0，意即psz_filename=="-" */	
	/*
	用法：#include <string.h>
	功能：比较字符串s1和s2。
	一般形式：strcmp(字符串1，字符串2)
	说明：
	当s1<s2时，返回值<0 　　当s1=s2时，返回值=0 　　当s1>s2时，返回值>0 　　即：两个字符串自左向右逐个字符相比（按ASCII值大小相比较），直到出现不同的字符或遇'\0'为止。	
	*/
        h->fh = stdin;		/* ??? */
    else
        h->fh = fopen(psz_filename, "rb");	/* 执行打开文件动作 */
    if( h->fh == NULL )	/* 打开文件失败 */
        return -1;

    h->frame_header_len = strlen(Y4M_FRAME_MAGIC)+1;	/* #define Y4M_FRAME_MAGIC "FRAME" ; MAGIC:魔法 */
														/* strlen("FRAME") + 1 */
    /* 读取头部 Read header */
    for( i=0; i<MAX_YUV4_HEADER; i++ )	/* 循环80次，读80字节 #define MAX_YUV4_HEADER 80 */
    {
        header[i] = fgetc(h->fh);		/* 每次循环读取一个字符，遇到'\n'时退出 */
		/*  
		格式：int fgetc(FILE *stream);
		意为从文件指针stream指向的文件中读取一个字符。
		这个函数的返回值，是返回读取的一个字节。如果读到文件末尾返回EOF。		
		*/
        if( header[i] == '\n' )		/* \n ASCII码里面\r就是0x0D \n 0x0A  头部结尾，这个头部是以字符串来处理的 C字符串实际上就是一个以null('\0')字符结尾的字符数组，null字符表示字符串的结束。需要注意的是：只有以null字符结尾的字符数组才是C字符串，否则只是一般的C字符数组。 */
        {
            /* Add a space after last option. Makes parsing(分[剖]析,分解) "444" vs
               "444alpha" easier(更容易的). */
            header[i+1] = 0x20;	/* 0010 0000 ?在此位置放它是什么意思 */
            header[i+2] = 0;	/* ? */
            break;
        }
    }
	/* 如果不是y4m格式，退出函数 */
    if( i == MAX_YUV4_HEADER || strncmp(header, Y4M_MAGIC, strlen(Y4M_MAGIC)) )	/* 长度是80 或者 header是"YUV4MPEG2" */
		/*
		条件判断为真，即 i == 80 或者 文件头部没有YUV4MPEG2，会执行return (因为strncmp在比较相等时返回0，当不相等时，返回非0)
		#define MAX_YUV4_HEADER 80
		#define Y4M_MAGIC "YUV4MPEG2"
		*/
        return -1;	/* 退出函数 */

    /* Scan properties(特性) */
    header_end = &header[i+1]; /* 头的末尾，就是i+1处  Include space */
    h->seq_header_len = i+1;	/* Sequence:序列,有关联的一组事物, 一连串；先后次序, 顺序, 连续 */
    for( tokstart = &header[strlen(Y4M_MAGIC)+1]; tokstart < header_end; tokstart++ )	/* 从YUV4MPEG2后面开始扫描，YUV4MPEG2在文件的最开始处 */
    {	/* 上面for的条件是内存地址 */
        if(*tokstart==0x20) continue;	
		/* 	
		 跳过0x20，因为YUV4MPEG2后紧跟着一个0x20，continue:终止本次循环，接着开始下次循环 
		 如：YUV4MPEG2 W176 H144 F15:1 Ip A128:117 FRAME,.1# $%$""$##..
		*/
        switch(*tokstart++)	/* http://wmnmtm.blog.163.com/blog/static/3824571420118424330628/ */
        {
        case 'W': /* (如W176) 这项是必须的 Width. Required. W后紧跟着就是数值*/
            h->width = p_param->i_width = strtol(tokstart, &tokend, 10);	/* 将tokstart转换成长整型,最后一个参数代表tokstart的进制类型，此处它是10进制数 */
			/*
			long int strtol(const char *nptr,char **endptr,int base);
			这个函数会将参数nptr字符串根据参数base来转换成长整型数。参数base范围从2至36，或0。参数base代表采的进制方式，如base值为10则采用10进制，若base值为16则采用16进制等。当base值为0时则是采用10进制做转换，但遇到如’0x’前置字符则会使用16进制做转换、遇到’0’前置字符而不是’0x’的时候会使用8进制做转换。一开始strtol()会扫描参数nptr字符串，跳过前面的空格字符，直到遇上数字或正负符号才开始做转换，再遇到非数字或字符串结束时('\0')结束转换，并将结果返回。若参数endptr不为NULL，则会将遇到不合条件而终止的nptr中的字符指针由endptr返回。
			strtol是atoi的增强版
			主要体现在这几方面：
			1.不仅可以识别十进制整数，还可以识别其它进制的整数，取决于base参数，比如strtol("0XDEADbeE~~", NULL, 16)返回0xdeadbee的值，strtol("0777~~", NULL, 8)返回0777的值。
		　　2.endptr是一个传出参数，函数返回时指向后面未被识别的第一个字符。例如char *pos; strtol("123abc", &pos, 10);，strtol返回123，pos指向字符串中的字母a。如果字符串开头没有可识别的整数，例如char *pos; strtol("ABCabc", &pos, 10);，则strtol返回0，pos指向字符串开头，可以据此判断这种出错的情况，而这是atoi处理不了的。
		　　3.如果字符串中的整数值超出long int的表示范围（上溢或下溢），则strtol返回它所能表示的最大（或最小）整数，并设置errno为ERANGE，例如strtol("0XDEADbeef~~", NULL, 16)返回0x7fffffff并设置errno为ERANGE
			*/
            tokstart=tokend;	/*  */
            break;
        case 'H': /* (如 H144) 这项是必须的 Height. Required. */
            h->height = p_param->i_height = strtol(tokstart, &tokend, 10);	/*  */
            tokstart=tokend;	/*  */
            break;
        case 'C': /* 色彩空间 这项不是必须的 Color space */
            if( strncmp("420", tokstart, 3) )	/*  */
            {
                fprintf(stderr, "Colorspace(色彩空间) unhandled(未经处理过的)\n");	/*  */
                return -1;
            }
            tokstart = strchr(tokstart, 0x20);	/*  */
			/*
			原型：extern char *strchr(const char *s,char c); 
			const char *strchr(const char* _Str,int _Val)
			char *strchr(char* _Str,int _Ch)
			头文件：#include <string.h>
			功能：查找字符串s中首次出现字符c的位置
			说明：返回首次出现c的位置的指针，如果s中不存在c则返回NULL。
			返回值：Returns the address of the first occurrence of the character in the string if successful, or NULL otherwise 			
			*/
            break;
        case 'I': /* (如Ip) Interlace(隔行;使交错;使交织) type */
            switch(*tokstart++)	/*  */
            {
            case 'p': interlaced = 0; break;	/* 不是隔行 */
            case '?':	/*  */
            case 't':	/*  */
            case 'b':	/*  */
            case 'm':	/*  */
            default: interlaced = 1;	/* 隔行扫描 */
                fprintf(stderr, "Warning, this sequence(序列) might(可能) be interlaced\n");	/*  */
            }
            break;
        case 'F': /* (如 F15:1)Frame rate(比率, 率；(运动、变化等的)速度; 进度) - 0:0 if unknown */
            if( sscanf(tokstart, "%d:%d", &n, &d) == 2 && n && d )	/*  */
            {
                x264_reduce_fraction( &n, &d );	/* x264_reduce_fraction:分数化简 (common.c中定义) reduce:转化，将…还原,将…概括[简化]; fraction:分数 */
                p_param->i_fps_num = n;	/* 把文件中的值存到编码器的结构体字段中去 */
                p_param->i_fps_den = d;	/*  */
            }
            tokstart = strchr(tokstart, 0x20);	/* 查找字符串s中首次出现字符c的位置 */
            break;
        case 'A': /* (如 A128:117) Pixel aspect(方面) - 0:0 if unknown */
            if( sscanf(tokstart, "%d:%d", &n, &d) == 2 && n && d )	/*  */
            {
                x264_reduce_fraction( &n, &d );	/*  */
                p_param->vui.i_sar_width = n;	/* 标准313页 句法元素sar_height表示样点高宽比的水平尺寸(以任意单位)  */
                p_param->vui.i_sar_height = d;	/* 标准313页 句法元素sar_height表示样点高宽比的垂直尺寸(以与句法元素sar_width相同的任意单位) */
            }
            tokstart = strchr(tokstart, 0x20);	/* 找到后面这个空格符0x20，记下它的地址，下次从这儿查找 */
            break;
        case 'X': /* Vendor(卖主;卖方) extensions(扩展) */
            if( !strncmp("YSCSS=",tokstart,6) )	/*  */
            {
                /* Older nonstandard pixel format representation */
                tokstart += 6;	/*  */
                if( strncmp("420JPEG",tokstart,7) &&	/*  */
                    strncmp("420MPEG2",tokstart,8) &&	/*  */
                    strncmp("420PALDV",tokstart,8) )	/*  */
                {
                    fprintf(stderr, "Unsupported extended colorspace\n");	/* 不被支持的扩展色彩空间 */
                    return -1;
                }
            }
            tokstart = strchr(tokstart, 0x20);	/* 功能：查找字符串s中首次出现字符c的位置 */
            break;
        }
    }

    fprintf(stderr, "yuv4mpeg: %ix%i@%i/%ifps, %i:%i\n",
            h->width, h->height, p_param->i_fps_num, p_param->i_fps_den,
            p_param->vui.i_sar_width, p_param->vui.i_sar_height);	/*  */

    *p_handle = (hnd_t)h;	/*  */
    return 0;
}

/* Most common(常见的,共有的) case(事例, 实例,情况): frame_header = "FRAME" 
 * 取总帧数：.y4m
 * 
 * file format:
 * seq_header
 * [frame_header +frame]
 * [frame_header +frame]
 * ... ...
 * [frame_header +frame]
*/
int get_frame_total_y4m( hnd_t handle )			/*  */
{
    y4m_input_t *h             = handle;		/* 定义结构体的指针 */	
    int          i_frame_total = 0;				/* 原始帧总数 */
    off_t        init_pos      = ftell(h->fh);	/* fh:结构体y4m_input_t的fh字段是FILE类型的指针 */

    if( !fseek( h->fh, 0, SEEK_END ) )		/* 重定位流(数据流/文件)上的文件内部位置指针;函数设置文件指针stream的位置。如果执行成功，stream将指向以fromwhere（偏移起始位置：文件头0，当前位置1，文件尾2）为基准，偏移offset（指针偏移量）个字节的位置。如果执行失败(比如offset超过文件自身大小)，则不改变stream指向的位置。 */
    {
        uint64_t i_size = ftell( h->fh );	/* 函数 ftell() 用于得到文件位置指针当前位置相对于文件首的偏移字节数。在随机方式存取文件时，由于文件位置频繁的前后移动，程序不容易确定文件的当前位置。调用函数ftell()就能非常容易地确定文件的当前位置 */
        fseek( h->fh, init_pos, SEEK_SET );	/* 重定位流(数据流/文件)上的文件内部位置指针,注意：不是定位文件指针，文件指针指向文件/流。位置指针指向文件内部的字节位置，随着文件的读取会移动，文件指针如果不重新赋值将不会改变指向别的文件 */
        i_frame_total = (int)((i_size - h->seq_header_len) /
                              (3*(h->width * h->height)/2 + h->frame_header_len)); /* 原始视频文件的总帧数 */
							/* 原始帧数 = (文件总大小 - 序列的头长度)/((宽*高)*1.5 + 帧头部长度) */
							/* 每帧前都有"FRAME" */
							/* 一个像素占1.5字节? */
    }

    return i_frame_total;	/* 原始视频文件的总帧数 */
}


/*
 * 名称：
 * 参数：
 * 注意：
 * 思路：
 * 资料：
*/
int read_frame_y4m( x264_picture_t *p_pic, hnd_t handle, int i_frame )
{
    int          slen = strlen(Y4M_FRAME_MAGIC);	/* #define Y4M_FRAME_MAGIC "FRAME" */
    int          i    = 0;
    char         header[16];
    y4m_input_t *h    = handle;

    if( i_frame != h->next_frame )
    {
        if (fseek(h->fh, (uint64_t)i_frame*(3*(h->width*h->height)/2+h->frame_header_len)
                  + h->seq_header_len, SEEK_SET))
            return -1;
    }

    /* Read frame header - without terminating(使终结) '\n' */
    if (fread(header, 1, slen, h->fh) != slen)	/*  */
        return -1;
/*
	
　　功 能: 从一个流中读数据
	函数原型: size_t fread( void *buffer, size_t size, size_t count, FILE *stream );
	参 数：
	　　1.用于接收数据的地址(指针)(buffer)
	　　2.单个元素的大小(size) ：单位是字节而不是位，例如读取一个整型数就是2个字节
	　　3.元素个数(count)
	　　4.提供数据的文件指针(stream)
	　　返回值：成功读取的元素个数
	　　功 能: 从一个流中读数据
	　　函数原型: size_t fread( void *buffer, size_t size, size_t count, FILE *stream );
		参 数：
		　　1.用于接收数据的地址(指针)(buffer)
		　　2.单个元素的大小(size) ：单位是字节而不是位，例如读取一个整型数就是2个字节
		　　3.元素个数(count)
			4.提供数据的文件指针(stream)
		返回值：成功读取的元素个数
*/    
    header[slen] = 0;
    if (strncmp(header, Y4M_FRAME_MAGIC, slen))	/* 条件为真，说明strncmp返回了非0值,与"FRAME"不相等 */
		/* "FRAME"，每帧前面都有这个 */
    {
        fprintf(stderr, "Bad header magic (%08X <=> %s)\n",
                *((uint32_t*)header), header);
        return -1;
    }
  
    /* 
	Skip most of it 

	*/
    while (i<MAX_FRAME_HEADER && fgetc(h->fh) != '\n')	/* 循环，遇'\n'中止循环 */
        i++;
    if (i == MAX_FRAME_HEADER)	/* #define MAX_FRAME_HEADER 80 */
    {
        fprintf(stderr, "Bad frame header(错误的帧头部)!\n");
        return -1;
    }
    h->frame_header_len = i+slen+1;		/* i + strlen(Y4M_FRAME_MAGIC) + 1 */

    if( fread(p_pic->img.plane[0], 1, h->width*h->height, h->fh) <= 0				/* x264_picture_t.plane[0] */
        || fread(p_pic->img.plane[1], 1, h->width * h->height / 4, h->fh) <= 0		/* x264_picture_t.plane[1] */
        || fread(p_pic->img.plane[2], 1, h->width * h->height / 4, h->fh) <= 0)		/* x264_picture_t.plane[2] */
        return -1;

    h->next_frame = i_frame+1;

    return 0;
}

/*
 * 名称：
 * 参数：
 * 功能：关闭".y4m"文件
 * 思路：
 * 资料：
*/
int close_file_y4m(hnd_t handle)
{
    y4m_input_t *h = handle;	/* 把传入的指针保存进结构体y4m_input_t的字段 */
    if( !h || !h->fh )			/* h指针为空 || y4m_input_h对象的fh字段为空 */
        return 0;
    return fclose(h->fh);		/* FILE *fh; */
	/*
	功 能: 关闭一个流。
	注意：使用fclose()函数就可以把缓冲区内最后剩余的数据输出到磁盘文件中，并释放文件指针和有关的缓冲区。
	用 法: int fclose(FILE *stream); 
	如果流成功关闭，fclose 返回 0，否则返回EOF(-1)。
	如果流为NULL，而且程序可以继续执行，fclose设定error number给EINVAL，并返回EOF
	*/
}

/* avs/avi input file support(支持) under cygwin(安装) */

#ifdef AVIS_INPUT
typedef struct {
    PAVISTREAM p_avi;	/* P AVI STREAM */
    int width, height;	/* 宽,高 */
} avis_input_t;

/* 
yuv_input_t 
avis_input_t
*/

/*
 * 名称：
 * 参数：
 * 功能：求最大公约数
 * 思路：
 * 资料：
*/
int gcd(int a, int b)
{
    int c;

    while (1)
    {
        c = a % b;
        if (!c)
            return b;
        a = b;
        b = c;
    }
}

/*
 * 名称：
 * 参数：
 * 功能：打开.avi/.avs文件
 * 思路：
 * 资料：typedef void *hnd_t
 *		 http://wmnmtm.blog.163.com/blog/static/3824571420118494618377/
 *		 为了对avi进行读写，微软提供了一套API，总共50个函数，他们的用途主要有两类，一个是avi文件的操作，一类是数据流streams的操作。
 *		1、打开和关闭文件
		AVIFileOpen ，AVIFileAddRef， AVIFileRelease

		2、从文件中读取文件信息
		通过AVIFileInfo可以获取avi文件的一些信息，这个函数返回一个AVIFILEINFO结构，通过AVIFileReadData可以用来 获取AVIFileInfo函数得不到的信息。这些信息也许不包含在文件的头部，比如拥有file的公司和个人的名称。

		3、写入文件信息
		可以通过AVIFileWriteData函数来写入文件的一些额外信息。

	　　4、打开和关闭一个流

	　　打开一个数据流就跟打开文件一样，你可以通过 AVIFileGetStream函数来打开一个数据流，这个函数创建了一个流的接口，然后在该接口中保存了一个句柄。

	　　如果你想操作文件的某一个单独的流，你可以采用AVIStreamOpenFromFile函数，这个函数综合了AVIFileOpen和AVIFileGetStream函数。

	　　如果你想操作文件中的多个数据流，你就要首先AVIFileOpen，然后AVIFileGetStream。

	　　可以通过AVIStreamAddRef来增加stream接口的引用。

	　　通过AVIStreamRelease函数来关闭数据流。这个函数用来减少streams的引用计数，当计数减少为0时，删除。

	　　5、从流中读取数据和信息

	　　AVIStreamInfo函数可以获取数据的一些信息，该函数返回一个AVISTREAMINFO结构，该结构包含了数据的类型压缩方法，建议的buffersize，回放的rate，以及一些description。

	　　如果数据流还有一些其它的额外的信息，你可以通过AVIStreamReadData函数来获取。应用程序分配一个内存，传递给这个函数，然后这个函数会通过这个内存返回数据流的信息，额外的信息可能包括数据流的压缩和解压缩的方法，你可以通过AVIStreamDataSize宏来回去需要申请内存块的大小。

		可以通过AVIStreamReadFormat函数获取数据流的格式信息。这个函数通过指定的内存返回数据流的格式信息，比如对于视频流，这个 buffer包含了一个BIMAPINFO结构，对于音频流，内存块包含了WAVEFORMATEX或者PCMAVEFORMAT结构。你可以通过给 AVIStreamReadFormat传递一个空buffer就可以获取buffer的大小。也可以通过AVIStreamFormatSize宏。

		可以通过AVIStreamRead函数来返回多媒体的数据。这个函数将数据复制到应用程序提供的内存中，对于视频流，这个函数返回图像祯，对于音频 流，这个函数返回音频的sample数据。可以通过给AVIStreamRead传递一个NULL的buffer来获取需要的buffer的大小。也可以 通过AVIStreamSampleSize宏来获取buffer的大小。

		有些AVI数据流句柄可能需要在启动数据流的前要做一下准 备工作，此时，我们可以调用AVIStreamBeginStreaming函数来告知AVI数据流handle来申请分配它需要的一些资源。在完毕后， 调用AVIStreamEndStreamming函数来释放资源。

	　　6、操作压缩的视频数据

		7、根据已存在的数据流创建文件

		8、向文件写入一个数据流

		9、数据流中的祯的位置

*/
int open_file_avis( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param )
{
    avis_input_t *h = malloc(sizeof(avis_input_t));		/* 动态内存申请，用来存放avis_input_t */
    AVISTREAMINFO info;		/*  */
    int i;

    *p_handle = (hnd_t)h;

    AVIFileInit();		/* 初始化AVI库 */
    if( AVIStreamOpenFromFile( &h->p_avi, psz_filename, streamtypeVIDEO, 0, OF_READ, NULL ) )	/* 如果你想操作文件的某一个单独的流，你可以采用AVIStreamOpenFromFile函数，这个函数综合了AVIFileOpen和AVIFileGetStream函数 */
    {
        AVIFileExit();	/* 释放AVI库 */
        return -1;
    }

    if( AVIStreamInfo(h->p_avi, &info, sizeof(AVISTREAMINFO)) )
    {
        AVIStreamRelease(h->p_avi);		
		/* 关闭数据流 通过AVIStreamRelease函数来关闭数据流。这个函数用来减少streams的引用计数，当计数减少为0时，删除 
		可以通过AVIStreamAddRef来增加stream接口的引用
		*/
        AVIFileExit();	/* 释放AVI库 */
        return -1;
    }

    // check input format
    if (info.fccHandler != MAKEFOURCC('Y', 'V', '1', '2'))
    {	/* AVISTREAMINFO.fccHandler */
        fprintf( stderr, "avis [error]: unsupported input format (%c%c%c%c)\n",
            (char)(info.fccHandler & 0xff), (char)((info.fccHandler >> 8) & 0xff),
            (char)((info.fccHandler >> 16) & 0xff), (char)((info.fccHandler >> 24)) );

        AVIStreamRelease(h->p_avi);
        AVIFileExit();

        return -1;
    }

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
    h->width  = p_param->i_width = info.rcFrame.right - info.rcFrame.left;	/* RECT rcFrame */
    h->height = p_param->i_height = info.rcFrame.bottom - info.rcFrame.top;	
	/* avis_input_t *h */
	/* x264_param_t *p_param */
    i = gcd(info.dwRate, info.dwScale);		/* 求最大公约数 */
    p_param->i_fps_den = info.dwScale / i;	/* AVISTREAMINFO.dwScale */
    p_param->i_fps_num = info.dwRate / i;	/* AVISTREAMINFO.dwRate */

    fprintf( stderr, "avis [info]: %dx%d @ %.2f fps (%d frames)\n",	/*  */
        p_param->i_width, p_param->i_height,						/*  */
        (double)p_param->i_fps_num / (double)p_param->i_fps_den,	/*  */
        (int)info.dwLength );

    return 0;
}

/*
 * 名称：取帧的总数 .avi/avs
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int get_frame_total_avis( hnd_t handle )
{
    avis_input_t *h = handle;
    AVISTREAMINFO info;

    if( AVIStreamInfo(h->p_avi, &info, sizeof(AVISTREAMINFO)) )
        return -1;

    return info.dwLength;
}

/*
 * 名称：
 * 参数：
 * 功能：读取帧 .avi/.avs
 * 思路：
 * 资料：
*/
int read_frame_avis( x264_picture_t *p_pic, hnd_t handle, int i_frame )
{
    avis_input_t *h = handle;

    p_pic->img.i_csp = X264_CSP_YV12;

    if( AVIStreamRead(h->p_avi, i_frame, 1, p_pic->img.plane[0], h->width * h->height * 3 / 2, NULL, NULL ) )
        return -1;

    return 0;
}

/*
 * 名称：
 * 参数：
 * 功能：关闭文件 .avi/.avs
 * 思路：
 * 资料：
*/
int close_file_avis( hnd_t handle )
{
    avis_input_t *h = handle;
    AVIStreamRelease(h->p_avi);
    AVIFileExit();
    free(h);
    return 0;
}
#endif


#ifdef HAVE_PTHREAD
typedef struct {
    int (*p_read_frame)( x264_picture_t *p_pic, hnd_t handle, int i_frame );
    int (*p_close_infile)( hnd_t handle );
    hnd_t p_handle;
    x264_picture_t pic;
    pthread_t tid;			/* typedef unsigned long int pthread_t; */
    int next_frame;
    int frame_total;
    struct thread_input_arg_t *next_args;
} thread_input_t;

typedef struct thread_input_arg_t {
    thread_input_t *h;
    x264_picture_t *pic;
    int i_frame;
    int status;
} thread_input_arg_t;

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int open_file_thread( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param )
{
    thread_input_t *h = malloc(sizeof(thread_input_t));
    x264_picture_alloc( &h->pic, X264_CSP_I420, p_param->i_width, p_param->i_height );
    h->p_read_frame = p_read_frame;
    h->p_close_infile = p_close_infile;
    h->p_handle = *p_handle;
    h->next_frame = -1;
    h->next_args = malloc(sizeof(thread_input_arg_t));
    h->next_args->h = h;
    h->next_args->status = 0;
    h->frame_total = p_get_frame_total( h->p_handle );

    *p_handle = (hnd_t)h;
    return 0;
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int get_frame_total_thread( hnd_t handle )
{
    thread_input_t *h = handle;
    return h->frame_total;
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
void read_frame_thread_int( thread_input_arg_t *i )
{
    i->status = i->h->p_read_frame( i->pic, i->h->p_handle, i->i_frame );
}	/* status:状况 */

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int read_frame_thread( x264_picture_t *p_pic, hnd_t handle, int i_frame )
{
    thread_input_t *h = handle;
    UNUSED void *stuff;
    int ret = 0;

    if( h->next_frame >= 0 )
    {
        pthread_join( h->tid, &stuff );
        ret |= h->next_args->status;
    }

    if( h->next_frame == i_frame )
    {
        XCHG( x264_picture_t, *p_pic, h->pic );
    }
    else
    {
        ret |= h->p_read_frame( p_pic, h->p_handle, i_frame );
    }

    if( !h->frame_total || i_frame+1 < h->frame_total )
    {
        h->next_frame =
        h->next_args->i_frame = i_frame+1;
        h->next_args->pic = &h->pic;
        pthread_create( &h->tid, NULL, (void*)read_frame_thread_int, h->next_args );
    }
    else
        h->next_frame = -1;

    return ret;
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int close_file_thread( hnd_t handle )
{
    thread_input_t *h = handle;
    h->p_close_infile( h->p_handle );
    x264_picture_clean( &h->pic );
    free( h );
    return 0;
}
#endif

/*
功能：打开文件
方式：w+ 
说明：加入b 字符用来告诉函数库打开的文件为二进制文件，而非纯文字文件
注意：打开可读写文件，若文件存在则文件长度清为零，即该文件内容会消失。若文件不存在则建立该文件w+ 打开可读写文件，若文件存在则文件长度清为零，即该文件内容会消失。若文件不存在则建立该文件
*/
int open_file_bsf( char *psz_filename, hnd_t *p_handle )
{
    if ((*p_handle = fopen(psz_filename, "w+b")) == NULL)//+ 打开可读写文件，若文件存在则文件长度清为零，即该文件内容会消失。若文件不存在则建立该文件。上述的形态字符串都可以再加一个b字符，如rb、w+b或ab＋等组合，加入b 字符用来告诉函数库打开的文件为二进制文件，而非纯文字文件。
        return -1;

	printf("\n (muxers.c) open_file_bsf \n");//zjh 这句的执行：输出文件后缀名为.264 .yuv .avc .mp3 .mp6 .mkv 
    return 0;								//输出文件后缀为.mp4时会出错 提示：not compiled with MP4 output support 不能编译MP4 http://wmnmtm.blog.163.com/blog/static/38245714201181824344687/
}

/*
 * 名称：
 * 参数：设置_参数_bsf
 * 功能：
 * 思路：
 * 资料：
*/
int set_param_bsf( hnd_t handle, x264_param_t *p_param )
{
    return 0;
}

/*
 * 名称：
 * 参数：hnd_t handle，第一个参数实际是个句柄
 * 功能：
 * 思路：
 * 资料：
*/

int write_nalu_bsf( hnd_t handle, uint8_t *p_nalu, int i_size )
{
   if (fwrite(p_nalu, i_size, 1, (FILE *)handle) > 0)
	{
		//system("pause");//暂停，任意键继续
		//printf("muxers.c写文件：fwrite i_size=%d\n",i_size);
		//exit(0);
	//	if (i_size > 5) 
	//		exit(0);
		return i_size;
	}
    return -1;
}

/*
size_t fwrite(const void*buffer,size_t size,size_t count,FILE*stream); 　　
注意：这个函数以二进制形式对文件进行操作，不局限于文本文件 　　
返回值：返回实际写入的数据块数目
(1) buffer：是一个指针，对fwrite来说，是要输出数据的地址。
(2) size：要写入内容的单字节数；
(3) count:要进行写入size字节的数据项的个数；
(4) stream:目标文件指针。
(5) 成功返回 1 ，否则返回 0
说明：写入到文件的哪里？ 这个与文件的打开模式有关，如果是w+，则是从file pointer指向的地址开始写，替换掉之后的内容，文件的长度可以不变，stream的位置移动count个数。如果是a+，则从文件的末尾开始添加，文件长度加大，而且是fseek函数对此函数没有作用。
*/


/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int set_eop_bsf( hnd_t handle,  x264_picture_t *p_picture )
{
    return 0;
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int close_file_bsf( hnd_t handle )
{
    if ((handle == NULL) || (handle == stdout))
        return 0;

    return fclose((FILE *)handle);	/* 关闭一个流 */
}
/* 
函数名: fclose()
功能: 关闭一个流
注意：使用fclose()函数就可以把缓冲区内最后剩余的数据输出到磁盘文件中，并释放文件指针和有关的缓冲区。
*/

/* -- mp4 muxing support ------------------------------------------------- */
#ifdef MP4_OUTPUT

typedef struct
{
    GF_ISOFile *p_file;
    GF_AVCConfig *p_config;
    GF_ISOSample *p_sample;
    int i_track;
    uint32_t i_descidx;
    int i_time_inc;
    int i_time_res;
    int i_numframe;
    int i_init_delay;
    uint8_t b_sps;
    uint8_t b_pps;
} mp4_t;

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
void recompute_bitrate_mp4(GF_ISOFile *p_file, int i_track)		/* recompute:再计算,验算 */
{
    u32 i, count, di, timescale, time_wnd, rate;
    u64 offset;
    Double br;
    GF_ESD *esd;

    esd = gf_isom_get_esd(p_file, i_track, 1);
    if (!esd) return;

    esd->decoderConfig->avgBitrate = 0;
    esd->decoderConfig->maxBitrate = 0;
    rate = time_wnd = 0;

    timescale = gf_isom_get_media_timescale(p_file, i_track);
    count = gf_isom_get_sample_count(p_file, i_track);
    for (i=0; i<count; i++) {
        GF_ISOSample *samp = gf_isom_get_sample_info(p_file, i_track, i+1, &di, &offset);

        if (samp->dataLength>esd->decoderConfig->bufferSizeDB) esd->decoderConfig->bufferSizeDB = samp->dataLength;

        if (esd->decoderConfig->bufferSizeDB < samp->dataLength) esd->decoderConfig->bufferSizeDB = samp->dataLength;
        esd->decoderConfig->avgBitrate += samp->dataLength;
        rate += samp->dataLength;
        if (samp->DTS > time_wnd + timescale) {
            if (rate > esd->decoderConfig->maxBitrate) esd->decoderConfig->maxBitrate = rate;
            time_wnd = samp->DTS;
            rate = 0;
        }

        gf_isom_sample_del(&samp);
    }

    br = (Double) (s64) gf_isom_get_media_duration(p_file, i_track);
    br /= timescale;
    esd->decoderConfig->avgBitrate = (u32) (esd->decoderConfig->avgBitrate / br);
    /*move to bps*/
    esd->decoderConfig->avgBitrate *= 8;
    esd->decoderConfig->maxBitrate *= 8;

    gf_isom_change_mpeg4_description(p_file, i_track, 1, esd);
    gf_odf_desc_del((GF_Descriptor *) esd);			/*  */
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int close_file_mp4( hnd_t handle )
{
    mp4_t *p_mp4 = (mp4_t *)handle;

    if (p_mp4 == NULL)
        return 0;

    if (p_mp4->p_config)
        gf_odf_avc_cfg_del(p_mp4->p_config);

    if (p_mp4->p_sample)
    {
        if (p_mp4->p_sample->data)
            free(p_mp4->p_sample->data);

        gf_isom_sample_del(&p_mp4->p_sample);
    }

    if (p_mp4->p_file)
    {
        recompute_bitrate_mp4(p_mp4->p_file, p_mp4->i_track);
        gf_isom_set_pl_indication(p_mp4->p_file, GF_ISOM_PL_VISUAL, 0x15);
        gf_isom_set_storage_mode(p_mp4->p_file, GF_ISOM_STORE_FLAT);
        gf_isom_close(p_mp4->p_file);
    }

    free(p_mp4);

    return 0;
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int open_file_mp4( char *psz_filename, hnd_t *p_handle )
{
    mp4_t *p_mp4;

    *p_handle = NULL;

    if ((p_mp4 = (mp4_t *)malloc(sizeof(mp4_t))) == NULL)
        return -1;

    memset(p_mp4, 0, sizeof(mp4_t));
    p_mp4->p_file = gf_isom_open(psz_filename, GF_ISOM_OPEN_WRITE, NULL);

    if ((p_mp4->p_sample = gf_isom_sample_new()) == NULL)
    {
        close_file_mp4( p_mp4 );
        return -1;
    }

    gf_isom_set_brand_info(p_mp4->p_file, GF_ISOM_BRAND_AVC1, 0);

    *p_handle = p_mp4;

    return 0;
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int set_param_mp4( hnd_t handle, x264_param_t *p_param )
{
    mp4_t *p_mp4 = (mp4_t *)handle;

    p_mp4->i_track = gf_isom_new_track(p_mp4->p_file, 0, GF_ISOM_MEDIA_VISUAL,
        p_param->i_fps_num);

    p_mp4->p_config = gf_odf_avc_cfg_new();
    gf_isom_avc_config_new(p_mp4->p_file, p_mp4->i_track, p_mp4->p_config,
        NULL, NULL, &p_mp4->i_descidx);

    gf_isom_set_track_enabled(p_mp4->p_file, p_mp4->i_track, 1);

    gf_isom_set_visual_info(p_mp4->p_file, p_mp4->i_track, p_mp4->i_descidx,
        p_param->i_width, p_param->i_height);

    p_mp4->p_sample->data = (char *)malloc(p_param->i_width * p_param->i_height * 3 / 2);
    if (p_mp4->p_sample->data == NULL)
        return -1;

    p_mp4->i_time_res = p_param->i_fps_num;
    p_mp4->i_time_inc = p_param->i_fps_den;
    p_mp4->i_init_delay = p_param->i_bframe ? (p_param->b_bframe_pyramid ? 2 : 1) : 0;
    p_mp4->i_init_delay *= p_mp4->i_time_inc;
    fprintf(stderr, "mp4 [info]: initial delay %d (scale %d)\n",
        p_mp4->i_init_delay, p_mp4->i_time_res);

    return 0;
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int write_nalu_mp4( hnd_t handle, uint8_t *p_nalu, int i_size )
{
    mp4_t *p_mp4 = (mp4_t *)handle;
    GF_AVCConfigSlot *p_slot;
    uint8_t type = p_nalu[4] & 0x1f;
    int psize;

    switch(type)
    {
    // sps
    case 0x07:
        if (!p_mp4->b_sps)
        {
            p_mp4->p_config->configurationVersion = 1;
            p_mp4->p_config->AVCProfileIndication = p_nalu[5];
            p_mp4->p_config->profile_compatibility = p_nalu[6];
            p_mp4->p_config->AVCLevelIndication = p_nalu[7];
            p_slot = (GF_AVCConfigSlot *)malloc(sizeof(GF_AVCConfigSlot));
            p_slot->size = i_size - 4;
            p_slot->data = (char *)malloc(p_slot->size);
            memcpy(p_slot->data, p_nalu + 4, i_size - 4);
            gf_list_add(p_mp4->p_config->sequenceParameterSets, p_slot);
            p_slot = NULL;
            p_mp4->b_sps = 1;
        }
        break;

    // pps
    case 0x08:
        if (!p_mp4->b_pps)
        {
            p_slot = (GF_AVCConfigSlot *)malloc(sizeof(GF_AVCConfigSlot));
            p_slot->size = i_size - 4;
            p_slot->data = (char *)malloc(p_slot->size);
            memcpy(p_slot->data, p_nalu + 4, i_size - 4);
            gf_list_add(p_mp4->p_config->pictureParameterSets, p_slot);
            p_slot = NULL;
            p_mp4->b_pps = 1;
            if (p_mp4->b_sps)
                gf_isom_avc_config_update(p_mp4->p_file, p_mp4->i_track, 1, p_mp4->p_config);
        }
        break;

    // slice, sei
    case 0x1:
    case 0x5:
    case 0x6:
        psize = i_size - 4 ;
        memcpy(p_mp4->p_sample->data + p_mp4->p_sample->dataLength, p_nalu, i_size);
        p_mp4->p_sample->data[p_mp4->p_sample->dataLength + 0] = (psize >> 24) & 0xff;
        p_mp4->p_sample->data[p_mp4->p_sample->dataLength + 1] = (psize >> 16) & 0xff;
        p_mp4->p_sample->data[p_mp4->p_sample->dataLength + 2] = (psize >> 8) & 0xff;
        p_mp4->p_sample->data[p_mp4->p_sample->dataLength + 3] = (psize >> 0) & 0xff;
        p_mp4->p_sample->dataLength += i_size;
        break;
    }

    return i_size;
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int set_eop_mp4( hnd_t handle, x264_picture_t *p_picture )
{
    mp4_t *p_mp4 = (mp4_t *)handle;
    uint64_t dts = (uint64_t)p_mp4->i_numframe * p_mp4->i_time_inc;
    uint64_t pts = (uint64_t)p_picture->i_pts;
    int32_t offset = p_mp4->i_init_delay + pts - dts;

    p_mp4->p_sample->IsRAP = p_picture->i_type == X264_TYPE_IDR ? 1 : 0;
    p_mp4->p_sample->DTS = dts;
    p_mp4->p_sample->CTS_Offset = offset;
    gf_isom_add_sample(p_mp4->p_file, p_mp4->i_track, p_mp4->i_descidx, p_mp4->p_sample);

    p_mp4->p_sample->dataLength = 0;
    p_mp4->i_numframe++;

    return 0;
}

#endif


/* -- mkv muxing support ------------------------------------------------- */
typedef struct
{
    mk_Writer *w;

    uint8_t   *sps, *pps;
    int       sps_len, pps_len;//序列参数集和图像参数集的长度

    int       width, height, d_width, d_height;

    int64_t   frame_duration;//uration:持续的时间
    int       fps_num;

    int       b_header_written;
    char      b_writing_frame;
} mkv_t;


/*
 * 名称：写头部
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int write_header_mkv( mkv_t *p_mkv )
{
    int       ret;
    uint8_t   *avcC;
    int  avcC_len;

    if( p_mkv->sps == NULL || p_mkv->pps == NULL ||
        p_mkv->width == 0 || p_mkv->height == 0 ||
        p_mkv->d_width == 0 || p_mkv->d_height == 0)
        return -1;

    avcC_len = 5 + 1 + 2 + p_mkv->sps_len + 1 + 2 + p_mkv->pps_len;
    avcC = malloc(avcC_len);
    if (avcC == NULL)
        return -1;

    avcC[0] = 1;
    avcC[1] = p_mkv->sps[1];
    avcC[2] = p_mkv->sps[2];
    avcC[3] = p_mkv->sps[3];
    avcC[4] = 0xff; // nalu size length is four bytes
    avcC[5] = 0xe1; // one sps

    avcC[6] = p_mkv->sps_len >> 8;
    avcC[7] = p_mkv->sps_len;

    memcpy(avcC+8, p_mkv->sps, p_mkv->sps_len);

    avcC[8+p_mkv->sps_len] = 1; // one pps
    avcC[9+p_mkv->sps_len] = p_mkv->pps_len >> 8;
    avcC[10+p_mkv->sps_len] = p_mkv->pps_len;

    memcpy( avcC+11+p_mkv->sps_len, p_mkv->pps, p_mkv->pps_len );

    ret = mk_writeHeader( p_mkv->w, "x264", "V_MPEG4/ISO/AVC",
                          avcC, avcC_len, p_mkv->frame_duration, 50000,
                          p_mkv->width, p_mkv->height,
                          p_mkv->d_width, p_mkv->d_height );

    free( avcC );

    p_mkv->b_header_written = 1;

    return ret;
}

/*
 * 名称：写头部
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int open_file_mkv( char *psz_filename, hnd_t *p_handle )
{
    mkv_t *p_mkv;

    *p_handle = NULL;
	printf("\n(muxer.c) open_file_mkv(...) done");//zjh 输出文件为.mkv时 命令参数如--crf 22 -o test.mkv hall_cif.yuv 352x288

    p_mkv  = malloc(sizeof(*p_mkv));//动态内存申请，用于结构体mkv_t
    if (p_mkv == NULL)//内存申请失败退出
        return -1;

    memset(p_mkv, 0, sizeof(*p_mkv));//初始化为00000...

    p_mkv->w = mk_createWriter(psz_filename);//它内部有一句w->fp = fopen(filename, "wb");，实际完成了文件以写入方式打开的动作
    if (p_mkv->w == NULL)
    {
        free(p_mkv);//失败的话，才释放动态申请的内存
        return -1;
    }

    *p_handle = p_mkv;//把mkv_t结构体对象的指针传出去，供其它函数用

    return 0;
}


/*
 * 名称：
 * 参数：
 * 功能：设置mkv参数，针对的是x264_param_t
 * 思路：
 * 资料：
*/
int set_param_mkv( hnd_t handle, x264_param_t *p_param )
{
    mkv_t   *p_mkv = handle;
    int64_t dw, dh;

    if( p_param->i_fps_num > 0 )
    {
        p_mkv->frame_duration = (int64_t)p_param->i_fps_den *
                                (int64_t)1000000000 / p_param->i_fps_num;
        p_mkv->fps_num = p_param->i_fps_num;
    }
    else
    {
        p_mkv->frame_duration = 0;
        p_mkv->fps_num = 1;
    }

    p_mkv->width = p_param->i_width;
    p_mkv->height = p_param->i_height;

    if( p_param->vui.i_sar_width && p_param->vui.i_sar_height )
    {
        dw = (int64_t)p_param->i_width  * p_param->vui.i_sar_width;
        dh = (int64_t)p_param->i_height * p_param->vui.i_sar_height;
    }
    else
    {
        dw = p_param->i_width;
        dh = p_param->i_height;
    }

    if( dw > 0 && dh > 0 )
    {
        int64_t a = dw, b = dh;

        for (;;)
        {
            int64_t c = a % b;
            if( c == 0 )
              break;
            a = b;
            b = c;
        }

        dw /= b;
        dh /= b;
    }

    p_mkv->d_width = (int)dw;
    p_mkv->d_height = (int)dh;

    return 0;
}


/*
 * 名称：
 * 参数：
 * 功能：写NAL单元，把Nal写入到一个文件
 * 思路：
 * 资料：
*/
int write_nalu_mkv( hnd_t handle, uint8_t *p_nalu, int i_size )
{
    mkv_t *p_mkv = handle;
    uint8_t type = p_nalu[4] & 0x1f;//typedef unsigned char   uint8_t;
    uint8_t dsize[4];
    int psize;

    switch( type )
    {
    // sps//SPS(序列参数集)
    case 0x07:		//[毕厚杰：Page159 nal_uint_type语义表] nal_uint_type=7，是序列参数集
        if( !p_mkv->sps )
        {
            p_mkv->sps = malloc(i_size - 4);
            if (p_mkv->sps == NULL)
                return -1;
            p_mkv->sps_len = i_size - 4;
            memcpy(p_mkv->sps, p_nalu + 4, i_size - 4);
        }
        break;

    // pps//PPS(图像参数集)
    case 0x08:		/* [毕厚杰：Page159 nal_uint_type语义表] nal_uint_type=8，是图像参数集 */
        if( !p_mkv->pps )
        {
            p_mkv->pps = malloc(i_size - 4);
            if (p_mkv->pps == NULL)
                return -1;
            p_mkv->pps_len = i_size - 4;
            memcpy(p_mkv->pps, p_nalu + 4, i_size - 4);
        }
        break;

    // slice, sei
    case 0x1:	//[毕厚杰：Page159 nal_uint_type语义表] nal_uint_type=1，是不分区，非IDR图像的片
    case 0x5:	//[毕厚杰：Page159 nal_uint_type语义表] nal_uint_type=5，是IDR图像中的片
    case 0x6:	//[毕厚杰：Page159 nal_uint_type语义表] nal_uint_type=5，是补充增强信息单元(SEI)
        if( !p_mkv->b_writing_frame )
        {
            if( mk_startFrame(p_mkv->w) < 0 )
                return -1;
            p_mkv->b_writing_frame = 1;
        }
        psize = i_size - 4 ;
        dsize[0] = psize >> 24;
        dsize[1] = psize >> 16;
        dsize[2] = psize >> 8;
        dsize[3] = psize;
        if( mk_addFrameData(p_mkv->w, dsize, 4) < 0 ||
            mk_addFrameData(p_mkv->w, p_nalu + 4, i_size - 4) < 0 )
            return -1;
        break;

    default:
        break;
    }

    if( !p_mkv->b_header_written && p_mkv->pps && p_mkv->sps &&
        write_header_mkv(p_mkv) < 0 )
        return -1;

    return i_size;
}

/*
 * 名称：
 * 参数：
 * 功能：
 * 思路：
 * 资料：
*/
int set_eop_mkv( hnd_t handle, x264_picture_t *p_picture )
{
    mkv_t *p_mkv = handle;		/*  */
    int64_t i_stamp = (int64_t)(p_picture->i_pts * 1e9 / p_mkv->fps_num);	/*  */

    p_mkv->b_writing_frame = 0;	/*  */

    return mk_setFrameFlags( p_mkv->w, i_stamp,
                             p_picture->i_type == X264_TYPE_IDR );	/*  */
}


/*
 * 名称：
 * 参数：
 * 功能：关闭mkv文件
 * 思路：
 * 资料：
*/
int close_file_mkv( hnd_t handle )
{
    mkv_t *p_mkv = handle;
    int   ret;

    if( p_mkv->sps )
        free( p_mkv->sps );
    if( p_mkv->pps )
        free( p_mkv->pps );

    ret = mk_close(p_mkv->w);

    free( p_mkv );

    return ret;
}

/*
typedef struct {

    DWORD fccType;    

    DWORD fccHandler; 

    DWORD dwFlags; 

    DWORD dwCaps; 

    WORD  wPriority; 

    WORD  wLanguage; 

    DWORD dwScale; 

    DWORD dwRate; 

    DWORD dwStart; 

    DWORD dwLength; 

    DWORD dwInitialFrames; 

    DWORD dwSuggestedBufferSize; 

    DWORD dwQuality; 

    DWORD dwSampleSize; 

    RECT  rcFrame; 

    DWORD dwEditCount; 

    DWORD dwFormatChangeCount; 

    TCHAR szName[64]; 

} AVISTREAMINFO; 



*/

int tst()
{
return 0;
}