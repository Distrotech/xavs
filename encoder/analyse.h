/*****************************************************************************
 * analyse.h: xavs encoder library
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef _XAVS_ANALYSE_H_
#define _XAVS_ANALYSE_H_

void xavs_macroblock_analyse (xavs_t * h);
void xavs_slicetype_decide (xavs_t * h);
void xavs_slicetype_analyse (xavs_t * h);

int  xavs_lookahead_init( xavs_t *h, int i_slicetype_length );
int  xavs_lookahead_is_empty( xavs_t *h );
void xavs_lookahead_put_frame( xavs_t *h, xavs_frame_t *frame );
void xavs_lookahead_get_frames( xavs_t *h );
void xavs_lookahead_delete( xavs_t *h );

#endif
