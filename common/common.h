/*****************************************************************************
 * common.h: xavs encoder
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

#ifndef XAVS_COMMON_H
#define XAVS_COMMON_H

/****************************************************************************
 * Macros
 ****************************************************************************/
#define XAVS_MIN(a,b) ( (a)<(b) ? (a) : (b) )
#define XAVS_MAX(a,b) ( (a)>(b) ? (a) : (b) )
#define XAVS_MIN3(a,b,c) XAVS_MIN((a),XAVS_MIN((b),(c)))
#define XAVS_MAX3(a,b,c) XAVS_MAX((a),XAVS_MAX((b),(c)))
#define XAVS_MIN4(a,b,c,d) XAVS_MIN((a),XAVS_MIN3((b),(c),(d)))
#define XAVS_MAX4(a,b,c,d) XAVS_MAX((a),XAVS_MAX3((b),(c),(d)))
#define XCHG(type,a,b) do{ type t = a; a = b; b = t; } while(0)
#define FIX8(f) ((int)(f*(1<<8)+.5))

#define CHECKED_MALLOC( var, size )\
do {\
    var = xavs_malloc( size );\
    if( !var )\
        goto fail;\
} while( 0 )
#define CHECKED_MALLOCZERO( var, size )\
do {\
    CHECKED_MALLOC( var, size );\
    memset( var, 0, size );\
} while( 0 )

#define XAVS_BFRAME_MAX 16
#define XAVS_THREAD_MAX 128
#define XAVS_SLICE_MAX 4
#define XAVS_NAL_MAX (4 + XAVS_SLICE_MAX)
#define XAVS_PCM_COST (386*8)
#define XAVS_LOOKAHEAD_MAX 250

// number of pixels (per thread) in progress at any given time.
// 16 for the macroblock in progress + 3 for deblocking + 3 for motion compensation filter + 2 for extra safety
#define XAVS_THREAD_HEIGHT 24

#define TRACE_TB 0
/****************************************************************************
 * Includes
 ****************************************************************************/
#include "osdep.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "xavs.h"
#include "bs.h"
#include "set.h"
#include "predict.h"
#include "pixel.h"
#include "mc.h"
#include "frame.h"
#include "dct.h"
#include "quant.h"
#include "cabac.h"
#include "vlc.h"

/****************************************************************************
 * Generals functions
 ****************************************************************************/
/* xavs_malloc : will do or emulate a memalign
 * you have to use xavs_free for buffers allocated with xavs_malloc */
void *xavs_malloc( int );
void  xavs_free( void * );

/* xavs_slurp_file: malloc space for the whole file and read it */
char *xavs_slurp_file( const char *filename );

/* mdate: return the current date in microsecond */
int64_t xavs_mdate( void );

/* xavs_param2string: return a (malloced) string containing most of
 * the encoding options */
char *xavs_param2string( xavs_param_t *p, int b_res );

/* log */
void xavs_log( xavs_t *h, int i_level, const char *psz_fmt, ... );

void xavs_reduce_fraction( int *n, int *d );
void xavs_init_vlc_tables();

static inline uint8_t xavs_clip_uint8( int x )
{
    return x&(~255) ? (-x)>>31 : x;
}

static inline int xavs_clip3( int v, int i_min, int i_max )
{
    return ( (v < i_min) ? i_min : (v > i_max) ? i_max : v );
}

static inline double xavs_clip3f( double v, double f_min, double f_max )
{
    return ( (v < f_min) ? f_min : (v > f_max) ? f_max : v );
}

static inline int xavs_median( int a, int b, int c )
{
    int t = (a-b)&((a-b)>>31);
    a -= t;
    b += t;
    b -= (b-c)&((b-c)>>31);
    b += (a-b)&((a-b)>>31);
    return b;
}

static inline void xavs_median_mv( int16_t *dst, int16_t *a, int16_t *b, int16_t *c )
{
    dst[0] = xavs_median( a[0], b[0], c[0] );
    dst[1] = xavs_median( a[1], b[1], c[1] );
}

static inline int xavs_predictor_difference( int16_t (*mvc)[2], intptr_t i_mvc )
{
    int sum = 0, i;
    for( i = 0; i < i_mvc-1; i++ )
    {
        sum += abs( mvc[i][0] - mvc[i+1][0] )
             + abs( mvc[i][1] - mvc[i+1][1] );
    }
    return sum;
}

static inline uint32_t xavs_cabac_amvd_sum( int16_t *mvdleft, int16_t *mvdtop )
{
    int amvd0 = abs(mvdleft[0]) + abs(mvdtop[0]);
    int amvd1 = abs(mvdleft[1]) + abs(mvdtop[1]);
    amvd0 = (amvd0 > 2) + (amvd0 > 32);
    amvd1 = (amvd1 > 2) + (amvd1 > 32);
    return amvd0 + (amvd1<<16);
}

static const uint8_t exp2_lut[64] = {
      1,   4,   7,  10,  13,  16,  19,  22,  25,  28,  31,  34,  37,  40,  44,  47,
     50,  53,  57,  60,  64,  67,  71,  74,  78,  81,  85,  89,  93,  96, 100, 104,
    108, 112, 116, 120, 124, 128, 132, 137, 141, 145, 150, 154, 159, 163, 168, 172,
    177, 182, 186, 191, 196, 201, 206, 211, 216, 221, 226, 232, 237, 242, 248, 253,
};

static ALWAYS_INLINE int xavs_exp2fix8( float x )
{
    int i, f;
    x += 8;
    if( x <= 0 ) return 0;
    if( x >= 16 ) return 0xffff;
    i = x;
    f = (x-i)*64;
    return (exp2_lut[f]+256) << i >> 8;
}

static const double log2_lut[128] = {
    0.00000, 0.01123, 0.02237, 0.03342, 0.04439, 0.05528, 0.06609, 0.07682,
    0.08746, 0.09803, 0.10852, 0.11894, 0.12928, 0.13955, 0.14975, 0.15987,
    0.16993, 0.17991, 0.18982, 0.19967, 0.20945, 0.21917, 0.22882, 0.23840,
    0.24793, 0.25739, 0.26679, 0.27612, 0.28540, 0.29462, 0.30378, 0.31288,
    0.32193, 0.33092, 0.33985, 0.34873, 0.35755, 0.36632, 0.37504, 0.38370,
    0.39232, 0.40088, 0.40939, 0.41785, 0.42626, 0.43463, 0.44294, 0.45121,
    0.45943, 0.46761, 0.47573, 0.48382, 0.49185, 0.49985, 0.50779, 0.51570,
    0.52356, 0.53138, 0.53916, 0.54689, 0.55459, 0.56224, 0.56986, 0.57743,
    0.58496, 0.59246, 0.59991, 0.60733, 0.61471, 0.62205, 0.62936, 0.63662,
    0.64386, 0.65105, 0.65821, 0.66534, 0.67243, 0.67948, 0.68650, 0.69349,
    0.70044, 0.70736, 0.71425, 0.72110, 0.72792, 0.73471, 0.74147, 0.74819,
    0.75489, 0.76155, 0.76818, 0.77479, 0.78136, 0.78790, 0.79442, 0.80090,
    0.80735, 0.81378, 0.82018, 0.82655, 0.83289, 0.83920, 0.84549, 0.85175,
    0.85798, 0.86419, 0.87036, 0.87652, 0.88264, 0.88874, 0.89482, 0.90087,
    0.90689, 0.91289, 0.91886, 0.92481, 0.93074, 0.93664, 0.94251, 0.94837,
    0.95420, 0.96000, 0.96578, 0.97154, 0.97728, 0.98299, 0.98868, 0.99435
};

static ALWAYS_INLINE float xavs_log2( uint32_t x )
{
    int lz = xavs_clz( x );
    return log2_lut[(x<<lz>>24)&0x7f] + (31 - lz);
}

/****************************************************************************
 *
 ****************************************************************************/
enum slice_type_e
{
    SLICE_TYPE_P  = 0,
    SLICE_TYPE_B  = 1,
    SLICE_TYPE_I  = 2,
    SLICE_TYPE_SP = 3,
    SLICE_TYPE_SI = 4
};

static const char slice_type_to_char[] = { 'P', 'B', 'I', 'S', 'S' };

typedef struct
{

    int i_type;
    int i_first_mb;
    int i_last_mb;

    int i_pps_id;

    int i_frame_num;

    int b_mbaff;
    int b_field_pic;
    int b_bottom_field;

    int i_idr_pic_id;   /* -1 if nal_type != 5 */

    int i_poc_lsb;
    int i_delta_poc_bottom;

    int i_delta_poc[2];
    int i_redundant_pic_cnt;

    int b_direct_spatial_mv_pred;

    int b_num_ref_idx_override;
    int i_num_ref_idx_l0_active;
    int i_num_ref_idx_l1_active;

    int b_ref_pic_list_reordering_l0;
    int b_ref_pic_list_reordering_l1;
    struct {
        int idc;
        int arg;
    } ref_pic_list_order[2][16];

    int i_cabac_init_idc;

    int i_qp;
    int i_qp_delta;
    int b_sp_for_swidth;
    int i_qs_delta;

    /* deblocking filter */
    int i_disable_deblocking_filter_idc;
    int i_alpha_c0_offset;
    int i_beta_offset;

	int i_slice_start_code;
	int i_slice_vertical_position;
	int i_slice_vertical_position_extension;
	int b_fixed_slice_qp;
	int i_slice_qp;
	int b_slice_weighting_flag;
	int i_luma_scale[4];
	int i_luma_shift[4];
	int i_chroma_scale[4];
	int i_chroma_shift[4];
	int b_mb_weighting_flag;

	// 
 	int b_picture_fixed_qp;	

} xavs_slice_header_t;


#define XAVS_SCAN8_SIZE (6*8)
#define XAVS_SCAN8_0 (4+1*8)

static const int xavs_scan8[16+2*4+3] =
{
    /* Luma */
    4+1*8, 5+1*8, 4+2*8, 5+2*8,
    6+1*8, 7+1*8, 6+2*8, 7+2*8,
    4+3*8, 5+3*8, 4+4*8, 5+4*8,
    6+3*8, 7+3*8, 6+4*8, 7+4*8,

    /* Cb */
    1+1*8, 2+1*8,
    1+2*8, 2+2*8,

    /* Cr */
    1+4*8, 2+4*8,
    1+5*8, 2+5*8,

    /* Luma DC */
    4+5*8,

    /* Chroma DC */
    5+5*8, 6+5*8
};
/*
   0 1 2 3 4 5 6 7
 0
 1   B B   L L L L
 2   B B   L L L L
 3         L L L L
 4   R R   L L L L
 5   R R   DyDuDv
*/

typedef struct xavs_ratecontrol_t   xavs_ratecontrol_t;

typedef struct xavs_loopfilter_t
{
	int s8x8;
	int s4x4;
	int b_interlaced;
	int mvy_limit;
	int qp_thresh;
	int no_sub8x8;

	int stridey;
	int stride2y;
	int strideuv;
	int stride2uv;

	int mb_xy;
	int mb_8x8;
	int mb_4x4;
	int b_8x8_transform;
	int i_qp;

	int i_edge_end;
	uint8_t *pixy;
	uint8_t *pixu;
	uint8_t *pixv;
	int i_qpn;
	int mbn_xy;
	int mbn_8x8;
	int mbn_4x4;

	uint8_t bS[4];
	int i_edge;
}xavs_loopfilter_t;

typedef struct
{
    int i_video_sequence_start_code;

    int i_profile_idc;
    int i_level_idc;

    int b_progressive_sequence;

    int i_horizontal_size;
    int i_vertical_size;
    int i_chroma_format;
    int i_sample_precision;
    int i_aspect_ratio;
    int i_frame_rate_code;
    int i_bit_rate_lower;
    int i_bit_rate_upper;
    int b_low_delay;
    int i_bbv_buffer_size;

	int i_mb_width;
	int i_mb_height;
}xavs_seq_header_t;

typedef struct
{
	int i_i_picture_start_code;
	int i_bbv_delay;
	int b_time_code_flag;
	int i_time_code;
	int i_picture_distance;
	int i_bbv_check_times;
	int b_progressive_frame;
	int b_picture_structure;
	int b_top_field_first;
	int b_repeat_first_field;
	int b_fixed_picture_qp;
	int i_picture_qp;
	int b_skip_mode_flag;
	int i_reserved_bits;
	int b_loop_filter_disable;
	int b_loop_filter_parameter_flag;
	int i_alpha_c_offset;
	int i_beta_offset;
} xavs_i_pic_header_t;

typedef struct
{
	int i_pb_picture_start_code;
	int i_bbv_delay;
	int i_picture_coding_type;
	int i_picture_distance;
	int i_bbv_check_times;
	int b_progressive_frame;
	int b_picture_structure;
	int b_advanced_pred_mode_disable;
	int b_top_field_first;
	int b_repeat_first_field;
	int b_fixed_picture_qp;
	int i_picture_qp;
	int b_picture_reference_flag;
	int b_no_forward_reference_flag;
	int b_skip_mode_flag;
	int b_loop_filter_disable;
	int b_loop_filter_parameter_flag;
	int i_alpha_c_offset;
	int i_beta_offset;
}xavs_pb_pic_header_t;

typedef struct
{
	int i_slice_start_code;
	int i_slice_vertical_position;
	int i_slice_vertical_position_extension;
	int b_fixed_slice_qp;
	int i_slice_qp;
	int b_slice_weighting_flag;
	int i_luma_scale[4];
	int i_luma_shift[4];
	int i_chroma_scale[4];
	int i_chroma_shift[4];
	int b_mb_weighting_flag;

	//
	int i_type;//slice type
	int i_first_mb;
    int i_last_mb;
    int i_frame_num;
    int i_qp;
    int i_disable_deblocking_filter_idc;
    int i_alpha_c0_offset;
    int i_beta_offset;
	int i_num_ref_idx_l0_active;
	int i_num_ref_idx_l1_active;
	int b_picture_fixed_qp;
}changhong_xavs_slice_header_t;

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
    float   f_qp_avg;

    /* YUV buffer */
    int     i_plane;
    int     i_stride[4];
    int     i_lines[4];
    int     i_stride_lowres;
    int     i_lines_lowres;
    uint8_t *plane[4];
    uint8_t *filtered[4]; /* plane[0], H, V, HV */
    uint8_t *lowres[4]; /* half-size copy of input frame: Orig, H, V, HV */
    uint16_t *integral;

    /* for unrestricted mv we allocate more data than needed
     * allocated data are stored in buffer */
    void    *buffer[8];
    void    *buffer_lowres[4];

    /* motion data */
    int8_t  *mb_type;
    int16_t (*mv[2])[2];
    int8_t  *ref[2];
    int     i_ref[2];
    int     ref_poc[2][16];

    /* for adaptive B-frame decision.
     * contains the SATD cost of the lowres frame encoded in various modes
     * FIXME: how big an array do we need? */
    int     i_cost_est[XAVS_BFRAME_MAX+2][XAVS_BFRAME_MAX+2];
    int     i_satd; // the i_cost_est of the selected frametype
    int     i_intra_mbs[XAVS_BFRAME_MAX+2];
    int     *i_row_satds[XAVS_BFRAME_MAX+2][XAVS_BFRAME_MAX+2];
    int     *i_row_satd;
    int     *i_row_bits;
    int     *i_row_qp;

    /* threading */
    int     i_lines_completed; /* in pixels */
    int     i_reference_count; /* number of threads using this frame (not necessarily the number of pointers) */
    xavs_pthread_mutex_t mutex;      
    xavs_pthread_cond_t  cv;

} changhong_xavs_frame_t;


struct xavs_t
{
    /* encoder parameters */
    xavs_param_t    param;
#if TRACE_TB
	FILE *ptrace; //yangping 
	#define             TRACESTRING_SIZE 100            //!< size of trace string
	char                tracestring[TRACESTRING_SIZE];  //!< trace string
#endif

    xavs_t          *thread[XAVS_THREAD_MAX];
    xavs_pthread_t  thread_handle;
    int             b_thread_active;
    int             i_thread_phase; /* which thread to use for the next frame */

    /* bitstream output */
    struct
    {
        int         i_bitstream;    /* size of p_bitstream */
        uint8_t     *p_bitstream;   /* will hold data for all nal */
        bs_t        bs;
        int         i_frame_size;
    } out;

    /**** thread synchronization starts here ****/
    /* frame number/poc */
    int             i_frame;

    int             i_frame_offset; /* decoding only */
    int             i_frame_num;    /* decoding only */
    int             i_poc_msb;      /* decoding only */
    int             i_poc_lsb;      /* decoding only */
    int             i_poc;          /* decoding only */

    int             i_thread_num;   /* threads only */
    int             i_nal_type;     /* threads only */
    int             i_nal_ref_idc;  /* threads only */

    /* We use only one SPS and one PPS */
    xavs_sps_t      sps_array[1];
    xavs_sps_t      *sps;
    xavs_pps_t      pps_array[1];
    xavs_pps_t      *pps;
    int             i_idr_pic_id;

    /* quantization matrix for decoding, [cqm][qp][coef_y][coef_x] */
    int             dequant8_mf[4][64][8][8]; /* [2][64][8][8] */
	
    /* quantization matrix for trellis, [cqm][qp][coef_y][coef_x] */
    int             unquant8_mf[4][64][8][8];   /* [2][64][64] */

    /* quantization matrix for encoding */
    uint16_t        quant8_mf[4][64][64];     /* [2][64][64] */

    /* quantization matrix for encoding deadzone */
    uint16_t        quant8_bias[4][64][64];   /* [2][64][64] */

	/* quantization matrix for decoding, [cqm][qp%6][coef_y][coef_x] */
    int             (*dequant4_mf[4])[4][4]; /* [4][6][4][4] */
    /* quantization matrix for trellis, [cqm][qp][coef] */
    int             (*unquant4_mf[4])[16];   /* [4][52][16] */
    /* quantization matrix for deadzone */
    uint16_t        (*quant4_mf[4])[16];     /* [4][52][16] */
    uint16_t        (*quant4_bias[4])[16];   /* [4][52][16] */

    const uint8_t   *chroma_qp_table; /* includes both the nonlinear luma->chroma mapping and chroma_qp_offset */

    DECLARE_ALIGNED_16( uint32_t nr_residual_sum[2][64] );
    DECLARE_ALIGNED_16( uint16_t nr_offset[2][64] );
    uint32_t        nr_count[2];

    /* Slice header */
    xavs_slice_header_t sh;

	/* cabac context */
    xavs_cabac_t  cabac;

    struct
    {
        /* Frames to be encoded (whose types have been decided) */
        xavs_frame_t *current[XAVS_LOOKAHEAD_MAX+3];
        /* Temporary buffer (frames types not yet decided) */
        xavs_frame_t *next[XAVS_LOOKAHEAD_MAX+3];
        /* Unused frames */
        xavs_frame_t *unused[XAVS_LOOKAHEAD_MAX + XAVS_THREAD_MAX*2 + 16+4];
        /* For adaptive B decision */
        xavs_frame_t *last_nonb;

        /* frames used for reference + sentinels */
        xavs_frame_t *reference[16+2];

        int i_last_idr; /* Frame number of the last IDR */

        int i_input;    /* Number of input frames already accepted */

        int i_max_dpb;  /* Number of frames allocated in the decoded picture buffer */
        int i_max_ref0;
        int i_max_ref1;
        int i_delay;    /* Number of frames buffered for B reordering */
        int b_have_lowres;  /* Whether 1/2 resolution luma planes are being used */
        int b_have_sub8x8_esa;
    } frames;

    /* current frame being encoded */
    xavs_frame_t    *fenc;

    /* frame being reconstructed */
    xavs_frame_t    *fdec;

    /* references lists */
    int             i_ref0;
    xavs_frame_t    *fref0[16+3];     /* ref list 0 */
    int             i_ref1;
    xavs_frame_t    *fref1[16+3];     /* ref list 1 */
    int             b_ref_reorder[2];



    /* Current MB DCT coeffs */
    struct
    {
        DECLARE_ALIGNED_16( int16_t luma16x16_dc[16] );
        DECLARE_ALIGNED_16( int16_t chroma[64] );
        DECLARE_ALIGNED_16( int16_t chroma_dc[2][4] );
        // FIXME share memory?
        DECLARE_ALIGNED_16( int16_t luma8x8[4][64] );
        DECLARE_ALIGNED_16( int16_t luma4x4[16+8][16] );

		DECLARE_ALIGNED_16( int16_t chroma8x8[2][64] );
    } dct;

    /* MB table and cache for current frame/mb */
    struct
    {
        int     i_mb_count;                 /* number of mbs in a frame */

        /* Strides */
        int     i_mb_stride;
        int     i_b8_stride;
        int     i_b4_stride;

        /* Current index */
        int     i_mb_x;
        int     i_mb_y;
        int     i_mb_xy;
        int     i_b8_xy;
        int     i_b4_xy;

        /* Search parameters */
        int     i_me_method;
        int     i_subpel_refine;
        int     b_chroma_me;
        int     b_trellis;
        int     b_noise_reduction;
        int     i_psy_rd; /* Psy RD strength--fixed point value*/
        int     i_psy_trellis; /* Psy trellis strength--fixed point value*/

        int     b_interlaced;

        /* Allowed qpel MV range to stay within the picture + emulated edge pixels */
        int     mv_min[2];
        int     mv_max[2];
        /* Subpel MV range for motion search.
         * same mv_min/max but includes levels' i_mv_range. */
        int     mv_min_spel[2];
        int     mv_max_spel[2];
        /* Fullpel MV range for motion search */
        int     mv_min_fpel[2];
        int     mv_max_fpel[2];

        /* neighboring MBs */
        unsigned int i_neighbour;
        unsigned int i_neighbour8[4];       /* neighbours of each 8x8 or 4x4 block that are available */
        unsigned int i_neighbour4[16];      /* at the time the block is coded */
        int     i_mb_type_top;
        int     i_mb_type_left;
        int     i_mb_type_topleft;
        int     i_mb_type_topright;
        int     i_mb_prev_xy;
        int     i_mb_top_xy;

        /**** thread synchronization ends here ****/
        /* subsequent variables are either thread-local or constant,
         * and won't be copied from one thread to another */

        /* mb table */
        int8_t  *type;                      /* mb type */
        int8_t  *qp;                        /* mb qp */
        int16_t *cbp;                       /* mb cbp: 0x0?: luma, 0x?0: chroma, 0x100: luma dc, 0x0200 and 0x0400: chroma dc  (all set for PCM)*/
        int8_t  (*intra4x4_pred_mode)[8];   /* intra4x4 pred mode. for non I4x4 set to I_PRED_4x4_DC(2) */
                                            /* actually has only 7 entries; set to 8 for write-combining optimizations */
        uint8_t (*non_zero_count)[16+4+4];  /* nzc. for I_PCM set to 16 */
        int8_t  *chroma_pred_mode;          /* chroma_pred_mode. cabac only. for non intra I_PRED_CHROMA_DC(0) */
        int16_t (*mv[2])[2];                /* mb mv. set to 0 for intra mb */
        int16_t (*mvd[2])[2];               /* mb mv difference with predict. set to 0 if intra. cabac only */
        int8_t   *ref[2];                   /* mb ref. set to -1 if non used (intra or Lx only) */
        int16_t (*mvr[2][32])[2];           /* 16x16 mv for each possible ref */
        int8_t  *skipbp;                    /* block pattern for SKIP or DIRECT (sub)mbs. B-frames + cabac only */
        int8_t  *mb_transform_size;         /* transform_size_8x8_flag of each mb */
        uint8_t *intra_border_backup[2][3]; /* bottom pixels of the previous mb row, used for intra prediction after the framebuffer has been deblocked */
        uint8_t (*nnz_backup)[16];          /* when using cavlc + 8x8dct, the deblocker uses a modified nnz */

        /* current value */
        int     i_type;
        int     i_partition;
        DECLARE_ALIGNED_4( uint8_t i_sub_partition[4] );
        int     b_transform_8x8;

        int     i_cbp_luma;
        int     i_cbp_chroma;

        int     i_intra16x16_pred_mode;
        int     i_chroma_pred_mode;

	    int                 b8mode[4];
		int                 b8pdir[4];
		int		i_current_mb_num;
		int code_counter;
		int intra_pred_modes[4];
		int c_ipred_mode;
 

        /* skip flags for i4x4 and i8x8
         * 0 = encode as normal.
         * 1 (non-RD only) = the DCT is still in h->dct, restore fdec and skip reconstruction.
         * 2 (RD only) = the DCT has since been overwritten by RD; restore that too. */
        int i_skip_intra;
        /* skip flag for motion compensation */
        /* if we've already done MC, we don't need to do it again */
        int b_skip_mc;

        struct
        {
            /* space for p_fenc and p_fdec */
#define FENC_STRIDE 16
#define FDEC_STRIDE 32
            DECLARE_ALIGNED_16( uint8_t fenc_buf[24*FENC_STRIDE] );
            DECLARE_ALIGNED_16( uint8_t fdec_buf[27*FDEC_STRIDE] );

            /* i4x4 and i8x8 backup data, for skipping the encode stage when possible */
            DECLARE_ALIGNED_16( uint8_t i4x4_fdec_buf[16*16] );
            DECLARE_ALIGNED_16( uint8_t i8x8_fdec_buf[16*16] );
            DECLARE_ALIGNED_16( int16_t i8x8_dct_buf[3][64] );
            DECLARE_ALIGNED_16( int16_t i4x4_dct_buf[15][16] );
            uint32_t i4x4_nnz_buf[4];
            uint32_t i8x8_nnz_buf[4];
            int i4x4_cbp;
            int i8x8_cbp;

            /* Psy trellis DCT data */
            DECLARE_ALIGNED_16( int16_t fenc_dct8[4][64] );
            DECLARE_ALIGNED_16( int16_t fenc_dct4[16][16] );

            /* Psy RD SATD scores */
            int fenc_satd[4][4];
            int fenc_satd_sum;
            int fenc_sa8d[2][2];
            int fenc_sa8d_sum;

            /* pointer over mb of the frame to be compressed */
            uint8_t *p_fenc[3];
            /* pointer to the actual source frame, not a block copy */
            uint8_t *p_fenc_plane[3];

            /* pointer over mb of the frame to be reconstructed  */
            uint8_t *p_fdec[3];

            /* pointer over mb of the references */
            int i_fref[2];
            uint8_t *p_fref[2][32][4+2]; /* last: lN, lH, lV, lHV, cU, cV */
            uint16_t *p_integral[2][16];

            /* fref stride */
            int     i_stride[3];
        } pic;

        /* cache */
        struct
        {
            /* real intra4x4_pred_mode if I_4X4 or I_8X8, I_PRED_4x4_DC if mb available, -1 if not */
            int8_t  intra4x4_pred_mode[XAVS_SCAN8_SIZE];
			int8_t  intra8x8_pred_mode[XAVS_SCAN8_SIZE];
            /* i_non_zero_count if available else 0x80 */
            uint8_t non_zero_count[XAVS_SCAN8_SIZE];

            /* -1 if unused, -2 if unavailable */
            DECLARE_ALIGNED_4( int8_t ref[2][XAVS_SCAN8_SIZE] );

            /* 0 if not available */
            DECLARE_ALIGNED_16( int16_t mv[2][XAVS_SCAN8_SIZE][2] );
            DECLARE_ALIGNED_8( int16_t mvd[2][XAVS_SCAN8_SIZE][2] );

            /* 1 if SKIP or DIRECT. set only for B-frames + CABAC */
            DECLARE_ALIGNED_4( int8_t skip[XAVS_SCAN8_SIZE] );

            DECLARE_ALIGNED_16( int16_t direct_mv[2][XAVS_SCAN8_SIZE][2] );
            DECLARE_ALIGNED_4( int8_t  direct_ref[2][XAVS_SCAN8_SIZE] );
            DECLARE_ALIGNED_4( int16_t pskip_mv[2] );

            /* number of neighbors (top and left) that used 8x8 dct */
            int     i_neighbour_transform_size;
            int     i_neighbour_interlaced;

            /* neighbor CBPs */
            int     i_cbp_top;
            int     i_cbp_left;
        } cache;

        /* */
        int     i_qp;       /* current qp */
        int     i_chroma_qp;
        int     i_last_qp;  /* last qp */
        int     i_last_dqp; /* last delta qp */
        int     b_variable_qp; /* whether qp is allowed to vary per macroblock */
        int     b_lossless;
        int     b_direct_auto_read; /* take stats for --direct auto from the 2pass log */
        int     b_direct_auto_write; /* analyse direct modes, to use and/or save */

        /* lambda values */
        int     i_trellis_lambda2[2][2]; /* [luma,chroma][inter,intra] */
        int     i_psy_rd_lambda;
        int     i_chroma_lambda2_offset;

        /* B_direct and weighted prediction */
        int16_t dist_scale_factor[16][2];
        int16_t bipred_weight[32][4];
        /* maps fref1[0]'s ref indices into the current list0 */
        int8_t  map_col_to_list0_buf[2]; // for negative indices
        int8_t  map_col_to_list0[16];
    } mb;

    /* rate control encoding only */
    xavs_ratecontrol_t *rc;

	/* xavs_loop_filter */
	xavs_loopfilter_t xavs_loop_filter; //jiessie

    /* stats */
    struct
    {
        /* Current frame stats */
        struct
        {
            /* MV bits (MV+Ref+Block Type) */
            int i_mv_bits;
            /* Texture bits (DCT coefs) */
            int i_tex_bits;
            /* ? */
            int i_misc_bits;
            /* MB type counts */
            int i_mb_count[19];
            int i_mb_count_i;
            int i_mb_count_p;
            int i_mb_count_skip;
            int i_mb_count_8x8dct[2];
            int i_mb_count_ref[2][32];
            int i_mb_partition[17];
            int i_mb_cbp[6];
            /* Adaptive direct mv pred */
            int i_direct_score[2];
            /* Metrics */
            int64_t i_ssd[3];
            double f_ssim;
        } frame;

        /* Cumulated stats */

        /* per slice info */
        int     i_slice_count[5];
        int64_t i_slice_size[5];
        double  f_slice_qp[5];
        int     i_consecutive_bframes[XAVS_BFRAME_MAX+1];
        /* */
        int64_t i_ssd_global[5];
        double  f_psnr_average[5];
        double  f_psnr_mean_y[5];
        double  f_psnr_mean_u[5];
        double  f_psnr_mean_v[5];
        double  f_ssim_mean_y[5];
        /* */
        int64_t i_mb_count[5][19];
        int64_t i_mb_partition[2][17];
        int64_t i_mb_count_8x8dct[2];
        int64_t i_mb_count_ref[2][2][32];
        int64_t i_mb_cbp[6];
        /* */
        int     i_direct_score[2];
        int     i_direct_frames[2];

    } stat;

    void *scratch_buffer; /* for any temporary storage that doesn't want repeated malloc */

    /* CPU functions dependents */
    xavs_predict_t      predict_16x16[4+3];
    xavs_predict8x8_t      predict_8x8c[4+3];
    xavs_predict8x8_t   predict_8x8[9+3];
    xavs_predict_t      predict_4x4[9+3];
    xavs_predict_8x8_filter_t predict_8x8c_filter;
    xavs_predict_8x8_filter_t predict_8x8_filter;

    xavs_pixel_function_t pixf;
    xavs_mc_functions_t   mc;
    xavs_dct_function_t   dctf;
    xavs_zigzag_function_t zigzagf;
    xavs_quant_function_t quantf;
    xavs_deblock_function_t loopf;

#if VISUALIZE
    struct visualize_t *visualize;
#endif

    DECLARE_ALIGNED_16( uint8_t edgeCb[60] );	
	DECLARE_ALIGNED_16( uint8_t edgeCr[60] );	

	changhong_xavs_frame_t  b;
	changhong_xavs_slice_header_t a; 
	xavs_seq_header_t  sqh;
	xavs_i_pic_header_t ih;
	xavs_pb_pic_header_t pbh;

};

// included at the end because it needs xavs_t
#include "macroblock.h"

#ifdef HAVE_MMX
#include "x86/util.h"
#endif

#endif

