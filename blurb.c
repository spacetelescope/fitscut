/* 
 *
 * Blurb header update functions
 *
 * 
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
                             
#ifdef	HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif


#include "blurb.h"

void
blurb_error (char *m)
{
        fprintf (stderr, "\n%s\n", m);
}


char
*blurb_read (char *fname)
{
    int len = 0;
    int buf_size = BLURB_BUFFER_INIT;
    char card[73];
    char *cards = NULL;
    FILE *fp = NULL;

    if ((cards = (char *)malloc (buf_size)) == NULL) {
        blurb_error ("memory allocation failed for blurb cards.");
        return NULL;
    }

    if ((fp = fopen (fname, "r")) == NULL) {
        blurb_error ("unable to open blurb file:");
        blurb_error (fname);
        free (cards);
        return NULL;
    }

    cards[0] = '\0';
    while (fgets (card, 73, fp) != NULL) {
        if ((len += strlen (card)) >= buf_size) {
            if ((cards = (char *)realloc (cards, buf_size + BLURB_BUFFER_INIT)) == NULL) {
                blurb_error ("memory allocation failed for blurb cards.");
                fclose (fp);
                return NULL;
            } else {
                buf_size += BLURB_BUFFER_INIT;
            }
        }
        strcat (cards , card);
    }

    if (ferror (fp)) {
        blurb_error ("trouble reading blurb file:");
        blurb_error (fname);
        fclose (fp);
        free (cards);
        return NULL;
    } else {
        fclose (fp);
    }

    return cards;
}
