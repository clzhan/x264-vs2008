/*****************************************************************************
 * macroblock.h: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: macroblock.h,v 1.1 2004/06/03 19:27:07 fenrir Exp $
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

#ifndef _MACROBLOCK_H
#define _MACROBLOCK_H 1

/*
枚举：宏块_位置_枚举
最后的_e代表枚举的enum
*/
enum macroblock_position_e
{
    MB_LEFT     = 0x01,	//左
    MB_TOP      = 0x02,	//上
    MB_TOPRIGHT = 0x04,	//右上
    MB_TOPLEFT  = 0x08,	//左上

    MB_PRIVATE  = 0x10,	//PRIVATE:私人的; 个人的2. 秘密

    ALL_NEIGHBORS = 0xf,	//所有_邻元素 (可能是指左、上、右上、左上都有)
};

/*
neighbors:邻元素,邻节点
标准的8.3.1.2 Intra_4x4样点预测 即第120页
或，毕厚杰的书，第200页
帧内4x4预测模式下，邻近像素情况
*/
static const int x264_pred_i4x4_neighbors[12] =
{
    MB_TOP,                         // I_PRED_4x4_V		[毕厚杰：第200页,Intra_4x4_Vertical预测模式]宏块_上面的宏块
    MB_LEFT,                        // I_PRED_4x4_H		[毕厚杰：第200页,Intra_4x4_Horizontal预测模式] 宏块_左面的宏块
    MB_LEFT | MB_TOP,               // I_PRED_4x4_DC	[毕厚杰：第200页,Intra_4x4_DC预测模式]	(A+B+C+D+I+J+K+L+4)/8	宏块_左面的宏块 | 宏块_左面的宏块 (|是按位或，并集)
    MB_TOP  | MB_TOPRIGHT,          // I_PRED_4x4_DDL	[毕厚杰：第200页,Intra_4x4_Diagonal_Down_Left预测模式]宏块_上面的 | 宏块_上面 右面 
    MB_LEFT | MB_TOPLEFT | MB_TOP,  // I_PRED_4x4_DDR	[毕厚杰：第200页,Intra_4x4_Diagonal_Down_Right预测模式]宏块_左面的 | 宏块_左上方的 | 宏块_上面的
    MB_LEFT | MB_TOPLEFT | MB_TOP,  // I_PRED_4x4_VR	[毕厚杰：第200页,Intra_4x4_Vertical_Right预测模式]同上
    MB_LEFT | MB_TOPLEFT | MB_TOP,  // I_PRED_4x4_HD	[毕厚杰：第200页,Intra_4x4_Horizontal_Down预测模式]同上
    MB_TOP  | MB_TOPRIGHT,          // I_PRED_4x4_VL	[毕厚杰：第200页,Intra_4x4_Vertical_Left预测模式]宏块_上面的 | 宏块_右上方的
    MB_LEFT,                        // I_PRED_4x4_HU	[毕厚杰：第200页,Intra_4x4_Horizontal_Up预测模式]	
    MB_LEFT,                        // I_PRED_4x4_DC_LEFT	[毕厚杰：第204页,Intra_4x4_DC]	(I+J+K+L+2)/4
    MB_TOP,                         // I_PRED_4x4_DC_TOP	[毕厚杰：第204页,Intra_4x4_DC] (A+B+C+D+2)/4
    0                               // I_PRED_4x4_DC_128	[毕厚杰：第204页,Intra_4x4_DC]	(128,...,128)	DC模式有4种情况，对应4个图，只有左边，只有上边，同时有左和上边，左和上都没有，直接预测值为128
};


/* XXX mb_type isn't the one written in the bitstream -> only internal usage */
#define IS_INTRA(type) ( (type) == I_4x4 || (type) == I_8x8 || (type) == I_16x16 )
#define IS_SKIP(type)  ( (type) == P_SKIP || (type) == B_SKIP )
#define IS_DIRECT(type)  ( (type) == B_DIRECT )

/*
枚举
在声明了枚举类型之后，可以用它来定义变量,在C语言中，枚举类型名包括关键字enum，以上的定义可以写为
在C++中允许不写enum，一般也不写enum，但保留了C的用法。
enum 枚举类型名 {枚举常量表列}; 
*/
/*
宏块模式选择
选择模式前，先把mb模块的类型列举出来
*/
enum mb_class_e
{
	//以I_表示的是I帧内的宏块模式，采用帧内预测
    I_4x4           = 0,//表示使用帧内4*4预测，宏块只有一种16*16，不存在4*4的宏块
    I_8x8           = 1,//??????????????????
    I_16x16         = 2,//??????????????????
    I_PCM           = 3,//直接传输像素值

	//P片中的mb_type [毕厚杰：第173页] 表6.28
    P_L0            = 4,//表示用参考列表L0，即前向预测
    P_8x8           = 5,//预测模式无，宏块分区宽度和高度均为8
    P_SKIP          = 6,//预测模式(mb_type,0)为Pred_L0，预测模式(mb_type,1)元；宏块分区宽度和高度均为16

	//B片中的mb_type [毕厚杰：第173页] 表6.29
    B_DIRECT        = 7,//[毕厚杰：第173页 B_Direct_16x16] 宏块分区宽度和高度8//b_direct, 一种宏块类型，采用direct的预测模式
    B_L0_L0         = 8,//[毕厚杰：第173页 ] 
    B_L0_L1         = 9,//[毕厚杰：第173页 ] 
    B_L0_BI         = 10,//[毕厚杰：第173页 ] 
    B_L1_L0         = 11,//[毕厚杰：第173页 ] 
    B_L1_L1         = 12,//[毕厚杰：第173页 ] 
    B_L1_BI         = 13,//[毕厚杰：第173页 ] 
    B_BI_L0         = 14,//[毕厚杰：第173页 ] 
    B_BI_L1         = 15,//[毕厚杰：第173页 ] 
    B_BI_BI         = 16,//[毕厚杰：第173页 ] 
    B_8x8           = 17,//[毕厚杰：第173页 ] 
    B_SKIP          = 18,//[毕厚杰：第173页 ] //p/b_skip，一种宏块类型，当图像采用帧间预测编码时，在图像平坦的区域使用“跳跃”块，“跳跃”块本身不携带任何数据，在解码端是通过 direct方式预测出MV或者直接周围已重建的宏块来恢复。对于B片中的skip宏块是采用direct模式，有时间和空间的direct预测方式。对 于P片的skip宏块采用利用周围已重建的宏块copy而来
};//[毕厚杰，纸书第二版] 172,173页，给了I、P、B片中的mb_type的三个表

/*
一个静态数组
上面定义的枚举中的元素，可以直接当对应的int型数字来用，比如I_4x4与0等价，B_SKIP与18等价，所以下面这个数组中，就可以理解了
*/
static const int x264_mb_type_fix[19] =						/* 与用以下数字等价 */
{
    I_4x4, I_4x4, I_16x16, I_PCM,							/* 0、1、2、3 */
    P_L0, P_8x8, P_SKIP,									/* 4、5、6 */
    B_DIRECT, B_L0_L0, B_L0_L1, B_L0_BI, B_L1_L0, B_L1_L1,	/* 7、8、9、10、11、12 */
    B_L1_BI, B_BI_L0, B_BI_L1, B_BI_BI, B_8x8, B_SKIP		/* 13、14、15、16、17、18 */
};

/*
fix：确定
二维数组
宏块类型查找表list0

这个和上面那个数组好像有某种对应关系，就好似同时定义了两个那个数组一样
*/
static const int x264_mb_type_list0_table[19][2] =
{
    {0,0}, {0,0}, {0,0}, {0,0}, /* INTRA */

    {1,1},                  /* P_L0 */
    {0,0},                  /* P_8x8 */
    {1,1},                  /* P_SKIP */

    {0,0},                  /* B_DIRECT */
    {1,1}, {1,0}, {1,1},    /* B_L0_* */
    {0,1}, {0,0}, {0,1},    /* B_L1_* */
    {1,1}, {1,0}, {1,1},    /* B_BI_* */
    {0,0},                  /* B_8x8 */
    {0,0}                   /* B_SKIP */
};

/*
*/
static const int x264_mb_type_list1_table[19][2] =
{
    {0,0}, {0,0}, {0,0}, {0,0}, /* INTRA */
    {0,0},                  /* P_L0 */
    {0,0},                  /* P_8x8 */
    {0,0},                  /* P_SKIP */
    {0,0},                  /* B_DIRECT */
    {0,0}, {0,1}, {0,1},    /* B_L0_* */
    {1,0}, {1,1}, {1,1},    /* B_L1_* */
    {1,0}, {1,1}, {1,1},    /* B_BI_* */
    {0,0},                  /* B_8x8 */
    {0,0}                   /* B_SKIP */
};

//判断是否是某种子块
#define IS_SUB4x4(type) ( (type ==D_L0_4x4)||(type ==D_L1_4x4)||(type ==D_BI_4x4))// 4*4		||逻辑或,只要有一个为true，结果就为true，在这儿，输入的type只可能是一种，所以后面的==判断只有一个可能是true，也不可能有两个或者三个成立
#define IS_SUB4x8(type) ( (type ==D_L0_4x8)||(type ==D_L1_4x8)||(type ==D_BI_4x8))// 4*8		那个括号里的type，一看就是下面的枚举的类型 mb_partition_e
#define IS_SUB8x4(type) ( (type ==D_L0_8x4)||(type ==D_L1_8x4)||(type ==D_BI_8x4))// 8*4
#define IS_SUB8x8(type) ( (type ==D_L0_8x8)||(type ==D_L1_8x8)||(type ==D_BI_8x8)||(type ==D_DIRECT_8x8))// 8*8
/*
下面是将Mb分割的块列举
*/
enum mb_partition_e	//宏块的分割//http://wmnmtm.blog.163.com/blog/static/382457142011773583805/
{
    /* sub partition type for P_8x8 and B_8x8 (用于两者P_8x8 and B_8x8)*/
    D_L0_4x4        = 0,
    D_L0_8x4        = 1,
    D_L0_4x8        = 2,
    D_L0_8x8        = 3,

    /* sub partition type for B_8x8 only (只用于B_8x8)*/
    D_L1_4x4        = 4,
    D_L1_8x4        = 5,
    D_L1_4x8        = 6,
    D_L1_8x8        = 7,

    D_BI_4x4        = 8,
    D_BI_8x4        = 9,
    D_BI_4x8        = 10,
    D_BI_8x8        = 11,
    D_DIRECT_8x8    = 12,

    /* partition */
    D_8x8           = 13,
    D_16x8          = 14,
    D_8x16          = 15,
    D_16x16         = 16,
};

/*
这个和上面的枚举是有关的，上面的取值0-16，共17个，下面这个二维数组的一个空间长度正好是17

*/
static const int x264_mb_partition_listX_table[2][17] =
{{
    1, 1, 1, 1, /* D_L0_* */
    0, 0, 0, 0, /* D_L1_* */
    1, 1, 1, 1, /* D_BI_* */
    0,          /* D_DIRECT_8x8 */
    0, 0, 0, 0  /* 8x8 .. 16x16 */
},
{
    0, 0, 0, 0, /* D_L0_* */
    1, 1, 1, 1, /* D_L1_* */
    1, 1, 1, 1, /* D_BI_* */
    0,          /* D_DIRECT_8x8 */
    0, 0, 0, 0  /* 8x8 .. 16x16 */
}};
static const int x264_mb_partition_count_table[17] =
{
    /* sub L0 */
    4, 2, 2, 1,
    /* sub L1 */
    4, 2, 2, 1,
    /* sub BI */
    4, 2, 2, 1,
    /* Direct */
    1,
    /* Partition */
    4, 2, 2, 1
};
static const int x264_mb_partition_pixel_table[17] =
{
    6, 4, 5, 3, 6, 4, 5, 3, 6, 4, 5, 3, 3, 3, 1, 2, 0
};

/* zigzags(锯齿形) are transposed(1. 使变位2. 变换顺序3. <数>移项) with respect(关联) to the tables(关联表) in the standard(标准) */
static const int x264_zigzag_scan4[16] =
{
    0,  4,  1,  2,  5,  8, 12,  9,  6,  3,  7, 10, 13, 14, 11, 15
};
/*
4x4块
	0	1	2	3

	4	5	6	7

	8	9	10	11

	12	13	14	15

	两下一比较，可以看出是一种锯齿形扫描，竖向开始

*/

/*
将一个一维数组，按下面这个数组里的元素所标的顺序依次读出来，就实现了锯齿形扫描
*/
static const int x264_zigzag_scan8[64] =
{
    0,  8,  1,  2,  9, 16, 24, 17, 10,  3,  4, 11, 18, 25, 32, 40,
   33, 26, 19, 12,  5,  6, 13, 20, 27, 34, 41, 48, 56, 49, 42, 35,
   28, 21, 14,  7, 15, 22, 29, 36, 43, 50, 57, 58, 51, 44, 37, 30,
   23, 31, 38, 45, 52, 59, 60, 53, 46, 39, 47, 54, 61, 62, 55, 63
};

/*	
8x8块(下面看成一个一维数组)
	0	1	2	3	4	5	6	7	

	8	9	10	11	12	13	14	15

	16	17	18	19	20	21	22	23

	24	25	26	27	28	29	30	31

	32	33	34	35	36	37	38	39

	40	41	42	43	44	45	46	47

	48	49	50	51	52	53	54	55

	56	57	58	59	60	61	62	63
*/

/*
x坐标
*/
static const uint8_t block_idx_x[16] =
{
    0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3
};
/*
Y坐标
*/
static const uint8_t block_idx_y[16] =
{
    0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3
};

/*
xy
这个数组很明显和上面的两个有联系，好像是平面坐标与一维数组的一种对应，具体是什么呢？
一个宏块可以分为4*4=16个 4x4块，这个好像就是标那个顺序的
[毕厚杰，第108页]DCT直流系数的变换量化，有一个图5.49 宏块中亮度块序号与其在宏块中位置的对应关系
此三个数组与图5.49的关系看下面博文
http://wmnmtm.blog.163.com/blog/static/3824571420117815650365/

*/
static const uint8_t block_idx_xy[4][4] =
{
    { 0, 2, 8,  10 },
    { 1, 3, 9,  11 },
    { 4, 6, 12, 14 },
    { 5, 7, 13, 15 }
};

/*
标准：表 8-15－作为qPI函数的QPC的规范
qPI  <30 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51
QPC =qPI 29 30 31 32 32 33 34 34 35 35 36 36 37 37 37 38 38 38 39 39 39 39
*/
static const int i_chroma_qp_table[52] =		//色度量化表
{
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,		/* 0-9 */
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,		/* 10-19 */
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,		/* 20-29 */
    29, 30, 31, 32, 32, 33, 34, 34, 35, 35,
    36, 36, 37, 37, 37, 38, 38, 38, 39, 39,
    39, 39
};//29开始，与上面的QPC一样

/*
ctxBlockCat
标准：251/341:表 9-33－不同块ctxBlockCat的规定
*/
enum cabac_ctx_block_cat_e
{
    DCT_LUMA_DC   = 0,
    DCT_LUMA_AC   = 1,
    DCT_LUMA_4x4  = 2,
    DCT_CHROMA_DC = 3,
    DCT_CHROMA_AC = 4,
    DCT_LUMA_8x8  = 5,
};


int  x264_macroblock_cache_init( x264_t *h );////分配x264_t结构体下子结构体mb对应的qp、cbp、skipbp、mb_transform_size、intra4x4_pred_mode、non_zero_count等等在宏块编码时用于控制和传输等用到的表
void x264_macroblock_slice_init( x264_t *h );//
void x264_macroblock_cache_load( x264_t *h, int i_mb_x, int i_mb_y );//它是将要编码的宏块的周围的宏块的值读进来, 要想得到当前块的预测值，要先知道上面，左面的预测值，将参考帧的编码信息存储在h->mb.cache中，可以反复使用
void x264_macroblock_cache_save( x264_t *h );
void x264_macroblock_cache_end( x264_t *h );

void x264_macroblock_bipred_init( x264_t *h );

/* x264_mb_predict_mv_16x16:
 *      set mvp with predicted mv for D_16x16 block
 *      h->mb. need only valid values from other blocks */
void x264_mb_predict_mv_16x16( x264_t *h, int i_list, int i_ref, int mvp[2] );
/* x264_mb_predict_mv_pskip:
 *      set mvp with predicted mv for P_SKIP
 *      h->mb. need only valid values from other blocks */
void x264_mb_predict_mv_pskip( x264_t *h, int mv[2] );
/* x264_mb_predict_mv:
 *      set mvp with predicted mv for all blocks except SKIP and DIRECT
 *      h->mb. need valid ref/partition/sub of current block to be valid
 *      and valid mv/ref from other blocks. */
void x264_mb_predict_mv( x264_t *h, int i_list, int idx, int i_width, int mvp[2] );
/* x264_mb_predict_mv_direct16x16:
 *      set h->mb.cache.mv and h->mb.cache.ref for B_SKIP or B_DIRECT
 *      h->mb. need only valid values from other blocks.
 *      return 1 on success, 0 on failure.
 *      if b_changed != NULL, set it to whether refs or mvs differ from
 *      before this functioncall. */
int x264_mb_predict_mv_direct16x16( x264_t *h, int *b_changed );
/* x264_mb_load_mv_direct8x8:
 *      set h->mb.cache.mv and h->mb.cache.ref for B_DIRECT
 *      must be called only after x264_mb_predict_mv_direct16x16 */
void x264_mb_load_mv_direct8x8( x264_t *h, int idx );
/* x264_mb_predict_mv_ref16x16:
 *      set mvc with D_16x16 prediction.
 *      uses all neighbors, even those that didn't end up using this ref.
 *      h->mb. need only valid values from other blocks */
void x264_mb_predict_mv_ref16x16( x264_t *h, int i_list, int i_ref, int mvc[8][2], int *i_mvc );


int  x264_mb_predict_intra4x4_mode( x264_t *h, int idx );
int  x264_mb_predict_non_zero_code( x264_t *h, int idx );

/* x264_mb_transform_8x8_allowed:
 *      check whether any partition is smaller than 8x8 (or at least
 *      might be, according to just partition type.)
 *      doesn't check for intra or cbp */
int  x264_mb_transform_8x8_allowed( x264_t *h );

void x264_mb_encode_i4x4( x264_t *h, int idx, int i_qscale );
void x264_mb_encode_i8x8( x264_t *h, int idx, int i_qscale );

void x264_mb_mc( x264_t *h );
void x264_mb_mc_8x8( x264_t *h, int i8 );


static inline void x264_macroblock_cache_ref( x264_t *h, int x, int y, int width, int height, int i_list, int ref )
{
    int dy, dx;
    for( dy = 0; dy < height; dy++ )
    {
        for( dx = 0; dx < width; dx++ )
        {
            h->mb.cache.ref[i_list][X264_SCAN8_0+x+dx+8*(y+dy)] = ref;
        }
    }
}
static inline void x264_macroblock_cache_mv( x264_t *h, int x, int y, int width, int height, int i_list, int mvx, int mvy )
{
    int dy, dx;
    for( dy = 0; dy < height; dy++ )
    {
        for( dx = 0; dx < width; dx++ )
        {
            h->mb.cache.mv[i_list][X264_SCAN8_0+x+dx+8*(y+dy)][0] = mvx;
            h->mb.cache.mv[i_list][X264_SCAN8_0+x+dx+8*(y+dy)][1] = mvy;
        }
    }
}
static inline void x264_macroblock_cache_mvd( x264_t *h, int x, int y, int width, int height, int i_list, int mdx, int mdy )
{
    int dy, dx;
    for( dy = 0; dy < height; dy++ )
    {
        for( dx = 0; dx < width; dx++ )
        {
            h->mb.cache.mvd[i_list][X264_SCAN8_0+x+dx+8*(y+dy)][0] = mdx;
            h->mb.cache.mvd[i_list][X264_SCAN8_0+x+dx+8*(y+dy)][1] = mdy;
        }
    }
}
static inline void x264_macroblock_cache_skip( x264_t *h, int x, int y, int width, int height, int b_skip )
{
    int dy, dx;
    for( dy = 0; dy < height; dy++ )
    {
        for( dx = 0; dx < width; dx++ )
        {
            h->mb.cache.skip[X264_SCAN8_0+x+dx+8*(y+dy)] = b_skip;
        }
    }
}
static inline void x264_macroblock_cache_intra8x8_pred( x264_t *h, int x, int y, int i_mode )
{
    int *cache = &h->mb.cache.intra4x4_pred_mode[X264_SCAN8_0+x+8*y];
    cache[0] = cache[1] = cache[8] = cache[9] = i_mode;
}

#endif

/*
A. 帧内预测：
根据H.264标准规定的9种帧内4x4亮度分量预测、4种帧内16xl6亮度分量预测以及4种帧内8x8色差分量预测模式，针对宏块左邻和上邻宏块存在 或缺失的不同情况，分别直接调用不同的预测函数，以节约逻辑判断的时间。《新一代视频编码标准》P104
B. 帧间预测：
在对P帧或B帧的宏块进行预测之前，先判断当前帧是否适宜用帧内模式，如果宏块的临近已编码宏块
均不采用帧内模式，并且若宏块所在的slice为p的话，参考帧相应位置的宏块也不采用帧内模式的话，则该宏块采用帧内预测的可能性就很小。那么在该宏块用帧间模式得到的最小的SAD后，只要计算帧内16*16预测模式的SAD，将二者相比，当比值超过门限值时，即用帧间预测模式，而不用计算帧内4*4的模式了。

1. P_SKIP模式：先判断是否是SKIP模式，其模式的判断有以下几个条件：
（1）最佳模式选择为Inter16×16；
（2）MC得到的最终运动矢量等于预测运动矢量，即运动矢量的残差为0；
（3）变换系数均被量化为0。

2.亮度宏块划分子宏块预测模式的选择：当在判断不是skip模式的时候，可以根据命令行的输入来决定是否进行对16*16，8*8的宏块进行子宏块的划分。
1),首先计算16*16宏块的运动矢量即其cost.
2).进行子宏块的划分，计算4个8*8子宏块的运动矢量所对应的cost和，与16*16模式进行比较，若16*16的cost较小时，则结束划分更小 的宏块。否则，继续对8*8的块继续下分：8*4，4*8，4*4，分别进行比较，得到最小的cost，并保存对应的最佳的Mv.
3).计算8*16，16*8模式的cost，与上面的mv的cost进行比较，得到最终的mv（1or2or4o8or16个）。

ctxIdx:
H.264将一个片内可能出现的数据划分为399个上下文模型，每个模型以ctxIdx标识，在每个模型内部进行概率的查找和更新。H.264共要建立399个概率表，....(毕厚杰，第120页，算术编码CABAC的上下文模型)
*/