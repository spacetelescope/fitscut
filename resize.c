/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * Image resizing functions
 *
 * Author: William Jon McCann
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
 * $Id: resize.c,v 1.9 2004/05/11 22:05:06 mccannwj Exp $
 *
 * Extensively modified 2006 October 11 by R. White (including
 * both interface and algorithm changes)
 * Added exact_resize_image_channel, RLW, 2007 March 6
 * Skip zero pixels in resize_image_channel_reduce, RLW, 2008 January 9
 */

/* make lround work ok with old gcc on linux */
#define _ISOC99_SOURCE

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

#ifdef  HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_CFITSIO_FITSIO_H
#include <cfitsio/fitsio.h> 
#else
#include <fitsio.h>
#endif

#include "fitscut.h"
#include "resize.h"

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

/* get the size for (possibly intermediate) integral zoom scaling of the image */

void
get_zoom_size_channel (int ncols, int nrows, float zoom_factor, int output_size,
		int *pixfac, int *zoomcols, int *zoomrows, int *doshrink)
{
int maxsize;

	if (output_size > 0) {

		/* fixed output size */

		maxsize = (ncols > nrows) ? ncols : nrows;
		*pixfac = maxsize/output_size;
		if (*pixfac > 1) {
			*zoomcols = (ncols-1) / (*pixfac) + 1;
			if (*zoomcols<1) *zoomcols = 1;
			*zoomrows = (nrows-1) / (*pixfac) + 1;
			if (*zoomrows<1) *zoomrows = 1;
			*doshrink = 1;
		} else {
			/* no rebinning if output is larger than input */
			*pixfac = 1;
			*zoomcols = ncols;
			*zoomrows = nrows;
			*doshrink = 0;
		}
	} else {
		if (zoom_factor <=0 || zoom_factor == 1) {
			/* no zoom */
			*pixfac = 1;
			*zoomcols = ncols;
			*zoomrows = nrows;
			*doshrink = 0;
		} else if (zoom_factor > 1) {
			/* zoom expanding */
			*pixfac = lround(zoom_factor);
			*zoomcols = ncols * (*pixfac);
			*zoomrows = nrows * (*pixfac);
			*doshrink = 0;
		} else {
			/* zoom shrinking */
			*pixfac = lround(1./zoom_factor);
			*zoomcols = (ncols-1) / (*pixfac) + 1;
			if (*zoomcols<1) *zoomcols = 1;
			*zoomrows = (nrows-1) / (*pixfac) + 1;
			if (*zoomrows<1) *zoomrows = 1;
			*doshrink = 1;
		}
	}
}

void
exact_resize_image_channel (FitsCutImage *srcImagePtr, int k, int output_size)
{
	FitsCutImage destImage;
	int orig_width, orig_height, maxsize;

	orig_width = srcImagePtr->ncols[k];
	orig_height = srcImagePtr->nrows[k];
	maxsize = (orig_width > orig_height) ? orig_width : orig_height;

	/* we're done if size matches */
	if (maxsize == output_size) return;

	/* use simple interpolation to get desired size */
	interpolate_image (srcImagePtr, &destImage, k, output_size);
	free (srcImagePtr->data[k]);
	srcImagePtr->data[k] = destImage.data[k];
	destImage.data[k] = NULL;
	srcImagePtr->ncols[k] = destImage.ncols[k];
	srcImagePtr->nrows[k] = destImage.nrows[k];
	srcImagePtr->output_zoom[k] = destImage.output_zoom[k];
}

/* modify reference header parameters for exact-resize scaling */

void
exact_resize_reference (FitsCutImage *Image, int output_size)
{
	int width, height, orig_width, orig_height, maxsize;
	double zoom_factor, orig_zoom;

	orig_width = Image->ncolsref;
	orig_height = Image->nrowsref;
	orig_zoom = Image->output_zoomref;
	if (orig_zoom == 0) orig_zoom = 1.0;

	if (orig_width > orig_height) {
		maxsize = orig_width;
		width = output_size;
		zoom_factor = ((double) width)/orig_width;
		height = lround(zoom_factor*orig_height);
		if (height<1) height = 1;
	} else {
		maxsize = orig_height;
		height = output_size;
		zoom_factor = ((double) height)/orig_height;
		width = lround(zoom_factor*orig_width);
		if (width<1) width = 1;
	}

	/* we're done if size matches */
	if (maxsize == output_size) return;

	Image->output_zoomref = zoom_factor * orig_zoom;
	Image->ncolsref = width;
	Image->nrowsref = height;
}

void
reduce_array (float *input, float *output, int orig_width, int orig_height, int pixfac, float bad_data_value)
{
	float *src;
	float *dest;
	int width, height;
	int x, y, i, j, jmin, jmax;
	int *count;

	width = (orig_width-1)/pixfac + 1;
	if (width<1) width = 1;
	height = (orig_height-1)/pixfac + 1;
	if (height<1) height = 1;

	count = (int *) malloc (width * sizeof (int));

	for (y=0; y<height; y++) {
		dest = output + y*width;
		for (x=0; x<width; x++) {
			dest[x] = 0.0;
			count[x] = 0;
		}
		jmin = pixfac*y;
		jmax = jmin + pixfac;
		if (jmax > orig_height) jmax = orig_height;
		for (j=jmin; j<jmax; j++) {
			src = input + j*orig_width;
			for (i=0; i<orig_width; i++) {
				/* ignore bad-value and NaN pixels, which are missing data */
				if (src[i] != bad_data_value && isfinite(src[i])) {
					dest[i/pixfac] += src[i];
					count[i/pixfac] += 1;
				}
			}
		}
		for (x=0; x<width; x++) {
			if (count[x] > 0) {
				dest[x] /= count[x];
			} else {
				dest[x] = NAN;
			}
		}
	}
	free(count);
}

void
enlarge_array (float *input, float *output, int orig_width, int orig_height, int pixfac)
{
	float *src;
	float *dest;
	int width, height;
	int x, y, j;

	width = orig_width*pixfac;
	height = orig_height*pixfac;

	for (j=0; j<orig_height; j++) {
		src = input + j*orig_width;
		for (y=pixfac*j; y<pixfac*(j+1); y++) {
			dest = output + y*width;
			for (x=0; x<width; x++) {
				dest[x] = src[x/pixfac];
			}
		}
	}
}

void
interpolate_image (FitsCutImage *srcImagePtr,
                   FitsCutImage *destImagePtr, 
                   int k, int output_size)
{
	float *src1;
	float *dest;
	int width, height, orig_width, orig_height;
	int x, y, i, j;
	double zoom_factor, u, v;
	/* variables for linear interpolation */
	/*
	**	float *src2;
	**	double wti1, wti2, wtj1, wtj2, zoom_factor, u, v;
	*/

	orig_width = srcImagePtr->ncols[k];
	orig_height = srcImagePtr->nrows[k];
	if (orig_width > orig_height) {
		width = output_size;
		zoom_factor = ((double) width)/orig_width;
		height = lround(zoom_factor*orig_height);
		if (height<1) height = 1;
	} else {
		height = output_size;
		zoom_factor = ((double) height)/orig_height;
		width = lround(zoom_factor*orig_width);
		if (width<1) width = 1;
	}

	destImagePtr->output_zoom[k] = zoom_factor * srcImagePtr->output_zoom[k];
	destImagePtr->ncols[k] = width;
	destImagePtr->nrows[k] = height;
	destImagePtr->data[k] = (float *) malloc (width * height * sizeof (float));

	fitscut_message (2, "\tresizing channel to x=%d y=%d from x=%d y=%d\n",
					 width, height, orig_width, orig_height);

	/* nearest neighbor interpolation */
	for (y=0; y<height; y++) {
		v = (y+0.5)/zoom_factor - 0.5;
		j = lround(v);
		dest = destImagePtr->data[k] + y*width;
		src1 = srcImagePtr->data[k] + j*orig_width;
		for (x=0; x<width; x++) {
			u = (x+0.5)/zoom_factor - 0.5;
			i = lround(u);
			dest[x] = src1[i];
		}
	}

	/* linear interpolation (would need bad pixel checks for this) */
/***
	for (y=0; y<height; y++) {
		v = (y+0.5)/zoom_factor - 0.5;
		j = (int) v;
		wtj2 = v-j;
		wtj1 = 1.0-wtj2;
		dest = destImagePtr->data[k] + y*width;
		src1 = srcImagePtr->data[k] + j*orig_width;
		src2 = src1 + orig_width;
		for (x=0; x<width; x++) {
			u = (x+0.5)/zoom_factor - 0.5;
			i = (int) u;
			wti2 = u-i;
			wti1 = 1.0-wti2;
			dest[x] = wtj1*(wti1*src1[i]+wti2*src1[i+1]) + wtj2*(wti1*src2[i]+wti2*src2[i+1]);
		}
	}
***/

}
