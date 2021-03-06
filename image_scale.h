/* declarations for image_scale.c
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
 * $Id: image_scale.h,v 1.9 2004/04/30 18:28:12 mccannwj Exp $
 */

void autoscale_image   (FitsCutImage *);
void autoscale_channel (FitsCutImage *, int);
void autoscale_full_channel (FitsCutImage *, int);
void histeq_image      (FitsCutImage *);
void log_image         (FitsCutImage *);
void sqrt_image        (FitsCutImage *);
void asinh_image       (FitsCutImage *);
void scan_min_max      (FitsCutImage *);
void mult_image        (FitsCutImage *);
void rate_image        (FitsCutImage *);

double compute_hist_mode (FitsCutImage *Image, int k);
