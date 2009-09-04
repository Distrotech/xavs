/*****************************************************************************
 * frame.c:xavs encoder library
 *****************************************************************************
 * Copyright (C) 2009  xavs project
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

#define ALIGN(x,a) (((x)+((a)-1))&~((a)-1))

xavs_frame_t *xavs_frame_new( xavs_t *h )
{
    xavs_frame_t *frame;
    int i, j;

    int i_mb_count = h->mb.i_mb_count;
    int i_stride, i_width, i_lines;
    int i_padv = PADV << h->param.b_interlaced;
    int luma_plane_size;
    int chroma_plane_size;
    int align = h->param.cpu&XAVS_CPU_CACHELINE_64 ? 64 : h->param.cpu&XAVS_CPU_CACHELINE_32 ? 32 : 16;

    CHECKED_MALLOCZERO( frame, sizeof(xavs_frame_t) );

    /* allocate frame data (+64 for extra data for me) */
    i_width  = ALIGN( h->param.i_width, 16 );
    i_stride = ALIGN( i_width + 2*PADH, align );
    i_lines  = ALIGN( h->param.i_height, 16<<h->param.b_interlaced );

    frame->i_plane = 3;
    for( i = 0; i < 3; i++ )
    {
        frame->i_stride[i] = ALIGN( i_stride >> !!i, align );
        frame->i_width[i] = i_width >> !!i;
        frame->i_lines[i] = i_lines >> !!i;
    }

    luma_plane_size = (frame->i_stride[0] * ( frame->i_lines[0] + 2*i_padv ));
    chroma_plane_size = (frame->i_stride[1] * ( frame->i_lines[1] + 2*i_padv ));
    for( i = 1; i < 3; i++ )
    {
        CHECKED_MALLOC( frame->buffer[i], chroma_plane_size );
        frame->plane[i] = frame->buffer[i] + (frame->i_stride[i] * i_padv + PADH)/2;
    }
    /* all 4 luma planes allocated together, since the cacheline split code
     * requires them to be in-phase wrt cacheline alignment. */
    if( h->param.analyse.i_subpel_refine )
    {
        CHECKED_MALLOC( frame->buffer[0], 4*luma_plane_size);
        for( i = 0; i < 4; i++ )
            frame->filtered[i] = frame->buffer[0] + i*luma_plane_size + frame->i_stride[0] * i_padv + PADH;
        frame->plane[0] = frame->filtered[0];
    }
    else
    {
        CHECKED_MALLOC( frame->buffer[0], luma_plane_size);
        frame->plane[0] = frame->buffer[0] + frame->i_stride[0] * i_padv + PADH;
    }

    if( h->frames.b_have_lowres )
    {
        frame->i_width_lowres = frame->i_width[0]/2;
        frame->i_stride_lowres = ALIGN( frame->i_width_lowres + 2*PADH, align );
        frame->i_lines_lowres = frame->i_lines[0]/2;

        luma_plane_size = frame->i_stride_lowres * ( frame->i_lines[0]/2 + 2*i_padv );

        CHECKED_MALLOC( frame->buffer_lowres[0], 4 * luma_plane_size );
        for( i = 0; i < 4; i++ )
            frame->lowres[i] = frame->buffer_lowres[0] + (frame->i_stride_lowres * i_padv + PADH) + i * luma_plane_size;

        for( j = 0; j <= !!h->param.i_bframe; j++ )
            for( i = 0; i <= h->param.i_bframe; i++ )
            {
                CHECKED_MALLOCZERO( frame->lowres_mvs[j][i], 2*h->mb.i_mb_count*sizeof(int16_t) );
                CHECKED_MALLOC( frame->lowres_mv_costs[j][i], h->mb.i_mb_count*sizeof(int) );
            }
        CHECKED_MALLOC( frame->i_intra_cost, i_mb_count * sizeof(uint16_t) );
        memset( frame->i_intra_cost, -1, i_mb_count * sizeof(uint16_t) );
        CHECKED_MALLOC( frame->i_propagate_cost, i_mb_count * sizeof(uint16_t) );
        for( j = 0; j <= h->param.i_bframe+1; j++ )
            for( i = 0; i <= h->param.i_bframe+1; i++ )
            {
                CHECKED_MALLOC( frame->lowres_costs[j][i], i_mb_count * sizeof(uint16_t) );
                CHECKED_MALLOC( frame->lowres_inter_types[j][i], i_mb_count * sizeof(uint8_t) );
            }
    }

    if( h->param.analyse.i_me_method >= XAVS_ME_ESA )
    {
        CHECKED_MALLOC( frame->buffer[3],
                        frame->i_stride[0] * (frame->i_lines[0] + 2*i_padv) * sizeof(uint16_t) << h->frames.b_have_sub8x8_esa );
        frame->integral = (uint16_t*)frame->buffer[3] + frame->i_stride[0] * i_padv + PADH;
    }

    frame->i_poc = -1;
    frame->i_type = XAVS_TYPE_AUTO;
    frame->i_qpplus1 = 0;
    frame->i_pts = -1;
    frame->i_frame = -1;
    frame->i_frame_num = -1;
    frame->i_lines_completed = -1;

    CHECKED_MALLOC( frame->mb_type, i_mb_count * sizeof(int8_t));
    CHECKED_MALLOC( frame->mv[0], 2*16 * i_mb_count * sizeof(int16_t) );
    CHECKED_MALLOC( frame->ref[0], 4 * i_mb_count * sizeof(int8_t) );
    if( h->param.i_bframe )
    {
        CHECKED_MALLOC( frame->mv[1], 2*16 * i_mb_count * sizeof(int16_t) );
        CHECKED_MALLOC( frame->ref[1], 4 * i_mb_count * sizeof(int8_t) );
    }
    else
    {
        frame->mv[1]  = NULL;
        frame->ref[1] = NULL;
    }

    CHECKED_MALLOC( frame->i_row_bits, i_lines/16 * sizeof(int) );
    CHECKED_MALLOC( frame->i_row_qp, i_lines/16 * sizeof(int) );
    for( i = 0; i < h->param.i_bframe + 2; i++ )
        for( j = 0; j < h->param.i_bframe + 2; j++ )
            CHECKED_MALLOC( frame->i_row_satds[i][j], i_lines/16 * sizeof(int) );

    if( h->param.rc.i_aq_mode )
    {
        CHECKED_MALLOC( frame->f_qp_offset, h->mb.i_mb_count * sizeof(float) );
        if( h->frames.b_have_lowres )
            CHECKED_MALLOC( frame->i_inv_qscale_factor, h->mb.i_mb_count * sizeof(uint16_t) );
    }

    if( xavs_pthread_mutex_init( &frame->mutex, NULL ) )
        goto fail;
    if( xavs_pthread_cond_init( &frame->cv, NULL ) )
        goto fail;

    return frame;

fail:
    xavs_free( frame );
    return NULL;
}

void xavs_frame_delete( xavs_frame_t *frame )
{
    int i, j;
    for( i = 0; i < 4; i++ )
        xavs_free( frame->buffer[i] );
    for( i = 0; i < 4; i++ )
        xavs_free( frame->buffer_lowres[i] );
    for( i = 0; i < XAVS_BFRAME_MAX+2; i++ )
        for( j = 0; j < XAVS_BFRAME_MAX+2; j++ )
            xavs_free( frame->i_row_satds[i][j] );
    for( j = 0; j < 2; j++ )
        for( i = 0; i <= XAVS_BFRAME_MAX; i++ )
        {
            xavs_free( frame->lowres_mvs[j][i] );
            xavs_free( frame->lowres_mv_costs[j][i] );
        }
    xavs_free( frame->i_propagate_cost );
    for( j = 0; j <= XAVS_BFRAME_MAX+1; j++ )
        for( i = 0; i <= XAVS_BFRAME_MAX+1; i++ )
        {
            xavs_free( frame->lowres_costs[j][i] );
            xavs_free( frame->lowres_inter_types[j][i] );
        }
    xavs_free( frame->f_qp_offset );
    xavs_free( frame->i_inv_qscale_factor );
    xavs_free( frame->i_intra_cost );
    xavs_free( frame->i_row_bits );
    xavs_free( frame->i_row_qp );
    xavs_free( frame->mb_type );
    xavs_free( frame->mv[0] );
    xavs_free( frame->mv[1] );
    xavs_free( frame->ref[0] );
    xavs_free( frame->ref[1] );
    xavs_pthread_mutex_destroy( &frame->mutex );
    xavs_pthread_cond_destroy( &frame->cv );
    xavs_free( frame );
}

int xavs_frame_copy_picture( xavs_t *h, xavs_frame_t *dst, xavs_picture_t *src )
{
    int i_csp = src->img.i_csp & XAVS_CSP_MASK;
    int i;
    if( i_csp != XAVS_CSP_I420 && i_csp != XAVS_CSP_YV12 )
    {
        xavs_log( h, XAVS_LOG_ERROR, "Arg invalid CSP\n" );
        return -1;
    }

    dst->i_type     = src->i_type;
    dst->i_qpplus1  = src->i_qpplus1;
    dst->i_pts      = src->i_pts;

    for( i=0; i<3; i++ )
    {
        int s = (i_csp == XAVS_CSP_YV12 && i) ? i^3 : i;
        uint8_t *plane = src->img.plane[s];
        int stride = src->img.i_stride[s];
        int width = h->param.i_width >> !!i;
        int height = h->param.i_height >> !!i;
        if( src->img.i_csp & XAVS_CSP_VFLIP )
        {
            plane += (height-1)*stride;
            stride = -stride;
        }
        h->mc.plane_copy( dst->plane[i], dst->i_stride[i], plane, stride, width, height );
    }
    return 0;
}



static void plane_expand_border( uint8_t *pix, int i_stride, int i_width, int i_height, int i_padh, int i_padv, int b_pad_top, int b_pad_bottom )
{
#define PPIXEL(x, y) ( pix + (x) + (y)*i_stride )
    int y;
    for( y = 0; y < i_height; y++ )
    {
        /* left band */
        memset( PPIXEL(-i_padh, y), PPIXEL(0, y)[0], i_padh );
        /* right band */
        memset( PPIXEL(i_width, y), PPIXEL(i_width-1, y)[0], i_padh );
    }
    /* upper band */
    if( b_pad_top )
    for( y = 0; y < i_padv; y++ )
        memcpy( PPIXEL(-i_padh, -y-1), PPIXEL(-i_padh, 0), i_width+2*i_padh );
    /* lower band */
    if( b_pad_bottom )
    for( y = 0; y < i_padv; y++ )
        memcpy( PPIXEL(-i_padh, i_height+y), PPIXEL(-i_padh, i_height-1), i_width+2*i_padh );
#undef PPIXEL
}

void xavs_frame_expand_border( xavs_t *h, xavs_frame_t *frame, int mb_y, int b_end )
{
    int i;
    int b_start = !mb_y;
    if( mb_y & h->sh.b_mbaff )
        return;
    for( i = 0; i < frame->i_plane; i++ )
    {
        int stride = frame->i_stride[i];
        int width = 16*h->sps->i_mb_width >> !!i;
        int height = (b_end ? 16*(h->sps->i_mb_height - mb_y) >> h->sh.b_mbaff : 16) >> !!i;
        int padh = PADH >> !!i;
        int padv = PADV >> !!i;
        // buffer: 2 chroma, 3 luma (rounded to 4) because deblocking goes beyond the top of the mb
        uint8_t *pix = frame->plane[i] + XAVS_MAX(0, (16*mb_y-4)*stride >> !!i);
        if( b_end && !b_start )
            height += 4 >> (!!i + h->sh.b_mbaff);
        if( h->sh.b_mbaff )
        {
            plane_expand_border( pix, stride*2, width, height, padh, padv, b_start, b_end );
            plane_expand_border( pix+stride, stride*2, width, height, padh, padv, b_start, b_end );
        }
        else
        {
            plane_expand_border( pix, stride, width, height, padh, padv, b_start, b_end );
        }
    }
}

void xavs_frame_expand_border_filtered( xavs_t *h, xavs_frame_t *frame, int mb_y, int b_end )
{
    /* during filtering, 8 extra pixels were filtered on each edge,
     * but up to 3 of the horizontal ones may be wrong.
       we want to expand border from the last filtered pixel */
    int b_start = !mb_y;
    int stride = frame->i_stride[0];
    int width = 16*h->sps->i_mb_width + 8;
    int height = b_end ? (16*(h->sps->i_mb_height - mb_y) >> h->sh.b_mbaff) + 16 : 16;
    int padh = PADH - 4;
    int padv = PADV - 8;
    int i;
    for( i = 1; i < 4; i++ )
    {
        // buffer: 8 luma, to match the hpel filter
        uint8_t *pix = frame->filtered[i] + (16*mb_y - (8 << h->sh.b_mbaff)) * stride - 4;
        if( h->sh.b_mbaff )
        {
            plane_expand_border( pix, stride*2, width, height, padh, padv, b_start, b_end );
            plane_expand_border( pix+stride, stride*2, width, height, padh, padv, b_start, b_end );
        }
        else
        {
            plane_expand_border( pix, stride, width, height, padh, padv, b_start, b_end );
        }
    }
}

void xavs_frame_expand_border_lowres( xavs_frame_t *frame )
{
    int i;
    for( i = 0; i < 4; i++ )
        plane_expand_border( frame->lowres[i], frame->i_stride_lowres, frame->i_stride_lowres - 2*PADH, frame->i_lines_lowres, PADH, PADV, 1, 1 );
}

void xavs_frame_expand_border_mod16( xavs_t *h, xavs_frame_t *frame )
{
    int i, y;
    for( i = 0; i < frame->i_plane; i++ )
    {
        int i_subsample = i ? 1 : 0;
        int i_width = h->param.i_width >> i_subsample;
        int i_height = h->param.i_height >> i_subsample;
        int i_padx = ( h->sps->i_mb_width * 16 - h->param.i_width ) >> i_subsample;
        int i_pady = ( h->sps->i_mb_height * 16 - h->param.i_height ) >> i_subsample;

        if( i_padx )
        {
            for( y = 0; y < i_height; y++ )
                memset( &frame->plane[i][y*frame->i_stride[i] + i_width],
                         frame->plane[i][y*frame->i_stride[i] + i_width - 1],
                         i_padx );
        }
        if( i_pady )
        {
            //FIXME interlace? or just let it pad using the wrong field
            for( y = i_height; y < i_height + i_pady; y++ )
                memcpy( &frame->plane[i][y*frame->i_stride[i]],
                        &frame->plane[i][(i_height-1)*frame->i_stride[i]],
                        i_width + i_padx );
        }
    }
}


/* cavlc + 8x8 transform stores nnz per 16 coeffs for the purpose of
 * entropy coding, but per 64 coeffs for the purpose of deblocking */
static void munge_cavlc_nnz_row( xavs_t *h, int mb_y, uint8_t (*buf)[16] )
{
    uint32_t (*src)[6] = (uint32_t(*)[6])h->mb.non_zero_count + mb_y * h->sps->i_mb_width;
    int8_t *transform = h->mb.mb_transform_size + mb_y * h->sps->i_mb_width;
    int x, nnz;
    for( x=0; x<h->sps->i_mb_width; x++ )
    {
        memcpy( buf+x, src+x, 16 );
        if( transform[x] )
        {
            nnz = src[x][0] | src[x][1];
            src[x][0] = src[x][1] = ((uint16_t)nnz ? 0x0101 : 0) + (nnz>>16 ? 0x01010000 : 0);
            nnz = src[x][2] | src[x][3];
            src[x][2] = src[x][3] = ((uint16_t)nnz ? 0x0101 : 0) + (nnz>>16 ? 0x01010000 : 0);
        }
    }
}

static void restore_cavlc_nnz_row( xavs_t *h, int mb_y, uint8_t (*buf)[16] )
{
    uint8_t (*dst)[24] = h->mb.non_zero_count + mb_y * h->sps->i_mb_width;
    int x;
    for( x=0; x<h->sps->i_mb_width; x++ )
        memcpy( dst+x, buf+x, 16 );
}

static void munge_cavlc_nnz( xavs_t *h, int mb_y, uint8_t (*buf)[16], void (*func)(xavs_t*, int, uint8_t (*)[16]) )
{
    func( h, mb_y, buf );
    if( mb_y > 0 )
        func( h, mb_y-1, buf + h->sps->i_mb_width );
    if( h->sh.b_mbaff )
    {
        func( h, mb_y+1, buf + h->sps->i_mb_width * 2 );
        if( mb_y > 0 )
            func( h, mb_y-2, buf + h->sps->i_mb_width * 3 );
    }
}


/* Deblocking filter */
static const uint8_t i_alpha_table[64]  = {
	0, 0, 0, 0, 0, 0, 1, 1,
	1, 1, 1, 2, 2, 2, 3, 3,
	4, 4, 5, 5, 6, 7, 8, 9,
	10,11,12,13,15,16,18,20,
	22,24,26,28,30,33,33,35,
	35,36,37,37,39,39,42,44,
	46,48,50,52,53,54,55,56,
	57,58,59,60,61,62,63,64 
} ;
static const uint8_t  i_beta_table[64]  = {
	0, 0, 0, 0, 0, 0, 1, 1,
	1, 1, 1, 1, 1, 2, 2, 2,
	2, 2, 3, 3, 3, 3, 4, 4,
	4, 4, 5, 5, 5, 5, 6, 6,
	6, 7, 7, 7, 8, 8, 8, 9,
	9,10,10,11,11,12,13,14,
	15,16,17,18,19,20,21,22,
	23,23,24,24,25,25,26,27
};

static const uint8_t i_tc0_table[64] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 2, 2,
	2, 2, 2, 2, 2, 2, 3, 3,
	3, 3, 3, 3, 3, 4, 4, 4,
	5, 5, 5, 6, 6, 6, 7, 7,
	7, 7, 8, 8, 8, 9, 9, 9
} ;


/* From ffmpeg */
static inline void deblock_luma_c( xavs_t *h,uint8_t *pix, int xstride, int ystride, int alpha, int beta, int8_t tc0 )
{
	int i = 0;
	int ap = 0;
	int aq = 0;
	int small_gap = 0;
	int qp0 = 0;
	int Chroma = 0;
	int delta = 0;
	int bs = h->xavs_loop_filter.bS[h->xavs_loop_filter.i_edge];
	for (i = 0;i<16;i++)
	{
		const int p2 = pix[-3*xstride];
		const int p1 = pix[-2*xstride];
		const int p0 = pix[-1*xstride];
		const int q0 = pix[ 0*xstride];
		const int q1 = pix[ 1*xstride];
		const int q2 = pix[ 2*xstride];
		if (abs(q0 - p0) < alpha &&
			abs(q0 - p1) < beta &&
			abs(p0 - q0) < beta)
		{
			aq = (abs( q0 - q2) < beta) ;
			ap = (abs( p0 - p2) < beta) ;
			//if (bs == 2)
			//{
			//	qp0 = q0 +p0;
			//	small_gap = ((abs(q0 - p0)) < ((alpha >> 2) + 2));
			//	aq &= small_gap;
			//	ap &= small_gap;

			//	pix[  0*xstride] = aq ? (q1 + qp0 + q0  + 2) >> 2 : ((q1 << 1) + q0 + p0 + 2) >> 2;
			//	pix[ -1*xstride] = ap ? (p0 + p0  + qp0 + 2) >> 2 : ((p1 << 1) + p0 + q0 + 2) >> 2;

			//	pix[  1*xstride] = (aq && !Chroma) ? (q0 + q1 + p0 + q1 + 2) >> 2 : pix[  1*xstride];
			//	pix[ -2*xstride] = (ap && !Chroma) ? (p1 + p1 + p0 + q0 + 2) >> 2 : pix[ -2*xstride];
			//}
			//else
			{ //bs == 1
                delta = xavs_clip3( -tc0, tc0, ( (q0 - p0) * 3 + (p1 - q1) + 4) >> 3 ) ;
				pix[ -1*xstride] = xavs_clip_uint8(p0 + delta);
				pix[  0*xstride] = xavs_clip_uint8(q0 - delta);
				if (!Chroma)
				{
					if (ap)
					{
						delta =  xavs_clip3( -tc0, tc0, ( (p0 - p1) * 3 + (p2 - q0) + 4) >> 3 ) ;
						pix[ -2*xstride] = xavs_clip_uint8(p1 + delta);
					}
					if (aq)
					{
						delta =  xavs_clip3( -tc0, tc0, ( (q1 - q0) * 3 + (p0 - q2) + 4) >> 3 ) ;
						pix[ -2*xstride] = xavs_clip_uint8(q1 - delta);
					}
					
				}
			}	
		}
		pix += ystride;
	}
}
static void deblock_v_luma_c(xavs_t *h, uint8_t *pix, int stride, int alpha, int beta, int8_t tc0 )
{
    deblock_luma_c(h, pix, stride, 1, alpha, beta, tc0 );
}
static void deblock_h_luma_c(xavs_t *h, uint8_t *pix, int stride, int alpha, int beta, int8_t tc0 )
{
    deblock_luma_c(h, pix, 1, stride, alpha, beta, tc0 );
}

static inline void deblock_chroma_c( xavs_t *h,uint8_t *pix, int xstride, int ystride, int alpha, int beta, int8_t tc0 )
{
	int i = 0;
	int ap = 0;
	int aq = 0;
	int small_gap = 0;
	int qp0 = 0;
	int Chroma = 1;
	int delta = 0;
	int bs = h->xavs_loop_filter.bS[h->xavs_loop_filter.i_edge];
	for (i = 0;i<16;i++)
	{
		const int p2 = pix[-3*xstride];
		const int p1 = pix[-2*xstride];
		const int p0 = pix[-1*xstride];
		const int q0 = pix[ 0*xstride];
		const int q1 = pix[ 1*xstride];
		const int q2 = pix[ 2*xstride];
		if (abs(q0 - p0) < alpha &&
			abs(q0 - p1) < beta &&
			abs(p0 - q0) < beta)
		{
			aq = (abs( q0 - q2) < beta) ;
			ap = (abs( p0 - p2) < beta) ;
			//if (bs == 2)
			//{
			//	qp0 = q0 +p0;
			//	small_gap = ((abs(q0 - p0)) < ((alpha >> 2) + 2));
			//	aq &= small_gap;
			//	ap &= small_gap;

			//	pix[  0*xstride] = aq ? (q1 + qp0 + q0  + 2) >> 2 : ((q1 << 1) + q0 + p0 + 2) >> 2;
			//	pix[ -1*xstride] = ap ? (p0 + p0  + qp0 + 2) >> 2 : ((p1 << 1) + p0 + q0 + 2) >> 2;

			//	pix[  1*xstride] = (aq && !Chroma) ? (q0 + q1 + p0 + q1 + 2) >> 2 : pix[  1*xstride];
			//	pix[ -2*xstride] = (ap && !Chroma) ? (p1 + p1 + p0 + q0 + 2) >> 2 : pix[ -2*xstride];
			//}
			//else 
			{//bs == 1
				delta = xavs_clip3( -tc0, tc0, ( (q0 - p0) * 3 + (p1 - q1) + 4) >> 3 ) ;
				pix[ -1*xstride] = xavs_clip_uint8(p0 + delta);
				pix[  0*xstride] = xavs_clip_uint8(q0 - delta);
				if (!Chroma)
				{
					if (ap)
					{
						delta =  xavs_clip3( -tc0, tc0, ( (p0 - p1) * 3 + (p2 - q0) + 4) >> 3 ) ;
						pix[ -2*xstride] = xavs_clip_uint8(p1 + delta);
					}
					if (aq)
					{
						delta =  xavs_clip3( -tc0, tc0, ( (q1 - q0) * 3 + (p0 - q2) + 4) >> 3 ) ;
						pix[ -2*xstride] = xavs_clip_uint8(q1 - delta);
					}

				}
			}	
		}
		pix += ystride;
	}
}
static void deblock_v_chroma_c(xavs_t *h, uint8_t *pix, int stride, int alpha, int beta, int8_t tc0 )
{
    deblock_chroma_c( h, pix, stride, 1, alpha, beta, tc0 );
}
static void deblock_h_chroma_c(xavs_t *h, uint8_t *pix, int stride, int alpha, int beta, int8_t tc0 )
{
    deblock_chroma_c( h, pix, 1, stride, alpha, beta, tc0 );
}

static inline void deblock_luma_intra_c( xavs_t *h,uint8_t *pix, int xstride, int ystride, int alpha, int beta )
{
	int i = 0;
	int ap = 0;
	int aq = 0;
	int small_gap = 0;
	int qp0 = 0;
	int Chroma = 0;
	int delta = 0;
	int bs = h->xavs_loop_filter.bS[h->xavs_loop_filter.i_edge];
	for (i = 0;i<16;i++)
	{
		const int p2 = pix[-3*xstride];
		const int p1 = pix[-2*xstride];
		const int p0 = pix[-1*xstride];
		const int q0 = pix[ 0*xstride];
		const int q1 = pix[ 1*xstride];
		const int q2 = pix[ 2*xstride];
		if (abs(q0 - p0) < alpha &&
			abs(q0 - p1) < beta &&
			abs(p0 - q0) < beta)
		{
			aq = (abs( q0 - q2) < beta) ;
			ap = (abs( p0 - p2) < beta) ;
			//if (bs == 2)
			{
				qp0 = q0 +p0;
				small_gap = ((abs(q0 - p0)) < ((alpha >> 2) + 2));
				aq &= small_gap;
				ap &= small_gap;

				pix[  0*xstride] = aq ? (q1 + qp0 + q0  + 2) >> 2 : ((q1 << 1) + q0 + p0 + 2) >> 2;
				pix[ -1*xstride] = ap ? (p0 + p0  + qp0 + 2) >> 2 : ((p1 << 1) + p0 + q0 + 2) >> 2;

				pix[  1*xstride] = (aq && !Chroma) ? (q0 + q1 + p0 + q1 + 2) >> 2 : pix[  1*xstride];
				pix[ -2*xstride] = (ap && !Chroma) ? (p1 + p1 + p0 + q0 + 2) >> 2 : pix[ -2*xstride];
			}
		}
		pix += ystride;
	}
}

static void deblock_v_luma_intra_c( xavs_t *h,uint8_t *pix, int stride, int alpha, int beta )
{
    deblock_luma_intra_c( h,pix, stride, 1, alpha, beta );
}
static void deblock_h_luma_intra_c( xavs_t *h,uint8_t *pix, int stride, int alpha, int beta )
{
    deblock_luma_intra_c( h,pix, 1, stride, alpha, beta );
}

static inline void deblock_chroma_intra_c( xavs_t *h,uint8_t *pix, int xstride, int ystride, int alpha, int beta )
{
	int i = 0;
	int ap = 0;
	int aq = 0;
	int small_gap = 0;
	int qp0 = 0;
	int Chroma = 1;
	int delta = 0;
	int bs = h->xavs_loop_filter.bS[h->xavs_loop_filter.i_edge];
	for (i = 0;i<16;i++)
	{
		const int p2 = pix[-3*xstride];
		const int p1 = pix[-2*xstride];
		const int p0 = pix[-1*xstride];
		const int q0 = pix[ 0*xstride];
		const int q1 = pix[ 1*xstride];
		const int q2 = pix[ 2*xstride];
		if (abs(q0 - p0) < alpha &&
			abs(q0 - p1) < beta &&
			abs(p0 - q0) < beta)
		{
			aq = (abs( q0 - q2) < beta) ;
			ap = (abs( p0 - p2) < beta) ;
			//if (bs == 2)
			{
				qp0 = q0 +p0;
				small_gap = ((abs(q0 - p0)) < ((alpha >> 2) + 2));
				aq &= small_gap;
				ap &= small_gap;

				pix[  0*xstride] = aq ? (q1 + qp0 + q0  + 2) >> 2 : ((q1 << 1) + q0 + p0 + 2) >> 2;
				pix[ -1*xstride] = ap ? (p0 + p0  + qp0 + 2) >> 2 : ((p1 << 1) + p0 + q0 + 2) >> 2;

				pix[  1*xstride] = (aq && !Chroma) ? (q0 + q1 + p0 + q1 + 2) >> 2 : pix[  1*xstride];
				pix[ -2*xstride] = (ap && !Chroma) ? (p1 + p1 + p0 + q0 + 2) >> 2 : pix[ -2*xstride];
			}
		}
		pix += ystride;
	}
}

static void deblock_v_chroma_intra_c( xavs_t *h,uint8_t *pix, int stride, int alpha, int beta )
{
    deblock_chroma_intra_c( h,pix, stride, 1, alpha, beta );
}
static void deblock_h_chroma_intra_c( xavs_t *h,uint8_t *pix, int stride, int alpha, int beta )
{
    deblock_chroma_intra_c( h,pix, 1, stride, alpha, beta );
}

static inline void deblock_edge( xavs_t *h, uint8_t *pix1, uint8_t *pix2, int i_stride, uint8_t bS[4], int i_qp, int b_chroma, xavs_deblock_inter_t pf_inter )
{
    const int alpha = i_alpha_table[xavs_clip3(0,63,i_qp+h->sh.i_alpha_c0_offset)]; 
    const int beta  = i_beta_table[xavs_clip3(0,63,i_qp + h->sh.i_beta_offset)];
    int8_t tc = i_tc0_table[xavs_clip3(0,63,i_qp+h->sh.i_alpha_c0_offset)];

    if( !alpha || !beta )
        return;
    pf_inter(h, pix1, i_stride, alpha, beta, tc );
    if( b_chroma )
        pf_inter(h, pix2, i_stride, alpha, beta, tc );
}

static inline void deblock_edge_intra( xavs_t *h, uint8_t *pix1, uint8_t *pix2, int i_stride, uint8_t bS[4], int i_qp, int b_chroma, xavs_deblock_intra_t pf_intra )
{
	const int alpha = i_alpha_table[xavs_clip3(0,63,i_qp+h->sh.i_alpha_c0_offset)]; 
	const int beta  = i_beta_table[xavs_clip3(0,63,i_qp + h->sh.i_beta_offset)];

    if( !alpha || !beta )
        return;

    pf_intra(h, pix1, i_stride, alpha, beta);
    if( b_chroma )
        pf_intra(h, pix2, i_stride, alpha, beta );
}

void deblock_strength_xavs(xavs_t *h,int i_dir)
{
	int i = 0;
	/* *** Get bS for each 4px for the current edge *** */
	if( IS_INTRA( h->mb.type[h->xavs_loop_filter.mb_xy] ) || IS_INTRA( h->mb.type[h->xavs_loop_filter.mbn_xy]) )
		*(uint32_t*)h->xavs_loop_filter.bS = 0x02020202;
	else
	{
		*(uint32_t*)h->xavs_loop_filter.bS = 0x00000000;
		for( i = 0; i < 4; i++ )
		{
			int x  = i_dir == 0 ? h->xavs_loop_filter.i_edge : i;
			int y  = i_dir == 0 ? i      : h->xavs_loop_filter.i_edge;
			int xn = i_dir == 0 ? (x - 1)&0x03 : x;
			int yn = i_dir == 0 ? y : (y - 1)&0x03;
			if(!(h->xavs_loop_filter.i_edge&h->xavs_loop_filter.no_sub8x8))
			{
				{
					/* FIXME: A given frame may occupy more than one position in
					* the reference list. So we should compare the frame numbers,
					* not the indices in the ref list.
					* No harm yet, as we don't generate that case.*/
					int i8p= h->xavs_loop_filter.mb_8x8+(x>>1)+(y>>1)*h->xavs_loop_filter.s8x8;
					int i8q= h->xavs_loop_filter.mbn_8x8+(xn>>1)+(yn>>1)*h->xavs_loop_filter.s8x8;
					if(h->sh.i_type == SLICE_TYPE_P)
						h->xavs_loop_filter.bS[i] = (abs( h->mb.mv[0][i8p][0] - h->mb.mv[0][i8q][0] ) >= 4 ) ||
						(abs( h->mb.mv[0][i8p][1] - h->mb.mv[0][i8q][1] ) >= 4) ||
						(h->mb.ref[0][i8p] != h->mb.ref[0][i8q]);
					else
						h->xavs_loop_filter.bS[i] = (abs( h->mb.mv[0][i8p][0] - h->mb.mv[0][i8q][0] ) >= 4) ||
						(abs( h->mb.mv[0][i8p][1] - h->mb.mv[0][i8q][1] ) >= 4) ||
						(abs( h->mb.mv[1][i8p][0] - h->mb.mv[1][i8q][0] ) >= 4) ||
						(abs( h->mb.mv[1][i8p][1] - h->mb.mv[1][i8q][1] ) >= 4) || 
						(h->mb.ref[0][i8p] != h->mb.ref[0][i8q]) || 
						(h->mb.ref[1][i8p] != h->mb.ref[1][i8q]);

				}
			}
		}
	}
}
void filter_dir(xavs_t *h,int intra, int i_dir)
{
	/* Y plane */
	h->xavs_loop_filter.i_qpn= h->mb.qp[h->xavs_loop_filter.mbn_xy];
	if (intra)
	{
		if( i_dir == 0 )
		{
			/* vertical edge */
			deblock_edge_intra( h, h->xavs_loop_filter.pixy + 4*h->xavs_loop_filter.i_edge, NULL,
				h->xavs_loop_filter.stride2y, h->xavs_loop_filter.bS, (h->xavs_loop_filter.i_qp+h->xavs_loop_filter.i_qpn+1) >> 1, 0,
				intra == 1 ? h->loopf.deblock_h_luma_intra : h->loopf.deblock_h_luma);
			if( !(h->xavs_loop_filter.i_edge & 1) )
			{
				/* U/V planes */
				int i_qpc = (h->chroma_qp_table[h->xavs_loop_filter.i_qp] + h->chroma_qp_table[h->xavs_loop_filter.i_qpn] + 1) >> 1;
				deblock_edge_intra( h, h->xavs_loop_filter.pixu + 2*h->xavs_loop_filter.i_edge, h->xavs_loop_filter.pixv + 2*h->xavs_loop_filter.i_edge,
					h->xavs_loop_filter.stride2uv, h->xavs_loop_filter.bS, i_qpc, 1,
					intra == 1 ? h->loopf.deblock_h_chroma_intra : h->loopf.deblock_h_chroma  );
			}
		}
		else
		{
			/* horizontal edge */
			deblock_edge_intra( h, h->xavs_loop_filter.pixy + 4*h->xavs_loop_filter.i_edge*h->xavs_loop_filter.stride2y, NULL,
				h->xavs_loop_filter.stride2y, h->xavs_loop_filter.bS, (h->xavs_loop_filter.i_qp+h->xavs_loop_filter.i_qpn+1) >> 1, 0,
				intra == 1 ? h->loopf.deblock_v_luma_intra : h->loopf.deblock_v_luma);
			/* U/V planes */
			if( !(h->xavs_loop_filter.i_edge & 1) )
			{
				int i_qpc = (h->chroma_qp_table[h->xavs_loop_filter.i_qp] + h->chroma_qp_table[h->xavs_loop_filter.i_qpn] + 1) >> 1;
				deblock_edge_intra( h, h->xavs_loop_filter.pixu + 2*h->xavs_loop_filter.i_edge*h->xavs_loop_filter.stride2uv, h->xavs_loop_filter.pixv + 2*h->xavs_loop_filter.i_edge*h->xavs_loop_filter.stride2uv,
					h->xavs_loop_filter.stride2uv, h->xavs_loop_filter.bS, i_qpc, 1,
					intra == 1? h->loopf.deblock_v_chroma_intra : h->loopf.deblock_v_chroma );
			}
		}
	}
	else
	{
		if( i_dir == 0 )
		{
			/* vertical edge */
			deblock_edge( h, h->xavs_loop_filter.pixy + 4*h->xavs_loop_filter.i_edge, NULL,
				h->xavs_loop_filter.stride2y, h->xavs_loop_filter.bS, (h->xavs_loop_filter.i_qp+h->xavs_loop_filter.i_qpn+1) >> 1, 0,
				intra == 1 ? h->loopf.deblock_h_luma_intra : h->loopf.deblock_h_luma);
			if( !(h->xavs_loop_filter.i_edge & 1) )
			{
				/* U/V planes */
				int i_qpc = (h->chroma_qp_table[h->xavs_loop_filter.i_qp] + h->chroma_qp_table[h->xavs_loop_filter.i_qpn] + 1) >> 1;
				deblock_edge( h, h->xavs_loop_filter.pixu + 2*h->xavs_loop_filter.i_edge, h->xavs_loop_filter.pixv + 2*h->xavs_loop_filter.i_edge,
					h->xavs_loop_filter.stride2uv, h->xavs_loop_filter.bS, i_qpc, 1,
					intra == 1 ? h->loopf.deblock_h_chroma_intra : h->loopf.deblock_h_chroma  );
			}
		}
		else
		{
			/* horizontal edge */
			deblock_edge( h, h->xavs_loop_filter.pixy + 4*h->xavs_loop_filter.i_edge*h->xavs_loop_filter.stride2y, NULL,
				h->xavs_loop_filter.stride2y, h->xavs_loop_filter.bS, (h->xavs_loop_filter.i_qp+h->xavs_loop_filter.i_qpn+1) >> 1, 0,
				intra == 1 ? h->loopf.deblock_v_luma_intra : h->loopf.deblock_v_luma);
			/* U/V planes */
			if( !(h->xavs_loop_filter.i_edge & 1) )
			{
				int i_qpc = (h->chroma_qp_table[h->xavs_loop_filter.i_qp] + h->chroma_qp_table[h->xavs_loop_filter.i_qpn] + 1) >> 1;
				deblock_edge( h, h->xavs_loop_filter.pixu + 2*h->xavs_loop_filter.i_edge*h->xavs_loop_filter.stride2uv, h->xavs_loop_filter.pixv + 2*h->xavs_loop_filter.i_edge*h->xavs_loop_filter.stride2uv,
					h->xavs_loop_filter.stride2uv, h->xavs_loop_filter.bS, i_qpc, 1,
					intra == 1? h->loopf.deblock_v_chroma_intra : h->loopf.deblock_v_chroma );
			}
		}
	}
}
void deblock_dir(xavs_t *h,int i_dir,int mb_x,int mb_y)
{
			//DECLARE_ALIGNED_4( h->xavs_loop_filter.bS);  /* filtering strength */
	        h->xavs_loop_filter.i_edge = (i_dir ? (mb_y <= h->xavs_loop_filter.b_interlaced) : (mb_x == 0));
			if( h->xavs_loop_filter.i_edge )
				h->xavs_loop_filter.i_edge+= h->xavs_loop_filter.b_8x8_transform;
			else
			{
				h->xavs_loop_filter.mbn_xy  = i_dir == 0 ? h->xavs_loop_filter.mb_xy  - 1 : h->xavs_loop_filter.mb_xy - h->mb.i_mb_stride;
				h->xavs_loop_filter.mbn_8x8 = i_dir == 0 ? h->xavs_loop_filter.mb_8x8 - 2 : h->xavs_loop_filter.mb_8x8 - 2 * h->xavs_loop_filter.s8x8;
				h->xavs_loop_filter.mbn_4x4 = i_dir == 0 ? h->xavs_loop_filter.mb_4x4 - 4 : h->xavs_loop_filter.mb_4x4 - 4 * h->xavs_loop_filter.s4x4;
				if( h->xavs_loop_filter.b_interlaced && i_dir == 1 )
				{
					h->xavs_loop_filter.mbn_xy -= h->mb.i_mb_stride;
					h->xavs_loop_filter.mbn_8x8 -= 2 * h->xavs_loop_filter.s8x8;
					h->xavs_loop_filter.mbn_4x4 -= 4 * h->xavs_loop_filter.s4x4;
				}
				else if( IS_INTRA( h->mb.type[h->xavs_loop_filter.mb_xy] ) 
					|| IS_INTRA( h->mb.type[h->xavs_loop_filter.mbn_xy]) )
				{
					filter_dir( h,1, i_dir );
				}
				else
				{
					deblock_strength_xavs(h,i_dir);
					//if( *(uint32_t*)h->xavs_loop_filter.bS )
					if( h->xavs_loop_filter.bS[h->xavs_loop_filter.i_edge] )
						filter_dir(h,0 , i_dir);
				}
				h->xavs_loop_filter.i_edge += h->xavs_loop_filter.b_8x8_transform+1;
			}
			h->xavs_loop_filter.mbn_xy  = h->xavs_loop_filter.mb_xy;
			h->xavs_loop_filter.mbn_8x8 = h->xavs_loop_filter.mb_8x8;
			h->xavs_loop_filter.mbn_4x4 = h->xavs_loop_filter.mb_4x4;
			for( ; h->xavs_loop_filter.i_edge < h->xavs_loop_filter.i_edge_end; h->xavs_loop_filter.i_edge+=h->xavs_loop_filter.b_8x8_transform+1 )
			{
				deblock_strength_xavs(h,i_dir);
				//if( *(uint32_t*)h->xavs_loop_filter.bS )
				if( h->xavs_loop_filter.bS[h->xavs_loop_filter.i_edge] )
					filter_dir(h,0 , i_dir);
			}
}

void xavs_frame_deblock_row( xavs_t *h, int mb_y )
{
	int mb_x;
	h->xavs_loop_filter.s8x8 = 2 * h->mb.i_mb_stride;
	h->xavs_loop_filter.s4x4 = 4 * h->mb.i_mb_stride;
	h->xavs_loop_filter.b_interlaced = h->sh.b_mbaff;
	h->xavs_loop_filter.mvy_limit = 4 >> h->xavs_loop_filter.b_interlaced;
	h->xavs_loop_filter.qp_thresh = 15 - XAVS_MIN(h->sh.i_alpha_c0_offset, h->sh.i_beta_offset) - XAVS_MAX(0, h->param.analyse.i_chroma_qp_offset);
	h->xavs_loop_filter.no_sub8x8 = !(h->param.analyse.inter & XAVS_ANALYSE_PSUB8x8);
	h->xavs_loop_filter.stridey   = h->fdec->i_stride[0];
	h->xavs_loop_filter.stride2y  = h->xavs_loop_filter.stridey  << h->xavs_loop_filter.b_interlaced;
	h->xavs_loop_filter.strideuv  = h->fdec->i_stride[1];
	h->xavs_loop_filter.stride2uv = h->xavs_loop_filter.strideuv << h->xavs_loop_filter.b_interlaced;

	if( !h->pps->b_cabac && h->pps->b_transform_8x8_mode )
		munge_cavlc_nnz( h, mb_y, h->mb.nnz_backup, munge_cavlc_nnz_row );

	for( mb_x = 0; mb_x < h->sps->i_mb_width; mb_x += (~(h->xavs_loop_filter.b_interlaced) | mb_y)&1, mb_y ^= h->xavs_loop_filter.b_interlaced )
	{
		h->xavs_loop_filter.mb_xy  = mb_y * h->mb.i_mb_stride + mb_x;
		h->xavs_loop_filter.mb_8x8 = 2 * h->xavs_loop_filter.s8x8 * mb_y + 2 * mb_x;
		h->xavs_loop_filter.mb_4x4 = 4 * h->xavs_loop_filter.s4x4 * mb_y + 4 * mb_x;
		h->xavs_loop_filter.b_8x8_transform = h->mb.mb_transform_size[h->xavs_loop_filter.mb_xy];
		h->xavs_loop_filter.i_qp = h->mb.qp[h->xavs_loop_filter.mb_xy];
		h->xavs_loop_filter.i_edge_end = (h->mb.type[h->xavs_loop_filter.mb_xy] == P_SKIP) ? 1 : 4;
		h->xavs_loop_filter.pixy = h->fdec->plane[0] + 16*mb_y*h->xavs_loop_filter.stridey  + 16*mb_x;
		h->xavs_loop_filter.pixu = h->fdec->plane[1] +  8*mb_y*h->xavs_loop_filter.strideuv +  8*mb_x;
		h->xavs_loop_filter.pixv = h->fdec->plane[2] +  8*mb_y*h->xavs_loop_filter.strideuv +  8*mb_x;
		if( h->xavs_loop_filter.b_interlaced && (mb_y&1) )
		{
			h->xavs_loop_filter.pixy -= 15*h->xavs_loop_filter.stridey;
			h->xavs_loop_filter.pixu -=  7*h->xavs_loop_filter.strideuv;
			h->xavs_loop_filter.pixv -=  7*h->xavs_loop_filter.strideuv;
		}

		xavs_prefetch_fenc( h, h->fdec, mb_x, mb_y );

		if( h->xavs_loop_filter.i_qp <= h->xavs_loop_filter.qp_thresh )
			h->xavs_loop_filter.i_edge_end = 1;
		/* i_dir == 0 -> vertical edge
		* i_dir == 1 -> horizontal edge */

		deblock_dir(h,0,mb_x,mb_y);
		deblock_dir(h,1,mb_x,mb_y);
	}

	if( !h->pps->b_cabac && h->pps->b_transform_8x8_mode )
		munge_cavlc_nnz( h, mb_y, h->mb.nnz_backup, restore_cavlc_nnz_row );
}

void xavs_frame_deblock( xavs_t *h )
{
    int mb_y;
    for( mb_y = 0; mb_y < h->sps->i_mb_height; mb_y += 1 + h->sh.b_mbaff )
        xavs_frame_deblock_row( h, mb_y );
}

#ifdef HAVE_MMX
void xavs_deblock_v_chroma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void xavs_deblock_h_chroma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void xavs_deblock_v_chroma_intra_mmxext( uint8_t *pix, int stride, int alpha, int beta );
void xavs_deblock_h_chroma_intra_mmxext( uint8_t *pix, int stride, int alpha, int beta );

void xavs_deblock_v_luma_sse2( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void xavs_deblock_h_luma_sse2( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void xavs_deblock_v_luma_intra_sse2( uint8_t *pix, int stride, int alpha, int beta );
void xavs_deblock_h_luma_intra_sse2( uint8_t *pix, int stride, int alpha, int beta );
#ifdef ARCH_X86
void xavs_deblock_h_luma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void xavs_deblock_v8_luma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void xavs_deblock_h_luma_intra_mmxext( uint8_t *pix, int stride, int alpha, int beta );
void xavs_deblock_v8_luma_intra_mmxext( uint8_t *pix, int stride, int alpha, int beta );

static void xavs_deblock_v_luma_mmxext( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 )
{
    xavs_deblock_v8_luma_mmxext( pix,   stride, alpha, beta, tc0   );
    xavs_deblock_v8_luma_mmxext( pix+8, stride, alpha, beta, tc0+2 );
}
static void xavs_deblock_v_luma_intra_mmxext( uint8_t *pix, int stride, int alpha, int beta )
{
    xavs_deblock_v8_luma_intra_mmxext( pix,   stride, alpha, beta );
    xavs_deblock_v8_luma_intra_mmxext( pix+8, stride, alpha, beta );
}
#endif
#endif

#ifdef ARCH_PPC
void xavs_deblock_v_luma_altivec( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
void xavs_deblock_h_luma_altivec( uint8_t *pix, int stride, int alpha, int beta, int8_t *tc0 );
#endif // ARCH_PPC

void xavs_deblock_init( int cpu, xavs_deblock_function_t *pf )
{
    pf->deblock_v_luma = deblock_v_luma_c;
    pf->deblock_h_luma = deblock_h_luma_c;
    pf->deblock_v_chroma = deblock_v_chroma_c;
    pf->deblock_h_chroma = deblock_h_chroma_c;
    pf->deblock_v_luma_intra = deblock_v_luma_intra_c;
    pf->deblock_h_luma_intra = deblock_h_luma_intra_c;
    pf->deblock_v_chroma_intra = deblock_v_chroma_intra_c;
    pf->deblock_h_chroma_intra = deblock_h_chroma_intra_c;

#ifdef HAVE_MMX
    if( cpu&XAVS_CPU_MMXEXT )
    {
        pf->deblock_v_chroma = xavs_deblock_v_chroma_mmxext;
        pf->deblock_h_chroma = xavs_deblock_h_chroma_mmxext;
        pf->deblock_v_chroma_intra = xavs_deblock_v_chroma_intra_mmxext;
        pf->deblock_h_chroma_intra = xavs_deblock_h_chroma_intra_mmxext;
#ifdef ARCH_X86
        pf->deblock_v_luma = xavs_deblock_v_luma_mmxext;
        pf->deblock_h_luma = xavs_deblock_h_luma_mmxext;
        pf->deblock_v_luma_intra = xavs_deblock_v_luma_intra_mmxext;
        pf->deblock_h_luma_intra = xavs_deblock_h_luma_intra_mmxext;
#endif
        if( (cpu&XAVS_CPU_SSE2) && !(cpu&XAVS_CPU_STACK_MOD4) )
        {
            pf->deblock_v_luma = xavs_deblock_v_luma_sse2;
            pf->deblock_h_luma = xavs_deblock_h_luma_sse2;
            pf->deblock_v_luma_intra = xavs_deblock_v_luma_intra_sse2;
            pf->deblock_h_luma_intra = xavs_deblock_h_luma_intra_sse2;
        }
    }
#endif

#ifdef ARCH_PPC
    if( cpu&XAVS_CPU_ALTIVEC )
    {
        pf->deblock_v_luma = xavs_deblock_v_luma_altivec;
        pf->deblock_h_luma = xavs_deblock_h_luma_altivec;
   }
#endif // ARCH_PPC
}


/* threading */
void xavs_frame_cond_broadcast( xavs_frame_t *frame, int i_lines_completed )
{
    xavs_pthread_mutex_lock( &frame->mutex );
    frame->i_lines_completed = i_lines_completed;
    xavs_pthread_cond_broadcast( &frame->cv );
    xavs_pthread_mutex_unlock( &frame->mutex );
}

void xavs_frame_cond_wait( xavs_frame_t *frame, int i_lines_completed )
{
    xavs_pthread_mutex_lock( &frame->mutex );
    while( frame->i_lines_completed < i_lines_completed )
        xavs_pthread_cond_wait( &frame->cv, &frame->mutex );
    xavs_pthread_mutex_unlock( &frame->mutex );
}

/* list operators */

void xavs_frame_push( xavs_frame_t **list, xavs_frame_t *frame )
{
    int i = 0;
    while( list[i] ) i++;
    list[i] = frame;
}

xavs_frame_t *xavs_frame_pop( xavs_frame_t **list )
{
    xavs_frame_t *frame;
    int i = 0;
    assert( list[0] );
    while( list[i+1] ) i++;
    frame = list[i];
    list[i] = NULL;
    return frame;
}

void xavs_frame_unshift( xavs_frame_t **list, xavs_frame_t *frame )
{
    int i = 0;
    while( list[i] ) i++;
    while( i-- )
        list[i+1] = list[i];
    list[0] = frame;
}

xavs_frame_t *xavs_frame_shift( xavs_frame_t **list )
{
    xavs_frame_t *frame = list[0];
    int i;
    for( i = 0; list[i]; i++ )
        list[i] = list[i+1];
    assert(frame);
    return frame;
}

void xavs_frame_push_unused( xavs_t *h, xavs_frame_t *frame )
{
    assert( frame->i_reference_count > 0 );
    frame->i_reference_count--;
    if( frame->i_reference_count == 0 )
        xavs_frame_push( h->frames.unused, frame );
    assert( h->frames.unused[ sizeof(h->frames.unused) / sizeof(*h->frames.unused) - 1 ] == NULL );
}

xavs_frame_t *xavs_frame_pop_unused( xavs_t *h )
{
    xavs_frame_t *frame;
    if( h->frames.unused[0] )
        frame = xavs_frame_pop( h->frames.unused );
    else
        frame = xavs_frame_new( h );
    if( !frame )
        return NULL;
    frame->i_reference_count = 1;
    frame->b_intra_calculated = 0;
    return frame;
}

void xavs_frame_sort( xavs_frame_t **list, int b_dts )
{
    int i, b_ok;
    do {
        b_ok = 1;
        for( i = 0; list[i+1]; i++ )
        {
            int dtype = list[i]->i_type - list[i+1]->i_type;
            int dtime = list[i]->i_frame - list[i+1]->i_frame;
            int swap = b_dts ? dtype > 0 || ( dtype == 0 && dtime > 0 )
                             : dtime > 0;
            if( swap )
            {
                XCHG( xavs_frame_t*, list[i], list[i+1] );
                b_ok = 0;
            }
        }
    } while( !b_ok );
}
