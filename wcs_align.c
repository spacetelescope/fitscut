/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * Functions for remapping an image based on WCS info
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
 * $Id: wcs_align.c,v 1.7 2004/04/21 21:28:47 mccannwj Exp $
 */

/* make lround work ok with old gcc on linux */
#define _ISOC99_SOURCE

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <math.h>
#include "getopt.h"
#include <signal.h>
#include <ctype.h>

#ifdef HAVE_CFITSIO_FITSIO_H
#include <cfitsio/fitsio.h> 
#else
#include <fitsio.h>
#endif

#include "fitscut.h"
#include "extract.h"
#include <libwcs/wcs.h>
#include "wcs_align.h"

#ifdef  STDC_HEADERS
#include <stdlib.h>
#else   /* Not STDC_HEADERS */
extern void exit ();
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

void
wcs_initialize_channel (FitsCutImage *Image, int k)
{
        if (Image->header[k] != NULL) {
                struct WorldCoor *wcs;

                wcs = wcsninit (Image->header[k], Image->header_cards[k]*80);
                if (iswcs (wcs)) {
                        Image->wcs[k] = wcs;
                }
        }
}

void
wcs_initialize (FitsCutImage *Image)
{
        int k;

        for (k = 0; k < Image->channels; k++)
                wcs_initialize_channel (Image, k);
}

/* open FITS file, extract the WCS, and close it
 * also returns the image dimensions in naxes array
 */

struct WorldCoor *wcs_read(char *filename, long *naxes)
{
    fitsfile *fptr;
    char *header;
    int status = 0;
    struct WorldCoor *wcs;

    if (fits_open_image (&fptr, filename, READONLY, &status)) 
        printerror (status);

    /* let cfitsio extract the entire header as a string */
    if (fits_get_image_wcs_keys (fptr, &header, &status))
        printerror (status);

    if (fits_get_img_size (fptr, 2, naxes, &status))
        printerror (status);

    if (fits_close_file(fptr, &status))
        printerror (status);

    /* get world coordinate system info from header */
    wcs = wcsinit (header);

    if (nowcs(wcs)) {
        fitscut_message (1,
            "fitscut: warning: no WCS info for file %s\n", filename);
    }

    free(header);
    return (wcs);
}

void
wcs_initialize_ref (FitsCutImage *Image, long *naxes)
{
    Image->wcsref = wcs_read(Image->reference_filename, naxes);
}

void
wcs_align_ref (FitsCutImage *Image)
{
        int k;
  
        if (Image->wcsref == NULL)
                return;

        if (nowcs (Image->wcsref))
                return;

        for (k = 0; k < Image->channels; k++) {
            if (Image->data[k] != NULL) {
                fitscut_message (2, "\t\tremapping channel %d\n", k);
                wcs_remap_channel (Image, k);
            }
        }
}

/* returns true if 2 WCS systems are the same */

static int
wcs_equal(struct WorldCoor *wcs1, struct WorldCoor *wcs2)
{
    return ( wcs1 == wcs2 || (
            wcs1->nxpix == wcs2->nxpix &&
            wcs1->nypix == wcs2->nypix &&
            wcs1->yref == wcs2->yref &&
            wcs1->xrefpix == wcs2->xrefpix &&
            wcs1->yrefpix == wcs2->yrefpix &&
            wcs1->xinc == wcs2->xinc &&
            wcs1->yinc == wcs2->yinc &&
            wcs1->rot == wcs2->rot &&
            wcs1->equinox == wcs2->equinox &&
            wcs1->cd[0] == wcs2->cd[0] &&
            wcs1->cd[1] == wcs2->cd[1] &&
            wcs1->cd[2] == wcs2->cd[2] &&
            wcs1->cd[3] == wcs2->cd[3] &&
            wcs1->wcsproj == wcs2->wcsproj
       ));
}

/* convert pixel coordinates in wcs_in to pixel coordinates in wcs_out */

static void
pix2pix(struct WorldCoor *wcs_in, double x_in, double y_in, struct WorldCoor *wcs_out, double *x_out, double *y_out, int *offscl)
{
    double xpos, ypos;

    pix2wcs (wcs_in, x_in, y_in, &xpos, &ypos);
    wcs2pix (wcs_out, xpos, ypos, x_out, y_out, offscl);
}

int
wcs_remap_channel (FitsCutImage *Image, int channel)
{
        struct WorldCoor *wcs_in, *wcs_out;
        int offscl;
        int iin, iout, jin, jout;
        int iout1, iout2, jout1, jout2;

        double xout, yout, xin, yin;
        double xmin, xmax, ymin, ymax;
        double x0, y0, x1, y1;

        int ncols_in, nrows_in;
        int ncols_out, nrows_out;

        float *image_out, *image;

        wcs_out = Image->wcsref;
        wcs_in = Image->wcs[channel];
        if (wcs_equal(wcs_in, wcs_out)) {
            fitscut_message (3, "\t\tWCS for channel %d matches reference image\n", channel);
            return(0);
        }

        image = Image->data[channel];
        ncols_in = Image->ncols[channel];
        nrows_in = Image->nrows[channel];

        /* Allocate space for output image */
        ncols_out = Image->ncolsref;
        nrows_out = Image->nrowsref;

        fitscut_message (3, "\t\tCreating temp image [%d,%d]\n", ncols_out, nrows_out);

        image_out = cutout_alloc(ncols_out, nrows_out, NAN);

        /* Set input WCS output coordinate system to output coordinate system */
        wcs_in->sysout = wcs_out->syswcs;
        strcpy (wcs_in->radecout, wcs_out->radecsys);

        /* Set output WCS output coordinate system to input coordinate system */
        wcs_out->sysout = wcs_in->syswcs;

        /* Find limiting edges of input image in output image */
        x0 = 0.5;
        y0 = 0.5;
        x1 = wcs_in->nxpix+0.5;
        y1 = wcs_in->nypix+0.5;

        pix2pix(wcs_in, x0, y0, wcs_out, &xout, &yout, &offscl);
        xmin = xout;
        xmax = xout;
        ymin = yout;
        ymax = yout;
        pix2pix(wcs_in, x0, y1, wcs_out, &xout, &yout, &offscl);
        if (xout < xmin) { xmin = xout; } else if (xout > xmax) { xmax = xout; }
        if (yout < ymin) { ymin = yout; } else if (yout > ymax) { ymax = yout; }
        pix2pix(wcs_in, x1, y0, wcs_out, &xout, &yout, &offscl);
        if (xout < xmin) { xmin = xout; } else if (xout > xmax) { xmax = xout; }
        if (yout < ymin) { ymin = yout; } else if (yout > ymax) { ymax = yout; }
        pix2pix(wcs_in, x1, y1, wcs_out, &xout, &yout, &offscl);
        if (xout < xmin) { xmin = xout; } else if (xout > xmax) { xmax = xout; }
        if (yout < ymin) { ymin = yout; } else if (yout > ymax) { ymax = yout; }
        iout1 = (int) ceil(ymin);
        iout2 = (int) floor(ymax);
        jout1 = (int) ceil(xmin);
        jout2 = (int) floor(xmax);
        if (iout1 < 1) iout1 = 1;
        if (iout2 > nrows_out) iout2 = nrows_out;
        if (jout1 < 1) jout1 = 1;
        if (jout2 > ncols_out) jout2 = ncols_out;

        fitscut_message (3, "REMAP: Output x: %d-%d, y: %d-%d\n",
                         jout1, jout2, iout1, iout2);

        /* Loop through vertical pixels (output image lines) */
        for (iout = iout1; iout <= iout2; iout++) {
            yout = (double) iout;

            /* Loop through horizontal pixels (output image columns) */
            for (jout = jout1; jout <= jout2; jout++) {
                xout = (double) jout;

                /* Get image coordinates of this pixel in input image */
                pix2pix(wcs_out, xout, yout, wcs_in, &xin, &yin, &offscl);

                if (!offscl) {
                    iin = lround(yin);
                    jin = lround(xin);
                    if (iin >= 1 && iin <= nrows_in && jin >= 1 && jin <= ncols_in) {
                        /* Copy pixel from input to output */
                        image_out[(jout-1)+(iout-1)*ncols_out] = image[(jin-1)+(iin-1)*ncols_in];
                    }
                }
            }
        }

        free (Image->data[channel]);
        Image->data[channel] = (float *) image_out;
        Image->ncols[channel] = ncols_out;
        Image->nrows[channel] = nrows_out;

        /* update the wcs for this channel */
        Image->wcs[channel] = Image->wcsref;
        Image->output_zoom[channel] = Image->output_zoomref;
        Image->x0[channel] = Image->x0ref;
        Image->y0[channel] = Image->y0ref;

        return (0);
}

/* modify image section for channel to match reference image using WCS */

int
wcs_match_channel (FitsCutImage *Image, int channel)
{
        struct WorldCoor *wcs_chan, *wcs_ref;
        int offscl;
        int iout1, iout2, jout1, jout2;

        double xout, yout;
        double xmin, xmax, ymin, ymax;
        double x0, y0, x1, y1;

        wcs_chan = Image->wcs[channel];
        wcs_ref = Image->wcsref;
        if (wcs_equal(wcs_ref, wcs_chan)) {
            fitscut_message (3, "\t\tWCS for channel %d matches reference image\n", channel);
            return(0);
        }

        /* Set reference WCS output coordinate system to channel coordinate system*/
        wcs_ref->sysout = wcs_chan->syswcs;
        strcpy (wcs_ref->radecout, wcs_chan->radecsys);

        /* Set channel WCS output coordinate system to reference coordinate system*/
        wcs_chan->sysout = wcs_ref->syswcs;

        /* Find limiting edges of reference image in channel image */
        x0 = 0.5;
        y0 = 0.5;
        x1 = Image->ncolsref+0.5;
        y1 = Image->nrowsref+0.5;
        pix2pix(wcs_ref, x0, y0, wcs_chan, &xout, &yout, &offscl);
        xmin = xout;
        xmax = xout;
        ymin = yout;
        ymax = yout;
        pix2pix(wcs_ref, x0, y1, wcs_chan, &xout, &yout, &offscl);
        if (xout < xmin) { xmin = xout; } else if (xout > xmax) { xmax = xout; }
        if (yout < ymin) { ymin = yout; } else if (yout > ymax) { ymax = yout; }
        pix2pix(wcs_ref, x1, y0, wcs_chan, &xout, &yout, &offscl);
        if (xout < xmin) { xmin = xout; } else if (xout > xmax) { xmax = xout; }
        if (yout < ymin) { ymin = yout; } else if (yout > ymax) { ymax = yout; }
        pix2pix(wcs_ref, x1, y1, wcs_chan, &xout, &yout, &offscl);
        if (xout < xmin) { xmin = xout; } else if (xout > xmax) { xmax = xout; }
        if (yout < ymin) { ymin = yout; } else if (yout > ymax) { ymax = yout; }

        /* make sure the image section is big enough to cover the region */
        iout1 = (int) floor(ymin);
        iout2 = (int) ceil(ymax);
        jout1 = (int) floor(xmin);
        jout2 = (int) ceil(xmax);

        fitscut_message (3, "REMAP: Channel %d Output x: %d-%d, y: %d-%d\n",
                         channel, jout1, jout2, iout1, iout2);

        Image->ncols[channel] = jout2-jout1+1;
        Image->nrows[channel] = iout2-iout1+1;
        Image->input_x_corner[channel] = 1;
        Image->input_y_corner[channel] = 1;
        Image->input_x[channel] = jout1-1;
        Image->input_y[channel] = iout1-1;

        return (0);
}


/* modify world coordinate system for cutout position and rebinning */

void
wcs_update (FitsCutImage *Image)
{
    int k;

    for (k = 0; k < Image->channels; k++)
        wcs_update_channel (Image, k);
}

void
wcs_apply_update (struct WorldCoor *wcs, double x0, double y0, double zoom, int ncols, int nrows)
{
    double cd[4], *cdptr = NULL, cdelt1 = 0, cdelt2 = 0, crota = 0;
    double crpix1, crpix2, crval1, crval2;
    int i;

    if (wcs == NULL) return;

    if (zoom == 0) zoom = 1;

    /* return if no change */
    if (x0 == 0 && y0 == 0 && zoom == 1) return;

    crpix1 = zoom*(wcs->crpix[0] - x0) - 0.5*(zoom-1);
    crpix2 = zoom*(wcs->crpix[1] - y0) - 0.5*(zoom-1);
    crval1 = wcs->crval[0];
    crval2 = wcs->crval[1];
    if (wcs->rotmat) {
        /* using rotation matrix */
        for (i=0; i<4; i++) cd[i] = wcs->cd[i]/zoom;
        cdptr = cd;
    } else {
        /* using CROTA, CDELT */
        cdelt1 = wcs->cdelt[0]/zoom;
        cdelt2 = wcs->cdelt[1]/zoom;
        crota = wcs->rot;
    }

    if (wcs->prjcode == WCS_DSS) {
        /* special code for DSS polynomial coordinate system
         * XXX Offsets could have an off-by-1/2 error, haven't figured out the details
         */
        wcs->x_pixel_offset = -zoom*(wcs->x_pixel_offset - x0);
        wcs->y_pixel_offset = -zoom*(wcs->y_pixel_offset - y0);
        wcs->x_pixel_size = wcs->x_pixel_size/zoom;
        wcs->y_pixel_size = wcs->y_pixel_size/zoom;
        wcs->crpix[0] = crpix1;
        wcs->crpix[1] = crpix2;
    } else {
        wcsreset(wcs, crpix1, crpix2, crval1, crval2, cdelt1, cdelt2, crota, cdptr);
    }
    wcs->nxpix = ncols;
    wcs->nypix = nrows;
}

void
wcs_update_channel (FitsCutImage *Image,  int k)
{
    wcs_apply_update(Image->wcs[k], Image->x0[k], Image->y0[k], Image->output_zoom[k], Image->ncols[k], Image->nrows[k]);
}

void
wcs_update_ref (FitsCutImage *Image)
{
    wcs_apply_update(Image->wcsref, Image->x0ref, Image->y0ref, Image->output_zoomref, Image->ncolsref, Image->nrowsref);
}
