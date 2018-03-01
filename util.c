/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * Utility functions
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
 * $Id: util.c,v 1.4 2004/04/21 20:13:10 mccannwj Exp $
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

#include <float.h>
#include <math.h>

#ifdef  STDC_HEADERS
#include <stdlib.h>
#else   /* Not STDC_HEADERS */
extern void exit ();
extern char *malloc ();
#endif  /* STDC_HEADERS */
                                                                                
#ifdef	HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "fitscut.h"
#include "util.h"

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

float *
interpolate_points (long nresult, float *px, float *py, long npoints)
{
        long i, j;
        long last_index, next_index;
        float *list;
        float m, x, y;

        /* x and y are normalized to unity */
        list = (float *) malloc (sizeof (float) * nresult);
        last_index = 0;
        for (i = 0; i < npoints-1; i++) {
                next_index = ceil (nresult * px[i+1]);
                m = (py[i + 1] - py[i]) / (px[i + 1] - px[i]);
                for (j = last_index; j < next_index; j++) {
                        /* point slope formula */
                        x = j / (float) nresult;
                        y = m * (x - px[i]) + py[i];
                        list[j] = y;
                }
                last_index = next_index;
        }
        return list;
}

long
parse_coordinates (char *str, float **x, float **y)
{
        int count,i;
        char *sptr;
        char *tmpstr;
        float tx,ty;
        float *xp;
        float *yp;

        count = 0;
        for (i = 0; i < strlen (str); i++) {
                if (str[i] == ',') 
                        count++;
        } 

        xp = (float *)malloc (sizeof (float) * count);
        yp = (float *)malloc (sizeof (float) * count);
        tmpstr = strdup(str);
        for (i = 0; i < count; i++) {
                if (i == 0)
                        sptr = strtok (tmpstr, ")");
                else
                        sptr = strtok (NULL, ")");
                sscanf (sptr, "(%f,%f", &tx, &ty);
                xp[i] = tx;
                yp[i] = ty;
        }

        *x = xp;
        *y = yp;
        free (tmpstr);
        return (count);
}

#if 0

void
byte_swap_vector (void *p, int n, int size)
{
        char *a, *b, c;
  
        switch (size) {
        case 2:
                for (a = (char*)p ; n > 0; n--, a += 1) {
                        b = a + 1;
                        c = *a; *a++ = *b; *b   = c;
                }
                break;
        case 4:
                for (a = (char*)p ; n > 0; n--, a += 2) {
                        b = a + 3;
                        c = *a; *a++ = *b; *b-- = c;
                        c = *a; *a++ = *b; *b   = c;
                }
                break;
        case 8:
                for (a = (char*)p ; n > 0; n--, a += 4) {
                        b = a + 7;
                        c = *a; *a++ = *b; *b-- = c;
                        c = *a; *a++ = *b; *b-- = c;
                        c = *a; *a++ = *b; *b-- = c;
                        c = *a; *a++ = *b; *b   = c;
                }
                break;
        default:
                break;
        }
}
#endif

/*
 * Put string s in lower case, return s.
 */
char *
strlwr (char *s)
{
        char *t;
        for (t = s; *t; t++)
                *t = tolow(*t);
        return s;
}
