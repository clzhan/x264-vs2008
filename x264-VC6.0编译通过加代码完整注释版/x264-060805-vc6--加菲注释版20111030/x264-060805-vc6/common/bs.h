/*****************************************************************************
 * bs.h :
 * http://wmnmtm.blog.163.com/blog/static/382457142011724101824726/
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: bs.h,v 1.1 2004/06/03 19:27:06 fenrir Exp $
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
/*
函数名称：
函数功能：
参    数：
返 回 值：
思    路：
资    料：
*/

/*
注意：填充的时候先填充的右边低位
字节序，即字节在电脑中存放时的序列与输入（输出）时的序列是先到的在前还是后到的在前。
字节顺序是指占内存多于一个字节类型的数据在内存中的存放顺序，通常有小端、大端两种字节顺序。小端字节序指低字节数据存放在内存低地址处，高字节数据存放在内存高地址处；大端字节序是高字节数据存放在低地址处，低字节数据存放在高地址处。基于X86平台的PC机是小端字节序的，而有的嵌入式平台则是大端字节序的。

例子：如果我们将0x1234abcd写入到以0x0000开始的内存中，则结果为：

 addr	 big-endian   little-endian
0x0000		0x12		0xcd
0x0001		0x23		0xab
0x0002		0xab		0x34
0x0003		0xcd		0x12

网络字节顺序是TCP/IP中规定好的一种数据表示格式，它与具体的CPU类型、操作系统等无关，从而可以保证数据在不同主机之间传输时能够被正确解释。网络字节顺序采用big endian排序方式。

*/
#ifdef _BS_H
#warning FIXME Multiple inclusion of bs.h	
#else
#define _BS_H	//inclusion:包括,包含 ;Multiple:多重的, 多种多样的

typedef struct bs_s
{
    uint8_t *p_start;	//缓冲区首地址(这个开始是最低地址)
    uint8_t *p;			//缓冲区当前的读写指针 当前字节的地址，这个会不断的++，每次++，进入一个新的字节
    uint8_t *p_end;		//缓冲区尾地址		//typedef unsigned char   uint8_t;

    int     i_left;				/* p所指字节当前还有多少比特可读写 i_count number of available(可用的) bits */
    int     i_bits_encoded;		/* RD only */	//已编码的比特数???
} bs_t;		


/*
函数名称：
函数功能：初始化结构体
参    数：
返 回 值：无返回值,void类型
思    路：
资    料：
		  
*/
static inline void bs_init( bs_t *s, void *p_data, int i_data )
{
    s->p_start = (unsigned char *)p_data;		//用传入的p_data首地址初始化p_start，只记下有效数据的首地址
    s->p       = (unsigned char *)p_data;		//字节首地址，一开始用p_data初始化，每读完一个整字节，就移动到下一字节首地址
    s->p_end   = s->p + i_data;	//尾地址，最后一个字节的首地址?
    s->i_left  = 8;				//还没有开始读写，当前字节剩余未读取的位是8
}

/*
函数名称：
函数功能：获取当前流读取到哪个位置了，可能的返回值是，105、106....
返 回 值：
思    路：

		p_start		p_start+1	p_start+2	p_start+3(即:p_end)
		00000000	00000000	00000000	00000000

	
*/
static inline int bs_pos( bs_t *s )
{
    return( 8 * ( s->p - s->p_start ) + 8 - s->i_left );	//s->p - s->p_start是当前字节与流开始字节的差，
															//(8 - s->i_left) 是当前字节中已读取的位数
															//8 * ( s->p - s->p_start ) 是流中已读取过的整字节，所以乘以8，换算成比特数
}

/*
函数名称：
函数功能：判断是否是流结尾
返 回 值：1或0
思    路：bs_s结构体的对象的2个字段bs_t->p与bs->p_end进行比较
*/
static inline int bs_eof( bs_t *s )
{
    return( s->p >= s->p_end ? 1: 0 );	//s->p从低开始，++，直到=P_end
}

/*
该函数的作用是：从s中读出i_count位，并将其做为uint32_t类型返回
思路:
	若i_count>0且s流并未结束，则开始或继续读取码流；
	若s当前字节中剩余位数大于等于要读取的位数i_count，则直接读取；
	若s当前字节中剩余位数小于要读取的位数i_count，则读取剩余位，进入s下一字节继续读取。
补充:
	写入s时，i_left表示s当前字节还没被写入的位，若一个新的字节，则i_left=8；
	读取s时，i_left表示s当前字节还没被读取的位，若一个新的字节，则i_left＝8。
	注意两者的区别和联系。

	00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 0000000
	-------- -----000 00000000 ...
	写入s时：i_left = 3
	读取s时：i_left = 5

我思：
	字节流提前放在了结构体bs_s的对象bs_t里了，可能字节流不会一次性读取/分析完，而是根据需要，每次都读取几比特
	bs_s里，有专门的字段用来记录历史读取的结果,每次读取，都会在上次的读取位置上进行
	比如，100字节的流，经过若干次读取，当前位置处于中间一个字节处，前3个比特已经读取过了，此次要读取2比特

	00001001
	000 01 001 (已读过的 本次要读的 以后要读的 )
	i_count = 2	(计划去读2比特)
	i_left  = 5	(还有5比特未读，在本字节中)
	i_shr = s->i_left - i_count = 5 - 2 = 3
	*s->p >> i_shr，就把本次要读的比特移到了字节最右边(未读，但本次不需要的给移到了字节外，抛掉了)
	00000001
	i_mask[i_count] 即i_mask[2] 即0x03:00000011
	( *s->p >> i_shr )&i_mask[i_count]; 即00000001 & 00000011 也就是00000001 按位与 00000011
	结果是：00000001
	i_result |= ( *s->p >> i_shr )&i_mask[i_count];即i_result |=00000001 也就是 i_result =i_result | 00000001 = 00000000 00000000 00000000 00000000 | 00000001 =00000000 00000000 00000000 00000001
	i_result =
	return( i_result ); 返回的i_result是4字节长度的，是unsigned类型 sizeof(unsigned)=4
*/

static inline uint32_t bs_read( bs_t *s, int i_count )
{
	 static uint32_t i_mask[33] ={0x00,
                                  0x01,      0x03,      0x07,      0x0f,
                                  0x1f,      0x3f,      0x7f,      0xff,
                                  0x1ff,     0x3ff,     0x7ff,     0xfff,
                                  0x1fff,    0x3fff,    0x7fff,    0xffff,
                                  0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
                                  0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
                                  0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
                                  0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};
	/*
			  数组中的元素用二进制表示如下：

			  假设：初始为0，已写入为+，已读取为-
			  
			  字节:		1		2		3		4
				   00000000 00000000 00000000 00000000		下标

			  0x00:							  00000000		x[0]

			  0x01:							  00000001		x[1]
			  0x03:							  00000011		x[2]
			  0x07:							  00000111		x[3]
			  0x0f:							  00001111		x[4]

			  0x1f:							  00011111		x[5]
			  0x3f:							  00111111		x[6]
			  0x7f:							  01111111		x[7]
			  0xff:							  11111111		x[8]	1字节

			 0x1ff:						 0001 11111111		x[9]
			 0x3ff:						 0011 11111111		x[10]	i_mask[s->i_left]
			 0x7ff:						 0111 11111111		x[11]
			 0xfff:						 1111 11111111		x[12]	1.5字节

			0x1fff:					 00011111 11111111		x[13]
			0x3fff:					 00111111 11111111		x[14]
			0x7fff:					 01111111 11111111		x[15]
			0xffff:					 11111111 11111111		x[16]	2字节

		   0x1ffff:				0001 11111111 11111111		x[17]
		   0x3ffff:				0011 11111111 11111111		x[18]
		   0x7ffff:				0111 11111111 11111111		x[19]
		   0xfffff:				1111 11111111 11111111		x[20]	2.5字节

		  0x1fffff:			00011111 11111111 11111111		x[21]
		  0x3fffff:			00111111 11111111 11111111		x[22]
		  0x7fffff:			01111111 11111111 11111111		x[23]
		  0xffffff:			11111111 11111111 11111111		x[24]	3字节

		 0x1ffffff:	   0001 11111111 11111111 11111111		x[25]
		 0x3ffffff:	   0011 11111111 11111111 11111111		x[26]
		 0x7ffffff:    0111 11111111 11111111 11111111		x[27]
		 0xfffffff:    1111 11111111 11111111 11111111		x[28]	3.5字节

		0x1fffffff:00011111 11111111 11111111 11111111		x[29]
		0x3fffffff:00111111 11111111 11111111 11111111		x[30]
		0x7fffffff:01111111 11111111 11111111 11111111		x[31]
		0xffffffff:11111111 11111111 11111111 11111111		x[32]	4字节

	 */
    int      i_shr;			//
    uint32_t i_result = 0;	//用来存放读取到的的结果 typedef unsigned   uint32_t;

    while( i_count > 0 )	//要读取的比特数
    {
        if( s->p >= s->p_end )	//字节流的当前位置>=流结尾，即代表此比特流s已经读完了。
        {						//
            break;
        }

        if( ( i_shr = s->i_left - i_count ) >= 0 )	//当前字节剩余的未读位数，比要读取的位数多，或者相等
        {											//i_left当前字节剩余的未读位数，本次要读i_count比特，i_shr=i_left-i_count的结果如果>=0，说明要读取的都在当前字节内
													//i_shr>=0，说明要读取的比特都处于当前字节内
			//这个阶段，一次性就读完了，然后返回i_result(退出了函数)
            /* more in the buffer than requested */
            i_result |= ( *s->p >> i_shr )&i_mask[i_count];//“|=”:按位或赋值，A |= B 即 A = A|B
									//|=应该在最后执行，把结果放在i_result(按位与优先级高于复合操作符|=)
									//i_mask[i_count]最右侧各位都是1,与括号中的按位与，可以把括号中的结果复制过来
									//!=,左边的i_result在这儿全是0，右侧与它按位或，还是复制结果过来了，好象好几步都多余
			/*读取后，更新结构体里的字段值*/
            s->i_left -= i_count; //即i_left = i_left - i_count，当前字节剩余的未读位数，原来的减去这次读取的
            if( s->i_left == 0 )	//如果当前字节剩余的未读位数正好是0，说明当前字节读完了，就要开始下一个字节
            {
                s->p++;				//移动指针，所以p好象是以字节为步长移动指针的
                s->i_left = 8;		//新开始的这个字节来说，当前字节剩余的未读位数，就是8比特了
            }
            return( i_result );		//可能的返回值之一为：00000000 00000000 00000000 00000001 (4字节长)
        }
        else	/* i_shr < 0 ,跨字节的情况*/
        {
			//这个阶段，是while的一次循环，可能还会进入下一次循环，第一次和最后一次都可能读取的非整字节，比如第一次读了3比特，中间读取了2字节(即2x8比特)，最后一次读取了1比特，然后退出while循环
			//当前字节剩余的未读位数，比要读取的位数少，比如当前字节有3位未读过，而本次要读7位
			//???对当前字节来说，要读的比特，都在最右边，所以不再移位了(移位的目的是把要读的比特放在当前字节最右)
            /* less(较少的) in the buffer than requested */
			i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;	//"-i_shr"相当于取了绝对值
									//|= 和 << 都是位操作符，优先级相同，所以从左往右顺序执行
									//举例:int|char ，其中int是4字节，char是1字节，sizeof(int|char)是4字节
									//i_left最大是8，最小是0，取值范围是[0,8]
			i_count  -= s->i_left;	//待读取的比特数，等于原i_count减去i_left，i_left是当前字节未读过的比特数，而此else阶段，i_left代表的当前字节未读的比特全被读过了，所以减它
			s->p++;	//定位到下一个新的字节
			s->i_left = 8;	//对一个新字节来说，未读过的位数当然是8，即本字节所有位都没读取过
        }
    }

    return( i_result );//可能的返回值之一为：00000000 00000000 00000000 00000001 (4字节长)
}
/*
函数名称：
函数功能：从s中读出1位，并将其做为uint32_t类型返回。
函数参数：
返 回 值：
思    路：若s流并未结束，则读取一位
资    料：
		毕厚杰：第145页，u(n)/u(v)，读进连续的若干比特，并将它们解释为“无符号整数”
		return i_result;	//unsigned int
*/
static inline uint32_t bs_read1( bs_t *s )
{

    if( s->p < s->p_end )	//
    {
        unsigned int i_result;

        s->i_left--;//当前字节未读取的位数少了1位
        i_result = ( *s->p >> s->i_left )&0x01;//把要读的比特移到当前字节最右，然后与0x01:00000001进行逻辑与操作，因为要读的只是一个比特，这个比特不是0就是1，与0000 0001按位与就可以得知此情况
        if( s->i_left == 0 )//如果当前字节剩余未读位数是0，即是说当前字节全读过了
        {
            s->p++;			//指针s->p 移到下一字节
            s->i_left = 8;	//新字节中，未读位数当然是8位
        }
        return i_result;//unsigned int
    }

    return 0;//返回0应该是没有读到东西
}

/*
函数名称：
函数功能：从流中取出一段用于显示，长度必须限制在<=4字节
返 回 值：如 00010100 00000111 00111111 00000011 (最前面的3比特0可能是填充用，这种情况是读取24-3比特长度)
思    路：
		 比特流：00000000 00000000 00000000 00000000 10101000 10000001 00000011 01110000
		 结  果：									 10101000 10000001 00000011 01110000(4字节，类型为uint32_t)


*/
static inline uint32_t bs_show( bs_t *s, int i_count )
{
    if( s->p < s->p_end && i_count > 0 )	//
    {
        uint32_t i_cache = ((s->p[0] << 24)+(s->p[1] << 16)+(s->p[2] << 8)+s->p[3]) << (8-s->i_left);
										//从当前字节处，把比特流当作字节数组，p[0],p[1],p[2],p[3]是四个字节
										//把这四个字节按顺序放到一个uint32_t的对象中，此对象长度也是4个字节
										//这步的结果是截取的流段处于i_cache的最左边，右边可能多了一部分无用的比特，下一步会进行纠正
        return( i_cache >> ( 32 - i_count) );//把有效内容放在最右侧，左侧以0填充
    }
    return 0;
}

/*
函数名称：
函数功能：跳过流中的若干比特
参    数：
返 回 值：
思    路：
*/
/* TODO optimize(优化,使最优化) */
static inline void bs_skip( bs_t *s, int i_count )
{
    s->i_left -= i_count;	//当前字节剩余的未读取位数 = 原剩余未读位数 - 要跳过的比特数
							//i_left = i_left - i_count
							//有两种情况，	1、跳过若干比特，仍在当前字节
							//				2、跳跃起后，进入新的字节

    while( s->i_left <= 0 )	//当要跳到下一个字节的时候，满足此条件
    {
        s->p++;				//到下一字节，针对跨字节的情况
        s->i_left += 8;		//到了新的字节，+=8会与函数第一句的-=抵消，结果最终是正确的
    }
}

/*
函数名称：
函数功能：从s中解码并读出一个语法元素值
参    数：
返 回 值：
思    路：
		从s的当前位读取并计数，直至读取到1为止；
		while( bs_read1( s ) == 0 && s->p < s->p_end && i < 32 )这个循环用i记录了s当前位置到1为止的0的个数，并丢弃读到的第一个1；
		返回2^i-1+bs_read(s,i)。
		例：当s字节中存放的是0001010时，1前有3个0，所以i＝3；
		返回的是：2^i-1+bs_read(s,i)即：8－1＋010＝9
资    料：
		毕厚杰：第145页，ue(v)；无符号指数Golomb熵编码
		x264中bs.h文件部分函数解读 http://wmnmtm.blog.163.com/blog/static/382457142011724101824726/
		无符号整数指数哥伦布码编码 http://wmnmtm.blog.163.com/blog/static/38245714201172623027946/
*/
static inline int bs_read_ue( bs_t *s )
{
    int i = 0;

    while( bs_read1( s ) == 0 && s->p < s->p_end && i < 32 )	//条件为：读到的当前比特=0，指针未越界，最多只能读32比特
    {
        i++;
    }
    return( ( 1 << i) - 1 + bs_read( s, i ) );	
}

/*
函数名称：
函数功能：通过ue（v）编码实现se（v）编码的读取。
参    数：
返 回 值：
思    路：
		直接bs_read_ue( s )读取出来的实际上是codenum的值，
		因为“当val<=0时，codenum＝-val*2；否则，codenum=val*2-1。”,
		所以当codenum为奇数即 codenum&0x01>0 时，val＝（codenum＋1）/2，否则val=-(codenum/2)。
资    料：
		毕厚杰：第145页，se(v)；有符号指数Golomb熵编码


*/
static inline int bs_read_se( bs_t *s )
{
    int val = bs_read_ue( s );

    return val&0x01 ? (val+1)/2 : -(val/2);	//&按位与操作符，需要两个整值操作数。在每个位所在处，如果两个操作数都含有1，则结果该位为1，否则为0
}											//解释为：如果val&0x01为真，返回(val+1)/2，否则返回-(val/2)

/*
函数名称：
函数功能：通过ue（v）编码实现te（v）编码的读取。
参    数：
返 回 值：
思    路：
		当x＝1时，直接读出一位，然后取反；当x>1时，读取方式同ue(v)。
资    料：
		毕厚杰：第145页，te(v)：截断指数Golomb熵编码
*/
static inline int bs_read_te( bs_t *s, int x )
{
    if( x == 1 )
    {
        return 1 - bs_read1( s );
    }
    else if( x > 1 )
    {
        return bs_read_ue( s );
    }
    return 0;
}


/*
函数名称：
函数功能：向s里写入i_bits流的后i_count位。
参    数：( bs_t *s 流结构, int i_count 要写入的比特数, uint32_t i_bits 要写入的值 )
返 回 值：
思    路：
资    料：
流程简介：
		首先判断是否有空闲的存储空间供新的码流写入，若剩余不足4个字节的空间，则认为
		空间不够，直接返回（uint32_t是32位，要4个字节的存储空间）； i_count是否大于0，
		如果是，则可以写入码流。这个条件是在判断是否该开始或继续写入码流。

		若可以写码流，若i_count<32,则i_bits &= (1<<i_count)-1，将不需要写入的位“置0”。
		注意，该函数写i_bits不是逐位写入的。
		如果要写入的位数i_count < 本字节空余的位数s->i_left，则一次性将i_count位写入：             *s->p = (*s->p << i_count) | i_bits且s->i_left -= i_count，然后完成写入break；
		否则，则先写入i_left位：
		*s->p = (*s->p << s->i_left) | (i_bits >> (i_count - s->i_left))，i_count -= s->i_left
		（要写入的数减少），s->p++（进入s的下一个字节），i_left重新置位为8。
		while循环直至写入i_count位。

		注：该函数同1）小节中介绍的函数作用一样，不过效率要高。后面函数流程就不再详细
		介绍了，只说明大概思路。


*/

static inline void bs_write( bs_t *s, int i_count, uint32_t i_bits )
{
    if( s->p >= s->p_end - 4 )	//够4字节才可以写入，因为uint32_t是32位，要4个字节的存储空间
	{
        return;
	}

    while( i_count > 0 )
    {
        if( i_count < 32 )
		{
			// i_count 是要写入的位数，假设是i_count=30
			// 1					:							 00000001
			// 00000001 << 30		: 01000000 00000000 00000000 00000000(左移几位后面有几个0)
			// 00000001 << 30 - 1	: 00111111 11111111 11111111 11111111
			// i_bits = i_bits &	  00111111 11111111 11111111 11111111
            i_bits &= (1<<i_count)-1;//&=按位与赋值操作符 《C++Primer第三版中文版》第136页
									//A &= B => A = A & B
									//
		}
		//如果当前字节就能放下
        if( i_count < s->i_left ) //要写入的位数i_count < 本字节空余的位数s->i_left，则一次性将i_count位写入
        {
			//把当前字节里的数据左移i_count，然后与i_bits执行位运算
			//现在，有数据的位，都在当前字节的右边，如果还有位没写数据，它会是在左边
            *s->p = (*s->p << i_count) | i_bits;//*s->p => *(s->p)
												//????????完成写入
            s->i_left -= i_count;//记下当前字节还空的位数
            break;//这儿直接就退出while循环
        }
		//如果当前字节剩余位放不下
        else
        {
			// (*s->p << s->i_left) 把当前字节的数据位左移，空位，即待写位就在右边了
			// (i_bits >> (i_count - s->i_left)) 把本次要写的数据移在最右边
			// 例如：
			// i_count 是要写入的位数，假设是i_count=30
			// 下面这段在if外做好了
			// 1:00000001
			// 00000001 << 30		: 01000000 00000000 00000000 00000000
			// 00000001 << 30 - 1	: 00111111 11111111 11111111 11111111
			// i_bits = i_bits &	  00111111 11111111 11111111 11111111
			// 下面这段是本else分支的
			// 假设当前字节有3位是空的，现在它还在当前字节的左侧
			// (*s->p << s->i_left) 把数据往左移3位，数据都居左了，空位在右边了
			// 接下来要做的，是把需要写的数据进行一下处理
			// 本次只能写当前字节的三位，所以把源数据对应位移在最右侧进行按位或
			// 比如，一共30位，本次写3位，把源数据右移 30 -3 即27
			// 00111111 11111111 11111111 11111111 >> 27
			// 00000000 00000000 00000000 00000111
			// 现在源的待写数据都在最右侧了，用它和当前字节进行按位或运算
			// 就完成了当前字节的写入
			// 然后进行一些相关的信息记录，比如更新指针和i_count

			//下一次循环，会根据新的i_count再进行一次同样的操作，注意两次的i_count是不一样的了


            *s->p = (*s->p << s->i_left) | (i_bits >> (i_count - s->i_left));
            i_count -= s->i_left;	//这儿更新循环条件
									//剩余待写的位数
            s->p++;
            s->i_left = 8;
        }
    }
}

/*
**************************************************************************
函数名称：网上的同功能函数，仅供参考、比较
函数功能：同bs_write函数，只是一次只写入1比特，多次循环直到完成

1） static inline void bs_write( bs_t *s, int i_count, uint32_t i_bits )
该函数的作用是：向s里写入i_bits流的后i_count位，s以字节为单位，所以够8个位就写下个（注意i_bits的单位是uint32_t）。 
static inline void bs_write( bs_t *s, int i_count, uint32_t i_bits )
{
    while( i_count > 0 )
    {
        if( s->p >= s->p_end )
        {
            break;
        }
        i_count--;
        if( ( i_bits >> i_count )&0x01 )
        {
            *s->p |= 1 << ( s->i_left - 1 );
        }
        else
        {
            *s->p &= ~( 1 << ( s->i_left - 1 ) );
        }
        s->i_left--;
        if( s->i_left == 0 )
        {
            s->p++;
            s->i_left = 8;
        }
    }
}
函数功能：
i_count是循环的次数即要写入的位数，i_bits是要编码的数，i_left是当前空闲码流的位数即记录当前字节的空余位数。将i_bits编为i_count位的码流，每次循环，i_count和i_left都会减1，i_count和i_left并不一定相等。当i_left==0时，s->p指针指向下一个码流单元，i_left更新为8。
 
函数流程：
首先判断i_count是否大于0，如果是，则判断s->p是否大于s->p_end，若是则终止，否则，可以写入码流。
这个条件是在判断是否有空闲的存储空间供新的码流写入。
若可以写码流，则i_count--,表明可以进行写一位的操作。注意，写i_bits是逐位写入的。
if( ( i_bits >> i_count )&0x01 )是用来判断当前要写入的i_bits位是0还是1，从而选择不同的算法来写入这个码流。如果当前要写入的是0，则选择*s->p &= ~( 1 << ( s->i_left - 1 ) )来把这个0写入码流；如果当前要写入的是1，则选择*s->p |= 1 << ( s->i_left - 1 )来把这个1写入码流。
   
写完一位码流后，初始的i_left被新写入的bit占了一位,所以i_left的值-1.
   这时判断I_left是否等于0，如果i_left还大于0，表示当前的存储单元中还有剩余的空间供码流写入，则直接进入下一次循环。如果i_left==0时，表示当前存储单元已被写满，所以s->p指针指向下一个存储单元，i_left更新为8。这时再进入下一循环。
   
在进入下一循环的时候，先判断i_count的值，如果非零，程序继续；如果为0，表示码流已经全部写入，程序终止。

注：该函数是采用一个while循环，一次写入一个位，循环i_count次将i_bits的前i_count位写入s中。


2）static inline void bs_write1( bs_t *s, uint32_t i_bits )
该函数的作用是：将i_bits流的后1位写入s。

static inline void bs_write1( bs_t *s, uint32_t i_bits )
{
    if( s->p < s->p_end )
    {    
        if( i_bits&0x01 )
        {
            *s->p |= 1 <<( s->i_left-1);
        }
        else
        {
            *s->p &= ~( 1 << (s->i_left-1) );
        }
         s->i_left--;
        if( s->i_left == 0 )
        {
            s->p++;
            s->i_left = 8;
        }
    }
}
 
bs_write1（）相当于bs_write(bs_t *s,1, uint32_t i_bits) 就是只写入1 bit码流。

函数流程：
首先判断s->p是否大于s->p_end，若是则终止，否则，可以写入码流。
if(  i_bits &0x01 )是用来判断当前要写入的i_bits位是0还是1，如果当前要写入的是1，则选择*s->p |= 1 <<( s->i_left-1)来把这个1写入码流；如果当前要写入的是0，则选择            *s->p &= ~( 1 << (s->i_left-1) )来把这个0写入码流。
   
写完一位码流后，初始的i_left被新写入的bit占了一位,所以i_left的值-1.
这时判断I_left是否等于0，如果i_left==0时，表示当前存储单元已被写满，所以s->p指针指向下一个存储单元，i_left更新为8。这时再进入下一循环。
   
注：上面两个write函数是从网上找的，一次写入一个位数，先判断要写入的位是0或1，再直接写入0或1，比较繁琐但是便于理解，故此附上以供参考。

***************************************************************************
*/


/*
该函数的作用是：将i_bits流的后1位写入s。
思路：直接写入*s->p <<= 1;
             *s->p |= i_bit;
 * 针对*s->p |= i_bit;一边是char，一边是32位的，能否将char最末位置1，进行了验证，是可以的
 * http://wmnmtm.blog.163.com/blog/static/38245714201192523012109/
*/
static inline void bs_write1( bs_t *s, uint32_t i_bit )
{
    if( s->p < s->p_end )		//当前字节的“字节起始地址” < 流的尾地址
    {
        *s->p <<= 1;			/* A <<= 1  => A = A << 1 */
								//当前字节里的二进制位左移1位，空出的一位用0填充,这是为了后面放新的1位
        *s->p |= i_bit;			/* 按位或赋值 A |= B => A = A|B */
								//流s的当前字段与i_bit按位或，是不是应该保证i_bit最后一位前的其它位都为0???
        s->i_left--;			/* 更新i_left，当前字节剩余的未被写入的位减1 */
        if( s->i_left == 0 )	/*  */
        {
            s->p++;				/* 移到新字节 注意这儿地址是向上加的 */
            s->i_left = 8;		/* 当前字节剩余的未被读取的位 */
        }
    }
}

/*
函数名称：
函数功能：向后调整比特流的读写位置，使其处于字节对齐位置；中间这些跳过的比特位全部置0
参    数：
返 回 值：
思    路：
资    料：
*/
static inline void bs_align_0( bs_t *s )	/*  */
{
    if( s->i_left != 8 )		/* 当前字节“剩（五笔:tuxj）余未读取字节i_left不为8” */
    {							// 比如当前字节只写入3比特，还有5比特没写数据
        *s->p <<= s->i_left;	/* (*s->p) = (*s->p) << s->i_left */
								// 这儿不知道为什么要左移
        s->i_left = 8;			/* p所指字节当前还有多少比特可读写 */
        s->p++;					/* 缓冲区当前的读写指针 当前字节的地址,移到下一字节 */
								//现在指针在一个新字节的开始
    }
}

/*
函数名称：
函数功能：向后调整比特流的读写位置，使其处于字节对齐位置；中间这些跳过的比特位全部置1
参    数：
返 回 值：
思    路：
资    料：
*/
static inline void bs_align_1( bs_t *s )	/*  */
{
    if( s->i_left != 8 )					/*  */
    {
        *s->p <<= s->i_left;				/*  */
        *s->p |= (1 << s->i_left) - 1;		/*  */
        s->i_left = 8;						/*  */
        s->p++;								/*  */
    }
}

/*
函数名称：同bs_align_0
函数功能：向后调整比特流的读写位置，使其处于字节对齐位置；中间这些跳过的比特位全部置0
参    数：
返 回 值：
思    路：
资    料：
*/
static inline void bs_align( bs_t *s )
{
    bs_align_0( s );						/*  */
}



/* 
golomb(变长编码算法) functions 
指数哥伦布编码ue(v)

该函数的作用是：将val以哥伦布编码ue（v）方式编码并写入s中。

思路：


例子：




*/
/*
函数名称：
函数功能：指数哥伦布编码ue（v）
参    数：将val以哥伦布编码ue（v）方式编码并写入s中
返 回 值：
思    路：当val＝0时，写入1；
		  当val!＝0时：该函数主要是通过i_size0_255[256]表计算出一个i_size,然后写入val的2*i_size－1位，注意
		  tmp＝＋＋val，实际写入的val比传入的val大1。
资    料：
		  when val＝9 ，val!=0, so tmp=10;
		  tmp<0x00010000 and tmp<0x100, so i_size=0+i_size0_255[10]=4;
		  tmp=10=00001010  and 2*i_size-1=7 ,so bs_write(s,7,00001010);
		  由上所述：当val＝9时，写入s的字串为：0001010
*/
static inline void bs_write_ue( bs_t *s, unsigned int val )
{
    int i_size = 0;
    static const int i_size0_255[256] =
    {
        1,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
    };
	/*
	
	  1		2
	  2		2
	  3		4
	  4		8
	  5		16
	  6		32
	  7		64
	  8		128
	*/

    if( val == 0 )
    {
        bs_write1( s, 1 );
    }
    else
    {
        unsigned int tmp = ++val;

        if( tmp >= 0x00010000 )
        {
            i_size += 16;
            tmp >>= 16;
        }
        if( tmp >= 0x100 )
        {
            i_size += 8;
            tmp >>= 8;
        }
        i_size += i_size0_255[tmp];

        bs_write( s, 2 * i_size - 1, val );
    }
}


/*
函数名称：指数哥伦布编码se(v)
函数功能：通过ue(v)编码实现se(v)编码的写入。
参    数：
返 回 值：
思    路：se(v)表示有符号指数哥伦布编码,当val<=0时，codenum＝-val*2；否则，codenum=val*2-1。
		  然后,bs_write_ue(s,codenum)；
资    料：
		在标准中的codenum只是一个假设的中间值，就像在函数中设置的一个变量一样，val才是要得到的语法元素值。
		在ue（v）编码中，val=codenum,标准中也这样描述：“如果语法元素编码为ue(v)，语法元素值等于codeNum。”
		在se（v）编码中，val与codenum的关系如下：当val<=0时，codenum＝-val*2；否则，codenum=val*2-1（参看标准9.1.1小节中的表9-3）。
*/
static inline void bs_write_se( bs_t *s, int val )
{
    bs_write_ue( s, val <= 0 ? -val * 2 : val * 2 - 1);
}


/*
函数名称：
函数功能：通过ue(v)编码实现te(v)编码的写入。
参    数：x表示语法元素的范围。注意参考标准中关于te(v)与ue(v)关系的描述。
返 回 值：
思    路：当x＝1时，将val最后一位的取反，然后写入；当x>1时，编码方式同ue(v)。
资    料：指数哥伦布编码te(v)
*/
static inline void bs_write_te( bs_t *s, int x, int val )
{
    if( x == 1 )
    {
        bs_write1( s, 1&~val );
    }
    else if( x > 1 )
    {
        bs_write_ue( s, val );
    }
}

/*
函数名称：
函数功能：在s流后紧跟着写一个0x00
参    数：
返 回 值：
思    路：
资    料：H.264官方中文版.pdf 60/341 尾比特RBSP语法 trailing:拖尾的
*/
static inline void bs_rbsp_trailing( bs_t *s )
{
    bs_write1( s, 1 );		//bs_write1( bs_t *s, uint32_t i_bit )
	//while( !byte_aligned() )
    if( s->i_left != 8 )	//当前字节剩余未写的位数不是8，即当前字节已被写过，但仍有空位可写
    {
		//rbsp_alignment_zero_bit /* equal to 0 */ 全部 f(1)
		//把当前字节剩余位写入00000
        bs_write( s, s->i_left, 0x00 );	/* 0x00,一字节的0，即 00000000 */
    }
}


/*
函数名称：
函数功能：计算val经过ue(v)编码后所对应的位。
参    数：
返 回 值：
思    路：其实就是计算bs_write_ue( bs_t *s, unsigned int val )函数中的2*i_size-1的值。
资    料：毕厚杰：145页，ue(v):无符号指数熵编码
*/
static inline int bs_size_ue( unsigned int val )//
{
    static const int i_size0_254[255] =
    {
        1, 3, 3, 5, 5, 5, 5, 7, 7, 7, 7, 7, 7, 7, 7,
        9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
        11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
        11,11,11,11,11,11,11,11,11,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
        13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
        13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
        13,13,13,13,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
        15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15
    };
		/*
		数组元素的分布情况

		元素		数量(个)
		1		1	个
		3		2	个
		5		4	个
		7		8	个
		9		16	个
		11		32	个
		13		64	个
		15		151	个

		*/

    if( val < 255 )
    {
        return i_size0_254[val];	//返回数组里的对应元素
    }
    else
    {
        int i_size = 0;

        val++;

        if( val >= 0x10000 )	//0x 1 00 00 (即十进制65536)
        {
            i_size += 32;		//
            val = (val >> 16) - 1;//除以2^16 (即连除16次2)
        }
        if( val >= 0x100 )		//2^8 = 256 
        {
            i_size += 16;		//
            val = (val >> 8) - 1;	//除以2^8 (即连除8次2)
        }
        return i_size0_254[val] + i_size;
    }
}


/*
函数名称：
函数功能：通过ue(v)编码计算位数的方式实现se(v)编码的位数计算。
参    数：
返 回 值：
思    路：原理同ue(v)。
资    料：
*/
static inline int bs_size_se( int val )
{
    return bs_size_ue( val <= 0 ? -val * 2 : val * 2 - 1);
}


/*
函数名称：
函数功能：通过ue(v)编码计算位数的方式实现te(v)编码的位数计算。
参    数：
返 回 值：
思    路：当x＝1时，直接为1；当x>1时，同ue(v)
资    料：
*/
static inline int bs_size_te( int x, int val )
{
    if( x == 1 )
    {
        return 1;
    }
    else if( x > 1 )
    {
        return bs_size_ue( val );
    }
    return 0;
}



#endif
