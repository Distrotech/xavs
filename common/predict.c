/*****************************************************************************
 * predict.c: xavs encoder
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

/* predict4x4 are inspired from ffmpeg havs decoder */


#include "common.h"

#ifdef HAVE_MMX
#   include "x86/predict.h"
#endif
#ifdef ARCH_PPC
#   include "ppc/predict.h"
#endif


/****************************************************************************
 * 8x8 prediction for intra chroma block
 ****************************************************************************/
#define SRC(x,y) src[(x)+(y)*FDEC_STRIDE]
#define SRC32(x,y) *(uint32_t*)&SRC(x,y)
#define PREDICT_8x8_DC(v) \
    int y; \
    for( y = 0; y < 8; y++ ) { \
        ((uint32_t*)src)[0] = \
        ((uint32_t*)src)[1] = v; \
        src += FDEC_STRIDE; \
    }


static void predict_8x8c_dc_128( uint8_t *src,uint8_t edge[60] )
{
	PREDICT_8x8_DC(0x80808080);
}
static void predict_8x8c_dc_left(uint8_t *src,uint8_t edge[60])
{
//	PREDICT_8x8_LOAD_LEFT
	uint32_t dc;
//	PREDICT_8x8_DC(dc);
	int y; 

	for( y = 0; y < 8; y++ ) { 
		dc =  (*(edge+19-y)) * 0x01010101;
		((uint32_t*)src)[0] = dc;
		((uint32_t*)src)[1] = dc; 
		src += FDEC_STRIDE; 
	}

}
static void predict_8x8c_dc_top( uint8_t *src ,uint8_t edge[60])
{
//	PREDICT_8x8_LOAD_TOP
	uint32_t dc0,dc1;
	//    PREDICT_8x8_DC(dc);
	int x,y; 

	for( x = 0; x < 8; x++ ) 
	{ 
		dc0 = *((uint32_t*)(edge+21));
		dc1 = *((uint32_t*)(edge+25));
		((uint32_t*)src)[0] = dc0;
		((uint32_t*)src)[1] = dc1; 
		src += FDEC_STRIDE; 
	}

}
static void predict_8x8c_dc( uint8_t *src,uint8_t edge[60]  )
{
//	PREDICT_8x8_LOAD_LEFT
//	PREDICT_8x8_LOAD_TOP
	uint32_t dc;
	//    PREDICT_8x8_DC(dc);
	int x,y;
	uint8_t pLeft,pTop;
	
	for( y = 0; y < 8; y++ ) 
	{ 
		for( x= 0; x<8;x++ )
		{
			pLeft	= (*(edge+19-y));
			pTop	= (*(edge+21+x));
			dc		= ((pLeft + pTop)>>1); 
			src[x]	= dc;
		}
		src	   += FDEC_STRIDE;
	}
}
static void predict_8x8c_h( uint8_t *src ,uint8_t edge[60] )
{
    int i;

    for( i = 0; i < 8; i++ )
    {
        uint32_t v = 0x01010101 * src[-1];
        uint32_t *p = (uint32_t*)src;
        *p++ = v;
        *p++ = v;
        src += FDEC_STRIDE;
    }
}

static void predict_8x8c_v( uint8_t *src ,uint8_t edge[60] )
{
    uint32_t v0 = *(uint32_t*)&src[0-FDEC_STRIDE];
    uint32_t v1 = *(uint32_t*)&src[4-FDEC_STRIDE];
    int i;

    for( i = 0; i < 8; i++ )
    {
        uint32_t *p = (uint32_t*)src;
        *p++ = v0;
        *p++ = v1;
        src += FDEC_STRIDE;
    }
}
static void predict_8x8c_p( uint8_t *src ,uint8_t edge[60] )
{
      int i,j,ih,iv;
      int iaa,ib,ic;
	  
      ih = 4*(edge[50-1-7] - SRC(-1,-1));
      iv = 4*(edge[50+1+7] - SRC(-1,-1));
	  
      for (i=1;i<4;i++)
      {
        ih += i*(edge[50-1-3-i] - edge[50-1-3+i]);
        iv += i*(edge[50+1+3+i] - edge[50+1+3-i]);
      }
	  
      ib=(17*ih+16)>>5;
      ic=(17*iv+16)>>5;

      iaa=16*(edge[50-1-7]+edge[50+1+7] );
      
      for (j=0; j<8; j++)
        for (i=0; i<8; i++)
        {
          *(src+i+j*FDEC_STRIDE)=XAVS_MAX(0,XAVS_MIN(255,(iaa+(i-3)*ib +(j-3)*ic + 16)/32));// store plane prediction
    }
}


/****************************************************************************
 * 4x4 prediction for intra luma block
 ****************************************************************************/


#define PREDICT_4x4_DC(v)\
    SRC32(0,0) = SRC32(0,1) = SRC32(0,2) = SRC32(0,3) = v;


#define PREDICT_4x4_LOAD_LEFT\
    const int l0 = SRC(-1,0);\
    const int l1 = SRC(-1,1);\
    const int l2 = SRC(-1,2);\
    UNUSED const int l3 = SRC(-1,3);

#define PREDICT_4x4_LOAD_TOP\
    const int t0 = SRC(0,-1);\
    const int t1 = SRC(1,-1);\
    const int t2 = SRC(2,-1);\
    UNUSED const int t3 = SRC(3,-1);

#define PREDICT_4x4_LOAD_TOP_RIGHT\
    const int t4 = SRC(4,-1);\
    const int t5 = SRC(5,-1);\
    const int t6 = SRC(6,-1);\
    UNUSED const int t7 = SRC(7,-1);

#define F1(a,b)   (((a)+(b)+1)>>1)
#define F2(a,b,c) (((a)+2*(b)+(c)+2)>>2)


/****************************************************************************
 * 8x8 prediction for intra luma block
 ****************************************************************************/

#define PL(y) \
    edge[14-y] = F2(SRC(-1,y-1), SRC(-1,y), SRC(-1,y+1));
#define PT(x) \
    edge[16+x] = F2(SRC(x-1,-1), SRC(x,-1), SRC(x+1,-1));

  static void predict_8x8_filter( uint8_t *src, uint8_t edge[60], int i_neighbor, int chroma )
 {
	 int last_pix,new_pix;
	 int have_dl = i_neighbor & MB_DOWNLEFT;
	 int have_tr = i_neighbor & MB_TOPRIGHT;
	 int jj,i;
	 int offset = chroma ? 0 : 8;

	 if( i_neighbor & MB_LEFT )
	 {
		 for(jj=0; jj<8; jj++){
			 edge[-jj-1+20] = SRC(-1,jj);   
			 edge[-jj-1+50] = SRC(-1,jj);
		 }
		 edge[20] = SRC(-1,0);
		 for(jj=8; jj<16; jj++){
			 edge[-jj-1+20] = have_dl ? SRC(-1,jj) : edge[-8+20];      
		 }
		 for(jj=0; jj<2; jj++){
			 edge[-jj-1-8-8+20] = edge[-jj-8-8+20];   
		 }
	 }

	 if( i_neighbor & MB_TOP )
	 {
		 for(jj=0; jj<8; jj++){
			 edge[jj+1+20] = SRC(jj,-1);   
			 edge[jj+1+50] = SRC(jj,-1);
		 }
		 edge[20] = SRC( 0,-1 );

		 for(jj=8; jj<16; jj++){
			 edge[jj+1+20] = have_tr ? SRC(jj,-1) : edge[8+20];      
		 }

		 for(jj=0; jj<2; jj++){
			 edge[jj+1+8+8+20] = edge[jj+8+8+20];      
		 }
	 }

     if ( i_neighbor & MB_TOPLEFT )
     {
		 edge[20] = SRC(-1,-1);
     }

	//lowpass (Those emlements that are not needed will not disturb), used in DC/downLeft/upRight prediction
	last_pix=edge[-(8+offset)+20];
	for(i=-(8+offset);i<=(8+offset);i++)
	{
		new_pix=( last_pix + (edge[i+20]<<1) + edge[i+1+20] + 2 )>>2;
		last_pix=edge[i+20];
		edge[i+20]=(unsigned char)new_pix;
	}

 }

#undef PL
#undef PT

#define PL(y) \
    UNUSED const int l##y = edge[19-y];//    UNUSED const int l##y = edge[14-y];
#define PLO(y) \
	UNUSED const int l##y = edge[49-y];//    UNUSED const int l##y = edge[14-y];
#define PT(x) \
    UNUSED const int t##x = edge[21+x];//UNUSED const int t##x = edge[16+x];
#define PTO(x) \
	UNUSED const int t##x = edge[51+x];//UNUSED const int t##x = edge[16+x];

#define PREDICT_8x8_LOAD_TOPLEFT \
    const int lt = edge[20];// const int lt = edge[15];
#define PREDICT_8x8_LOAD_LEFT \
    PL(0) PL(1) PL(2) PL(3) PL(4) PL(5) PL(6) PL(7)
#define PREDICT_8x8_LOAD_TOP \
    PT(0) PT(1) PT(2) PT(3) PT(4) PT(5) PT(6) PT(7)
#define PREDICT_8x8_LOAD_TOPRIGHT \
    PT(8) PT(9) PT(10) PT(11) PT(12) PT(13) PT(14) PT(15)
#define PREDICT_8x8_LOAD_LEFT_UNFILTER \
	PLO(0) PLO(1) PLO(2) PLO(3) PLO(4) PLO(5) PLO(6) PLO(7)
#define PREDICT_8x8_LOAD_TOP_UNFILTER \
	PTO(0) PTO(1) PTO(2) PTO(3) PTO(4) PTO(5) PTO(6) PTO(7)

static void predict_8x8_dc_128( uint8_t *src, uint8_t edge[60] )
{
	PREDICT_8x8_DC(0x80808080);
}

static void predict_8x8_dc_left( uint8_t *src, uint8_t edge[60] )
{
	PREDICT_8x8_LOAD_LEFT
	uint32_t dc;
	//    PREDICT_8x8_DC(dc);
	int y; 

	for( y = 0; y < 8; y++ ) { 
		dc =  (*(edge+19-y)) * 0x01010101;
		((uint32_t*)src)[0] = dc;
		((uint32_t*)src)[1] = dc; 
		src += FDEC_STRIDE; 
	}
}

static void predict_8x8_dc_top( uint8_t *src, uint8_t edge[60] )
{
	PREDICT_8x8_LOAD_TOP
	uint32_t dc0,dc1;
	//    PREDICT_8x8_DC(dc);
	int x,y; 

	for( x = 0; x < 8; x++ ) 
	{ 
		dc0 = *((uint32_t*)(edge+21));
		dc1 = *((uint32_t*)(edge+25));
		((uint32_t*)src)[0] = dc0;
		((uint32_t*)src)[1] = dc1; 
		src += FDEC_STRIDE; 
	}
}

static void predict_8x8_dc( uint8_t *src, uint8_t edge[60] )
{
	PREDICT_8x8_LOAD_LEFT
	PREDICT_8x8_LOAD_TOP
	uint32_t dc;
	//    PREDICT_8x8_DC(dc);
	int x,y;
	uint8_t pLeft,pTop;
	
	for( y = 0; y < 8; y++ ) 
	{ 
		for( x= 0; x<8;x++ )
		{
			pLeft	= (*(edge+19-y));
			pTop	= (*(edge+21+x));
			dc		= ((pLeft + pTop)>>1); 
			src[x]	= dc;
		}
		src	   += FDEC_STRIDE;
	}
}

static void predict_8x8_h( uint8_t *src, uint8_t edge[60] )
{
	PREDICT_8x8_LOAD_LEFT_UNFILTER
#define ROW(y) ((uint32_t*)(src+y*FDEC_STRIDE))[0] =\
	((uint32_t*)(src+y*FDEC_STRIDE))[1] = 0x01010101U * l##y
		ROW(0); ROW(1); ROW(2); ROW(3); ROW(4); ROW(5); ROW(6); ROW(7);
#undef ROW
}

static void predict_8x8_v( uint8_t *src, uint8_t edge[60] )
{
	const uint64_t top = *(uint64_t*)(edge+51);
	int y;
	for( y = 0; y < 8; y++ )
		*(uint64_t*)(src+y*FDEC_STRIDE) = top;
}

static void predict_8x8_down_left( uint8_t *src, uint8_t edge[60] )
{
//    PREDICT_8X8_LOAD_DOWNLEFT
//     const uint64_t top = *(uint64_t*)(edge+16);
    int y,x;
    for( y = 0; y < 8; y++ )
		for( x = 0; x < 8; x++)
		{
			*(src+y*FDEC_STRIDE+x) = ( *(edge+20+2+x+y) + *(edge+20-2-(x+y) ) ) >> 1;
		}
}

static void predict_8x8_up_right( uint8_t *src, uint8_t edge[60] )
{
// 	PREDICT_8x8_LOAD_TOPRIGHT
	int y,x;
	for( y = 0; y < 8; y++ )
		for( x = 0; x < 8; x++)
		{
			*(src+y*FDEC_STRIDE+x) = *(edge+20+x-y);
		}
}


/****************************************************************************
 * Exported functions:
 ****************************************************************************/

void xavs_predict_8x8c_init( int cpu, xavs_predict_t pf[7] )
{
	pf[I_PRED_CHROMA_V ]     	= predict_8x8c_v;
	pf[I_PRED_CHROMA_H ]     	= predict_8x8c_h;
	pf[I_PRED_CHROMA_DC]     	= predict_8x8c_dc;
	pf[I_PRED_CHROMA_P ]     	= predict_8x8c_p;
	pf[I_PRED_CHROMA_DC_LEFT]	= predict_8x8c_dc_left;
	pf[I_PRED_CHROMA_DC_TOP ]	= predict_8x8c_dc_top;
	pf[I_PRED_CHROMA_DC_128 ]	= predict_8x8c_dc_128;

#ifdef HAVE_MMX
    xavs_predict_8x8c_init_mmx( cpu, pf );
#endif

#ifdef ARCH_PPC
    if( cpu&XAVS_CPU_ALTIVEC )
    {
        xavs_predict_8x8c_init_altivec( pf );
    }
#endif
}

void xavs_predict_8x8_init( int cpu, xavs_predict8x8_t pf[12], xavs_predict_8x8_filter_t *predict_filter )
{
	pf[AVS_I_PRED_8x8_V ]		= predict_8x8_v;
	pf[AVS_I_PRED_8x8_H ]		= predict_8x8_h;
	pf[AVS_I_PRED_8x8_DC]		= predict_8x8_dc;
	pf[AVS_I_PRED_8x8_DLF ]		= predict_8x8_down_left;
	pf[AVS_I_PRED_8x8_URT]		= predict_8x8_up_right;
	pf[AVS_I_PRED_8x8_DC_LEFT]	= predict_8x8_dc_left;
	pf[AVS_I_PRED_8x8_DC_TOP ]	= predict_8x8_dc_top;
	pf[AVS_I_PRED_8x8_DC_128 ]	= predict_8x8_dc_128;

	*predict_filter				= predict_8x8_filter;

#ifdef HAVE_MMX
    xavs_predict_8x8_init_mmx( cpu, pf, predict_filter );
#endif
}


