/*****************************************************************************
 * set.c: xavs encoder library
 *****************************************************************************
 * Copyright (C) 2009 xavs project
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

#define SHIFT(x,s) ((s)<0 ? (x)<<-(s) : (s)==0 ? (x) : ((x)+(1<<((s)-1)))>>(s))
#define DIV(n,d) (((n) + ((d)>>1)) / (d))


static const int quant8_table[64] =
{
   	    32768,29775,27554,25268,23170,21247,19369,17770,
		16302,15024,13777,12634,11626,10624,9742,8958,
		8192,7512,6889,6305,5793,5303,4878,4467,
		4091,3756,3444,3161,2894,2654,2435,2235,
		2048,1878,1722,1579,1449,1329,1218,1117,
		1024,939,861,790,724,664,609,558,
		512,470,430,395,362,332,304,279,
		256,235,215,197,181,166,152,140
};

int quant8_scale[64] ={
	  32768,37958,36158,37958,32768,37958,36158,37958,
	  37958,43969,41884,43969,37958,43969,41884,43969,
	  36158,41884,39898,41884,36158,41884,39898,41884,
	  37958,43969,41884,43969,37958,43969,41884,43969,
      32768,37958,36158,37958,32768,37958,36158,37958,
	  37958,43969,41884,43969,37958,43969,41884,43969,
	  36158,41884,39898,41884,36158,41884,39898,41884,
	  37958,43969,41884,43969,37958,43969,41884,43969
  };

static const int dequant8_table[64] =
{
 		32768,36061,38968,42495,46341,50535,55437,60424,
		32932,35734,38968,42495,46177,50535,55109,59933,
		65535,35734,38968,42577,46341,50617,55027,60097,
		32809,35734,38968,42454,46382,50576,55109,60056,
		65535,35734,38968,42495,46320,50515,55109,60076,
		65535,35744,38968,42495,46341,50535,55099,60087,
		65535,35734,38973,42500,46341,50535,55109,60097,
		32771,35734,38965,42497,46341,50535,55109,60099
};


int xavs_cqm_init( xavs_t *h )
{
    int def_quant8[64][64];
    int def_dequant8[64][64];
    int q, i, j, i_list;
	int pos; 
    int deadzone[4] = { 32 - h->param.analyse.i_luma_deadzone[1],
                        32 - h->param.analyse.i_luma_deadzone[0],
                        32 - 11, 32 - 21 };
    int max_qp_err = -1;

	//initial the dequant and quant table with q for 8x8 
	for( q = 0; q < 64; q++ )
	{
		for( i = 0; i < 64; i++ )
        {
           def_dequant8[q][i] = dequant8_table[q];
           def_quant8[q][i]   = (quant8_scale[i] * quant8_table[q])>>19;
		}
	}

	//i_list =0 Intra Y ; i_list = 1 inter Y ; i_list =2 Intra C ; i_list =3 Inter C;
    for( i_list = 0; i_list < 4; i_list++ )
	{
	   for( q = 0; q < 64; q++ )
       {
            for( pos = 0; pos < 64; pos++ )
            {
				int xx = pos%8; 
			    int yy = pos/8; 
				int mf; 
        		h->dequant8_mf[i_list][q][yy][xx] = DIV(def_dequant8[q][pos] * 16, h->pps->scaling_list[i_list][pos]);
				h->quant8_mf[i_list][q][pos] = mf = DIV(def_quant8[q][pos] * 16, h->pps->scaling_list[i_list][pos]);

				//here the deadzone is a value from 0~32 2~5  
				//deadzone 1/2  should be 0.5 <<15  / mf 
		     	h->quant8_bias[i_list][q][pos] = XAVS_MIN( DIV(deadzone[i_list]<<10, mf), (1<<15)/mf );

            }
       }
	}


    if( !h->mb.b_lossless && max_qp_err >= h->param.rc.i_qp_min )
    {
        xavs_log( h, XAVS_LOG_ERROR, "Quantization overflow.\n" );
        xavs_log( h, XAVS_LOG_ERROR, "Your CQM is incompatible with QP < %d, but min QP is set to %d\n",
                  max_qp_err+1, h->param.rc.i_qp_min );
        return -1;
    }
    return 0;
fail:
    xavs_cqm_delete( h );
    return -1;
}

void xavs_cqm_delete( xavs_t *h )
{
    
}

static int xavs_cqm_parse_jmlist( xavs_t *h, const char *buf, const char *name,
                           uint8_t *cqm, const uint8_t *jvt, int length )
{
    char *p;
    char *nextvar;
    int i;

    p = strstr( buf, name );
    if( !p )
    {
        memset( cqm, 16, length );
        return 0;
    }

    p += strlen( name );
    if( *p == 'U' || *p == 'V' )
        p++;

    nextvar = strstr( p, "INT" );

    for( i = 0; i < length && (p = strpbrk( p, " \t\n," )) && (p = strpbrk( p, "0123456789" )); i++ )
    {
        int coef = -1;
        sscanf( p, "%d", &coef );
        if( i == 0 && coef == 0 )
        {
            memcpy( cqm, jvt, length );
            return 0;
        }
        if( coef < 1 || coef > 255 )
        {
            xavs_log( h, XAVS_LOG_ERROR, "bad coefficient in list '%s'\n", name );
            return -1;
        }
        cqm[i] = coef;
    }

    if( (nextvar && p > nextvar) || i != length )
    {
        xavs_log( h, XAVS_LOG_ERROR, "not enough coefficients in list '%s'\n", name );
        return -1;
    }

    return 0;
}

int xavs_cqm_parse_file( xavs_t *h, const char *filename )
{
    char *buf, *p;
    int b_error = 0;

    h->param.i_cqm_preset = XAVS_CQM_CUSTOM;

    buf = xavs_slurp_file( filename );
    if( !buf )
    {
        xavs_log( h, XAVS_LOG_ERROR, "can't open file '%s'\n", filename );
        return -1;
    }

    while( (p = strchr( buf, '#' )) != NULL )
        memset( p, ' ', strcspn( p, "\n" ) );

    b_error |= xavs_cqm_parse_jmlist( h, buf, "INTRA4X4_LUMA",   h->param.cqm_4iy, xavs_cqm_jvt4i, 16 );
    b_error |= xavs_cqm_parse_jmlist( h, buf, "INTRA4X4_CHROMA", h->param.cqm_4ic, xavs_cqm_jvt4i, 16 );
    b_error |= xavs_cqm_parse_jmlist( h, buf, "INTER4X4_LUMA",   h->param.cqm_4py, xavs_cqm_jvt4p, 16 );
    b_error |= xavs_cqm_parse_jmlist( h, buf, "INTER4X4_CHROMA", h->param.cqm_4pc, xavs_cqm_jvt4p, 16 );
    b_error |= xavs_cqm_parse_jmlist( h, buf, "INTRA8X8_LUMA",   h->param.cqm_8iy, xavs_cqm_jvt8i, 64 );
    b_error |= xavs_cqm_parse_jmlist( h, buf, "INTER8X8_LUMA",   h->param.cqm_8py, xavs_cqm_jvt8p, 64 );

    xavs_free( buf );
    return b_error;
}

void xavs_sequence_init( xavs_t *h )
{
	int i_frame_rate_code =h->param.i_fps_den? (h->param.i_fps_num/h->param.i_fps_den):0;

	h->sqh.i_video_sequence_start_code = 0xB0;
	h->sqh.i_profile_idc               = 0x20;//Jizhun profile
	h->sqh.i_level_idc                 = h->param.i_level_idc;
	h->sqh.b_progressive_sequence      = 1;//frame sequence

	h->sqh.i_horizontal_size           = h->param.i_width;
	h->sqh.i_vertical_size             = h->param.i_height;
	h->sqh.i_mb_width                  = (h->param.i_width + 15 ) / 16;
	h->sqh.i_mb_height                 = (h->param.i_height + 15 ) / 16;

	h->sqh.i_chroma_format             = 1;//h->param.i_chroma_format;   // 4:2:0
	h->sqh.i_sample_precision          = 1;//h->param.i_sample_precision; // 8 bits per sample
	h->sqh.i_aspect_ratio              = 1;//h->param.i_aspect_ratio;  // 1:1
    
	switch(i_frame_rate_code) {
	case 24:
		i_frame_rate_code = 2;//0010
		break;
	case 25:
		i_frame_rate_code = 3;//0011
		break;
	case 30:
		i_frame_rate_code = 5;//0101
		break;
	case 50:
		i_frame_rate_code = 6;//0110
		break;
	case 60:
		i_frame_rate_code = 8;//1000
		break;
	default:
		if(h->param.i_fps_num == 24000 && h->param.i_fps_den == 1001)
			i_frame_rate_code = 1;//0001
		else if(h->param.i_fps_num == 30000 && h->param.i_fps_den == 1001)
			i_frame_rate_code = 4;//0100
		else if(h->param.i_fps_num == 60000 && h->param.i_fps_den == 1001)
			i_frame_rate_code = 7;//0111
		else
			i_frame_rate_code = 9;//1001
		break;
	}
	h->sqh.i_frame_rate_code           = i_frame_rate_code;

	h->sqh.i_bit_rate_lower            = h->param.rc.i_bitrate & (0x3FFFF); // lower 18 bits of bitrate
	h->sqh.i_bit_rate_upper            = h->param.rc.i_bitrate >> 18; // bits upper to 18 bits
	h->sqh.b_low_delay                 = (h->param.i_bframe == 0);
	h->sqh.i_bbv_buffer_size           = h->param.rc.i_vbv_buffer_size;
    
}

void xavs_sequence_write( bs_t *s, xavs_seq_header_t *sqh )
{
    bs_write( s, 24, 1 );
    bs_write( s, 8, sqh->i_video_sequence_start_code );
	bs_write( s, 8, sqh->i_profile_idc);
	bs_write( s, 8, sqh->i_level_idc);
	bs_write1( s, sqh->b_progressive_sequence);
	bs_write( s, 14, sqh->i_horizontal_size);
	bs_write( s, 14, sqh->i_vertical_size);
	bs_write( s, 2, sqh->i_chroma_format);
	bs_write( s, 3, sqh->i_sample_precision);
	bs_write( s, 4, sqh->i_aspect_ratio);
	bs_write( s, 4, sqh->i_frame_rate_code);
	bs_write( s, 18, sqh->i_bit_rate_lower);
	bs_write1( s, 1);//marker bit
	bs_write( s, 12, sqh->i_bit_rate_upper);
	bs_write1( s, sqh->b_low_delay);
	bs_write1( s, 1);
	bs_write( s, 18, sqh->i_bbv_buffer_size);
	bs_write( s, 3, 0);//reserved bits
	bs_rbsp_trailing( s );
	//bs_align_0(s);
}

void xavs_i_picture_write( bs_t *s, xavs_i_pic_header_t *ih, xavs_seq_header_t *sqh )
{
    bs_write( s, 24, 1 );
    bs_write( s, 8, ih->i_i_picture_start_code);
	bs_write( s, 16, ih->i_bbv_delay);
	bs_write1( s, ih->b_time_code_flag);
	if(ih->b_time_code_flag)
		bs_write( s, 24, ih->i_time_code);
	bs_write1( s, 1);//marker bit
	bs_write( s, 8, ih->i_picture_distance);
	if(sqh->b_low_delay)
		bs_write_ue( s, ih->i_bbv_check_times);
	bs_write1( s, ih->b_progressive_frame);
	if(!ih->b_progressive_frame)
		bs_write1( s, ih->b_picture_structure);
	bs_write1( s, ih->b_top_field_first);
	bs_write1( s, ih->b_repeat_first_field);
	bs_write1( s, ih->b_fixed_picture_qp);
	bs_write( s, 6, ih->i_picture_qp);
	if(!ih->b_progressive_frame && !ih->b_picture_structure)
		bs_write1( s, ih->b_skip_mode_flag);
	bs_write( s, 4, ih->i_reserved_bits);
	bs_write1( s, ih->b_loop_filter_disable);
	if(!ih->b_loop_filter_disable)
		bs_write1( s, ih->b_loop_filter_parameter_flag);
	if(ih->b_loop_filter_parameter_flag)
	{
		bs_write_se( s, ih->i_alpha_c_offset);
		bs_write_se( s, ih->i_beta_offset);
	}
	bs_rbsp_trailing( s );
	//bs_align_0(s);
}

void xavs_pb_picture_write( bs_t *s, xavs_pb_pic_header_t *pbh, xavs_seq_header_t *sqh )
{
    bs_write( s, 24, 1 );
    bs_write( s, 8, pbh->i_pb_picture_start_code);
	bs_write( s, 16, pbh->i_bbv_delay);
	bs_write( s, 2, pbh->i_picture_coding_type);
	bs_write( s, 8, pbh->i_picture_distance);
	if(sqh->b_low_delay)
		bs_write_ue( s, pbh->i_bbv_check_times);
	bs_write1( s, pbh->b_progressive_frame);
	if(!pbh->b_progressive_frame){
		bs_write1( s, pbh->b_picture_structure);
		if(!pbh->b_picture_structure)
			bs_write1( s, pbh->b_advanced_pred_mode_disable);
	}
	bs_write1( s, pbh->b_top_field_first);
	bs_write1( s, pbh->b_repeat_first_field);
	bs_write1( s, pbh->b_fixed_picture_qp);
	bs_write( s, 6, pbh->i_picture_qp);
	if(!(pbh->i_picture_coding_type == SLICE_TYPE_B && pbh->b_picture_structure))
		bs_write1( s, pbh->b_picture_reference_flag);
	bs_write1( s, pbh->b_no_forward_reference_flag);
	bs_write( s, 3, 0);//reserved bits
	bs_write1( s, pbh->b_skip_mode_flag);
	bs_write1( s, pbh->b_loop_filter_disable);
	if(!pbh->b_loop_filter_disable)
		bs_write1( s, pbh->b_loop_filter_parameter_flag);
	if(pbh->b_loop_filter_parameter_flag)
	{
		bs_write_se( s, pbh->i_alpha_c_offset);
		bs_write_se( s, pbh->i_beta_offset);
	}
	bs_rbsp_trailing( s );
	//bs_align_0(s);
}

void xavs_slice_header_init( xavs_t *h, xavs_slice_header_t *sh,
                                    int i_idr_pic_id, int i_frame, int i_qp )
{
    xavs_param_t *param = &h->param;
    int i;
	sh->i_slice_start_code = 0x1;
	sh->i_slice_vertical_position = 0;//one slice per frame now.
	sh->i_slice_vertical_position_extension = 0;
	sh->b_fixed_slice_qp = 0;
	sh->i_slice_qp = i_qp;
	sh->b_slice_weighting_flag = 0;
	for(i = 0; i < 4; i++)
		sh->i_luma_scale[i] = 1;
		sh->i_luma_shift[i] = 0;
		sh->i_chroma_scale[i] = 1;
		sh->i_chroma_shift[i] = 0;
	sh->b_mb_weighting_flag = 0;

	//added 
	sh->i_first_mb  = 0;
    sh->i_last_mb   = h->sqh.i_mb_width * h->sqh.i_mb_height;
    sh->i_frame_num = h->i_frame_num;
    sh->i_qp = i_qp;
    /* If effective qp <= 15, deblocking would have no effect anyway */
    sh->i_disable_deblocking_filter_idc = !h->param.b_deblocking_filter;
    sh->i_alpha_c0_offset = h->param.i_deblocking_filter_alphac0;
    sh->i_beta_offset = h->param.i_deblocking_filter_beta;

}

void xavs_slice_header_write( bs_t *s, xavs_slice_header_t *sh,  xavs_seq_header_t *sqh)
{
	int i;
    bs_write( s, 24, 1 );	
	//bs_write( s, 8, sh->i_slice_start_code);
	bs_write( s, 8, sh->i_slice_vertical_position);
	if(sqh->i_vertical_size > 2800)
		bs_write( s, 3, sh->i_slice_vertical_position_extension);
	if(!sh->b_picture_fixed_qp)
	{
		bs_write1( s, sh->b_fixed_slice_qp);
		bs_write( s, 6, sh->i_slice_qp);
	}
	if(!( sh->i_type == SLICE_TYPE_I ))
	{
		bs_write1( s, sh->b_slice_weighting_flag);	
		if( sh->b_slice_weighting_flag)
		{
			for( i = 0; i < sh->i_num_ref_idx_l0_active; i++ )
			{
				bs_write( s, 8, sh->i_luma_scale[i]);
				bs_write( s, 8, sh->i_luma_shift[i]);
				bs_write1( s, 1);//marker bit
				bs_write( s, 8, sh->i_chroma_scale[i]);
                bs_write( s, 8, sh->i_chroma_shift[i]);
				bs_write1( s, 1);//marker bit
			}
		}
		bs_write1( s, sh->b_mb_weighting_flag);
	}

}



