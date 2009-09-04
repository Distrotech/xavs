/*****************************************************************************
 * dct.h: xavs encoder library
 *****************************************************************************
 * Copyright (C) 2009 XAVS project
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

#ifndef XAVS_DCT_H
#define XAVS_DCT_H

typedef struct
{
    // pix1  stride = FENC_STRIDE
    // pix2  stride = FDEC_STRIDE
    // p_dst stride = FDEC_STRIDE
    void (*sub8x8_dct8) ( int16_t dct[8][8], uint8_t *pix1, uint8_t *pix2 );
    void (*add8x8_idct8)( uint8_t *p_dst, int16_t dct[8][8] );
	
    void (*sub16x16_dct8) ( int16_t dct[4][8][8], uint8_t *pix1, uint8_t *pix2 );
    void (*add16x16_idct8)( uint8_t *p_dst, int16_t dct[4][8][8] );

} xavs_dct_function_t;

typedef struct
{
    void (*scan_8x8)( int16_t level[64], int16_t dct[8][8] );
    int  (*sub_8x8) ( int16_t level[64], const uint8_t *p_src, uint8_t *p_dst );
} xavs_zigzag_function_t;

void xavs_dct_init( int cpu, xavs_dct_function_t *dctf );
void xavs_zigzag_init( int cpu, xavs_zigzag_function_t *pf, int b_interlaced );

#endif
