/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * JPEG output functions
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
 * $Id: output_jpg.c,v 1.5 2004/04/30 18:28:13 mccannwj Exp $
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

#include <jpeglib.h>
#include <float.h>
#include <math.h>

#ifdef  STDC_HEADERS
#include <stdlib.h>
#else   /* Not STDC_HEADERS */
extern void exit ();
extern char *malloc ();
#endif  /* STDC_HEADERS */
                                                                                
#ifdef  HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
                                                                                
#include "fitscut.h"
#include "output_jpg.h"
#include "image_scale.h"
#include "revision.h"

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

static void
jpg_create_mean_green (JSAMPLE *line, int ncols)
{
        long col;
        int skip = 1;
        int stride = 3;
        unsigned char *pp;
        float temp_val;

        pp = line + skip;

        for (col=0; col < ncols; col++) {
                temp_val = (*(pp - 1) + *(pp + 1)) / 2;
                *pp = (unsigned char) (temp_val);
                pp += stride;
        }
}

static void
jpg_scale_row_linear (float *arrayp,
                      JSAMPLE *line,
                      int skip,
                      int stride,
                      long ncols,
                      float scale,
                      float minval,
                      float maxval,
                      float clip_val,
                      int invert)
{
        long col;
        float t,tx;
        unsigned char *pp;
        int do_scale = 1;

        if ((minval == 0) && (maxval == clip_val))
                do_scale = 0;

        pp = line + skip;
        for (col = 0; col < ncols; col++) {
                if (do_scale) {
                        /*t = scale * ( val * bscale + bzer - datamin );*/
                        t = scale * (arrayp[col] - minval);
                        tx = MIN (t, clip_val);
                        t = MAX (0, tx);
                        /*fprintf(stderr, " val: %f ", t);*/
                }
                else {
                        t = arrayp[col];
                }
                if (invert) {
                        t = clip_val - t;
                }
                *pp = (unsigned char)t;
                /*fprintf(stderr,"t: %d\n", *pp);*/
                pp += stride;
        }
}

static void
jpg_write_rgb_image (struct jpeg_compress_struct *cinfo_ptr, FitsCutImage *Image, int bit_depth)
{
        float scale[MAX_CHANNELS];
        int row, k;
        double *datamin, *datamax, clip_val;
        int line_len;
        unsigned char *line;
        int mean_green = 0;
        JSAMPROW row_pointer[1];      /* pointer to JSAMPLE row[s] */

        clip_val = pow (2.0,bit_depth)-1;

        switch (Image->output_scale_mode) {
        case SCALE_MODE_AUTO:
                datamin = Image->autoscale_min;
                datamax = Image->autoscale_max;
                /* check for user overrides */
                if (Image->user_min_set == 1) {
                        fitscut_message (1, "user min takes precedent over auto scale min\n");
                        datamin = Image->user_min;
                }
                if (Image->user_max_set == 1) {
                        fitscut_message (1, "user max takes precedent over auto scale max\n");
                        datamax = Image->user_max;
                }
                break;
    
                break;
        case SCALE_MODE_USER:
                if (Image->user_min_set == 1)
                        datamin = Image->user_min;
                else
                        datamin = Image->data_min;
                if (Image->user_max_set == 1)
                        datamax = Image->user_max;
                else
                        datamax = Image->data_max;
                break;
        case SCALE_MODE_MINMAX:
        default:
                datamin = Image->data_min;
                datamax = Image->data_max;
                break;
        }

        for (k = 0; k < Image->channels; k++) {
                if (Image->data[k] == NULL) 
                        continue;
                if ( datamin[k] < datamax[k] )
                        scale[k] = clip_val / (datamax[k] - datamin[k]);
                else
                        scale[k] = 1.0;

                fitscut_message (2, "\tchannel %d data min: %f max: %f clip: %f scale: %f\n",
                                 k, datamin[k], datamax[k], clip_val, scale[k]);
        }

        if (Image->data[0] != NULL &&
            Image->data[1] == NULL &&
            Image->data[2] != NULL) {
                fitscut_message (1, "creating a green channel from mean of red and blue\n");
                mean_green = 1;
        }
  
        line_len = Image->ncols[0] * (bit_depth / 8) * Image->channels;
        line = (unsigned char *) calloc (line_len, 1);
        for (row = Image->nrows[0] - 1; row >= 0; row--) {
                for (k = 0; k < Image->channels; k++) {
                        if (Image->data[k] == NULL) 
                                continue;
                        jpg_scale_row_linear (Image->data[k] + row * Image->ncols[k],
                                              line, k,
                                              Image->channels, Image->ncols[k],
                                              scale[k], datamin[k], datamax[k],
                                              clip_val, Image->output_invert);
                }
                if (mean_green == 1)
                        jpg_create_mean_green (line, Image->ncols[0]);
                row_pointer[0] = line;
                jpeg_write_scanlines (cinfo_ptr, (JSAMPARRAY) row_pointer, 1);
        }
        free (line);
}

static void
jpg_write_simple_image (struct jpeg_compress_struct *cinfo_ptr, FitsCutImage *Image, int bit_depth)
{
        float scale = 1.0;
        int row;
        double datamin, datamax, clip_val;
        unsigned char *line;
        JSAMPROW row_pointer[1];      /* pointer to JSAMPLE row[s] */
  
        clip_val = pow (2.0,bit_depth)-1;

        switch (Image->output_scale_mode) {
        case SCALE_MODE_AUTO:
                datamin = Image->autoscale_min[0];
                datamax = Image->autoscale_max[0];
                break;
        case SCALE_MODE_USER:
                if (Image->user_min_set == 1)
                        datamin = Image->user_min[0];
                else
                        datamin = Image->data_min[0];
                if (Image->user_max_set == 1)
                        datamax = Image->user_max[0];
                else
                        datamax = Image->data_max[0];
                break;
        case SCALE_MODE_MINMAX:
        default:
                datamin = Image->data_min[0];
                datamax = Image->data_max[0];
                break;
        }
  
        /* Just in case we have a file with constant value. */
        if (datamin < datamax)
                scale = clip_val / (datamax - datamin);
  
        fitscut_message (2, "\tdata min: %f max: %f clip: %f scale: %f\n",
                         datamin, datamax, clip_val, scale);
  
        if ((line = (unsigned char *) malloc (Image->ncols[0] * bit_depth / 8)) == NULL)
                fitscut_error ("out of memory allocating JPEG row buffer");

        for (row = Image->nrows[0]-1; row >= 0; row--) {

                /*fitscut_message (10, "\trow %d of %ld - scaling", row, Image->nrows[0]);*/

                jpg_scale_row_linear (Image->data[0] + row * Image->ncols[0],
                                      line, 0, 1, Image->ncols[0], scale,
                                      datamin, datamax, clip_val,
                                      Image->output_invert);

                /*fitscut_message (10, "\twriting\n");*/

                row_pointer[0] = line;
                jpeg_write_scanlines (cinfo_ptr, (JSAMPARRAY) row_pointer, 1);
        }
        free (line);
}

int
write_to_jpg (FitsCutImage *Image)
{
        FILE *outfile;
        long width, height;
        int bit_depth, num_palette;
        char comment_text[64];

        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;

        cinfo.err = jpeg_std_error (&jerr);
        jpeg_create_compress (&cinfo);

        width = Image->ncols[0];
        height = Image->nrows[0];
        bit_depth = 8;

        if (to_stdout)
                outfile = stdout;
        else
                outfile = fopen (Image->output_filename, "wb");

        if (!outfile)
                return ERROR;

        jpeg_stdio_dest (&cinfo, outfile);

        cinfo.image_width = width;      /* image width and height, in pixels */
        cinfo.image_height = height;

        if (Image->channels == 1) {
                num_palette = 256;
                switch (Image->output_colormap) {
                case CMAP_RED:
                case CMAP_GREEN:
                case CMAP_BLUE:
                case CMAP_RAINBOW:
                case CMAP_COOL:
                case CMAP_HEAT:
                        break;
                case CMAP_GRAY:
                default:
                        break;
                }

                fitscut_message (1, "\t\tusing GRAYSCALE color type\n");
                cinfo.input_components = 1;     /* # of color components per pixel */
                cinfo.in_color_space = JCS_GRAYSCALE;
        }
        else {
                fitscut_message (1, "\t\tusing RGB color type\n");
                cinfo.input_components = 3;     /* # of color components per pixel */
                cinfo.in_color_space = JCS_RGB; /* colorspace of input image */
        }

        jpeg_set_defaults (&cinfo);

        jpeg_start_compress (&cinfo, TRUE);
        sprintf (comment_text, "Created by fitscut %s (William Jon McCann)", VERSION);
        jpeg_write_marker (&cinfo, JPEG_COM, comment_text, strlen (comment_text));

        if (Image->channels == 1)
                jpg_write_simple_image (&cinfo, Image, bit_depth);
        else
                jpg_write_rgb_image (&cinfo, Image, bit_depth);

        fitscut_message (2, "finished writing JPEG image\n");

        jpeg_finish_compress (&cinfo);
        jpeg_destroy_compress (&cinfo);
        fflush (stdout);

        return (OK);
}
