/*****************************************************************************
 * mdate.c: h264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: mdate.c,v 1.1 2004/06/03 19:27:07 fenrir Exp $
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

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif

#if !(defined(_MSC_VER) || defined(__MINGW32__))
#include <sys/time.h>
#else
#include <sys/types.h>
#include <sys/timeb.h>
#endif
#include <time.h>

int64_t x264_mdate( void )
{
#if !(defined(_MSC_VER) || defined(__MINGW32__))
    struct timeval tv_date;

    gettimeofday( &tv_date, NULL );//取得目前的时间//原型:int gettimeofday ( struct timeval * tv , struct timezone * tz )//gettimeofday()会把目前的时间有tv所指的结构返回，当地时区的信息则放到tz所指的结构中。
    return( (int64_t) tv_date.tv_sec * 1000000 + (int64_t) tv_date.tv_usec );
#else
    struct _timeb tb;
    _ftime(&tb);
    return ((int64_t)tb.time * (1000) + (int64_t)tb.millitm) * (1000);
#endif
}

//////////////////////////////////////////////////

//函数原型
//    int gettimeofday ( struct timeval * tv , struct timezone * tz )
//函数说明
//    gettimeofday()会把目前的时间有tv所指的结构返回，当地时区的信息则放到tz所指的结构中。

//timeval结构定义为:
//struct timeval{
//          long tv_sec; /*秒*/
//          long tv_usec; /*微秒*/
//};
//timezone 结构定义为:
//struct timezone{
//        int tz_minuteswest; /*和Greenwich 时间差了多少分钟*/
//        int tz_dsttime; /*日光节约时间的状态*/
//};

//上述两个结构都定义在/usr/include/sys/time.h。tz_dsttime 所代表的状态如下

//DST_NONE /*不使用*/
//DST_USA /*美国*/
//DST_AUST /*澳洲*/
//DST_WET /*西欧*/
//DST_MET /*中欧*/
//DST_EET /*东欧*/
//DST_CAN /*加拿大*/
//DST_GB /*大不列颠*/
//DST_RUM /*罗马尼亚*/
//DST_TUR /*土耳其*/
//DST_AUSTALT /*澳洲（1986年以后）*/

//返回值
//    成功则返回0，失败返回－1，错误代码存于errno。附加说明EFAULT指针tv和tz所指的内存空间超出存取权限。

