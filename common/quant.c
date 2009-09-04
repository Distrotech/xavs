/*****************************************************************************
 * quant.c: xavs encoder library
 *****************************************************************************
 * Copyright (C) 2009 xavs project
 *
 * Authors: 
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *****************************************************************************/

#include "common.h"

#ifdef HAVE_MMX
#include "x86/quant.h"
#endif
#ifdef ARCH_PPC
#   include "ppc/quant.h"
#endif

static const int dequant_shifttable[64] = {
		14,14,14,14,14,14,14,14,
		13,13,13,13,13,13,13,13,
		13,12,12,12,12,12,12,12,
		11,11,11,11,11,11,11,11,
		11,10,10,10,10,10,10,10,
		10, 9, 9, 9, 9, 9, 9, 9,
		 9, 8, 8, 8, 8, 8, 8, 8,
		 7, 7, 7, 7, 7, 7, 7, 7
};

/*
*************************************************************************
* Function: quantization 8x8 block 
* Input:    dct[8][8] 
*           mf  quantization matrix for 8x8 block 
*           f   quantization deadzone for 8x8 block          
* Output:   dct[8][8] quantizated data
* Return:   
* Attention: The data address of the input 8x8 block should be continuous
*  
*************************************************************************
*/

#define QUANT_ONE( coef, mf, f ) \
{ \
    if( (coef) > 0 ) \
        (coef) = (f + (coef)) * (mf) >> 15; \
    else \
        (coef) = - ((f - (coef)) * (mf) >> 15); \
    nz |= (coef); \
}

static int quant_8x8( int16_t dct[8][8], uint16_t mf[64], uint16_t bias[64] )
{
    int i, nz = 0;
    for( i = 0; i < 64; i++ )
        QUANT_ONE( dct[0][i], mf[i], bias[i] );
    return !!nz;
}


/*
*************************************************************************
* Function: dequantization 8x8 block 
* Input:    dct[8][8] 
*           dequant_mf  dequantization matrix for 8x8 block 
*           iq   quantization parameter
* Output:   dct[8][8] dequantizated data
* Return: 
* Attention: 
*************************************************************************
*/
#define DEQUANT_SHR( x ) \
    dct[y][x] = ( dct[y][x] * dequant_mf[i_qp][y][x] + f ) >> (shift_bits);\
    dct[y][x] = ( dct[y][x] < (-32768))?(-32768):(dct[y][x]>32767)?32767:(dct[y][x]);

static void dequant_8x8( int16_t dct[8][8], int dequant_mf[64][8][8], int i_qp )
{
	int y;
    const int shift_bits = dequant_shifttable[i_qp]; 
	const int f = 1 << (shift_bits-1);
		for( y = 0; y < 8; y++ )
        {
			DEQUANT_SHR( 0 )
            DEQUANT_SHR( 1 )
            DEQUANT_SHR( 2 )
            DEQUANT_SHR( 3 )
            DEQUANT_SHR( 4 )
            DEQUANT_SHR( 5 )
            DEQUANT_SHR( 6 )
            DEQUANT_SHR( 7 )
		}
}


static int ALWAYS_INLINE xavs_coeff_last_internal( int16_t *l, int i_count )
{
    int i_last;
    for( i_last = i_count-1; i_last >= 3; i_last -= 4 )
        if( *(uint64_t*)(l+i_last-3) )
            break;
    while( i_last >= 0 && l[i_last] == 0 )
        i_last--;
    return i_last;
}

static int xavs_coeff_last4( int16_t *l )
{
    return xavs_coeff_last_internal( l, 4 );
}
static int xavs_coeff_last15( int16_t *l )
{
    return xavs_coeff_last_internal( l, 15 );
}
static int xavs_coeff_last16( int16_t *l )
{
    return xavs_coeff_last_internal( l, 16 );
}
static int xavs_coeff_last64( int16_t *l )
{
    return xavs_coeff_last_internal( l, 64 );
}
#define level_run(num)\
static int xavs_coeff_level_run##num( int16_t *dct, xavs_run_level_t *runlevel )\
{\
    int i_last = runlevel->last = xavs_coeff_last##num(dct);\
    int i_total = 0;\
	int i=0;\
    do\
    {\
        int r = 0;\
        while( i <=i_last && dct[i] == 0 )\
            r++;\
        runlevel->level[i_total] = dct[i];\
        runlevel->run[i_total++] = r;\
    } while( i<=i_last);\
    return i_total;\
}


level_run(4)
level_run(15)
level_run(16)
level_run(64)
void xavs_quant_init( xavs_t *h, int cpu, xavs_quant_function_t *pf )
{
    pf->quant_8x8 = quant_8x8;
    pf->dequant_8x8 = dequant_8x8;

#ifdef HAVE_MMX
    if( cpu&XAVS_CPU_MMX )
    {
#ifdef ARCH_X86
        pf->quant_8x8 = xavs_quant_8x8_mmx;
        pf->dequant_8x8 = xavs_dequant_8x8_mmx;
        if( h->param.i_cqm_preset == XAVS_CQM_FLAT )
        {
            pf->dequant_8x8 = xavs_dequant_8x8_flat16_mmx;
        }
#endif
    }

    if( cpu&XAVS_CPU_SSE2 )
    {
        pf->quant_8x8 = xavs_quant_8x8_sse2;
        pf->dequant_8x8 = xavs_dequant_8x8_sse2;
    }

    if( cpu&XAVS_CPU_SSSE3 )
    {
        pf->quant_8x8 = xavs_quant_8x8_ssse3;
    }

    if( cpu&XAVS_CPU_SSE4 )
    {
        pf->quant_8x8 = xavs_quant_8x8_sse4;
    }
#endif // HAVE_MMX

#ifdef ARCH_PPC
    if( cpu&XAVS_CPU_ALTIVEC ) {
        pf->quant_8x8 = xavs_quant_8x8_altivec;
        pf->dequant_8x8 = xavs_dequant_8x8_altivec;
    }
#endif
}
