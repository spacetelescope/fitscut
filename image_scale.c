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
#include <stdlib.h>
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
#include "image_scale.h"
#include "histogram.h"
#include "extract.h"

void
autoscale_image (FitsCutImage *Image)
{
        int k;

        if (Image->autoscale_performed) return;

        for (k = 0; k < Image->channels; k++) {
                if (Image->data[k] == NULL) 
                        continue;
                switch(Image->output_scale_mode) {
                    case SCALE_MODE_AUTO:
                        autoscale_channel (Image, k);
                        break;
                    case SCALE_MODE_FULL:
                        autoscale_full_channel (Image, k);
                        break;
                    default:
                        break;
                }
        }
        Image->autoscale_performed = TRUE;
}

static void
autoscale_range_set (FitsCutImage *Image, int k, float *arrayp, int npix)
{
        float *hist; /* Histogram */
        float inmin, inmax;
        int num_bins = NBINS;
        int ll, hh, mm, count;
        long cutoff_low, cutoff_high, cutoff_median;
        double amin, amax, median;
        long pixcount;
        float bad_data_value;

        amin = Image->data_min[k];
        amax = Image->data_max[k];
        bad_data_value = Image->bad_data_value[k];
        /* get histogram */
        hist = compute_histogram (arrayp, num_bins, amin, amax, npix, bad_data_value, &pixcount, &inmin, &inmax);

        /* find the upper cutoff */
        cutoff_high = (pixcount * (100.0 - Image->autoscale_percent_high[k]) / 100.0);
        for (hh = num_bins, count = 0; hh >= 0; hh--) {
                count += hist[hh];
                if (count > cutoff_high)
                        break;
        }

        /* calculate lower cutoff */
        cutoff_low = (pixcount * Image->autoscale_percent_low[k] / 100.0);
        for (ll = 0, count = 0; ll <= num_bins; ll++) {
                count += hist[ll];
                if (count > cutoff_low)
                        break;
        }

        /* find the median */
        if (hh == ll) {
            mm = ll;
        } else {
            /* continue from lower count */
            cutoff_median = pixcount * 0.5;
            mm = ll;
            while (count <= cutoff_median) {
                mm++;
                if (mm > num_bins) break;
                count += hist[mm];
            }
        }

        /* minimize round-off errors in case abs(amax) << abs(amin) */
        if (ll == num_bins) {
            Image->autoscale_min[k] = amax;
        } else {
            Image->autoscale_min[k] = (amax-amin) / (num_bins-1) * (ll-1) + amin;
        }
        if (hh == num_bins-1) {
            Image->autoscale_max[k] = amax;
        } else {
            Image->autoscale_max[k] = (amax-amin) / (num_bins-1) * hh + amin;
        }
        median = (amax-amin) / (num_bins-1) * (mm-0.5) + amin;

        if (ll+10 >= hh && inmin != inmax) {
            /*
             * Range was too big -- repeat with restricted range.
             * This is often an indication of bad data in the image, so a
             * warning is printed.
             */
            Image->data_min[k] = MAX(inmin, Image->autoscale_min[k]);
            Image->data_max[k] = MIN(inmax, Image->autoscale_max[k]);
            fitscut_message (1, "\tPossible bad data in channel %d - Iterating autoscale min: %e max: %e\n",
                             k, Image->data_min[k], Image->data_max[k]);
            free (hist);
            autoscale_range_set(Image, k, arrayp, npix);
            return;
        }

        /*
         * Fix a common problem: when the image looks almost like pure noise
         * (max-median ~= median-min), the stretch is too severe.
         * In that case move the max value away from the median.
         */
        if (median + 5*(median-Image->autoscale_min[k]) > Image->autoscale_max[k]) {
            fitscut_message (2, "\tautoscale before median corr min: %f max: %f ll: %d hh: %d\n", 
                         Image->autoscale_min[k], Image->autoscale_max[k], ll, hh);
            Image->autoscale_max[k] = median + 5*(median-Image->autoscale_min[k]);
        }
        fitscut_message (2, "\tautoscale min: %f max: %f ll: %d hh: %d\n", 
                         Image->autoscale_min[k], Image->autoscale_max[k], ll, hh);
        free (hist);
}


void
autoscale_channel (FitsCutImage *Image, int k)
{
        long npix;

        fitscut_message (1, "Autoscaling channel %d by histogram %.4f%% - %.4f%%\n", 
                         k, Image->autoscale_percent_low[k],
                         Image->autoscale_percent_high[k]);

        npix = Image->ncols[k] * Image->nrows[k];
        autoscale_range_set (Image, k, Image->data[k], npix);
}

/* autoscaling using a sample of the full image
 * This produces the same lookup table for every cutout from an image
 * (useful for making tiles that match.) 
 */

void
autoscale_full_channel (FitsCutImage *Image, int k)
{
        long npix;
        double amin, amax;
        float *arrayp, val;

        fitsfile *fptr;       /* pointer to the FITS file; defined in fitsio.h */
        fitsfile *dqptr;
        long nplanes;
        int status, ydelta, i, nbad;
        long naxes[2];
        int rows_used = 0;
        int cols_used = 0;
        /* allow room for trailing dimensions */
        long fpixel[7] = {1,1,1,1,1,1,1};
        long lpixel[7] = {1,1,1,1,1,1,1};
        long inc[7] = {1,1,1,1,1,1,1};
        int anynull;
        float nullval = NAN;
        int nsample = NSAMPLE;
        float minfrac;

        int useBsoften;
        double boffset, bsoften;

        /*
         * read a sample of rows from the image
         * we've already read the cutout from this image, but need to open
         * it again to read some scattered rows
         */

        status = 0;
        fitscut_message (1, "Sampling FITS channel %d...\n", k);
        if (fits_open_image (&fptr, Image->input_filename[k], READONLY, &status)) 
                printerror (status);

        /* get image dimensions */
        if (fits_get_img_size (fptr, 2, naxes, &status))
                printerror (status);

        /* increase number of pixels to sample for very small fractions */
        minfrac = 1 - Image->autoscale_percent_high[k]/100;
        if (minfrac > Image->autoscale_percent_low[k]/100) {
            minfrac = Image->autoscale_percent_low[k]/100;
        }
        if (minfrac*nsample < 20) {
            nsample = 20/minfrac + 0.5;
            fitscut_message (1, "Increasing nsample from %d to %d...\n", NSAMPLE, nsample);
        }

        /* number of rows to read to get approximately nsample pixels */
        cols_used = naxes[0];
        rows_used = nsample/cols_used;
        if (rows_used < MINSAMPLEROWS) rows_used = MINSAMPLEROWS;
        if (rows_used > naxes[1]) rows_used = naxes[1];
        ydelta = naxes[1]/(rows_used+1);
        if (ydelta <= 0) ydelta = 1;
        arrayp = cutout_alloc (cols_used, rows_used, NAN);

        fpixel[0] = 1;
        fpixel[1] = ydelta;
        inc[0] = 1;
        inc[1] = ydelta;
        lpixel[0] = naxes[0];
        lpixel[1] = fpixel[1]+ydelta*(rows_used-1);

        if (fitscut_read_subset (fptr, TFLOAT, fpixel, lpixel, inc,
                &nullval, arrayp, &anynull, &status))
            printerror (status);

        /* get info for data quality flagging (if it is used) */
        if (get_qual_info (&dqptr, &nplanes, &Image->badmin[k], &Image->badmax[k], &Image->bad_data_value[k],
                fptr, Image->header[k], Image->header_cards[k],
                Image->qext_set, Image->qext[k], Image->useBadpix,
                &status))
            printerror (status);

        /* apply data quality flagging to zero bad pixels */

        nbad = apply_qual (dqptr, nplanes, Image->badmin[k], Image->badmax[k], Image->bad_data_value[k],
                fpixel, lpixel, inc,
                arrayp, anynull, Image->qext_bad_value[k], &status);
        if (status)
            printerror (status);
        if (nbad)
            fitscut_message(2, "\tZeroed %d bad pixels\n", nbad);

        /* invert asinh scaling using header parameters if requested */
        fits_get_bsoften (Image, k, &useBsoften, &bsoften, &boffset);
        if (useBsoften) {
            invert_bsoften(bsoften, boffset, fpixel, lpixel, inc,
                arrayp, Image->bad_data_value[k]);
        }

        if (fits_close_file (fptr, &status)) 
            printerror (status);

        if (dqptr != NULL) {
            if (fits_close_file (dqptr, &status)) 
                printerror (status);
        }

        fitscut_message (1, "Full autoscaling channel %d by histogram %.4f%% - %.4f%%\n", 
                         k, Image->autoscale_percent_low[k],
                         Image->autoscale_percent_high[k]);

        npix = rows_used*cols_used;

        /* get the min and max for the sample */
        amin = FLT_MAX;
        amax = -FLT_MAX;
        /* initialize with first finite value */
        for (i=0; i<npix; i++) {
            val = arrayp[i];
            if (finite(val) && val != Image->bad_data_value[k]) {
                amax = val;
                amin = val;
                break;
            }
        }
        for (i=i+1; i<npix; i++) {
            val = arrayp[i];
            if (finite(val) && val != Image->bad_data_value[k]) {
                if (val>amax) {
                    amax = val;
                } else if (val<amin) {
                    amin = val;
                }
            }
        }
        if (amin > amax) {
            /* apparently the entire section is blank, use zeros for limits */
            amin = 0.0;
            amax = 0.0;
        }
        fitscut_message (2, "\twhole image min %f max %f\n", amin, amax);
        /* use these max/min values for future scaling */
        Image->data_max[k] = amax;
        Image->data_min[k] = amin;

        autoscale_range_set (Image, k, arrayp, npix);
        free (arrayp);
}

void
histeq_image (FitsCutImage *Image)
{
        float *hist;
        float inmin, inmax;
        unsigned char *lut;
        unsigned char temp;
        float *linep;
        long y, x, ind;
        float *src, *dest;
        float binsize;
        int num_bins = NBINS;
        int k;
        float bad_data_value;

        long nrows, ncols, pixcount;
        double dmin, dmax;
        float *arrayp;

        for (k = 0; k < Image->channels; k++) {
                if (Image->data[k] == NULL) 
                        continue;
                arrayp = Image->data[k];
                dmin = Image->data_min[k];
                dmax = Image->data_max[k];
                bad_data_value = Image->bad_data_value[k];
                nrows = Image->nrows[k];
                ncols = Image->ncols[k];

                fitscut_message (1, "Scaling channel %d (histeq)\n", k);

                /* This code is based on GIMP equalize */

                hist = compute_histogram (arrayp, num_bins, dmin, dmax, nrows*ncols, bad_data_value, &pixcount, &inmin, &inmax);
                binsize = (dmax - dmin) / (num_bins - 1);

                /* Build equalization LUT */
                lut = eq_histogram (hist, num_bins, pixcount);

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
        float inmin, inmax;
        float *arrayp;
        int num_bins = NBINS;
        double amin, amax;
        int i, max_bin = 0;
        float depth;
        long pixcount;
        float bad_data_value;

        if (Image->data[k] == NULL) 
                return 0.0;

        arrayp = Image->data[k];
        amin = Image->data_min[k];
        amax = Image->data_max[k];
        bad_data_value = Image->bad_data_value[k];
        /* get histogram */
        hist = compute_histogram (arrayp, num_bins, amin, amax, Image->nrows[k]*Image->ncols[k], bad_data_value, &pixcount, &inmin, &inmax);

        for (i = 0, depth = 0; i <= num_bins; i++) {
                if (hist[i] > depth) {
                        depth = hist[i];
                        max_bin = i;
                }
        }
        return (amax - amin) / (num_bins-1) * (max_bin-0.5) + amin;
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
        double  minval[MAX_CHANNELS];
        double  *user_maxval, *user_minval;
        int     k, chancount;
        float  *arrayp;
        float   nonlinearity = 3.0;

        double  weight;
        double *vals;
        double  sum, maxval, maxval_scaled, data_max;
        long blankcount = 0;

        vals = (double *) calloc (Image->channels, sizeof (double));

        if (!Image->autoscale_performed &&
            !(Image->user_max_set && Image->user_min_set))
                autoscale_image (Image);

        fitscut_message (1, "Scaling image (asinh)...\n");
        user_maxval = (Image->user_max_set) ? Image->user_max : Image->autoscale_max;
        user_minval = (Image->user_min_set) ? Image->user_min : Image->autoscale_min;
        /*
         * Use min value as the base level.  This handles the case where there is
         * a large non-zero sky background (either positive or negative.)
         */
        for (k = 0; k < Image->channels; k++) {
            minval[k] = user_minval[k];
        }

        for (i = 0; i < Image->nrowsref * Image->ncolsref; i++) {
                maxval = 0.0;
                sum    = 0.0;
                chancount = 0;
                for (k = 0; k < Image->channels; k++) {
                        if (Image->data[k] == NULL) continue;
                        arrayp = Image->data[k];
                        if (finite(arrayp[i]) && arrayp[i]!=Image->bad_data_value[k]) {
                            t = (arrayp[i] - minval[k]) / (user_maxval[k] - user_minval[k]);
                            sum += t;
                            if (t > maxval) maxval = t;
                            vals[k] = t;
                            chancount += 1;
                        } else {
                            /* mark blank pixels with zero */
                            vals[k] = 0.0;
                        }
                }
                if (chancount == 0) {
                    blankcount++;
                    weight = NAN;
                } else if (sum != 0) {
                    weight = asinh (sum * nonlinearity) / (nonlinearity * sum);
                } else {
                    weight = 1.0;
                }
                maxval_scaled = maxval * weight;
                if (maxval_scaled > 1) weight /= maxval_scaled;
                for (k = 0; k < Image->channels; k++) {
                        if (Image->data[k] == NULL) continue;
                        arrayp = Image->data[k];
                        arrayp[i] = vals[k] * weight;
                }
        }

        fitscut_message (2, "Found %d blank pixels...\n", blankcount);

        /* empirical mapping to get comparable contrast */

        chancount = 0;
        for (k = 0; k < Image->channels; k++) {
            if (Image->data[k] != NULL) chancount++;
        }
        if (chancount >= 3) {
            data_max = 0.33;
        } else if (chancount == 2) {
            data_max = 0.4;
        } else {
            data_max = 0.7;
        }

        for (k = 0; k < Image->channels; k++) {
                Image->data_max[k] = data_max;
                Image->data_min[k] = 0.0;
                Image->user_max[k] = data_max;
                Image->user_min[k] = 0.0;
                Image->autoscale_max[k] = data_max;
                Image->autoscale_min[k] = 0.0;
        }        
        free (vals);
}

void
scan_min_max (FitsCutImage *Image)
{
        long i, npix;
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

                /* initialize with first finite value */
                npix = Image->nrows[k] * Image->ncols[k];
                for (i=0; i<npix; i++) {
                    val = arrayp[i];
                    if (finite(val) && val != Image->bad_data_value[k]) {
                        dmax = val;
                        dmin = val;
                        break;
                    }
                }
                for (i=i+1; i<npix; i++) {
                    val = arrayp[i];
                    if (finite(val) && val != Image->bad_data_value[k]) {
                        if (val>dmax) {
                            dmax = val;
                        } else if (val<dmin) {
                            dmin = val;
                        }
                    }
                }
                if (dmin > dmax) {
                    /* apparently the entire section is blank, use zeros for limits */
                    dmin = 0.0;
                    dmax = 0.0;
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
                exptime = fits_get_exposure_time (Image->header[k], Image->header_cards[k]);
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
