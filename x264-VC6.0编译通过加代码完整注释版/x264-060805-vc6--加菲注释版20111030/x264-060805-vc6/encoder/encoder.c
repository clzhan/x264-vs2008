/*****************************************************************************
 * x264: h264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: encoder.c,v 1.1 2004/06/03 19:27:08 fenrir Exp $
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
#include <math.h>

#include "common/common.h"
#include "common/cpu.h"

#include "set.h"
#include "analyse.h"
#include "ratecontrol.h"
#include "macroblock.h"

#if VISUALIZE

#include "common/visualize.h"//形象化
#endif

//#define DEBUG_MB_TYPE
//#define DEBUG_DUMP_FRAME
//#define DEBUG_BENCHMARK

#ifdef DEBUG_BENCHMARK
	static int64_t i_mtime_encode_frame = 0;
	static int64_t i_mtime_analyse = 0;
	static int64_t i_mtime_encode = 0;
	static int64_t i_mtime_write = 0;
	static int64_t i_mtime_filter = 0;
	#define TIMER_START( d ) \
    { \
        int64_t d##start = x264_mdate();

		#define TIMER_STOP( d ) \
        d += x264_mdate() - d##start;\
    }	//当前时间 - 开始时间
#else	//上面只是debug时才输出
	#define TIMER_START( d )
	#define TIMER_STOP( d )
#endif

#define NALU_OVERHEAD 5 // startcode + NAL type costs 5 bytes per frame//OVERHEAD:开销
	//0x000000->0x00000300
	//0x000001->0x00000301
	//0x000002->0x00000302
	//0x000003->0x00000303
	//这些是4字节
	//0bit+2bit+5bit(禁止位+优先级+NAL_unit的类型) =1字节
	//共5字节


/****************************************************************************
 *
 ******************************* x264 libs **********************************
 *
 ****************************************************************************/
/*
 * 名称：
 * 参数：i_size好像是像素的个数(帧尺寸)，因为有调用代码为：
		 h->stat.f_psnr_mean_y[i_slice_type] += x264_psnr( i_sqe_y, h->param.i_width * h->param.i_height ) 
		 h->stat.f_psnr_mean_u[i_slice_type] += x264_psnr( i_sqe_u, h->param.i_width * h->param.i_height / 4 );
		 h->stat.f_psnr_mean_v[i_slice_type] +=  x264_psnr( i_sqe_v, h->param.i_width * h->param.i_height / 4 );
  * 返回：
 * 功能：求解信噪比x264_psnr()
 * 思路：
 * 资料：峰值信噪比（PSNR），一种评价图像的客观标准。它具有局限性，PSNR是 “PeakSignaltoNoiseRatio”的缩写。peak的中文意思是顶点。而radio的意思是比率或比列的。整个意思就是到达噪音比率的顶 点信号，psnr是一般是用于最大值信号和背景噪音之间的一个工程项目。通常在经过影像压缩之后，输出的影像通常都会有某种程度与原始影像?一样。为了衡 量经过处理后的影像品质，我们通常会参考PSNR值来认定某个处理程序够不够令人满意。它是原图像与处理图像之间均方误差相对于(2^n-1)^2的对数 值(信号最大值的平方，n是每个采样值的比特数)，它的单位是dB。公式如下：
		 http://wmnmtm.blog.163.com/blog/static/38245714201181722213153/
 * 
*/
static float x264_psnr( int64_t i_sqe, int64_t i_size )//PSNR(峰值信噪比)
{	//实际就是公式的实现，公式见： http://wmnmtm.blog.163.com/blog/static/38245714201181722213153/
    double f_mse = (double)i_sqe / ((double)65025.0 * (double)i_size);//均方误差（Mean Square Error）
    if( f_mse <= 0.0000000001 ) /* 图像压缩中典型的峰值信噪比值在 30 到 40dB 之间  Max 100dB */
        return 100;

    return (float)(-10.0 * log( f_mse ) / log( 10.0 ));//MATH.H:double  __cdecl log(double);
}

//#ifdef DEBUG_DUMP_FRAME
/*
 * 名称：
 * 参数：
 * 返回：
 * 功能：x264_frame_dump(...)把x264_frame_t的plane中存储的420格式的图像写入一个指定的文件
 * 思路：在函数内部打开、写入、关闭文件，好像只在调试时用，搜索源代码，未被调用过
 * 资料：http://wmnmtm.blog.163.com/blog/static/3824571420118188540701/
 * 
*/
static void x264_frame_dump( x264_t *h, x264_frame_t *fr, char *name )//dump:倾倒
{//把x264_frame_t的plane中存储的420格式的图像写入一个指定的文件，但搜索源码中，未调用过此函数
	//传入参数h，只为取到图像的宽和高，实际的图像来源于第二个参数fr
    FILE *f = fopen( name, "r+b" );//原为r+b追加 覆盖wb+ //打开文件//r+ 以可读写方式打开文件，该文件必须存在;加入b 字符用来告诉函数库打开的文件为二进制文件，而非纯文字文件。
    int i, y;
    if( !f )//返回值： 文件顺利打开后，指向该流的文件指针就会被返回。如果文件打开失败则返回NULL，并把错误代码存在errno 中
        return;//一般而言，打开文件后会作一些文件读取或写入的动作，若打开文件失败，接下来的读写动作也无法顺利进行，所以一般在fopen()后作错误判断及处理。

    /* 写入帧，以显示顺序 注意这儿，它是定位到了准确的位置，而不是从文件头开始写，也不是直接在原数据后面追加，如果只写第9帧，前面的帧在文件中仍存在，只是数据全是无实际意义的000000 Write the frame in display order */
    fseek( f, fr->i_frame * h->param.i_height * h->param.i_width * 3 / 2, SEEK_SET );//此句为定位文件指针到合适位置
	//f被设置为指向位置：从文件开头(SEEK_SET)起，偏移位置为第二个参数代表的大小(帧的编号x图像的宽x高x1.5)
	//
	//一帧分为好几个面，Y、U、V分量，也许会有另一个，不过一般都是3个，但前面定义的最大值为4
    for( i = 0; i < fr->i_plane; i++ )
    {
        for( y = 0; y < h->param.i_height / ( i == 0 ? 1 : 2 ); y++ )//0时代表亮度,1和2代表色度
        {
            fwrite( &fr->plane[i][y*fr->i_stride[i]], 1, h->param.i_width / ( i == 0 ? 1 : 2 ), f );//水平方向，i=0代表水平方向是图像宽度，非0时，代表是图像宽度的一半

        }
		//上面这个for实际就是，Y亮度是每像素都抽样，色度水平是二取一，垂直方向也是二取一(意思是这，但此处是写文件)
    }
    fclose( f );//关闭文件
	/*
	i=0:
		for(图像高/1)//即每个像素均有亮度Y
			fwrite(源缓冲区,要写入内容的单字节数,要进行写入size字节的数据项的个数,目标文件指针)
			fwrite( &fr->plane[0][0*fr->i_stride[0]], 1, h->param.i_width , f );

	i=1:
	i=2:
	i=3:	

	*/


}
//#endif


/* 
 * 填充默认值 ，初始化 Fill "default" values 
 * 顾名思议，片头初始化
*/
static void x264_slice_header_init( x264_t *h, x264_slice_header_t *sh,
                                    x264_sps_t *sps, x264_pps_t *pps,
                                    int i_type, int i_idr_pic_id, int i_frame, int i_qp )
{

    x264_param_t *param = &h->param;
    int i;
	//printf("\n encoder.c:i_frame = %d",i_frame);//zjh 打印效果：0、1、2、3...249,0、1、2、3 (测试序列为300帧)，说明i_frame最大值为250或249

    /* 一开始我们填充所有字段 First we fill all field */
    sh->sps = sps;		/* 序列参数集 右值是调用函数时提供的 */
    sh->pps = pps;		/* 图像参数集 右值是调用函数时提供的 */

    sh->i_type      = i_type;	/* [毕厚杰：Page 164] 相当于slice_type，指明片的类型；0123456789对应P、B、I、SP、SI */
    sh->i_first_mb  = 0;	/* 第一个宏块，注意这个是int型，不是指针，猜想它应该是一个数组的元素序号吧 [毕厚杰：Page 164] 相当于 first_mb_in_slice 片中的第一个宏块的地址，片通过这个句法元素来标定它自己的地址。要注意的是，在帧场自适应模式下，宏块都是成对出现的，这时本句法元素表示的是第几个宏块对，对应的第一个宏块的真实地址应该是2*--- */
    sh->i_last_mb   = h->sps->i_mb_width * h->sps->i_mb_height;	/* 最后一个宏块 比如 一个图像中长x宽12x10个宏块，最后一个宏块就是 12x10 */
    sh->i_pps_id    = pps->i_id;	/* [毕厚杰：Page 164] 相当于 pic_parameter_set_id ; 图像参数集的索引号，取值范围[0,255] */

    sh->i_frame_num = i_frame;	/* 编码顺序 从0开始到249，然后又从0开始 */

    sh->b_field_pic = 0;    /* 现在不支持场 Not field support for now [毕厚杰：Page 165] 相当于 field_pic_flag; 这是在片层标识图像编码模式的唯一一个句法元素。所谓的编码模式是指的帧编码、场编码、帧场自适应编码。当这个句法元素取值为1时属于场编码，0时为非场编码。 */
    sh->b_bottom_field = 1; /* 仍然不支持，底场 not yet used */

    sh->i_idr_pic_id = i_idr_pic_id;	/* [毕厚杰：Page 164] 相当于idr_pic_id ;IDR图像的标识。不同的IDR图像有不同的idr_pic_id值。值得注意的是，IDR图像不等价于I图像，只有在作为IDR图像的I帧才有这个句法元素。在场模式下，IDR帧的两个场有相同的idr_pic_id值。idr_pic_id的取值范围是[0,65536]，和frame_num类似，当它的值超过这个范围时，它会以循环的方式重新开始计数。 */
										/*  -1 if nal_type != 5  */
    /* poc stuff(填充), fixed(fix:确定) later */
    sh->i_poc_lsb = 0;				/* [毕厚杰：Page 166] 相当于 pic_order_cnt_lsb，在POC的第一种算法中本句法元素来计算POC值，在POC的第一种算法中是显式地传递POC的值，而其他两种算法是通过frame_num来映射POC的值，注意这个句法元素的读取函数是u(v)，这个v的值是序列参数集的句法元素--+4个比特得到的。取值范围是... */
    sh->i_delta_poc_bottom = 0;	/* [毕厚杰：Page 166] 相当于 delta_pic_order_cnt_bottom，如果是在场模式下，场对中的两个场各自被构造为一个图像，它们有各自的POC算法来分别计算两个场的POC值，也就是一个场对拥有一对POC值；而在帧模式或帧场自适应模式下，一个图像只能根据片头的句法元素计算出一个POC值。根据H.264的规定，在序列中可能出现场的情况，即frame_mbs_only_flag不为1时，每个帧或帧场自适应的图像在解码完后必须分解为两个场，以供后续图像中的场作为参考图像。所以当时frame_mb_only_flag不为1时，帧或帧场自适应 */
    sh->i_delta_poc[0] = 0;	/* [毕厚杰：Page 167] 相当于 delta_pic_order_cnt[0] , delta_pic_order_cnt[1] */
    sh->i_delta_poc[1] = 0;	/*  */

    sh->i_redundant_pic_cnt = 0;	/* [毕厚杰：Page 167] 相当于 redundant_pic_cnt;冗余片的id号，取值范围[0,127] redundant:多余的 */

    if( !h->mb.b_direct_auto_read )	/* ??? */
    {
        if( h->mb.b_direct_auto_write )	/*  */
            sh->b_direct_spatial_mv_pred = ( h->stat.i_direct_score[1] > h->stat.i_direct_score[0] );
        else
            sh->b_direct_spatial_mv_pred = ( param->analyse.i_direct_mv_pred == X264_DIRECT_PRED_SPATIAL );
    }
    /* else b_direct_spatial_mv_pred was read from the 2pass statsfile */

    sh->b_num_ref_idx_override = 0;		/* [毕厚杰：Page 167] 相当于 num_ref_idx_active_override_flag，在图像参数集中，有两个句法元素指定当前参考帧队列中实际可用的参考帧的数目。在片头可以重载这对句法元素，以给某特定图像更大的灵活度。这个句法元素就是指明片头是否会重载，如果该句法元素等于1，下面会出现新的num_ref_idx_10_active_minus1和...11... */
    sh->i_num_ref_idx_l0_active = 1;	/* [毕厚杰：Page 167] 相当于 num_ref_idx_10_active_minus1 */
    sh->i_num_ref_idx_l1_active = 1;	/* [毕厚杰：Page 167] 相当于 num_ref_idx_11_active_minus1 */

    sh->b_ref_pic_list_reordering_l0 = h->b_ref_reorder[0];	/* 短期参考图像列表重排序? */
    sh->b_ref_pic_list_reordering_l1 = h->b_ref_reorder[1];	/* 长期参考图像列表重排序? */

    /* 如果参考列表不是使用默认顺序，创建重排序头 If the ref list isn't in the default order, construct创建 reordering header */
    /* 参考列表1 仍然来需要重排序，只对参考列表0进行重排序 List1 reordering isn't needed yet */
    if( sh->b_ref_pic_list_reordering_l0 )	/*  */
    {
        int pred_frame_num = i_frame;	/*  */
        for( i = 0; i < h->i_ref0; i++ )	/*  */
        {
            int diff = h->fref0[i]->i_frame_num - pred_frame_num;	/*  */
            if( diff == 0 )	/*  */
                x264_log( h, X264_LOG_ERROR, "diff frame num == 0\n" );	/*  */
            sh->ref_pic_list_order[0][i].idc = ( diff > 0 );	/*  */
            sh->ref_pic_list_order[0][i].arg = abs( diff ) - 1;	/*  */
            pred_frame_num = h->fref0[i]->i_frame_num;	/*  */
        }
    }

    sh->i_cabac_init_idc = param->i_cabac_init_idc;	/* [毕厚杰：Page 167] 相当于 cabac_init_idc，给出cabac初始化时表格的选择，取值范围为[0,2] */

    sh->i_qp = i_qp;	/*  */
    sh->i_qp_delta = i_qp - pps->i_pic_init_qp;	/* [毕厚杰：P147 pic_init_qp_minus26] 亮度分量的量化参数的初始值 */
    sh->b_sp_for_swidth = 0;	/* [毕厚杰：Page 167] 相当于 sp_for_switch_flag */
    sh->i_qs_delta = 0;	/* [毕厚杰：Page 167] 相当于 slice_qs_delta，与slice_qp_delta语义相似，用在SI和SP中，由下式计算：... */

    /* 如果实际的qp<=15 ,解决将没有任何效果 If effective有效的,实际的 qp <= 15, deblocking解块 would将 have no没有 effect效果 anyway */
    if( param->b_deblocking_filter
        && ( h->mb.b_variable_qp
        || 15 < i_qp + 2 * X264_MAX(param->i_deblocking_filter_alphac0, param->i_deblocking_filter_beta) ) )
    {
        sh->i_disable_deblocking_filter_idc = 0;	/* [毕厚杰：Page 167] 相当于 disable_deblocking_filter_idc，H.264规定一套可以在解码器端独立地计算图像中各边界的滤波强度进行 */
    }		//disable:禁用
    else
    {
        sh->i_disable_deblocking_filter_idc = 1;	/* 去看 H.264 协议 200503 版本 8.7 小节 */
    }
    sh->i_alpha_c0_offset = param->i_deblocking_filter_alphac0 << 1;	/*  */
    sh->i_beta_offset = param->i_deblocking_filter_beta << 1;	/*  */
}
/*
简单说：
http://bbs.chinavideo.org/viewthread.php?tid=10671
disable_deblocking_filter_idc = 0，所有边界全滤波
disable_deblocking_filter_idc = 1，所有边界都不滤波
disable_deblocking_filter_idc = 2，片边界不滤波

*/


/*
 * 
   
*/
static void x264_slice_header_write( bs_t *s, x264_slice_header_t *sh, int i_nal_ref_idc )
{
    int i;	/*  */

    bs_write_ue( s, sh->i_first_mb );	/*  */
    bs_write_ue( s, sh->i_type + 5 );   /* same type things */
    bs_write_ue( s, sh->i_pps_id );	/*  */
    bs_write( s, sh->sps->i_log2_max_frame_num, sh->i_frame_num );	/*  */

    if( sh->i_idr_pic_id >= 0 ) /* NAL IDR */
    {
        bs_write_ue( s, sh->i_idr_pic_id );	/*  */
    }

    if( sh->sps->i_poc_type == 0 )	/*  */
    {
        bs_write( s, sh->sps->i_log2_max_poc_lsb, sh->i_poc_lsb );	/*  */
        if( sh->pps->b_pic_order && !sh->b_field_pic )	/*  */
        {
            bs_write_se( s, sh->i_delta_poc_bottom );	/*  */
        }
    }
    else if( sh->sps->i_poc_type == 1 && !sh->sps->b_delta_pic_order_always_zero )	/*  */
    {
        bs_write_se( s, sh->i_delta_poc[0] );	/*  */
        if( sh->pps->b_pic_order && !sh->b_field_pic )	/*  */
        {
            bs_write_se( s, sh->i_delta_poc[1] );	/*  */
        }
    }

    if( sh->pps->b_redundant_pic_cnt )	/*  */
    {
        bs_write_ue( s, sh->i_redundant_pic_cnt );	/*  */
    }

    if( sh->i_type == SLICE_TYPE_B )	/*  */
    {
        bs_write1( s, sh->b_direct_spatial_mv_pred );	/*  */
    }
    if( sh->i_type == SLICE_TYPE_P || sh->i_type == SLICE_TYPE_SP || sh->i_type == SLICE_TYPE_B )
    {
        bs_write1( s, sh->b_num_ref_idx_override );	/*  */
        if( sh->b_num_ref_idx_override )	/*  */
        {
            bs_write_ue( s, sh->i_num_ref_idx_l0_active - 1 );	/*  */
            if( sh->i_type == SLICE_TYPE_B )	/*  */
            {
                bs_write_ue( s, sh->i_num_ref_idx_l1_active - 1 );	/*  */
            }
        }
    }

    /* 引用图像列表重排序 ref pic list reordering */
    if( sh->i_type != SLICE_TYPE_I )	/* I片 */
    {
        bs_write1( s, sh->b_ref_pic_list_reordering_l0 );	/*  */
        if( sh->b_ref_pic_list_reordering_l0 )	/*  */
        {
            for( i = 0; i < sh->i_num_ref_idx_l0_active; i++ )	/*  */
            {
                bs_write_ue( s, sh->ref_pic_list_order[0][i].idc );	/*  */
                bs_write_ue( s, sh->ref_pic_list_order[0][i].arg );	/*  */
                        
            }
            bs_write_ue( s, 3 );	/*  */
        }
    }
    if( sh->i_type == SLICE_TYPE_B )	/* B片 */
    {
        bs_write1( s, sh->b_ref_pic_list_reordering_l1 );	/*  */
        if( sh->b_ref_pic_list_reordering_l1 )	/*  */
        {
            for( i = 0; i < sh->i_num_ref_idx_l1_active; i++ )	/*  */
            {
                bs_write_ue( s, sh->ref_pic_list_order[1][i].idc );	/*  */
                bs_write_ue( s, sh->ref_pic_list_order[1][i].arg );	/*  */
            }
            bs_write_ue( s, 3 );	/*  */
        }
    }

    if( ( sh->pps->b_weighted_pred && ( sh->i_type == SLICE_TYPE_P || sh->i_type == SLICE_TYPE_SP ) ) ||
        ( sh->pps->b_weighted_bipred == 1 && sh->i_type == SLICE_TYPE_B ) )	/*  */
    {
        /* FIXME 修理自我*/
    }

    if( i_nal_ref_idc != 0 )	/* [毕厚杰 159页] nal_ref_idc，指示当前NAL的优先级，取值范围[0,1,2,3]，值越高，表示当前NAL越重要，越需要优先受到保护，H.264规定如果当前NAL是一个序列参数集，或者是一个图像参数集，或者属于参考图像的片或片分区等重要的数据单位时，本句法元素必须大于0。但是大于0时具体该取何值，并没有进一步的规定，通信双方可以灵活地制定策略。当nal_unit_type等于5时，nal_ref_idc大于0；nal_unit_type等于6、9、10、11或12时，nal_ref_idc等于0。毕厚杰159页nal_ref_idc，指示当前NAL的优先级，取值范围[0,1,2,3]，值越高，表示当前NAL越重要，越需要优先受到保护，H.264规定如果当前NAL是一个序列参数集，或者是一个图像参数集，或者属于参考图像的片或片分区等重要的数据单位时，本句法元素必须大于0。但是大于0时具体该取何值，并没有进一步的规定，通信双方可以灵活地制定策略。当nal_unit_type等于5时，nal_ref_idc大于0；nal_unit_type等于6、9、10、11或12时，nal_ref_idc等于0。 */
    {
        if( sh->i_idr_pic_id >= 0 )	/* [毕厚杰：Page 164] 相当于idr_pic_id ;IDR图像的标识。不同的IDR图像有不同的idr_pic_id值。值得注意的是，IDR图像不等价于I图像，只有在作为IDR图像的I帧才有这个句法元素。在场模式下，IDR帧的两个场有相同的idr_pic_id值。idr_pic_id的取值范围是[0,65536]，和frame_num类似，当它的值超过这个范围时，它会以循环的方式重新开始计数。  */
        {
            bs_write1( s, 0 );  /* 没有输出优先图像标志 no output of prior优先的 pics flag */
            bs_write1( s, 0 );  /* 长期参考标志 long term reference flag */
        }
        else
        {
            bs_write1( s, 0 );  /* adaptive_ref_pic_marking_mode_flag 适应_参考_图像_标记_模式_标志*/
        }
    }

    if( sh->pps->b_cabac && sh->i_type != SLICE_TYPE_I )	/* 图像参数集b_cacbc不为0 && I条带 */
    {
        bs_write_ue( s, sh->i_cabac_init_idc );	/*  */
    }
    bs_write_se( s, sh->i_qp_delta );      /* slice qp delta */

    if( sh->pps->b_deblocking_filter_control )	/*  */
    {
        bs_write_ue( s, sh->i_disable_deblocking_filter_idc );	/*  */
        if( sh->i_disable_deblocking_filter_idc != 1 )	/*  */
        {
            bs_write_se( s, sh->i_alpha_c0_offset >> 1 );	/*  */
            bs_write_se( s, sh->i_beta_offset >> 1 );	/*  */
        }
    }
}

/****************************************************************************
 *
 ****************************************************************************
 ****************************** External(外界的) API*************************
 ****************************************************************************
 *
 ****************************************************************************/

static int x264_validate_parameters( x264_t *h )//检查结构体是否有效并试图校正错误
{
    if( h->param.i_width <= 0 || h->param.i_height <= 0 )	/* 编码器h.param. 宽 或 高<=0 */
    {
        x264_log( h, X264_LOG_ERROR, "invalid width x height (%dx%d)\n",	/* 错误日志:错误的宽x高 */
                  h->param.i_width, h->param.i_height );	/*  */
        return -1;	//退出
    }

    if( h->param.i_width % 2 || h->param.i_height % 2 )	/*  */
    {
        x264_log( h, X264_LOG_ERROR, "width or height not divisible可除尽的 by 2 (%dx%d)\n",
                  h->param.i_width, h->param.i_height );	/* 宽或高不能被2整除 */
        return -1;
    }
    if( h->param.i_csp != X264_CSP_I420 )
    {
        x264_log( h, X264_LOG_ERROR, "invalid CSP (only I420 supported)\n" );	/*  */
        return -1;
    }

    if( h->param.i_threads == 0 )//如果这个参数是0，设置成和cpu个数一样
        h->param.i_threads = x264_cpu_num_processors();	/* 返回cpu个数，赋给i_threads */
    h->param.i_threads = x264_clip3( h->param.i_threads, 1, X264_SLICE_MAX );	/* v在区间[i_min,i_max]的左边时，返回区间左边界,在区间右边时，返回区间右边界，其它情况下，返回v */
    h->param.i_threads = X264_MIN( h->param.i_threads, (h->param.i_height + 15) / 16 );	/*  */
#ifndef HAVE_PTHREAD//如果没有定义HAVE_PTHREAD
    if( h->param.i_threads > 1 )	/*  */
    {
		//不能用多线程支持编译
        x264_log( h, X264_LOG_WARNING, "not compiled with pthread support!\n");
        x264_log( h, X264_LOG_WARNING, "multislicing anyway, but you won't see any speed gain.\n" );
    }
#endif

    if( h->param.rc.i_rc_method < 0 || h->param.rc.i_rc_method > 2 )	/*  */
    {
        x264_log( h, X264_LOG_ERROR, "invalid RC method\n" );	/*  */
        return -1;
    }
    h->param.rc.i_rf_constant = x264_clip3( h->param.rc.i_rf_constant, 0, 51 );	/*  */
    h->param.rc.i_qp_constant = x264_clip3( h->param.rc.i_qp_constant, 0, 51 );	/*  */
    if( h->param.rc.i_rc_method == X264_RC_CRF )	/*  */
        h->param.rc.i_qp_constant = h->param.rc.i_rf_constant;	/*  */
    if( (h->param.rc.i_rc_method == X264_RC_CQP || h->param.rc.i_rc_method == X264_RC_CRF)	/*  */
        && h->param.rc.i_qp_constant == 0 )	/*  */
    {
        h->mb.b_lossless = 1;	/*  */
        h->param.i_cqm_preset = X264_CQM_FLAT;	/*  */
        h->param.psz_cqm_file = NULL;	/*  */
        h->param.rc.i_rc_method = X264_RC_CQP;	/*  */
        h->param.rc.f_ip_factor = 1;	/*  */
        h->param.rc.f_pb_factor = 1;	/*  */
        h->param.analyse.b_transform_8x8 = 0;	/*  */
        h->param.analyse.b_psnr = 0;	/*  */
        h->param.analyse.i_chroma_qp_offset = 0;	/*  */
        h->param.analyse.i_trellis = 0;	/*  */
        h->param.analyse.b_fast_pskip = 0;	/*  */
        h->param.analyse.i_noise_reduction = 0;	/*  */
        h->param.analyse.i_subpel_refine = x264_clip3( h->param.analyse.i_subpel_refine, 1, 6 );	/*  */
    }

    if( ( h->param.i_width % 16 || h->param.i_height % 16 ) && !h->mb.b_lossless )	/*  */
    {
        x264_log( h, X264_LOG_WARNING, 	/*  */
                  "width or height not divisible by 16 (%dx%d), compression will suffer.\n",	/*  */
                  h->param.i_width, h->param.i_height );	/*  */
    }

    h->param.i_frame_reference = x264_clip3( h->param.i_frame_reference, 1, 16 );	/*  */
    if( h->param.i_keyint_max <= 0 )	/*  */
        h->param.i_keyint_max = 1;	/*  */
    h->param.i_keyint_min = x264_clip3( h->param.i_keyint_min, 1, h->param.i_keyint_max/2+1 );

    h->param.i_bframe = x264_clip3( h->param.i_bframe, 0, X264_BFRAME_MAX );
    h->param.i_bframe_bias = x264_clip3( h->param.i_bframe_bias, -90, 100 );
    h->param.b_bframe_pyramid = h->param.b_bframe_pyramid && h->param.i_bframe > 1;
    h->param.b_bframe_adaptive = h->param.b_bframe_adaptive && h->param.i_bframe > 0;
    h->param.analyse.b_weighted_bipred = h->param.analyse.b_weighted_bipred && h->param.i_bframe > 0;
    h->mb.b_direct_auto_write = h->param.analyse.i_direct_mv_pred == X264_DIRECT_PRED_AUTO
                                && h->param.i_bframe
                                && ( h->param.rc.b_stat_write || !h->param.rc.b_stat_read );

    h->param.i_deblocking_filter_alphac0 = x264_clip3( h->param.i_deblocking_filter_alphac0, -6, 6 );
    h->param.i_deblocking_filter_beta    = x264_clip3( h->param.i_deblocking_filter_beta, -6, 6 );

    h->param.i_cabac_init_idc = x264_clip3( h->param.i_cabac_init_idc, 0, 2 );

    if( h->param.i_cqm_preset < X264_CQM_FLAT || h->param.i_cqm_preset > X264_CQM_CUSTOM )
        h->param.i_cqm_preset = X264_CQM_FLAT;

    if( h->param.analyse.i_me_method < X264_ME_DIA ||
        h->param.analyse.i_me_method > X264_ME_ESA )
        h->param.analyse.i_me_method = X264_ME_HEX;
    if( h->param.analyse.i_me_range < 4 )
        h->param.analyse.i_me_range = 4;
    if( h->param.analyse.i_me_range > 16 && h->param.analyse.i_me_method <= X264_ME_HEX )
        h->param.analyse.i_me_range = 16;
    h->param.analyse.i_subpel_refine = x264_clip3( h->param.analyse.i_subpel_refine, 1, 7 );
    h->param.analyse.b_bframe_rdo = h->param.analyse.b_bframe_rdo && h->param.analyse.i_subpel_refine >= 6;
    h->param.analyse.b_mixed_references = h->param.analyse.b_mixed_references && h->param.i_frame_reference > 1;
    h->param.analyse.inter &= X264_ANALYSE_PSUB16x16|X264_ANALYSE_PSUB8x8|X264_ANALYSE_BSUB16x16|
                              X264_ANALYSE_I4x4|X264_ANALYSE_I8x8;
    h->param.analyse.intra &= X264_ANALYSE_I4x4|X264_ANALYSE_I8x8;
    if( !(h->param.analyse.inter & X264_ANALYSE_PSUB16x16) )
        h->param.analyse.inter &= ~X264_ANALYSE_PSUB8x8;
    if( !h->param.analyse.b_transform_8x8 )
    {
        h->param.analyse.inter &= ~X264_ANALYSE_I8x8;
        h->param.analyse.intra &= ~X264_ANALYSE_I8x8;
    }
    h->param.analyse.i_chroma_qp_offset = x264_clip3(h->param.analyse.i_chroma_qp_offset, -12, 12);
    if( !h->param.b_cabac )
        h->param.analyse.i_trellis = 0;
    h->param.analyse.i_trellis = x264_clip3( h->param.analyse.i_trellis, 0, 2 );
    h->param.analyse.i_noise_reduction = x264_clip3( h->param.analyse.i_noise_reduction, 0, 1<<16 );

    {
        const x264_level_t *l = x264_levels;
        while( l->level_idc != 0 && l->level_idc != h->param.i_level_idc )
            l++;
        if( l->level_idc == 0 )
        {
            x264_log( h, X264_LOG_ERROR, "invalid level_idc: %d\n", h->param.i_level_idc );
            return -1;
        }
        if( h->param.analyse.i_mv_range <= 0 )
            h->param.analyse.i_mv_range = l->mv_range;
        else
            h->param.analyse.i_mv_range = x264_clip3(h->param.analyse.i_mv_range, 32, 2048);
    }

    if( h->param.rc.f_qblur < 0 )
        h->param.rc.f_qblur = 0;
    if( h->param.rc.f_complexity_blur < 0 )
        h->param.rc.f_complexity_blur = 0;

    h->param.i_sps_id &= 31;

    /* ensure the booleans are 0 or 1 so they can be used in math */
#define BOOLIFY(x) h->param.x = !!h->param.x
    BOOLIFY( b_cabac );
    BOOLIFY( b_deblocking_filter );
    BOOLIFY( analyse.b_transform_8x8 );
    BOOLIFY( analyse.b_bidir_me );
    BOOLIFY( analyse.b_chroma_me );
    BOOLIFY( analyse.b_fast_pskip );
    BOOLIFY( rc.b_stat_write );
    BOOLIFY( rc.b_stat_read );
#undef BOOLIFY

    return 0;
}

/****************************************************************************
 * x264_encoder_open:
 * 创建x264编码器并读入所有编码器参数。这个函数是对不正确的参数进行修改,并对各结构体参数和cabac 编码,预测等需要的参数进行初始化
 ****************************************************************************/
x264_t *x264_encoder_open   ( x264_param_t *param )
{
    x264_t *h = x264_malloc( sizeof( x264_t ) );	/* 分配空间并进行初始化,x264_malloc( )在common.c中 */
    int i;

    memset( h, 0, sizeof( x264_t ) );	/* 初始化动态分配的内存空间 */

    /* 建立一个参数的拷贝 Create a copy of param */
    memcpy( &h->param, param, sizeof( x264_param_t ) );	/* 把传进来的参数拷贝一份到动态分配的内存里 */

    if( x264_validate_parameters( h ) < 0 )	/* 函数x264_validate_parameters( h )在encoder.c中,功能为判断参数是否有效，并对不合适的参数进行修改 */
    {
        x264_free( h );	/*  */
        return NULL;
    }//validate:确认,使有效

    if( h->param.psz_cqm_file )	/* 一个字符串(char *)，指定一个文件 */
        if( x264_cqm_parse_file( h, h->param.psz_cqm_file ) < 0 )	/* 解析这个文件，取到的一些值用在h的字段里 */
        {
            x264_free( h );	/* 解析文件失败，销毁编码器 */
            return NULL;
        }

	/* x264_param_t/rc子结构 */
    if( h->param.rc.psz_stat_out )	/* 字符串(char *) */
        h->param.rc.psz_stat_out = strdup( h->param.rc.psz_stat_out );	/*  */
    if( h->param.rc.psz_stat_in )	/* 字符串(char *) */
        h->param.rc.psz_stat_in = strdup( h->param.rc.psz_stat_in );	/*  */
    if( h->param.rc.psz_rc_eq )		/* 字符串(char *) */
        h->param.rc.psz_rc_eq = strdup( h->param.rc.psz_rc_eq );	/*  */
		/*
		头文件：#include <string.h>
	　　用法：char *strdup(char *s);
		功能：复制字符串s
		说明：返回指向被复制的字符串的指针(新字符串)，所需空间由malloc()分配且可以由free()释放。
		例子：
		char *s="Golden Global View";
		char *d;
		clrscr();
		d=strdup(s);
		printf("%s",d);
		free(d); 			  
		*/


    /* VUI */
    if( h->param.vui.i_sar_width > 0 && h->param.vui.i_sar_height > 0 )	/*  */
    {
        int i_w = param->vui.i_sar_width;	/*  */
        int i_h = param->vui.i_sar_height;

        x264_reduce_fraction( &i_w, &i_h );	/* 分数化简,common.c */

        while( i_w > 65535 || i_h > 65535 ) /* 65536是2的16次方 */
        {
            i_w /= 2; /*  */
            i_h /= 2; /*  */
        }

        h->param.vui.i_sar_width = 0; /* 赋值0 */
        h->param.vui.i_sar_height = 0; /* 赋值0  */
        if( i_w == 0 || i_h == 0 ) /*  */
        {
            x264_log( h, X264_LOG_WARNING, "cannot create valid(有效的) sample aspect(方面) ratio\n" );	/* 日志 */
        }
        else
        {
            x264_log( h, X264_LOG_INFO, "using SAR=%d/%d\n", i_w, i_h );	/* 日志 */
            h->param.vui.i_sar_width = i_w; /* 赋值 */
            h->param.vui.i_sar_height = i_h; /* 赋值 */
        }
    }

    x264_reduce_fraction( &h->param.i_fps_num, &h->param.i_fps_den );	/* 分数化简,common.c */
																		/* i_fps_num:视频数据帧率 i_fps_den:用两个整型的数的比值，来表示帧率 common.c */
    /* 初始化x264_t结构体  Init x264_t */
	/* out子结构:字节流输出 */
    h->out.i_nal = 0;
    h->out.i_bitstream = X264_MAX( 1000000, h->param.i_width * h->param.i_height * 1.7
        * ( h->param.rc.i_rc_method == X264_RC_ABR ? pow( 0.5, h->param.rc.i_qp_min )
          : pow( 0.5, h->param.rc.i_qp_constant ) * X264_MAX( 1, h->param.rc.f_ip_factor )));/* 求最大值 */

	//动态分配内存
    h->out.p_bitstream = x264_malloc( h->out.i_bitstream );/* 存放整个nal */

    h->i_frame = 0;
    h->i_frame_num = 0;
    h->i_idr_pic_id = 0;

	//序列参数集h->sps初始化
    h->sps = &h->sps_array[0];
    x264_sps_init( h->sps, h->param.i_sps_id, &h->param );

    h->pps = &h->pps_array[0];
    x264_pps_init( h->pps, h->param.i_sps_id, &h->param, h->sps);

    x264_validate_levels( h );

    x264_cqm_init( h );
    
    h->mb.i_mb_count = h->sps->i_mb_width * h->sps->i_mb_height;

    /* 初始化帧 Init frames. */
    h->frames.i_delay = h->param.i_bframe;
    h->frames.i_max_ref0 = h->param.i_frame_reference;
    h->frames.i_max_ref1 = h->sps->vui.i_num_reorder_frames;
    h->frames.i_max_dpb  = h->sps->vui.i_max_dec_frame_buffering + 1;
    h->frames.b_have_lowres = !h->param.rc.b_stat_read
        && ( h->param.rc.i_rc_method == X264_RC_ABR
          || h->param.rc.i_rc_method == X264_RC_CRF
          || h->param.b_bframe_adaptive );

    for( i = 0; i < X264_BFRAME_MAX + 3; i++ )
    {
        h->frames.current[i] = NULL;
        h->frames.next[i]    = NULL;
        h->frames.unused[i]  = NULL;
    }
    for( i = 0; i < 1 + h->frames.i_delay; i++ )//delay:延迟
    {
        h->frames.unused[i] =  x264_frame_new( h );
        if( !h->frames.unused[i] )
            return NULL;
    }
    for( i = 0; i < h->frames.i_max_dpb; i++ )
    {
        h->frames.reference[i] = x264_frame_new( h );//reference:参考
        if( !h->frames.reference[i] )
            return NULL;
    }
    h->frames.reference[h->frames.i_max_dpb] = NULL;
    h->frames.i_last_idr = - h->param.i_keyint_max;
    h->frames.i_input    = 0;
    h->frames.last_nonb  = NULL;

    h->i_ref0 = 0;
    h->i_ref1 = 0;

    h->fdec = h->frames.reference[0];

    if( x264_macroblock_cache_init( h ) < 0 )//宏块缓冲初始化？
        return NULL;
    x264_rdo_init( );	/*  */	//rdo:率失真优化策略：毕厚杰:P87

    /* init CPU functions */
    x264_predict_16x16_init( h->param.cpu, h->predict_16x16 );	/*  */
    x264_predict_8x8c_init( h->param.cpu, h->predict_8x8c );	/*  */
    x264_predict_8x8_init( h->param.cpu, h->predict_8x8 );		/*  */
    x264_predict_4x4_init( h->param.cpu, h->predict_4x4 );		/*  */

    x264_pixel_init( h->param.cpu, &h->pixf );		/*  */
    x264_dct_init( h->param.cpu, &h->dctf );		/*  */
    x264_mc_init( h->param.cpu, &h->mc );			/*  */
    x264_csp_init( h->param.cpu, h->param.i_csp, &h->csp );	/*  */
    x264_quant_init( h, h->param.cpu, &h->quantf );			/*  */
    x264_deblock_init( h->param.cpu, &h->loopf );			/*  */

    memcpy( h->pixf.mbcmp,
            ( h->mb.b_lossless || h->param.analyse.i_subpel_refine <= 1 ) ? h->pixf.sad : h->pixf.satd,
            sizeof(h->pixf.mbcmp) );	/*  */

    /* 速度控制 rate control */
    if( x264_ratecontrol_new( h ) < 0 )
        return NULL;

    x264_log( h, X264_LOG_INFO, "using cpu capabilities %s%s%s%s%s%s\n",
             param->cpu&X264_CPU_MMX ? "MMX " : "",
             param->cpu&X264_CPU_MMXEXT ? "MMXEXT " : "",
             param->cpu&X264_CPU_SSE ? "SSE " : "",
             param->cpu&X264_CPU_SSE2 ? "SSE2 " : "",
             param->cpu&X264_CPU_3DNOW ? "3DNow! " : "",
             param->cpu&X264_CPU_ALTIVEC ? "Altivec " : "" );

    h->thread[0] = h;
    h->i_thread_num = 0;
    for( i = 1; i < h->param.i_threads; i++ )
        h->thread[i] = x264_malloc( sizeof(x264_t) );//为其它线程创建x264_t对象，但它们的值在哪儿设置呢？

	#ifdef DEBUG_DUMP_FRAME		//DUMP:转储,倾倒
    {
        /* create or truncate(截短) the reconstructed(重建的) video file */
        FILE *f = fopen( "fdec.yuv", "w" );
        if( f )
            fclose( f );
        else
        {
            x264_log( h, X264_LOG_ERROR, "can't write to fdec.yuv\n" );
            x264_free( h );
            return NULL;
        }
    }
	#endif

    return h;
}

/****************************************************************************
 * x264_encoder_reconfig:
 ****************************************************************************/
int x264_encoder_reconfig( x264_t *h, x264_param_t *param )
{
    h->param.i_bframe_bias = param->i_bframe_bias;
    h->param.i_deblocking_filter_alphac0 = param->i_deblocking_filter_alphac0;
    h->param.i_deblocking_filter_beta    = param->i_deblocking_filter_beta;
    h->param.analyse.i_me_method = param->analyse.i_me_method;
    h->param.analyse.i_me_range = param->analyse.i_me_range;
    h->param.analyse.i_subpel_refine = param->analyse.i_subpel_refine;
    h->param.analyse.i_trellis = param->analyse.i_trellis;
    h->param.analyse.intra = param->analyse.intra;
    h->param.analyse.inter = param->analyse.inter;

    memcpy( h->pixf.mbcmp,
            ( h->mb.b_lossless || h->param.analyse.i_subpel_refine <= 1 ) ? h->pixf.sad : h->pixf.satd,
            sizeof(h->pixf.mbcmp) );

    return x264_validate_parameters( h );
}

/* internal内部的 usage用法,习惯 
 * 
*/
static void x264_nal_start( x264_t *h, int i_type, int i_ref_idc )
{
	//当前是第几个nal，一个图像可能产生好几个nal
    x264_nal_t *nal = &h->out.nal[h->out.i_nal];//x264_nal_t  nal[X264_NAL_MAX]; 

	//操作的实际是&h->out.nal[h->out.i_nal]
    nal->i_ref_idc = i_ref_idc;		//优先级
    nal->i_type    = i_type;		//类型

    bs_align_0( &h->out.bs );   /* not needed */

    nal->i_payload= 0;//payload:有效载荷长度
    nal->p_payload= &h->out.p_bitstream[bs_pos( &h->out.bs) / 8];//起始地址  bitstream:比特流
										//bs_pos:获取当前流读取到哪个位置了，可能的返回值是，105、106
}

static void x264_nal_end( x264_t *h )
{
    x264_nal_t *nal = &h->out.nal[h->out.i_nal];

    bs_align_0( &h->out.bs );   /* not needed */

    nal->i_payload = &h->out.p_bitstream[bs_pos( &h->out.bs)/8] - nal->p_payload;////bitstream:比特流  payload:有效载荷

    h->out.i_nal++;
}

/****************************************************************************
 * x264_encoder_headers:
 * 这个函数没用，把内部全注释掉仍可正常编译，而且搜索后也未发现有调用它的地方
 * 用这个能生成PPS和SPS，是不是在需要时调用它然后往外发，好像是可以的
 ****************************************************************************/
int x264_encoder_headers( x264_t *h, x264_nal_t **pp_nal, int *pi_nal )
{
    /* init bitstream(比特流) context(上下文,背景) */
    h->out.i_nal = 0;
    bs_init( &h->out.bs, h->out.p_bitstream, h->out.i_bitstream );

    /* Put SPS(序列参数集) and PPS(图像参数集) */
    if( h->i_frame == 0 )
    {
        /* identify(识别,支持) ourself(自我) */
        x264_nal_start( h, NAL_SEI, NAL_PRIORITY_DISPOSABLE );
        x264_sei_version_write( h, &h->out.bs );//SEI_版本_写入
        x264_nal_end( h );	//

        /* generate(生成) sequence(先后次序, 顺序, 连续) parameters(参数) */
		//这儿应该是生成序列参数集SPS
        x264_nal_start( h, NAL_SPS, NAL_PRIORITY_HIGHEST );	//
        x264_sps_write( &h->out.bs, h->sps );	//序列参数集_写入
        x264_nal_end( h );	//

        /* generate(生成) picture parameters(生成图像参数集PPS) */
        x264_nal_start( h, NAL_PPS, NAL_PRIORITY_HIGHEST );	/* PRIORITY:优先权 HIGHEST:最高的 */
        x264_pps_write( &h->out.bs, h->pps );	//图像参数集_写入
        x264_nal_end( h );
    }
    /* 设置输出 now set output */
    *pi_nal = h->out.i_nal;
    *pp_nal = &h->out.nal[0];

    return 0;
}

/*
 * 名称：
 * 功能：->栈，把一帧放到list参考链表，跟在后面，新进来的放在list[i]，其中i不一定为0
 * 参数：
 * 返回：
 * 资料：
 * 思路：
 *  i
 * [0]  
 * [1]
 * [2]
 * [3]
 * [4] <---------新结点
*/
static void x264_frame_put( x264_frame_t *list[X264_BFRAME_MAX], x264_frame_t *frame )//->栈，把一帧放到list参考链表，跟在后面，新进来的放在list[i]，其中i不一定为0
{
    int i = 0;
    while( list[i] ) i++;
    list[i] = frame;
}

/*
 * 名称：
 * 功能：push:压栈,推, 推动，移动；新进来的放在list[0]
 * 参数：
 * 返回：
 * 资料：
 * 思路：
 *  i
 * [0] <---------新结点
 * [1]
 * [2]
 * [3]
 * [4]
*/
static void x264_frame_push( x264_frame_t *list[X264_BFRAME_MAX], x264_frame_t *frame )
{
    int i = 0;
    while( list[i] ) i++;	//找到i的最大值
    while( i-- )
        list[i+1] = list[i];		//把链表结点向下移，即把第i个结点移到第i+1个结点，这样就把最栈顶空出来了
    list[0] = frame;				//栈顶放新提供的指针(指向一个结构体对象)
}

/*
 * 名称：
 * 功能：取链表里取第0个元素，就是第0帧
 * 参数：
 * 返回：
 * 资料：#define X264_BFRAME_MAX 16
 * 思路：
 * 
*/
static x264_frame_t *x264_frame_get( x264_frame_t *list[X264_BFRAME_MAX+1] )//取链表里取第0个元素，就是第0帧
{
    x264_frame_t *frame = list[0];	/* 取链表里取第0个元素，就是第0帧 */
    int i;
    for( i = 0; list[i]; i++ )//for(i = 0 ; list[0]; i++)//感觉是先取出第0帧，然后其它帧往前移一下
        list[i] = list[i+1];//后一个前移
    return frame;	//返回list[0]
}


/*
 * 名称：
 * 功能：
 * 参数：第一个参数是帧结构类型，第二个参数是0或1，指明是否是...，只用在一个地方：
		 #define x264_frame_sort_dts(list) x264_frame_sort(list, 1)
		 #define x264_frame_sort_pts(list) x264_frame_sort(list, 0)
 * 返回：
 * 资料：sort:分类,排序,把…排序
 * 思路：
 * 
*/
static void x264_frame_sort( x264_frame_t *list[X264_BFRAME_MAX+1], int b_dts )
{
    int i, b_ok;
    do {
        b_ok = 1;
        for( i = 0; list[i+1]; i++ )
        {
            int dtype = list[i]->i_type - list[i+1]->i_type;
            int dtime = list[i]->i_frame - list[i+1]->i_frame;
            int swap = b_dts ? dtype > 0 || ( dtype == 0 && dtime > 0 )
                             : dtime > 0;
            if( swap )
            {
                XCHG( x264_frame_t*, list[i], list[i+1] );////a和b的值互换,在common.h里定义(#define XCHG(type,a,b) { type t = a; a = b; b = t; }//交换a和b)
                b_ok = 0;
            }
        }
    } while( !b_ok );
}
#define x264_frame_sort_dts(list) x264_frame_sort(list, 1)
#define x264_frame_sort_pts(list) x264_frame_sort(list, 0)

/*
reference:参考
build:开发, 创建
参考_建立_列表

Encode_frame(...)调用x264_encoder_encode(...),x264_encoder_encode(...)再调用x264_reference_build_list(...)
资料：http://wmnmtm.blog.163.com/blog/static/38245714201181234153295/
main()中定义一个x264_param_t对象param，命令行选项--keyint的值如2或5... ，被存进了param的一个字段里x264_param_t.i_keyint_max 

所以本函数根据指定的参考帧个数，创建它的参考帧列表，如果keyint指定了5，里面的for就循环5次
好像不是keyint，而是--ref http://wmnmtm.blog.163.com/blog/static/3824571420118191170628/
keyint：设定x264输出的资料流之最大IDR帧（亦称为关键帧）间隔,默认值：250 

在JM里面的参考帧是分前向和后向参考的，list0是用来前向参考的，list1是后向参考的，对每一个list里面放的参考帧，又分短期和长期参考，短期是按降序排列，长期是按升序排列的
这里有没有考虑短期和长期参考的问题了，只是将参考帧的等级设为HIGHEST或DISPOSABLE(一次性的)

*/
static inline void x264_reference_build_list( x264_t *h, int i_poc, int i_slice_type )
{
    int i;
    int b_ok;

    /* build ref list 0/1 建立参考列表0和参考列表1*/
    h->i_ref0 = 0;
    h->i_ref1 = 0;

	
    for( i = 1; i < h->frames.i_max_dpb; i++ )	/* i_max_dpb图像缓冲区中分配的帧的数量 --ref 参数设置 */
    {	/* 在命令行用 --ref n 指定几个参考帧，就循环几次 */
        if( h->frames.reference[i]->i_poc >= 0 )		//x264_t结构体的frames子结构中：x264_frame_t *reference[16+2+1+2]
        {//注意这个数组已经可能被使用过
            if( h->frames.reference[i]->i_poc < i_poc )	//如果传入的i_poc，即参考帧i比当前帧的播放顺序小，reference:参考
            {
				//放入列表0
				//list0 ?
                h->fref0[h->i_ref0++] = h->frames.reference[i];	//list0//x264 --crf 22 --ref 6 -o test.264 hall_cif.yuv 352x288，此命令时，只执行这句
				//printf("a\n");//zjh
            }
            else if( h->frames.reference[i]->i_poc > i_poc )	//frames;//指示和控制帧编码过程的结构
            {
				//list1 ?
                h->fref1[h->i_ref1++] = h->frames.reference[i];	//list1
				//printf("b\n");//zjh
            }
        }
//		printf(" h->fenc->i_frame=%d,  i_ref0=%d,  i_ref1=%d \n",h->fenc->i_frame,h->i_ref0,h->i_ref1);//zjh//http://wmnmtm.blog.163.com/blog/static/38245714201181234153295/
		//printf("           i_ref1=%d  \n",h->fenc->i_frame,h->i_ref1);//zjh
    }

    /* 
	Order ref0 from higher to lower poc 
	排序参考0，按POC从高到底排列 
	相当于JM里面的list0，将ref0中的参考帧按照POC进行排序
	*/
    do
    {
        b_ok = 1;
        for( i = 0; i < h->i_ref0 - 1; i++ )//i_ref0在某个地方应该会被不断的更新，这个应该是参考队列里的结点数
        {
            if( h->fref0[i]->i_poc < h->fref0[i+1]->i_poc )//现在这个和下一个的i_poc比较
            {
                XCHG( x264_frame_t*, h->fref0[i], h->fref0[i+1] );//交换a和b:XCHG(type,a,b) { type t = a; a = b; b = t; }//交换a和b
                b_ok = 0;
                break;
            }
        }
    } while( !b_ok );
    /* 
	Order ref1 from lower to higher poc (bubble泡,水泡,气泡 sort排序 [冒泡排序] ) for B-frame (B帧)
	排列参考1，按POC从低到高对B-帧
	*/
    do
    {
        b_ok = 1;
        for( i = 0; i < h->i_ref1 - 1; i++ )
        {
            if( h->fref1[i]->i_poc > h->fref1[i+1]->i_poc )
            {
                XCHG( x264_frame_t*, h->fref1[i], h->fref1[i+1] );//交换a和b:XCHG(type,a,b),common.h里定义，实现
                b_ok = 0;
                break;
            }
        }
    } while( !b_ok );

    /* In the standard, a P-frame's ref list is sorted排序的 by frame_num.
     * We use POC, but check检查 whether是否 explicit明确的 reordering is needed必要 
	 * 在标准里，一个p帧的参考队列是通过frame_num排序的
	 * 我们使用POC，但是检查是不时确确的排序了仍然是必要的
	 * 所以用如下的一个有二个元素的数组来保存这个状态
	*/
    h->b_ref_reorder[0] =
    h->b_ref_reorder[1] = 0;
    if( i_slice_type == SLICE_TYPE_P )
    {
        for( i = 0; i < h->i_ref0 - 1; i++ )
            if( h->fref0[i]->i_frame_num < h->fref0[i+1]->i_frame_num )
            {
                h->b_ref_reorder[0] = 1;
                break;
            }
    }

    h->i_ref1 = X264_MIN( h->i_ref1, h->frames.i_max_ref1 );//求最小值(参考1)
    h->i_ref0 = X264_MIN( h->i_ref0, h->frames.i_max_ref0 );//求最小值(参考0)
    h->i_ref0 = X264_MIN( h->i_ref0, 16 - h->i_ref1 );
}

/*
deblock:解块
deblocking:解块
disable:使无效
*/
static inline void x264_fdec_deblock( x264_t *h )//
{
    /* 
	apply(使用) deblocking(去块) filter(滤波器) to the current(当前) decoded(解码) picture(图像) 
	使用去块滤波器，对当前解码的图像
	*/
    if( !h->sh.i_disable_deblocking_filter_idc )//
    {
        TIMER_START( i_mtime_filter );
        x264_frame_deblocking_filter( h, h->sh.i_type );//帧去块滤波主函数
        TIMER_STOP( i_mtime_filter );
    }
}


/*
 * 名称：x264_reference_update
 * 功能：最主要的工作是将上一个参考帧放入参考帧队列，
		 并从空闲帧队列中取出一帧作为当前的参考工作帧（即解码操作的目的帧），即h->fdec。
		 它会在h->frames.reference 保留需要的参考帧，然后根据参考帧队列的大小限制，移除不使用的参考帧
 * 参数：
 * 返回：
 * 资料：
 * 思路：
 * 
*/
static inline void x264_reference_update( x264_t *h )
{
    int i;

    x264_fdec_deblock( h );

    /* expand border扩边 */
    x264_frame_expand_border( h->fdec );

    /* create filtered images */
    x264_frame_filter( h->param.cpu, h->fdec );

    /* expand border of filtered images */
    x264_frame_expand_border_filtered( h->fdec );

    /* move lowres copy of the image to the ref frame */
    for( i = 0; i < 4; i++)
        XCHG( uint8_t*, h->fdec->lowres[i], h->fenc->lowres[i] );

    /* adaptive B decision判断 needs a pointer, since自从…之后 it can't use the ref lists */
    if( h->sh.i_type != SLICE_TYPE_B )
        h->frames.last_nonb = h->fdec;

    /* move frame in the buffer */
    h->fdec = h->frames.reference[h->frames.i_max_dpb-1];
    for( i = h->frames.i_max_dpb-1; i > 0; i-- )
    {
        h->frames.reference[i] = h->frames.reference[i-1];
    }
    h->frames.reference[0] = h->fdec;//h->fdec:正在被重建的帧
}

/*
 * 名称：
 * 功能：
 * 参数：
 * 返回：
 * 资料：
 * 思路：全部参考帧的poc设为-1，把第一个设为0
 * 
*/
static inline void x264_reference_reset( x264_t *h )//reference:参考
{
    int i;

    /* 
	重设引用 reset ref pictures 
	分析来看，i_max_dpb是指重建帧的数量
	从那句英文翻译来看，i_max_dpb是在解码的图像缓冲区中分配的帧的数量，也是这个意思
	所以下面这个循环，就是遍历重建帧或者可参考帧
	*/
	/*
	printf("\n (filename:function) h->frames.i_max_dpb=%d",h->frames.i_max_dpb);//zjh
	printf("\n (filename:function) h->sps->vui.i_max_dec_frame_buffering=%d",h->sps->vui.i_max_dec_frame_buffering);//zjh
	当不采用--ref 时，输出2 ，当采用--ref 12 等时，输出的比指定值大1，--ref 16时，输出17 ,ref再大，也只输出17
	ref参数解释：控制解码图片缓冲（DPB：Decoded Picture Buffer）的大小。范围是从0到16。总之，此值是每个P帧可以使用先前多少帧作为参照帧的数目
	printf("\n\n");//zjh
	system("pause");//暂停，任意键继续	
	*/
    for( i = 1; i < h->frames.i_max_dpb;/* --ref 修改*/ i++ )	/* i_max_dpb:Number of frames allocated (分配) in the decoded picture buffer */
    {
        h->frames.reference[i]->i_poc = -1;	//全部参考帧的poc设为-1
    }
    h->frames.reference[0]->i_poc = 0;		//把第一个设为0 /* poc标识图像的播放顺序 ，毕厚杰，第160页 */
}
/*
 * 名称：
 * 功能：
 * 参数：
 * 返回：
 * 资料：
 * 思路：
 * 
*/
static inline void x264_slice_init( x264_t *h, int i_nal_type, int i_slice_type, int i_global_qp )
{
    /* ------------------------ Create slice header  建立片头 ----------------------- */
    if( i_nal_type == NAL_SLICE_IDR )
    {
        x264_slice_header_init( h, &h->sh, h->sps, h->pps, i_slice_type, h->i_idr_pic_id, h->i_frame_num, i_global_qp );

        /* increment id 增长 id */
        h->i_idr_pic_id = ( h->i_idr_pic_id + 1 ) % 65536;
    }
    else
    {
        x264_slice_header_init( h, &h->sh, h->sps, h->pps, i_slice_type, -1, h->i_frame_num, i_global_qp );

        /* always set the real higher num of ref frame used */
        h->sh.b_num_ref_idx_override = 1;
        h->sh.i_num_ref_idx_l0_active = h->i_ref0 <= 0 ? 1 : h->i_ref0;
        h->sh.i_num_ref_idx_l1_active = h->i_ref1 <= 0 ? 1 : h->i_ref1;
    }

    h->fdec->i_frame_num = h->sh.i_frame_num;

    if( h->sps->i_poc_type == 0 )
    {
        h->sh.i_poc_lsb = h->fdec->i_poc & ( (1 << h->sps->i_log2_max_poc_lsb) - 1 );
        h->sh.i_delta_poc_bottom = 0;   /* XXX won't work for field */
    }
    else if( h->sps->i_poc_type == 1 )
    {
        /* FIXME TODO FIXME */
    }
    else
    {
        /* Nothing to do ? */
    }

    x264_macroblock_slice_init( h );
}

/*
 * 名称：
 * 参数：
 * 返回：
 * 功能：
 * 思路：
 * 资料：
 * 
*/
static int x264_slice_write( x264_t *h )
{
    int i_skip;
    int mb_xy;
    int i;
	//printf("HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH");
    /* init stats */
    memset( &h->stat.frame, 0, sizeof(h->stat.frame) );

    /* Slice */
    x264_nal_start( h, h->i_nal_type, h->i_nal_ref_idc );	//设置优先级、类型、载荷长度、载荷起始地址

    /* Slice header */
    x264_slice_header_write( &h->out.bs, &h->sh, h->i_nal_ref_idc ); /* Slice header */
    if( h->param.b_cabac )
    {
        /* alignment needed */
        bs_align_1( &h->out.bs );

        /* init cabac */
        x264_cabac_context_init( &h->cabac, h->sh.i_type, h->sh.i_qp, h->sh.i_cabac_init_idc );//初始化上下文
        x264_cabac_encode_init ( &h->cabac, &h->out.bs );
    }
    h->mb.i_last_qp = h->sh.i_qp;
    h->mb.i_last_dqp = 0;

    for( mb_xy = h->sh.i_first_mb, i_skip = 0; mb_xy < h->sh.i_last_mb; mb_xy++ )
    {
        const int i_mb_y = mb_xy / h->sps->i_mb_width;
        const int i_mb_x = mb_xy % h->sps->i_mb_width;

        int mb_spos = bs_pos(&h->out.bs);

        /* load cache */
        x264_macroblock_cache_load( h, i_mb_x, i_mb_y );

        /* analyse parameters
         * Slice I: choose I_4x4 or I_16x16 mode
         * Slice P: choose between using P mode or intra (4x4 or 16x16)
         * */
        TIMER_START( i_mtime_analyse );
        x264_macroblock_analyse( h );
        TIMER_STOP( i_mtime_analyse );

        /* encode this macrobock -> be carefull it can change the mb type to P_SKIP if needed */
        TIMER_START( i_mtime_encode );
        x264_macroblock_encode( h );
        TIMER_STOP( i_mtime_encode );

        TIMER_START( i_mtime_write );
        if( h->param.b_cabac )
        {
            if( mb_xy > h->sh.i_first_mb )
                x264_cabac_encode_terminal( &h->cabac, 0 );

            if( IS_SKIP( h->mb.i_type ) )
                x264_cabac_mb_skip( h, 1 );
            else
            {
                if( h->sh.i_type != SLICE_TYPE_I )
                    x264_cabac_mb_skip( h, 0 );
                x264_macroblock_write_cabac( h, &h->cabac );
            }
        }
        else
        {
            if( IS_SKIP( h->mb.i_type ) )
                i_skip++;
            else
            {
                if( h->sh.i_type != SLICE_TYPE_I )
                {
                    bs_write_ue( &h->out.bs, i_skip );  /* skip run */
                    i_skip = 0;
                }
                x264_macroblock_write_cavlc( h, &h->out.bs );
            }
        }
        TIMER_STOP( i_mtime_write );

#if VISUALIZE
        if( h->param.b_visualize )
            x264_visualize_mb( h );
#endif

        /* save cache */
        x264_macroblock_cache_save( h );

        /* accumulate积累 mb stats */
        h->stat.frame.i_mb_count[h->mb.i_type]++;
        if( !IS_SKIP(h->mb.i_type) && !IS_INTRA(h->mb.i_type) && !IS_DIRECT(h->mb.i_type) )
        {
            if( h->mb.i_partition != D_8x8 )
                h->stat.frame.i_mb_count_size[ x264_mb_partition_pixel_table[ h->mb.i_partition ] ] += 4;
            else
                for( i = 0; i < 4; i++ )
                    h->stat.frame.i_mb_count_size[ x264_mb_partition_pixel_table[ h->mb.i_sub_partition[i] ] ] ++;
            if( h->param.i_frame_reference > 1 )
            {
                for( i = 0; i < 4; i++ )
                {
                    int i_ref = h->mb.cache.ref[0][ x264_scan8[4*i] ];
                    if( i_ref >= 0 )
                        h->stat.frame.i_mb_count_ref[i_ref] ++;
                }
            }
        }
        if( h->mb.i_cbp_luma && !IS_INTRA(h->mb.i_type) )
        {
            h->stat.frame.i_mb_count_8x8dct[0] ++;
            h->stat.frame.i_mb_count_8x8dct[1] += h->mb.b_transform_8x8;
        }

        if( h->mb.b_variable_qp )
            x264_ratecontrol_mb(h, bs_pos(&h->out.bs) - mb_spos);
    }

    if( h->param.b_cabac )
    {
        /* end of slice */
        x264_cabac_encode_terminal( &h->cabac, 1 );
    }
    else if( i_skip > 0 )
    {
        bs_write_ue( &h->out.bs, i_skip );  /* last skip run */
    }

    if( h->param.b_cabac )
    {
        x264_cabac_encode_flush( &h->cabac );

    }
    else
    {
        /* rbsp_slice_trailing_bits */
        bs_rbsp_trailing( &h->out.bs );	//拖尾句法
    }

    x264_nal_end( h );

    /* Compute计算 misc各种各样的；杂项的 bits */
    h->stat.frame.i_misc_bits = bs_pos( &h->out.bs )
                              + NALU_OVERHEAD * 8
                              - h->stat.frame.i_itex_bits
                              - h->stat.frame.i_ptex_bits
                              - h->stat.frame.i_hdr_bits;

    return 0;
}
/*
 * 名称：
 * 功能：
 * 参数：
 * 返回：
 * 资料：
 * 思路：
 * 
*/
static inline int x264_slices_write( x264_t *h )
{
    int i_frame_size;
	//printf("EEEEEEEEEEEEEEEEEEEEEEEEEE");
	#if VISUALIZE//形象化, 设想, 想像, 构想
		//printf("VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV");
		if( h->param.b_visualize )
			x264_visualize_init( h );
	#endif

	if( h->param.i_threads == 1 )//
	{
		x264_ratecontrol_threads_start( h );
		x264_slice_write( h );
		//printf("gfgfgfgfdfdfdfdfdfdfdfsfsfsfsdfdsfG");
		i_frame_size = h->out.nal[h->out.i_nal-1].i_payload;
	}
    else
    {
        int i_nal = h->out.i_nal;
        int i_bs_size = h->out.i_bitstream / h->param.i_threads;
        int i;
		
        /* duplicate复制 contexts上下文 */

        for( i = 0; i < h->param.i_threads; i++ )
        {
			
            x264_t *t = h->thread[i];
			//printf("GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG");
            if( i > 0 )
            {
                memcpy( t, h, sizeof(x264_t) );
                t->out.p_bitstream += i*i_bs_size;
                bs_init( &t->out.bs, t->out.p_bitstream, i_bs_size );
                t->i_thread_num = i;
            }
            t->sh.i_first_mb = (i    * h->sps->i_mb_height / h->param.i_threads) * h->sps->i_mb_width;
            t->sh.i_last_mb = ((i+1) * h->sps->i_mb_height / h->param.i_threads) * h->sps->i_mb_width;
            t->out.i_nal = i_nal + i;
        }
        x264_ratecontrol_threads_start( h );

        /* dispatch派遣调度 */
		#ifdef HAVE_PTHREAD
        {
            pthread_t handles[X264_SLICE_MAX];
			//printf("DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD");
            for( i = 0; i < h->param.i_threads; i++ )
                pthread_create( &handles[i], NULL, (void*)x264_slice_write, (void*)h->thread[i] );//创建其它线程，实际就是CreateThread(...)
            for( i = 0; i < h->param.i_threads; i++ )
                pthread_join( handles[i], NULL );
        }
		#else
			for( i = 0; i < h->param.i_threads; i++ )//
			{
				x264_slice_write( h->thread[i] );

			}
		#endif

        /* merge合并 contexts */
        i_frame_size = h->out.nal[i_nal].i_payload;
        for( i = 1; i < h->param.i_threads; i++ )
        {
            int j;
            x264_t *t = h->thread[i];
            h->out.nal[i_nal+i] = t->out.nal[i_nal+i];
            i_frame_size += t->out.nal[i_nal+i].i_payload;
            // all entries in stat.frame are ints
            for( j = 0; j < sizeof(h->stat.frame) / sizeof(int); j++ )
                ((int*)&h->stat.frame)[j] += ((int*)&t->stat.frame)[j];
        }
        h->out.i_nal = i_nal + h->param.i_threads;
    }

	#if VISUALIZE//形象化
		if( h->param.b_visualize )
		{
			x264_visualize_show( h );
			x264_visualize_close( h );
		}
	#endif
	//printf("GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG");
    return i_frame_size;
}

/****************************************************************************
 * x264_encoder_encode:
 把picture复制成一个frame
 x264_encoder_encode每次会以参数送入一帧待编码的帧pic_in，函数首先会从空闲队列中
 取出一帧用于承载该新帧，而它的i_frame被设定为播放顺序计数，如：fenc->i_frame = h->frames.i_input++。 
 x264_encoder_encode在根据上述判据确定B帧缓冲充满的情况下才进行后续编码工作
 *  XXX: i_poc   : is the poc of the current given picture
 *       i_frame : is the number of the frame being coded
 *  ex:  type frame poc
 *       I      0   2*0	//poc是实际的帧的位置
 *       P      1   2*3	//frame是编码的顺序
 *       B      2   2*1
 *       B      3   2*2
 *       P      4   2*6
 *       B      5   2*4
 *       B      6   2*5
 ****************************************************************************/
int     x264_encoder_encode( x264_t *h,/* 指定编码器 */
                             x264_nal_t **pp_nal, /* x264_nal_t * */
							 int *pi_nal,	/* int */
                             x264_picture_t *pic_in,
                             x264_picture_t *pic_out )
{
    x264_frame_t   *frame_psnr = h->fdec;	/* just to keep the current decoded frame for psnr(峰值信噪比) calculation(预测) */
											/* 仅仅指向当前被解码重建的帧 frame being reconstructed(重建的) */
    int     i_nal_type;
    int     i_nal_ref_idc;
    int     i_slice_type;
    int     i_frame_size;

    int i;

    int   i_global_qp;

	//char psz_message[80];//提示信息，无什么用处
	//printf("\n( encoder.c:x264_encoder_encode )h->out.i_nal = %d ",h->out.i_nal);//zjh打印结果：一直是1
    //printf("\n( encoder.c:x264_encoder_encode ) * pi_nal = %d ",*pi_nal);//zjh此处打印是1244268，显然不正确，因为还没有对它赋值
	
	/* 
	no data out 
	无数据输出
	*/
    *pi_nal = 0;	//int
    *pp_nal = NULL;	//x264_nal_t *

	printf("\n          x264_encoder_encode(...) in \n");//zjh
    /* -------------------主要是将图片的原始数据赋值给一个未使用的帧，用于编码 Setup new frame from picture -------------------- */
    TIMER_START( i_mtime_encode_frame );//取开始时间，是一个define;#define TIMER_START( d ) {int64_t d##start = x264_mdate();  #define TIMER_STOP( d ) d += x264_mdate() - d##start;}
    if( pic_in != NULL )//输入的原始图像不为空
    {
        /* 1: Copy the picture to a frame and move it to a buffer 拷贝图片到一个帧并且移动它到一个缓冲区*/
        x264_frame_t *fenc = x264_frame_get( h->frames.unused );//frames是x264_t结构体里定义的另一个结构体，frames结构体里有一个字段是x264_frame_t *unused[X264_BFRAME_MAX+3];其中X264_BFRAME_MAX=16
																//x264_frame_get的参数是x264_frame_t *list[X264_BFRAME_MAX+1]，而这个unused就是这种类型
        x264_frame_copy_picture( h, fenc, pic_in );//把x264_picture_t的几个字段值赋给x264_frame_t，并处理色彩空间问题

        if( h->param.i_width % 16 || h->param.i_height % 16 ) /* 条件为真，说明宽和高至少有一个不是16的整数倍 */
            x264_frame_expand_border_mod16( h, fenc );//将长和宽扩展为16的倍数，估计是为了保证帧有整数个宏块 在frame.c中实现

        fenc->i_frame = h->frames.i_input++;	//输入的总帧数

		printf("\n          h->frames.i_input = %d " , h->frames.i_input);

        x264_frame_put( h->frames.next, fenc );	//新的帧跟在原帧的后面，放入链表list[i]

        if( h->frames.b_have_lowres )//这句后面比视频上少了一行
            x264_frame_init_lowres( h->param.cpu, fenc );//视频:http://www.powercam.cc/slide/8377

        if( h->frames.i_input <= h->frames.i_delay )	/* 延迟两帧编码，所以前两次不进入do_encode: */
        {
            /* Nothing yet to encode 仍然没有去编码 */
            /* waiting for filling bframe buffer 等待填充b帧缓冲区 */
            pic_out->i_type = X264_TYPE_AUTO;
			//printf("\n%d\n",h->frames.i_delay);//zjh
            return 0;//第0、1次调用Encode_Frame()，到这儿就返回了，第3次才会执行do_encode:后面
        }
    }

	/* 如果已经准备好的可编码的帧[0]没有 */
    if( h->frames.current[0] == NULL )//x264_t中，x264_frame_t *current[X264_BFRAME_MAX+3];//current是已经准备就绪可以编码的帧，其类型已经确定
    {
        int bframes = 0;
		//printf("\n h->frames.current[0] == NULL \n");//zjh

        /* 
		2: Select frame types 
		如果未确定类型的帧也没有，只能退出
		*/
        if( h->frames.next[0] == NULL )	//x264_t中，x264_frame_t *next[X264_BFRAME_MAX+3];//next是尚未确定类型的帧
            return 0;	//退出本函数

		//到此处，意味着，确定了类型的待编码的帧不存在，但是有未确定类型的帧
        x264_slicetype_decide( h );//这个函数确定当前条带（帧）的类型 decide:决定

        /* 3: move some B-frames and 1 non-B to encode编码 queue队列 
		 * 将一些B帧和xx?移进编码队列
		*/
        while( IS_X264_TYPE_B( h->frames.next[bframes]->i_type ) )	//判断连续的几帧是否是X264_TYPE_B或X264_TYPE_BREF
            bframes++;	//知道有几个帧是X264_TYPE_B或X264_TYPE_BREF

        x264_frame_put( h->frames.current, x264_frame_get( &h->frames.next[bframes] ) );	//x264_frame_put:放入链表list[i]，跟在后面
        /* FIXME: when max B-frames > 3, BREF may no longer be centered after GOP closing */
        if( h->param.b_bframe_pyramid && bframes > 1 )	//pyramid:金字塔
        {
            x264_frame_t *mid = x264_frame_get( &h->frames.next[bframes/2] );//返回list[0]
            mid->i_type = X264_TYPE_BREF;
            x264_frame_put( h->frames.current, mid );	////新的帧跟在原帧的后面，放入链表list[i]
            bframes--;
        }
        while( bframes-- )
            x264_frame_put( h->frames.current, x264_frame_get( h->frames.next ) );////新的帧跟在原帧的后面，放入链表list[i]
    }

    TIMER_STOP( i_mtime_encode_frame );

//	printf("\n......\n");//zjh

    /* ------------------- Get frame to be encoded 得到帧去编码------------------------- */
    /* 4: get picture to encode 
	 * 得到图像去编码
	*/
    h->fenc = x264_frame_get( h->frames.current );

    if( h->fenc == NULL )
    {
        /* Nothing yet to encode (ex: waiting for I/P with B frames) */
        /* waiting for filling bframe buffer */
        pic_out->i_type = X264_TYPE_AUTO;

        return 0;
    }

do_encode:

	/*
	 * 亲测得出以下结论
	 * Encode()函数中的i_frame会从0增长到总帧数-1
	 * h->frames.i_last_idr:上一个IDR帧是第几个，这个序号，比Encode()的i_frame少2(延迟编码2帧)
	 * 每次遇到编码IDR帧，都会更新h->frames.i_last_idr
	 * 更新时用h->frames.i_last_idr = h->fenc->i_frame;
	 * h->fenc->i_frame代表编码了多少帧
	 * 每次遇到IDR帧，h->fenc->i_poc归0
	*/

	//printf("\n%d\n",h->frames.i_delay);//zjh这个延迟始终不变，是2
	//如果是IDR立即刷新帧，要记下它的i_frame，因为有两个IDR帧距的限制，要随时计算是否该再插一个IDR
    if( h->fenc->i_type == X264_TYPE_IDR )//IDR
    {
        h->frames.i_last_idr = h->fenc->i_frame;//http://wmnmtm.blog.163.com/blog/static/38245714201181110467887/
		//printf("\n          h->frames.i_last_idr=%d ",h->frames.i_last_idr);//zjh//这个感觉是距离第一个idr帧的距离，而不是距离上一个idr帧  0 20 40 60 ...
    }
printf("\n          h->frames.i_last_idr=%d ",h->frames.i_last_idr);
    /* ------------------- Setup frame context ----------------------------- */
    /* 5: Init data dependant of frame type 
	 * 初始化数据取决于帧类型
	 * dependant:取决于
	*/
    TIMER_START( i_mtime_encode_frame );

	//printf("h->fenc->i_frame=%d    ",h->fenc->i_frame);//zjh
	
	/*
	 * h->fenc:正被编码的当前帧(x264_frame_t *fenc)
	 * 如果当前编码的帧是IDR帧，那么将会有一些限制：(各elseif语句都类似)
	 * 当前的片类型只能是I片
	 * NAL的类型只能是NAL_SLICE_IDR
	 * NAL的优先级只能是最高
	 * --keyint 5 将会每5帧输出一个IDR图像，具体对应为 h->fenc->i_frame=0    h->fenc->i_type == X264_TYPE_IDR //h->fenc->i_frame=1    h->fenc->i_type == X264_TYPE_P //h->fenc->i_frame=5    h->fenc->i_type == X264_TYPE_IDR
	*/
    if( h->fenc->i_type == X264_TYPE_IDR ) //fenc : 正被编码的当前帧(x264_frame_t  *fenc) //IDR
    {
        /* reset ref pictures 
		 * 重设参考图像，因为IDR图像之后的图像，不能参考此IDR前的任何图像，防止错误扩散
		 * 
		*/
        x264_reference_reset( h );	//重设参考图像，注意参数是x264_t指针，所以是全部的参考帧

		//因为是IDR图像，所以设置下面这些
        i_nal_type    = NAL_SLICE_IDR;			//NAL的类型，设为IDR图像
        i_nal_ref_idc = NAL_PRIORITY_HIGHEST;	//NAL的优先级，此处设为最高
        i_slice_type = SLICE_TYPE_I;			//片类型，设为I片
		//printf("h->fenc->i_type == X264_TYPE_IDR\n");//zjh
    }
	//帧内编码帧
    else if( h->fenc->i_type == X264_TYPE_I )
    {
        i_nal_type    = NAL_SLICE;				//NAL_SLICE   = 1 //不分区、非IDR图像的片
        i_nal_ref_idc = NAL_PRIORITY_HIGH; /* Not completely true but for now it is (as all I/P are kept as ref)*/
        i_slice_type = SLICE_TYPE_I;			//片类型，设为I片
		//printf("h->fenc->i_type == X264_TYPE_I\n");//zjh
    }
	//P帧
    else if( h->fenc->i_type == X264_TYPE_P )
    {
        i_nal_type    = NAL_SLICE;
        i_nal_ref_idc = NAL_PRIORITY_HIGH;		/* Not completely true but for now it is (as all I/P are kept as ref)*/
        i_slice_type = SLICE_TYPE_P;
		//printf("h->fenc->i_type == X264_TYPE_P\n");//zjh
    }
	//可作参考的B帧
    else if( h->fenc->i_type == X264_TYPE_BREF )/* Non-disposable(一次性) B-frame(B-帧) ???*/
    {
        i_nal_type    = NAL_SLICE;
        i_nal_ref_idc = NAL_PRIORITY_HIGH;		/* maybe add MMCO to forget it? -> low */
        i_slice_type = SLICE_TYPE_B;			//B片
		//printf("h->fenc->i_type == X264_TYPE_BREF\n");//zjh
    }
	//B帧
    else    /* B frame */
    {
        i_nal_type    = NAL_SLICE;				//NAL_SLICE   = 1 //不分区、非IDR图像的片
        i_nal_ref_idc = NAL_PRIORITY_DISPOSABLE;//NAL_优先级_可丢弃的
        i_slice_type = SLICE_TYPE_B;			//B片
		//printf("else\n");//zjh
    }

	/*
	 *
	 * POC标识图像的播放顺序
	*/
    h->fdec->i_poc =
    h->fenc->i_poc = 2 * (h->fenc->i_frame - h->frames.i_last_idr);	/* x264_frame_t    *fdec; 正在被重建的帧 frame being reconstructed(重建的) */

	printf("\n          h->fenc->i_poc = %d ;h->fenc->i_frame = %d ;h->fenc->i_frame_num = %d \n \n ",h->fenc->i_poc,h->fenc->i_frame,h->fenc->i_frame_num);
	//zjh//http://wmnmtm.blog.163.com/blog/static/38245714201181111372460/

//system("pause");//暂停，任意键继续
	/* 正在被重建的帧 ... = 正被编码的当前帧... */
    h->fdec->i_type = h->fenc->i_type;
    h->fdec->i_frame = h->fenc->i_frame;
    h->fenc->b_kept_as_ref =
    h->fdec->b_kept_as_ref = i_nal_ref_idc != NAL_PRIORITY_DISPOSABLE;

    /* ------------------- Init初始化----------------------------- */
    /* 创建参考列表list0和list1   build ref list 0/1 */
    x264_reference_build_list( h, h->fdec->i_poc, i_slice_type );//编码每一帧时，都会建立参考列表，调用时用了三个参数，第一个是指定编码器，第二个是播放顺序，第三个是片类型

    /* Init the rate control */
    x264_ratecontrol_start( h, i_slice_type, h->fenc->i_qpplus1 );
    i_global_qp = x264_ratecontrol_qp( h );

    pic_out->i_qpplus1 =
    h->fdec->i_qpplus1 = i_global_qp + 1;

    if( i_slice_type == SLICE_TYPE_B )
        x264_macroblock_bipred_init( h );

    /* ------------------------ Create slice header  ----------------------- */
    x264_slice_init( h, i_nal_type, i_slice_type, i_global_qp );

    if( h->fenc->b_kept_as_ref )
        h->i_frame_num++;

    /* ---------------------- Write the bitstream 写入比特流 -------------------------- */
    /* 初始化比特流上下文 */
    h->out.i_nal = 0;
	//初始化存放流的空间(从此处看出，每次进入本函数编码，都是重新开始)
    bs_init( &h->out.bs, h->out.p_bitstream, h->out.i_bitstream );
											//p_bitstream是动态分配内存时记下来的初始起始地址
											//这块内存是根据h->out.i_bitstream分配的
											//i_bitstream是1000000或者更大(1000000不够用会自动比它大)

    /*
	 * 分界符，毕厚杰：191/326 nal_unit_type = 9 分界符
	 * x264默认是不生成分界符的
	 * 优先级：NAL_PRIORITY_DISPOSABLE = 0
	 * 表现在文件中是0x 09
	 * 0x 09 10 ，其中的10是拖尾句法bs_rbsp_trailing(&h->out.bs);生成的
	*/
	//h->param.b_aud=0;//设为1，会生成一个0x 00 00 00 01 09 10 //http://wmnmtm.blog.163.com/blog/static/38245714201192523012109/
	if(h->param.b_aud)//生成访问单元分隔符,默认值为0，即不生成
	{
        int pic_type;

        if(i_slice_type == SLICE_TYPE_I)		//I片
		{
            pic_type = 0;
			printf("***8888888888888888888888888888 pic_type = %d",pic_type);
		}
        else if(i_slice_type == SLICE_TYPE_P)	//P片
		{
            pic_type = 1;
			printf("***8888888888888888888888888888 pic_type = %d",pic_type);
		}
        else if(i_slice_type == SLICE_TYPE_B)	//B片
            pic_type = 2;
        else
            pic_type = 7;

        x264_nal_start(h, NAL_AUD/* 9 */, NAL_PRIORITY_DISPOSABLE);//DISPOSABLE:用后可弃的，一次用弃的
        bs_write(&h->out.bs, 3, pic_type);//:::::::pic_type
			/*
			 * 即可能为如下之一：
			 * bs_write(&h->out.bs, 3, 0);//这个怎么会写出0x 09 10来,不清楚啊
			 * bs_write(&h->out.bs, 3, 1);
			 * bs_write(&h->out.bs, 3, 2);
			 * bs_write(&h->out.bs, 3, 7);
			*/
		//printf(":::::::::::::::::::::::");system("pause");//暂停，任意键继续
        bs_rbsp_trailing(&h->out.bs);
        x264_nal_end(h);
    }

    h->i_nal_type = i_nal_type;			//指明当前NAL_unit的类型
    h->i_nal_ref_idc = i_nal_ref_idc;	//指示当前NAL的优先级

    /* Write SPS and PPS 
	 * 写序列参数集、图像参数集以及SEI版本信息，并不是写入文件，而是写入输出缓冲区 
	 * 最后通过p_write_nalu中的fwrite写入到文件
	*/
    if( i_nal_type == NAL_SLICE_IDR && h->param.b_repeat_headers )//只在特定的条件下才输出SPS和PPS
    {
		printf("         encoder.c : Write SPS and PPS");
		system("pause");//暂停，任意键继续

        if( h->fenc->i_frame == 0 )//
        {
            /* identify ourself */
            x264_nal_start( h, NAL_SEI, NAL_PRIORITY_DISPOSABLE );
            //x264_sei_version_write( h, &h->out.bs );//注释掉仍能用ffplay播放，好像仅是版权声明
            x264_nal_end( h );
			
        }

        /* generate sequence parameters 
		 * 序列参数SPS写入流
		*/
        x264_nal_start( h, NAL_SPS, NAL_PRIORITY_HIGHEST );
		x264_sps_write( &h->out.bs, h->sps );//注释掉这句就不能播了
        x264_nal_end( h );
		/*
		 * 把这儿写入文件，看一下保存的sps是什么样子
		*/
		if (1)
		{
		//FILE *f11 = fopen( "c:\\sps.sps", "wb+" );
		//fwrite(data, 1024, 1, f11);
		}

        /* generate picture parameters 
		 * 图像参数集PPS写入流
		*/
        x264_nal_start( h, NAL_PPS, NAL_PRIORITY_HIGHEST );
        x264_pps_write( &h->out.bs, h->pps );//注释掉这句就不能播了
        x264_nal_end( h );
    }


    /* Write frame 写入帧，并不是写入文件，而是写入输出缓冲区*/
    i_frame_size = x264_slices_write( h );

    /* 恢复CPU状态 restore CPU state (before using float again) */
    x264_cpu_restore( h->param.cpu );

    if( i_slice_type == SLICE_TYPE_P && !h->param.rc.b_stat_read 
        && h->param.i_scenecut_threshold >= 0 )
    {
        const int *mbs = h->stat.frame.i_mb_count;
        int i_mb_i = mbs[I_16x16] + mbs[I_8x8] + mbs[I_4x4];
        int i_mb_p = mbs[P_L0] + mbs[P_8x8];
        int i_mb_s = mbs[P_SKIP];
        int i_mb   = h->sps->i_mb_width * h->sps->i_mb_height;
        int64_t i_inter_cost = h->stat.frame.i_inter_cost;
        int64_t i_intra_cost = h->stat.frame.i_intra_cost;

        float f_bias;
        int i_gop_size = h->fenc->i_frame - h->frames.i_last_idr;
        float f_thresh_max = h->param.i_scenecut_threshold / 100.0;
        /* magic numbers pulled out of thin air */
        float f_thresh_min = f_thresh_max * h->param.i_keyint_min
                             / ( h->param.i_keyint_max * 4 );
        if( h->param.i_keyint_min == h->param.i_keyint_max )
             f_thresh_min= f_thresh_max;

        /* macroblock_analyse() doesn't further进一步的 analyse解释 skipped mbs,
         * so we have to guess估计 their cost代价 */
        if( i_mb_s < i_mb )
            i_intra_cost = i_intra_cost * i_mb / (i_mb - i_mb_s);

        if( i_gop_size < h->param.i_keyint_min / 4 )
            f_bias = f_thresh_min / 4;
        else if( i_gop_size <= h->param.i_keyint_min )
            f_bias = f_thresh_min * i_gop_size / h->param.i_keyint_min;
        else
        {
            f_bias = f_thresh_min
                     + ( f_thresh_max - f_thresh_min )
                       * ( i_gop_size - h->param.i_keyint_min )
                       / ( h->param.i_keyint_max - h->param.i_keyint_min );
        }
        f_bias = X264_MIN( f_bias, 1.0 );

        /* 坏的P帧将被作为I帧重新编码 Bad P will be reencoded重新编码 as I */
        if( i_mb_s < i_mb &&
            i_inter_cost >= (1.0 - f_bias) * i_intra_cost )
        {
            int b;

            x264_log( h, X264_LOG_DEBUG, "scene cut at %d Icost:%.0f Pcost:%.0f ratio:%.3f bias=%.3f lastIDR:%d (I:%d P:%d S:%d)\n",
                      h->fenc->i_frame,
                      (double)i_intra_cost, (double)i_inter_cost,
                      (double)i_inter_cost / i_intra_cost,
                      f_bias, i_gop_size,
                      i_mb_i, i_mb_p, i_mb_s );
				//printf("\n");//zjh
				//printf("h->i_frame_num%d",h->i_frame_num);//zjh
				//printf("\n");//zjh

            /* Restore frame num */
            h->i_frame_num--;
				//printf("\n");//zjh
				//printf("h->i_frame_num--");//zjh
				//printf("\n");//zjh
				//printf("\n");//zjh
				//printf("h->i_frame_num%d",h->i_frame_num);//zjh
				//printf("\n");//zjh

            for( b = 0; h->frames.current[b] && IS_X264_TYPE_B( h->frames.current[b]->i_type ); b++ );
            if( b > 0 )
            {
                /* If using B-frames, force GOP to be closed.
                 * Even if this frame is going to be I and not IDR, forcing a
                 * P-frame before the scenecut will probably help compression.
                 * 
                 * We don't yet know exactly which frame is the scene cut, so
                 * we can't assign an I-frame. Instead, change the previous
                 * B-frame to P, and rearrange coding order. */

                if( h->param.b_bframe_adaptive || b > 1 )
                    h->fenc->i_type = X264_TYPE_AUTO;
                x264_frame_sort_pts( h->frames.current );
                x264_frame_push( h->frames.next, h->fenc );
                h->fenc = h->frames.current[b-1];
                h->frames.current[b-1] = NULL;
                h->fenc->i_type = X264_TYPE_P;
                x264_frame_sort_dts( h->frames.current );
            }
            /* 如果需要，执行IDR, Do IDR if needed */
            else if( i_gop_size >= h->param.i_keyint_min )	/* 从选项说明知道，设置了最小IDR间隔时，达到此设置值时就会插入IDR */
            {
                x264_frame_t *tmp;

				printf("\n");//zjh
				printf("i_gop_size >= h->param.i_keyint_min");//zjh
				printf("\n");//zjh

                /* Reset重设 */
                h->i_frame_num = 0;

                /* Reinit重初始化 field字段 of fenc */
                h->fenc->i_type = X264_TYPE_IDR;
                h->fenc->i_poc = 0;

                /* Put enqueued队列 frames back in the pool水池 */
                while( (tmp = x264_frame_get( h->frames.current ) ) != NULL )
                    x264_frame_put( h->frames.next, tmp );////新的帧跟在原帧的后面，放入链表list[i]
                x264_frame_sort_pts( h->frames.next );
            }
            else
            {
                h->fenc->i_type = X264_TYPE_I;
				printf("\n");//zjh
				printf("i_gop_size >= h->param.i_keyint_min else");//zjh
				printf("\n");//zjh
            }
            goto do_encode;//又跳到编码那儿
        }
    }

    /* 
	End bitstream, set output  
	结束比特流，设置输出，这样x264.c的Encode_Frame就获取到了
	*/
    *pi_nal = h->out.i_nal;//本函数这最最后一次出现此变量
    *pp_nal = h->out.nal;//本函数这最最后一次出现此变量

    /* 设置输出图像特性 Set output picture properties特性 */
    if( i_slice_type == SLICE_TYPE_I )
        pic_out->i_type = i_nal_type == NAL_SLICE_IDR ? X264_TYPE_IDR : X264_TYPE_I;//设置x264_picture_t的第1个字段
    else if( i_slice_type == SLICE_TYPE_P )
        pic_out->i_type = X264_TYPE_P;//x264_picture_t.i_type
    else
        pic_out->i_type = X264_TYPE_B;//x264_picture_t.i_type
    pic_out->i_pts = h->fenc->i_pts;//正被编码的当前帧的时间戳
	//printf("\n正被编码的当前帧的时间戳%d",h->fenc->i_pts);//zjh  打印出：0、1、2、3、4....,298、299

    pic_out->img.i_plane = h->fdec->i_plane;//x264_picture_t.img.i_plane 色彩空间的个数,
	//printf("\npic_out->img.i_plane = h->fdec->i_plane=%d",h->fdec->i_plane);//zjh 打印出：3

    for(i = 0; i < 4; i++){
        pic_out->img.i_stride[i] = h->fdec->i_stride[i];//正在重建的帧的YUV buffer步长
		//i_stride 是跨度，因为有时候要对图像进行边界扩展，跨度就是扩展后的宽度http://bbs.chinavideo.org/viewthread.php?tid=12772&extra=page%3D3
		//printf("\n pic_out->img.i_stride[%d] = h->fdec->i_stride[%d] = %d",i,i,h->fdec->i_stride[i]);//zjh  打印出：
		/*打印出：
		pic_out->img.i_stride[0] = h->fdec->i_stride[0] = 416
		pic_out->img.i_stride[1] = h->fdec->i_stride[1] = 208
		pic_out->img.i_stride[2] = h->fdec->i_stride[2] = 208
		pic_out->img.i_stride[3] = h->fdec->i_stride[3] = 0		
		*/
        
		
		pic_out->img.plane[i] = h->fdec->plane[i];	/* 面数，一般YUV都有3个 */
		//printf("pic_out->img.plane[i] = h->fdec->plane[i] = %s ",h->fdec->plane[i]);//这样打印出的是满屏的乱字符，有汉字有字母http://wmnmtm.blog.163.com/blog/static/382457142011816115130313/
    }

    /* 更新编码器状态---------------------- Update encoder state状态, 状况 ------------------------- */

    /* update rc */
    x264_cpu_restore( h->param.cpu );
    x264_ratecontrol_end( h, i_frame_size * 8 );//编码完一帧后，保存状态并且更新速率控制器状态//X264码率控制流程分析http://wmnmtm.blog.163.com/blog/static/3824571420118170113300/
	
    /* handle references */
    if( i_nal_ref_idc != NAL_PRIORITY_DISPOSABLE )
        x264_reference_update( h );
	#ifdef DEBUG_DUMP_FRAME
    else
        x264_fdec_deblock( h );
	#endif
    x264_frame_put( h->frames.unused, h->fenc );////新的帧跟在原帧的后面，放入链表list[i]

    /* increase增大 frame count */
    h->i_frame++;

    /* 恢复CPU状态restore CPU state (before using float again) */
    x264_cpu_restore( h->param.cpu );//恢复CPU状态

    x264_noise_reduction_update( h );//降噪更新

    TIMER_STOP( i_mtime_encode_frame );

	//把原始帧和重建帧选择性的输出，以便比较调试
	//if (h->i_frame==20)//第一帧4==*pi_nal
	//{
	//	x264_frame_dump( h, h->fdec, "d:\\fdec.yuv" );
	//	x264_frame_dump( h, h->fenc, "d:\\fenc.yuv" );
	//	printf("\n(encoder.c \ x264_encoder_encode()),i_frame=%d",h->i_frame);
	//	printf("\n\n");//zjh
	//	system("pause");//暂停，任意键继续
	//}



    /* 计算和打印统计资料---------------------- Compute/Print statistics 统计资料--------------------- */
    /* Slice stat */
	/*
    h->stat.i_slice_count[i_slice_type]++;
    h->stat.i_slice_size[i_slice_type] += i_frame_size + NALU_OVERHEAD;
    h->stat.i_slice_qp[i_slice_type] += i_global_qp;

    for( i = 0; i < 19; i++ )
        h->stat.i_mb_count[h->sh.i_type][i] += h->stat.frame.i_mb_count[i];
    for( i = 0; i < 2; i++ )
        h->stat.i_mb_count_8x8dct[i] += h->stat.frame.i_mb_count_8x8dct[i];
    if( h->sh.i_type != SLICE_TYPE_I )
    {
        for( i = 0; i < 7; i++ )
            h->stat.i_mb_count_size[h->sh.i_type][i] += h->stat.frame.i_mb_count_size[i];
        for( i = 0; i < 16; i++ )
            h->stat.i_mb_count_ref[h->sh.i_type][i] += h->stat.frame.i_mb_count_ref[i];
    }
    if( i_slice_type == SLICE_TYPE_B )
    {
        h->stat.i_direct_frames[ h->sh.b_direct_spatial_mv_pred ] ++;
        if( h->mb.b_direct_auto_write )
        {
            //FIXME somewhat arbitrary time constants
            if( h->stat.i_direct_score[0] + h->stat.i_direct_score[1] > h->mb.i_mb_count )
            {
                for( i = 0; i < 2; i++ )
                    h->stat.i_direct_score[i] = h->stat.i_direct_score[i] * 9/10;
            }
            for( i = 0; i < 2; i++ )
                h->stat.i_direct_score[i] += h->stat.frame.i_direct_score[i];
        }
    }
	*/
	/*
    if( h->param.analyse.b_psnr )
    {
        int64_t i_sqe_y, i_sqe_u, i_sqe_v;

        // PSNR(峰值信噪比) 
        i_sqe_y = x264_pixel_ssd_wxh( &h->pixf, frame_psnr->plane[0], frame_psnr->i_stride[0], h->fenc->plane[0], h->fenc->i_stride[0], h->param.i_width, h->param.i_height );
        i_sqe_u = x264_pixel_ssd_wxh( &h->pixf, frame_psnr->plane[1], frame_psnr->i_stride[1], h->fenc->plane[1], h->fenc->i_stride[1], h->param.i_width/2, h->param.i_height/2);
        i_sqe_v = x264_pixel_ssd_wxh( &h->pixf, frame_psnr->plane[2], frame_psnr->i_stride[2], h->fenc->plane[2], h->fenc->i_stride[2], h->param.i_width/2, h->param.i_height/2);
        x264_cpu_restore( h->param.cpu );

        h->stat.i_sqe_global[i_slice_type] += i_sqe_y + i_sqe_u + i_sqe_v;
        h->stat.f_psnr_average[i_slice_type] += x264_psnr( i_sqe_y + i_sqe_u + i_sqe_v, 3 * h->param.i_width * h->param.i_height / 2 );
        h->stat.f_psnr_mean_y[i_slice_type] += x264_psnr( i_sqe_y, h->param.i_width * h->param.i_height );
        h->stat.f_psnr_mean_u[i_slice_type] += x264_psnr( i_sqe_u, h->param.i_width * h->param.i_height / 4 );
        h->stat.f_psnr_mean_v[i_slice_type] += x264_psnr( i_sqe_v, h->param.i_width * h->param.i_height / 4 );

        snprintf( psz_message, 80, " PSNR Y:%2.2f U:%2.2f V:%2.2f",
                  x264_psnr( i_sqe_y, h->param.i_width * h->param.i_height ),
                  x264_psnr( i_sqe_u, h->param.i_width * h->param.i_height / 4),
                  x264_psnr( i_sqe_v, h->param.i_width * h->param.i_height / 4) );
        psz_message[79] = '\0';
    }
    else
    {
        psz_message[0] = '\0';
    }
    
    x264_log( h, X264_LOG_DEBUG,
                  "frame=%4d QP=%i NAL=%d Slice:%c Poc:%-3d I:%-4d P:%-4d SKIP:%-4d size=%d bytes%s\n",
              h->i_frame - 1,
              i_global_qp,
              i_nal_ref_idc,
              i_slice_type == SLICE_TYPE_I ? 'I' : (i_slice_type == SLICE_TYPE_P ? 'P' : 'B' ),
              frame_psnr->i_poc,
              h->stat.frame.i_mb_count_i,
              h->stat.frame.i_mb_count_p,
              h->stat.frame.i_mb_count_skip,
              i_frame_size,
              psz_message );
	*/

#ifdef DEBUG_MB_TYPE
{
    static const char mb_chars[] = { 'i', 'i', 'I', 'C', 'P', '8', 'S',
        'D', '<', 'X', 'B', 'X', '>', 'B', 'B', 'B', 'B', '8', 'S' };
    int mb_xy;
    for( mb_xy = 0; mb_xy < h->sps->i_mb_width * h->sps->i_mb_height; mb_xy++ )//有多少个宏块就循环多少次
	/*            ;       < 宏块表示的宽x宏块表示的高，实际就是宏块总数      */
    {
        if( h->mb.type[mb_xy] < 19 && h->mb.type[mb_xy] >= 0 )
            fprintf( stderr, "%c ", mb_chars[ h->mb.type[mb_xy] ] );
        else
            fprintf( stderr, "? " );

        if( (mb_xy+1) % h->sps->i_mb_width == 0 )
            fprintf( stderr, "\n" );
    }
}
#endif

	#ifdef DEBUG_DUMP_FRAME	//这个估计是为了调试进行输出的
    /* Dump丢弃 reconstructed重建的 frame */
    x264_frame_dump( h, frame_psnr, "fdec.yuv" );printf("\n( encoder.c:x264_encoder_encode ) * pi_nal = %d ",*pi_nal);//zjh
	#endif
	/*
	if (4==*pi_nal)//第一帧
	{
	x264_frame_dump( h, h->fdec, "d:\\fdec.yuv" );//写出来的是yuv420文件，可以用yuvviewer查看//http://wmnmtm.blog.163.com/blog/static/3824571420118188540701/
	x264_frame_dump( h, h->fenc, "d:\\fenc.yuv" );//写出来的是yuv420文件，可以用yuvviewer查看//http://wmnmtm.blog.163.com/blog/static/3824571420118188540701/
	}
	*/
    return 0;
}

/****************************************************************************
 * x264_encoder_close:
 ****************************************************************************/
void    x264_encoder_close  ( x264_t *h )
{
#ifdef DEBUG_BENCHMARK
    int64_t i_mtime_total = i_mtime_analyse + i_mtime_encode + i_mtime_write + i_mtime_filter + 1;
#endif
    int64_t i_yuv_size = 3 * h->param.i_width * h->param.i_height / 2;	//宽x高x1.5
    int i;

#ifdef DEBUG_BENCHMARK	//BENCHMARK:基准,标准
	//日志
    x264_log( h, X264_LOG_INFO,
              "analyse=%d(%lldms) encode=%d(%lldms) write=%d(%lldms) filter=%d(%lldms)\n",
              (int)(100*i_mtime_analyse/i_mtime_total), i_mtime_analyse/1000,
              (int)(100*i_mtime_encode/i_mtime_total), i_mtime_encode/1000,
              (int)(100*i_mtime_write/i_mtime_total), i_mtime_write/1000,
              (int)(100*i_mtime_filter/i_mtime_total), i_mtime_filter/1000 );
#endif

    /* 片使用的，和峰值信噪比 Slices used and PSNR(峰值信噪比) */
    for( i=0; i<5; i++ )
    {
        static const int slice_order[] = { SLICE_TYPE_I, SLICE_TYPE_SI, SLICE_TYPE_P, SLICE_TYPE_SP, SLICE_TYPE_B };
        static const char *slice_name[] = { "P", "B", "I", "SP", "SI" };
        int i_slice = slice_order[i];

        if( h->stat.i_slice_count[i_slice] > 0 )
        {
            const int i_count = h->stat.i_slice_count[i_slice];
            if( h->param.analyse.b_psnr )
            {
				//日志
                x264_log( h, X264_LOG_INFO,
                          "slice %s:%-5d Avg QP:%5.2f  size:%6.0f  PSNR Mean Y:%5.2f U:%5.2f V:%5.2f Avg:%5.2f Global:%5.2f\n",
                          slice_name[i_slice],
                          i_count,
                          (double)h->stat.i_slice_qp[i_slice] / i_count,
                          (double)h->stat.i_slice_size[i_slice] / i_count,
                          h->stat.f_psnr_mean_y[i_slice] / i_count, h->stat.f_psnr_mean_u[i_slice] / i_count, h->stat.f_psnr_mean_v[i_slice] / i_count,
                          h->stat.f_psnr_average[i_slice] / i_count,
                          x264_psnr( h->stat.i_sqe_global[i_slice], i_count * i_yuv_size ) );
            }
            else
            {
				//日志
                x264_log( h, X264_LOG_INFO,
                          "slice %s:%-5d Avg QP:%5.2f  size:%6.0f\n",
                          slice_name[i_slice],
                          i_count,
                          (double)h->stat.i_slice_qp[i_slice] / i_count,
                          (double)h->stat.i_slice_size[i_slice] / i_count );
            }
        }
    }

    /* 宏块类型使用的 MB types used */
    if( h->stat.i_slice_count[SLICE_TYPE_I] > 0 )
    {
        const int64_t *i_mb_count = h->stat.i_mb_count[SLICE_TYPE_I];
        const double i_count = h->stat.i_slice_count[SLICE_TYPE_I] * h->mb.i_mb_count / 100.0;
        x264_log( h, X264_LOG_INFO,
                  "mb I  I16..4: %4.1f%% %4.1f%% %4.1f%%\n",
                  i_mb_count[I_16x16]/ i_count,
                  i_mb_count[I_8x8]  / i_count,
                  i_mb_count[I_4x4]  / i_count );
    }
    if( h->stat.i_slice_count[SLICE_TYPE_P] > 0 )
    {
        const int64_t *i_mb_count = h->stat.i_mb_count[SLICE_TYPE_P];
        const int64_t *i_mb_size = h->stat.i_mb_count_size[SLICE_TYPE_P];
        const double i_count = h->stat.i_slice_count[SLICE_TYPE_P] * h->mb.i_mb_count / 100.0;
        x264_log( h, X264_LOG_INFO,
                  "mb P  I16..4: %4.1f%% %4.1f%% %4.1f%%  P16..4: %4.1f%% %4.1f%% %4.1f%% %4.1f%% %4.1f%%    skip:%4.1f%%\n",
                  i_mb_count[I_16x16]/ i_count,
                  i_mb_count[I_8x8]  / i_count,
                  i_mb_count[I_4x4]  / i_count,
                  i_mb_size[PIXEL_16x16] / (i_count*4),
                  (i_mb_size[PIXEL_16x8] + i_mb_size[PIXEL_8x16]) / (i_count*4),
                  i_mb_size[PIXEL_8x8] / (i_count*4),
                  (i_mb_size[PIXEL_8x4] + i_mb_size[PIXEL_4x8]) / (i_count*4),
                  i_mb_size[PIXEL_4x4] / (i_count*4),
                  i_mb_count[P_SKIP] / i_count );
    }
    if( h->stat.i_slice_count[SLICE_TYPE_B] > 0 )
    {
        const int64_t *i_mb_count = h->stat.i_mb_count[SLICE_TYPE_B];
        const int64_t *i_mb_size = h->stat.i_mb_count_size[SLICE_TYPE_B];
        const double i_count = h->stat.i_slice_count[SLICE_TYPE_B] * h->mb.i_mb_count / 100.0;
        x264_log( h, X264_LOG_INFO,
                  "mb B  I16..4: %4.1f%% %4.1f%% %4.1f%%  B16..8: %4.1f%% %4.1f%% %4.1f%%  direct:%4.1f%%  skip:%4.1f%%\n",
                  i_mb_count[I_16x16]  / i_count,
                  i_mb_count[I_8x8]    / i_count,
                  i_mb_count[I_4x4]    / i_count,
                  i_mb_size[PIXEL_16x16] / (i_count*4),
                  (i_mb_size[PIXEL_16x8] + i_mb_size[PIXEL_8x16]) / (i_count*4),
                  i_mb_size[PIXEL_8x8] / (i_count*4),
                  i_mb_count[B_DIRECT] / i_count,
                  i_mb_count[B_SKIP]   / i_count );
    }

    x264_ratecontrol_summary( h );	/* summary:即刻的, 立即的 */

    if( h->stat.i_slice_count[SLICE_TYPE_I] + h->stat.i_slice_count[SLICE_TYPE_P] + h->stat.i_slice_count[SLICE_TYPE_B] > 0 )
    {
        const int i_count = h->stat.i_slice_count[SLICE_TYPE_I] +
                            h->stat.i_slice_count[SLICE_TYPE_P] +
                            h->stat.i_slice_count[SLICE_TYPE_B];
        float fps = (float) h->param.i_fps_num / h->param.i_fps_den;
#define SUM3(p) (p[SLICE_TYPE_I] + p[SLICE_TYPE_P] + p[SLICE_TYPE_B])
#define SUM3b(p,o) (p[SLICE_TYPE_I][o] + p[SLICE_TYPE_P][o] + p[SLICE_TYPE_B][o])
        float f_bitrate = fps * SUM3(h->stat.i_slice_size) / i_count / 125;

        if( h->param.analyse.b_transform_8x8 )
        {
            int64_t i_i8x8 = SUM3b( h->stat.i_mb_count, I_8x8 );
            int64_t i_intra = i_i8x8 + SUM3b( h->stat.i_mb_count, I_4x4 )
                                     + SUM3b( h->stat.i_mb_count, I_16x16 );
            x264_log( h, X264_LOG_INFO, "8x8 transform  intra:%.1f%%  inter:%.1f%%\n",
                      100. * i_i8x8 / i_intra,
                      100. * h->stat.i_mb_count_8x8dct[1] / h->stat.i_mb_count_8x8dct[0] );
        }

        if( h->param.analyse.i_direct_mv_pred == X264_DIRECT_PRED_AUTO
            && h->stat.i_slice_count[SLICE_TYPE_B] )
        {
            x264_log( h, X264_LOG_INFO, "direct mvs  spatial:%.1f%%  temporal:%.1f%%\n",
                      h->stat.i_direct_frames[1] * 100. / h->stat.i_slice_count[SLICE_TYPE_B],
                      h->stat.i_direct_frames[0] * 100. / h->stat.i_slice_count[SLICE_TYPE_B] );
        }

        if( h->param.i_frame_reference > 1 )
        {
            int i_slice;
            for( i_slice = 0; i_slice < 2; i_slice++ )
            {
                char buf[200];
                char *p = buf;
                int64_t i_den = 0;
                int i_max = 0;
                for( i = 0; i < h->param.i_frame_reference; i++ )
                    if( h->stat.i_mb_count_ref[i_slice][i] )
                    {
                        i_den += h->stat.i_mb_count_ref[i_slice][i];
                        i_max = i;
                    }
                if( i_max == 0 )
                    continue;
                for( i = 0; i <= i_max; i++ )
                    p += sprintf( p, " %4.1f%%", 100. * h->stat.i_mb_count_ref[i_slice][i] / i_den );
                x264_log( h, X264_LOG_INFO, "ref %c %s\n", i_slice==SLICE_TYPE_P ? 'P' : 'B', buf );
            }
        }

        if( h->param.analyse.b_psnr )
            x264_log( h, X264_LOG_INFO,
                      "PSNR Mean Y:%6.3f U:%6.3f V:%6.3f Avg:%6.3f Global:%6.3f kb/s:%.2f\n",
                      SUM3( h->stat.f_psnr_mean_y ) / i_count,
                      SUM3( h->stat.f_psnr_mean_u ) / i_count,
                      SUM3( h->stat.f_psnr_mean_v ) / i_count,
                      SUM3( h->stat.f_psnr_average ) / i_count,
                      x264_psnr( SUM3( h->stat.i_sqe_global ), i_count * i_yuv_size ),
                      f_bitrate );
        else
            x264_log( h, X264_LOG_INFO, "kb/s:%.1f\n", f_bitrate );
    }

    /* frames */
    for( i = 0; i < X264_BFRAME_MAX + 3; i++ )
    {
        if( h->frames.current[i] ) x264_frame_delete( h->frames.current[i] );
        if( h->frames.next[i] )    x264_frame_delete( h->frames.next[i] );
        if( h->frames.unused[i] )  x264_frame_delete( h->frames.unused[i] );
    }
    /* ref frames */
    for( i = 0; i < h->frames.i_max_dpb; i++ )
    {
        x264_frame_delete( h->frames.reference[i] );
    }

    /* rc */
    x264_ratecontrol_delete( h );

    /* param */
    if( h->param.rc.psz_stat_out )
        free( h->param.rc.psz_stat_out );
    if( h->param.rc.psz_stat_in )
        free( h->param.rc.psz_stat_in );
    if( h->param.rc.psz_rc_eq )
        free( h->param.rc.psz_rc_eq );

    x264_cqm_delete( h );
    x264_macroblock_cache_end( h );
    x264_free( h->out.p_bitstream );
    for( i = 1; i < h->param.i_threads; i++ )
        x264_free( h->thread[i] );
    x264_free( h );
}

/*
函数原型：FILE * fopen(const char * path,const char * mode);
相关函数：open，fclose，fopen_s[1] ，_wfopen 　　
所需库： <stdio.h>
返回值： 文件顺利打开后，指向该流的文件指针就会被返回。如果文件打开失败则返回NULL，并把错误代码存在errno 中。
 　　一般而言，打开文件后会作一些文件读取或写入的动作，若打开文件失败，接下来的读写动作也无法顺利进行，所以一般在fopen()后作错误判断及处理。 　　
参数说明： 　　
参数path字符串包含欲打开的文件路径及文件名，参数mode字符串则代表着流形态。 　　
mode有下列几种形态字符串: 　　
r   以只读方式打开文件，该文件必须存在。 　　
r+  以可读写方式打开文件，该文件必须存在。 　　
rb+ 读写打开一个二进制文件，只允许读写数据。 　　
rt+ 读写打开一个文本文件，允许读和写。 　　
w   打开只写文件，若文件存在则文件长度清为0，即该文件内容会消失。若文件不存在则建立该文件。 　　
w+  打开可读写文件，若文件存在则文件长度清为零，即该文件内容会消失。若文件不存在则建立该文件。 　　
a   以附加的方式打开只写文件。若文件不存在，则会建立该文件，如果文件存在，写入的数据会被加到文件尾，即文件原先的内容会被保留。（EOF符保留） 　　
a+  以附加方式打开可读写的文件。若文件不存在，则会建立该文件，如果文件存在，写入的数据会被加到文件尾后，即文件原先的内容会被保留。 （原来的EOF符不保留） 　　
wb  只写打开或新建一个二进制文件；只允许写数据。 　　
wb+ 读写打开或建立一个二进制文件，允许读和写。 　　
wt+ 读写打开或着建立一个文本文件；允许读写。 　　
at+ 读写打开一个文本文件，允许读或在文本末追加数据。 　　
ab+ 读写打开一个二进制文件，允许读或在文件末追加数据。 　　
上述的形态字符串都可以再加一个b字符，如rb、w+b或ab＋等组合，加入b 字符用来告诉函数库打开的文件为二进制文件，而非纯文字文件。不过在POSIX系统，包含Linux都会忽略该字符。由fopen()所建立的新文件会具有S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH(0666)权限，此文件权限也会参考umask值。 　　
有些C编译系统可能不完全提供所有这些功能，有的C版本不用"r+","w+","a+",而用"rw","wr","ar"等，读者注意所用系统的规定。
 
*/


/*
原型：
	int fseek(FILE *stream, long offset, int fromwhere);
描述：
	函数设置文件指针stream的位置。如果执行成功，stream将指向以fromwhere（偏移起始位置：文件头0，当前位置1，文件尾2）为基准，偏移offset（指针偏移量）个字节的位置。如果执行失败(比如offset超过文件自身大小)，则不改变stream指向的位置。
返回：
	成功，返回0，否则返回其他值。 　　
	fseek position the file（文件） position（位置） pointer（指针） for the file referenced by stream to the byte location calculated by offset.

fseek函数的文件指针，应该为已经打开的文件。如果没有打开的文件，那么将会出现错误。 
fseek函数也可以这样理解，相当于在文件当中定位。这样在读取规律性存储才文件时可以利用其OFFSET偏移量读取文件上任意的内容。 　　
fseek函数一般用于二进制文件，因为文本文件要发生字符转换，计算位置时往往会发生混乱。

*/