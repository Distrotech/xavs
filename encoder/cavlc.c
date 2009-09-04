  /*****************************************************************************
* cavlc.c: avs encoder library
*****************************************************************************
* Copyright (C) 2009 xavs project
*
* Authors: Ping Yang <yangping00@hotmail.com>

*****************************************************************************/

#include "common/common.h"
#include "macroblock.h"
#include "common/vlc.h"

#ifndef RDO_SKIP_BS
#define RDO_SKIP_BS 0
#endif


static int xavs_mbtype_value(xavs_t *h)
{
  static const int dir1offset[3]    =  { 1,  2, 3};
  static const int dir2offset[3][3] = {{ 0,  4,  8},   // 1. block forward
                                       { 6,  2, 10},   // 1. block backward
                                       {12, 14, 16}};  // 1. block bi-directional

  int mb_type = h->mb.i_type;
  const int slice_type = h->sh.i_type;
  const int cbp = ( h->mb.i_cbp_chroma << 4 )|h->mb.i_cbp_luma;
  int mbtype, pdir0, pdir1;

  if (slice_type!=SLICE_TYPE_B)
  {
	//this statment maybe have no use   xfwang 2004.7.29
    if (mb_type==3)
      mb_type = P_8x8;

    if (mb_type==I_8x8)
      return (slice_type==SLICE_TYPE_I ? 0 : 5)+NCBP[cbp][0];

    else if (mb_type==P_8x8)
    {         
	    return 4;
    }
    else
      return mb_type;
  }
  else
  {
    if(mb_type==4)
      mb_type = P_8x8;
    
    mbtype = mb_type;
    pdir0  = h->mb.b8pdir[0];
    pdir1  = h->mb.b8pdir[3];
    
    if      (mbtype==0)
      return 0;
    else if (mbtype==I_8x8)
      return 23+NCBP[cbp][0];// qhg;
    else if (mbtype==P_8x8)
      return 22;
    else if (mbtype==1)
      return dir1offset[pdir0];
    else if (mbtype==2)
      return 4 + dir2offset[pdir0][pdir1];
    else
      return 5 + dir2offset[pdir0][pdir1];
  }
}

static int xavs_b8mode_value(xavs_t *h, int idx)
{
  static const int b8start[8] = {0,0,0,0, 1, 4, 5, 10};
  static const int b8inc  [8] = {0,0,0,0, 1, 2, 2, 1};
  const int i_mb_type = h->mb.i_type;
  const int i_slice_type = h->sh.i_type;
  int b8mode = h->mb.b8mode[idx];
  int b8pdir = h->mb.b8pdir[idx];
  if(i_slice_type!=SLICE_TYPE_B)
	  return (b8mode-4);
  else
	  return b8start[b8mode] + b8inc[b8mode] * b8pdir;
}

static void xavs_write_MBHeader_uvlc(xavs_t *h, bs_t *s)
{
	const int i_mb_type = h->mb.i_type;
	const int i_slice_type = h->sh.i_type;
	const int mb_cbp = ( h->mb.i_cbp_chroma << 4 )|h->mb.i_cbp_luma;
	int i,tmp;
	
	/*
	 *	write Skip counter
	 */
	if (i_slice_type != SLICE_TYPE_I)
	{
		if (h->param.i_skip_mode_flag)
		{
			if (i_mb_type!=0||(i_slice_type==SLICE_TYPE_B)&&mb_cbp)
			{
				/* write skip mb numbers*/
				bs_write_ue_big(s, h->mb.code_counter); //to check weather the skip numbers can be greater than 255
				h->mb.code_counter=0;	//reset here??? 
				
				/*set mb type*/
				tmp = xavs_mbtype_value(h);
				if (i_slice_type!=SLICE_TYPE_B)
					tmp-=1;
				bs_write_ue (s, tmp);
			}
			else {
				h->mb.code_counter++;
				if(h->mb.i_current_mb_num==h->sh.i_last_mb)
				{
					bs_write_ue_big(s, h->mb.code_counter);
					h->mb.code_counter = 0;
				}
			}
		}
		else{
			tmp = xavs_mbtype_value(h);
			if(i_slice_type!=SLICE_TYPE_B)
				tmp--;
			if(i_mb_type==0&&((i_slice_type!=SLICE_TYPE_B)||(mb_cbp==0)))
				tmp = 0;
			else
				tmp++;
			bs_write_ue(s,tmp);
		}
	}

	/*
	 *	write b8 mode;
	 */
	if(IS_P8x8(i_mb_type))
	{
		if(i_slice_type!=SLICE_TYPE_P)
		{
			for(i=0; i<4; i++)
			{
				tmp = xavs_b8mode_value(h, i);
				bs_write(s,2,tmp);
			}
		}
	}

	/*
	 *	write intra prediction mode
	 */

	if(IS_INTRA(i_mb_type))
	{
		for(i=0; i<4; i++)
		{
			tmp = h->mb.intra_pred_modes[i];
			if (tmp == -1)
				bs_write(s, 1, 1);
			else
				bs_write(s, 3, tmp);

		}
		tmp = h->mb.c_ipred_mode;
		bs_write_ue(s,tmp);
	}
}


void xavs_macroblock_write_cavlc ( xavs_t *h, bs_t *s )
{
    return; 
}


static int xavs_partition_size_cavlc( xavs_t *h, int i8, int i_pixel )
{
	return 0; 
}

static int xavs_subpartition_size_cavlc( xavs_t *h, int i4, int i_pixel )
{
   return 0; 
}

static int xavs_partition_i8x8_size_cavlc( xavs_t *h, int i8, int i_mode )
{
	return 0; 
}

static int xavs_i8x8_chroma_size_cavlc( xavs_t *h )
{

}

static void xavs_write_ref_idx(xavs_t *h, bs_t *s)
{
	const int i_mb_type = h->mb.i_type;
	const int i_slice_type = h->sh.i_type;
	const int i_partition_type = h->mb.i_partition;
	int has_forward_ref[4] ;
	int has_backward_ref[4];
	int max_ref_num = h->param.i_reference_number;
	int ref_idx, ref_num;
	int i;


	for (i=0; i<4; i++)
	{
		has_forward_ref[i] = ((h->mb.b8pdir[i]==0)||(h->mb.b8pdir[i]==2))&&(h->mb.b8mode[i]!=0)&&(i_slice_type!=SLICE_TYPE_I);
		has_backward_ref[i] = (h->mb.b8pdir[i]==1)&&(h->mb.b8mode[i]!=0);
	}
	
	if ( i_mb_type == P_L0 && i_partition_type == D_16x16)
	{
		if(has_forward_ref[0]&&max_ref_num>1)
		{			
			if (i_slice_type==SLICE_TYPE_P || (!h->param.i_is_interlaced))
			{
				if((i_slice_type==SLICE_TYPE_P)&&(!h->param.i_is_interlaced))
					bs_write(s, 2, h->mb.cache.ref[0][xavs_scan8[0]]);
				else
					bs_write(s, 1, h->mb.cache.ref[0][xavs_scan8[0]]);
			}
		}
	}
	else if (i_mb_type == P_L0 && i_partition_type == D_16x8)
	{
		if (has_forward_ref[0]&&max_ref_num>1)
		{
			if (i_slice_type==SLICE_TYPE_P || (!h->param.i_is_interlaced))
			{
				if((i_slice_type==SLICE_TYPE_P)&&(!h->param.i_is_interlaced))
					bs_write(s, 2, h->mb.cache.ref[0][xavs_scan8[0]]);
				else
					bs_write(s, 1, h->mb.cache.ref[0][xavs_scan8[0]]);
			}
		}
		if (has_forward_ref[2]&&max_ref_num>1)
		{
			if (i_slice_type==SLICE_TYPE_P || (!h->param.i_is_interlaced))
			{
				if((i_slice_type==SLICE_TYPE_P)&&(!h->param.i_is_interlaced))
					bs_write(s, 2, h->mb.cache.ref[0][xavs_scan8[8]]);
				else
					bs_write(s, 1, h->mb.cache.ref[0][xavs_scan8[8]]);
			}
		}
	}
	else if (i_mb_type == P_L0 && i_partition_type == D_8x16)
	{
		if (has_forward_ref[0]&&max_ref_num>1)
		{
			if (i_slice_type==SLICE_TYPE_P || (!h->param.i_is_interlaced))
			{
				if((i_slice_type==SLICE_TYPE_P)&&(!h->param.i_is_interlaced))
					bs_write(s, 2, h->mb.cache.ref[0][xavs_scan8[0]]);
				else
					bs_write(s, 1, h->mb.cache.ref[0][xavs_scan8[0]]);
			}
		}
		if (has_forward_ref[1]&&max_ref_num>1)
		{
			if (i_slice_type==SLICE_TYPE_P || (!h->param.i_is_interlaced))
			{
				if((i_slice_type==SLICE_TYPE_P)&&(!h->param.i_is_interlaced))
					bs_write(s, 2, h->mb.cache.ref[0][xavs_scan8[4]]);
				else
					bs_write(s, 1, h->mb.cache.ref[0][xavs_scan8[4]]);
			}
		}
	}
	else if (i_mb_type == P_L0 && i_partition_type == D_8x8)
	{
		if (has_forward_ref[0]&&max_ref_num>1)
		{
			if (i_slice_type==SLICE_TYPE_P || (!h->param.i_is_interlaced))
			{
				if((i_slice_type==SLICE_TYPE_P)&&(!h->param.i_is_interlaced))
					bs_write(s, 2, h->mb.cache.ref[0][xavs_scan8[0]]);
				else
					bs_write(s, 1, h->mb.cache.ref[0][xavs_scan8[0]]);
			}
		}
		if (has_forward_ref[1]&&max_ref_num>1)
		{
			if (i_slice_type==SLICE_TYPE_P || (!h->param.i_is_interlaced))
			{
				if((i_slice_type==SLICE_TYPE_P)&&(!h->param.i_is_interlaced))
					bs_write(s, 2, h->mb.cache.ref[0][xavs_scan8[4]]);
				else
					bs_write(s, 1, h->mb.cache.ref[0][xavs_scan8[4]]);
			}
		}
		if (has_forward_ref[2]&&max_ref_num>1)
		{
			if (i_slice_type==SLICE_TYPE_P || (!h->param.i_is_interlaced))
			{
				if((i_slice_type==SLICE_TYPE_P)&&(!h->param.i_is_interlaced))
					bs_write(s, 2, h->mb.cache.ref[0][xavs_scan8[8]]);
				else
					bs_write(s, 1, h->mb.cache.ref[0][xavs_scan8[8]]);
			}
		}
		if (has_forward_ref[3]&&max_ref_num>1)
		{
			if (i_slice_type==SLICE_TYPE_P || (!h->param.i_is_interlaced))
			{
				if((i_slice_type==SLICE_TYPE_P)&&(!h->param.i_is_interlaced))
					bs_write(s, 2, h->mb.cache.ref[0][xavs_scan8[12]]);
				else
					bs_write(s, 1, h->mb.cache.ref[0][xavs_scan8[12]]);
			}
		}
	}

	if (i_slice_type==SLICE_TYPE_B)
	{
		if (i_partition_type == D_16x16)
		{
			if(has_backward_ref[0]&&max_ref_num>1)
			{			
				//			ref_num = i_slice_type==SLICE_TYPE_P? 2:1;
				//			if (ref_num == 1 && h->param.i_is_interlaced)
				//				break;
				//			if (!h->param.i_is_interlaced && (i_slice_type!=SLICE_TYPE_B))
				//				bs_write(s, 2, ref_idx);
				//			else
				//				bs_write(s, 1, ref_idx);
				
				if (!h->param.i_is_interlaced)
				{
					bs_write(s, 1, 1-h->mb.cache.ref[1][xavs_scan8[0]]);
				}
			}
		}
		else if (i_partition_type ==D_16x8)
		{
			if(has_backward_ref[0]&&max_ref_num>1&&(!h->param.i_is_interlaced))
			{			
				bs_write(s, 1, 1-h->mb.cache.ref[1][xavs_scan8[0]]);
			}
			if(has_backward_ref[2]&&max_ref_num>1&&(!h->param.i_is_interlaced))
			{			
				bs_write(s, 1, 1-h->mb.cache.ref[1][xavs_scan8[8]]);
			}
		}
		else if (i_partition_type ==D_8x16)
		{
			if(has_backward_ref[0]&&max_ref_num>1&&(!h->param.i_is_interlaced))
			{			
				bs_write(s, 1, 1-h->mb.cache.ref[1][xavs_scan8[0]]);
			}
			if(has_backward_ref[1]&&max_ref_num>1&&(!h->param.i_is_interlaced))
			{			
				bs_write(s, 1, 1-h->mb.cache.ref[1][xavs_scan8[4]]);
			}
		}
		else if (i_partition_type == D_8x8)
		{
			if(has_backward_ref[0]&&max_ref_num>1&&(!h->param.i_is_interlaced))
			{			
				bs_write(s, 1, 1-h->mb.cache.ref[1][xavs_scan8[0]]);
			}
			if(has_backward_ref[1]&&max_ref_num>1&&(!h->param.i_is_interlaced))
			{			
				bs_write(s, 1, 1-h->mb.cache.ref[1][xavs_scan8[4]]);
			}
			if(has_backward_ref[2]&&max_ref_num>1&&(!h->param.i_is_interlaced))
			{			
				bs_write(s, 1, 1-h->mb.cache.ref[1][xavs_scan8[8]]);
			}
			if(has_backward_ref[1]&&max_ref_num>1&&(!h->param.i_is_interlaced))
			{			
				bs_write(s, 1, 1-h->mb.cache.ref[1][xavs_scan8[12]]);
			}
		}		
	}
}

static void cavlc_mb_mvd( xavs_t *h, bs_t *s, int i_list, int idx, int width )
{
    DECLARE_ALIGNED_4( int16_t mvp[2] );
    xavs_mb_predict_mv( h, i_list, idx, width, mvp );
    bs_write_se( s, h->mb.cache.mv[i_list][xavs_scan8[idx]][0] - mvp[0] );
    bs_write_se( s, h->mb.cache.mv[i_list][xavs_scan8[idx]][1] - mvp[1] );
}

static void xavs_write_mvd(xavs_t *h, bs_t *s)
{
	const int i_mb_type = h->mb.i_type;
	const int i_slice_type = h->sh.i_type;
	const int i_partition_type = h->mb.i_partition;
	int has_forward_mv[4] ;
	int has_backward_mv[4];
	int i,j;
	for (i=0; i<4; i++)
	{
		has_forward_mv[i] = ((h->mb.b8pdir[i]==0)||(h->mb.b8pdir[i]==2))&&(h->mb.b8mode[i]!=0);
		has_backward_mv[i] = (h->mb.b8pdir[i]==1)&&(h->mb.b8mode[i]!=0);
	}

	if (i_mb_type == P_L0 && i_partition_type == D_16x16)
	{
		if (has_forward_mv[0])
			cavlc_mb_mvd( h, s, 0, 0, 4 );
	}
	else if (i_mb_type == P_L0 && i_partition_type == D_16x8)
	{
		if (has_forward_mv[0])
			cavlc_mb_mvd( h, s, 0, 0, 4 );
		if (has_forward_mv[2])
			cavlc_mb_mvd( h, s, 0, 8, 4 );
	}
	else if (i_mb_type == P_L0 && i_partition_type == D_8x16)
	{
		if (has_forward_mv[0])
			cavlc_mb_mvd( h, s, 0, 0, 2 );
		if (has_forward_mv[2])
			cavlc_mb_mvd( h, s, 0, 4, 2 );
	}
	else if (i_mb_type == P_L0 && i_partition_type == D_8x8)
	{
		if (has_forward_mv[0])
			cavlc_mb_mvd( h, s, 0, 0, 2 );
		if (has_forward_mv[1])
			cavlc_mb_mvd( h, s, 0, 4, 2 );
		if (has_forward_mv[2])
			cavlc_mb_mvd( h, s, 0, 8, 2 );
		if (has_forward_mv[3])
			cavlc_mb_mvd( h, s, 0, 12, 2 );
	}

	if (i_slice_type==SLICE_TYPE_B)
	{
		if (i_partition_type == D_16x16)
		{
			if (has_backward_mv[0])
				cavlc_mb_mvd( h, s, 1, 0, 4 );
		}
		else if (i_partition_type == D_16x8)
		{
			if (has_backward_mv[0])
				cavlc_mb_mvd( h, s, 1, 0, 4 );
			if (has_backward_mv[2])
				cavlc_mb_mvd( h, s, 1, 8, 4 );
		}
		else if (i_partition_type == D_8x16)
		{
			if (has_backward_mv[0])
				cavlc_mb_mvd( h, s, 1, 0, 2 );
			if (has_backward_mv[2])
				cavlc_mb_mvd( h, s, 1, 4, 2 );
		}
		else if (i_partition_type == D_8x8)
		{
			if (has_backward_mv[0])
				cavlc_mb_mvd( h, s, 1, 0, 2 );
			if (has_backward_mv[1])
				cavlc_mb_mvd( h, s, 1, 4, 2 );
			if (has_backward_mv[2])
				cavlc_mb_mvd( h, s, 1, 8, 2 );
			if (has_backward_mv[3])
				cavlc_mb_mvd( h, s, 1, 12, 2 );
		}
	}
}




static void xavs_write_cbp(xavs_t *h, bs_t *s)
{
	const int i_mb_type = h->mb.i_type;
	const int i_slice_type = h->sh.i_type;
	const int mb_cbp = ( h->mb.i_cbp_chroma << 4 )|h->mb.i_cbp_luma;
	if (IS_INTRA(i_mb_type))
		bs_write_ue(s,NCBP[mb_cbp][0]);
	else
		bs_write_ue(s,NCBP[mb_cbp][1]);

}

static void xavs_write_dqp(xavs_t *h, bs_t *s)
{
	const int i_mb_type = h->mb.i_type;
	const int i_slice_type = h->sh.i_type;
	const int mb_cbp = ( h->mb.i_cbp_chroma << 4 )|h->mb.i_cbp_luma;
	int i_dqp = h->mb.i_qp - h->mb.i_last_qp; // asking for how to get the delta qp
	if (mb_cbp&&!h->param.i_fixed_qp)
		bs_write_se(s,i_dqp);
}

static void code_golomb_word(int sym, int grad, int maxlevels, int *val, int *len)
{
	unsigned int level,res,numbits;
	
	res=1UL<<grad;
	level=1UL;
	numbits=1UL+grad;
	
	
	//find golomb level
	while (sym>=res&&level<maxlevels) {
		sym -=res;
		res = res<<1;
		level ++;
		numbits +=2UL;
	}
	if ((level >=maxlevels) && (sym >=res)) 
		sym = res-1UL;
	*val = res|sym;
	*len = numbits;
}


static void encode_multilayer_golomb_word(int sym, int *grad, int *maxlevels, int *val, int *len)
{
	  unsigned accbits,acclen,tmpbit,tmplen,tmp;
	  
	  accbits=acclen=0UL;
	  while (1)
	  {
		  code_golomb_word(sym, *grad, *maxlevels, &tmpbit, &tmplen);
		  accbits = (accbits<<tmplen)|tmpbit;
		  acclen +=tmplen;
		  tmp = *maxlevels - 1;
		  if(!(( tmplen == (tmp<<1)+(*grad) )&&( tmpbit == (1UL<<(tmp+*grad))-1UL )))
			  break;
		  tmp = *maxlevels;
		  sym-=(((1UL<<tmp)-1UL)<<(*grad))-1UL;
		  grad++;
		  maxlevels++;
	  }
	  *val = accbits;
	  *len = acclen;
}

static void xavs_write_golomb_code(bs_t *s, int grad, int symbol2D, int maxlevels)
{
	int len, val, i;
	unsigned int grad_array[4],max_lev[4];
	if (!(maxlevels&~0xFF))
		code_golomb_word(symbol2D,grad,maxlevels,&val,&len);
	else{
		for (i = 0UL; i<4UL; i++)
		{
			grad_array[i]=(grad>>(i<<3))&0xFFUL;
			max_lev[i]=(maxlevels>>(i<<3))&0xFFUL;
		}
		encode_multilayer_golomb_word(symbol2D, grad_array, max_lev, &val, &len);
	}
	bs_write(s, val, len);
}

static void xavs_write_luma_coeff(xavs_t *h, bs_t *s, int b8)
{
	static const int incVlc_intra[7] = { 0,1,2,4,7,10,3000}; 
	static const int incVlc_inter[7] = { 0,1,2,3,6,9, 3000};
	const char (*AVS_2DVLC_table_intra)[26][27];  
	const char (*AVS_2DVLC_table_inter)[26][27];  
	int16_t *l = h->dct.luma8x8[b8];
	xavs_run_level_t runlevel;
	int i_total, i_total_zeros, run, level, table_idx, symbol2D, golomb_grad, golomb_maxlevels;
	int escape_level_diff;
	int i_max_coeff = 65; //all scanned position and EOB
    runlevel.level[1] = 2;
    runlevel.level[2] = 2;
//	different with x264, the last of (run, level) should be (0,0) as a EOB

    i_total = h->quantf.coeff_level_run[DCT_LUMA_8x8]( l, &runlevel );
	i_total_zeros = runlevel.last + 1 - i_total;
	
//set the EOB, be aware of the run level direction
//	runlevel.run[i_total] = 0;
//	runlevel.level[i_total] = 0;
//	i_total ++;

// 
	AVS_2DVLC_table_intra = AVS_2DVLC_INTRA;
	AVS_2DVLC_table_inter = AVS_2DVLC_INTER;

	if (IS_INTRA(h->mb.i_type))
	{
		table_idx = 0;
		for (i_total; i_total>=0; i_total--)
		{
			if(i_total == 0){
				run = 0;
				level = 0;
			}
			else{
				run = runlevel.run[i_total];
				level = runlevel.level[i_total];
			}
			symbol2D = CODE2D_ESCAPE_SYMBOL;
			if(level>-27 && level<27 && run<26)
			{
				if(table_idx == 0)
					symbol2D = AVS_2DVLC_table_intra[table_idx][run][abs(level)-1];   
				else
					symbol2D = AVS_2DVLC_table_intra[table_idx][run][abs(level)];   
				if(symbol2D >= 0 && level < 0)
					symbol2D++;
				if(symbol2D < 0)
					symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0)); 
			}
			else
			{
				symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0)); 
			}
	        golomb_grad			= VLC_Golomb_Order[0][table_idx][0];    
		    golomb_maxlevels	= VLC_Golomb_Order[0][table_idx][1];    
 			xavs_write_golomb_code(s, symbol2D, golomb_grad, golomb_maxlevels);
			if (i_total==0)
				break;
			if (symbol2D>=CODE2D_ESCAPE_SYMBOL)
			{
				golomb_grad = 1;
				golomb_maxlevels = 11;
				escape_level_diff = abs(level)-((run>MaxRun[0][table_idx])?1:RefAbsLevel[table_idx][run]);
				xavs_write_golomb_code(s, escape_level_diff,golomb_grad, golomb_maxlevels );
			}
			if(abs(level) > incVlc_intra[table_idx])   //qwang 11.29
			{
				if(abs(level) <= 2)
					table_idx = abs(level);
				else if(abs(level) <= 4)
					table_idx = 3;
				else if(abs(level) <= 7) 
					table_idx = 4;
				else if(abs(level) <= 10)
					table_idx = 5;
				else
					table_idx = 6;
			}		
		}
	}
	else // inter
	{
		table_idx = 0;
		for (i_total; i_total>=0; i_total--)
		{
			if(i_total == 0){
				run = 0;
				level = 0;
			}
			else{
				run = runlevel.run[i_total];
				level = runlevel.level[i_total];
			}
			symbol2D = CODE2D_ESCAPE_SYMBOL;
			if(level>-27 && level<27 && run<26)
			{
				if(table_idx == 0)
					symbol2D = AVS_2DVLC_table_inter[table_idx][run][abs(level)-1];   
				else
					symbol2D = AVS_2DVLC_table_inter[table_idx][run][abs(level)];   
				if(symbol2D >= 0 && level < 0)
					symbol2D++;
				if(symbol2D < 0)
					symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0)); 
			}
			else
			{
				symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0)); 
			}
	        golomb_grad			= VLC_Golomb_Order[1][table_idx][0];    
		    golomb_maxlevels	= VLC_Golomb_Order[1][table_idx][1];    
 			xavs_write_golomb_code(s, symbol2D, golomb_grad, golomb_maxlevels);
			if (i_total==0)
				break;
			if (symbol2D>=CODE2D_ESCAPE_SYMBOL)
			{
				golomb_grad = 0;
				golomb_maxlevels = 12;
				escape_level_diff = abs(level)-((run>MaxRun[1][table_idx])?1:RefAbsLevel[table_idx+7][run]);
				xavs_write_golomb_code(s, escape_level_diff,golomb_grad, golomb_maxlevels );
			}
			if(abs(level) > incVlc_inter[table_idx])   //qwang 11.29
			{
				if(abs(level) <= 3)
					table_idx = abs(level);
				else if(abs(level) <= 6)
					table_idx = 4;
				else if(abs(level) <= 9) 
					table_idx = 5;
				else
					table_idx = 6;
			}
		}		
	}	
}

static void xavs_write_chroma_coeff(xavs_t *h, bs_t *s, int uv)
{
	static const int incVlc_chroma[5] = { 0,1,2,4,3000};  
	const char (*AVS_2DVLC_table_chroma)[26][27];
	int16_t *l = h->dct.luma8x8[uv]; // ????? should be reset
	xavs_run_level_t runlevel;
	int i_total, i_total_zeros, run, level, table_idx, symbol2D, golomb_grad, golomb_maxlevels;
	int escape_level_diff;
	int i_max_coeff = 65; //all scanned position and EOB
    runlevel.level[1] = 2;
    runlevel.level[2] = 2;
//	different with x264, the last of (run, level) should be (0,0) as a EOB

    i_total = h->quantf.coeff_level_run[DCT_LUMA_8x8]( l, &runlevel );
	i_total_zeros = runlevel.last + 1 - i_total;
	
//set the EOB, be aware of the run level direction
//	runlevel.run[i_total] = 0;
//	runlevel.level[i_total] = 0;
//	i_total ++;

// 
	AVS_2DVLC_table_chroma = AVS_2DVLC_CHROMA;
	
	table_idx = 0;
	for (i_total; i_total>=0; i_total--)
	{
		if(i_total == 0){
			run = 0;
			level = 0;
		}
		else{
			run = runlevel.run[i_total];
			level = runlevel.level[i_total];
		}
		symbol2D = CODE2D_ESCAPE_SYMBOL;
		if(level>-27 && level<27 && run<26)
		{
			if(table_idx == 0)
				symbol2D = AVS_2DVLC_table_chroma[table_idx][run][abs(level)-1];   
			else
				symbol2D = AVS_2DVLC_table_chroma[table_idx][run][abs(level)];   
			if(symbol2D >= 0 && level < 0)
				symbol2D++;
			if(symbol2D < 0)
				symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0)); 
		}
		else
		{
			symbol2D = (CODE2D_ESCAPE_SYMBOL + (run<<1) + ((level>0)?1:0)); 
		}
		golomb_grad			= VLC_Golomb_Order[2][table_idx][0];    
		golomb_maxlevels	= VLC_Golomb_Order[2][table_idx][1];    
		xavs_write_golomb_code(s, symbol2D, golomb_grad, golomb_maxlevels);
		if (i_total==0)
			break;
		if (symbol2D>=CODE2D_ESCAPE_SYMBOL)
		{
			golomb_grad = 0;
			golomb_maxlevels = 11;
			escape_level_diff = abs(level)-((run>MaxRun[2][table_idx])?1:RefAbsLevel[table_idx+14][run]);
			xavs_write_golomb_code(s, escape_level_diff,golomb_grad, golomb_maxlevels );
		}
        if(abs(level) > incVlc_chroma[table_idx])   //qwang 11.29
        {
            if(abs(level) <= 2)
              table_idx = abs(level);
            else if(abs(level) <= 4)
              table_idx = 3;
            else
              table_idx = 4;
        }
	}
	
}


static void xavs_write_coeff(xavs_t *h, bs_t *s)
{
	int i;
	const int mb_cbp = ( h->mb.i_cbp_chroma << 4 )|h->mb.i_cbp_luma;
	for (i=0; i<4; i++)
		if (mb_cbp&&(1<<i))
			xavs_write_luma_coeff(h, s, i);
	for (i=4; i<6; i++)
		if (mb_cbp&&(1<<i))
			xavs_write_chroma_coeff(h, s, i);
}

