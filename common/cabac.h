/*****************************************************************************
 * cabac.h: xavs encoder library
 *****************************************************************************
 * Copyright (C) 2009 xavs project
 *
 * Authors: 
 *          
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

#ifndef XAVS_CABAC_H
#define XAVS_CABAC_H

typedef struct
{
    /* state */
    int i_low;
    int i_range;

    /* bit stream */
    int i_queue;
    int i_bytes_outstanding;

    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;

    /* aligned for memcpy_aligned starting here */
    DECLARE_ALIGNED_16( int f8_bits_encoded ); // only if using xavs_cabac_size_decision()

    /* context */
    uint8_t state[460];
} xavs_cabac_t;

extern const uint8_t xavs_cabac_transition[128][2];
extern const uint16_t xavs_cabac_entropy[128][2];

/* init the contexts given i_slice_type, the quantif and the model */
void xavs_cabac_context_init( xavs_cabac_t *cb, int i_slice_type, int i_qp, int i_model );

/* encoder only: */
void xavs_cabac_encode_init ( xavs_cabac_t *cb, uint8_t *p_data, uint8_t *p_end );
void xavs_cabac_encode_decision_c( xavs_cabac_t *cb, int i_ctx, int b );
void xavs_cabac_encode_decision_asm( xavs_cabac_t *cb, int i_ctx, int b );
void xavs_cabac_encode_bypass( xavs_cabac_t *cb, int b );
void xavs_cabac_encode_ue_bypass( xavs_cabac_t *cb, int exp_bits, int val );
void xavs_cabac_encode_terminal( xavs_cabac_t *cb );
void xavs_cabac_encode_flush( xavs_t *h, xavs_cabac_t *cb );

#ifdef HAVE_MMX
#define xavs_cabac_encode_decision xavs_cabac_encode_decision_asm
#else
#define xavs_cabac_encode_decision xavs_cabac_encode_decision_c
#endif
#define xavs_cabac_encode_decision_noup xavs_cabac_encode_decision

static inline int xavs_cabac_pos( xavs_cabac_t *cb )
{
    return (cb->p - cb->p_start + cb->i_bytes_outstanding) * 8 + cb->i_queue;
}

/* internal only. these don't write the bitstream, just calculate bit cost: */

static inline void xavs_cabac_size_decision( xavs_cabac_t *cb, long i_ctx, long b )
{
    int i_state = cb->state[i_ctx];
    cb->state[i_ctx] = xavs_cabac_transition[i_state][b];
    cb->f8_bits_encoded += xavs_cabac_entropy[i_state][b];
}

static inline int xavs_cabac_size_decision2( uint8_t *state, long b )
{
    int i_state = *state;
    *state = xavs_cabac_transition[i_state][b];
    return xavs_cabac_entropy[i_state][b];
}

static inline void xavs_cabac_size_decision_noup( xavs_cabac_t *cb, long i_ctx, long b )
{
    int i_state = cb->state[i_ctx];
    cb->f8_bits_encoded += xavs_cabac_entropy[i_state][b];
}

static inline int xavs_cabac_size_decision_noup2( uint8_t *state, long b )
{
    return xavs_cabac_entropy[*state][b];
}

#endif
