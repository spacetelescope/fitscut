/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * JPEG and PNG output functions
 *
 * Authors: William Jon McCann/Rick White
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
#include "png.h"    /* includes zlib.h and setjmp.h */
#include <float.h>
#include <math.h>
#include "colormap.h"

#define PNG_NUM_TEXT 1

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

#ifdef HAVE_CFITSIO_FITSIO_H
#include <cfitsio/fitsio.h> 
#else
#include <fitsio.h>
#endif

#include "fitscut.h"
#include "output_graphic.h"
#include "image_scale.h"
#include "extract.h"
#include "revision.h"

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

typedef struct {
        int usejpeg;
        int bit_depth;
        struct jpeg_compress_struct jpeg_info;
        png_struct *png_ptr;
        png_info *png_info_ptr;
} GraphicsInfo;

static void create_mean_green (unsigned char *line, int ncols);
static void scale_row_linear (float *arrayp, unsigned char *line, int skip,
                  int stride, long ncols, float scale, float minval,
                  float maxval, float clip_val, int invert);
static void write_rgb_image (GraphicsInfo *info, FitsCutImage *Image);
static void write_simple_image (GraphicsInfo *info, FitsCutImage *Image);

static void jpg_write_line(GraphicsInfo *info, unsigned char *line);
static void jpg_add_header_info(FitsCutImage *Image, GraphicsInfo *info);
static void jpg_open (FitsCutImage *Image, FILE *outfile, GraphicsInfo *info);
static void jpg_close(GraphicsInfo *info);

static void png_write_line(GraphicsInfo *info, unsigned char *line);
static void png_open (FitsCutImage *Image, FILE *outfile, GraphicsInfo *info);
static void png_close(GraphicsInfo *info);

int
write_to_jpg (FitsCutImage *Image)
{
        FILE *outfile;
        GraphicsInfo info;

        if (to_stdout)
                outfile = stdout;
        else
                outfile = fopen(Image->output_filename,"wb");
        if (!outfile)
                return ERROR;

        jpg_open(Image, outfile, &info);

        if (Image->channels == 1)
                write_simple_image (&info, Image);
        else
                write_rgb_image (&info, Image);

        jpg_close(&info);

        return (OK);
}

int
write_to_png (FitsCutImage *Image)
{
        FILE *outfile;
        GraphicsInfo info;

        if (to_stdout)
                outfile = stdout;
        else
                outfile = fopen(Image->output_filename,"wb");
        if (!outfile)
                return ERROR;

        png_open(Image, outfile, &info);

        if (Image->channels == 1)
                write_simple_image (&info, Image);
        else
                write_rgb_image (&info, Image);

        png_close(&info);
        return (OK);
}

static void
create_mean_green (unsigned char *line, int ncols)
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
scale_row_linear (float *arrayp,
                  unsigned char *line,
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
                        t = scale * (arrayp[col] - minval);
                        tx = MIN (t, clip_val);
                        t = MAX (0, tx);
                }
                else {
                        t = arrayp[col];
                }
                if (invert) {
                        t = clip_val - t;
                }
                *pp = (unsigned char) t;
                pp += stride;
        }
}

static void
write_rgb_image (GraphicsInfo *info, FitsCutImage *Image)
{
        int bit_depth = info->bit_depth;
        int usejpeg = info->usejpeg;
        float scale[MAX_CHANNELS];
        int row, k;
        double *datamin, *datamax, clip_val;
        int line_len;
        unsigned char *line;
        int mean_green = 0;

        clip_val = pow(2.0,bit_depth) - 1;

        switch (Image->output_scale_mode) {
        case SCALE_MODE_AUTO:
        case SCALE_MODE_FULL:
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

        line_len = Image->ncolsref * (bit_depth / 8) * Image->channels;
        line = (unsigned char *) calloc (line_len, 1);
        for (row = Image->nrowsref - 1; row >= 0; row--) {
                for (k = 0; k < Image->channels; k++) {
                        if (Image->data[k] == NULL)
                                continue;
                        scale_row_linear (Image->data[k] + row * Image->ncols[k],
                                              line, k,
                                              Image->channels, Image->ncols[k],
                                              scale[k], datamin[k], datamax[k],
                                              clip_val, Image->output_invert);
                }
                if (mean_green == 1) {
                        create_mean_green (line, Image->ncolsref);
                }
                if (usejpeg) {
                        jpg_write_line(info, line);
                } else {
                        png_write_line(info, line);
                }
        }
        free (line);
}

static void
write_simple_image (GraphicsInfo *info, FitsCutImage *Image)
{
        int bit_depth = info->bit_depth;
        int usejpeg = info->usejpeg;
        float scale = 1.0;
        int row;
        double datamin, datamax, clip_val;
        unsigned char *line;

        clip_val = pow(2.0,bit_depth) - 1;

        switch (Image->output_scale_mode) {
        case SCALE_MODE_AUTO:
        case SCALE_MODE_FULL:
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

        if ((line = (unsigned char *) malloc (Image->ncolsref * bit_depth / 8)) == NULL)
                fitscut_error ("out of memory allocating JPEG/PNG row buffer");

        for (row = Image->nrowsref-1; row >= 0; row--) {
                scale_row_linear (Image->data[0] + row * Image->ncolsref,
                                      line, 0, 1, Image->ncolsref, scale,
                                      datamin, datamax, clip_val,
                                      Image->output_invert);
                if (usejpeg) {
                        jpg_write_line(info, line);
                } else {
                        png_write_line(info, line);
                }
        }
        free (line);
}

static void
jpg_write_line(GraphicsInfo *info, unsigned char *line)
{
        struct jpeg_compress_struct *cinfo_ptr = &(info->jpeg_info);
        JSAMPROW row_pointer[1];      /* pointer to JSAMPLE row[s] */

        row_pointer[0] = line;
        jpeg_write_scanlines (cinfo_ptr, (JSAMPARRAY) row_pointer, 1);
}

/*
 * Add WCS from FITS header to JPEG comment section
 * This allows the JPEG images to be used directly in Aladin and compatible display programs
 */

static void
jpg_add_header_info(FitsCutImage *Image, GraphicsInfo *info)
{
    struct jpeg_compress_struct *cinfo_ptr = &(info->jpeg_info);
    char output_text[15*80];
	/* initialize these to eliminate compiler warnings */
    double crval1=0, crval2=0, crpix1=0, crpix2=0, cd1_1=1, cd1_2=0, cd2_1=0, cd2_2=1,
		cdelt1=1, cdelt2=1, crota2=0, pc1_1=0, pc1_2=0, pc2_1=0, pc2_2=0, zoom=1;
    char ctype1[FLEN_VALUE], ctype2[FLEN_VALUE];
    /* counts for various classes of keywords */
    int crcount = 0, cdcount = 0, delcount = 0, rotcount = 0, pccount=0;

    int status = 0;
    char *header, keyname[FLEN_KEYWORD];
    int i,namelen,num_cards;
    char card[FLEN_CARD];
    char value_string[FLEN_VALUE];
    char comment[FLEN_COMMENT];

    header = Image->header[0];
    if (header != NULL) {
        num_cards = Image->header_cards[0];
        keyname[0] = '\0';
        card[FLEN_CARD-1] = '\0';
 
        for (i = 0; i < num_cards; i++) {
            strncpy (card, header + i * (FLEN_CARD - 1), FLEN_CARD - 1);
            fits_get_keyname (card, keyname, &namelen, &status);
            if (strequ (keyname, "CRVAL1")) {
                fits_parse_value (card, value_string, comment, &status);
                crval1 = strtod (value_string, (char **)NULL);
                crcount += 1;
            } else if (strequ (keyname, "CRVAL2")) {
                fits_parse_value (card, value_string, comment, &status);
                crval2 = strtod (value_string, (char **)NULL);
                crcount += 1;
            } else if (strequ (keyname, "CRPIX1")) {
                fits_parse_value (card, value_string, comment, &status);
                crpix1 = strtod (value_string, (char **)NULL);
                crcount += 1;
            } else if (strequ (keyname, "CRPIX2")) {
                fits_parse_value (card, value_string, comment, &status);
                crpix2 = strtod (value_string, (char **)NULL);
                crcount += 1;
            } else if (strequ (keyname, "CD1_1")) {
                fits_parse_value (card, value_string, comment, &status);
                cd1_1 = strtod (value_string, (char **)NULL);
                cdcount += 1;
            } else if (strequ (keyname, "CD1_2")) {
                fits_parse_value (card, value_string, comment, &status);
                cd1_2 = strtod (value_string, (char **)NULL);
                cdcount += 1;
            } else if (strequ (keyname, "CD2_1")) {
                fits_parse_value (card, value_string, comment, &status);
                cd2_1 = strtod (value_string, (char **)NULL);
                cdcount += 1;
            } else if (strequ (keyname, "CD2_2")) {
                fits_parse_value (card, value_string, comment, &status);
                cd2_2 = strtod (value_string, (char **)NULL);
                cdcount += 1;
            } else if (strequ (keyname, "PC001001") || strequ(keyname, "PC1_1")) {
                fits_parse_value (card, value_string, comment, &status);
                pc1_1 = strtod (value_string, (char **)NULL);
                pccount += 1;
            } else if (strequ (keyname, "PC001002") || strequ(keyname, "PC1_2")) {
                fits_parse_value (card, value_string, comment, &status);
                pc1_2 = strtod (value_string, (char **)NULL);
                pccount += 1;
            } else if (strequ (keyname, "PC002001") || strequ(keyname, "PC2_1")) {
                fits_parse_value (card, value_string, comment, &status);
                pc2_1 = strtod (value_string, (char **)NULL);
                pccount += 1;
            } else if (strequ (keyname, "PC002002") || strequ(keyname, "PC2_2")) {
                fits_parse_value (card, value_string, comment, &status);
                pc2_2 = strtod (value_string, (char **)NULL);
                pccount += 1;
            } else if (strequ (keyname, "CDELT1")) {
                fits_parse_value (card, value_string, comment, &status);
                cdelt1 = strtod (value_string, (char **)NULL);
                delcount += 1;
            } else if (strequ (keyname, "CDELT2")) {
                fits_parse_value (card, value_string, comment, &status);
                cdelt2 = strtod (value_string, (char **)NULL);
                delcount += 1;
            } else if (strequ (keyname, "CROTA2")) {
                fits_parse_value (card, value_string, comment, &status);
                crota2 = strtod (value_string, (char **)NULL);
                rotcount += 1;
            } else if (strequ (keyname, "CTYPE1")) {
                fits_parse_value (card, ctype1, comment, &status);
                crcount += 1;
            } else if (strequ (keyname, "CTYPE2")) {
                fits_parse_value (card, ctype2, comment, &status);
                crcount += 1;
            }
        }
    }

    /* catch all errors in above section */
    if (status) printerror (status);

    /* update WCS info */

    fitscut_message (3, "\tWCS counts: crval/pix/type %d cd1_1 %d pc1_1 %d cdelt %d rota %d\n",
		   crcount, cdcount, pccount, delcount, rotcount);
    if (crcount == 6 && (cdcount == 4 || delcount == 2)) {
        zoom = Image->output_zoom[0];
		if (zoom == 0.0) zoom = 1.0;
        crpix1 = crpix1 - Image->x0[0];
        crpix2 = crpix2 - Image->y0[0];
		crpix1 = crpix1*zoom - 0.5*(zoom-1);
		crpix2 = crpix2*zoom - 0.5*(zoom-1);
        if (cdcount == 4) {
            cd1_1 = cd1_1/zoom;
            cd1_2 = cd1_2/zoom;
            cd2_1 = cd2_1/zoom;
            cd2_2 = cd2_2/zoom;
            sprintf(output_text,
                "CTYPE1  = %s\n"
                "CTYPE2  = %s\n"
                "CRPIX1  = %.15g\n"
                "CRPIX2  = %.15g\n"
                "CRVAL1  = %.15g\n"
                "CRVAL2  = %.15g\n"
                "CD1_1   = %.15g\n"
                "CD1_2   = %.15g\n"
                "CD2_1   = %.15g\n"
                "CD2_2   = %.15g\n"
                "COMMENT Created by fitscut %s (William Jon McCann)\n",
                ctype1, ctype2, crpix1, crpix2, crval1, crval2,
                cd1_1, cd1_2, cd2_1, cd2_2, VERSION);
			fitscut_message (2, "\tAdded CD1_1 format WCS to JPEG comment\n");
        } else {
            cdelt1 = cdelt1/zoom;
            cdelt2 = cdelt2/zoom;
		   	/* CROTA2 is optional */
			if (rotcount == 0) crota2 = 0.0;
			if (pccount == 4) {
				sprintf(output_text,
					"CTYPE1  = %s\n"
					"CTYPE2  = %s\n"
					"CRPIX1  = %.15g\n"
					"CRPIX2  = %.15g\n"
					"CRVAL1  = %.15g\n"
					"CRVAL2  = %.15g\n"
					"CDELT1  = %.15g\n"
					"CDELT2  = %.15g\n"
					"CROTA2  = %.15g\n"
					"PC1_1   = %.15g\n"
					"PC1_2   = %.15g\n"
					"PC2_1   = %.15g\n"
					"PC2_2   = %.15g\n"
					"COMMENT Created by fitscut %s (William Jon McCann)\n",
					ctype1, ctype2, crpix1, crpix2, crval1, crval2,
					cdelt1, cdelt2, crota2, pc1_1, pc1_2, pc2_1, pc2_2, VERSION);
				fitscut_message (2, "\tAdded PC/CDELT1 format WCS to JPEG comment\n");
			} else {
				sprintf(output_text,
					"CTYPE1  = %s\n"
					"CTYPE2  = %s\n"
					"CRPIX1  = %.15g\n"
					"CRPIX2  = %.15g\n"
					"CRVAL1  = %.15g\n"
					"CRVAL2  = %.15g\n"
					"CDELT1  = %.15g\n"
					"CDELT2  = %.15g\n"
					"CROTA2  = %.15g\n"
					"COMMENT Created by fitscut %s (William Jon McCann)\n",
					ctype1, ctype2, crpix1, crpix2, crval1, crval2,
					cdelt1, cdelt2, crota2, VERSION);
				fitscut_message (2, "\tAdded CDELT1 format WCS to JPEG comment\n");
			}
        }
    } else {
        /* no WCS found, so just add the comment */
		fitscut_message (3, "\tNo WCS found for JPEG comment\n");
        sprintf (output_text, "Created by fitscut %s (William Jon McCann)", VERSION);
    }
    jpeg_write_marker (cinfo_ptr, JPEG_COM, (unsigned char *) output_text, strlen (output_text));
}

static void
jpg_open (FitsCutImage *Image, FILE *outfile, GraphicsInfo *info)
{
        struct jpeg_compress_struct *cinfo_ptr = &(info->jpeg_info);
        struct jpeg_error_mgr jerr;
        long width, height;
        int num_palette;

        info->usejpeg = 1;
        cinfo_ptr->err = jpeg_std_error (&jerr);
        jpeg_create_compress (cinfo_ptr);

        jpeg_stdio_dest (cinfo_ptr, outfile);

        width = Image->ncolsref;
        height = Image->nrowsref;
        info->bit_depth = 8;

        cinfo_ptr->image_width = width;      /* image width and height, in pixels */
        cinfo_ptr->image_height = height;

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
                cinfo_ptr->input_components = 1;     /* # of color components per pixel */
                cinfo_ptr->in_color_space = JCS_GRAYSCALE;
        }
        else {
                fitscut_message (1, "\t\tusing RGB color type\n");
                cinfo_ptr->input_components = 3;     /* # of color components per pixel */
                cinfo_ptr->in_color_space = JCS_RGB; /* colorspace of input image */
        }

        jpeg_set_defaults (cinfo_ptr);
        jpeg_set_quality (cinfo_ptr, Image->jpeg_quality, FALSE);

        jpeg_start_compress (cinfo_ptr, TRUE);
        jpg_add_header_info(Image, info);
}

static void
jpg_close(GraphicsInfo *info)
{
        struct jpeg_compress_struct *cinfo_ptr = &(info->jpeg_info);

        fitscut_message (2, "finished writing JPEG image\n");
        jpeg_finish_compress (cinfo_ptr);
        jpeg_destroy_compress (cinfo_ptr);
        fflush (stdout);
}

static void
png_write_line(GraphicsInfo *info, unsigned char *line)
{
        png_write_row (info->png_ptr, line);
}

static void
png_open (FitsCutImage *Image, FILE *outfile, GraphicsInfo *info)
{
        long width, height;
        int color_type, num_palette = 256;
        char comment_text[64];
        png_text text_ptr[PNG_NUM_TEXT];
        png_color *palette = NULL;

        info->usejpeg = 0;

        width = Image->ncolsref;
        height = Image->nrowsref;
        info->bit_depth = 8;

        info->png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (info->png_ptr == NULL)
                fitscut_error ("cannot allocate LIBPNG structure");

        info->png_info_ptr = png_create_info_struct (info->png_ptr);
        if (info->png_info_ptr == NULL) {
                png_destroy_write_struct (&info->png_ptr, (png_infopp)NULL);
                fitscut_error ("cannot allocate LIBPNG structures");
        }

        if (setjmp (png_jmpbuf (info->png_ptr))) {
                png_destroy_write_struct (&info->png_ptr, &info->png_info_ptr);
                fitscut_error ("setjmp returns error condition (1)");
        }

        png_init_io (info->png_ptr, outfile);

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
        png_set_IHDR (info->png_ptr, info->png_info_ptr, width, height, info->bit_depth, color_type,
                      PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

        if (color_type == PNG_COLOR_TYPE_PALETTE)
                png_set_PLTE (info->png_ptr, info->png_info_ptr, palette, num_palette);

        /* write comments into the image */
        text_ptr[0].key = "Software";
        sprintf (comment_text, "Created by fitscut %s (William Jon McCann)", VERSION);
        text_ptr[0].text = strdup (comment_text);
        text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
#ifdef PNG_iTXt_SUPPORTED
        text_ptr[0].lang = NULL;
#endif
        png_set_text (info->png_ptr, info->png_info_ptr, text_ptr, PNG_NUM_TEXT);

        /* write the png-info struct */
        png_write_info (info->png_ptr, info->png_info_ptr);
}

static void
png_close(GraphicsInfo *info)
{
        fitscut_message (2, "finished writing PNG image\n");

        png_write_end (info->png_ptr, info->png_info_ptr);
        png_destroy_write_struct (&info->png_ptr, &info->png_info_ptr);
        fflush (stdout);
        free (info->png_ptr);
        free (info->png_info_ptr);
        info->png_ptr = NULL;
        info->png_info_ptr = NULL;
}
