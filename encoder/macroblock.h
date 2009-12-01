/*****************************************************************************
 * macroblock.h: xavs encoder library
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

#ifndef XAVS_ENCODER_MACROBLOCK_H
#define XAVS_ENCODER_MACROBLOCK_H

#include "common/macroblock.h"

extern const int xavs_lambda2_tab[52];
extern const int xavs_lambda_tab[52];

void xavs_rdo_init( void );

int xavs_macroblock_probe_skip( xavs_t *h, int b_bidir );

static inline int xavs_macroblock_probe_pskip( xavs_t *h )
    { return xavs_macroblock_probe_skip( h, 0 ); }
static inline int xavs_macroblock_probe_bskip( xavs_t *h )
    { return xavs_macroblock_probe_skip( h, 1 ); }

void xavs_predict_lossless_8x8_chroma( xavs_t *h, int i_mode );
void xavs_predict_lossless_4x4( xavs_t *h, uint8_t *p_dst, int idx, int i_mode );
void xavs_predict_lossless_8x8( xavs_t *h, uint8_t *p_dst, int idx, int i_mode, uint8_t edge[33] );
void xavs_predict_lossless_16x16( xavs_t *h, int i_mode );

void xavs_macroblock_encode      ( xavs_t *h );
void xavs_macroblock_write_cabac ( xavs_t *h, xavs_cabac_t *cb );
//void xavs_macroblock_write_cavlc ( xavs_t *h, bs_t *s );
void xavs_macroblock_write_cavlc ( xavs_t *h, bs_t *s );//yangping

void xavs_macroblock_encode_p8x8( xavs_t *h, int i8 );
void xavs_macroblock_encode_p4x4( xavs_t *h, int i4 );
void xavs_mb_encode_i8x8( xavs_t *h, int idx, int i_qp );
void xavs_mb_encode_8x8_chroma( xavs_t *h, int b_inter, int i_qp );

void xavs_cabac_mb_skip( xavs_t *h, int b_skip );

int xavs_quant_dc_trellis( xavs_t *h, int16_t *dct, int i_quant_cat,
                             int i_qp, int i_ctxBlockCat, int b_intra, int b_chroma );
int xavs_quant_4x4_trellis( xavs_t *h, int16_t dct[4][4], int i_quant_cat,
                             int i_qp, int i_ctxBlockCat, int b_intra, int b_chroma, int idx );
int xavs_quant_8x8_trellis( xavs_t *h, int16_t dct[8][8], int i_quant_cat,
                             int i_qp, int b_intra, int b_chroma, int idx );

void xavs_noise_reduction_update( xavs_t *h );

#endif

