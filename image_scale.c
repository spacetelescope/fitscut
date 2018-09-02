/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * Image scaling functions
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
 * $Id: image_scale.c,v 1.21 2004/04/30 18:28:12 mccannwj Exp $
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

#include <float.h>
#include <math.h>

#ifdef	STDC_HEADERS
#include <stdlib.h>
#else	/* Not STDC_HEADERS */
extern void exit ();
extern char *malloc ();
#endif	/* STDC_HEADERS */

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "fitscut.h"
#include "image_scale.h"
#include "histogram.h"
#include "extract.h"

void
autoscale_channel (FitsCutImage *Image, int k)
{
        float *hist; /* Histogram */
        int num_bins = NBINS;
        int ll, hh, count;
        long npix, cutoff_low, cutoff_high;
        double amin, amax;
        float *arrayp;
        float half;

        npix = Image->ncols[k] * Image->nrows[k];

        fitscut_message (1, "Autoscaling channel %d by histogram %.4f%% - %.4f%%\n", 
                         k, Image->autoscale_percent_low[k],
                         Image->autoscale_percent_high[k]);

        if (Image->data[k] == NULL) 
                return;
        arrayp = Image->data[k];
        amin = Image->data_min[k];
        amax = Image->data_max[k];
        /* get histogram */
        hist = compute_histogram (arrayp, num_bins, amin, amax,
                                  Image->nrows[k], Image->ncols[k]);

        /* calculate cutoff */
        half = (Image->autoscale_percent_low[k] == Image->autoscale_percent_high[k]) ? 0.5 : 1.0;
        cutoff_low = (npix * Image->autoscale_percent_low[k] / 100.0) * half;
        cutoff_high = (npix * (100.0 - Image->autoscale_percent_high[k]) / 100.0) * half;
        for (ll = 0, count = 0; ll < num_bins; ll++) {
                count += hist[ll];
                if (count > cutoff_low)
                        break;
        }

        for (hh = num_bins - 1, count = 0; hh >= 0; hh--) {
                count += hist[hh];
                if (count > cutoff_high)
                        break;
        }

        Image->autoscale_min[k] = (amax-amin) / num_bins * ll + amin;
        Image->autoscale_max[k] = (amax-amin) / num_bins * hh + amin;

        fitscut_message (3, "\t\tautoscale min: %f max: %f ll: %d hh: %d\n", 
                         Image->autoscale_min[k], Image->autoscale_max[k], ll, hh);

        free (hist);
}

void
autoscale_image (FitsCutImage *Image)
{
        int k;

        for (k = 0; k < Image->channels; k++) {
                if (Image->data[k] == NULL) 
                        continue;
                autoscale_channel (Image, k);
        }
        Image->autoscale_performed = TRUE;
}

void
histeq_image (FitsCutImage *Image)
{
        float *hist;
        unsigned char *lut;
        unsigned char temp;
        float *linep;
        long y, x, ind;
        float *src, *dest;
        float binsize;
        int num_bins = NBINS;
        int k;

        long nrows, ncols;
        double dmin, dmax;
        float *arrayp;

        for (k = 0; k < Image->channels; k++) {
                if (Image->data[k] == NULL) 
                        continue;
                arrayp = Image->data[k];
                dmin = Image->data_min[k];
                dmax = Image->data_max[k];
                nrows = Image->nrows[k];
                ncols = Image->ncols[k];

                fitscut_message (1, "Scaling channel %d (histeq)\n", k);

                /* This code is based on GIMP equalize */

                hist = compute_histogram (arrayp, num_bins, dmin, dmax, nrows, ncols);
                binsize = (dmax - dmin) / (num_bins - 1);

                /* Build equalization LUT */
                lut = eq_histogram (hist, num_bins, ncols, nrows);

                /* Substitute */
                linep  = arrayp;

                for (y = 0; y < nrows; y++) {
                        src  = linep;
                        dest = linep;

                        for (x = 0; x < ncols; x++) {
                                ind = ceil (((*src) - dmin) / binsize);
                                temp = lut[ind];
                                *dest = temp;
                                src++;
                                dest++;
                        }
      
                        linep += ncols;
                }
                free (hist);
                free (lut);

                /* reset the data min/max values */
                Image->data_min[k] = 0;
                Image->data_max[k] = 255;
        }
}

double
compute_hist_mode (FitsCutImage *Image, int k)
{
        float *hist;
        float *arrayp;
        int num_bins = NBINS;
        double amin, amax;
        int i, max_bin = 0;
        float depth;

        if (Image->data[k] == NULL) 
                return 0.0;

        arrayp = Image->data[k];
        amin = Image->data_min[k];
        amax = Image->data_max[k];
        /* get histogram */
        hist = compute_histogram (arrayp, num_bins, amin, amax, Image->nrows[k], Image->ncols[k]);

        for (i = 0, depth = 0; i < num_bins; i++) {
                if (hist[i] > depth) {
                        depth = hist[i];
                        max_bin = i;
                }
        }
        return (amax - amin) / num_bins * max_bin + amin;
}

void
log_image (FitsCutImage *Image)
{
        long i;
        float t;
        float threshold;
        float user_threshold;
        double maxval, minval;
        double user_maxval, user_minval;
        double linear_shift;
        int k;
        float *arrayp;

        for (k = 0; k < Image->channels; k++) {
                if (Image->data[k] == NULL) 
                        continue;
                arrayp = Image->data[k];
                maxval = Image->data_max[k];
                minval = Image->data_min[k];
                user_maxval = (Image->user_max_set == 1) ? Image->user_max[k] : maxval;
                user_minval = (Image->user_min_set == 1) ? Image->user_min[k] : minval;
                threshold = (float) maxval / 10000.0;
                user_threshold = (float) user_maxval / 10000.0;

                fitscut_message (1, "Scaling channel %d (log) min: %f max: %f\n", 
                                 k, minval, maxval);

                linear_shift = -1 * minval + 1;
                for (i=0; i < Image->nrows[k] * Image->ncols[k]; i++) {
                        /* MAX is a macro we have to be careful */
                        /* shift the data value to be greater than unity */
                        t = arrayp[i] + linear_shift;
                        /*arrayp[i] = log10( MAX(threshold,t) );*/
                        arrayp[i] = log10 (t);
                }
                Image->data_max[k] = log10 (maxval + linear_shift);
                Image->data_min[k] = 0; /* log10(1) */
                Image->user_max[k] = log10 (user_maxval + linear_shift);
                Image->user_min[k] = log10 (user_minval + linear_shift);
        }
}

void
sqrt_image (FitsCutImage *Image)
{
        long i;
        float t,tm;
        float threshold;
        double maxval, minval;
        double user_maxval, user_minval;
        int k;
        float *arrayp;

        for (k = 0; k < Image->channels; k++) {
                if (Image->data[k] == NULL) 
                        continue;
                arrayp = Image->data[k];
                maxval = Image->data_max[k];
                minval = Image->data_min[k];
                user_maxval = (Image->user_max_set == 1) ? Image->user_max[k] : maxval;
                user_minval = (Image->user_min_set == 1) ? Image->user_min[k] : minval;
                threshold = 0.0;

                fitscut_message (1, "Scaling channel %d (sqrt) min: %f max: %f\n", 
                                 k, minval, maxval);

                for (i = 0; i < Image->nrows[k] * Image->ncols[k]; i++) {
                        /* MAX is a macro we have to be careful */
                        t = arrayp[i] - user_minval;
                        arrayp[i] = sqrt (MAX (0,t) );
                }
                tm = maxval;
                Image->data_max[k] = sqrt (tm - user_minval);
                Image->data_min[k] = 0.0;
                Image->user_max[k] = sqrt (user_maxval - user_minval);
                Image->user_min[k] = 0.0;
        }
}

void
asinh_image (FitsCutImage *Image)
{
        long    i;
        float   t;
        double  minval;
        double  user_maxval, user_minval;
        int     k;
        float  *arrayp;
        float   nonlinearity = 3.0;

        double  weight;
        double *vals;
        double  sum, maxval, maxval_scaled;

        vals = (double *) calloc (Image->channels, sizeof (double));

        if (!Image->autoscale_performed &&
            !(Image->user_max_set && Image->user_min_set))
                autoscale_image (Image);

        fitscut_message (1, "Scaling image (asinh)...\n");

        for (i = 0; i < Image->nrows[0] * Image->ncols[0]; i++) {
                maxval = 0.0;
                sum    = 0.0;
                for (k = 0; k < Image->channels; k++) {
                        if (Image->data[k] == NULL) 
                                continue;

                        minval = 0.0;
                        user_maxval = (Image->user_max_set) ? Image->user_max[k] : Image->autoscale_max[k];
                        user_minval = (Image->user_min_set) ? Image->user_min[k] : Image->autoscale_min[k];
                        arrayp = Image->data[k];
                        t = (arrayp[i] - minval) / (user_maxval - user_minval);
                        vals[k] = t;
                        sum += t;
                        if (t > maxval)
                                maxval = t;
                }

                weight = asinh (sum * nonlinearity) / (nonlinearity * sum);
                for (k = 0; k < Image->channels; k++) {
                        if (Image->data[k] == NULL) 
                                continue;

                        arrayp = Image->data[k];
                        arrayp[i] = vals[k] * weight;
                        maxval_scaled = maxval * weight;
                        if (maxval_scaled > 1)
                                arrayp[i] /= maxval_scaled;
                }

        }

        for (k = 0; k < Image->channels; k++) {
                /* FIXME: these are bogus */
                Image->data_max[k] = 1.0;
                Image->data_min[k] = 0; /*Image->autoscale_min[k];*/
                Image->user_max[k] = 1.0;
                Image->user_min[k] = 0; /*Image->autoscale_min[k];*/
        }        

        free (vals);
}

void
scan_min_max (FitsCutImage *Image)
{
        long i;
        float fmaxval = FLT_MAX;
        float val;
        int k;
        double dmin, dmax;
        float *arrayp;

        fitscut_message (2, "Scanning file for scaling parameters...\n");

        for (k = 0; k < Image->channels; k++) {
                dmax = -fmaxval;
                dmin = fmaxval;
                if (Image->data[k] == NULL) 
                        continue;
                arrayp = Image->data[k];

                fitscut_message (2, "Scanning channel %d...", k);
                for (i = 0; i < Image->nrows[k] * Image->ncols[k]; i++) {
                        val = arrayp[i];
                        if (val > dmax)
                                dmax = val;
                        if (val < dmin)
                                dmin = val;
                }
                Image->data_min[k] = dmin;
                Image->data_max[k] = dmax;

                fitscut_message (2, "  min: %f max: %f\n",
                                 Image->data_min[k], Image->data_max[k]);
        }
}

void
mult_image (FitsCutImage *Image)
{
        long i;
        double user_scale_value;
        double maxval, minval;
        double user_maxval, user_minval;
        int k;
        float *arrayp;
        float max_max,min_min,max_usermax,min_usermin;

        max_max = -FLT_MAX;
        min_min = FLT_MAX;
        max_usermax = -FLT_MAX;
        min_usermin = FLT_MAX;

        for (k = 0; k < Image->channels; k++) {
                if (Image->data[k] == NULL) 
                        continue;
                arrayp = Image->data[k];
                maxval = Image->data_max[k];
                minval = Image->data_min[k];
                user_scale_value = Image->user_scale_factor[k];
                user_maxval = (Image->user_max_set == 1) ? Image->user_max[k] : maxval;
                user_minval = (Image->user_min_set == 1) ? Image->user_min[k] : minval;

                fitscut_message (1, "\tscaling channel %d by factor %f, min: %f max: %f\n", 
                                 k, user_scale_value, minval, maxval);

                for (i=0; i < Image->nrows[k] * Image->ncols[k]; i++) {
                        /* MAX is a macro we have to be careful */
                        arrayp[i] *= user_scale_value;
                }
                Image->data_max[k] = maxval * user_scale_value;
                Image->data_min[k] = minval * user_scale_value;
                Image->user_max[k] = user_maxval * user_scale_value;
                Image->user_min[k] = user_minval * user_scale_value;

                /* keep track of the overall maximum and minimum */
                max_max = MAX (max_max, Image->data_max[k]);
                min_min = MIN (min_min, Image->data_min[k]);
                max_usermax = MAX (max_usermax, Image->user_max[k]);
                min_usermin = MIN (min_usermin, Image->user_min[k]);
        }
        /* apply the outer boundary of the min/max pairs to each channel 
         * so that we don't take out the scaling when we scale to a byte */
        fitscut_message (1, "using data min: %f max: %f\n", min_min, max_max);
        fitscut_message (1, "using user min: %f max: %f\n", min_usermin, max_usermax);

        for (k = 0; k < Image->channels; k++) {
                Image->data_max[k] = max_max;
                Image->data_min[k] = min_min;
                Image->user_max[k] = max_usermax;
                Image->user_min[k] = min_usermin;
        }
}

void
rate_image (FitsCutImage *Image)
{
        int k;
        double exptime;
        double factor = 1.0;

        for (k = 0; k < Image->channels; k++) {
                if (Image->data[k] == NULL) 
                        continue;    
                /* get the exposure time and populate the user_scale_factor field */
                exptime = fits_get_exposure_time (Image->header[k], Image->header_cards[0]);
                if (exptime > 0) {
                        factor = 1.0 / exptime;
                        Image->user_scale_factor[k] = factor;
                }
                else {
                        fitscut_message (1, "Warning: EXPTIME not found, using 1.0\n");
                        Image->user_scale_factor[k] = 1.0;
                }
        }
        mult_image (Image);
}

