/*****************************************************************************
 * frame.c: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: frame.c,v 1.1 2004/06/03 19:27:06 fenrir Exp $
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
 为了支持H264复杂的帧存机制，X264以专门的一个模块frame.c进行处理
 common/frame.c中包括一组帧缓冲操作函数。包括对帧进行FILO(FIFO 先进先出)和FIFO存取，空闲帧队列的相应操作等
 *****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "common.h"

/*
 * 功能：进行x264_frame_t结构体中数据元素的空间分配
 * 参数：x264_t的指针
 * 返回：结构体的指针
 * 资料：http://wmnmtm.blog.163.com/blog/static/3824571420118111141234/
 * 思路：从参数h里读了几个值，好像没往h回写什么
*/
x264_frame_t *x264_frame_new( x264_t *h )
{
    x264_frame_t *frame = x264_malloc( sizeof(x264_frame_t) );	/* 申请一块内存，用指针frame来引用此内存块 */
    int i, j;

    int i_mb_count = h->mb.i_mb_count;	/* 从结构x264_t中取出i_mb_count (在一帧中的宏块中的序号) */
    int i_stride;//stride:步伐
    int i_lines;

    if( !frame ) return NULL;	/* 内存块分配失败 */

    memset( frame, 0, sizeof(x264_frame_t) );	/* 内存块所有位设为0 */

    /* allocate(分配) frame data (+64 for extra(额外的) data for me) */
    i_stride = ( ( h->param.i_width  + 15 )&0xfffff0 )+ 64;	/* i_width:视频属性:宽度 */
    i_lines  = ( ( h->param.i_height + 15 )&0xfffff0 );
				/* 0xfffff0 : 1111 1111  1111 1111  1111 0000 */
    frame->i_plane = 3;/* plane:平面 */
    for( i = 0; i < 3; i++ )	//循环三次
    {
        int i_divh = 1;
        int i_divw = 1;
        if( i > 0 )
        {
            if( h->param.i_csp == X264_CSP_I420 )//csp:color space 颜色空间
                i_divh = i_divw = 2;	/* div高=div宽 DIV在编程中又叫做整除，即只得商的整数*/
            else if( h->param.i_csp == X264_CSP_I422 )
                i_divw = 2;			/* 
									对照取样网格，这点看的更明显，
									422是水平方向二取一
									420是水平和垂直方向都是二取一
									照此理看，这些i_divh,i_divw将被作为除数来用


									  YUV 4:2:2 样例位置
									  ¤ X ¤ X

									  ¤ X ¤ X

									  ¤ X ¤ X

									  ¤ X ¤ X


									  YUV 4:2:0 样例位置

									  X X X X
									   O   O
									  X X X X
									   O   O
									  X X X X
									   O   O
									  X X X X
									(MPEG-1 方案)
									  或

									  X X X X 
									  O   O  
									  X X X X
  
									  X X X X
									  O   O  
									  X X X X
									(MPEG-2 方案)
									 */
        }
        frame->i_stride[i] = i_stride / i_divw;		//看看，这儿用作除数了
        frame->i_lines[i] = i_lines / i_divh;		//看看，也是用作除数 行数/i_divh，这个结果应该是色度取样后的行数，循环了3次，那么yuv都有了
        CHECKED_MALLOC( frame->buffer[i],
                        frame->i_stride[i] * ( frame->i_lines[i] + 64 / i_divh ) );//实际是调用x264_malloc( size )//最终调用malloc( i_size )或者memalign( 16, i_size )
        frame->plane[i] = ((uint8_t*)frame->buffer[i]) +
                          frame->i_stride[i] * 32 / i_divh + 32 / i_divw;
		/*
		frame->plane[0]
		frame->plane[1]
		frame->plane[2]
		*/
    }
    frame->i_stride[3] = 0;
    frame->i_lines[3] = 0;
    frame->buffer[3] = NULL;
    frame->plane[3] = NULL;

    frame->filtered[0] = frame->plane[0];
    for( i = 0; i < 3; i++ )
    {
        CHECKED_MALLOC( frame->buffer[4+i],
                        frame->i_stride[0] * ( frame->i_lines[0] + 64 ) );
        frame->filtered[i+1] = ((uint8_t*)frame->buffer[4+i]) +
                                frame->i_stride[0] * 32 + 32;
    }

    if( h->frames.b_have_lowres )
    {
        frame->i_stride_lowres = frame->i_stride[0]/2 + 32;
        frame->i_lines_lowres = frame->i_lines[0]/2;
        for( i = 0; i < 4; i++ )
        {
            CHECKED_MALLOC( frame->buffer[7+i],
                            frame->i_stride_lowres * ( frame->i_lines[0]/2 + 64 ) );
            frame->lowres[i] = ((uint8_t*)frame->buffer[7+i]) +
                                frame->i_stride_lowres * 32 + 32;
        }
    }

    if( h->param.analyse.i_me_method == X264_ME_ESA )
    {
        CHECKED_MALLOC( frame->buffer[11],
                        frame->i_stride[0] * (frame->i_lines[0] + 64) * sizeof(uint16_t) );
        frame->integral = (uint16_t*)frame->buffer[11] + frame->i_stride[0] * 32 + 32;
    }
	
	/* 对分配的内存块各字段赋值，通过返回的指针来操作内存块 */
    frame->i_poc = -1;
    frame->i_type = X264_TYPE_AUTO;
    frame->i_qpplus1 = 0;	//I_qpplus1 ：此参数减1代表当前画面的量化参数值
    frame->i_pts = -1;		//I_pts ：program time stamp 程序时间戳，指示这幅画面编码的时间戳。
    frame->i_frame = -1;
    frame->i_frame_num = -1;	//[毕厚杰：Page 164] 相当于 frame_num ; 只有当这个图像是参考帧时，它所携带的这个句法元素在解码时才有意义

    CHECKED_MALLOC( frame->mb_type, i_mb_count * sizeof(int8_t));	//为每个MB的类型分配空间 分配一块内存，第一个参数(int8_t  *mb_type;)是返回的地址，第二个参数是长度
    CHECKED_MALLOC( frame->mv[0], 2*16 * i_mb_count * sizeof(int16_t) );	/* 为每个MB分配16个MV空间，满足使用4x4宏块进行帧间预测的需要 */
    CHECKED_MALLOC( frame->ref[0], 4 * i_mb_count * sizeof(int8_t) );	/* 为参考帧的每个MB分配4个字节空间? */
    if( h->param.i_bframe )		/* 如果使用B帧，分配相应的mv/ref存储空间 */
    {
        CHECKED_MALLOC( frame->mv[1], 2*16 * i_mb_count * sizeof(int16_t) );	//
        CHECKED_MALLOC( frame->ref[1], 4 * i_mb_count * sizeof(int8_t) );		//
    }
    else
    {
        frame->mv[1]  = NULL;
        frame->ref[1] = NULL;
    }

    CHECKED_MALLOC( frame->i_row_bits, i_lines/16 * sizeof(int) );	/* 为每行MB分配一个INT空间，存储该行MB一共多少个比特 */
    CHECKED_MALLOC( frame->i_row_qp, i_lines/16 * sizeof(int) );	/* 为每行MB分配一个INT空间，存储该行的qp */
    for( i = 0; i < h->param.i_bframe + 2; i++ )
        for( j = 0; j < h->param.i_bframe + 2; j++ )
            CHECKED_MALLOC( frame->i_row_satds[i][j], i_lines/16 * sizeof(int) );	/* 为每行MB分配一个INT空间，存储该行的satds，一共4组??【可能是对应4种不同的SATD的信息】 */

    return frame;

fail:
    x264_frame_delete( frame );
    return NULL;
}

/*
删除帧,释放空间

*/
void x264_frame_delete( x264_frame_t *frame )
{
    int i, j;
    for( i = 0; i < 12; i++ )
        x264_free( frame->buffer[i] );
    for( i = 0; i < X264_BFRAME_MAX+2; i++ )
        for( j = 0; j < X264_BFRAME_MAX+2; j++ )
            x264_free( frame->i_row_satds[i][j] );
	//需单独释放的内存
    x264_free( frame->i_row_bits );	//
    x264_free( frame->i_row_qp );
    x264_free( frame->mb_type );	//
    x264_free( frame->mv[0] );		//
    x264_free( frame->mv[1] );		//
    x264_free( frame->ref[0] );		//
    x264_free( frame->ref[1] );		//
    x264_free( frame );				//最后释放frame指向的内存
}

/*
将图像拷贝到帧中

*/
void x264_frame_copy_picture( x264_t *h, x264_frame_t *dst, x264_picture_t *src )
{
	/* 可以参考一下C++中结构体的赋值，这儿还是两个不一样的结构体，得按结构体里的字段取出来一个个赋值 */
    dst->i_type     = src->i_type;
    dst->i_qpplus1  = src->i_qpplus1;
    dst->i_pts      = src->i_pts;

	/* 源图像src里所用的色彩空间？ */
    switch( src->img.i_csp & X264_CSP_MASK )	/* 按位与 */
    {		/*  */
        case X264_CSP_I420:		//x264_t中x264_csp_function_t   csp;
            h->csp.i420( dst, &src->img, h->param.i_width, h->param.i_height );//void (*i420)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
            break;//???通过函数指针调用函数???
			//这儿是通过函数指针调用了，那么在此之前此函数指针应该被进行了赋值操作，但没找到在哪儿赋的值
			//在csp.c中x264_csp_init中实现的赋值
			/*
            pf->i420 = i420_to_i420;	//给结构体的字段赋值
            pf->i422 = i422_to_i420;
            pf->i444 = i444_to_i420;
            pf->yv12 = yv12_to_i420;
            pf->yuyv = yuyv_to_i420;
            pf->rgb  = rgb_to_i420;
            pf->bgr  = bgr_to_i420;
            pf->bgra = bgra_to_i420;
			*/

        case X264_CSP_YV12:
            h->csp.yv12( dst, &src->img, h->param.i_width, h->param.i_height );//void (*yv12)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
            break;
        case X264_CSP_I422:
            h->csp.i422( dst, &src->img, h->param.i_width, h->param.i_height );//void (*i422)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
            break;
        case X264_CSP_I444:
            h->csp.i444( dst, &src->img, h->param.i_width, h->param.i_height );//void (*i444)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
            break;
        case X264_CSP_YUYV:
            h->csp.yuyv( dst, &src->img, h->param.i_width, h->param.i_height );//void (*yuyv)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
            break;
        case X264_CSP_RGB:
            h->csp.rgb( dst, &src->img, h->param.i_width, h->param.i_height );//void (*rgb )( x264_frame_t *, x264_image_t *, int i_width, int i_height );
            break;
        case X264_CSP_BGR:
            h->csp.bgr( dst, &src->img, h->param.i_width, h->param.i_height );//void (*bgr )( x264_frame_t *, x264_image_t *, int i_width, int i_height );
            break;
        case X264_CSP_BGRA:
            h->csp.bgra( dst, &src->img, h->param.i_width, h->param.i_height );//void (*bgra)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
            break;

        default:
            x264_log( h, X264_LOG_ERROR, "Arg invalid无效的 CSP色彩空间 \n" );
            break;
    }
}



/*
 * 名称：
 * 功能：边界扩展(被其他具体的扩展函数调用)
 * 参数：
 * 返回：
 * 资料：pad:填充
 * 思路：
 * 
*/
static void plane_expand_border( uint8_t *pix, int i_stride, int i_height, int i_pad )
{
#define PPIXEL(x, y) ( pix + (x) + (y)*i_stride )
    const int i_width = i_stride - 2*i_pad;
    int y;

    for( y = 0; y < i_height; y++ )
    {
        /* left左 band区 */
        memset( PPIXEL(-i_pad, y), PPIXEL(0, y)[0], i_pad );
        /* right band */
        memset( PPIXEL(i_width, y), PPIXEL(i_width-1, y)[0], i_pad );
    }
    /* upper上部的 band区 */
    for( y = 0; y < i_pad; y++ )
        memcpy( PPIXEL(-i_pad, -y-1), PPIXEL(-i_pad, 0), i_stride );
    /* lower下部的 band区 */
    for( y = 0; y < i_pad; y++ )
        memcpy( PPIXEL(-i_pad, i_height+y), PPIXEL(-i_pad, i_height-1), i_stride );
#undef PPIXEL
	/*
	#undef 标识符　
	其中，标识符是一个宏名称。如果标识符当前没有被定义成一个宏名称，那么就会忽略该指令。 　　一旦定义预处理器标识符，它将保持已定义状态且在作用域内，直到程序结束或者使用#undef 指令取消定义。 
	*/
}

/*
 * 名称：
 * 功能：帧边界扩展
 * 参数：
 * 返回：
 * 资料：video:http://www.powercam.cc/slide/8377
 * 思路：
 * 
*/
void x264_frame_expand_border( x264_frame_t *frame )
{
    int i;
    for( i = 0; i < frame->i_plane; i++ )	/* 检查每个plane，一般图像都是有3个 */
    {
        int i_pad = i ? 16 : 32;	/* 应该等效于 int i_pad = (i ? 16 : 32) 亮度plane返回32 色度plane时返回16*/
        plane_expand_border( frame->plane[i], frame->i_stride[i], frame->i_lines[i], i_pad );
		//i=0:plane_expand_border( frame->plane[0], frame->i_stride[0], frame->i_lines[0], 32 );
		//i=1:plane_expand_border( frame->plane[1], frame->i_stride[1], frame->i_lines[1], 16 );
		//i=2:plane_expand_border( frame->plane[2], frame->i_stride[2], frame->i_lines[2], 16 );
    }
}

/*
为滤波进行的边界扩展

*/
void x264_frame_expand_border_filtered( x264_frame_t *frame )
{
    /* during filtering, 8 extra pixels were filtered on each edge. 
       we want to expand border from the last filtered pixel */
    int i;
    for( i = 1; i < 4; i++ )
        plane_expand_border( frame->filtered[i] - 8*frame->i_stride[0] - 8, frame->i_stride[0], frame->i_lines[0]+2*8, 24 );
}

/*
为计算亮度半像素值进行边界扩展

*/
void x264_frame_expand_border_lowres( x264_frame_t *frame )
{
    int i;
    for( i = 0; i < 4; i++ )
        plane_expand_border( frame->lowres[i], frame->i_stride_lowres, frame->i_lines_lowres, 32 );
}

/*
帧边界不是16整数倍时进行边界扩展

*/
void x264_frame_expand_border_mod16( x264_t *h, x264_frame_t *frame )//让长和宽都是16的倍数
{
    int i, y;
    for( i = 0; i < frame->i_plane; i++ )//每个frame会有3个plane，YUV
    {
        int i_subsample = i ? 1 : 0;//i为0时，右侧为0，i非0时，右侧为1
        int i_width = h->param.i_width >> i_subsample;//...>>0或...>>1两种情况
        int i_height = h->param.i_height >> i_subsample;
		//用16的倍数的宽度-原本的宽度，就是我们要扩展的宽度http://www.powercam.cc/slide/8377
        int i_padx = ( h->sps->i_mb_width * 16 - h->param.i_width ) >> i_subsample;//pad:衰减器，填充//i_mb_width指的是从宽度上说有多少个宏快
        int i_pady = ( h->sps->i_mb_height * 16 - h->param.i_height ) >> i_subsample;

        if( i_padx )//i_padx是计算出来的要扩展的列数
        {
            for( y = 0; y < i_height; y++ )//每列都有i_height个像素
                memset( &frame->plane[i][y*frame->i_stride[i] + i_width],	/* 目标列 */
                         frame->plane[i][y*frame->i_stride[i] + i_width - 1],/* 源：左边n列 */
                         i_padx );//向右延伸，把靠近的复制过去
        }
        if( i_pady )//i_pady是计算出来的要扩展的行数
        {
            for( y = i_height; y < i_height + i_pady; y++ )
                memcpy( &frame->plane[i][y*frame->i_stride[i]],
                        &frame->plane[i][(i_height-1)*frame->i_stride[i]],
                        i_width + i_padx );//向下延伸，把靠近的复制过来
        }
    }
}//视频：http://www.powercam.cc/slide/8377


/*

*/
/* Deblocking filter */

static const int i_alpha_table[52] =
{
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  4,  4,  5,  6,
     7,  8,  9, 10, 12, 13, 15, 17, 20, 22,
    25, 28, 32, 36, 40, 45, 50, 56, 63, 71,
    80, 90,101,113,127,144,162,182,203,226,
    255, 255
};

/*

*/
static const int i_beta_table[52] =
{
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  2,  2,  2,  3,
     3,  3,  3,  4,  4,  4,  6,  6,  7,  7,
     8,  8,  9,  9, 10, 10, 11, 11, 12, 12,
    13, 13, 14, 14, 15, 15, 16, 16, 17, 17,
    18, 18
};

/*

*/
static const int i_tc0_table[52][3] =
{
    { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
    { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
    { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 1 },
    { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 1 }, { 1, 1, 1 },
    { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 2 }, { 1, 1, 2 }, { 1, 1, 2 },
    { 1, 1, 2 }, { 1, 2, 3 }, { 1, 2, 3 }, { 2, 2, 3 }, { 2, 2, 4 }, { 2, 3, 4 },
    { 2, 3, 4 }, { 3, 3, 5 }, { 3, 4, 6 }, { 3, 4, 6 }, { 4, 5, 7 }, { 4, 5, 8 },
    { 4, 6, 9 }, { 5, 7,10 }, { 6, 8,11 }, { 6, 8,13 }, { 7,10,14 }, { 8,11,16 },
    { 9,12,18 }, {10,13,20 }, {11,15,23 }, {13,17,25 }
};

/*

*/
/* From ffmpeg */
static inline int clip_uint8( int a )
{
    if (a&(~255))
        return (-a)>>31;
    else
        return a;
}

/*
bs=1~3时,修正亮度MB边界的p0和q0值

*/
static inline void deblock_luma_c( uint8_t *pix, int xstride, int ystride, int alpha, int beta, int8_t *tc0 )
{
    int i, d;
    for( i = 0; i < 4; i++ ) {
        if( tc0[i] < 0 ) {
            pix += 4*ystride;
            continue;
        }
        for( d = 0; d < 4; d++ ) {
            const int p2 = pix[-3*xstride];
            const int p1 = pix[-2*xstride];
            const int p0 = pix[-1*xstride];
            const int q0 = pix[ 0*xstride];
            const int q1 = pix[ 1*xstride];
            const int q2 = pix[ 2*xstride];
   
            if( abs( p0 - q0 ) < alpha &&
                abs( p1 - p0 ) < beta &&
                abs( q1 - q0 ) < beta ) {
   
                int tc = tc0[i];
                int delta;
   
                if( abs( p2 - p0 ) < beta ) {
                    pix[-2*xstride] = p1 + x264_clip3( (( p2 + ((p0 + q0 + 1) >> 1)) >> 1) - p1, -tc0[i], tc0[i] );
                    tc++; 
                }
                if( abs( q2 - q0 ) < beta ) {
                    pix[ 1*xstride] = q1 + x264_clip3( (( q2 + ((p0 + q0 + 1) >> 1)) >> 1) - q1, -tc0[i], tc0[i] );
                    tc++;
                }
    
                delta = x264_clip3( (((q0 - p0 ) << 2) + (p1 - q1) + 4) >> 3, -tc, tc );
                pix[-1*xstride] = clip_uint8( p0 + delta );    /* p0' */
                pix[ 0*xstride] = clip_uint8( q0 - delta );    /* q0' */
            }
            pix += ystride;
        }
    }
}

/*
亮度分量垂直边界去块滤波

*/
static void deblock_v_luma_c( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
{
    deblock_luma_c( pix, stride, 1, alpha, beta, tc0 ); 
}

/*
亮度分量水平边界去块滤波

*/
static void deblock_h_luma_c( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
{
    deblock_luma_c( pix, 1, stride, alpha, beta, tc0 );
}

/*
bs=1~3时,修正色度MB边界的p0和q0值
*/
static inline void deblock_chroma_c( uint8_t *pix, int xstride, int ystride, int alpha, int beta, int8_t *tc0 )
{
    int i, d;
    for( i = 0; i < 4; i++ ) {
        const int tc = tc0[i];
        if( tc <= 0 ) {
            pix += 2*ystride;
            continue;
        }
        for( d = 0; d < 2; d++ ) {
            const int p1 = pix[-2*xstride];
            const int p0 = pix[-1*xstride];
            const int q0 = pix[ 0*xstride];
            const int q1 = pix[ 1*xstride];

            if( abs( p0 - q0 ) < alpha &&
                abs( p1 - p0 ) < beta &&
                abs( q1 - q0 ) < beta ) {

                int delta = x264_clip3( (((q0 - p0 ) << 2) + (p1 - q1) + 4) >> 3, -tc, tc );
                pix[-1*xstride] = clip_uint8( p0 + delta );    /* p0' */
                pix[ 0*xstride] = clip_uint8( q0 - delta );    /* q0' */
            }
            pix += ystride;
        }
    }
}

/*
色度分量垂直边界去块滤波

*/
static void deblock_v_chroma_c( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
{   
    deblock_chroma_c( pix, stride, 1, alpha, beta, tc0 );
}

/*
色度分量水平边界去块滤波

*/
static void deblock_h_chroma_c( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
{   
    deblock_chroma_c( pix, 1, stride, alpha, beta, tc0 );
}

/*
bs=4时,修正亮度MB边界的值

*/
static inline void deblock_luma_intra_c( uint8_t *pix, int xstride, int ystride, int alpha, int beta )
{
    int d;
    for( d = 0; d < 16; d++ ) {
        const int p2 = pix[-3*xstride];
        const int p1 = pix[-2*xstride];
        const int p0 = pix[-1*xstride];
        const int q0 = pix[ 0*xstride];
        const int q1 = pix[ 1*xstride];
        const int q2 = pix[ 2*xstride];

        if( abs( p0 - q0 ) < alpha &&
            abs( p1 - p0 ) < beta &&
            abs( q1 - q0 ) < beta ) {

            if(abs( p0 - q0 ) < ((alpha >> 2) + 2) ){
                if( abs( p2 - p0 ) < beta)
                {
                    const int p3 = pix[-4*xstride];
                    /* p0', p1', p2' */
                    pix[-1*xstride] = ( p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4 ) >> 3;
                    pix[-2*xstride] = ( p2 + p1 + p0 + q0 + 2 ) >> 2;
                    pix[-3*xstride] = ( 2*p3 + 3*p2 + p1 + p0 + q0 + 4 ) >> 3;
                } else {
                    /* p0' */
                    pix[-1*xstride] = ( 2*p1 + p0 + q1 + 2 ) >> 2;
                }
                if( abs( q2 - q0 ) < beta)
                {
                    const int q3 = pix[3*xstride];
                    /* q0', q1', q2' */
                    pix[0*xstride] = ( p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4 ) >> 3;
                    pix[1*xstride] = ( p0 + q0 + q1 + q2 + 2 ) >> 2;
                    pix[2*xstride] = ( 2*q3 + 3*q2 + q1 + q0 + p0 + 4 ) >> 3;
                } else {
                    /* q0' */
                    pix[0*xstride] = ( 2*q1 + q0 + p1 + 2 ) >> 2;
                }
            }else{
                /* p0', q0' */
                pix[-1*xstride] = ( 2*p1 + p0 + q1 + 2 ) >> 2;
                pix[ 0*xstride] = ( 2*q1 + q0 + p1 + 2 ) >> 2;
            }
        }
        pix += ystride;
    }
}

/*
帧内亮度分量垂直边界去块滤波

*/
static void deblock_v_luma_intra_c( uint8_t *pix, int stride, int alpha, int beta )
{   
    deblock_luma_intra_c( pix, stride, 1, alpha, beta );
}

/*
帧内亮度分量水平边界去块滤波

*/
static void deblock_h_luma_intra_c( uint8_t *pix, int stride, int alpha, int beta )
{   
    deblock_luma_intra_c( pix, 1, stride, alpha, beta );
}

/*
bs=4时,修正色度MB边界的值

*/
static inline void deblock_chroma_intra_c( uint8_t *pix, int xstride, int ystride, int alpha, int beta )
{   
    int d; 
    for( d = 0; d < 8; d++ ) {
        const int p1 = pix[-2*xstride];
        const int p0 = pix[-1*xstride];
        const int q0 = pix[ 0*xstride];
        const int q1 = pix[ 1*xstride];

        if( abs( p0 - q0 ) < alpha &&
            abs( p1 - p0 ) < beta &&
            abs( q1 - q0 ) < beta ) {

            pix[-1*xstride] = (2*p1 + p0 + q1 + 2) >> 2;   /* p0' */
            pix[ 0*xstride] = (2*q1 + q0 + p1 + 2) >> 2;   /* q0' */
        }

        pix += ystride;
    }
}

/*
帧内色度分量垂直边界去块滤波

*/
static void deblock_v_chroma_intra_c( uint8_t *pix, int stride, int alpha, int beta )
{   
    deblock_chroma_intra_c( pix, stride, 1, alpha, beta );
}

/*
帧内色度分量水平边界去块滤波

*/
static void deblock_h_chroma_intra_c( uint8_t *pix, int stride, int alpha, int beta )
{   
    deblock_chroma_intra_c( pix, 1, stride, alpha, beta );
}

/*
bs值确定

*/
static inline void deblock_edge( x264_t *h, uint8_t *pix, int i_stride, int bS[4], int i_qp, int b_chroma,
                                 x264_deblock_inter_t pf_inter, x264_deblock_intra_t pf_intra )
{
    int i;
    const int index_a = x264_clip3( i_qp + h->sh.i_alpha_c0_offset, 0, 51 );
    const int alpha = i_alpha_table[index_a];
    const int beta  = i_beta_table[x264_clip3( i_qp + h->sh.i_beta_offset, 0, 51 )];

    if( bS[0] < 4 ) {
        int8_t tc[4]; 
        for(i=0; i<4; i++)
            tc[i] = (bS[i] ? i_tc0_table[index_a][bS[i] - 1] : -1) + b_chroma;
        pf_inter( pix, i_stride, alpha, beta, tc );
    } else {
        pf_intra( pix, i_stride, alpha, beta );
    }
}


/*
 * 名称：
 * 功能：帧去块滤波主函数，对一幅cif格式的图像进行边界滤波
 * 参数：x264_t *h：指定编码器
		 int i_slice_type:slice的类型
		 SLICE_TYPE_P  = 0,SLICE_TYPE_B  = 1,SLICE_TYPE_I  = 2
 * 返回：
 * 资料：
 * 思路：
 * 
*/
void x264_frame_deblocking_filter( x264_t *h, int i_slice_type )//帧去块滤波主函数
{
    const int s8x8 = 2 * h->mb.i_mb_stride;
    const int s4x4 = 4 * h->mb.i_mb_stride;

    int mb_y, mb_x;

    for( mb_y = 0, mb_x = 0; mb_y < h->sps->i_mb_height; )//从上往下，循环次数是以宏块为单位表示的高度
    {
        const int mb_xy  = mb_y * h->mb.i_mb_stride + mb_x;
        const int mb_8x8 = 2 * s8x8 * mb_y + 2 * mb_x;
        const int mb_4x4 = 4 * s4x4 * mb_y + 4 * mb_x;
        const int b_8x8_transform = h->mb.mb_transform_size[mb_xy];
        const int i_edge_end = (h->mb.type[mb_xy] == P_SKIP) ? 1 : 4;
        int i_edge, i_dir;

        /* cavlc + 8x8 transform stores累积 nnz per 16 coeffs for the purpose of
         * entropy熵,平均信息量 coding编码, but per 64 coeffs系数 for the purpose目的 of deblocking解块 */
        if( !h->param.b_cabac && b_8x8_transform )
        {
            uint32_t *nnz = (uint32_t*)h->mb.non_zero_count[mb_xy];
            if( nnz[0] ) nnz[0] = 0x01010101;
            if( nnz[1] ) nnz[1] = 0x01010101;
            if( nnz[2] ) nnz[2] = 0x01010101;
            if( nnz[3] ) nnz[3] = 0x01010101;
        }

        /* i_dir == 0 -> vertical垂直的 edge边
         * i_dir == 1 -> horizontal水平的 edge边 */
        for( i_dir = 0; i_dir < 2; i_dir++ )
        {
            int i_start = (i_dir ? mb_y : mb_x) ? 0 : 1;
            int i_qp, i_qpn;

            for( i_edge = i_start; i_edge < i_edge_end; i_edge++ )
            {
                int mbn_xy, mbn_8x8, mbn_4x4;
                int bS[4];  /* filtering strength */

                if( b_8x8_transform && (i_edge&1) )
                    continue;

                mbn_xy  = i_edge > 0 ? mb_xy  : ( i_dir == 0 ? mb_xy  - 1 : mb_xy - h->mb.i_mb_stride );
                mbn_8x8 = i_edge > 0 ? mb_8x8 : ( i_dir == 0 ? mb_8x8 - 2 : mb_8x8 - 2 * s8x8 );
                mbn_4x4 = i_edge > 0 ? mb_4x4 : ( i_dir == 0 ? mb_4x4 - 4 : mb_4x4 - 4 * s4x4 );

                /* *** Get bS for each 4px for the current edge *** */
                if( IS_INTRA( h->mb.type[mb_xy] ) || IS_INTRA( h->mb.type[mbn_xy] ) )
                {
                    bS[0] = bS[1] = bS[2] = bS[3] = ( i_edge == 0 ? 4 : 3 );
                }
                else
                {
                    int i;
                    for( i = 0; i < 4; i++ )
                    {
                        int x  = i_dir == 0 ? i_edge : i;
                        int y  = i_dir == 0 ? i      : i_edge;
                        int xn = (x - (i_dir == 0 ? 1 : 0 ))&0x03;
                        int yn = (y - (i_dir == 0 ? 0 : 1 ))&0x03;

                        if( h->mb.non_zero_count[mb_xy][block_idx_xy[x][y]] != 0 ||
                            h->mb.non_zero_count[mbn_xy][block_idx_xy[xn][yn]] != 0 )
                        {
                            bS[i] = 2;
                        }
                        else
                        {
                            /* FIXME: A given frame may occupy more than one position in
                             * the reference list. So we should compare the frame numbers,
                             * not the indices in the ref list.
                             * No harm yet, as we don't generate that case.*/

                            int i8p= mb_8x8+(x/2)+(y/2)*s8x8;
                            int i8q= mbn_8x8+(xn/2)+(yn/2)*s8x8;
                            int i4p= mb_4x4+x+y*s4x4;
                            int i4q= mbn_4x4+xn+yn*s4x4;
                            int l;

                            bS[i] = 0;

                            for( l = 0; l < 1 + (i_slice_type == SLICE_TYPE_B); l++ )
                            {
                                if( h->mb.ref[l][i8p] != h->mb.ref[l][i8q] ||
                                    abs( h->mb.mv[l][i4p][0] - h->mb.mv[l][i4q][0] ) >= 4 ||
                                    abs( h->mb.mv[l][i4p][1] - h->mb.mv[l][i4q][1] ) >= 4 )
                                {
                                    bS[i] = 1;
                                    break;
                                }
                            }
                        }
                    }
                }

                /* *** filter *** */
                /* Y plane */
                i_qp = h->mb.qp[mb_xy];
                i_qpn= h->mb.qp[mbn_xy];

                if( i_dir == 0 )
                {
                    /* vertical edge */
                    deblock_edge( h, &h->fdec->plane[0][16*mb_y * h->fdec->i_stride[0] + 16*mb_x + 4*i_edge],
                                  h->fdec->i_stride[0], bS, (i_qp+i_qpn+1) >> 1, 0,
                                  h->loopf.deblock_h_luma, h->loopf.deblock_h_luma_intra );
                    if( !(i_edge & 1) )
                    {
                        /* U/V planes */
                        int i_qpc = ( i_chroma_qp_table[x264_clip3( i_qp + h->pps->i_chroma_qp_index_offset, 0, 51 )] +
                                      i_chroma_qp_table[x264_clip3( i_qpn + h->pps->i_chroma_qp_index_offset, 0, 51 )] + 1 ) >> 1;
                        deblock_edge( h, &h->fdec->plane[1][8*(mb_y*h->fdec->i_stride[1]+mb_x)+2*i_edge],
                                      h->fdec->i_stride[1], bS, i_qpc, 1,
                                      h->loopf.deblock_h_chroma, h->loopf.deblock_h_chroma_intra );
                        deblock_edge( h, &h->fdec->plane[2][8*(mb_y*h->fdec->i_stride[2]+mb_x)+2*i_edge],
                                      h->fdec->i_stride[2], bS, i_qpc, 1,
                                      h->loopf.deblock_h_chroma, h->loopf.deblock_h_chroma_intra );
                    }
                }
                else
                {
                    /* horizontal edge */
                    deblock_edge( h, &h->fdec->plane[0][(16*mb_y + 4*i_edge) * h->fdec->i_stride[0] + 16*mb_x],
                                  h->fdec->i_stride[0], bS, (i_qp+i_qpn+1) >> 1, 0,
                                  h->loopf.deblock_v_luma, h->loopf.deblock_v_luma_intra );
                    /* U/V planes */
                    if( !(i_edge & 1) )
                    {
                        int i_qpc = ( i_chroma_qp_table[x264_clip3( i_qp + h->pps->i_chroma_qp_index_offset, 0, 51 )] +
                                      i_chroma_qp_table[x264_clip3( i_qpn + h->pps->i_chroma_qp_index_offset, 0, 51 )] + 1 ) >> 1;
                        deblock_edge( h, &h->fdec->plane[1][8*(mb_y*h->fdec->i_stride[1]+mb_x)+2*i_edge*h->fdec->i_stride[1]],
                                      h->fdec->i_stride[1], bS, i_qpc, 1,
                                      h->loopf.deblock_v_chroma, h->loopf.deblock_v_chroma_intra );
                        deblock_edge( h, &h->fdec->plane[2][8*(mb_y*h->fdec->i_stride[2]+mb_x)+2*i_edge*h->fdec->i_stride[2]],
                                      h->fdec->i_stride[2], bS, i_qpc, 1,
                                      h->loopf.deblock_v_chroma, h->loopf.deblock_v_chroma_intra );
                    }
                }
            }
        }

        /* newt mb */
        mb_x++;
        if( mb_x >= h->sps->i_mb_width )
        {
            mb_x = 0;
            mb_y++;
        }
    }
}

#ifdef HAVE_MMXEXT
void x264_deblock_v_chroma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void x264_deblock_h_chroma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void x264_deblock_v_chroma_intra_mmxext( uint8_t *pix, int stride, int alpha, int beta );
void x264_deblock_h_chroma_intra_mmxext( uint8_t *pix, int stride, int alpha, int beta );
#endif

#ifdef ARCH_X86_64
void x264_deblock_v_luma_sse2( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void x264_deblock_h_luma_sse2( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
#elif defined( HAVE_MMXEXT )
void x264_deblock_h_luma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void x264_deblock_v8_luma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );

/*

*/
void x264_deblock_v_luma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
{
    x264_deblock_v8_luma_mmxext( pix,   stride, alpha, beta, tc0   );
    x264_deblock_v8_luma_mmxext( pix+8, stride, alpha, beta, tc0+2 );
}
#endif

/*
去块滤波初始化

*/
void x264_deblock_init( int cpu, x264_deblock_function_t *pf )
{
    pf->deblock_v_luma = deblock_v_luma_c;
    pf->deblock_h_luma = deblock_h_luma_c;
    pf->deblock_v_chroma = deblock_v_chroma_c;
    pf->deblock_h_chroma = deblock_h_chroma_c;
    pf->deblock_v_luma_intra = deblock_v_luma_intra_c;
    pf->deblock_h_luma_intra = deblock_h_luma_intra_c;
    pf->deblock_v_chroma_intra = deblock_v_chroma_intra_c;
    pf->deblock_h_chroma_intra = deblock_h_chroma_intra_c;

#ifdef HAVE_MMXEXT
    if( cpu&X264_CPU_MMXEXT )
    {
        pf->deblock_v_chroma = x264_deblock_v_chroma_mmxext;
        pf->deblock_h_chroma = x264_deblock_h_chroma_mmxext;
        pf->deblock_v_chroma_intra = x264_deblock_v_chroma_intra_mmxext;
        pf->deblock_h_chroma_intra = x264_deblock_h_chroma_intra_mmxext;

#ifdef ARCH_X86_64
        if( cpu&X264_CPU_SSE2 )
        {
            pf->deblock_v_luma = x264_deblock_v_luma_sse2;
            pf->deblock_h_luma = x264_deblock_h_luma_sse2;
        }
#else
        pf->deblock_v_luma = x264_deblock_v_luma_mmxext;
        pf->deblock_h_luma = x264_deblock_h_luma_mmxext;
#endif
    }
#endif
}

