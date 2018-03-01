/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * JSON output functions
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
#include "output_json.h"
#include "revision.h"

/*
 * Write extracted image pixel values in JSON format
 *
 * The format is a simple nested array of values:
 * [pixels0, pixels1, pixels2]
 * where pixels0,pixels1,pixels2 are the (up to) 3 color planes and
 * pixels is a 2-D pixel array, e.g.:
 * [ [ p00, p01, p02 ], [ p10, p11, p12 ] ]
 *
 * Note this format could be verbose if you write a lot of pixels.
 * It is intended for small image sections.
 */

int write_to_json(FitsCutImage *Image)
{
	FILE *outfile;
	long i, j, k, ncols, nrows;
	float *data;

	if (to_stdout) {
		outfile = stdout;
	} else {
		outfile = fopen(Image->output_filename, "w");
	}
	if (!outfile) return ERROR;

	fprintf(outfile, "[");
	for (k=0; k<Image->channels; k++) {
		data = Image->data[k];
		nrows = Image->nrows[k];
		ncols = Image->ncols[k];
		if (data) {
			if (k != 0) fprintf(outfile, ",");
			fprintf(outfile, "\n [");
			for (i=0; i<nrows; i++) {
				if (i != 0) fprintf(outfile, ",");
				fprintf(outfile, "\n  [");
				for (j=0; j<ncols; j++) {
					if (j != 0) fprintf(outfile, ", ");
					/* replace NaN values with zeros (NaN is not allowed in JSON) */
					if (data[j+ncols*i] == data[j+ncols*i]) {
						fprintf(outfile, "%e", data[j+ncols*i]);
					} else {
						fprintf(outfile, "0.0");
					}
				}
				fprintf(outfile, "]");
			}
			fprintf(outfile, "\n ]");
		}
	}
	fprintf(outfile, "\n]\n");
	fflush(stdout);
	return OK;
}
