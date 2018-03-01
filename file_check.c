/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * File and filename check functions
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
 * $Id: file_check.c,v 1.4 2004/07/27 22:09:44 mccannwj Exp $
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

#ifdef  HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
 
#include "fitscut.h"
#include "tailor.h"
#include "file_check.h"

#include "util.h"

extern char *progname;
extern int foreground;

char fits_suffix[MAX_SUFFIX+1]; /* default suffix */
int  suffix_len;           /* strlen(fits_suffix) */
struct stat istat;         /* status for input file */

/*
 * Use lstat if available, except for -c or -f. Use stat otherwise.
 * This allows links when not removing the original file.
 */
static int
do_stat (char *name, struct stat *sbuf)
{
        errno = 0;
#if (defined(S_IFLNK) || defined (S_ISLNK)) && !defined(NO_SYMLINK)
        if (!to_stdout && !force) {
                return lstat (name, sbuf);
        }
#endif
        return stat (name, sbuf);
}

static int
get_istat (char *iname, struct stat *sbuf)
{
        /* If input file exists, return OK. */
        if (do_stat (iname, sbuf) == 0)
                return OK;

        if (errno != ENOENT) {
                perror (iname);
                exit_code = ERROR;
                return ERROR;
        }

        perror (iname);
        exit_code = ERROR;
        return ERROR;
}

/*
 * Generate ofname. Return OK, or WARNING if file must be skipped.
 * Sets save_orig_name to true if the file name has been truncated.
 */
int
make_output_name (char *ofname, char *oname)
{
        sprintf (ofname, "%s", oname);
        return OK;
}

/*
 * Return true if the two stat structures correspond to the same file.
 */
static int
same_file (struct stat *stat1, struct stat *stat2)
{
        return stat1->st_ino   == stat2->st_ino
                && stat1->st_dev   == stat2->st_dev
#ifdef NO_ST_INO
                /* Can't rely on st_ino and st_dev, use other fields: */
                && stat1->st_mode  == stat2->st_mode
                && stat1->st_uid   == stat2->st_uid
                && stat1->st_gid   == stat2->st_gid
                && stat1->st_size  == stat2->st_size
                && stat1->st_atime == stat2->st_atime
                && stat1->st_mtime == stat2->st_mtime
                && stat1->st_ctime == stat2->st_ctime
#endif
                ;
}

/*
 * Return true if a file name is ambiguous because the operating system
 * truncates file names.
 */
static int
name_too_long (char *name, struct stat *statb)
{
        int s = strlen (name);
        char c = name[s-1];
        struct stat tstat;   /* stat for truncated name */
        int res;
  
        tstat = *statb;      /* Just in case OS does not fill all fields */
        name[s-1] = '\0';
        res = stat (name, &tstat) == 0 && same_file (statb, &tstat);
        name[s-1] = c;

        return res;
}

static void
shorten_name (char *name)
{
        fprintf (stderr, "%s: error: output file name too long: %s\n",
                 progname, name);
        do_exit (ERROR);
}

/*
 * Check if ofname is not ambiguous
 * because the operating system truncates names. Otherwise, generate
 * a new ofname and save the original name in the compressed file.
 * If the compressed file already exists, ask for confirmation.
 *    The check for name truncation is made dynamically, because different
 * file systems on the same OS might use different truncation rules (on SVR4
 * s5 truncates to 14 chars and ufs does not truncate).
 *    This function returns -1 if the file must be skipped, and
 * updates save_orig_name if necessary.
 * IN assertions: save_orig_name is already set if ofname has been
 * already truncated because of NO_MULTIPLE_DOTS. The input file has
 * already been open and istat is set.
 */
int
check_output_file (char *ofname, char *ifname)
{
        struct stat ostat; /* stat for ofname */

#ifdef ENAMETOOLONG
        /* Check for strictly conforming Posix systems (which return ENAMETOOLONG
         * instead of silently truncating filenames).
         */
        errno = 0;
        while (stat (ofname, &ostat) != 0) {
                if (errno != ENAMETOOLONG) return 0; /* ofname does not exist */
                shorten_name (ofname);
        }
#else
        if (stat (ofname, &ostat) != 0) {
                return 0;
        }
#endif

        /* Check for name truncation on existing file. Do this even on systems
         * defining ENAMETOOLONG, because on most systems the strict Posix
         * behavior is disabled by default (silent name truncation allowed).
         */
        if (name_too_long (ofname, &ostat)) {
                shorten_name (ofname);
                if (stat (ofname, &ostat) != 0)
                        return 0;
        }
        /* Check that the input and output files are different (could be
         * the same by name truncation or links).
         */
        if (same_file (&istat, &ostat)) {
                if (strequ (ifname, ofname)) {
                        fprintf (stderr, "%s: %s: cannot convert onto itself\n",
                                 progname, ifname );
                } else {
                        fprintf (stderr, "%s: %s and %s are the same file\n",
                                 progname, ifname, ofname);
                }
                exit_code = ERROR;
                return ERROR;
        }
        /* Ask permission to overwrite the existing file */
        if (!force) {
                char response[80];
                strcpy (response,"n");
                fprintf (stderr, "%s: %s already exists;", progname, ofname);
                if (foreground && isatty (fileno (stdin))) {
                        fprintf (stderr, " do you wish to overwrite (y or n)? ");
                        fflush (stderr);
                        (void)fgets (response, sizeof (response)-1, stdin);
                }
                if (tolow (*response) != 'y') {
                        fprintf (stderr, "\tnot overwritten\n");
                        if (exit_code == OK)
                                exit_code = WARNING;
                        return ERROR;
                }
        }
        (void) chmod (ofname, 0777);
        if (unlink (ofname)) {
                fprintf (stderr, "%s: ", progname);
                perror (ofname);
                exit_code = ERROR;
                return ERROR;
        }
        return OK;
}

int
check_input_file (char *iname)
{
char tmpname[1024];
int i, ltmpname;

        if (strlen(iname) >= sizeof(tmpname)) {
                WARN ((stderr, "%s: filename is >= 1024 characters long\n", progname));
        }
        (void) strncpy(tmpname, iname, sizeof(tmpname) - 1);
        tmpname[sizeof(tmpname) - 1] = '\0';

        /* Omit any extension that was specified */
        ltmpname = strlen(tmpname);
        for (i=0; i<ltmpname; i++) {
            if (tmpname[i] == '[') {
                tmpname[i] = '\0';
                break;
            }
        }

        /* Check if the input file is present, set globals ifname and istat: */
        if (get_istat (tmpname, &istat) != OK) {
                return ERROR;
        }

        /* If the input name is that of a directory ignore: */
        if (S_ISDIR (istat.st_mode)) {
                WARN ((stderr, "%s: %s is a directory -- ignored\n", progname, tmpname));
                return WARNING;
        }
        if (!S_ISREG (istat.st_mode) && !S_ISLNK (istat.st_mode)) {
                WARN ((stderr,
                       "%s: %s is not a directory, symbolic link, or regular file - ignored\n",
                      progname, tmpname));
                return WARNING;
        }
        if (istat.st_nlink > 1 && !to_stdout && !force) {
                WARN ((stderr, "%s: %s has %d other link%c\n",
                       progname, tmpname,
                       (int)istat.st_nlink - 1, istat.st_nlink > 2 ? 's' : ' '));
                return WARNING;
        }

        return OK;
}
