/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * FITS output functions
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
 * $Id: output_fits.c,v 1.11 2004/05/11 22:05:06 mccannwj Exp $
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
#include "output_fits.h"
#include "extract.h"
#include "revision.h"

static int
fitscut_write_primary (fitsfile *fptr,
                       float **dataptr,
                       int bitpix,
                       int nrows,
                       int ncols,
                       int channels)
{
        int status;
        long fpixel[3];
        long naxis;
        long nelements;
        long naxes[3];

        long tablerow, nfits, narray, ii, k;

        fpixel[0] = 1;
        fpixel[1] = 1;
        fpixel[2] = 1;

        naxis = (channels == 1) ? 2 : 3;
        naxes[0] = ncols;
        naxes[1] = nrows;
        naxes[2] = channels;
        nelements = naxes[0] * naxes[1] * naxes[2]; /* number of pixels to write */

        status = 0;
        /* Create the primary array image */
        if (fits_create_img (fptr, bitpix, naxis, naxes, &status))
                printerror (status);

        /*
          the primary array is represented as a binary table:
          each group of the primary array is a row in the table,
          where the first column contains the group parameters
          and the second column contains the image itself.
        */
        tablerow = 1;
        nfits = 1;   /* next pixel in FITS image to write to */
        narray = 0;  /* next pixel in input array to be written */
        for (k = 0; k < channels; k++) {
                if (dataptr[k] == NULL) 
                        continue;
                /* loop over the naxis2 rows in the FITS image, */
                /* writing naxis1 pixels to each row            */
    
                for (ii = 0; ii < nrows; ii++) {
                        if (fits_write_col (fptr, TFLOAT, 2, tablerow, nfits,
                                            ncols, dataptr[k] + narray, &status) > 0)
                                return (status);

                        nfits += naxes[0];
                        narray += ncols;
                }
                narray = 0;
        }

        return (status);
}

static int
fitscut_create_fits (char *ofname, fitsfile **fptrptr)
{
        int status = 0;

        if (strlen (ofname) <= 0) {
                fitscut_error ("output filename is null.");
        }

        if (fits_create_file (fptrptr, ofname, &status))   /* create new file */
                printerror (status);           /* call printerror if error occurs */
        return (status);
}

static int
fitscut_write_header (fitsfile *fptr, char *header, int num_cards)
{
        int status = 0;
        char keyname[FLEN_KEYWORD];
        int i, namelen;
        char card[FLEN_CARD+1];

        /* Write a keyword; must pass the ADDRESS of the value */
        if (header != NULL) {
                keyname[0] = '\0';
                fits_write_date (fptr, &status);
 
                for (i = 0; i < num_cards; i++) {
                        strncpy (card, header + i * (FLEN_CARD - 1), FLEN_CARD - 1);
                        fits_get_keyname (card, keyname, &namelen, &status);
                        /*fprintf(stderr," keyname: %s \n",keyname);*/
                        if ( (!strequ (keyname,"SIMPLE")) &&
                             (!strequ (keyname,"BITPIX")) &&
                             (!strequ (keyname,"NAXIS")) &&
                             (!strequ (keyname,"NAXIS1")) &&
                             (!strequ (keyname,"NAXIS2")) &&
                             (!strequ (keyname,"NAXIS3")) &&
                             (!strequ (keyname,"EXTEND")) &&
                             (!strequ (keyname,"HISTORY")) &&
                             (!strequ (keyname,"COMMENT")) &&
                             (!strequ (keyname,"")) &&
                             (!strequ (keyname,"DATE")) )
                                fits_write_record (fptr, card, &status);
                }
        }

        return (status);
}

static int
fitscut_close_fits (fitsfile *fptr)
{
        int status = 0;

        if (fits_close_file (fptr, &status))
                printerror (status);
        return (status);
}

void
write_to_fits(FitsCutImage *Image)
{
        fitsfile *fptr;
        int bitpix;
        int status = 0;
        char history[64];

        fitscut_message (1, "\tCreating FITS...\n");
        fitscut_create_fits (Image->output_filename, &fptr);

        fitscut_message (2, "\tWriting primary...\n");
        if (Image->output_scale == SCALE_LINEAR) {
                /* use the input bitpix */
                /* have to pick one, so pick first one */
                bitpix = Image->input_datatype[0];
        }
        else {
                bitpix = FLOAT_IMG;
        }
        fitscut_write_primary (fptr, Image->data, bitpix,
                               Image->nrows[0], Image->ncols[0], Image->channels);

        /* write header */
        /* for now I'm not going to try to merge the headers 
         * just use the first one */
        fitscut_write_header (fptr, Image->header[0], Image->header_cards[0]);

        sprintf (history, "Created by fitscut %s (William Jon McCann)", VERSION);

        fits_write_history (fptr, history, &status);

        fitscut_message  (1, "\tClosing file...\n");
        fitscut_close_fits (fptr);
}
