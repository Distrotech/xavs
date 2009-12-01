#include "common/common.h"
#include "macroblock.h"
#include "common/vlc.h"
#include "vlctest.h"
/*
 set the vlc coder benches
*/
#if TRACE_TB
void xavs_macroblock_vlc_tb( xavs_t *h )
{
	h->sh.i_type = SLICE_TYPE_I;

}

void xavs_intra_pred_mode_tb(xavs_t *h)
{
	h->mb.intra_pred_modes[0]=-1;
	h->mb.intra_pred_modes[1]=1;
	h->mb.intra_pred_modes[2]=2;
	h->mb.intra_pred_modes[3]=3;
	h->mb.c_ipred_mode = 0;

}

void xavs_coeff_tb(xavs_t *h)
{
	int i,j;
	for (i=0;i<4;i++)
		for (j=0;j<64;j++)
			h->dct.luma8x8[i][j]=0;
	h->dct.luma8x8[0][0]=3;
	h->dct.luma8x8[0][3]=5;
	h->dct.luma8x8[0][13]=65;
	h->dct.luma8x8[0][23]=4;

	h->dct.luma8x8[1][0]=3;
	h->dct.luma8x8[1][3]=5;
	h->dct.luma8x8[1][13]=65;
	h->dct.luma8x8[1][23]=4;
	h->dct.luma8x8[2][0]=3;
	h->dct.luma8x8[2][3]=5;
	h->dct.luma8x8[2][13]=65;
	h->dct.luma8x8[2][23]=4;

	h->dct.luma8x8[3][0]=3;
	h->dct.luma8x8[3][3]=5;
	h->dct.luma8x8[3][13]=65;
	h->dct.luma8x8[3][23]=4;

}

void xavs_cbp_tb(xavs_t *h)
{
	h->mb.i_cbp_luma = 0x0F;
	h->mb.i_cbp_chroma = 0x01;
}
void xavs_dqp_tb(xavs_t *h)
{
	h->mb.i_qp = 34;
	h->mb.i_last_qp = 34;
	h->param.i_fixed_qp = 1;
}


void xavs_trace2out(xavs_t *h, char *str)
{
	fprintf(h->ptrace,"%s\n",str);
	fflush(h->ptrace);
}


void xavs_init_trace(xavs_t *h)
{
	if ((h->ptrace = fopen("trace_vlc.txt","a+t"))==NULL){
		printf("Trace file open error!\n");
		exit(-1);
	}
}

#endif