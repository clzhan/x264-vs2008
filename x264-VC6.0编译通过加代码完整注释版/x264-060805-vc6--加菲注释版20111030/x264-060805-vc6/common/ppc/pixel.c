/*****************************************************************************
 * pixel.c: h264 encoder
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: pixel.c,v 1.1 2004/06/03 19:27:07 fenrir Exp $
 *
 * Authors: Eric Petit <titer@m0k.org>
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

#ifdef SYS_LINUX
#include <altivec.h>
#endif

#include "common/common.h"
#include "ppccommon.h"

/***********************************************************************
 * SAD routines
 **********************************************************************/

#define PIXEL_SAD_ALTIVEC( name, lx, ly, a, b )        \
static int name( uint8_t *pix1, int i_pix1,            \
                 uint8_t *pix2, int i_pix2 )           \
{                                                      \
    int y;                                             \
    DECLARE_ALIGNED( int, sum, 16 );                   \
                                                       \
    LOAD_ZERO;                                         \
    PREP_LOAD;                                         \
    vec_u8_t  pix1v, pix2v;                            \
    vec_s32_t sumv = zero_s32v;                        \
    for( y = 0; y < ly; y++ )                          \
    {                                                  \
        VEC_LOAD( pix1, pix1v, lx, vec_u8_t );         \
        VEC_LOAD( pix2, pix2v, lx, vec_u8_t );         \
        sumv = (vec_s32_t) vec_sum4s(                  \
                   vec_sub( vec_max( pix1v, pix2v ),   \
                            vec_min( pix1v, pix2v ) ), \
                   (vec_u32_t) sumv );                 \
        pix1 += i_pix1;                                \
        pix2 += i_pix2;                                \
    }                                                  \
    sumv = vec_sum##a( sumv, zero_s32v );              \
    sumv = vec_splat( sumv, b );                       \
    vec_ste( sumv, 0, &sum );                          \
    return sum;                                        \
}

PIXEL_SAD_ALTIVEC( pixel_sad_16x16_altivec, 16, 16, s,  3 )
PIXEL_SAD_ALTIVEC( pixel_sad_8x16_altivec,  8,  16, 2s, 1 )
PIXEL_SAD_ALTIVEC( pixel_sad_16x8_altivec,  16, 8,  s,  3 )
PIXEL_SAD_ALTIVEC( pixel_sad_8x8_altivec,   8,  8,  2s, 1 )



/***********************************************************************
 * SATD routines
 **********************************************************************/

/***********************************************************************
 * VEC_HADAMAR
 ***********************************************************************
 * b[0] = a[0] + a[1] + a[2] + a[3]
 * b[1] = a[0] + a[1] - a[2] - a[3]
 * b[2] = a[0] - a[1] - a[2] + a[3]
 * b[3] = a[0] - a[1] + a[2] - a[3]
 **********************************************************************/
#define VEC_HADAMAR(a0,a1,a2,a3,b0,b1,b2,b3) \
    b2 = vec_add( a0, a1 ); \
    b3 = vec_add( a2, a3 ); \
    a0 = vec_sub( a0, a1 ); \
    a2 = vec_sub( a2, a3 ); \
    b0 = vec_add( b2, b3 ); \
    b1 = vec_sub( b2, b3 ); \
    b2 = vec_sub( a0, a2 ); \
    b3 = vec_add( a0, a2 )

/***********************************************************************
 * VEC_ABS
 ***********************************************************************
 * a: s16v
 *
 * a = abs(a)
 * 
 * Call vec_sub()/vec_max() instead of vec_abs() because vec_abs()
 * actually also calls vec_splat(0), but we already have a null vector.
 **********************************************************************/
#define VEC_ABS(a) \
    pix1v = vec_sub( zero_s16v, a ); \
    a     = vec_max( a, pix1v ); \

/***********************************************************************
 * VEC_ADD_ABS
 ***********************************************************************
 * a:    s16v
 * b, c: s32v
 *
 * c[i] = abs(a[2*i]) + abs(a[2*i+1]) + [bi]
 **********************************************************************/
#define VEC_ADD_ABS(a,b,c) \
    VEC_ABS( a ); \
    c = vec_sum4s( a, b )

/***********************************************************************
 * SATD 4x4
 **********************************************************************/
static int pixel_satd_4x4_altivec( uint8_t *pix1, int i_pix1,
                                   uint8_t *pix2, int i_pix2 )
{
    DECLARE_ALIGNED( int, i_satd, 16 );

    PREP_DIFF;
    vec_s16_t diff0v, diff1v, diff2v, diff3v;
    vec_s16_t temp0v, temp1v, temp2v, temp3v;
    vec_s32_t satdv;

    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff0v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff1v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff2v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff3v );

    /* Hadamar H */
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    
    VEC_TRANSPOSE_4( temp0v, temp1v, temp2v, temp3v,
                     diff0v, diff1v, diff2v, diff3v );
    /* Hadamar V */
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );

    VEC_ADD_ABS( temp0v, zero_s32v, satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );

    satdv = vec_sum2s( satdv, zero_s32v );
    satdv = vec_splat( satdv, 1 );
    vec_ste( satdv, 0, &i_satd );

    return i_satd / 2;
}

/***********************************************************************
 * SATD 4x8
 **********************************************************************/
static int pixel_satd_4x8_altivec( uint8_t *pix1, int i_pix1,
                                   uint8_t *pix2, int i_pix2 )
{
    DECLARE_ALIGNED( int, i_satd, 16 );

    PREP_DIFF;
    vec_s16_t diff0v, diff1v, diff2v, diff3v;
    vec_s16_t temp0v, temp1v, temp2v, temp3v;
    vec_s32_t satdv;

    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff0v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff1v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff2v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff3v );
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_TRANSPOSE_4( temp0v, temp1v, temp2v, temp3v,
                     diff0v, diff1v, diff2v, diff3v );
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_ADD_ABS( temp0v, zero_s32v, satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );

    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff0v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff1v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff2v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 4, diff3v );
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_TRANSPOSE_4( temp0v, temp1v, temp2v, temp3v,
                     diff0v, diff1v, diff2v, diff3v );
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_ADD_ABS( temp0v, satdv,     satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );

    satdv = vec_sum2s( satdv, zero_s32v );
    satdv = vec_splat( satdv, 1 );
    vec_ste( satdv, 0, &i_satd );

    return i_satd / 2;
}

/***********************************************************************
 * SATD 8x4
 **********************************************************************/
static int pixel_satd_8x4_altivec( uint8_t *pix1, int i_pix1,
                                   uint8_t *pix2, int i_pix2 )
{
    DECLARE_ALIGNED( int, i_satd, 16 );

    PREP_DIFF;
    vec_s16_t diff0v, diff1v, diff2v, diff3v,
              diff4v, diff5v, diff6v, diff7v;
    vec_s16_t temp0v, temp1v, temp2v, temp3v,
              temp4v, temp5v, temp6v, temp7v;
    vec_s32_t satdv;

    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff0v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff1v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff2v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff3v );

    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    /* This causes warnings because temp4v...temp7v haven't be set,
       but we don't care */
    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diff0v, diff1v, diff2v, diff3v,
                     diff4v, diff5v, diff6v, diff7v );
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diff4v, diff5v, diff6v, diff7v,
                 temp4v, temp5v, temp6v, temp7v );

    VEC_ADD_ABS( temp0v, zero_s32v, satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );

    satdv = vec_sum2s( satdv, zero_s32v );
    satdv = vec_splat( satdv, 1 );
    vec_ste( satdv, 0, &i_satd );

    return i_satd / 2;
}

/***********************************************************************
 * SATD 8x8
 **********************************************************************/
static int pixel_satd_8x8_altivec( uint8_t *pix1, int i_pix1,
                                   uint8_t *pix2, int i_pix2 )
{
    DECLARE_ALIGNED( int, i_satd, 16 );

    PREP_DIFF;
    vec_s16_t diff0v, diff1v, diff2v, diff3v,
              diff4v, diff5v, diff6v, diff7v;
    vec_s16_t temp0v, temp1v, temp2v, temp3v,
              temp4v, temp5v, temp6v, temp7v;
    vec_s32_t satdv;

    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff0v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff1v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff2v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff3v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff4v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff5v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff6v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff7v );

    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diff4v, diff5v, diff6v, diff7v,
                 temp4v, temp5v, temp6v, temp7v );

    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diff0v, diff1v, diff2v, diff3v,
                     diff4v, diff5v, diff6v, diff7v );

    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diff4v, diff5v, diff6v, diff7v,
                 temp4v, temp5v, temp6v, temp7v );

    VEC_ADD_ABS( temp0v, zero_s32v, satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );

    satdv = vec_sums( satdv, zero_s32v );
    satdv = vec_splat( satdv, 3 );
    vec_ste( satdv, 0, &i_satd );

    return i_satd / 2;
}

/***********************************************************************
 * SATD 8x16
 **********************************************************************/
static int pixel_satd_8x16_altivec( uint8_t *pix1, int i_pix1,
                                    uint8_t *pix2, int i_pix2 )
{
    DECLARE_ALIGNED( int, i_satd, 16 );

    PREP_DIFF;
    vec_s16_t diff0v, diff1v, diff2v, diff3v,
              diff4v, diff5v, diff6v, diff7v;
    vec_s16_t temp0v, temp1v, temp2v, temp3v,
              temp4v, temp5v, temp6v, temp7v;
    vec_s32_t satdv;

    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff0v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff1v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff2v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff3v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff4v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff5v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff6v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff7v );
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diff4v, diff5v, diff6v, diff7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diff0v, diff1v, diff2v, diff3v,
                     diff4v, diff5v, diff6v, diff7v );
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diff4v, diff5v, diff6v, diff7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_ADD_ABS( temp0v, zero_s32v, satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );

    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff0v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff1v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff2v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff3v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff4v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff5v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff6v );
    VEC_DIFF_H( pix1, i_pix1, pix2, i_pix2, 8, diff7v );
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diff4v, diff5v, diff6v, diff7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diff0v, diff1v, diff2v, diff3v,
                     diff4v, diff5v, diff6v, diff7v );
    VEC_HADAMAR( diff0v, diff1v, diff2v, diff3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diff4v, diff5v, diff6v, diff7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_ADD_ABS( temp0v, satdv,     satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );

    satdv = vec_sums( satdv, zero_s32v );
    satdv = vec_splat( satdv, 3 );
    vec_ste( satdv, 0, &i_satd );

    return i_satd / 2;
}

/***********************************************************************
 * SATD 16x8
 **********************************************************************/
static int pixel_satd_16x8_altivec( uint8_t *pix1, int i_pix1,
                                    uint8_t *pix2, int i_pix2 )
{
    DECLARE_ALIGNED( int, i_satd, 16 );

    LOAD_ZERO;
    PREP_LOAD;
    vec_s32_t satdv;
    vec_s16_t pix1v, pix2v;
    vec_s16_t diffh0v, diffh1v, diffh2v, diffh3v,
              diffh4v, diffh5v, diffh6v, diffh7v;
    vec_s16_t diffl0v, diffl1v, diffl2v, diffl3v,
              diffl4v, diffl5v, diffl6v, diffl7v;
    vec_s16_t temp0v, temp1v, temp2v, temp3v,
              temp4v, temp5v, temp6v, temp7v;

    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh0v, diffl0v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh1v, diffl1v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh2v, diffl2v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh3v, diffl3v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh4v, diffl4v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh5v, diffl5v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh6v, diffl6v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh7v, diffl7v );

    VEC_HADAMAR( diffh0v, diffh1v, diffh2v, diffh3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffh4v, diffh5v, diffh6v, diffh7v,
                 temp4v, temp5v, temp6v, temp7v );

    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diffh0v, diffh1v, diffh2v, diffh3v,
                     diffh4v, diffh5v, diffh6v, diffh7v );

    VEC_HADAMAR( diffh0v, diffh1v, diffh2v, diffh3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffh4v, diffh5v, diffh6v, diffh7v,
                 temp4v, temp5v, temp6v, temp7v );

    VEC_ADD_ABS( temp0v, zero_s32v, satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );

    VEC_HADAMAR( diffl0v, diffl1v, diffl2v, diffl3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffl4v, diffl5v, diffl6v, diffl7v,
                 temp4v, temp5v, temp6v, temp7v );

    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diffl0v, diffl1v, diffl2v, diffl3v,
                     diffl4v, diffl5v, diffl6v, diffl7v );

    VEC_HADAMAR( diffl0v, diffl1v, diffl2v, diffl3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffl4v, diffl5v, diffl6v, diffl7v,
                 temp4v, temp5v, temp6v, temp7v );

    VEC_ADD_ABS( temp0v, satdv,     satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );

    satdv = vec_sums( satdv, zero_s32v );
    satdv = vec_splat( satdv, 3 );
    vec_ste( satdv, 0, &i_satd );

    return i_satd / 2;
}

/***********************************************************************
 * SATD 16x16
 **********************************************************************/
static int pixel_satd_16x16_altivec( uint8_t *pix1, int i_pix1,
                                     uint8_t *pix2, int i_pix2 )
{
    DECLARE_ALIGNED( int, i_satd, 16 );

    LOAD_ZERO;
    PREP_LOAD;
    vec_s32_t satdv;
    vec_s16_t pix1v, pix2v;
    vec_s16_t diffh0v, diffh1v, diffh2v, diffh3v,
              diffh4v, diffh5v, diffh6v, diffh7v;
    vec_s16_t diffl0v, diffl1v, diffl2v, diffl3v,
              diffl4v, diffl5v, diffl6v, diffl7v;
    vec_s16_t temp0v, temp1v, temp2v, temp3v,
              temp4v, temp5v, temp6v, temp7v;

    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh0v, diffl0v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh1v, diffl1v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh2v, diffl2v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh3v, diffl3v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh4v, diffl4v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh5v, diffl5v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh6v, diffl6v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh7v, diffl7v );
    VEC_HADAMAR( diffh0v, diffh1v, diffh2v, diffh3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffh4v, diffh5v, diffh6v, diffh7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diffh0v, diffh1v, diffh2v, diffh3v,
                     diffh4v, diffh5v, diffh6v, diffh7v );
    VEC_HADAMAR( diffh0v, diffh1v, diffh2v, diffh3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffh4v, diffh5v, diffh6v, diffh7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_ADD_ABS( temp0v, zero_s32v, satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );
    VEC_HADAMAR( diffl0v, diffl1v, diffl2v, diffl3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffl4v, diffl5v, diffl6v, diffl7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diffl0v, diffl1v, diffl2v, diffl3v,
                     diffl4v, diffl5v, diffl6v, diffl7v );
    VEC_HADAMAR( diffl0v, diffl1v, diffl2v, diffl3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffl4v, diffl5v, diffl6v, diffl7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_ADD_ABS( temp0v, satdv,     satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );

    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh0v, diffl0v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh1v, diffl1v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh2v, diffl2v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh3v, diffl3v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh4v, diffl4v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh5v, diffl5v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh6v, diffl6v );
    VEC_DIFF_HL( pix1, i_pix1, pix2, i_pix2, diffh7v, diffl7v );
    VEC_HADAMAR( diffh0v, diffh1v, diffh2v, diffh3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffh4v, diffh5v, diffh6v, diffh7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diffh0v, diffh1v, diffh2v, diffh3v,
                     diffh4v, diffh5v, diffh6v, diffh7v );
    VEC_HADAMAR( diffh0v, diffh1v, diffh2v, diffh3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffh4v, diffh5v, diffh6v, diffh7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_ADD_ABS( temp0v, satdv,     satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );
    VEC_HADAMAR( diffl0v, diffl1v, diffl2v, diffl3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffl4v, diffl5v, diffl6v, diffl7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_TRANSPOSE_8( temp0v, temp1v, temp2v, temp3v,
                     temp4v, temp5v, temp6v, temp7v,
                     diffl0v, diffl1v, diffl2v, diffl3v,
                     diffl4v, diffl5v, diffl6v, diffl7v );
    VEC_HADAMAR( diffl0v, diffl1v, diffl2v, diffl3v,
                 temp0v, temp1v, temp2v, temp3v );
    VEC_HADAMAR( diffl4v, diffl5v, diffl6v, diffl7v,
                 temp4v, temp5v, temp6v, temp7v );
    VEC_ADD_ABS( temp0v, satdv,     satdv );
    VEC_ADD_ABS( temp1v, satdv,     satdv );
    VEC_ADD_ABS( temp2v, satdv,     satdv );
    VEC_ADD_ABS( temp3v, satdv,     satdv );
    VEC_ADD_ABS( temp4v, satdv,     satdv );
    VEC_ADD_ABS( temp5v, satdv,     satdv );
    VEC_ADD_ABS( temp6v, satdv,     satdv );
    VEC_ADD_ABS( temp7v, satdv,     satdv );

    satdv = vec_sums( satdv, zero_s32v );
    satdv = vec_splat( satdv, 3 );
    vec_ste( satdv, 0, &i_satd );

    return i_satd / 2;
}



/***********************************************************************
* Interleaved SAD routines
**********************************************************************/

static void pixel_sad_x4_16x16_altivec( uint8_t *fenc, uint8_t *pix0, uint8_t *pix1, uint8_t *pix2, uint8_t *pix3, int i_stride, int scores[4] )
{
    DECLARE_ALIGNED( int, sum0, 16 );
    DECLARE_ALIGNED( int, sum1, 16 );
    DECLARE_ALIGNED( int, sum2, 16 );
    DECLARE_ALIGNED( int, sum3, 16 );
    int y;
    
    LOAD_ZERO;
    vec_u8_t temp_lv, temp_hv;
    vec_u8_t fencv, pix0v, pix1v, pix2v, pix3v;
    //vec_u8_t perm0v, perm1v, perm2v, perm3v;
    vec_u8_t perm0vA, perm1vA, perm2vA, perm3vA, perm0vB, perm1vB, perm2vB, perm3vB;
    
    vec_s32_t sum0v, sum1v, sum2v, sum3v;
    
    sum0v = vec_splat_s32(0);
    sum1v = vec_splat_s32(0);
    sum2v = vec_splat_s32(0);
    sum3v = vec_splat_s32(0);
    
    perm0vA = vec_lvsl(0, pix0);
    perm1vA = vec_lvsl(0, pix1);
    perm2vA = vec_lvsl(0, pix2);
    perm3vA = vec_lvsl(0, pix3);
    
    perm0vB = vec_lvsl(0, pix0 + i_stride);
    perm1vB = vec_lvsl(0, pix1 + i_stride);
    perm2vB = vec_lvsl(0, pix2 + i_stride);
    perm3vB = vec_lvsl(0, pix3 + i_stride);
    
    
    for (y = 0; y < 8; y++)
    {
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vA);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vA);
        pix1 += i_stride;
        
        fencv = vec_ld(0, fenc);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vA);
        pix2 += i_stride;
        
        temp_lv = vec_ld(0, pix3);
        temp_hv = vec_ld(16, pix3);
        pix3v = vec_perm(temp_lv, temp_hv, perm3vA);
        pix3 += i_stride;
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        sum3v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix3v ), vec_min( fencv, pix3v ) ), (vec_u32_t) sum3v ); 
        
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vB);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vB);
        pix1 += i_stride;
        
        fencv = vec_ld(0, fenc);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vB);
        pix2 += i_stride;
        
        temp_lv = vec_ld(0, pix3);
        temp_hv = vec_ld(16, pix3);
        pix3v = vec_perm(temp_lv, temp_hv, perm3vB);
        pix3 += i_stride;
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        sum3v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix3v ), vec_min( fencv, pix3v ) ), (vec_u32_t) sum3v ); 
        
        
    }
    
    sum0v = vec_sums( sum0v, zero_s32v );
    sum1v = vec_sums( sum1v, zero_s32v );
    sum2v = vec_sums( sum2v, zero_s32v );
    sum3v = vec_sums( sum3v, zero_s32v );
    
    sum0v = vec_splat( sum0v, 3 );
    sum1v = vec_splat( sum1v, 3 );
    sum2v = vec_splat( sum2v, 3 );
    sum3v = vec_splat( sum3v, 3 );
    
    vec_ste( sum0v, 0, &sum0);
    vec_ste( sum1v, 0, &sum1);
    vec_ste( sum2v, 0, &sum2);
    vec_ste( sum3v, 0, &sum3);
    
    scores[0] = sum0;
    scores[1] = sum1;
    scores[2] = sum2;
    scores[3] = sum3;
    
    
    
}

static void pixel_sad_x3_16x16_altivec( uint8_t *fenc, uint8_t *pix0, uint8_t *pix1, uint8_t *pix2, int i_stride, int scores[3] )
{
    
    DECLARE_ALIGNED( int, sum0, 16 );
    DECLARE_ALIGNED( int, sum1, 16 );
    DECLARE_ALIGNED( int, sum2, 16 );
    int y;
    
    LOAD_ZERO;
    vec_u8_t temp_lv, temp_hv; // temporary load vectors
    vec_u8_t fencv, pix0v, pix1v, pix2v;
    vec_u8_t perm0vA, perm1vA, perm2vA, perm0vB, perm1vB, perm2vB;
    
    vec_s32_t sum0v, sum1v, sum2v;
    
    sum0v = vec_splat_s32(0);
    sum1v = vec_splat_s32(0);
    sum2v = vec_splat_s32(0);
    
    perm0vA = vec_lvsl(0, pix0);
    perm1vA = vec_lvsl(0, pix1);
    perm2vA = vec_lvsl(0, pix2);
    
    perm0vB = vec_lvsl(0, pix0 + i_stride);
    perm1vB = vec_lvsl(0, pix1 + i_stride);
    perm2vB = vec_lvsl(0, pix2 + i_stride);
    
    for (y = 0; y < 8; y++)
    {
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vA);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vA);
        pix1 += i_stride;
        
        fencv = vec_ld(0, fenc);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vA);
        pix2 += i_stride;
        
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vB);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vB);
        pix1 += i_stride;
        
        fencv = vec_ld(0, fenc);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vB);
        pix2 += i_stride;
        
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        
        
    }
    
    sum0v = vec_sums( sum0v, zero_s32v );
    sum1v = vec_sums( sum1v, zero_s32v );
    sum2v = vec_sums( sum2v, zero_s32v );
    
    sum0v = vec_splat( sum0v, 3 );
    sum1v = vec_splat( sum1v, 3 );
    sum2v = vec_splat( sum2v, 3 );
    
    vec_ste( sum0v, 0, &sum0);
    vec_ste( sum1v, 0, &sum1);
    vec_ste( sum2v, 0, &sum2);
    
    scores[0] = sum0;
    scores[1] = sum1;
    scores[2] = sum2;
    
} 

static void pixel_sad_x4_16x8_altivec( uint8_t *fenc, uint8_t *pix0, uint8_t *pix1, uint8_t *pix2, uint8_t *pix3, int i_stride, int scores[4] )
{
    DECLARE_ALIGNED( int, sum0, 16 );
    DECLARE_ALIGNED( int, sum1, 16 );
    DECLARE_ALIGNED( int, sum2, 16 );
    DECLARE_ALIGNED( int, sum3, 16 );
    int y;
    
    LOAD_ZERO;
    vec_u8_t temp_lv, temp_hv; 
    vec_u8_t fencv, pix0v, pix1v, pix2v, pix3v;
    vec_u8_t perm0vA, perm1vA, perm2vA, perm3vA, perm0vB, perm1vB, perm2vB, perm3vB;
    
    vec_s32_t sum0v, sum1v, sum2v, sum3v;
    
    sum0v = vec_splat_s32(0);
    sum1v = vec_splat_s32(0);
    sum2v = vec_splat_s32(0);
    sum3v = vec_splat_s32(0);
    
    perm0vA = vec_lvsl(0, pix0);
    perm1vA = vec_lvsl(0, pix1);
    perm2vA = vec_lvsl(0, pix2);
    perm3vA = vec_lvsl(0, pix3);
    
    perm0vB = vec_lvsl(0, pix0 + i_stride);
    perm1vB = vec_lvsl(0, pix1 + i_stride);
    perm2vB = vec_lvsl(0, pix2 + i_stride);
    perm3vB = vec_lvsl(0, pix3 + i_stride);
    
    
    
    for (y = 0; y < 4; y++)
    {
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vA);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vA);
        pix1 += i_stride;
        
        fencv = vec_ld(0, fenc);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vA);
        pix2 += i_stride;
        
        temp_lv = vec_ld(0, pix3);
        temp_hv = vec_ld(16, pix3);
        pix3v = vec_perm(temp_lv, temp_hv, perm3vA);
        pix3 += i_stride;
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        sum3v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix3v ), vec_min( fencv, pix3v ) ), (vec_u32_t) sum3v ); 
        
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vB);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vB);
        pix1 += i_stride;
        
        fencv = vec_ld(0, fenc);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vB);
        pix2 += i_stride;
        
        temp_lv = vec_ld(0, pix3);
        temp_hv = vec_ld(16, pix3);
        pix3v = vec_perm(temp_lv, temp_hv, perm3vB);
        pix3 += i_stride;
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        sum3v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix3v ), vec_min( fencv, pix3v ) ), (vec_u32_t) sum3v );
        
        
    }
    
    sum0v = vec_sums( sum0v, zero_s32v );
    sum1v = vec_sums( sum1v, zero_s32v );
    sum2v = vec_sums( sum2v, zero_s32v );
    sum3v = vec_sums( sum3v, zero_s32v );
    
    sum0v = vec_splat( sum0v, 3 );
    sum1v = vec_splat( sum1v, 3 );
    sum2v = vec_splat( sum2v, 3 );
    sum3v = vec_splat( sum3v, 3 );
    
    vec_ste( sum0v, 0, &sum0);
    vec_ste( sum1v, 0, &sum1);
    vec_ste( sum2v, 0, &sum2);
    vec_ste( sum3v, 0, &sum3);
    
    scores[0] = sum0;
    scores[1] = sum1;
    scores[2] = sum2;
    scores[3] = sum3;
    
    
    
}

static void pixel_sad_x3_16x8_altivec( uint8_t *fenc, uint8_t *pix0, uint8_t *pix1, uint8_t *pix2, int i_stride, int scores[3] )
{
    
    DECLARE_ALIGNED( int, sum0, 16 );
    DECLARE_ALIGNED( int, sum1, 16 );
    DECLARE_ALIGNED( int, sum2, 16 );
    int y;
    
    LOAD_ZERO;
    vec_u8_t temp_lv, temp_hv; 
    vec_u8_t fencv, pix0v, pix1v, pix2v;
    vec_u8_t perm0vA, perm1vA, perm2vA, perm0vB, perm1vB, perm2vB;
    
    vec_s32_t sum0v, sum1v, sum2v;
    
    sum0v = vec_splat_s32(0);
    sum1v = vec_splat_s32(0);
    sum2v = vec_splat_s32(0);

    
    perm0vA = vec_lvsl(0, pix0);
    perm1vA = vec_lvsl(0, pix1);
    perm2vA = vec_lvsl(0, pix2);
    
    perm0vB = vec_lvsl(0, pix0 + i_stride);
    perm1vB = vec_lvsl(0, pix1 + i_stride);
    perm2vB = vec_lvsl(0, pix2 + i_stride);
    
    for (y = 0; y < 4; y++)
    {
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vA);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vA);
        pix1 += i_stride;
        
        fencv = vec_ld(0, fenc);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vA);
        pix2 += i_stride;
        
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vB);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vB);
        pix1 += i_stride;
        
        fencv = vec_ld(0, fenc);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vB);
        pix2 += i_stride;
        
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v );
        
        
    }
    
    sum0v = vec_sums( sum0v, zero_s32v );
    sum1v = vec_sums( sum1v, zero_s32v );
    sum2v = vec_sums( sum2v, zero_s32v );
    
    sum0v = vec_splat( sum0v, 3 );
    sum1v = vec_splat( sum1v, 3 );
    sum2v = vec_splat( sum2v, 3 );
    
    vec_ste( sum0v, 0, &sum0);
    vec_ste( sum1v, 0, &sum1);
    vec_ste( sum2v, 0, &sum2);
    
    scores[0] = sum0;
    scores[1] = sum1;
    scores[2] = sum2;
    
} 


static void pixel_sad_x4_8x16_altivec( uint8_t *fenc, uint8_t *pix0, uint8_t *pix1, uint8_t *pix2, uint8_t *pix3, int i_stride, int scores[4] )
{
    DECLARE_ALIGNED( int, sum0, 16 );
    DECLARE_ALIGNED( int, sum1, 16 );
    DECLARE_ALIGNED( int, sum2, 16 );
    DECLARE_ALIGNED( int, sum3, 16 );
    int y;
    
    LOAD_ZERO;
    vec_u8_t temp_lv, temp_hv; 
    vec_u8_t fencv, pix0v, pix1v, pix2v, pix3v;
    vec_u8_t perm0vA, perm1vA, perm2vA, perm3vA, perm0vB, perm1vB, perm2vB, perm3vB, permEncv;
    
    vec_s32_t sum0v, sum1v, sum2v, sum3v;
    
    sum0v = vec_splat_s32(0);
    sum1v = vec_splat_s32(0);
    sum2v = vec_splat_s32(0);
    sum3v = vec_splat_s32(0);
    
    permEncv = vec_lvsl(0, fenc);
    perm0vA = vec_lvsl(0, pix0);
    perm1vA = vec_lvsl(0, pix1);
    perm2vA = vec_lvsl(0, pix2);
    perm3vA = vec_lvsl(0, pix3);
    
    perm0vB = vec_lvsl(0, pix0 + i_stride);
    perm1vB = vec_lvsl(0, pix1 + i_stride);
    perm2vB = vec_lvsl(0, pix2 + i_stride);
    perm3vB = vec_lvsl(0, pix3 + i_stride);
    
    
    for (y = 0; y < 8; y++)
    {
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vA);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vA);
        pix1 += i_stride;
        
        temp_lv = vec_ld(0, fenc);
        fencv = vec_perm(temp_lv, temp_hv, permEncv);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vA);
        pix2 += i_stride;
        
        temp_lv = vec_ld(0, pix3);
        temp_hv = vec_ld(16, pix3);
        pix3v = vec_perm(temp_lv, temp_hv, perm3vA);
        pix3 += i_stride;
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        sum3v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix3v ), vec_min( fencv, pix3v ) ), (vec_u32_t) sum3v ); 
        
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vB);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vB);
        pix1 += i_stride;
        
        temp_lv = vec_ld(0, fenc);
        fencv = vec_perm(temp_lv, temp_hv, permEncv);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vB);
        pix2 += i_stride;
        
        temp_lv = vec_ld(0, pix3);
        temp_hv = vec_ld(16, pix3);
        pix3v = vec_perm(temp_lv, temp_hv, perm3vB);
        pix3 += i_stride;
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        sum3v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix3v ), vec_min( fencv, pix3v ) ), (vec_u32_t) sum3v ); 
    }
    
    sum0v = vec_sum2s( sum0v, zero_s32v );
    sum1v = vec_sum2s( sum1v, zero_s32v );
    sum2v = vec_sum2s( sum2v, zero_s32v );
    sum3v = vec_sum2s( sum3v, zero_s32v );
    
    sum0v = vec_splat( sum0v, 1 );
    sum1v = vec_splat( sum1v, 1 );
    sum2v = vec_splat( sum2v, 1 );
    sum3v = vec_splat( sum3v, 1 );
    
    vec_ste( sum0v, 0, &sum0);
    vec_ste( sum1v, 0, &sum1);
    vec_ste( sum2v, 0, &sum2);
    vec_ste( sum3v, 0, &sum3);
    
    scores[0] = sum0;
    scores[1] = sum1;
    scores[2] = sum2;
    scores[3] = sum3; 
        
}

static void pixel_sad_x3_8x16_altivec( uint8_t *fenc, uint8_t *pix0, uint8_t *pix1, uint8_t *pix2, int i_stride, int scores[3] )
{
    DECLARE_ALIGNED( int, sum0, 16 );
    DECLARE_ALIGNED( int, sum1, 16 );
    DECLARE_ALIGNED( int, sum2, 16 );
    int y;
    
    LOAD_ZERO;
    vec_u8_t temp_lv, temp_hv; 
    vec_u8_t fencv, pix0v, pix1v, pix2v;
    vec_u8_t perm0vA, perm1vA, perm2vA, perm0vB, perm1vB, perm2vB,permEncv;
    
    vec_s32_t sum0v, sum1v, sum2v;
    
    sum0v = vec_splat_s32(0);
    sum1v = vec_splat_s32(0);
    sum2v = vec_splat_s32(0);
    
    permEncv = vec_lvsl(0, fenc);
    perm0vA = vec_lvsl(0, pix0);
    perm1vA = vec_lvsl(0, pix1);
    perm2vA = vec_lvsl(0, pix2);
    
    perm0vB = vec_lvsl(0, pix0 + i_stride);
    perm1vB = vec_lvsl(0, pix1 + i_stride);
    perm2vB = vec_lvsl(0, pix2 + i_stride);
    
    for (y = 0; y < 8; y++)
    {
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vA);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vA);
        pix1 += i_stride;
        
        temp_lv = vec_ld(0, fenc);
        fencv = vec_perm(temp_lv, temp_hv, permEncv);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vA);
        pix2 += i_stride;
        
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vB);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vB);
        pix1 += i_stride;
        
        temp_lv = vec_ld(0, fenc);
        fencv = vec_perm(temp_lv, temp_hv, permEncv);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vB);
        pix2 += i_stride;
        
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
    }
    
    
    sum0v = vec_sum2s( sum0v, zero_s32v );
    sum1v = vec_sum2s( sum1v, zero_s32v );
    sum2v = vec_sum2s( sum2v, zero_s32v );
    
    sum0v = vec_splat( sum0v, 1 );
    sum1v = vec_splat( sum1v, 1 );
    sum2v = vec_splat( sum2v, 1 );
    
    vec_ste( sum0v, 0, &sum0);
    vec_ste( sum1v, 0, &sum1);
    vec_ste( sum2v, 0, &sum2);
    
    scores[0] = sum0;
    scores[1] = sum1;
    scores[2] = sum2;
    
}

static void pixel_sad_x4_8x8_altivec( uint8_t *fenc, uint8_t *pix0, uint8_t *pix1, uint8_t *pix2, uint8_t *pix3, int i_stride, int scores[4] )
{
    DECLARE_ALIGNED( int, sum0, 16 );
    DECLARE_ALIGNED( int, sum1, 16 );
    DECLARE_ALIGNED( int, sum2, 16 );
    DECLARE_ALIGNED( int, sum3, 16 );
    int y;
    
    LOAD_ZERO;
    vec_u8_t temp_lv, temp_hv; 
    vec_u8_t fencv, pix0v, pix1v, pix2v, pix3v;
    vec_u8_t perm0vA, perm1vA, perm2vA, perm3vA, perm0vB, perm1vB, perm2vB, perm3vB, permEncv;
    
    vec_s32_t sum0v, sum1v, sum2v, sum3v;
    
    sum0v = vec_splat_s32(0);
    sum1v = vec_splat_s32(0);
    sum2v = vec_splat_s32(0);
    sum3v = vec_splat_s32(0);
    
    permEncv = vec_lvsl(0, fenc);
    perm0vA = vec_lvsl(0, pix0);
    perm1vA = vec_lvsl(0, pix1);
    perm2vA = vec_lvsl(0, pix2);
    perm3vA = vec_lvsl(0, pix3);
    
    perm0vB = vec_lvsl(0, pix0 + i_stride);
    perm1vB = vec_lvsl(0, pix1 + i_stride);
    perm2vB = vec_lvsl(0, pix2 + i_stride);
    perm3vB = vec_lvsl(0, pix3 + i_stride);
    
    
    for (y = 0; y < 4; y++)
    {
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vA);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vA);
        pix1 += i_stride;
        
        temp_lv = vec_ld(0, fenc);
        fencv = vec_perm(temp_lv, temp_hv, permEncv);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vA);
        pix2 += i_stride;
        
        temp_lv = vec_ld(0, pix3);
        temp_hv = vec_ld(16, pix3);
        pix3v = vec_perm(temp_lv, temp_hv, perm3vA);
        pix3 += i_stride;
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        sum3v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix3v ), vec_min( fencv, pix3v ) ), (vec_u32_t) sum3v ); 
        
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vB);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vB);
        pix1 += i_stride;
        
        temp_lv = vec_ld(0, fenc);
        fencv = vec_perm(temp_lv, temp_hv, permEncv);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vB);
        pix2 += i_stride;
        
        temp_lv = vec_ld(0, pix3);
        temp_hv = vec_ld(16, pix3);
        pix3v = vec_perm(temp_lv, temp_hv, perm3vB);
        pix3 += i_stride;
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        sum3v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix3v ), vec_min( fencv, pix3v ) ), (vec_u32_t) sum3v ); 
    }
    
    
    sum0v = vec_sum2s( sum0v, zero_s32v );
    sum1v = vec_sum2s( sum1v, zero_s32v );
    sum2v = vec_sum2s( sum2v, zero_s32v );
    sum3v = vec_sum2s( sum3v, zero_s32v );
    
    sum0v = vec_splat( sum0v, 1 );
    sum1v = vec_splat( sum1v, 1 );
    sum2v = vec_splat( sum2v, 1 );
    sum3v = vec_splat( sum3v, 1 );
    
    vec_ste( sum0v, 0, &sum0);
    vec_ste( sum1v, 0, &sum1);
    vec_ste( sum2v, 0, &sum2);
    vec_ste( sum3v, 0, &sum3);
    
    scores[0] = sum0;
    scores[1] = sum1;
    scores[2] = sum2;
    scores[3] = sum3; 
    
    
}

static void pixel_sad_x3_8x8_altivec( uint8_t *fenc, uint8_t *pix0, uint8_t *pix1, uint8_t *pix2, int i_stride, int scores[3] )
{
    DECLARE_ALIGNED( int, sum0, 16 );
    DECLARE_ALIGNED( int, sum1, 16 );
    DECLARE_ALIGNED( int, sum2, 16 );
    int y;
    
    LOAD_ZERO;
    vec_u8_t temp_lv, temp_hv; 
    vec_u8_t fencv, pix0v, pix1v, pix2v;
    vec_u8_t perm0vA, perm1vA, perm2vA, perm0vB, perm1vB, perm2vB,  permEncv;
    
    vec_s32_t sum0v, sum1v, sum2v;
    
    sum0v = vec_splat_s32(0);
    sum1v = vec_splat_s32(0);
    sum2v = vec_splat_s32(0);
    
    permEncv = vec_lvsl(0, fenc);
    perm0vA = vec_lvsl(0, pix0);
    perm1vA = vec_lvsl(0, pix1);
    perm2vA = vec_lvsl(0, pix2);
    
    perm0vB = vec_lvsl(0, pix0 + i_stride);
    perm1vB = vec_lvsl(0, pix1 + i_stride);
    perm2vB = vec_lvsl(0, pix2 + i_stride);
    
    for (y = 0; y < 4; y++)
    {
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vA);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vA);
        pix1 += i_stride;
        
        temp_lv = vec_ld(0, fenc);
        fencv = vec_perm(temp_lv, temp_hv, permEncv);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vA);
        pix2 += i_stride;
        
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
        
        temp_lv = vec_ld(0, pix0);
        temp_hv = vec_ld(16, pix0);
        pix0v = vec_perm(temp_lv, temp_hv, perm0vB);
        pix0 += i_stride;
        
        
        temp_lv = vec_ld(0, pix1);
        temp_hv = vec_ld(16, pix1);
        pix1v = vec_perm(temp_lv, temp_hv, perm1vB);
        pix1 += i_stride;
        
        temp_lv = vec_ld(0, fenc);
        fencv = vec_perm(temp_lv, temp_hv, permEncv);
        fenc += FENC_STRIDE;
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2v = vec_perm(temp_lv, temp_hv, perm2vB);
        pix2 += i_stride;
        
        
        sum0v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix0v ), vec_min( fencv, pix0v ) ), (vec_u32_t) sum0v ); 
        
        sum1v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix1v ), vec_min( fencv, pix1v ) ), (vec_u32_t) sum1v ); 
        
        sum2v = (vec_s32_t) vec_sum4s( vec_sub( vec_max( fencv, pix2v ), vec_min( fencv, pix2v ) ), (vec_u32_t) sum2v ); 
        
    }
    
    
    sum0v = vec_sum2s( sum0v, zero_s32v );
    sum1v = vec_sum2s( sum1v, zero_s32v );
    sum2v = vec_sum2s( sum2v, zero_s32v );
    
    sum0v = vec_splat( sum0v, 1 );
    sum1v = vec_splat( sum1v, 1 );
    sum2v = vec_splat( sum2v, 1 );
    
    vec_ste( sum0v, 0, &sum0);
    vec_ste( sum1v, 0, &sum1);
    vec_ste( sum2v, 0, &sum2);
    
    scores[0] = sum0;
    scores[1] = sum1;
    scores[2] = sum2;
}

/***********************************************************************
* SSD routines
**********************************************************************/

static int pixel_ssd_16x16_altivec ( uint8_t *pix1, int i_stride_pix1,
                                     uint8_t *pix2, int i_stride_pix2)
{
    DECLARE_ALIGNED( int, sum, 16 );
    
    int y;
    LOAD_ZERO;
    vec_u8_t  pix1vA, pix2vA, pix1vB, pix2vB;
    vec_u32_t sumv;
    vec_u8_t maxA, minA, diffA, maxB, minB, diffB;
    vec_u8_t temp_lv, temp_hv;
    vec_u8_t permA, permB;
    
    sumv = vec_splat_u32(0);
    
    permA = vec_lvsl(0, pix2);
    permB = vec_lvsl(0, pix2 + i_stride_pix2);
    
    temp_lv = vec_ld(0, pix2);
    temp_hv = vec_ld(16, pix2);
    pix2vA = vec_perm(temp_lv, temp_hv, permA);
    pix1vA = vec_ld(0, pix1);
    
    for (y=0; y < 7; y++)
    {
        pix1 += i_stride_pix1;
        pix2 += i_stride_pix2;
        
        
        maxA = vec_max(pix1vA, pix2vA);
        minA = vec_min(pix1vA, pix2vA);
    
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2vB = vec_perm(temp_lv, temp_hv, permB);
        pix1vB = vec_ld(0, pix1);
    
        
        diffA = vec_sub(maxA, minA);
        sumv = vec_msum(diffA, diffA, sumv);
        
        pix1 += i_stride_pix1;
        pix2 += i_stride_pix2;
        
        maxB = vec_max(pix1vB, pix2vB);
        minB = vec_min(pix1vB, pix2vB);
        
        temp_lv = vec_ld(0, pix2);
        temp_hv = vec_ld(16, pix2);
        pix2vA = vec_perm(temp_lv, temp_hv, permA);
        pix1vA = vec_ld(0, pix1);
        
        diffB = vec_sub(maxB, minB);
        sumv = vec_msum(diffB, diffB, sumv);
        
    }
    
    pix1 += i_stride_pix1;
    pix2 += i_stride_pix2;
    
    temp_lv = vec_ld(0, pix2);
    temp_hv = vec_ld(16, pix2);
    pix2vB = vec_perm(temp_lv, temp_hv, permB);
    pix1vB = vec_ld(0, pix1);
    
    maxA = vec_max(pix1vA, pix2vA);
    minA = vec_min(pix1vA, pix2vA);
    
    maxB = vec_max(pix1vB, pix2vB); 
    minB = vec_min(pix1vB, pix2vB); 
    
    diffA = vec_sub(maxA, minA);
    sumv = vec_msum(diffA, diffA, sumv);
    
    diffB = vec_sub(maxB, minB);
    sumv = vec_msum(diffB, diffB, sumv);
    
    sumv = (vec_u32_t) vec_sums((vec_s32_t) sumv, zero_s32v);
    sumv = vec_splat(sumv, 3);
    vec_ste((vec_s32_t) sumv, 0, &sum);
    return sum;
} 


/****************************************************************************
 * x264_pixel_init:
 ****************************************************************************/
void x264_pixel_altivec_init( x264_pixel_function_t *pixf )
{
    pixf->sad[PIXEL_16x16]  = pixel_sad_16x16_altivec;
    pixf->sad[PIXEL_8x16]   = pixel_sad_8x16_altivec;
    pixf->sad[PIXEL_16x8]   = pixel_sad_16x8_altivec;
    pixf->sad[PIXEL_8x8]    = pixel_sad_8x8_altivec;

    pixf->sad_x3[PIXEL_16x16] = pixel_sad_x3_16x16_altivec;
    pixf->sad_x3[PIXEL_8x16]  = pixel_sad_x3_8x16_altivec;
    pixf->sad_x3[PIXEL_16x8]  = pixel_sad_x3_16x8_altivec;
    pixf->sad_x3[PIXEL_8x8]   = pixel_sad_x3_8x8_altivec;

    pixf->sad_x4[PIXEL_16x16] = pixel_sad_x4_16x16_altivec;
    pixf->sad_x4[PIXEL_8x16]  = pixel_sad_x4_8x16_altivec;
    pixf->sad_x4[PIXEL_16x8]  = pixel_sad_x4_16x8_altivec;
    pixf->sad_x4[PIXEL_8x8]   = pixel_sad_x4_8x8_altivec;

    pixf->satd[PIXEL_16x16] = pixel_satd_16x16_altivec;
    pixf->satd[PIXEL_8x16]  = pixel_satd_8x16_altivec;
    pixf->satd[PIXEL_16x8]  = pixel_satd_16x8_altivec;
    pixf->satd[PIXEL_8x8]   = pixel_satd_8x8_altivec;
    pixf->satd[PIXEL_8x4]   = pixel_satd_8x4_altivec;
    pixf->satd[PIXEL_4x8]   = pixel_satd_4x8_altivec;
    pixf->satd[PIXEL_4x4]   = pixel_satd_4x4_altivec;
    
    pixf->ssd[PIXEL_16x16] = pixel_ssd_16x16_altivec;
}
