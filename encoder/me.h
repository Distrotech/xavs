/*****************************************************************************
 * me.h: xavs encoder library (Motion Estimation)
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

#ifndef XAVS_ME_H
#define XAVS_ME_H

#define COST_MAX (1<<28)
#define COST_MAX64 (1ULL<<60)

typedef struct
{
    /* input */
    int      i_pixel;   /* PIXEL_WxH */
    int16_t *p_cost_mv; /* lambda * nbits for each possible mv */
    int      i_ref_cost;
    int      i_ref;

    uint8_t *p_fref[6];
    uint8_t *p_fenc[3];
    uint16_t *integral;
    int      i_stride[2];

    DECLARE_ALIGNED_4( int16_t mvp[2] );

    /* output */
    int cost_mv;        /* lambda * nbits for the chosen mv */
    int cost;           /* satd + lambda * nbits */
    DECLARE_ALIGNED_4( int16_t mv[2] );
} DECLARE_ALIGNED_16( xavs_me_t );

typedef struct {
    int sad;
    int16_t mx, my;
} mvsad_t;

void xavs_me_search_ref( xavs_t *h, xavs_me_t *m, int16_t (*mvc)[2], int i_mvc, int *p_fullpel_thresh );
static inline void xavs_me_search( xavs_t *h, xavs_me_t *m, int16_t (*mvc)[2], int i_mvc )
    { xavs_me_search_ref( h, m, mvc, i_mvc, NULL ); }

void xavs_me_refine_qpel( xavs_t *h, xavs_me_t *m );
void xavs_me_refine_qpel_rd( xavs_t *h, xavs_me_t *m, int i_lambda2, int i4, int i_list );
void xavs_me_refine_bidir_rd( xavs_t *h, xavs_me_t *m0, xavs_me_t *m1, int i_weight, int i8, int i_lambda2 );
void xavs_me_refine_bidir_satd( xavs_t *h, xavs_me_t *m0, xavs_me_t *m1, int i_weight );
uint64_t xavs_rd_cost_part( xavs_t *h, int i_lambda2, int i8, int i_pixel );

extern uint16_t *xavs_cost_mv_fpel[92][4];

#define COPY1_IF_LT(x,y)\
if((y)<(x))\
    (x)=(y);

#define COPY2_IF_LT(x,y,a,b)\
if((y)<(x))\
{\
    (x)=(y);\
    (a)=(b);\
}

#define COPY3_IF_LT(x,y,a,b,c,d)\
if((y)<(x))\
{\
    (x)=(y);\
    (a)=(b);\
    (c)=(d);\
}

#define COPY4_IF_LT(x,y,a,b,c,d,f,e)\
if((y)<(x))\
{\
    (x)=(y);\
    (a)=(b);\
    (c)=(d);\
    (f)=(e);\
}

#endif
