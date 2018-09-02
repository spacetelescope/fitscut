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
 * $Id: resize.c,v 1.10 2006/04/17 14:43:21 mccannwj Exp $
 */

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

void
resize_image_channel (FitsCutImage *srcImagePtr, int k, int width, int height)
{
        FitsCutImage destImage;
        float xfactor, yfactor;

        xfactor = width / srcImagePtr->ncols[k];
        yfactor = height / srcImagePtr->nrows[k];
        destImage.ncols[k] = width;
        destImage.nrows[k] = height;

        destImage.x0[k] = srcImagePtr->x0[k] * xfactor;
        destImage.y0[k] = srcImagePtr->y0[k] * yfactor;

        destImage.data[k] = (float *) malloc (width * height * sizeof (float));
        if (xfactor > 1) {
                resize_image_sample_enlarge (srcImagePtr, &destImage, k);
        } else {
                resize_image_sample_reduce (srcImagePtr, &destImage, k);
        }
        free (srcImagePtr->data[k]);
        srcImagePtr->data[k] = destImage.data[k];
        destImage.data[k] = NULL;

        srcImagePtr->ncols[k] = width;
        srcImagePtr->nrows[k] = height;
        srcImagePtr->x0[k] = destImage.x0[k];
        srcImagePtr->y0[k] = destImage.y0[k];
}

void
resize_image_sample_reduce (FitsCutImage *srcImagePtr,
                            FitsCutImage *destImagePtr, 
                            int k)
{
        int *x_dest_offsets;
        int *y_dest_offsets;
        int *line_weights;
        float *src;
        float *dest;
        double tmpval;
        int width, height, orig_width, orig_height;
        int last_dest_y;
        int row_bytes;
        int x, y;
        char bytes;

        orig_width = srcImagePtr->ncols[k];
        orig_height = srcImagePtr->nrows[k];

        width = destImagePtr->ncols[k];
        height = destImagePtr->nrows[k];

        fitscut_message (2, "\tresizing channel to x=%d y=%d from x=%d y=%d\n",
                         width, height, orig_width, orig_height);
        bytes = sizeof (float);

        /*  the data pointers...  */
        x_dest_offsets = (int *) malloc (orig_width * sizeof (int));
        y_dest_offsets = (int *) malloc (orig_height * sizeof (int));
        src  = (float *) calloc (orig_width * bytes, 1);
        dest = (float *) calloc (width * bytes, 1);
        line_weights = (int *) calloc (width * sizeof (int), 1);
  
        /*  pre-calc the scale tables  */
        for (x = 0; x < orig_width; x++) {
                double fl;

                tmpval = (x * width) / (float)orig_width;
                fl = floor (tmpval);
                x_dest_offsets [x] = (int)fl;
        }

        for (y = 0; y < orig_height; y++) {
                double fl;

                tmpval = (y * height) / (float)orig_height;
                fl = floor (tmpval);
                y_dest_offsets [y] = (int)fl;
        }
  
        /*  do the scaling  */
        last_dest_y = 0;
        row_bytes = orig_width;
        for (y = 0; y < orig_height; y++) {
                if (last_dest_y != y_dest_offsets[y]) {
                        /* only write row when y changes lines reached */
                        for (x = 0; x < width ; x++) {
                                dest[x] /= (float)line_weights[x];
                        }
                        image_set_row (destImagePtr, k, 0, last_dest_y, width, dest);
                        memset (dest, 0, width * bytes);
                        memset (line_weights, 0, width * bytes);
                }
                image_get_row (srcImagePtr, k, 0, y, orig_width, src, 1);
                for (x = 0; x < row_bytes ; x++) {
                        dest[x_dest_offsets[x]] += src[x];
                        line_weights[x_dest_offsets[x]]++;
                }
                last_dest_y = y_dest_offsets[y];
        }
        /* only write row when y changes lines reached */
        for (x = 0; x < width ; x++)
                dest[x] /= (float)line_weights[x];

        image_set_row (destImagePtr, k, 0, last_dest_y, width, dest);

        free (x_dest_offsets);
        free (y_dest_offsets);
        free (src);
        free (dest);
}

void
resize_image_sample_enlarge (FitsCutImage *srcImagePtr,
                             FitsCutImage *destImagePtr, 
                             int k)
{
        int *x_src_offsets;
        int *y_src_offsets;
        float *src;
        float *dest;
        int width, height, orig_width, orig_height;
        int last_src_y;
        int row_bytes;
        int x, y;
        char bytes;

        orig_width = srcImagePtr->ncols[k];
        orig_height = srcImagePtr->nrows[k];

        width = destImagePtr->ncols[k];
        height = destImagePtr->nrows[k];

        fitscut_message (2, "\tresizing channel to x=%d y=%d from x=%d y=%d\n",
                         width, height, orig_width, orig_height);
        bytes = sizeof (float);

        /*  the data pointers...  */
        x_src_offsets = (int *) malloc (width * bytes);
        y_src_offsets = (int *) malloc (height * bytes);
        src  = (float *) calloc (orig_width * bytes, 1);
        dest = (float *) calloc (width * bytes, 1);
  
        /*  pre-calc the scale tables  */
        for (x = 0; x < width; x++)
                x_src_offsets [x] = (x * orig_width + orig_width / 2) / width;

        for (y = 0; y < height; y++)
                y_src_offsets [y] = (y * orig_height + orig_height / 2) / height;
  
        /*  do the scaling  */
        row_bytes = width;
        last_src_y = -1;
        for (y = 0; y < height; y++) {
                /* if the source of this line was the same as the source
                 *  of the last line, there's no point in re-rescaling.
                 */
                if (y_src_offsets[y] != last_src_y) {
                        image_get_row (srcImagePtr, k, 0, y_src_offsets[y], orig_width, src, 1);
                        for (x = 0; x < row_bytes ; x++)
                                dest[x] = src[x_src_offsets[x]];

                        last_src_y = y_src_offsets[y];
                }

                image_set_row (destImagePtr, k, 0, y, width, dest);
        }
        free (x_src_offsets);
        free (y_src_offsets);
        free (src);
        free (dest);
}

void
image_get_row (FitsCutImage *PR, 
	       int         k,
	       int         x, 
	       int         y, 
	       int         w, 
	       float       *data, 
	       int         subsample)
{
        float *linep;
        float *row;
        int end;
        int npixels;
        int bytespp;

        end = x + w;
        npixels = w;
        bytespp = sizeof (float);

        row = PR->data[k] + y * PR->ncols[k];
        while (x < end) {
                if (subsample == 1) { /* optimize for the common case */
                        linep = row + x;
                        memcpy (data, linep, bytespp * npixels);
                        data += npixels;
                        x += npixels;
                }
        }
}

void
image_set_row (FitsCutImage *PR, 
	       int         k,
	       int         x, 
	       int         y, 
	       int         w, 
	       float       *data)
{
        float *linep;
        float *row;
        int end;
        int npixels;
        int bytespp;

        end = x + w;
        npixels = w;
        bytespp = sizeof (float);

        row = PR->data[k] + y * PR->ncols[k];
        while (x < end) {
                linep = row + x;
                memcpy (linep, data, bytespp * npixels);
                data += npixels;
                x += npixels;
        }
 
}
