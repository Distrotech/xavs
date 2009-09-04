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

	//i_list =0 intra ; i_list = 1 inter 
    for( i_list = 0; i_list < 2; i_list++ )
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

