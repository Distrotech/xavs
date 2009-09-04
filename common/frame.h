/*****************************************************************************
 * frame.h: xavs encoder library
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

#ifndef XAVS_FRAME_H
#define XAVS_FRAME_H

/* number of pixels past the edge of the frame, for motion estimation/compensation */
#define PADH 32
#define PADV 32

typedef struct
{
    /* */
    int     i_poc;
    int     i_type;
    int     i_qpplus1;
    int64_t i_pts;
    int     i_frame;    /* Presentation frame number */
    int     i_frame_num; /* Coded frame number */
    int     b_kept_as_ref;
    float   f_qp_avg_rc; /* QPs as decided by ratecontrol */
    float   f_qp_avg_aq; /* QPs as decided by AQ in addition to ratecontrol */

    /* YUV buffer */
    int     i_plane;
    int     i_stride[3];
    int     i_width[3];
    int     i_lines[3];
    int     i_stride_lowres;
    int     i_width_lowres;
    int     i_lines_lowres;
    uint8_t *plane[3];
    uint8_t *filtered[4]; /* plane[0], H, V, HV */
    uint8_t *lowres[4]; /* half-size copy of input frame: Orig, H, V, HV */
    uint16_t *integral;

    /* for unrestricted mv we allocate more data than needed
     * allocated data are stored in buffer */
    uint8_t *buffer[4];
    uint8_t *buffer_lowres[4];

    /* motion data */
    int8_t  *mb_type;
    int16_t (*mv[2])[2];
    int16_t (*lowres_mvs[2][XAVS_BFRAME_MAX+1])[2];
    uint16_t (*lowres_costs[XAVS_BFRAME_MAX+2][XAVS_BFRAME_MAX+2]);
    uint8_t  (*lowres_inter_types[XAVS_BFRAME_MAX+2][XAVS_BFRAME_MAX+2]);
    int     *lowres_mv_costs[2][XAVS_BFRAME_MAX+1];
    int8_t  *ref[2];
    int     i_ref[2];
    int     ref_poc[2][16];
    int     inv_ref_poc[16]; // inverse values (list0 only) to avoid divisions in MB encoding

    /* for adaptive B-frame decision.
     * contains the SATD cost of the lowres frame encoded in various modes
     * FIXME: how big an array do we need? */
    int     i_cost_est[XAVS_BFRAME_MAX+2][XAVS_BFRAME_MAX+2];
    int     i_cost_est_aq[XAVS_BFRAME_MAX+2][XAVS_BFRAME_MAX+2];
    int     i_satd; // the i_cost_est of the selected frametype
    int     i_intra_mbs[XAVS_BFRAME_MAX+2];
    int     *i_row_satds[XAVS_BFRAME_MAX+2][XAVS_BFRAME_MAX+2];
    int     *i_row_satd;
    int     *i_row_bits;
    int     *i_row_qp;
    float   *f_qp_offset;
    int     b_intra_calculated;
    uint16_t *i_intra_cost;
    uint16_t *i_propagate_cost;
    uint16_t *i_inv_qscale_factor;

    /* threading */
    int     i_lines_completed; /* in pixels */
    int     i_reference_count; /* number of threads using this frame (not necessarily the number of pointers) */
    xavs_pthread_mutex_t mutex;
    xavs_pthread_cond_t  cv;

} xavs_frame_t;

typedef void (*xavs_deblock_inter_t)( xavs_t *h, uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
typedef void (*xavs_deblock_intra_t)( xavs_t *h, uint8_t *pix, int stride, int alpha, int beta );
typedef struct
{
    xavs_deblock_inter_t deblock_v_luma;
    xavs_deblock_inter_t deblock_h_luma;
    xavs_deblock_inter_t deblock_v_chroma;
    xavs_deblock_inter_t deblock_h_chroma;
    xavs_deblock_intra_t deblock_v_luma_intra;
    xavs_deblock_intra_t deblock_h_luma_intra;
    xavs_deblock_intra_t deblock_v_chroma_intra;
    xavs_deblock_intra_t deblock_h_chroma_intra;
} xavs_deblock_function_t;

xavs_frame_t *xavs_frame_new( xavs_t *h );
void          xavs_frame_delete( xavs_frame_t *frame );

int           xavs_frame_copy_picture( xavs_t *h, xavs_frame_t *dst, xavs_picture_t *src );

void          xavs_frame_expand_border( xavs_t *h, xavs_frame_t *frame, int mb_y, int b_end );
void          xavs_frame_expand_border_filtered( xavs_t *h, xavs_frame_t *frame, int mb_y, int b_end );
void          xavs_frame_expand_border_lowres( xavs_frame_t *frame );
void          xavs_frame_expand_border_mod16( xavs_t *h, xavs_frame_t *frame );

void          xavs_frame_deblock( xavs_t *h );
void          xavs_frame_deblock_row( xavs_t *h, int mb_y );

void          xavs_frame_filter( xavs_t *h, xavs_frame_t *frame, int mb_y, int b_end );
void          xavs_frame_init_lowres( xavs_t *h, xavs_frame_t *frame );

void          xavs_deblock_init( int cpu, xavs_deblock_function_t *pf );

void          xavs_frame_cond_broadcast( xavs_frame_t *frame, int i_lines_completed );
void          xavs_frame_cond_wait( xavs_frame_t *frame, int i_lines_completed );

void          xavs_frame_push( xavs_frame_t **list, xavs_frame_t *frame );
xavs_frame_t *xavs_frame_pop( xavs_frame_t **list );
void          xavs_frame_unshift( xavs_frame_t **list, xavs_frame_t *frame );
xavs_frame_t *xavs_frame_shift( xavs_frame_t **list );
void          xavs_frame_push_unused( xavs_t *h, xavs_frame_t *frame );
xavs_frame_t *xavs_frame_pop_unused( xavs_t *h );
void          xavs_frame_sort( xavs_frame_t **list, int b_dts );
#define xavs_frame_sort_dts(list) xavs_frame_sort(list, 1)
#define xavs_frame_sort_pts(list) xavs_frame_sort(list, 0)

#endif
