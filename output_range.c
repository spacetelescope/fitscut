/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * JSON range output functions
 *
 * Author: Rick White
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
#include "output_range.h"
#include "revision.h"

/*
 * Write image pixel value ranges in JSON format
 *
 * The format is a dictionary with a list of values:
 *
 * {'min': [0.0, 0.0, 0.0], 'max': [0.0, 0.0, 0.0]}
 *
 * The (up to) 3 values are for the red, green, blue bands.
 */

int write_range(FitsCutImage *Image)
{
	FILE *outfile;
	long k;
	double *datamin, *datamax;

	if (to_stdout) {
		outfile = stdout;
	} else {
		outfile = fopen(Image->output_filename, "w");
	}
	if (!outfile) return ERROR;

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

	fprintf(outfile, "{ 'min': [");
	for (k=0; k<Image->channels; k++) {
		if (Image->data[k] != NULL) {
			if (k > 0) fprintf(outfile, ", ");
			fprintf(outfile, "%.17e", datamin[k]);
		}
	}
	fprintf(outfile, "], 'max': [");
	for (k=0; k<Image->channels; k++) {
		if (Image->data[k] != NULL) {
			if (k > 0) fprintf(outfile, ", ");
			fprintf(outfile, "%.17e", datamax[k]);
		}
	}
	fprintf(outfile, "]}\n");
	fflush(stdout);
	return OK;
}
