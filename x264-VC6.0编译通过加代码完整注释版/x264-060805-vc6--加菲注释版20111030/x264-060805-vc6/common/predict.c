/*****************************************************************************
 * predict.c: h264 encoder
 * 亮度416和色度8帧内预测.doc
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: predict.c,v 1.1 2004/06/03 19:27:07 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Loren Merritt <lorenm@u.washington.edu>
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

/* predict4x4 are inspired from ffmpeg h264 decoder */


#include "common.h"
#include "clip1.h"

#ifdef _MSC_VER
	#undef HAVE_MMXEXT  /* not finished now */
#endif
#ifdef HAVE_MMXEXT
	#include "i386/predict.h"
#endif

/****************************************************************************
 * 16x16 prediction for intra luma(亮度) block
 * 16x16 预测 帧内亮度块

 ****************************************************************************/
/*
 * 16x16亮度块的帧内预测方式
 * Intra_16x16_DC
 * 左边和上边邻块均可用,即指相邻像素H和V均存在，
 * 当前宏块的所有像素值为(H+V)的平均值
 * 
 * 资料：毕厚杰，第205页
 * #define FDEC_STRIDE 32	//STRIDE:跨过
 * 本函数用v对16x16的所有像素进行赋值，循环一次赋值一行16个像素(宏定义)
 * 第一句既表明p指向的是32位无符号整型变量，也表明其指向的是src(像素起始位置)正因为是32位所以才有后面一次能给4个像素赋值
 * 指针加1 移四个字节即32位 ,这一句可以给四个像素点赋值，一个像素占一个字节
 * FDEC_STRIDE=32 和 FENC_STRIDE=16 是定义的。这里32，16 是字节数。（根据存储空间的使用规则不难理解）
*/
#define PREDICT_16x16_DC(v) \
    for( i = 0; i < 16; i++ )\
    {\
        uint32_t *p = (uint32_t*)src;\
        *p++ = v;\
        *p++ = v;\
        *p++ = v;\
        *p++ = v;\
        src += FDEC_STRIDE;\
    }

/*
 * 16x16亮度块的帧内预测方式
 * Intra_16x16_DC
 * 上和左邻块可用时,帧内16*16亮度块DC模式预测
 * 
 * 资料：毕厚杰，第205页
 * 功能：本函数是用当前宏块上方和左方共32个像素的和的均值进行预测，预测后所有16x16个像素的值都被该均值替代
*/
static void predict_16x16_dc( uint8_t *src )//typedef unsigned char   uint8_t;
{
    uint32_t dc = 0;	//定义dc为32 位无符号整型数 （目的同上）
    int i;

    for( i = 0; i < 16; i++ )
    {
        dc += src[-1 + i * FDEC_STRIDE];	//当前宏块的第i行左方的像素值
        dc += src[i - FDEC_STRIDE];			//此时dc为（宏块第i行左方的像素值）+（宏块第i 列上方的像素值）
    }										//循环16次后dc为当前宏块左方和上方所有32 个像素的和
    dc = (( dc + 16 ) >> 5) * 0x01010101;	//所有32个像素取均值：+16是为了四舍五入，右移5表示除以2的5次方即32，* 0x01010101为了一次能给4个像素赋值。
											//（例如0x12* 0x01010101=0x12121212，用windows自带的计算器可查看）对照PREDICT_16x16_DC(v)即可明白其作用
    PREDICT_16x16_DC(dc);					//将得出来的dc代入该赋值函数,所有16x16个像素的值相同
}

/*
 * 16x16亮度块的帧内预测方式
 * Intra_16x16_DC
 * 左边邻块可用时,帧内16*16亮度块DC模式预测
 * 
 * 资料：毕厚杰，第205页
 * 功能：本函数用左边的16个像素的均值对当前宏块进行预测，预测后当前宏块的所有16x16个像素的值都被该均值替代
*/
static void predict_16x16_dc_left( uint8_t *src )
{
    uint32_t dc = 0;
    int i;

    for( i = 0; i < 16; i++ )
    {
        dc += src[-1 + i * FDEC_STRIDE];	//当前宏块的第i行的左边像素的值
    }										//循环16次后dc为当前宏块左方的16个像素值的和
    dc = (( dc + 8 ) >> 4) * 0x01010101;	//对16个像素取均值

    PREDICT_16x16_DC(dc);					//对16x16所有像素赋值
}

/*
 * 16x16亮度块的帧内预测方式
 * Intra_16x16_DC
 * 上边邻块可用时,帧内16*16亮度块DC模式预测
 * 
 * 资料：毕厚杰，第205页
 * 功能：本函数用上方16个像素点的均值覆盖当前16x16块的所有像素值
*/
static void predict_16x16_dc_top( uint8_t *src )
{
    uint32_t dc = 0;
    int i;

    for( i = 0; i < 16; i++ )
    {
        dc += src[i - FDEC_STRIDE];			//当前宏块第i 列上方的像素值
    }
    dc = (( dc + 8 ) >> 4) * 0x01010101;	//求均值

    PREDICT_16x16_DC(dc);
}

/*
 * 16x16亮度块的帧内预测方式
 * Intra_16x16_DC
 * 邻块均不可用时,帧内16*16亮度块预测DC模式,预测值为128
 * 
 * 资料：毕厚杰，第205页
 * 功能：本函数用固定值128即0x80对16x16块的所有像素进行覆盖
*/
static void predict_16x16_dc_128( uint8_t *src )
{
    int i;
    PREDICT_16x16_DC(0x80808080);		//对16x16个像素赋值
}

/*
帧内16*16亮度块水平预测 
水平：horizontal;level
功能：本函数用当前块的左方像素进行水平方向的预测（宏块的第i 行所有像素值都等于其左边的像素值）
*/
static void predict_16x16_h( uint8_t *src )
{
    int i;

    for( i = 0; i < 16; i++ )
    {
        const uint32_t v = 0x01010101 * src[-1];	//src[-1]是当前宏块第i行左方的像素值( i = 0; i < 16; i++ )
        uint32_t *p = (uint32_t*)src;

        *p++ = v;
        *p++ = v;
        *p++ = v;
        *p++ = v;

        src += FDEC_STRIDE;		//循环一次，指针向下移一行

    }							//循环一次预测一行，共循环十六次
}

/*
帧内16*16亮度块垂直预测
垂直:vertical
功能：本函数用当前块的上方像素进行垂直方向预测（宏块的第i列像素都等于其上方的像素值）
*/
static void predict_16x16_v( uint8_t *src )
{
	//因为v是32位，四字节无符号整型数，可以将src指向的连续四个字节的单元内的值赋给它
    uint32_t v0 = *(uint32_t*)&src[ 0-FDEC_STRIDE];	//v0为当前块上方第0-3个像素值
    uint32_t v1 = *(uint32_t*)&src[ 4-FDEC_STRIDE];	//v1为当前块上方第4-7个像素值
    uint32_t v2 = *(uint32_t*)&src[ 8-FDEC_STRIDE];	//v2为当前块上方第8-11个像素值
    uint32_t v3 = *(uint32_t*)&src[12-FDEC_STRIDE];	//v3为当前块上方第12-15个像素值
    int i;

    for( i = 0; i < 16; i++ )
    {
        uint32_t *p = (uint32_t*)src;	//p指向当前块的第一个像素单元
        *p++ = v0;						//将v0赋给当前块第i行第0-3个像素
        *p++ = v1;						//将v1赋给当前块第i行第4-7个像素
        *p++ = v2;						//将v2赋给当前块第i行第8-11个像素
        *p++ = v3;						//将v3赋给当前块第i行第12-15个像素
        src += FDEC_STRIDE;				//使src指向下一行开始位置
    }
}

/*
帧内16*16亮度块平面预测
plane:平面
功能：用特定的计算方法对宏块进行预测
*/
static void predict_16x16_p( uint8_t *src )
{
    int x, y, i;
    int a, b, c;
    int H = 0;
    int V = 0;
    int i00;

    /* calcule(计算, 估计) H and V */
    for( i = 0; i <= 7; i++ )//循环8次
    {
        H += ( i + 1 ) * ( src[ 8 + i - FDEC_STRIDE ] - src[6 -i -FDEC_STRIDE] );	//宏块上方对应位置的16个像素点相减再求和（用到了左上角的点而舍弃了中间点）
        V += ( i + 1 ) * ( src[-1 + (8+i)*FDEC_STRIDE] - src[-1 + (6-i)*FDEC_STRIDE] );	//宏块左方对应位置的16个像素点相减再求和
    }

	//根据公式计算H和V以及a、b、c的值
    a = 16 * ( src[-1 + 15*FDEC_STRIDE] + src[15 - FDEC_STRIDE] );
    b = ( 5 * H + 32 ) >> 6;
    c = ( 5 * V + 32 ) >> 6;

    i00 = a - b * 7 - c * 7 + 16;

    for( y = 0; y < 16; y++ )
    {
        int pix = i00;
        for( x = 0; x < 16; x++ )	//循环一次对一行像素点附值
        {
            src[x] = x264_clip_uint8( pix>>5 );
            pix += b;		//-循环一次增加( 5 * H + 32 ) >> 6
        }					//Pred(x,j) =clip((a+b(-7+x)+c(-7+j))+16)>>5)第j行所有像素的预测值
        src += FDEC_STRIDE;	//指向下一行
        i00 += c;			//-循环一次增加( 5 * V + 32 ) >> 6
    }						//Pred(x,y) = clip((a+b(-7+x)+c(-7+y))+16)>>5)   16*16所有像素的预测值
}
/*
分析:A、255=2e8=1111 1111，(~255)= 0xffffff00
a&0xffffff00,如果不等于0,分两种情况
1.a<0,那么最高位肯定是1,-a变为正数,移位31后必然全为0,即0x00000000,结果为0
2.a>0,那么最高位肯定是0,另外a肯定>255,-a变为负数,移位31后必然全为1,即0xffffffff,结果为255
所以通过(-a)>>31这样的运算避免了判断:if(a<0)return 0;
else if(a>255) return 255;
else return a;
B、a&0xffffff00,如果等于0,说明0<a<255,不需要处理,直接输出.

*/

/****************************************************************************
 * 8x8 prediction for intra chroma block
 * 8x8 预测 帧内 色度块
 ****************************************************************************/

/*
 * 功能：8*8块的所有像素都用128代替
 * 资料：
 * 思路：
 *
 *    * 0 1 2 3 4 5 6 7
 *************************
 *  0 * 
 *  1 * 
 *  2 * 
 *  3 *     都是128
 *  4 * 
 *  5 * 
 *  6 * 
 *  7 * 
 *************************
 *
 *
 *
*/
static void predict_8x8c_dc_128( uint8_t *src )
{
    int y;

    for( y = 0; y < 8; y++ )			//循环8次，一次预测一行
    {
        uint32_t *p = (uint32_t*)src;	//指针指向源像素位置
        *p++ = 0x80808080;				//用128覆盖一行的前四个像素
        *p++ = 0x80808080;				//用128覆盖一行的后四个像素
        src += FDEC_STRIDE;				//移位到下一行
    }
}

/*
 * 用像素块左边的一列像素对当前8*8块进行预测
 *
 *    * 0 1 2 3 4 5 6 7
 *************************
 *  I * 
 *  J * 
 *  K * 
 *  L *   (I+..+P)/8
 *  M * 
 *  N * 
 *  O * 
 *  P * 
 *************************
 *
*/
static void predict_8x8c_dc_left( uint8_t *src )
{
    int y;
    uint32_t dc0 = 0, dc1 = 0;

    for( y = 0; y < 4; y++ )					//循环4次求出dc0和dc1
    {
        dc0 += src[y * FDEC_STRIDE     - 1];
        dc1 += src[(y+4) * FDEC_STRIDE - 1];
    }
    dc0 = (( dc0 + 2 ) >> 2)*0x01010101;		//使得DC0一次能够覆盖4个像素
    dc1 = (( dc1 + 2 ) >> 2)*0x01010101;		//使得DC1一次能够覆盖4个像素

    for( y = 0; y < 4; y++ )					//循环四次得到当前块前四行的预测值，一行有8个像素
    {
        uint32_t *p = (uint32_t*)src;
        *p++ = dc0;								//预测一行中前4个像素
        *p++ = dc0;								//预测一行中后4个像素
        src += FDEC_STRIDE;						//下移一行
    }
    for( y = 0; y < 4; y++ )					//循环四次得到当前块后四行的预测值
    {
        uint32_t *p = (uint32_t*)src;
        *p++ = dc1;
        *p++ = dc1;
        src += FDEC_STRIDE;
    }

}

/*
 * 功能：用像素块上边的一行像素对当前块进行预测
 *
 *
 *    * 0 1 2 3 4 5 6 7
 *************************
 *  I * 
 *  J * 
 *  K * 
 *  L *   (0+..+7)/8
 *  M * 
 *  N * 
 *  O * 
 *  P * 
 *************************
 *************************
 * 直流预测，用上边的一行像素的平均值预测整个块?
 *
*/
static void predict_8x8c_dc_top( uint8_t *src )
{
    int y, x;
    uint32_t dc0 = 0, dc1 = 0;

    for( x = 0; x < 4; x++ )				//循环四次得到dc0和dc1的值
    {
        dc0 += src[x     - FDEC_STRIDE];	//
        dc1 += src[x + 4 - FDEC_STRIDE];	//
    }
    dc0 = (( dc0 + 2 ) >> 2)*0x01010101;	//dc0=当前块上边一行像素中前四个像素的均值
    dc1 = (( dc1 + 2 ) >> 2)*0x01010101;	//dc1=当前块上边一行像素中后四个像素的均值

    for( y = 0; y < 8; y++ )				//循环8次得到当前块的8行像素值
    {
        uint32_t *p = (uint32_t*)src;		//
        *p++ = dc0;							//前四列均=dc0
        *p++ = dc1;							//后四列均=dc1
        src += FDEC_STRIDE;
    }
}

/*
 * 用当前块上方一行和左方一列像素联合起来对当前8*8块预测
 *
 *
 *
*/
static void predict_8x8c_dc( uint8_t *src )
{
    int y;
    int s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    uint32_t dc0, dc1, dc2, dc3;
    int i;

    /*
          s0 s1
       s2
       s3
	   代表位置如上

	参考word文档

    */
    for( i = 0; i < 4; i++ )
    {
        s0 += src[i - FDEC_STRIDE];
        s1 += src[i + 4 - FDEC_STRIDE];
        s2 += src[-1 + i * FDEC_STRIDE];
        s3 += src[-1 + (i+4)*FDEC_STRIDE];
    }
    /*
       dc0 dc1
       dc2 dc3
	   代表预测后的像素值分配
     */
    dc0 = (( s0 + s2 + 4 ) >> 3)*0x01010101;
    dc1 = (( s1 + 2 ) >> 2)*0x01010101;
    dc2 = (( s3 + 2 ) >> 2)*0x01010101;
    dc3 = (( s1 + s3 + 4 ) >> 3)*0x01010101;

    for( y = 0; y < 4; y++ )			//循环四次得到前四行的像素值
    {
        uint32_t *p = (uint32_t*)src;
        *p++ = dc0;						//8*8块中第一个4*4块的值=dc0
        *p++ = dc1;						//8*8块中第二个4*4块的值=dc1
        src += FDEC_STRIDE;
    }

    for( y = 0; y < 4; y++ )			//循环四次得到后四行的像素值
    {
        uint32_t *p = (uint32_t*)src;
        *p++ = dc2;						//8*8块中第三个4*4块的像素值
        *p++ = dc3;						//8*8块中第四个4*4块的像素值
        src += FDEC_STRIDE;
    }
}

/*
 * 功能：用当前块左边一列像素对当前块预测
 *
 *
 * Src  * 当前8*8块，分8行显示
 * *****************************************************************
 * -1	* 0 1 2 3 4 5 6 7 8 
 * *****************************************************************
 *  0   * 0 0 0 0 0 0 0 0 0	
 *  1   * 1 1 1 1 1 1 1 1 1
 *  2   * 2 2 2 2 2 2 2 2 2
 *  3   * 3 3 3 3 3 3 3 3 3
 *  4   * 4 4 4 4 4 4 4 4 4
 *  5   * 5 5 5 5 5 5 5 5 5
 *  6   * 6 6 6 6 6 6 6 6 6
 *  7   * 7 7 7 7 7 7 7 7 7
 * *****************************************************************
 * 同一行中8个像素点的值都相同都等于块左边对应的相邻的像素值
*/
static void predict_8x8c_h( uint8_t *src )
{
    int i;

    for( i = 0; i < 8; i++ )				//循环8次完成对8行像素值的预测
    {
        uint32_t v = 0x01010101 * src[-1];
        uint32_t *p = (uint32_t*)src;
        *p++ = v;
        *p++ = v;
        src += FDEC_STRIDE;
    }
}

/*
 * 
 * 用当前块的上面一行像素对当前块预测

 *      ↓↓↓↓↓↓↓↓
 *    * 0 1 2 3 4 5 6 7
 ***********************
 *  0 * 0 1 2 3 4 5 6 7
 *  1 * 0 1 2 3 4 5 6 7
 *  2 * 0 1 2 3 4 5 6 7
 *  3 * 0 1 2 3 4 5 6 7
 *  4 * 0 1 2 3 4 5 6 7
 *  5 * 0 1 2 3 4 5 6 7
 *  6 * 0 1 2 3 4 5 6 7
 *  7 * 0 1 2 3 4 5 6 7
 * 
*/
static void predict_8x8c_v( uint8_t *src )
{
    uint32_t v0 = *(uint32_t*)&src[0-FDEC_STRIDE];		//一次取出前四个参考像素
    uint32_t v1 = *(uint32_t*)&src[4-FDEC_STRIDE];		//取出后四个参考像素
    int i;

    for( i = 0; i < 8; i++ )					//循环8次完成对当前块8行像素值的预测(如右图)
    {
        uint32_t *p = (uint32_t*)src;			//
        *p++ = v0;								//循环一次对一行的前四个像素赋值
        *p++ = v1;								//循环一次对一行的后四个像素赋值
        src += FDEC_STRIDE;						//
    }
}

/*
 * 
 * 功能：用当前块的左边一列像素和上边一行像素联合起来对当前块预测,与16*16类似

 * -1 *  0  1  2  3  4  5  6  7
 ******************************
 *  0 *
 *  1 *
 *  2 *
 *  3 *        8x8块
 *  4 *
 *  5 *
 *  6 *
 *  7 *

*/
static void predict_8x8c_p( uint8_t *src )
{
    int i;
    int x,y;
    int a, b, c;
    int H = 0;
    int V = 0;
    int i00;

    for( i = 0; i < 4; i++ )
    {
        H += ( i + 1 ) * ( src[4+i - FDEC_STRIDE] - src[2 - i -FDEC_STRIDE] );			//H=1*（4-2）+2*(5-1)+3*(6-0)+4*(7--1)
        V += ( i + 1 ) * ( src[-1 +(i+4)*FDEC_STRIDE] - src[-1+(2-i)*FDEC_STRIDE] );	//V=1*（4-2）+2*(5-1)+3*(6-0)+4*(7--1)
    }

    a = 16 * ( src[-1+7*FDEC_STRIDE] + src[7 - FDEC_STRIDE] );	//a=16*(7+7)
    b = ( 17 * H + 16 ) >> 5;	//b=(17*H+16)/32
    c = ( 17 * V + 16 ) >> 5;	//c=(17*V+16)/32
    i00 = a -3*b -3*c + 16;

    for( y = 0; y < 8; y++ )
    {
        int pix = i00;
        for( x = 0; x < 8; x++ )	//predC[ x, y ] = Clip1C( ( a + b * ( x - 3 - xCF ) + c * ( y - 3 - yCF ) + 16 ) >> 5 ),
        {
            src[x] = x264_clip_uint8( pix>>5 );
            pix += b;
        }
        src += FDEC_STRIDE;
        i00 += c;
    }
}

/****************************************************************************
 * 4x4 prediction for intra luma block
 * 4x4 预测 帧内 亮度块
 ****************************************************************************/
/*
 * 
 * 功能：本函数对4*4块的每行像素赋同样的值
 * 给第一行的4个像素赋值
 * 给第二行的4个像素赋值
 * 给第三行的4个像素赋值
 * 给第四行的4个像素赋值
*/
#define PREDICT_4x4_DC(v) \
{\
    *(uint32_t*)&src[0*FDEC_STRIDE] =\
    *(uint32_t*)&src[1*FDEC_STRIDE] =\
    *(uint32_t*)&src[2*FDEC_STRIDE] =\
    *(uint32_t*)&src[3*FDEC_STRIDE] = v;\
}

/*
 *
 * 功能：本函数对4*4块的所有点赋值128
*/
static void predict_4x4_dc_128( uint8_t *src )
{
    PREDICT_4x4_DC(0x80808080);
}

/*
 *
 *
 * 功能：本函数用宏块左边四个像素（I、J、K、L）的均值对所有像素覆盖
*/
static void predict_4x4_dc_left( uint8_t *src )
{
    uint32_t dc = (( src[-1+0*FDEC_STRIDE] + src[-1+FDEC_STRIDE]+
                     src[-1+2*FDEC_STRIDE] + src[-1+3*FDEC_STRIDE] + 2 ) >> 2)*0x01010101;
    PREDICT_4x4_DC(dc);
}

/*
 *
 * 功能：本函数用宏块上方4个像素（A、B、C、D）的均值对所有像素覆盖
*/
static void predict_4x4_dc_top( uint8_t *src )
{
    uint32_t dc = (( src[0 - FDEC_STRIDE] + src[1 - FDEC_STRIDE] +
                     src[2 - FDEC_STRIDE] + src[3 - FDEC_STRIDE] + 2 ) >> 2)*0x01010101;
    PREDICT_4x4_DC(dc);
}

/*
 * 
 * 功能：本函数用左边和上边共8个像素（I、J、K、L 、A、B、C、D）的均值对所有像素覆盖
*/
static void predict_4x4_dc( uint8_t *src )
{
    uint32_t dc = (( src[-1+0*FDEC_STRIDE] + src[-1+FDEC_STRIDE] +
                     src[-1+2*FDEC_STRIDE] + src[-1+3*FDEC_STRIDE] +
                     src[0 - FDEC_STRIDE]  + src[1 - FDEC_STRIDE] +
                     src[2 - FDEC_STRIDE]  + src[3 - FDEC_STRIDE] + 4 ) >> 3)*0x01010101;
    PREDICT_4x4_DC(dc);
}

/*
 *
 * 功能：本函数用每行左边的像素对行进行覆盖（一行的所有像素值相同）
*/
static void predict_4x4_h( uint8_t *src )
{
    *(uint32_t*)&src[0*FDEC_STRIDE] = src[0*FDEC_STRIDE-1] * 0x01010101;
    *(uint32_t*)&src[1*FDEC_STRIDE] = src[1*FDEC_STRIDE-1] * 0x01010101;
    *(uint32_t*)&src[2*FDEC_STRIDE] = src[2*FDEC_STRIDE-1] * 0x01010101;
    *(uint32_t*)&src[3*FDEC_STRIDE] = src[3*FDEC_STRIDE-1] * 0x01010101;
}

/*
 * 
 * 功能：本函数用每列上方的像素对列进行覆盖（一列的所有像素值相同）
 *
 * 0 (Vertical)
 * *********************
 * M A B C D E F G H
 * I v v v v 
 * J v v v v
 * K v v v v 
 * L v v v v
*/
static void predict_4x4_v( uint8_t *src )
{
    uint32_t top = *((uint32_t*)&src[-FDEC_STRIDE]);	//取出当前块上方4个像素点的值
    PREDICT_4x4_DC(top);	//每行均用刚才取出的值覆盖
}

/*
 *
 * 功能：该定义表示依次取出当前宏块左边的4个像素
 * l0是第一行左边像素I
 * l1是第二行左边像素J
 * l2是第三行左边像素K
 * l3是第四行左边像素L
 * 
*/
#define PREDICT_4x4_LOAD_LEFT \
    const int l0 = src[-1+0*FDEC_STRIDE];   \
    const int l1 = src[-1+1*FDEC_STRIDE];   \
    const int l2 = src[-1+2*FDEC_STRIDE];   \
    UNUSED const int l3 = src[-1+3*FDEC_STRIDE];

/*
 * 
 * 功能：该定义表示依次取出当前宏块上方的4个像素
 * t0是第一列上方的像素A
 * t1是第二列上方的像素B
 * t2是第三列上方的像素C
 * t3是第四列上方的像素D
 * 
*/
#define PREDICT_4x4_LOAD_TOP \
    const int t0 = src[0-1*FDEC_STRIDE];   \
    const int t1 = src[1-1*FDEC_STRIDE];   \
    const int t2 = src[2-1*FDEC_STRIDE];   \
    UNUSED const int t3 = src[3-1*FDEC_STRIDE];

/*
 * 
 * 功能：该定义表示依次取出当前宏块右上方的4个像素
 * t4是右上方第一个像素E
 * t5是右上方第二个像素F
 * t6是右上方第三个像素G
 * t7是右上方第四个像素H
 * 
*/
#define PREDICT_4x4_LOAD_TOP_RIGHT \
    const int t4 = src[4-1*FDEC_STRIDE];   \
    const int t5 = src[5-1*FDEC_STRIDE];   \
    const int t6 = src[6-1*FDEC_STRIDE];   \
    UNUSED const int t7 = src[7-1*FDEC_STRIDE];

/*
 * 模式3 左下对角预测
 * 功能：45°右上至左下覆盖预测
 * 
 * 3 ()
 * *********************
 * M A B C D E F G H
 * I  
 * J 
 * K  
 * L 
 * 
 * 
 * 
*/
static void predict_4x4_ddl( uint8_t *src )
{
    PREDICT_4x4_LOAD_TOP			//A、B、C、D
    PREDICT_4x4_LOAD_TOP_RIGHT		//E、F、G、H

    src[0*FDEC_STRIDE+0] = ( t0 + 2*t1 + t2 + 2 ) >> 2;		//a=（A+2B+C+2）/4
															//+2表示四舍五入
    src[0*FDEC_STRIDE+1] =
    src[1*FDEC_STRIDE+0] = ( t1 + 2*t2 + t3 + 2 ) >> 2;		//b=e=（B+2C+D+2）/4

    src[0*FDEC_STRIDE+2] =
    src[1*FDEC_STRIDE+1] =
    src[2*FDEC_STRIDE+0] = ( t2 + 2*t3 + t4 + 2 ) >> 2;		//c=f=i=（C+2D+E+2）/4

    src[0*FDEC_STRIDE+3] =
    src[1*FDEC_STRIDE+2] =
    src[2*FDEC_STRIDE+1] =
    src[3*FDEC_STRIDE+0] = ( t3 + 2*t4 + t5 + 2 ) >> 2;		//d=g=j=m=（D+2E+F+2）/4

    src[1*FDEC_STRIDE+3] =
    src[2*FDEC_STRIDE+2] =
    src[3*FDEC_STRIDE+1] = ( t4 + 2*t5 + t6 + 2 ) >> 2;		//h=k=n=（E+2F+G+2）/4

    src[2*FDEC_STRIDE+3] =
    src[3*FDEC_STRIDE+2] = ( t5 + 2*t6 + t7 + 2 ) >> 2;		//i=o=（F+2G+H+2）/4

    src[3*FDEC_STRIDE+3] = ( t6 + 3*t7 + 2 ) >> 2;			//p=（G+3H+2）/4
}

/*
 * 模式4 右下对角预测
 * 功能：45°左上至右下覆盖预测
 *
 *
 * 4 ()
 * *********************
 * M A B C D E F G H
 * I a b c d
 * J e f g h
 * K i j k l
 * L m n o p
 *
*/
static void predict_4x4_ddr( uint8_t *src )
{
    const int lt = src[-1-FDEC_STRIDE];		//lt=M
    PREDICT_4x4_LOAD_LEFT					//I、J、K、L
    PREDICT_4x4_LOAD_TOP					//A、B、C、D

    src[0*FDEC_STRIDE+0] =
    src[1*FDEC_STRIDE+1] =
    src[2*FDEC_STRIDE+2] =
    src[3*FDEC_STRIDE+3] = ( t0 + 2 * lt + l0 + 2 ) >> 2;	//a=f=k=p=（A+2M+I+2）/4

    src[0*FDEC_STRIDE+1] =
    src[1*FDEC_STRIDE+2] =
    src[2*FDEC_STRIDE+3] = ( lt + 2 * t0 + t1 + 2 ) >> 2;	//b=g=l=（M+2A+B+2）/4

    src[0*FDEC_STRIDE+2] =
    src[1*FDEC_STRIDE+3] = ( t0 + 2 * t1 + t2 + 2 ) >> 2;	//c=h=（A+2B+C+2）/4

    src[0*FDEC_STRIDE+3] = ( t1 + 2 * t2 + t3 + 2 ) >> 2;	//d=（B+2C+D+2）/4

    src[1*FDEC_STRIDE+0] =
    src[2*FDEC_STRIDE+1] =
    src[3*FDEC_STRIDE+2] = ( lt + 2 * l0 + l1 + 2 ) >> 2;	//e=j=o=（M+2I+J+2）/4

    src[2*FDEC_STRIDE+0] =
    src[3*FDEC_STRIDE+1] = ( l0 + 2 * l1 + l2 + 2 ) >> 2;	//i=n=（I+2J+K+2）/4

    src[3*FDEC_STRIDE+0] = ( l1 + 2 * l2 + l3 + 2 ) >> 2;	//m=（J+2K+L+2）/4
}

/*
 * 模式5 垂直左下角
 * 
 * 功能：与y夹角26.6°左上至右下覆盖预测 （没用到L）
 * 
 *
 *
 *
 *
 *
 *
*/
static void predict_4x4_vr( uint8_t *src )
{
    const int lt = src[-1-FDEC_STRIDE];		//M
    PREDICT_4x4_LOAD_LEFT					//I、J、K、L
    PREDICT_4x4_LOAD_TOP					//A、B、C、D

    src[0*FDEC_STRIDE+0]=
    src[2*FDEC_STRIDE+1]= ( lt + t0 + 1 ) >> 1;	//a=j=（M+A+1）/2

    src[0*FDEC_STRIDE+1]=
    src[2*FDEC_STRIDE+2]= ( t0 + t1 + 1 ) >> 1;	//e=k=（A+B+1）/2

    src[0*FDEC_STRIDE+2]=
    src[2*FDEC_STRIDE+3]= ( t1 + t2 + 1 ) >> 1;	//c=i=（B+C+1）/2

    src[0*FDEC_STRIDE+3]= ( t2 + t3 + 1 ) >> 1;	//d=（C+D+1）/2

    src[1*FDEC_STRIDE+0]=
    src[3*FDEC_STRIDE+1]= ( l0 + 2 * lt + t0 + 2 ) >> 2;	//e=n=（I+2M+A+2）/4

    src[1*FDEC_STRIDE+1]=
    src[3*FDEC_STRIDE+2]= ( lt + 2 * t0 + t1 + 2 ) >> 2;	//f=o=（M+2A+B+2）/4

    src[1*FDEC_STRIDE+2]=
    src[3*FDEC_STRIDE+3]= ( t0 + 2 * t1 + t2 + 2) >> 2;		//g=p=（A+2B+C+2）/4

    src[1*FDEC_STRIDE+3]= ( t1 + 2 * t2 + t3 + 2 ) >> 2;	//h=（B+2C+D+2）/4
    src[2*FDEC_STRIDE+0]= ( lt + 2 * l0 + l1 + 2 ) >> 2;	//i=（M+2I+J+2）/4
    src[3*FDEC_STRIDE+0]= ( l0 + 2 * l1 + l2 + 2 ) >> 2;	//m=（I+2J+K+2）/4
}

/*
 * 模式6 水平斜下角
 * 
 * 功能：与x夹角26.6°左上至右下覆盖预测（没用到D）
 *
 * 
 * 
 * 
 * 
 * 
 * 
 * 
*/
static void predict_4x4_hd( uint8_t *src )
{
    const int lt= src[-1-1*FDEC_STRIDE];	//M
    PREDICT_4x4_LOAD_LEFT					//I、J、K、L
    PREDICT_4x4_LOAD_TOP					//A、B、C、D

    src[0*FDEC_STRIDE+0]=
    src[1*FDEC_STRIDE+2]= ( lt + l0 + 1 ) >> 1;		//a=g=（M+I+1）/2
    src[0*FDEC_STRIDE+1]=
    src[1*FDEC_STRIDE+3]= ( l0 + 2 * lt + t0 + 2 ) >> 2;	//b=h=（I+2M+A+2）/4
    src[0*FDEC_STRIDE+2]= ( lt + 2 * t0 + t1 + 2 ) >> 2;	//c=（M+2A+B+2）/4
    src[0*FDEC_STRIDE+3]= ( t0 + 2 * t1 + t2 + 2 ) >> 2;	//d=（A+2B+C+2）/4
    src[1*FDEC_STRIDE+0]=
    src[2*FDEC_STRIDE+2]= ( l0 + l1 + 1 ) >> 1;				//e=k=（I+J+1）/2
    src[1*FDEC_STRIDE+1]=
    src[2*FDEC_STRIDE+3]= ( lt + 2 * l0 + l1 + 2 ) >> 2;	//f=l=（M+2I+J+2）/4
    src[2*FDEC_STRIDE+0]=
    src[3*FDEC_STRIDE+2]= ( l1 + l2+ 1 ) >> 1;				//i=o=（J+K+1）/2
    src[2*FDEC_STRIDE+1]=
    src[3*FDEC_STRIDE+3]= ( l0 + 2 * l1 + l2 + 2 ) >> 2;	//j=p=（I+2J+K+2）/4
    src[3*FDEC_STRIDE+0]= ( l2 + l3 + 1 ) >> 1;				//m=（K+L+1）/2
    src[3*FDEC_STRIDE+1]= ( l1 + 2 * l2 + l3 + 2 ) >> 2;	//n=（J+2K+L+2）/4
}

/*
 * 模式7 垂直左下角
 * 与y夹角26.6°右上至左下覆盖预测（没用到H）

*/
static void predict_4x4_vl( uint8_t *src )
{
    PREDICT_4x4_LOAD_TOP							//A、B、C、D
    PREDICT_4x4_LOAD_TOP_RIGHT						//E、F、G、H

    src[0*FDEC_STRIDE+0]= ( t0 + t1 + 1 ) >> 1;		//a=（A+B+1）/2
    src[0*FDEC_STRIDE+1]=
    src[2*FDEC_STRIDE+0]= ( t1 + t2 + 1 ) >> 1;		//b=i=（B+C+1）/2
    src[0*FDEC_STRIDE+2]=
    src[2*FDEC_STRIDE+1]= ( t2 + t3 + 1 ) >> 1;		//c=j=（C+D+1）/2
    src[0*FDEC_STRIDE+3]=
    src[2*FDEC_STRIDE+2]= ( t3 + t4 + 1 ) >> 1;		//d=k=（D+E+1）/2
    src[2*FDEC_STRIDE+3]= ( t4 + t5 + 1 ) >> 1;		//l=（E+F+1）/2
    src[1*FDEC_STRIDE+0]= ( t0 + 2 * t1 + t2 + 2 ) >> 2;	//e=（A+2B+C+2）/4
    src[1*FDEC_STRIDE+1]=
    src[3*FDEC_STRIDE+0]= ( t1 + 2 * t2 + t3 + 2 ) >> 2;	//f=m=（B+2C+D+2）/4
    src[1*FDEC_STRIDE+2]=
    src[3*FDEC_STRIDE+1]= ( t2 + 2 * t3 + t4 + 2 ) >> 2;	//g=n=（C+2D+E+2）/4
    src[1*FDEC_STRIDE+3]=
    src[3*FDEC_STRIDE+2]= ( t3 + 2 * t4 + t5 + 2 ) >> 2;	//h=o=（D+2E+F+2）/4
    src[3*FDEC_STRIDE+3]= ( t4 + 2 * t5 + t6 + 2 ) >> 2;	//p=（E+2F+G+2）/4
}

/*
 * 模式8  水平斜上角
 * 与x夹角26.6°左下至右上覆盖预测
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
*/
static void predict_4x4_hu( uint8_t *src )
{
    PREDICT_4x4_LOAD_LEFT									//I、J、K、L

    src[0*FDEC_STRIDE+0]= ( l0 + l1 + 1 ) >> 1;				//a=（I+J+1）/2
    src[0*FDEC_STRIDE+1]= ( l0 + 2 * l1 + l2 + 2 ) >> 2;	//b=（I+2J+K+2）/4

    src[0*FDEC_STRIDE+2]=
    src[1*FDEC_STRIDE+0]= ( l1 + l2 + 1 ) >> 1;				//c=e=（J+K+1）/2

    src[0*FDEC_STRIDE+3]=
    src[1*FDEC_STRIDE+1]= ( l1 + 2*l2 + l3 + 2 ) >> 2;		//d=f=（J+2K+L+2）/4

    src[1*FDEC_STRIDE+2]=
    src[2*FDEC_STRIDE+0]= ( l2 + l3 + 1 ) >> 1;				//g=i=（K+L+1）/2

    src[1*FDEC_STRIDE+3]=
    src[2*FDEC_STRIDE+1]= ( l2 + 2 * l3 + l3 + 2 ) >> 2;	//h=j=（K+3L+2）/4

    src[2*FDEC_STRIDE+3]=
    src[3*FDEC_STRIDE+1]=
    src[3*FDEC_STRIDE+0]=
    src[2*FDEC_STRIDE+2]=
    src[3*FDEC_STRIDE+2]=
    src[3*FDEC_STRIDE+3]= l3;								//k=l=m=n=o=p=L
}

/****************************************************************************
 * 8x8 prediction for intra luma block
 * 8x8 预测 帧内 亮度块
 ****************************************************************************/

#define SRC(x,y) src[(x)+(y)*FDEC_STRIDE]
#define PL(y) \
    edge[14-y] = (SRC(-1,y-1) + 2*SRC(-1,y) + SRC(-1,y+1) + 2) >> 2;
#define PT(x) \
    edge[16+x] = (SRC(x-1,-1) + 2*SRC(x,-1) + SRC(x+1,-1) + 2) >> 2;

/*

*/
void x264_predict_8x8_filter( uint8_t *src, uint8_t edge[33], int i_neighbor, int i_filters )
{
    /* edge[7..14] = l7..l0
     * edge[15] = lt
     * edge[16..31] = t0 .. t15
     * edge[32] = t15 */

    int have_lt = i_neighbor & MB_TOPLEFT;
    if( i_filters & MB_LEFT )
    {
        edge[15] = (SRC(-1,0) + 2*SRC(-1,-1) + SRC(0,-1) + 2) >> 2;
        edge[14] = ((have_lt ? SRC(-1,-1) : SRC(-1,0))
                    + 2*SRC(-1,0) + SRC(-1,1) + 2) >> 2;
        PL(1) PL(2) PL(3) PL(4) PL(5) PL(6)
        edge[7] = (SRC(-1,6) + 3*SRC(-1,7) + 2) >> 2;
    }

    if( i_filters & MB_TOP )
    {
        int have_tr = i_neighbor & MB_TOPRIGHT;
        edge[16] = ((have_lt ? SRC(-1,-1) : SRC(0,-1))
                    + 2*SRC(0,-1) + SRC(1,-1) + 2) >> 2;
        PT(1) PT(2) PT(3) PT(4) PT(5) PT(6)
        edge[23] = ((have_tr ? SRC(8,-1) : SRC(7,-1))
                    + 2*SRC(7,-1) + SRC(6,-1) + 2) >> 2;

        if( i_filters & MB_TOPRIGHT )
        {
            if( have_tr )
            {
                PT(8) PT(9) PT(10) PT(11) PT(12) PT(13) PT(14)
                edge[31] =
                edge[32] = (SRC(14,-1) + 3*SRC(15,-1) + 2) >> 2;
            }
            else
            {
                //*(uint64_t*)(edge+24) = SRC(7,-1) * 0x0101010101010101ULL;
				*(uint64_t*)(edge+24) = SRC(7,-1) * 0x0101010101010101uI64; //lsp060515
                edge[32] = SRC(7,-1);
            }
        }
    }
}

#undef PL
#undef PT

#define PL(y) \
    UNUSED const int l##y = edge[14-y];
#define PT(x) \
    UNUSED const int t##x = edge[16+x];
#define PREDICT_8x8_LOAD_TOPLEFT \
    const int lt = edge[15];
#define PREDICT_8x8_LOAD_LEFT \
    PL(0) PL(1) PL(2) PL(3) PL(4) PL(5) PL(6) PL(7)
#define PREDICT_8x8_LOAD_TOP \
    PT(0) PT(1) PT(2) PT(3) PT(4) PT(5) PT(6) PT(7)
#define PREDICT_8x8_LOAD_TOPRIGHT \
    PT(8) PT(9) PT(10) PT(11) PT(12) PT(13) PT(14) PT(15)

#define PREDICT_8x8_DC(v) \
    int y; \
    for( y = 0; y < 8; y++ ) { \
        ((uint32_t*)src)[0] = \
        ((uint32_t*)src)[1] = v; \
        src += FDEC_STRIDE; \
    }

/*

*/
static void predict_8x8_dc_128( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_DC(0x80808080);
}

/*

*/
static void predict_8x8_dc_left( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_LEFT
    const uint32_t dc = ((l0+l1+l2+l3+l4+l5+l6+l7+4) >> 3) * 0x01010101;
    PREDICT_8x8_DC(dc);
}

/*

*/
static void predict_8x8_dc_top( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_TOP
    const uint32_t dc = ((t0+t1+t2+t3+t4+t5+t6+t7+4) >> 3) * 0x01010101;
    PREDICT_8x8_DC(dc);
}

/*

*/
static void predict_8x8_dc( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_LEFT
    PREDICT_8x8_LOAD_TOP
    const uint32_t dc = ((l0+l1+l2+l3+l4+l5+l6+l7
                         +t0+t1+t2+t3+t4+t5+t6+t7+8) >> 4) * 0x01010101;
    PREDICT_8x8_DC(dc);
}

/*

*/
static void predict_8x8_h( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_LEFT
#define ROW(y) ((uint32_t*)(src+y*FDEC_STRIDE))[0] =\
               ((uint32_t*)(src+y*FDEC_STRIDE))[1] = 0x01010101U * l##y
    ROW(0); ROW(1); ROW(2); ROW(3); ROW(4); ROW(5); ROW(6); ROW(7);
#undef ROW
}

/*

*/
static void predict_8x8_v( uint8_t *src, uint8_t edge[33] )
{
    const uint64_t top = *(uint64_t*)(edge+16);
    int y;
    for( y = 0; y < 8; y++ )
        *(uint64_t*)(src+y*FDEC_STRIDE) = top;
}

/*

*/
static void predict_8x8_ddl( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_TOP
    PREDICT_8x8_LOAD_TOPRIGHT
    SRC(0,0)= (t0 + 2*t1 + t2 + 2) >> 2;
    SRC(0,1)=SRC(1,0)= (t1 + 2*t2 + t3 + 2) >> 2;
    SRC(0,2)=SRC(1,1)=SRC(2,0)= (t2 + 2*t3 + t4 + 2) >> 2;
    SRC(0,3)=SRC(1,2)=SRC(2,1)=SRC(3,0)= (t3 + 2*t4 + t5 + 2) >> 2;
    SRC(0,4)=SRC(1,3)=SRC(2,2)=SRC(3,1)=SRC(4,0)= (t4 + 2*t5 + t6 + 2) >> 2;
    SRC(0,5)=SRC(1,4)=SRC(2,3)=SRC(3,2)=SRC(4,1)=SRC(5,0)= (t5 + 2*t6 + t7 + 2) >> 2;
    SRC(0,6)=SRC(1,5)=SRC(2,4)=SRC(3,3)=SRC(4,2)=SRC(5,1)=SRC(6,0)= (t6 + 2*t7 + t8 + 2) >> 2;
    SRC(0,7)=SRC(1,6)=SRC(2,5)=SRC(3,4)=SRC(4,3)=SRC(5,2)=SRC(6,1)=SRC(7,0)= (t7 + 2*t8 + t9 + 2) >> 2;
    SRC(1,7)=SRC(2,6)=SRC(3,5)=SRC(4,4)=SRC(5,3)=SRC(6,2)=SRC(7,1)= (t8 + 2*t9 + t10 + 2) >> 2;
    SRC(2,7)=SRC(3,6)=SRC(4,5)=SRC(5,4)=SRC(6,3)=SRC(7,2)= (t9 + 2*t10 + t11 + 2) >> 2;
    SRC(3,7)=SRC(4,6)=SRC(5,5)=SRC(6,4)=SRC(7,3)= (t10 + 2*t11 + t12 + 2) >> 2;
    SRC(4,7)=SRC(5,6)=SRC(6,5)=SRC(7,4)= (t11 + 2*t12 + t13 + 2) >> 2;
    SRC(5,7)=SRC(6,6)=SRC(7,5)= (t12 + 2*t13 + t14 + 2) >> 2;
    SRC(6,7)=SRC(7,6)= (t13 + 2*t14 + t15 + 2) >> 2;
    SRC(7,7)= (t14 + 3*t15 + 2) >> 2;
}

/*

*/
static void predict_8x8_ddr( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_TOP
    PREDICT_8x8_LOAD_LEFT
    PREDICT_8x8_LOAD_TOPLEFT
    SRC(0,7)= (l7 + 2*l6 + l5 + 2) >> 2;
    SRC(0,6)=SRC(1,7)= (l6 + 2*l5 + l4 + 2) >> 2;
    SRC(0,5)=SRC(1,6)=SRC(2,7)= (l5 + 2*l4 + l3 + 2) >> 2;
    SRC(0,4)=SRC(1,5)=SRC(2,6)=SRC(3,7)= (l4 + 2*l3 + l2 + 2) >> 2;
    SRC(0,3)=SRC(1,4)=SRC(2,5)=SRC(3,6)=SRC(4,7)= (l3 + 2*l2 + l1 + 2) >> 2;
    SRC(0,2)=SRC(1,3)=SRC(2,4)=SRC(3,5)=SRC(4,6)=SRC(5,7)= (l2 + 2*l1 + l0 + 2) >> 2;
    SRC(0,1)=SRC(1,2)=SRC(2,3)=SRC(3,4)=SRC(4,5)=SRC(5,6)=SRC(6,7)= (l1 + 2*l0 + lt + 2) >> 2;
    SRC(0,0)=SRC(1,1)=SRC(2,2)=SRC(3,3)=SRC(4,4)=SRC(5,5)=SRC(6,6)=SRC(7,7)= (l0 + 2*lt + t0 + 2) >> 2;
    SRC(1,0)=SRC(2,1)=SRC(3,2)=SRC(4,3)=SRC(5,4)=SRC(6,5)=SRC(7,6)= (lt + 2*t0 + t1 + 2) >> 2;
    SRC(2,0)=SRC(3,1)=SRC(4,2)=SRC(5,3)=SRC(6,4)=SRC(7,5)= (t0 + 2*t1 + t2 + 2) >> 2;
    SRC(3,0)=SRC(4,1)=SRC(5,2)=SRC(6,3)=SRC(7,4)= (t1 + 2*t2 + t3 + 2) >> 2;
    SRC(4,0)=SRC(5,1)=SRC(6,2)=SRC(7,3)= (t2 + 2*t3 + t4 + 2) >> 2;
    SRC(5,0)=SRC(6,1)=SRC(7,2)= (t3 + 2*t4 + t5 + 2) >> 2;
    SRC(6,0)=SRC(7,1)= (t4 + 2*t5 + t6 + 2) >> 2;
    SRC(7,0)= (t5 + 2*t6 + t7 + 2) >> 2;
  
}

/*

*/
static void predict_8x8_vr( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_TOP
    PREDICT_8x8_LOAD_LEFT
    PREDICT_8x8_LOAD_TOPLEFT
    SRC(0,6)= (l5 + 2*l4 + l3 + 2) >> 2;
    SRC(0,7)= (l6 + 2*l5 + l4 + 2) >> 2;
    SRC(0,4)=SRC(1,6)= (l3 + 2*l2 + l1 + 2) >> 2;
    SRC(0,5)=SRC(1,7)= (l4 + 2*l3 + l2 + 2) >> 2;
    SRC(0,2)=SRC(1,4)=SRC(2,6)= (l1 + 2*l0 + lt + 2) >> 2;
    SRC(0,3)=SRC(1,5)=SRC(2,7)= (l2 + 2*l1 + l0 + 2) >> 2;
    SRC(0,1)=SRC(1,3)=SRC(2,5)=SRC(3,7)= (l0 + 2*lt + t0 + 2) >> 2;
    SRC(0,0)=SRC(1,2)=SRC(2,4)=SRC(3,6)= (lt + t0 + 1) >> 1;
    SRC(1,1)=SRC(2,3)=SRC(3,5)=SRC(4,7)= (lt + 2*t0 + t1 + 2) >> 2;
    SRC(1,0)=SRC(2,2)=SRC(3,4)=SRC(4,6)= (t0 + t1 + 1) >> 1;
    SRC(2,1)=SRC(3,3)=SRC(4,5)=SRC(5,7)= (t0 + 2*t1 + t2 + 2) >> 2;
    SRC(2,0)=SRC(3,2)=SRC(4,4)=SRC(5,6)= (t1 + t2 + 1) >> 1;
    SRC(3,1)=SRC(4,3)=SRC(5,5)=SRC(6,7)= (t1 + 2*t2 + t3 + 2) >> 2;
    SRC(3,0)=SRC(4,2)=SRC(5,4)=SRC(6,6)= (t2 + t3 + 1) >> 1;
    SRC(4,1)=SRC(5,3)=SRC(6,5)=SRC(7,7)= (t2 + 2*t3 + t4 + 2) >> 2;
    SRC(4,0)=SRC(5,2)=SRC(6,4)=SRC(7,6)= (t3 + t4 + 1) >> 1;
    SRC(5,1)=SRC(6,3)=SRC(7,5)= (t3 + 2*t4 + t5 + 2) >> 2;
    SRC(5,0)=SRC(6,2)=SRC(7,4)= (t4 + t5 + 1) >> 1;
    SRC(6,1)=SRC(7,3)= (t4 + 2*t5 + t6 + 2) >> 2;
    SRC(6,0)=SRC(7,2)= (t5 + t6 + 1) >> 1;
    SRC(7,1)= (t5 + 2*t6 + t7 + 2) >> 2;
    SRC(7,0)= (t6 + t7 + 1) >> 1;
}

/*

*/
static void predict_8x8_hd( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_TOP
    PREDICT_8x8_LOAD_LEFT
    PREDICT_8x8_LOAD_TOPLEFT
    SRC(0,7)= (l6 + l7 + 1) >> 1;
    SRC(1,7)= (l5 + 2*l6 + l7 + 2) >> 2;
    SRC(0,6)=SRC(2,7)= (l5 + l6 + 1) >> 1;
    SRC(1,6)=SRC(3,7)= (l4 + 2*l5 + l6 + 2) >> 2;
    SRC(0,5)=SRC(2,6)=SRC(4,7)= (l4 + l5 + 1) >> 1;
    SRC(1,5)=SRC(3,6)=SRC(5,7)= (l3 + 2*l4 + l5 + 2) >> 2;
    SRC(0,4)=SRC(2,5)=SRC(4,6)=SRC(6,7)= (l3 + l4 + 1) >> 1;
    SRC(1,4)=SRC(3,5)=SRC(5,6)=SRC(7,7)= (l2 + 2*l3 + l4 + 2) >> 2;
    SRC(0,3)=SRC(2,4)=SRC(4,5)=SRC(6,6)= (l2 + l3 + 1) >> 1;
    SRC(1,3)=SRC(3,4)=SRC(5,5)=SRC(7,6)= (l1 + 2*l2 + l3 + 2) >> 2;
    SRC(0,2)=SRC(2,3)=SRC(4,4)=SRC(6,5)= (l1 + l2 + 1) >> 1;
    SRC(1,2)=SRC(3,3)=SRC(5,4)=SRC(7,5)= (l0 + 2*l1 + l2 + 2) >> 2;
    SRC(0,1)=SRC(2,2)=SRC(4,3)=SRC(6,4)= (l0 + l1 + 1) >> 1;
    SRC(1,1)=SRC(3,2)=SRC(5,3)=SRC(7,4)= (lt + 2*l0 + l1 + 2) >> 2;
    SRC(0,0)=SRC(2,1)=SRC(4,2)=SRC(6,3)= (lt + l0 + 1) >> 1;
    SRC(1,0)=SRC(3,1)=SRC(5,2)=SRC(7,3)= (l0 + 2*lt + t0 + 2) >> 2;
    SRC(2,0)=SRC(4,1)=SRC(6,2)= (t1 + 2*t0 + lt + 2) >> 2;
    SRC(3,0)=SRC(5,1)=SRC(7,2)= (t2 + 2*t1 + t0 + 2) >> 2;
    SRC(4,0)=SRC(6,1)= (t3 + 2*t2 + t1 + 2) >> 2;
    SRC(5,0)=SRC(7,1)= (t4 + 2*t3 + t2 + 2) >> 2;
    SRC(6,0)= (t5 + 2*t4 + t3 + 2) >> 2;
    SRC(7,0)= (t6 + 2*t5 + t4 + 2) >> 2;
}

/*

*/
static void predict_8x8_vl( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_TOP
    PREDICT_8x8_LOAD_TOPRIGHT
    SRC(0,0)= (t0 + t1 + 1) >> 1;
    SRC(0,1)= (t0 + 2*t1 + t2 + 2) >> 2;
    SRC(0,2)=SRC(1,0)= (t1 + t2 + 1) >> 1;
    SRC(0,3)=SRC(1,1)= (t1 + 2*t2 + t3 + 2) >> 2;
    SRC(0,4)=SRC(1,2)=SRC(2,0)= (t2 + t3 + 1) >> 1;
    SRC(0,5)=SRC(1,3)=SRC(2,1)= (t2 + 2*t3 + t4 + 2) >> 2;
    SRC(0,6)=SRC(1,4)=SRC(2,2)=SRC(3,0)= (t3 + t4 + 1) >> 1;
    SRC(0,7)=SRC(1,5)=SRC(2,3)=SRC(3,1)= (t3 + 2*t4 + t5 + 2) >> 2;
    SRC(1,6)=SRC(2,4)=SRC(3,2)=SRC(4,0)= (t4 + t5 + 1) >> 1;
    SRC(1,7)=SRC(2,5)=SRC(3,3)=SRC(4,1)= (t4 + 2*t5 + t6 + 2) >> 2;
    SRC(2,6)=SRC(3,4)=SRC(4,2)=SRC(5,0)= (t5 + t6 + 1) >> 1;
    SRC(2,7)=SRC(3,5)=SRC(4,3)=SRC(5,1)= (t5 + 2*t6 + t7 + 2) >> 2;
    SRC(3,6)=SRC(4,4)=SRC(5,2)=SRC(6,0)= (t6 + t7 + 1) >> 1;
    SRC(3,7)=SRC(4,5)=SRC(5,3)=SRC(6,1)= (t6 + 2*t7 + t8 + 2) >> 2;
    SRC(4,6)=SRC(5,4)=SRC(6,2)=SRC(7,0)= (t7 + t8 + 1) >> 1;
    SRC(4,7)=SRC(5,5)=SRC(6,3)=SRC(7,1)= (t7 + 2*t8 + t9 + 2) >> 2;
    SRC(5,6)=SRC(6,4)=SRC(7,2)= (t8 + t9 + 1) >> 1;
    SRC(5,7)=SRC(6,5)=SRC(7,3)= (t8 + 2*t9 + t10 + 2) >> 2;
    SRC(6,6)=SRC(7,4)= (t9 + t10 + 1) >> 1;
    SRC(6,7)=SRC(7,5)= (t9 + 2*t10 + t11 + 2) >> 2;
    SRC(7,6)= (t10 + t11 + 1) >> 1;
    SRC(7,7)= (t10 + 2*t11 + t12 + 2) >> 2;
}

/*

*/
static void predict_8x8_hu( uint8_t *src, uint8_t edge[33] )
{
    PREDICT_8x8_LOAD_LEFT
    SRC(0,0)= (l0 + l1 + 1) >> 1;
    SRC(1,0)= (l0 + 2*l1 + l2 + 2) >> 2;
    SRC(0,1)=SRC(2,0)= (l1 + l2 + 1) >> 1;
    SRC(1,1)=SRC(3,0)= (l1 + 2*l2 + l3 + 2) >> 2;
    SRC(0,2)=SRC(2,1)=SRC(4,0)= (l2 + l3 + 1) >> 1;
    SRC(1,2)=SRC(3,1)=SRC(5,0)= (l2 + 2*l3 + l4 + 2) >> 2;
    SRC(0,3)=SRC(2,2)=SRC(4,1)=SRC(6,0)= (l3 + l4 + 1) >> 1;
    SRC(1,3)=SRC(3,2)=SRC(5,1)=SRC(7,0)= (l3 + 2*l4 + l5 + 2) >> 2;
    SRC(0,4)=SRC(2,3)=SRC(4,2)=SRC(6,1)= (l4 + l5 + 1) >> 1;
    SRC(1,4)=SRC(3,3)=SRC(5,2)=SRC(7,1)= (l4 + 2*l5 + l6 + 2) >> 2;
    SRC(0,5)=SRC(2,4)=SRC(4,3)=SRC(6,2)= (l5 + l6 + 1) >> 1;
    SRC(1,5)=SRC(3,4)=SRC(5,3)=SRC(7,2)= (l5 + 2*l6 + l7 + 2) >> 2;
    SRC(0,6)=SRC(2,5)=SRC(4,4)=SRC(6,3)= (l6 + l7 + 1) >> 1;
    SRC(1,6)=SRC(3,5)=SRC(5,4)=SRC(7,3)= (l6 + 3*l7 + 2) >> 2;
    SRC(0,7)=SRC(1,7)=SRC(2,6)=SRC(2,7)=SRC(3,6)=
    SRC(3,7)=SRC(4,5)=SRC(4,6)=SRC(4,7)=SRC(5,5)=
    SRC(5,6)=SRC(5,7)=SRC(6,4)=SRC(6,5)=SRC(6,6)=
    SRC(6,7)=SRC(7,4)=SRC(7,5)=SRC(7,6)=SRC(7,7)= l7;
}

/****************************************************************************
 * Exported(输出) functions:
 * predict:预测
 * 
 * 
 * 
 * 
 * 
 * 
 * 资料：毕厚杰204页，16x16亮度块的帧内预测方式
 *
 * 预测模式序号			预测模式				注释
 *
 *		0			Intra_16x16_Vertical		垂直
 *		1			Intra_16x16_Horizontal		水平
 *		2			Intra_16x16_DC				 DC
 *		3			Intra_16x16_Plane			平面
 * 
 * 
 ****************************************************************************/
void x264_predict_16x16_init( int cpu, x264_predict_t pf[7] )
{
    pf[I_PRED_16x16_V ]     = predict_16x16_v;
    pf[I_PRED_16x16_H ]     = predict_16x16_h;
    pf[I_PRED_16x16_DC]     = predict_16x16_dc;
    pf[I_PRED_16x16_P ]     = predict_16x16_p;
    pf[I_PRED_16x16_DC_LEFT]= predict_16x16_dc_left;
    pf[I_PRED_16x16_DC_TOP ]= predict_16x16_dc_top;
    pf[I_PRED_16x16_DC_128 ]= predict_16x16_dc_128;

#ifdef HAVE_MMXEXT
    if( cpu&X264_CPU_MMXEXT )
    {
        x264_predict_16x16_init_mmxext( pf );
    }
#endif
}

/*
 * 
 * 
 * 帧内8x8色度块的预测方式
 *	预测模式序号		预测模式					注解
 * 
 *		0				Intra_Chroma_DC				 DC
 *		1				Intra_Chroma_Horizontal		水平
 *		2				Intra_Chroma_Vertical		垂直
 *		3				Intra_Chroma_Plane			平面
*/

void x264_predict_8x8c_init( int cpu, x264_predict_t pf[7] )
{
    pf[I_PRED_CHROMA_V ]     = predict_8x8c_v;
    pf[I_PRED_CHROMA_H ]     = predict_8x8c_h;
    pf[I_PRED_CHROMA_DC]     = predict_8x8c_dc;
    pf[I_PRED_CHROMA_P ]     = predict_8x8c_p;
    pf[I_PRED_CHROMA_DC_LEFT]= predict_8x8c_dc_left;
    pf[I_PRED_CHROMA_DC_TOP ]= predict_8x8c_dc_top;
    pf[I_PRED_CHROMA_DC_128 ]= predict_8x8c_dc_128;

#ifdef HAVE_MMXEXT
    if( cpu&X264_CPU_MMXEXT )
    {
        x264_predict_8x8c_init_mmxext( pf );
    }
#endif
}

/*
 *
 * 
 *				帧内8x8亮度块的预测方式
 ****************************************************************
 * 
 *		预测模式序号			预测模式				注解
 *			0					Vertical	(模式0)
 *			1					Horizontal(模式1)
 *			2					DC(模式2)
 *			3					Diagonal_Down_Left(模式3)
 *			4					Diagonal_Down_Right(模式4)
 *			5					Vertical_Right(模式5)
 *			6					Horizontal_Down(模式6)
 *			7					Vertical_Left(模式7)
 *			8					Horizontal_Up(模式8)
 *

*/
void x264_predict_8x8_init( int cpu, x264_predict8x8_t pf[12] )
{
    pf[I_PRED_8x8_V]      = predict_8x8_v;			//Vertical(模式0)
    pf[I_PRED_8x8_H]      = predict_8x8_h;			//Horizontal(模式1)
    pf[I_PRED_8x8_DC]     = predict_8x8_dc;			//DC(模式2)
    pf[I_PRED_8x8_DDL]    = predict_8x8_ddl;		//Diagonal_Down_Left(模式3)
    pf[I_PRED_8x8_DDR]    = predict_8x8_ddr;		//Diagonal_Down_Right(模式4)
    pf[I_PRED_8x8_VR]     = predict_8x8_vr;			//Vertical_Right(模式5)
    pf[I_PRED_8x8_HD]     = predict_8x8_hd;			//Horizontal_Down(模式6)
    pf[I_PRED_8x8_VL]     = predict_8x8_vl;			//Vertical_Left(模式7)
    pf[I_PRED_8x8_HU]     = predict_8x8_hu;			//Horizontal_Up(模式8)
    pf[I_PRED_8x8_DC_LEFT]= predict_8x8_dc_left;	//	
    pf[I_PRED_8x8_DC_TOP] = predict_8x8_dc_top;		//
    pf[I_PRED_8x8_DC_128] = predict_8x8_dc_128;		//

#ifdef HAVE_MMXEXT
    if( cpu&X264_CPU_MMXEXT )
    {
        x264_predict_8x8_init_mmxext( pf );
    }
#endif
#ifdef HAVE_SSE2
    if( cpu&X264_CPU_SSE2 )
    {
        x264_predict_8x8_init_sse2( pf );
    }
#endif
}

/*
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 资料：毕厚杰：第200页，4x4亮度块的帧内预测编码方式
*/
void x264_predict_4x4_init( int cpu, x264_predict_t pf[12] )
{
    pf[I_PRED_4x4_V]      = predict_4x4_v;			//Vertical(模式0)
    pf[I_PRED_4x4_H]      = predict_4x4_h;			//Horizontal(模式1)
    pf[I_PRED_4x4_DC]     = predict_4x4_dc;			//DC(模式2)
    pf[I_PRED_4x4_DDL]    = predict_4x4_ddl;		//Diagonal_Down_Left(模式3)
    pf[I_PRED_4x4_DDR]    = predict_4x4_ddr;		//Diagonal_Down_Right(模式4)
    pf[I_PRED_4x4_VR]     = predict_4x4_vr;			//Vertical_Right(模式5)
    pf[I_PRED_4x4_HD]     = predict_4x4_hd;			//Horizontal_Down(模式6)
    pf[I_PRED_4x4_VL]     = predict_4x4_vl;			//Vertical_Left(模式7)
    pf[I_PRED_4x4_HU]     = predict_4x4_hu;			//Horizontal_Up(模式8)
    pf[I_PRED_4x4_DC_LEFT]= predict_4x4_dc_left;
    pf[I_PRED_4x4_DC_TOP] = predict_4x4_dc_top;
    pf[I_PRED_4x4_DC_128] = predict_4x4_dc_128;

#ifdef HAVE_MMXEXT
    if( cpu&X264_CPU_MMXEXT )
    {
        x264_predict_4x4_init_mmxext( pf );
    }
#endif
}

