/*****************************************************************************
 * vlc.h :
 *****************************************************************************
 * Copyright (C) 2009 xavs project
 *
 ******************************************************************************/

#ifndef XAVS_VLC_H
#define XAVS_VLC_H

#define CODE2D_ESCAPE_SYMBOL 59


extern const int NCBP[64][2];
extern const int AVS_SCAN[2][64][2];
extern const int AVS_COEFF_COST[64]; 
extern const char AVS_2D_VLC[4][16][8];
extern const char AVS_2DVLC_INTRA[7][26][27];
extern const char AVS_2DVLC_INTER[7][26][27];
extern const char AVS_2DVLC_CHROMA[5][26][27]; 
extern const char VLC_Golomb_Order[3][7][2];
extern const char MaxRun[3][7];
extern const char RefAbsLevel[19][26];
extern vlc_large_t xavs_level_token[7][LEVEL_TABLE_SIZE];

#endif

