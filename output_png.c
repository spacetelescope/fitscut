/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * PNG output functions
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
 * $Id: output_png.c,v 1.21 2006/10/31 20:49:37 mccannwj Exp $
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

#include "png.h"    /* includes zlib.h and setjmp.h */
#include <float.h>
#include <math.h>

#include "fitscut.h"
#include "output_png.h"
#include "image_scale.h"
#include "revision.h"

#include "colormap.h"

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

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

#define PNG_NUM_TEXT 1

static void
png_create_mean_green (png_byte *line, int ncols)
{
        long col;
        int skip = 1;
        int stride = 3;
        png_byte *pp;
        float temp_val;

        pp = line+skip;

        for (col = 0; col < ncols; col++) {
                temp_val = (*(pp - 1) + *(pp + 1)) / 2;
                *pp = (png_byte) (temp_val);
                pp += stride;
        }
}

static void
png_scale_row_linear (float *arrayp,
                      png_byte *line,
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
        png_byte *pp;
        int do_scale = 1;

        if ( (minval == 0) && (maxval == clip_val) )
                do_scale = 0;

        pp = line + skip;
        for (col = 0; col < ncols; col++) {
                if (do_scale) {
                        /*t = scale * ( val * bscale + bzer - datamin );*/
                        t = scale * (arrayp[col] - minval);
                        tx = MIN (t, clip_val);
                        t = MAX (0,tx);
                        /*fprintf(stderr, " val: %f ", t);*/
                }
                else {
                        t = arrayp[col];
                }
                if (invert)
                        t = clip_val - t;

                *pp = (png_byte) t;
                pp += stride;
        }
}

static void
png_write_rgb_image (png_struct *png_ptr, FitsCutImage *Image, int bit_depth)
{
        float scale[MAX_CHANNELS];
        int row, k;
        double *datamin, *datamax, clip_val;
        int line_len;
        png_byte *line;
        int mean_green = 0;

        clip_val = pow (2.0,bit_depth) - 1;

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

                fitscut_message (1, "\tchannel %d data min: %f max: %f clip: %f scale: %f\n",
                                 k, datamin[k], datamax[k], clip_val, scale[k]);
        }

        if (Image->data[0] != NULL &&
            Image->data[1] == NULL &&
            Image->data[2] != NULL) {
                fitscut_message (1, "creating a green channel from mean of red and blue\n");
                mean_green = 1;
        }
  
        line_len = Image->ncols[0] * (bit_depth / 8) * Image->channels;
        line = (png_byte *) calloc (line_len, 1);
        for (row = Image->nrows[0] - 1; row >= 0; row--) {
                for (k = 0; k < Image->channels; k++) {
                        if (Image->data[k] == NULL) 
                                continue;
                        png_scale_row_linear (Image->data[k] + row * Image->ncols[k],
                                              line, k, 
                                              Image->channels, Image->ncols[k],
                                              scale[k], datamin[k], datamax[k],
                                              clip_val, Image->output_invert);
                }
                if (mean_green == 1) {
                        png_create_mean_green (line, Image->ncols[0]);
                }
                png_write_row (png_ptr, line);
        }
        free (line);
}

static void
png_write_simple_image (png_struct *png_ptr, FitsCutImage *Image, int bit_depth)
{
        float scale = 1.0;
        int row;
        double datamin, datamax, clip_val;
        png_byte *line;
  
        clip_val = pow (2.0, bit_depth) - 1;

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
        if ( datamin < datamax )
                scale = clip_val / (datamax - datamin );
  
        fitscut_message (2, "\tdata min: %f max: %f clip: %f scale: %f\n",
                         datamin, datamax, clip_val, scale);
  
        if ((line = (png_byte *) malloc (Image->ncols[0] * bit_depth / 8)) == NULL)
                fitscut_error ("out of memory allocating PNG row buffer");

        for (row = Image->nrows[0] - 1; row >= 0; row--) {
                png_scale_row_linear (Image->data[0] + row * Image->ncols[0],
                                      line, 0, 1, Image->ncols[0], scale,
                                      datamin, datamax, clip_val,
                                      Image->output_invert);
                png_write_row (png_ptr, line);
        }
        free (line);
}

int
write_to_png (FitsCutImage *Image)
{
        FILE *fp;
        long width, height;
        int bit_depth, color_type, num_palette = 256;
        char comment_text[64];
        png_color *palette = NULL;

        png_struct *png_ptr;
        png_info *info_ptr;
        png_text text_ptr[PNG_NUM_TEXT];

        width = Image->ncols[0];
        height = Image->nrows[0];
        bit_depth = 8;

        if (to_stdout)
                fp = stdout;
        else
                fp = fopen(Image->output_filename,"wb");

        if (!fp)
                return ERROR;

        png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (png_ptr == NULL)
                fitscut_error ("cannot allocate LIBPNG structure");

        info_ptr = png_create_info_struct (png_ptr);
        if (info_ptr == NULL) {
                png_destroy_write_struct (&png_ptr, (png_infopp)NULL);
                fitscut_error ("cannot allocate LIBPNG structures");
        }

        if (setjmp (png_jmpbuf (png_ptr))) {
                png_destroy_write_struct (&png_ptr, &info_ptr);
                fitscut_error ("setjmp returns error condition (1)");
        }

        png_init_io (png_ptr, fp);

        if (Image->channels == 1) {
                num_palette = 256;
                switch (Image->output_colormap) {
                case CMAP_RED:
                case CMAP_GREEN:
                case CMAP_BLUE:
                case CMAP_RAINBOW:
                case CMAP_COOL:
                case CMAP_HEAT:
                        color_type = PNG_COLOR_TYPE_PALETTE;
                        palette = get_png_palette (Image->output_colormap, num_palette);
                        break;
                case CMAP_GRAY:
                default:
                        color_type = PNG_COLOR_TYPE_GRAY;
                }
        }
        else {
                fitscut_message (2, "\t\tusing RGB color type\n");
                color_type = PNG_COLOR_TYPE_RGB;
        }

        /* Set the image information here.  Width and height are up to 2^31,
         * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
         * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
         * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
         * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
         * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
         * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
         */
        png_set_IHDR (png_ptr, info_ptr, width, height, bit_depth, color_type,
                      PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        if (color_type == PNG_COLOR_TYPE_PALETTE)
                png_set_PLTE (png_ptr, info_ptr, palette, num_palette);

        /* write comments into the image */
        text_ptr[0].key = "Software";
        sprintf (comment_text, "Created by fitscut %s (William Jon McCann)", VERSION);
        text_ptr[0].text = strdup (comment_text);
        text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
#ifdef PNG_iTXt_SUPPORTED
        text_ptr[0].lang = NULL;
#endif
        png_set_text (png_ptr, info_ptr, text_ptr, PNG_NUM_TEXT);

        /* write the png-info struct */
        png_write_info (png_ptr, info_ptr);

        if (Image->channels == 1)
                png_write_simple_image (png_ptr, Image, bit_depth);
        else
                png_write_rgb_image (png_ptr, Image, bit_depth);

        fitscut_message (2, "finished writing PNG image\n");

        png_write_end (png_ptr, info_ptr);
        png_destroy_write_struct (&png_ptr, &info_ptr);
        fflush (stdout);
        free (png_ptr);
        free (info_ptr);
        return (OK);
}
