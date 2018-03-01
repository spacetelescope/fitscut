/* declarations for wcs_align.c
 * Copyright (C) 2002 William Jon McCann
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id: wcs_align.h,v 1.4 2004/04/21 20:13:10 mccannwj Exp $
 */

void   wcs_initialize_channel  (FitsCutImage *, int);

void   wcs_initialize    (FitsCutImage *);
void   wcs_initialize_ref(FitsCutImage *, long *naxes);
void   wcs_align_ref     (FitsCutImage *);
int    wcs_remap_channel (FitsCutImage *, int);
void   wcs_update        (FitsCutImage *);
int    wcs_match_channel (FitsCutImage *, int);
void   wcs_update_channel(FitsCutImage *, int);
void   wcs_update_ref    (FitsCutImage *);

void   wcs_apply_update   (struct WorldCoor *, double, double, double, int, int);
struct WorldCoor *wcs_read(char *filename, long *naxes);
