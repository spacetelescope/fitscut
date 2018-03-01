/* declarations for resize.c
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
 * $Id: resize.h,v 1.3 2004/04/21 20:13:10 mccannwj Exp $
 */

void exact_resize_image_channel (FitsCutImage *, int, int);
void exact_resize_reference (FitsCutImage *, int);
void interpolate_image (FitsCutImage *, FitsCutImage *, int, int);

void get_zoom_size_channel (int ncols, int nrows, float zoom_factor, int output_size,
	   int *pixfac, int *zoomcols, int *zoomrows, int *doshrink);
void reduce_array (float *input, float *output, int orig_width, int orig_height, int pixfac, float bad_data_value);
void enlarge_array (float *input, float *output, int orig_width, int orig_height, int pixfac);
