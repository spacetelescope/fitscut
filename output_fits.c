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
#include "blurb.h"
#include "extract.h"
#include "revision.h"

static void
fitscut_create_primary (fitsfile *fptr,
                       int bitpix,
                       int nrows,
                       int ncols,
                       int channels)
{
    int status = 0;
    long naxis;
    long naxes[3];

    naxis = (channels == 1) ? 2 : 3;
    naxes[0] = ncols;
    naxes[1] = nrows;
    naxes[2] = channels;

    /* Create the primary array image */
    if (fits_create_img (fptr, bitpix, naxis, naxes, &status))
            printerror (status); /* note printerror exits with error status */
}

static void
fitscut_write_data (fitsfile *fptr,
                       float **dataptr,
                       int nrows,
                       int ncols,
                       int channels)
{
    int status = 0;
    long naxes[3], fpixel[3], nelements;
    long ii, jj, k;
    float *avgrow;

    naxes[0] = ncols;
    naxes[1] = nrows;
    naxes[2] = channels;
    nelements = naxes[0]*naxes[1];
    for (k=0; k < channels; k++) {
        fpixel[0] = 1;
        fpixel[1] = 1;
        fpixel[2] = k + 1;
        if (dataptr[k] != NULL) {
            if (fits_write_pix(fptr, TFLOAT, fpixel, nelements, dataptr[k], &status))
                printerror (status);
        } else {

            /* write mean green channel if possible */
            if (k==1 && channels==3 && dataptr[0] != NULL && dataptr[2] != NULL) {
                avgrow = (float *) malloc(ncols*sizeof(float));
                for (ii = 0; ii < nrows; ii++) {
                    for (jj = 0; jj < ncols; jj++) {
                        avgrow[jj] = 0.5*(dataptr[0][ncols*ii+jj] + dataptr[2][ncols*ii+jj]);
                    }
                    fpixel[1] = ii+1;
                    if (fits_write_pix(fptr, TFLOAT, fpixel, ncols, avgrow, &status))
                        printerror (status);
                }
                free(avgrow);
            }
        }
    }
}

static void
fitscut_create_fits (char *ofname, fitsfile **fptrptr)
{
        int status = 0;

        if (strlen (ofname) <= 0) {
                fitscut_error ("output filename is null.");
        }

        if (fits_create_file (fptrptr, ofname, &status))   /* create new file */
                printerror (status);           /* call printerror if error occurs */
}

static void
fitscut_write_header (fitsfile *fptr, char *header, int num_cards)
{
        int status = 0;
        char keyname[FLEN_KEYWORD];
        int i, namelen;
        char card[FLEN_CARD];

        /* Write a keyword; must pass the ADDRESS of the value */
        if (header != NULL) {
                keyname[0] = '\0';
                if (fits_write_date (fptr, &status))
                    printerror (status);
 
                for (i = 0; i < num_cards; i++) {
                        strncpy (card, header + i * (FLEN_CARD - 1), FLEN_CARD - 1);
                        if (fits_get_keyname (card, keyname, &namelen, &status))
                            printerror (status);
                        /*fprintf(stderr," keyname: %s \n",keyname);*/
                        if (strequ(keyname,"END")) break;
                        if ( (!strequ (keyname,"SIMPLE")) &&
                             (!strequ (keyname,"BITPIX")) &&
                             (!strequ (keyname,"NAXIS")) &&
                             (!strequ (keyname,"NAXIS1")) &&
                             (!strequ (keyname,"NAXIS2")) &&
                             (!strequ (keyname,"NAXIS3")) &&
                             (!strequ (keyname,"NAXIS4")) &&
                             (!strequ (keyname,"NAXIS5")) &&
                             (!strequ (keyname,"NAXIS6")) &&
                             (!strequ (keyname,"NAXIS7")) &&
                             (!strequ (keyname,"EXTEND")) &&
                             (!strequ (keyname,"DATE")) ) {
                                if (fits_write_record (fptr, card, &status))
                                    printerror(status);
                        }
                }
        }
}

static void
fitscut_close_fits (fitsfile *fptr)
{
        int status = 0;

        if (fits_close_file (fptr, &status))
                printerror (status);
}

/*
 * delete FITS keyword from header
 * it's OK if the keyword does not exist
 */
static void
fitscut_delete_key(fitsfile *fptr, char *keyword, int *status) {
    if (*status) return;
    fits_delete_key(fptr, keyword, status);
    if (*status == KEY_NO_EXIST) *status = 0;
}

/*
 * clean extraneous keywords out of the header
 */
static void
fitscut_clean_header(fitsfile *fptr, FitsCutImage *Image, int *status) {

    /* integer scaling */
    fitscut_delete_key(fptr, "BZERO", status);
    fitscut_delete_key(fptr, "BSCALE", status);
    fitscut_delete_key(fptr, "BLANK", status);

    /* compression */
    fitscut_delete_key(fptr, "ZBLANK", status);
    fitscut_delete_key(fptr, "TFIELDS", status);
    fitscut_delete_key(fptr, "TTYPE1", status);
    fitscut_delete_key(fptr, "TFORM1", status);
    fitscut_delete_key(fptr, "ZIMAGE", status);
    fitscut_delete_key(fptr, "ZSIMPLE", status);
    fitscut_delete_key(fptr, "ZBITPIX", status);
    fitscut_delete_key(fptr, "ZNAXIS", status);
    fitscut_delete_key(fptr, "ZNAXIS1", status);
    fitscut_delete_key(fptr, "ZNAXIS2", status);
    fitscut_delete_key(fptr, "ZTILE1", status);
    fitscut_delete_key(fptr, "ZTILE2", status);
    fitscut_delete_key(fptr, "ZCMPTYPE", status);
    fitscut_delete_key(fptr, "ZNAME1", status);
    fitscut_delete_key(fptr, "ZVAL1", status);
    fitscut_delete_key(fptr, "ZNAME2", status);
    fitscut_delete_key(fptr, "ZVAL2", status);

    /* extension info */
    fitscut_delete_key(fptr, "XTENSION", status);
    fitscut_delete_key(fptr, "EXTNAME", status);
    fitscut_delete_key(fptr, "EXTVER", status);
    fitscut_delete_key(fptr, "INHERIT", status);
    fitscut_delete_key(fptr, "PCOUNT", status);
    fitscut_delete_key(fptr, "GCOUNT", status);

    /* asinh scaling */
    if (Image->useBsoften) {
        fitscut_delete_key(fptr, "BSOFTEN", status);
        fitscut_delete_key(fptr, "BOFFSET", status);
    }

    /* catch all errors in above section */
    if (*status) printerror (*status);
}


/* return true if the keyword exists in the header */
static int
header_has_key(fitsfile *fptr, char *keyname, int *status)
{
char value[81];

    if (*status == 0) {
        fits_read_keyword(fptr, keyname, value, NULL, status);
        if (*status == VALUE_UNDEFINED || *status == KEY_NO_EXIST) {
            *status = 0;
            return 0; /* no such keyword */
        } else if (*status == 0) {
            return 1; /* keyword found */
        }
    }
    /* error in status */
    return 0;
}

void
write_to_fits(FitsCutImage *Image)
{
        fitsfile *fptr;
        int bitpix;
        int status = 0;
        char *last;
        int i, len;
        char history[64], retval[512];
        char *blurb = NULL;
        double crpix, cd1, cd2, zoom;

        fitscut_message (1, "\tCreating FITS...\n");
        fitscut_create_fits (Image->output_filename, &fptr);

        fitscut_message (2, "\tWriting primary...\n");

        /*
         * Force the bitpix to float regardless of the input image type.
         * This is necessary because the image rebinning could cause
         * integer values to overflow. There are also complicated issues
         * with the handling of blanks for non-float images.
         */
        bitpix = FLOAT_IMG;
        /*
         * clean scaling, compression & other keywords out of header
         */
        fitscut_clean_header (fptr, Image, &status);

        fitscut_create_primary (fptr, bitpix,
                               Image->nrowsref, Image->ncolsref, Image->channels);

        /* write header */
        /* for now I'm not going to try to merge the headers 
         * just use the first one */
        fitscut_write_header (fptr, Image->header[0], Image->header_cards[0]);

        if (Image->output_add_blurb) { /* add text message as HISTORY cards */
            if ((blurb = blurb_read (Image->input_blurbfile)) == NULL) {
                fitscut_message  (2, "\tProblem reading blurb file, header not updated\n");
            } else {
                
                len = strlen (blurb);
                for (i = 0, last = blurb; i < len ; ++i) {
                    if (blurb[i] == '\n') {
                        blurb[i] = '\0';
                        if (last == blurb + i) {
                            fits_write_history (fptr, " ", &status); // treat back-to-back carriage returns as blank lines
                        } else {
                            fits_write_history (fptr, last, &status);
                        }
                        last = blurb + i + 1;
                    }
                }
                free (blurb);
            }
        }

        sprintf (history, "Created by fitscut %s (William Jon McCann)", VERSION);

        fits_write_history (fptr, history, &status);

        /* update WCS info */

        zoom = Image->output_zoom[0];

        fits_read_key(fptr, TDOUBLE, "CRPIX1", &crpix, NULL, &status);
        if (status == 0) {
            crpix = crpix - Image->x0[0];
            if (zoom != 0 && zoom != 1) {
                crpix = zoom*crpix - 0.5*(zoom-1);
                fits_read_key(fptr, TDOUBLE, "CD1_1", &cd1, NULL, &status);
                fits_read_key(fptr, TDOUBLE, "CD1_2", &cd2, NULL, &status);
                if (status == 0) {
                    cd1 = cd1/zoom;
                    cd2 = cd2/zoom;
                    fits_update_key(fptr, TDOUBLE, "CD1_1", &cd1, NULL, &status);
                    fits_update_key(fptr, TDOUBLE, "CD1_2", &cd2, NULL, &status);
                } else if (status == VALUE_UNDEFINED || status == KEY_NO_EXIST) {
                    /* no CRPIX, so look for CDELT1 */
                    status = 0;
                    fits_read_key(fptr, TDOUBLE, "CDELT1", &cd1, NULL, &status);
                    if (status == 0) {
                        cd1 = cd1/zoom;
                        if (fits_update_key(fptr, TDOUBLE, "CDELT1", &cd1, NULL, &status))
                            printerror (status);
                    } else if (status == VALUE_UNDEFINED || status == KEY_NO_EXIST) {
                        status = 0;
                        fitscut_message  (2,
                            "\tNo CD1 or CDELT1 found, header scale not updated\n");
                    }
                }
            }
            fits_update_key(fptr, TDOUBLE, "CRPIX1", &crpix,
                "Reference pixel shifted for cutout", &status);
            fitscut_message  (2, "\tUpdated CRPIX1 to %f\n", crpix);
            /*
             * add RADESYS keyword if it is missing (for PS1 images)
             * only do this if both RADESYS and EQUINOX are missing and the
             * image has the old PC001001 keyword
             */
            if (status == 0) {
                if ((! header_has_key(fptr, "RADESYS", &status))
                  & (! header_has_key(fptr, "EQUINOX", &status))
                  & header_has_key(fptr, "PC001001", &status) ) {
                    /* keyword not found, add it */
                    if (fits_write_key(fptr, TSTRING, "RADESYS", "FK5", "added by fitscut", &status))
                        printerror (status);
                    fitscut_message  (2, "\tAdded RADESYS keyword\n");
                    /* also add the TIMESYS keyword for PS1 images (which are the source of the RADESYS problem) */
                    if (! header_has_key(fptr, "TIMESYS", &status)) {
                        /* keyword not found, add it */
                        if (fits_write_key(fptr, TSTRING, "TIMESYS", "TAI", "added by fitscut", &status))
                            printerror (status);
                        fitscut_message  (2, "\tAdded TIMESYS keyword\n");
                    }
                }
            }
        } else if (status == VALUE_UNDEFINED || status == KEY_NO_EXIST) {
            /* no CRPIX, so skip updating WCS */
            fitscut_message  (2, "\tNo CRPIX1 found, header not updated\n");
            status = 0;
        }
        /* catch all errors in above section */
        if (status) printerror (status);

        fits_read_key(fptr, TDOUBLE, "CRPIX2", &crpix, NULL, &status);
        if (status == 0) {
            crpix = crpix - Image->y0[0];
            if (zoom != 0 && zoom != 1) {
                crpix = zoom*crpix - 0.5*(zoom-1);
                fits_read_key(fptr, TDOUBLE, "CD2_1", &cd1, NULL, &status);
                fits_read_key(fptr, TDOUBLE, "CD2_2", &cd2, NULL, &status);
                if (status == 0) {
                    cd1 = cd1/zoom;
                    cd2 = cd2/zoom;
                    fits_update_key(fptr, TDOUBLE, "CD2_1", &cd1, NULL, &status);
                    fits_update_key(fptr, TDOUBLE, "CD2_2", &cd2, NULL, &status);
                } else if (status == VALUE_UNDEFINED || status == KEY_NO_EXIST) {
                    /* no CRPIX, so look for CDELT2 */
                    status = 0;
                    fits_read_key(fptr, TDOUBLE, "CDELT2", &cd1, NULL, &status);
                    if (status == 0) {
                        cd1 = cd1/zoom;
                        if (fits_update_key(fptr, TDOUBLE, "CDELT2", &cd1, NULL, &status))
                            printerror (status);
                    } else if (status == VALUE_UNDEFINED || status == KEY_NO_EXIST) {
                        status = 0;
                        fitscut_message  (2,
                            "\tNo CD2 or CDELT2 found, header scale not updated\n");
                    }
                }
            }
            fits_update_key(fptr, TDOUBLE, "CRPIX2", &crpix,
                "Reference pixel shifted for cutout", &status);
            fitscut_message  (2, "\tUpdated CRPIX2 to %f\n", crpix);
        } else if (status == VALUE_UNDEFINED || status == KEY_NO_EXIST) {
            /* no CRPIX, so skip updating WCS */
            fitscut_message  (2, "\tNo CRPIX2 found, header not updated\n");
            status = 0;
        }

        /*
         * clean the extension header too
         */
        fitscut_clean_header (fptr, Image, &status);

        if (Image->channels > 1) {
            /* add CTYPE3 = "RGB" for color images just after CTYPE2 */
            fits_read_key(fptr, TSTRING, "CTYPE2", retval, NULL, &status);
            if (status) {
                /* append key at end if CTYPE2 was not found */
                status = 0;
                fits_update_key(fptr, TSTRING, "CTYPE3", "RGB", "RGB color image", &status);
            } else {
                fits_insert_key_str(fptr, "CTYPE3", "RGB", "RGB color image", &status);
            }
            if (status) printerror (status);
        }

        fitscut_message(2, "\tWriting pixel data\n");
        fitscut_write_data (fptr, Image->data,
                               Image->nrowsref, Image->ncolsref, Image->channels);


        fitscut_message  (1, "\tClosing file...\n");
        fitscut_close_fits (fptr);
}
