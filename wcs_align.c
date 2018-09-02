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

#include "fitscut.h"
#include <libwcs/wcs.h>
#include "wcs_align.h"

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

void
wcs_initialize_channel (FitsCutImage *Image, int k)
{
        double xpix, ypix;

        xpix = Image->x0[k] + Image->ncols[k] / 2.0;
        ypix = Image->y0[k] + Image->nrows[k] / 2.0;
  
        if (Image->header[k] != NULL) {
                struct WorldCoor *wcs;

                wcs = wcsinit (Image->header[k]);
                if (iswcs (wcs)) {
                        /*
                          struct WorldCoor *wcs_new;
                          double            cra, cdec;
                        */

                        Image->wcs[k] = wcs;
#if 0      
                        if (verbose)
                                wcscent (wcs);
      
                        pix2wcs (wcs, xpix, ypix, &cra, &cdec);

                        fitscut_message (2,"\tNew center [%f,%f] -> %f %f\n",
                                         xpix, ypix, cra, cdec);
      
                        /*wcsshift (wcs,cra,cdec,"FK5");*/
                        wcs_new = wcskinit (Image->ncols[k], Image->nrows[k],
                                            wcs->ctype[0],wcs->ctype[1],
                                            xpix, ypix, cra, cdec, wcs->cd,
                                            wcs->cdelt[0], wcs->cdelt[1],
                                            wcs->rot, wcs->equinox, wcs->radecsys);
                        if (verbose)
                                wcscent (wcs_new);
                        wcsfree (wcs);
                        Image->wcs[k] = wcs_new;
#endif
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

void
wcs_align_first_channel (FitsCutImage *Image, int k)
{
  
        if (Image->wcs[0] == NULL)
                return;

        if (iswcs(Image->wcs[0]) == 0)
                return;

        wcs_remap_channel (Image, Image->wcs[0], k);
}

void
wcs_align_first (FitsCutImage *Image)
{
        int k;
  
        if (Image->wcs[0] == NULL)
                return;

        if (iswcs (Image->wcs[0]) == 0)
                return;

        for (k = 1; k < Image->channels; k++) {
                fitscut_message (2, "\t\tremapping channel %d\n", k);
                wcs_remap_channel (Image, Image->wcs[0], k);
        }
}

int
wcs_remap_channel (FitsCutImage *Image, struct WorldCoor *wcs_out, int channel)
{
        struct WorldCoor *wcs_in;
        int wpin, hpin;
        int offscl;
        int iin, iout, jin, jout;
        int iout1, iout2, jout1, jout2;

        double xout, yout, xin, yin, xpos, ypos, dpixi;
        double xmin, xmax, ymin, ymax;

        int nbytes;
        int bitpix,bitpix_out;
        int ncols_in, nrows_in;
        int ncols_out, nrows_out;

        char *image_out, *image;

        image = (char *) Image->data[channel];
        wcs_in = Image->wcs[channel];
        bitpix = -32;
        bitpix_out = -32;
        ncols_in = Image->ncols[channel];
        nrows_in = Image->nrows[channel];

        /* Allocate space for output image */
        ncols_out = Image->ncols[channel];
        nrows_out = Image->nrows[channel];

        fitscut_message (3, "\t\tCreating temp image [%d,%d]\n", ncols_out, nrows_out);

        nbytes = ncols_out * nrows_out * sizeof (float);
        image_out = (char *) malloc (nbytes);
        memset (image_out, 0, nbytes);

        /* Set input WCS output coordinate system to output coordinate system*/
        wcs_in->sysout = wcs_out->syswcs;
        strcpy (wcs_in->radecout, wcs_out->radecsys);
        wpin = wcs_in->nxpix;
        hpin = wcs_in->nypix;

        /* Set output WCS output coordinate system to input coordinate system*/
        wcs_out->sysout = wcs_in->syswcs;

        /* Find limiting edges of input image in output image */
        pix2wcs (wcs_in, 1.0, 1.0, &xpos, &ypos);
        wcs2pix (wcs_out, xpos, ypos, &xout, &yout, &offscl);
        xmin = xout;
        xmax = xout;
        ymin = yout;
        ymax = yout;
        pix2wcs (wcs_in, 1.0, (double)hpin, &xpos, &ypos);
        wcs2pix (wcs_out, xpos, ypos, &xout, &yout, &offscl);
        if (xout < xmin) xmin = xout;
        if (xout > xmax) xmax = xout;
        if (yout < ymin) ymin = yout;
        if (yout > ymax) ymax = yout;
        pix2wcs (wcs_in, (double)wpin, 1.0, &xpos, &ypos);
        wcs2pix (wcs_out, xpos, ypos, &xout, &yout, &offscl);
        if (xout < xmin) xmin = xout;
        if (xout > xmax) xmax = xout;
        if (yout < ymin) ymin = yout;
        if (yout > ymax) ymax = yout;
        pix2wcs (wcs_in, (double)wpin, (double)hpin, &xpos, &ypos);
        wcs2pix (wcs_out, xpos, ypos, &xout, &yout, &offscl);
        if (xout < xmin) xmin = xout;
        if (xout > xmax) xmax = xout;
        if (yout < ymin) ymin = yout;
        if (yout > ymax) ymax = yout;
        iout1 = (int) (ymin + 0.5);
        if (iout1 < 1) iout1 = 1;
        iout2 = (int) (ymax + 0.5);
        if (iout2 > nrows_out) iout2 = nrows_out;
        jout1 = (int) (xmin + 0.5);
        if (jout1 < 1) jout1 = 1;
        jout2 = (int) (xmax + 0.5);
        if (jout2 > ncols_out) jout2 = ncols_out;

        fitscut_message (3, "REMAP: Output x: %d-%d, y: %d-%d\n",
                         jout1, jout2, iout1, iout2);

        /* Loop through vertical pixels (output image lines) */
        for (iout = iout1; iout <= iout2; iout++) {
                yout = (double) iout;

                /* Loop through horizontal pixels (output image columns) */
                for (jout = jout1; jout <= jout2; jout++) {
                        xout = (double) jout;

                        /* Get WCS coordinates of this pixel in output image */
                        pix2wcs (wcs_out, xout, yout, &xpos, &ypos);

                        /* Get image coordinates of this subpixel in input image */
                        wcs2pix (wcs_in, xpos, ypos, &xin, &yin, &offscl);
                        if (!offscl) {
                                iin = (int) (yin + 0.5);
                                jin = (int) (xin + 0.5);

                                /* Read pixel from input */
                                dpixi = getpix (image, bitpix, ncols_in,
                                                nrows_in, 0.0, 1.0, jin, iin);

                                /* Write pixel to output */
                                addpix (image_out, bitpix_out, ncols_out,
                                        nrows_out, 0.0, 1.0, jout, iout, dpixi);
                        }
                }
        }

        free (Image->data[channel]);
        Image->data[channel] = (float *) image_out;

        return (0);
}

/* GETPIX -- Get pixel from 2D image of any numeric type */

double
getpix (char   *image,		/* Image array as 1-D vector */
        int	bitpix,		/* FITS bits per pixel */
        /*  16 = short, -16 = unsigned short, 32 = int */
        /* -32 = float, -64 = double */
        int	w,		/* Image width in pixels */
        int	h,		/* Image height in pixels */
        double  bzero,		/* Zero point for pixel scaling */
        double  bscale,		/* Scale factor for pixel scaling */
        int	x,		/* Zero-based horizontal pixel number */
        int	y)		/* Zero-based vertical pixel number */
        
{
        short *im2;
        int *im4;
        unsigned short *imu;
        float *imr;
        double *imd;
        double dpix;

        /* Return 0 if coordinates are not inside image */
        if (x < 0 || x >= w)
                return (0.0);
        if (y < 0 || y >= h)
                return (0.0);

        /* Extract pixel from appropriate type of array */
        switch (bitpix) {

	case 8:
                dpix = (double) image[(y*w) + x];
                break;

	case 16:
                im2 = (short *)image;
                dpix = (double) im2[(y*w) + x];
                break;

	case 32:
                im4 = (int *)image;
                dpix = (double) im4[(y*w) + x];
                break;

	case -16:
                imu = (unsigned short *)image;
                dpix = (double) imu[(y*w) + x];
                break;

	case -32:
                imr = (float *)image;
                dpix = (double) imr[(y*w) + x];
                break;

	case -64:
                imd = (double *)image;
                dpix = imd[(y*w) + x];
                break;

	default:
                dpix = 0.0;
	}
        return (bzero + (bscale * dpix));
}

/* ADDPIX -- Add pixel value into 2D image of any numeric type */

void
addpix (char   *image,
        int	bitpix,		/* Number of bits per pixel */
        /*  16 = short, -16 = unsigned short, 32 = int */
        /* -32 = float, -64 = double */
        int	w,		/* Image width in pixels */
        int	h,		/* Image height in pixels */
        double  bzero,		/* Zero point for pixel scaling */
        double  bscale,		/* Scale factor for pixel scaling */
        int	x,		/* Zero-based horizontal pixel number */
        int	y,		/* Zero-based vertical pixel number */
        double	dpix)		/* Value to add to pixel */
        
{
        short *im2;
        int *im4;
        unsigned short *imu;
        float *imr;
        double *imd;
        int ipix;

        /* Return if coordinates are not inside image */
        if (x < 0 || x >= w)
                return;
        if (y < 0 || y >= h)
                return;

        dpix = (dpix - bzero) / bscale;
        ipix = (y * w) + x;

        switch (bitpix) {

	case 8:
                if (dpix < 0)
                        image[ipix] = image[ipix] + (char) (dpix - 0.5);
                else
                        image[ipix] = image[ipix] + (char) (dpix + 0.5);
                break;

	case 16:
                im2 = (short *)image;
                if (dpix < 0)
                        im2[ipix] = im2[ipix] + (short) (dpix - 0.5);
                else
                        im2[ipix] = im2[ipix] + (short) (dpix + 0.5);
                break;

	case 32:
                im4 = (int *)image;
                if (dpix < 0)
                        im4[ipix] = im4[ipix] + (int) (dpix - 0.5);
                else
                        im4[ipix] = im4[ipix] + (int) (dpix + 0.5);
                break;

	case -16:
                imu = (unsigned short *)image;
                if (dpix > 0)
                        imu[ipix] = imu[ipix] + (unsigned short) (dpix + 0.5);
                break;

	case -32:
                imr = (float *)image;
                imr[ipix] = imr[ipix] + (float) dpix;
                break;

	case -64:
                imd = (double *)image;
                imd[ipix] = imd[ipix] + dpix;
                break;

	}
        return;
}
