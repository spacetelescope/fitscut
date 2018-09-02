/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * Cutout extraction functions
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
 * $Id: extract.c,v 1.20 2004/05/11 22:05:06 mccannwj Exp $
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
#include "extract.h"
#include "resize.h"

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#define LARGEST_BLOCK 60
#endif

void
extract_fits (FitsCutImage *Image)
{
        fitsfile *fptr;       /* pointer to the FITS file; defined in fitsio.h */
        int status;
        long fpixel[2];
        long lpixel[2];
        long inc[2];
        float *arrayptr;
        float *arrayptr2;
        int datatype, anynull;
        int nullval = 0;
        int rows_used = 0;
        int cols_used = 0;
        int xoffset = 0;
        int yoffset = 0;
        unsigned char first_channel = TRUE;

        int naxis;
        long naxes[2];
        long x1, y1, x0, y0;

        int k, i, j;
        int num_keys, more_keys;
        char *header;

        float zoom_factor;
        int resize_width = 0;
        int resize_height = 0;
  
        for (k = 0; k < Image->channels; k++) {

                status = 0;
    
                fitscut_message (1, "\tExamining FITS channel %d...\n", k);
    
                /* reset data pointer */
                Image->data[k] = NULL;

                if (Image->input_filename[k] == NULL)
                        continue;
                if (fits_open_file (&fptr, Image->input_filename[k], READONLY, &status)) 
                        printerror (status);

                /* check that cutout is within image dimensions */
                if (fits_get_img_dim (fptr, &naxis, &status))
                        printerror (status);

                if (fits_get_img_size (fptr, 2, naxes, &status))
                        printerror (status);
                if (Image->ncols[k] == MAGIC_SIZE_ALL_NUMBER) {
                        Image->ncols[k] = naxes[0];
                        Image->x0[k] = 0;
                }
                if (Image->nrows[k] == MAGIC_SIZE_ALL_NUMBER) {
                        Image->nrows[k] = naxes[1];
                        Image->y0[k] = 0;
                }
    
                x0 = Image->x0[k];
                y0 = Image->y0[k];

                /* CFITSIO starts indexing at 1 */
                x0 += 1;
                y0 += 1;
                x1 = x0 + Image->ncols[k]-1;
                y1 = y0 + Image->nrows[k]-1;
                fpixel[0] = MAX (1,x0);
                fpixel[1] = MAX (1,y0);
    
                inc[0] = 1;
                inc[1] = 1;
                lpixel[0] = MIN (naxes[0],x1);
                lpixel[1] = MIN (naxes[1],y1);

                if (fits_get_img_type (fptr, &datatype, &status))
                        printerror (status);
                Image->input_datatype[k] = datatype;

                rows_used = lpixel[1] - fpixel[1] + 1;
                cols_used = lpixel[0] - fpixel[0] + 1;

                fitscut_message (1, "\tAllocating space for %d x %d array\n",
                                 cols_used, rows_used);
                arrayptr = cutout_alloc (cols_used, rows_used, sizeof (float));

                if (fits_get_hdrspace (fptr, &num_keys, &more_keys, &status))
                        printerror (status);

                fitscut_message (3, "\t\theader has %d keys with space for %d more\n",
                                 num_keys, more_keys);

                /* let cfitsio extract the entire header as a string */
                if (fits_get_image_wcs_keys (fptr, &header, &status))
                        printerror (status);

                Image->header_cards[k] = num_keys;
                Image->header[k] = header;

                fitscut_message (1, "\tExtracting %s[%ld:%ld,%ld:%ld]...\n",
                                 Image->input_filename[k],
                                 fpixel[0], lpixel[0], fpixel[1], lpixel[1]);
                if (fits_read_subset (fptr, TFLOAT, fpixel, lpixel, inc,
                                      &nullval, arrayptr, &anynull, &status))
                        printerror (status);

                if (fits_close_file (fptr, &status)) 
                        printerror (status);

                if ((cols_used != Image->ncols[k]) || (rows_used != Image->nrows[k])) {
                        /* place the subset read from the fits file into an array
                         * of size ncols x nrows */

                        fitscut_message (1, "\tallocating space for %ld x %ld array\n",
                                         Image->ncols[k], Image->nrows[k]);
                        arrayptr2 = cutout_alloc (Image->ncols[k], Image->nrows[k], sizeof (float));
                        xoffset = (fpixel[0] == x0) ? 0 : (fpixel[0] - x0);
                        yoffset = (fpixel[1] == y0) ? 0 : (fpixel[1] - y0);

                        fitscut_message (2, "\tplacing subarray into array using offsets x=%d y=%d\n",
                                         xoffset, yoffset);
                        for (i = 0; i < cols_used; i++) {
                                for (j = 0; j < rows_used; j++) {
                                        *(arrayptr2 + (i + xoffset) + (j + yoffset) * Image->ncols[k])
                                                = *(arrayptr + i + j * cols_used);

                                        fitscut_message (3, "\tplacing [%d,%d]=%f into [%d,%d]\n",
                                                         i, j, *(arrayptr + i + j * cols_used),
                                                         i + xoffset, j + xoffset);
                                }
                        }
                        Image->data[k] = arrayptr2;
                        free (arrayptr);
                }
                else {
                        Image->data[k] = arrayptr;
                }

                if (Image->output_zoom != 0) {

                        if (first_channel) {
                                if (Image->output_zoom > 0) {
                                        zoom_factor = Image->output_zoom;
                                }
                                else {
                                        zoom_factor = 1.0;
                                }

                                fitscut_message (2, "Calculated zoom factor of %f from %f\n",
                                                 zoom_factor, Image->output_zoom);
                                resize_height = zoom_factor * Image->nrows[k];
                                resize_width = zoom_factor * Image->ncols[k];

                                fitscut_message (2, "Calculated zoom size of x=%d y=%d\n",
                                                 resize_width, resize_height);
                        }

                        fitscut_message (2, "\tresizing channel to x=%d y=%d\n",
                                         resize_width, resize_height);

                        resize_image_channel (Image, k, resize_width, resize_height);
                }

                first_channel = FALSE;
        }
}

/*
 * Allocate memory for cutout
 */
float *
cutout_alloc (unsigned int nx, unsigned int ny, unsigned int size)
{
        unsigned int nelem;
        float *ptr;

        nelem = nx * ny;
        ptr = (float *) calloc (nelem,size);
        if (ptr == NULL) {
                fitscut_message (0, "Unable to allocate memory for %d x %d image\n",
                                 nx, ny);
                do_exit (1);
        }
        return ptr;
}

/*
 * Print out cfitsio error messages and exit program
 */
void
printerror (int status)
{

        if (status) {
                fits_report_error (stderr, status); /* print error report */
                do_exit (status);    /* terminate the program, returning error status */
        }
}

double
fits_get_exposure_time (char *header, int num_cards)
{
        int status = 0;
        char keyname[FLEN_KEYWORD];
        int i,namelen;
        char card[FLEN_CARD+1];
        char value_string[FLEN_VALUE];
        char comment[FLEN_COMMENT];
        double value = -1.0;
        if (header != NULL) {
                keyname[0] = '\0';
 
                for (i = 0; i < num_cards; i++) {
                        strncpy (card, header + i * (FLEN_CARD - 1), FLEN_CARD - 1);
                        fits_get_keyname (card, keyname, &namelen, &status);
                        if (strequ (keyname, "EXPTIME")) {
                                fits_parse_value (card, value_string, comment, &status);
                                value = strtod (value_string, (char **)NULL);
                                fitscut_message (2, "Found EXPTIME='%s' converted to %f\n",
                                                 value_string, value);
                                return value;
                        }
                }
        }
        return value;
}
