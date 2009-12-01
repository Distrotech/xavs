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

#ifndef XAVS_VLCTEST_H
#define XAVS_VLCTEST_H


#ifdef WIN32
#define snprintf _snprintf
#endif

void xavs_macroblock_vlc_tb( xavs_t *h );
void xavs_trace2out(xavs_t *h, char *str);
void xavs_init_trace(xavs_t *h);
void xavs_intra_pred_mode_tb(xavs_t *h);
void xavs_coeff_tb(xavs_t *h);

void xavs_cbp_tb(xavs_t *h);
void xavs_dqp_tb(xavs_t *h);
#endif
