/* declarations for extract.c
 * Copyright (C) 2002 William Jon McCann
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
 * $Id: extract.h,v 1.6 2004/04/29 22:22:30 mccannwj Exp $
 */

void   extract_fits (FitsCutImage *);
void   printerror (int);
double fits_get_exposure_time (char *, int);
void fits_get_badpix (char *, int, float *, float *, float *);
float *cutout_alloc (unsigned int nx, unsigned int ny, float value);
int ext_exists(fitsfile *fptr, int qext, int *status);

extern int get_qual_info (fitsfile **dqptr, long *nplanes, float *badmin, float *badmax, float *bad_data_value,
	fitsfile *fptr, char *header, int num_cards,
	int qext_set, int qext, int useBadpix, int *status);

extern int apply_qual (fitsfile *dqptr, long nplanes, float badmin, float badmax, float bad_data_value,
	long fpixel[7], long lpixel[7], long inc[7],
	float *arrayptr, int anynull, int badvalue,
	int *status);

extern int fitscut_read_subset(fitsfile *fptr, int datatype, long *fpixel, long *lpixel, long *inc,
	void *nulval, void *array, int *anynul, int *status);

extern void invert_bsoften (double bsoften, double boffset, long fpixel[7], long lpixel[7], long inc[7],
	float *arrayptr, float bad_data_value);

extern void fits_get_bsoften (FitsCutImage *Image, int k, int *useBsoften, double *bsoften, double *boffset);
