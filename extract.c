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

#include <libwcs/wcs.h>
#include "wcs_align.h"

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#define LARGEST_BLOCK 60
#endif

/*
 * Return true if specified extension exists
 * Return false on failure or if extension does not exist
 */

extern int ext_exists(fitsfile *fptr, int qext, int *status)
{
int next;

    if (*status) return 0;
    if (fits_get_num_hdus(fptr, &next, status)) return 0;
    if (next < qext) {
        fitscut_message (1, "\tQuality extension %d not found in image\n", qext);
        return 0;
    } else {
        return 1;
    }
}

/*
 * equivalent to cfitsio fits_read_subset but works around a performance
 * problem for compressed images in some versions of cfitsio by reading
 * the rows separately when inc[1] != 1
 */
extern int
fitscut_read_subset(fitsfile *fptr, int datatype, long *fpixel, long *lpixel, long *inc,
    void *nulval, void *array, int *anynul, int *status)
{
long fpixel1[7], lpixel1[7], i, nx, ny;
float *farray;

    if (*status) return *status;
    if (inc[1] == 1) {
         /* just read one big block if we're reading every row */
         return fits_read_subset (fptr, datatype, fpixel, lpixel, inc,
                              nulval, array, anynul, status);
    } else {
        /* read each row as a separate call */
        nx = (lpixel[0]-fpixel[0])/inc[0] + 1;
        ny = (lpixel[1]-fpixel[1])/inc[1] + 1;
        for (i=0; i<6; i++) {
            fpixel1[i] = fpixel[i];
            lpixel1[i] = lpixel[i];
        }
        /* only handling the 2 datatypes fitscut needs, both 4-byte chunks */
        switch (datatype) {
            case TINT:
            case TFLOAT:
                farray = (float *) array;
                break;
            default:
                fprintf(stderr, "Unsupported datatype %d for fitscut_read_subset (only TFLOAT=%d and TINT=%d accepted)\n",
                    datatype, TFLOAT, TINT);
                *status = BAD_DATATYPE;
                return *status;
        }
        for (i=0; i<ny; i++) {
            fpixel1[1] = fpixel[1]+i*inc[1];
            lpixel1[1] = fpixel1[1];
            if (fits_read_subset (fptr, datatype, fpixel1, lpixel1, inc, nulval, &farray[nx*i], anynul, status)) {
                return *status;
            }
        }
    }
    return *status;
}

static int open_qual (fitsfile **dqptr, long *nplanes, fitsfile *fptr, int qext, int *status)
{
int naxis;
long naxes[3];
char filename[512];

    if (*status) return *status;
    *dqptr = NULL;
    if (! ext_exists(fptr, qext, status)) {
        /*
         * missing quality extension is not considered fatal
         * just skip reading the quality array
         */
        return *status;
    }

    (void) fits_file_name(fptr, filename, status);
    (void) fits_open_image (dqptr, filename, READONLY, status);
    (void) fits_movabs_hdu(*dqptr, qext, NULL, status);

    /* see if this is a 2-D or 3-D context/quality image */
    if (fits_get_img_dim (*dqptr, &naxis, status))
        return *status;
    if (naxis < 2 || naxis > 3) {
        /* not 2-D or 3-D -- just skip reading */
        (void) fits_close_file (*dqptr, status);
        *dqptr = NULL;
        return *status;
    } else if (naxis == 2) {
        /* normal case: 1-plane quality image */
        *nplanes = 1;
        fitscut_message (1, "\tReading quality extension %d [*,*]...\n",
                 qext);
    } else {
        if (fits_get_img_size (*dqptr, 3, naxes, status))
            return *status;
        *nplanes = naxes[2];
        fitscut_message (1, "\tReading quality extension %d [*,*,1:%ld]...\n",
                 qext, *nplanes);
    }
    return *status;
}


/*
 * Get info required for data quality flagging
 * Open data quality extension if it exists, or read badpix limits
 * Output parameters are:
 * dqptr = fits file pointer (NULL if no quality data available)
 * nplanes = number of planes in DQ array
 * badmin, badmax = range indicating bad pixel values
 */

extern int get_qual_info (fitsfile **dqptr, long *nplanes, float *badmin, float *badmax, float *bad_data_value,
        fitsfile *fptr, char *header, int num_cards,
        int qext_set, int qext, int useBadpix, int *status)
{
    if (*status) return *status;
    *dqptr = NULL;
    *badmin = *bad_data_value;
    *badmax = *bad_data_value;
    if (qext_set) {
        if (open_qual (dqptr, nplanes, fptr, qext, status))
            return *status;
    }
    if (*dqptr == NULL && useBadpix) {
        /* get BADPIX keyword from header */
        fits_get_badpix(header, num_cards, badmin, badmax, bad_data_value);
        if (*badmin != *bad_data_value) {
            if (*badmax != *bad_data_value) {
                fitscut_message (1, "\tZeroing pixels outside range %f - %f\n",
                     *badmin, *badmax);
            } else {
                fitscut_message (1, "\tZeroing pixels less than %f\n",
                     *badmin);
            }
        }
    }
    return *status;
}

/*
 * Apply data quality array or limits and set bad pixels to NaN.
 * Returns the number of bad pixels that got blanked.
 */

extern int apply_qual (fitsfile *dqptr, long nplanes, float badmin, float badmax, float bad_data_value,
    long fpixel[7], long lpixel[7], long inc[7],
    float *arrayptr, int anynull, int badvalue,
    int *status)
{
int i, j, *qarrayptr, nbad=0;
long dim[7], totsize;
unsigned char *qbadptr;

    if (*status) return 0;

    /* give up if original image is not 2-D */
    totsize = 1;
    for (i=0; i<7; i++) {
        dim[i] = (lpixel[i]-fpixel[i])/inc[i] + 1;
        totsize *= dim[i];
    }
    for (i=2; i<7; i++) {
        if (dim[i] != 1) {
            return 0;
        }
    }

    if (dqptr != NULL) {
        qarrayptr = (int *) malloc(totsize*sizeof(int));
        qbadptr = (unsigned char *) malloc(totsize*sizeof(unsigned char));
        if (qarrayptr == NULL || qbadptr == NULL) {
            *status = MEMORY_ALLOCATION;
            return 0;
        }
        for (i=0; i<totsize; i++) {
            qbadptr[i] = 1;
        }
        for (j=1; j<=nplanes; j++) {
            fpixel[2] = j;
            lpixel[2] = j;
            if (fitscut_read_subset (dqptr, TINT, fpixel, lpixel, inc,
                                  0, qarrayptr, &anynull, status))
                return 0;
            /* pixel are good if any plane indicates good data */
            for (i=0; i<totsize; i++) {
                if (qarrayptr[i] != badvalue) qbadptr[i] = 0;
            }
        }
        free(qarrayptr);

        for (i=0; i<totsize; i++) {
            if (qbadptr[i] || arrayptr[i] != arrayptr[i]) {
                nbad++;
                arrayptr[i] = NAN;
            }
        }

        free(qbadptr);
    } else if (badmin != bad_data_value) {
        if (badmax != bad_data_value) {
            /* set values outside badmin,badmax to zero */
            for (i=0; i<totsize; i++) {
                /* note by testing this way we find NaN values too
                 * both comparisons are false for NaN
                 */
                if (! (arrayptr[i] >= badmin && arrayptr[i] <= badmax)) {
                    arrayptr[i] = NAN;
                    nbad++;
                }
            }
        } else {
            /* set values smaller than badmin to zero */
            for (i=0; i<totsize; i++) {
                if (! (arrayptr[i] >= badmin)) {
                    arrayptr[i] = NAN;
                    nbad++;
                }
            }
        }
    } else if (finite(bad_data_value)) {
        /* set bad_data_value values to NAN */
        for (i=0; i<totsize; i++) {
            if (arrayptr[i] == bad_data_value) {
                arrayptr[i] = NAN;
                nbad++;
            }
        }
    }

    return nbad;
}

/*
 * invert asinh scaling (used in some PanSTARRS images at least)
 * leave bad_data_value pixels unchanged
 */

void invert_bsoften (double bsoften, double boffset,
    long fpixel[7], long lpixel[7], long inc[7],
    float *arrayptr, float bad_data_value)
{
int i;
long totsize;
double temp;

    totsize = 1;
    for (i=0; i<7; i++) {
        totsize *= (lpixel[i]-fpixel[i])/inc[i] + 1;
    }

    /* do scaling for values that are not NaN */
    for (i=0; i<totsize; i++) {
        if (finite(arrayptr[i]) && arrayptr[i] != bad_data_value) {
            temp = ((double) arrayptr[i]) / 1.0857362;
            arrayptr[i] = bsoften*(exp(temp)-exp(-temp)) + boffset;
        }
    }
}

/*
 * get asinh scaling parameters for image k
 */

void
fits_get_bsoften (FitsCutImage *Image, int k, int *useBsoften, double *bsoften, double *boffset)
{
    int status = 0;
    char keyname[FLEN_KEYWORD];
    int i,namelen;
    char card[FLEN_CARD];
    char value_string[FLEN_VALUE];
    char comment[FLEN_COMMENT];
    char *header;
    int num_cards;

    *bsoften = NAN;
    *boffset = NAN;
    *useBsoften = Image->useBsoften;
    if (! *useBsoften) return;

    header = Image->header[k];
    num_cards = Image->header_cards[k];
    if (header != NULL) {
        keyname[0] = '\0';
        card[FLEN_CARD-1] = '\0';
        for (i = 0; i < num_cards; i++) {
            strncpy (card, header + i * (FLEN_CARD - 1), FLEN_CARD - 1);
            fits_get_keyname (card, keyname, &namelen, &status);
            if (strequ (keyname, "BSOFTEN")) {
                fits_parse_value (card, value_string, comment, &status);
                *bsoften = strtod (value_string, (char **)NULL);
                fitscut_message (2, "Found BSOFTEN=%f\n", *bsoften);
                break;
            }
        }
 
        for (i = 0; i < num_cards; i++) {
            strncpy (card, header + i * (FLEN_CARD - 1), FLEN_CARD - 1);
            fits_get_keyname (card, keyname, &namelen, &status);
            if (strequ (keyname, "BOFFSET")) {
                fits_parse_value (card, value_string, comment, &status);
                /* using strtod because strtof is not always declared */
                *boffset = strtod (value_string, (char **)NULL);
                fitscut_message (2, "Found BOFFSET=%f\n", *boffset);
                break;
            }
        }
    }
    if (! (finite(*bsoften) && finite(*boffset))) {
        fitscut_message (2, "Failed to find both BSOFTEN, BOFFSET\n");
        *boffset = NAN;
        *bsoften = NAN;
        *useBsoften = 0;
    }
}

void
extract_fits (FitsCutImage *Image)
{
    fitsfile *fptr;          /* pointer to the FITS file; defined in fitsio.h */
    fitsfile *dqptr = NULL;  /* pointer to the data quality extension */
    int status;
    /* allow room for trailing dimensions */
    long fpixel[7] = {1,1,1,1,1,1,1};
    long lpixel[7] = {1,1,1,1,1,1,1};
    long inc[7] = {1,1,1,1,1,1,1};
    float *arrayptr;
    float *bufferptr;
    int datatype, anynull;
    float nullval = NAN;
    int pixfac, doshrink, pstart, bufrows;
    int nrows, rows_read = 0, zoomrows;
    int ncols, cols_read = 0, zoomcols;
    int xoffset = 0;
    int yoffset = 0;

    int naxis;
    long naxes[2], nplanes;
    long x1, y1, x0, y0;
    double xsky, ysky, xpix, ypix;
    int offscl;
    int useBsoften;
    double boffset, bsoften;

    int k, i, j, j0, j1, nbad = 0, ngoodimages = 0;
    int num_keys, more_keys;
    char *header;

    struct WorldCoor *save_wcs = NULL;

    /* initialize to silence compiler warnings */
    float zoom_factor = 1.0;

    /*
     * set up reference image info
     * all cutout positions are initially the same, so copy channel zero
     */
    Image->nrowsref = Image->nrows[0];
    Image->ncolsref = Image->ncols[0];
    Image->output_zoomref = Image->output_zoom[0];
    xpix = Image->input_x[0];
    ypix = Image->input_y[0];

    /* get world coordinate system info for reference image */
    wcs_initialize_ref(Image,naxes);
    if (nowcs(Image->wcsref)) {
        if (Image->input_wcscoords[0] || Image->output_alignment == ALIGN_REF) {
            /* fatal error: wcs is required but not present */
            fitscut_message (0,
                "No WCS info for reference image %s\n", Image->reference_filename);
            do_exit (1);
        }

    } else if (Image->input_wcscoords[0]) {

        /* convert coordinates from sky to pixels */

        xsky = xpix;
        ysky = ypix;
        wcs2pix(Image->wcsref, xsky, ysky, &xpix, &ypix, &offscl);
        if (offscl == 1) {
            /* way off image center -- just use huge numbers */
            fitscut_message (0, "Position is very far off edge of reference image\n");
            do_exit (1);
        }
        /* convert to zero-based pixel numbers for x0, y0 */
        xpix = xpix-1;
        ypix = ypix-1;
    }

    if (Image->input_x_corner[0] == 0) {
        Image->x0ref = xpix - Image->ncolsref / 2;
    } else {
        Image->x0ref = xpix;
    }
    if (Image->input_y_corner[0] == 0) {
        Image->y0ref = ypix - Image->nrowsref / 2;
    } else {
        Image->y0ref = ypix;
    }

    if (Image->ncolsref == MAGIC_SIZE_ALL_NUMBER) {
        Image->ncolsref = naxes[0];
        Image->x0ref = 0;
    }
    if (Image->nrowsref == MAGIC_SIZE_ALL_NUMBER) {
        Image->nrowsref = naxes[1];
        Image->y0ref = 0;
    }
    nrows = Image->nrowsref;
    ncols = Image->ncolsref;

    x0 = floor(Image->x0ref+0.5);
    y0 = floor(Image->y0ref+0.5);

    /* propagate exact corner used back into Image structure */
    Image->x0ref = x0;
    Image->y0ref = y0;

    if (x0<0)
        fitscut_message (1, "fitscut: warning: cutout x0 < 0 for reference image\n");
    if (y0<0)
        fitscut_message (1, "fitscut: warning: cutout y0 < 0 for reference image\n");

    /* determine zoomed image size */

    if (Image->output_zoomref > 0) {
        zoom_factor = Image->output_zoomref;
    }
    else {
        zoom_factor = 1.0;
    }
    fitscut_message (2, "Calculated zoom factor of %f from %f\n",
             zoom_factor, Image->output_zoomref);

    get_zoom_size_channel (ncols, nrows, zoom_factor, Image->output_size,
        &pixfac, &zoomcols, &zoomrows, &doshrink);
    fitscut_message (1, "\tZoomed output size is %d x %d\n", zoomcols, zoomrows);

    Image->ncolsref = zoomcols;
    Image->nrowsref = zoomrows;
    if (doshrink) {
        Image->output_zoomref = 1.0/pixfac;
    } else if (pixfac > 1) {
        Image->output_zoomref = pixfac;
    } else {
        Image->output_zoomref = 1.0;
    }

    if (Image->output_size > 0) {

        /* do final resize to exact output size */
        fitscut_message (2, "Forcing output size %d\n", Image->output_size);
        
        if (Image->output_alignment == ALIGN_REF) {
            /*
             * Different filter images don't get resized inside the loop, so
             * we don't actually resize the reference image here.
             * This code is a little clumsy but should work.  The goal is
             * to enable checks to see whether the WCS of various filters are
             * identical to work correctly inside the loop.
             *
             * Save the original reference WCS for later restoration.
             */
            save_wcs = (struct WorldCoor *) malloc(sizeof(struct WorldCoor));
            memcpy(save_wcs, Image->wcsref, sizeof(struct WorldCoor));
        } else {
            exact_resize_reference (Image, Image->output_size);
        }
    }
    /* update world coordinate systems if cutout or zoom is used */
    wcs_update_ref(Image);

    fitscut_message (1, "\tReference %s[%ld:%ld,%ld:%ld]...\n",
             Image->reference_filename,
             (long) x0+1, (long) x0+ncols, (long) y0+1, (long) y0+nrows);

    for (k = 0; k < Image->channels; k++) {

        status = 0;
    
        fitscut_message (1, "\tExamining FITS channel %d...\n", k);
    
        /* reset data pointer */
        Image->data[k] = NULL;

        if (Image->input_filename[k] == NULL)
            continue;

        if (fits_open_image (&fptr, Image->input_filename[k], READONLY, &status)) 
            printerror (status);

        /* let cfitsio extract the entire header as a string */
        if (fits_get_image_wcs_keys (fptr, &header, &status))
            printerror (status);

        Image->header_cards[k] = strlen(header)/(FLEN_CARD-1);
        Image->header[k] = header;

        /* get world coordinate system info from header */
        wcs_initialize_channel(Image, k);

        if (Image->input_wcscoords[k]) {
            if (nowcs(Image->wcs[k])) {
                fitscut_message (1,
                    "fitscut: warning: no WCS info for channel %d\n", k);
            } else {

                /* convert coordinates from sky to pixels */

                xsky = Image->input_x[k];
                ysky = Image->input_y[k];
                wcs2pix(Image->wcs[k], xsky, ysky, &xpix, &ypix, &offscl);
                if (offscl == 1) {
                    /* way off image center -- just use huge numbers */
                    Image->input_x[k] = 1.e10;
                    Image->input_y[k] = 1.e10;
                } else {
                    /* convert to zero-based pixel numbers for x0, y0 */
                    xpix = xpix-1;
                    ypix = ypix-1;
                    Image->input_x[k] = xpix;
                    Image->input_y[k] = ypix;
                }
            }
        }

        if (Image->output_alignment == ALIGN_REF) {
            /* image section to extract is determined by reference image */
            wcs_match_channel (Image, k);
        }

        if (Image->input_x_corner[k] == 0) {
            Image->x0[k] = Image->input_x[k] - Image->ncols[k] / 2;
        } else {
            Image->x0[k] = Image->input_x[k];
        }
        if (Image->input_y_corner[k] == 0) {
            Image->y0[k] = Image->input_y[k] - Image->nrows[k] / 2;
        } else {
            Image->y0[k] = Image->input_y[k];
        }

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

        /* check that cutout is within image dimensions */
        nrows = Image->nrows[k];
        ncols = Image->ncols[k];

        x0 = floor(Image->x0[k]+0.5);
        y0 = floor(Image->y0[k]+0.5);

        /* propagate exact corner used back into Image structure */
        Image->x0[k] = x0;
        Image->y0[k] = y0;

        if (x0<0)
            fitscut_message (1, "fitscut: warning: cutout x0 < 0 for channel %d\n", k);
        if (y0<0)
            fitscut_message (1, "fitscut: warning: cutout y0 < 0 for channel %d\n", k);

        /* determine zoomed image size */

        if (Image->output_alignment == ALIGN_REF) {
            /* estimate zoom factor based on image region and output sizes */
            zoom_factor = ((float) Image->nrowsref)/((float) nrows);
            if (zoom_factor > 1) {
                zoom_factor = 1.0;
            } else {
                zoom_factor = 1.0/lround(1.0/zoom_factor);
            }
        } else if (Image->output_zoom[k] > 0) {
            zoom_factor = Image->output_zoom[k];
        }
        else {
            zoom_factor = 1.0;
        }
        fitscut_message (2, "Calculated zoom factor of %f from %f\n",
                 zoom_factor, Image->output_zoom[k]);

        get_zoom_size_channel (ncols, nrows, zoom_factor, Image->output_size,
            &pixfac, &zoomcols, &zoomrows, &doshrink);
        fitscut_message (1, "\tZoomed output size is %d x %d\n", zoomcols, zoomrows);
        fitscut_message(2, "\tpixfac = %d\n", pixfac);

        /* CFITSIO starts indexing at 1 */
        x0 += 1;
        y0 += 1;
        x1 = x0 + ncols - 1;
        y1 = y0 + nrows - 1;
        fpixel[0] = MAX (1,x0);
        fpixel[1] = MAX (1,y0);
    
        inc[0] = 1;
        inc[1] = 1;
        lpixel[0] = MIN (naxes[0],x1);
        lpixel[1] = MIN (naxes[1],y1);
        y1 = lpixel[1];

        if (fits_get_img_type (fptr, &datatype, &status))
            printerror (status);
        /*
         * force datatype to single precision if input is double (since all the code
         * below reads the image as single precision)
         */
        if (datatype == DOUBLE_IMG) datatype = FLOAT_IMG;
        Image->input_datatype[k] = datatype;

        rows_read = lpixel[1] - fpixel[1] + 1;
        cols_read = lpixel[0] - fpixel[0] + 1;
        if (rows_read <= 0 || cols_read <= 0) {
            /* create empty image for this band
             * will print error at end of loop if dimensions are negative for all bands
             */
            fitscut_message (1, "Some image dimensions are negative (%d x %d)\n",
                     cols_read, rows_read);
        } else {
            ngoodimages += 1;

            /* setup for data quality flagging */

            /*
             * get info for data quality flagging (if it is used)
             * don't apply flagging for JSON (pixel value) output or
             * for FITS output (unless the FITS image is being rebinned)
             */
            if (doshrink || (Image->output_type != OUTPUT_JSON && Image->output_type != OUTPUT_FITS)) {
                if (get_qual_info (&dqptr, &nplanes, &Image->badmin[k], &Image->badmax[k], &Image->bad_data_value[k],
                    fptr, Image->header[k], Image->header_cards[k],
                    Image->qext_set, Image->qext[k], Image->useBadpix,
                    &status))
                    printerror (status);
            }

            if (fits_get_hdrspace (fptr, &num_keys, &more_keys, &status))
                printerror (status);

            fitscut_message (3, "\t\theader has %d keys with space for %d more\n",
                     num_keys, more_keys);
        }

        /*
         * allocate partial buffer and bin in blocks
         */
        if (doshrink && (zoomcols < ncols || zoomrows < nrows)) {
            /* determine number of rows to read for buffer */
            bufrows = pixfac; /* TEST: ultimately make this bigger based on memory usage */
        } else {
            bufrows = nrows;
        }

        /* create array for output image */

        fitscut_message (1, "\tAllocating space for %d x %d output array\n",
                 zoomcols, zoomrows);
        arrayptr = cutout_alloc (zoomcols, zoomrows, NAN);

        nbad = 0;
        if (cols_read > 0 && rows_read > 0) {
            if (pixfac > 1) {
                fitscut_message (2, "\tAllocating space for %d x %d buffer\n",
                         ncols, bufrows);
                bufferptr = cutout_alloc (ncols, bufrows, NAN);
            } else {
                /* no resizing, so make buffer same as output array */
                bufferptr = arrayptr;
            }

            fitscut_message (1, "\tExtracting %s[%ld:%ld,%ld:%ld]...\n",
                     Image->input_filename[k],
                     fpixel[0], lpixel[0], fpixel[1], lpixel[1]);

            /* get asinh parameters from header if requested */
            fits_get_bsoften (Image, k, &useBsoften, &bsoften, &boffset);

            /*
             * read block of pixels into buffer
             * apply DQ flagging for the block
             * rebin using zoom factor and insert into zoomed array locations
             */

            for (j0=y0; j0 <= y1; j0 += bufrows) {
                j1 = j0 + bufrows - 1;
                fpixel[1] = MAX (1,j0);
                lpixel[1] = MIN (y1,j1);
                rows_read = lpixel[1] - fpixel[1] + 1;

                if (rows_read <= 0) {

                    /* off edge, mark data as missing */
                    for (j = 0; j < bufrows; j++) {
                        for (i = 0; i < ncols; i++) bufferptr[i+j*ncols] = NAN;
                    }

                } else {

                    /* put data at the end of the buffer to make shifting easier */
                    pstart = ncols*bufrows - cols_read*rows_read;

                    if (fitscut_read_subset (fptr, TFLOAT, fpixel, lpixel, inc,
                                  &nullval, &bufferptr[pstart], &anynull, &status))
                        printerror (status);

                    /* apply data quality flagging to zero bad pixels */

                    nbad += apply_qual (dqptr, nplanes, Image->badmin[k], Image->badmax[k], Image->bad_data_value[k],
                        fpixel, lpixel, inc,
                        &bufferptr[pstart], anynull, Image->qext_bad_value[k], &status);
                    if (status)
                        printerror (status);

                    if (useBsoften) {
                        /* invert asinh scaling */
                        invert_bsoften(bsoften, boffset, fpixel, lpixel, inc,
                            &bufferptr[pstart], Image->bad_data_value[k]);
                    }

                    if (pstart != 0) {
                        /* move the subset read from the fits file so it is embedded in
                         * an array of size ncols x bufrows
                         * This is done as an in-place move from the end of the buffer
                         * toward the beginning.
                         */

                        xoffset = fpixel[0] - x0;
                        yoffset = fpixel[1] - j0;
                        pstart = pstart - xoffset - cols_read*yoffset;

                        /* leading empty rows */
                        for (j = 0; j < yoffset; j++) {
                            for (i = 0; i < ncols; i++) bufferptr[i + j*ncols] = NAN;
                        }
                        for (j = yoffset; j < yoffset+rows_read; j++) {
                            /* leading empty columns */
                            for (i = 0; i < xoffset; i++) {
                                bufferptr[i+j*ncols] = NAN;
                            }
                            /* copy block of pixels */
                            for (i = xoffset; i < xoffset+cols_read; i++) {
                                bufferptr[i+j*ncols] = bufferptr[pstart + i + cols_read*j];
                            }
                            /* trailing empty columns */
                            for (i = xoffset+cols_read; i < ncols; i++) {
                                bufferptr[i+j*ncols] = NAN;
                            }
                        }
                        /* trailing empty rows */
                        for (j = yoffset+rows_read; j < bufrows; j++) {
                            for (i = 0; i < ncols; i++) bufferptr[i + j*ncols] = NAN;
                        }
                    }
                }

                if (doshrink) {
                    reduce_array(bufferptr, &arrayptr[(j0-y0)/pixfac*zoomcols], ncols, bufrows, pixfac, Image->bad_data_value[k]);
                } else if (pixfac > 1) {
                    enlarge_array(bufferptr, &arrayptr[(j0-y0)*pixfac*zoomcols], ncols, bufrows, pixfac);
                }
            }
        }

        Image->data[k] = arrayptr;
        Image->ncols[k] = zoomcols;
        Image->nrows[k] = zoomrows;
        if (doshrink) {
            Image->output_zoom[k] = 1.0/pixfac;
        } else if (pixfac > 1) {
            Image->output_zoom[k] = pixfac;
        } else {
            Image->output_zoom[k] = 1.0;
        }

        if (pixfac > 1 && cols_read > 0 && rows_read > 0) {
            free(bufferptr);
        }

        if (nbad) fitscut_message (2, "\tZeroed %d bad pixels\n", nbad);

        if (fits_close_file (fptr, &status)) 
            printerror (status);

        if (dqptr != NULL) {
           if (fits_close_file (dqptr, &status)) 
                printerror (status);
        }

        /*
         * exact output size resampling and size test are skipped if WCS resampling is
         * requested -- rely on that step to match things up and only do a single resampling
         * step
         */
        if (Image->output_alignment == ALIGN_NONE) {

            if (Image->output_size > 0) {

                /* do final resize to exact output size */

                fitscut_message (2, "Forcing output size %d\n", Image->output_size);
                fitscut_message (2, "\tresizing channel %d\n", k);
                exact_resize_image_channel (Image, k, Image->output_size);
            }

            /* check for size consistency */
            if (Image->nrows[k] != Image->nrowsref || Image->ncols[k] != Image->ncolsref) {
                fitscut_message(0, "Error: color band %d is not the same size as reference band\n", k);
                fitscut_message(0, "Band ref size %d %d\nBand %d   size %d %d\n",
                        Image->nrowsref, Image->ncolsref, k, Image->nrows[k], Image->ncols[k]);
                do_exit (1);
            }
        }
        /* update world coordinate systems if cutout or zoom was used */
        wcs_update_channel(Image, k);
    }

    if (ngoodimages == 0) {
        /* error if cutout is off edge for all planes */
        fitscut_message (0, "Some image dimensions are negative for all planes\n");
        do_exit (1);
    }

    if (Image->output_size > 0 && Image->output_alignment == ALIGN_REF) {

        /* do final reference coordinate system resize to exact output size */

        /* restore the original WCS, do the resize, and compute new WCS */
        memcpy(Image->wcsref, save_wcs, sizeof(struct WorldCoor));
        exact_resize_reference (Image, Image->output_size);
        wcs_update_ref(Image);
        free(save_wcs);
    }

}

/*
 * Allocate memory for cutout
 */
float *
cutout_alloc (unsigned int nx, unsigned int ny, float value)
{
    unsigned int nelem;
    float *ptr;
    int i;

    nelem = nx * ny;
    ptr = (float *) malloc (nelem*sizeof(float));
    if (ptr == NULL) {
        fitscut_message (0, "Unable to allocate memory for %d x %d image\n",
                 nx, ny);
        do_exit (1);
    }
    for (i=0; i<nelem; i++) ptr[i] = value;
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
    char card[FLEN_CARD];
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

void
fits_get_badpix (char *header, int num_cards, float *badmin, float *badmax, float *bad_data_value)
{
    int status = 0;
    char keyname[FLEN_KEYWORD];
    int i,namelen;
    char card[FLEN_CARD];
    char value_string[FLEN_VALUE];
    char comment[FLEN_COMMENT];
    double value;
    float new_bad_data_value;

    *badmin = *bad_data_value;
    *badmax = *bad_data_value;
    if (header != NULL) {
        keyname[0] = '\0';
        card[FLEN_CARD-1] = '\0';
 
        for (i = 0; i < num_cards; i++) {
            strncpy (card, header + i * (FLEN_CARD - 1), FLEN_CARD - 1);
            fits_get_keyname (card, keyname, &namelen, &status);
            if (strequ (keyname, "BADPIX")) {
                fits_parse_value (card, value_string, comment, &status);
                /* using strtod because strtof is not always declared */
                *bad_data_value = (float) strtod (value_string, (char **)NULL);
                *badmin = *bad_data_value;
                *badmax = *bad_data_value;
                fitscut_message (2, "Found BADPIX=%f\n", *bad_data_value);
                break;
            }
        }
        /* look for GOODMIN, GOODMAX too */
        for (i = 0; i < num_cards; i++) {
            strncpy (card, header + i * (FLEN_CARD - 1), FLEN_CARD - 1);
            fits_get_keyname (card, keyname, &namelen, &status);
            if (strequ (keyname, "GOODMIN")) {
                fits_parse_value (card, value_string, comment, &status);
                value = strtod (value_string, (char **)NULL);
                fitscut_message (2, "Found GOODMIN=%f\n", value);
                *badmin = value;
            } else if (strequ (keyname, "GOODMAX")) {
                fits_parse_value (card, value_string, comment, &status);
                value = strtod (value_string, (char **)NULL);
                fitscut_message (2, "Found GOODMAX=%f\n", value);
                *badmax = value;
            }
        }
        if (*badmin >= 0 && *badmin != *bad_data_value) {
            /* unreliable values */
            *badmin = *bad_data_value;
            *badmax = *bad_data_value;
        }
        if (*bad_data_value != (long) (*bad_data_value)) {
            /*
             * if BADPIX is anything except an integer, it just won't
             * work due to roundoff errors
             * set BADPIX to either a value < badmin or zero in that case
             */
            if (*badmin == *bad_data_value) {
                /* just use zero, this is hopeless */
                *bad_data_value = 0.0;
                *badmin = 0.0;
                *badmax = 0.0;
            } else {
                /*
                 * note badmin < 0 in this case
                 * make new bad value smaller than badmin and
                 * ensure it is an integer
                 */
                new_bad_data_value = 2*((long) (*badmin)) - 1;
                if (*badmax == *bad_data_value) {
                    *badmax = new_bad_data_value;
                }
                *bad_data_value = new_bad_data_value;
            }
            fitscut_message (2, "Reset BADPIX to %f\n", *bad_data_value);
        }
    }
}
