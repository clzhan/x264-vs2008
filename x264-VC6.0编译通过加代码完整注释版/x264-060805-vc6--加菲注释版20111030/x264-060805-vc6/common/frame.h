/*****************************************************************************
 * frame.h: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: frame.h,v 1.1 2004/06/03 19:27:06 fenrir Exp $
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

#ifndef _FRAME_H
	#define _FRAME_H 1


	/* x264_picture_t和x264_frame_t的区别
	前者是说明一个视频序列中每帧的特点.后者存放每帧实际的象素值 
	*/
	typedef struct
	{
		/* */
		int     i_poc;		/* poc标识图像的播放顺序 ，毕厚杰，第160页 */
		int     i_type;		/* I_type 指明被编码图像的类型，有X264_TYPE_AUTO X264_TYPE_IDR X264_TYPE_I X264_TYPE_P X264_TYPE_BREF X264_TYPE_B可供选择，初始化为AUTO，说明由x264在编码过程中自行控制 */
		int     i_qpplus1;	/* 此参数减1代表当前画面的量化参数值 */
		int64_t i_pts;		/* program time stamp 程序时间戳，指示这幅画面编码的时间戳 */
		int     i_frame;    /* Presentation(提示) frame number */
		int     i_frame_num; /* Coded(编码的) frame number */
		int     b_kept_as_ref;	/*  */
		float   f_qp_avg;		/*  */

		/* YUV buffer */
		int     i_plane;			/*  */
		int     i_stride[4];		/* 步长 */
		int     i_lines[4];			/*  */
		int     i_stride_lowres;	/*  */
		int     i_lines_lowres;		/*  */
		uint8_t *plane[4];			/* 面数，一般都有3个YUV */
		uint8_t *filtered[4];		/* plane[0], H, V, HV */ 
									/*
										0：滤波后的整像素点平面
										1：滤波后的整像素点右边 1/2 像素点平面
										2：滤波后的整像素点下边 1/2 像素点平面
										3：滤波后的整像素点右下 1/2 像素点平面
 
									*/


		uint8_t *lowres[4]; /* half-size copy of input frame: Orig, H, V, HV () */
							/*
							从变量名判断可能是存的下采样图像，X264 中有个模块要用到下采样图像来先计算，似乎是码率控制，记不清楚了。交换两个 lowres 是一种缓冲使用的技巧，相当于双缓冲乒乓操作。
							起始搜索点的确定：
							1、利用下采样图像得到的MV
							2.
							X (previous frame,this MB)        
							Y
							Z
							Fig 1 Sources of candidate MVs for motion search.
							这里"利用下采样图像得到的MV" 是不是直接获取Z的MV
							是
							http://www.chinavideo.org/viewthread.php?tid=9942&highlight=x264%5C_frame%5C_t
							*/
		uint16_t *integral; 	/* integral:完整的；完备的 */

		/* for unrestricted(无限制的) mv we allocate(分配) more data than needed
		 * allocated data are stored in buffer */
		void    *buffer[12];

		/* motion(运动) data(数据) */
		int8_t  *mb_type;	/* mb模式 */
		int16_t (*mv[2])[2];//int16_t (*mv[2])[2];这是一个指针数组，指向mv[2],mv为含有2个元素的指针数组,每个元素含有2个int16_t的一维数组 //http://wmnmtm.blog.163.com/blog/static/382457142011762857642/
							//囧森(18147145):后面的2,指的应该是x和y，前面的2 指的是前向和后项,这个就是存放mv信息的数组
		int8_t  *ref[2];
		int     i_ref[2];
		int     ref_poc[2][16];//

		/* for adaptive(自适应) B-frame decision(决策).
		 * contains(包含) the SATD cost(成本) of the lowres frame encoded in various(各种各样的) modes
		 * FIXME: 在需要时如何扩大一个数组how big an array do we need? */
		int     i_cost_est[X264_BFRAME_MAX+2][X264_BFRAME_MAX+2];	/*  */
		int     i_satd; 	/*  */ // the i_cost_est of the selected frametype
		int     i_intra_mbs[X264_BFRAME_MAX+2];		/*  */
		int     *i_row_satds[X264_BFRAME_MAX+2][X264_BFRAME_MAX+2];		/*  */
		int     *i_row_satd;	/*  */
		int     *i_row_bits;	/*  */
		int     *i_row_qp;		/* row:列,行 */

	} x264_frame_t;	//x264_picture_t和x264_frame_t的区别在于，前者是说明一个视频序列中每帧的特点，后者存放每帧实际的象素值。要注意区分

	typedef void (*x264_deblock_inter_t)( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
	typedef void (*x264_deblock_intra_t)( uint8_t *pix, int stride, int alpha, int beta );
	typedef struct
	{
		x264_deblock_inter_t deblock_v_luma;
		x264_deblock_inter_t deblock_h_luma;
		x264_deblock_inter_t deblock_v_chroma;
		x264_deblock_inter_t deblock_h_chroma;
		x264_deblock_intra_t deblock_v_luma_intra;
		x264_deblock_intra_t deblock_h_luma_intra;
		x264_deblock_intra_t deblock_v_chroma_intra;
		x264_deblock_intra_t deblock_h_chroma_intra;
	} x264_deblock_function_t;

	x264_frame_t *x264_frame_new( x264_t *h );
	void          x264_frame_delete( x264_frame_t *frame );

	void          x264_frame_copy_picture( x264_t *h, x264_frame_t *dst, x264_picture_t *src );/*图像到帧*/

	void          x264_frame_expand_border( x264_frame_t *frame );	/* 扩展边 */
	void          x264_frame_expand_border_filtered( x264_frame_t *frame );
	void          x264_frame_expand_border_lowres( x264_frame_t *frame );
	void          x264_frame_expand_border_mod16( x264_t *h, x264_frame_t *frame );/* 扩展成16的倍数 */

	void          x264_frame_deblocking_filter( x264_t *h, int i_slice_type );

	void          x264_frame_filter( int cpu, x264_frame_t *frame );
	void          x264_frame_init_lowres( int cpu, x264_frame_t *frame );

	void          x264_deblock_init( int cpu, x264_deblock_function_t *pf );

#endif
