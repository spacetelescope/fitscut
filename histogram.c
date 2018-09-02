/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * Histogram functions
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
 * $Id: histogram.c,v 1.7 2004/04/30 18:28:12 mccannwj Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include "getopt.h"
#include <signal.h>
#include <ctype.h>

#include <math.h>
#include <float.h>

#ifdef  STDC_HEADERS
#include <stdlib.h>
#else   /* Not STDC_HEADERS */
extern void exit ();
extern char *malloc ();
#endif  /* STDC_HEADERS */
                                                                                
#include "fitscut.h"
#include "histogram.h"

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

float *
compute_histogram (float *arrayp, int length, float dmin, float dmax, long nrows, long ncols)
{
        float *hist;
        float  binsize;
        float *src;
        long   y, x, ind;
        float  value;

        /* allocate histogram */
        hist = (float*) malloc (sizeof (float) * (length + 1));

        /* Initialize histogram */
        for (x = 0; x < length; x++)
                hist[x] = 0;
  
        /* Build histogram */
        fitscut_message (3, "\tbuilding histogram...\n");
  
        binsize = (dmax - dmin) / (length - 1.0);
        fitscut_message (4, "\tdmax: %f dmin: %f length: %d binsize: %f ncols: %ld nrows: %ld...\n",
                         dmax, dmin, length, binsize, ncols, nrows);
  
        for (y = 0; y < nrows; y++) {
                for (x = 0; x < ncols; x++) {
                        src = arrayp + y*ncols + x;
                        value = *src;
                        if (value < dmin)
                                ind = 0;
                        else if (value > dmax)
                                ind = length-1;
                        else
                                ind = ceil ((value-dmin) / binsize);
                        hist[ind] += 1.0;
                }
        }

        return (hist);
}

unsigned char *
eq_histogram (float *hist, int length, long width, long height)
{
        int i, j;
        int *part;
        unsigned char *lut;
        double pixels_per_value;
        double desired;
        double sum, dif;

        /* allocate partition */
        part = (int*) malloc (sizeof (int) * 257);
        /* allocate look up table */
        lut = (unsigned char*) malloc (sizeof (unsigned char) * (length + 1));

        /* Find partition points */
        pixels_per_value = (double) (width * height) / 256.0;

        /* First and last points in partition */
        part[0] = 0;
        part[256] = 256;

        /* Find intermediate points */
        j = 0;
        sum = hist[0] + hist[1];
        for (i = 1; i < 256; i++) {
                desired = i * pixels_per_value;
                while (sum <= desired) {
                        j++;
                        sum += hist[j+1];
                }

                /* Nearest sum */
                dif = sum - hist[j];
                if ((sum - desired) > (dif / 2.0))
                        part[i] = j;
                else
                        part[i] = j + 1;
        }

        /* Create equalization LUT */
        for (j = 0; j < length; j++) {
                i = 0;

                while (part[i + 1] <= j)
                        i++;
                if (i > 255)
                        i = 255;

                lut[j] = i;
        }

        free (part);
        return (lut);
}
