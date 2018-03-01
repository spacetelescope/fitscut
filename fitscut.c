/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * Extract a cutout from a FITS format file
 * Copyright (C) 2001 William Jon McCann
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
 * requires CFITSIO library
 * (http://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html)
 *
 * $Id: fitscut.c,v 1.44 2004/07/27 22:09:44 mccannwj Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* AIX requires this to be the first thing in the file.  */
#if defined (_AIX) && !defined (__GNUC__)
 #pragma alloca
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include "getopt.h"
#include <signal.h>
#include <ctype.h>

#ifndef errno
extern int errno;
#endif

#ifdef	STDC_HEADERS
#include <stdlib.h>
#else	/* Not STDC_HEADERS */
extern void exit ();
extern char *malloc ();
#endif	/* STDC_HEADERS */

#ifdef	HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef NO_TIME_H
#  include <sys/time.h>
#else
#  include <time.h>
#endif

#ifndef NO_FCNTL_H
#  include <fcntl.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#ifdef	__GNUC__
#undef	alloca
#define	alloca(n)	__builtin_alloca (n)
#else	/* Not GCC.  */
#ifdef	HAVE_ALLOCA_H
#include <alloca.h>
#else	/* Not HAVE_ALLOCA_H.  */
#ifndef	_AIX
extern char *alloca ();
#endif	/* Not AIX.  */
#endif	/* HAVE_ALLOCA_H.  */
#endif	/* GCC.  */

#include <netinet/in.h>
#include <inttypes.h>
#include <float.h>
#include <math.h>
#include <libgen.h>

#ifdef HAVE_CFITSIO_FITSIO_H
#include <cfitsio/fitsio.h> 
#else
#include <fitsio.h>
#endif

#ifdef HAVE_LIBPNG
#include <png.h>        /* includes zlib.h and setjmp.h */
#endif

#include <libwcs/wcs.h>

#include "tailor.h"
#include "fitscut.h"
#include "revision.h"

#include "wcs_align.h"

#include "draw.h"
#include "extract.h"
#include "file_check.h"
#include "image_scale.h"
#include "output_graphic.h"
#include "output_fits.h"
#include "output_json.h"
#include "output_range.h"

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif


struct option longopts[] = {
    { "verbose", 0, 0, 'v' },
    { "help", 0, 0, 'h' },
    { "version", 0, 0, 'V' },
    { "force", 0, 0, 'f' },
    { "png", 0, 0, 'p' },
    { "jpg", 0, 0, 'j' },
    { "jquality", required_argument, 0, 21 },
    { "x", required_argument, 0, 'x' },
    { "y", required_argument, 0, 'y' },
    { "wcs", 0, 0, 18 },
    { "rows", required_argument, 0, 'r' },
    { "columns", required_argument, 0, 'c' },
    { "autoscale", required_argument, 0, 'a' },
    { "autoscale-min", required_argument, 0, 16 },
    { "autoscale-max", required_argument, 0, 17 },
    { "full-scale", 0, 0, 19 },
    { "log-scale", 0, 0, 'l' },
    { "sqrt-scale", 0, 0, 's' },
    { "histeq-scale", 0, 0, 'e' },
    { "asinh-scale", 0, 0, 14 },
    { "linear-scale", 0, 0, 15 },
    { "palette", required_argument, 0, 't' },
    { "min",required_argument, 0, 1 },
    { "max",required_argument, 0, 2 },
    { "x0", required_argument, 0, 3 },
    { "y0", required_argument, 0, 4 },
    { "align", 0, 0, 5 },
    { "compass", 0, 0, 6 },
    { "rate", 0, 0, 7 },
    { "factor", required_argument, 0, 8 },
    { "red", required_argument, 0, 9 },
    { "green", required_argument, 0, 10 },
    { "blue", required_argument, 0, 11 },
    { "all", 0, 0, 12 },
    { "zoom", required_argument , 0, 13 },
    { "output-size", required_argument , 0, 20 },
    { "invert", 0, 0, 'i' },
    { "add_blurb", required_argument , 0, 22},
    { "qext", required_argument, 0, 23 },
    { "badpix", 0, 0, 24 },
    { "json", 0, 0, 25 },
    { "marker", 0, 0, 26 },
    { "range", 0, 0, 27 },
    { "badvalue", required_argument, 0, 28 },
    { "nobsoften", 0, 0, 29 },
    { "reference", required_argument, 0, 30 },
    { 0, 0, 0, 0 }
};

extern char version[];

char usage[] = "Usage: %s [options] file [green] [blue] [outfile]\n";

char *progname;

int foreground = 1;   /* set if program run in foreground */
int force = 0;        /* don't ask questions, overwrite (-f) */
int to_stdout = 1;    /* output to stdout (-c) */
int verbose = 0;      /* be verbose (-v) */
int quiet = 0;        /* be very quiet (-q) */
int exit_code = OK;   /* program exit code */


void
fitscut_message (int level, const char *format, ...)
{
        va_list args;
        va_start (args, format);

        if (verbose >= level)
                vfprintf (stderr, format, args);

        va_end (args);
}

/*
 * Free all dynamically allocated variables and exit with the given code.
 */
void
do_exit (int exitcode)
{
        static int in_exit = 0;

        if (in_exit)
                exit (exitcode);
        in_exit = 1;

        /* free memory etc. */

        exit (exitcode);
}

RETSIGTYPE
abort_fitscut ()
{
        /* close, remove files */

        do_exit (ERROR);
}

void
fitscut_error (char *m)
{
        fprintf (stderr, "\n%s: %s\n", progname, m);
        abort_fitscut ();
}

static void
show_version ()
{
        fprintf (stderr, "%s %s (%s)\n", progname, VERSION, REVDATE);
}

static void
show_supported_palettes ()
{
        fputs ("\nSupported palettes:\n", stderr);
        fputs ("\tgray heat cool rainbow red green blue\n", stderr);
}


static void
show_usage ()
{
        fprintf (stderr, usage, progname);
        fputs ("Options are:\n", stderr);
        fputs ("  -f, --force\t\tforce overwrite of output file\n", stderr);
        fputs ("  -h, --help\t\tdisplay this help and exit\n", stderr);
        fputs ("  -v, --verbose\t\tverbose operation\n", stderr);
        fputs ("  -V, --version\t\toutput version information and exit\n\n", stderr);

        fputs ("  -p, --png\t\toutput a PNG format file\n", stderr);
        fputs ("  -j, --jpg\t\toutput a JPEG format file\n", stderr);
        fputs ("      --jquality\tset JPEG quality parameter (default 75)\n", stderr);
        fputs ("      --json\t\toutput a JSON (ascii) format file\n", stderr);
        fputs ("      --range\t\toutput min/max data ranges (in JSON format)\n", stderr);
        fputs ("  -x, --x=N\t\tX center coordinate\n", stderr);
        fputs ("  -y, --y=N\t\tY center coordinate\n", stderr);
        fputs ("      --x0=N\t\tX corner coordinate\n", stderr);
        fputs ("      --y0=N\t\tY corner coordinate\n", stderr);
        fputs ("  -r, --rows=N\t\tnumber of rows (height)\n", stderr);
        fputs ("  -c, --columns=N\tnumber of columns (width)\n", stderr);
        fputs ("      --all\t\textract a cutout of the same size as the input image\n\n", stderr);

        fputs ("      --linear-scale\toutput in linear scale [default]\n", stderr);
        fputs ("  -l, --log-scale\toutput in log scale\n", stderr);
        fputs ("  -s, --sqrt-scale\toutput in square root scale\n", stderr);
        fputs ("  -e, --histeq-scale\thistogram equalized output\n", stderr);
        fputs ("      --asinh-scale\tasinh equalized output\n", stderr);
        fputs ("  -i, --invert\t\tinvert output colors\n", stderr);
        fputs ("      --palette=name\toutput palette name (png output only)\n", stderr);
        fputs ("      --factor=value\tmultiply image by value\n", stderr);
        fputs ("      --rate\t\tscale image by exposure time factor\n\n", stderr);

        fputs ("      --autoscale=percent\tpercent of histogram to include\n\n", stderr);
        fputs ("\t\t\tIf percent is a single value it is interpreted as\n", stderr);
        fputs ("\t\t\tthe percentage of the histogram to include.\n", stderr);
        fputs ("\t\t\tIf percent is two values separated by a comma it\n", stderr);
        fputs ("\t\t\tis interpreted as the lower and upper bounds\n", stderr);
        fputs ("\t\t\tof the histogram to include (eg. --autoscale=1.5,98.5)\n\n", stderr);

        fputs ("      --autoscale-min=percent\tlower bound percentage of histogram to include\n", stderr);
        fputs ("      --autoscale-max=percent\tupper bound percentage of histogram to include\n", stderr);
        fputs ("      --full-scale\tuse samples from entire image for autoscale\n", stderr);
        fputs ("      --min=value\timage value to use for scale minimum\n", stderr);
        fputs ("      --max=value\timage value to use for scale maximum\n\n", stderr);
        fputs ("\t\t\tThe value for --min or --max may be either one value to\n", stderr);
        fputs ("\t\t\tbe used for all images (eg. --min=0), or one value for\n", stderr);
        fputs ("\t\t\teach image separated by a comma (eg. --min=0,0,0)\n\n", stderr);

        fputs ("      --red=file\tfilename to be used in red channel of color image\n", stderr);
        fputs ("      --green=file\tfilename to be used in green channel of color image\n", stderr);
        fputs ("      --blue=file\tfilename to be used in blue channel of color image\n", stderr);
        fputs ("      --qext=number\textension for data quality array\n", stderr);
        fputs ("\t\t\tqext may be an integer or a comma-separated list of integers\n\n", stderr);
        fputs ("      --badpix\t\tignore pixels as specified in header values BADPIX/GOODMIN/GOODMAX\n", stderr);
        fputs ("      --badvalue=value\tignore pixels with this value (default 0.0)\n", stderr);
        fputs ("      --nobsoften\tdo NOT apply inverse asinh scaling using BSOFTEN/BOFFSET keywords (default=apply)\n", stderr);

        fputs ("      --zoom=factor\tzoom input image by positive multiplicative factor\n\n", stderr);
        fputs ("      --output-size=value\tforce image output size to given value\n\n", stderr);
        fputs ("      --add_blurb=\tfilename containing text to be added as HISTORY cards to the output header\n", stderr);

        fputs ("      --wcs\t\tconvert input X,Y from degrees to pixels using WCS information in header\n", stderr);
        fputs ("      --align\t\tuse WCS information in header to align images\n", stderr);
        fputs ("      --reference=file\tfilename to use for WCS reference when aligning images\n", stderr);
        fputs ("\t\t\tMay be red, green or blue to select one of the input files (default=red)\n\n", stderr);
        fputs ("      --compass\t\tadd a WCS compass to the image\n", stderr);
        fputs ("      --marker\t\tadd a crosshair marker around the image center\n", stderr);
  
        show_supported_palettes ();
}

static void
scale_image (FitsCutImage *Image)
{
    /* don't scale if output format is FITS or JSON */
    if (Image->output_type != OUTPUT_FITS && Image->output_type != OUTPUT_JSON) {

        if (Image->output_scale_mode != SCALE_MODE_USER)
            scan_min_max (Image);

        if (Image->output_type != OUTPUT_RANGE) {
            /* pre-scale pixel values unless simple range output is requested */
            switch (Image->output_scale) {
            case SCALE_HISTEQ:
                    histeq_image (Image);
                    break;
            case SCALE_SQRT:
                    sqrt_image (Image);
                    break;
            case SCALE_LOG:
                    log_image (Image);
                    break;
            case SCALE_FACTOR:
                    mult_image (Image);
                    break;
            case SCALE_RATE:
                    rate_image (Image);
                    break;
            case SCALE_ASINH:
                    asinh_image (Image);
                    break;
            case SCALE_LINEAR:
            default:
                    break;
            }
        }

        switch (Image->output_scale_mode) {
        case SCALE_MODE_AUTO:
        case SCALE_MODE_FULL:
                autoscale_image (Image);
                break;
        default:
                break;
        }
    }
}

static void
align_image (FitsCutImage *Image)
{
        switch (Image->output_alignment) {
        case ALIGN_REF:
                fitscut_message (1, "\tAligning to reference channel...\n");
                wcs_align_ref (Image);
                fitscut_message (1, "done.\n");
                break;
        case ALIGN_NONE:
        default:
                break;
        }
}

static void
render_compass (FitsCutImage *Image)
{
        float north_pa, east_pa;
        struct WorldCoor *wcs;

        wcs = Image->wcsref;
        if (wcs == NULL)
                return;

        if (nowcs (wcs))
                return;

        north_pa = -wcs->pa_north;
        east_pa = -wcs->pa_east;
        fitscut_message (1, "found WCS north: %f east: %f\n", north_pa, east_pa);
        draw_wcs_compass (Image, north_pa, east_pa);
}

static void
write_image (FitsCutImage *Image)
{
        int retval;

        switch (Image->output_type) {
        case OUTPUT_FITS:
                fitscut_message (1, "Creating FITS file...\n");
                write_to_fits (Image);
                break;
        case OUTPUT_PNG:
                fitscut_message (1, "Creating PNG file...\n");
                retval = write_to_png (Image);
                if (retval) {
                        fitscut_message (0, "error creating PNG file.\n");
                        do_exit (2);
                }
                break;    
        case OUTPUT_JPG:
                fitscut_message (1, "Creating JPG file...\n");
                retval = write_to_jpg (Image);
                if (retval) {
                        fitscut_message (0, "error creating JPG file.\n");
                        do_exit (2);
                }
                break;    
        case OUTPUT_JSON:
                fitscut_message (1, "Creating JSON file...\n");
                retval = write_to_json (Image);
                if (retval) {
                        fitscut_message (0, "error creating JSON file.\n");
                        do_exit (2);
                }
                break;    
        case OUTPUT_RANGE:
                fitscut_message (1, "Creating JSON pixel range file...\n");
                retval = write_range (Image);
                if (retval) {
                        fitscut_message (0, "error creating JSON pixel range file.\n");
                        do_exit (2);
                }
                break;    
        default:
                break;
        }
}

static void
release_data (FitsCutImage *Image)
{
        int k;

        for (k = 0; k < Image->channels; k++) {
            if (Image->data[k] != NULL) {
                free (Image->data[k]);
                if (Image->header[k] != NULL)
                        free (Image->header[k]);
            }
        }
}

static void
treat_input (FitsCutImage *Image)
{

        extract_fits (Image);
        align_image (Image);
        scale_image (Image);
        if (Image->output_type != OUTPUT_RANGE) {
            if (Image->output_compass)
                    render_compass (Image);
            if (Image->output_marker)
                    draw_center_marker (Image);
        }
        write_image (Image);
        release_data (Image);
}

static int
check_input (FitsCutImage *Image)
{
        int retval;
        int k;
        int fcount = 0;
        int findex[3];

        for (k = 0; k < Image->channels; k++) {
                if (Image->input_filename[k] != NULL) {
                        findex[fcount] = k;
                        fcount += 1;
                        fitscut_message (1, "Checking input file: %s\n",
                                         Image->input_filename[k]);
                        if ((retval = check_input_file (Image->input_filename[k])) != OK)
                                return ERROR;
                }
                if (Image->ncols[k] < 0) {
                        fitscut_message (0, "%s: cutout width must be > 0 for channel %d\n",
                                         progname, k);
                        return ERROR;
                }
                if (Image->nrows[k] < 0) {
                        fitscut_message (0, "%s: cutout height must be > 0 for channel %d\n",
                                         progname, k);
                        return ERROR;
                }
        }

        if (Image->reference_filename == NULL) {
            Image->reference_filename = "red";
        }
        if (strequ(Image->reference_filename, "red")) {
            Image->reference_filename = Image->input_filename[findex[0]];
        } else if (strequ(Image->reference_filename, "green")) {
            /* treat green as 2nd (if at least 2 files) */
            if (fcount > 1) {
                Image->reference_filename = Image->input_filename[findex[1]];
            } else {
                Image->reference_filename = Image->input_filename[findex[0]];
            }
        } else if (strequ(Image->reference_filename, "blue")) {
            /* treat blue as last */
            Image->reference_filename = Image->input_filename[findex[fcount-1]];
        } else {
            fitscut_message (1, "Checking reference file: %s\n",
                             Image->reference_filename);
            if ((retval = check_input_file (Image->reference_filename)) != OK)
                    return ERROR;
        }

        return OK;
}

static int
check_output (char *ofname, char *ifname)
{
        if (check_output_file (ofname, ifname) != OK)
                return ERROR;

        return OK;
}



static void
fitscut_initialize (FitsCutImage *Image)
{
        int k;

        Image->output_type = OUTPUT_FITS;
        Image->output_scale = SCALE_LINEAR;
        Image->output_scale_mode = SCALE_MODE_MINMAX;
        Image->output_colormap = CMAP_GRAY;
        Image->output_compass = 0;
        Image->output_marker = 0;
        Image->output_size = 0;
        Image->output_invert = 0;
        Image->output_add_blurb = 0;
        Image->jpeg_quality = 75;
        Image->useBadpix = 0;
        Image->useBsoften = 1;
        Image->channels = 0;
        Image->user_min_set = FALSE;
        Image->user_max_set = FALSE;
        Image->user_scale_factor_set = FALSE;
        Image->qext_set = FALSE;
        Image->input_filename[0] = Image->input_filename[1] = Image->input_filename[2] = NULL;
        Image->input_blurbfile = NULL;
        Image->autoscale_performed = FALSE;

        for (k = 0; k < MAX_CHANNELS; k++) {
                Image->output_zoom[k] = 0;
                Image->user_min[k] = 0;
                Image->user_max[k] = 0;
                Image->autoscale_percent_low[k] = 0.0;
                Image->autoscale_percent_high[k] = 100.0;
                Image->user_scale_factor[k] = 1.0;
                Image->qext[k] = -1;
                Image->qext_bad_value[k] = 0;
                Image->bad_data_value[k] = 0.0;
                Image->badmin[k] = 0.0;
                Image->badmax[k] = 0.0;
                Image->header[k] = NULL;
                Image->input_wcscoords[k] = 0;
                Image->input_x_corner[k] = 0;
                Image->input_y_corner[k] = 0;
                Image->wcs[k] = NULL;
                Image->data[k] = NULL;
                Image->input_x[k] = Image->input_y[k] = Image->ncols[k] = Image->nrows[k] = -1;
        }
        Image->nrowsref = Image->ncolsref = -1;
        Image->output_alignment = ALIGN_NONE;
        Image->wcsref = NULL;
        Image->reference_filename = "red";
}

int 
main (int argc, char *argv[])
{
        int optc;
        int h = 0;
        int V = 0;
        int lose = 0;
        int proglen;        /* length of progname */
        char ofname[MAX_PATH_LEN];
        int arg_count, k;
        FitsCutImage Image;
        char *cmap_name = NULL;
        char *tmpstr = NULL;
        char *sptr = NULL;
        int user_min_count = 1;
        int user_max_count = 1;
        int autoscale_min_count = 1;
        int autoscale_max_count = 1;
        int i;
        float bad_data_value;

#ifdef DEBUGGING
        mtrace ();
#endif

        fitscut_initialize (&Image);

        /* set up globals */
  
        EXPAND (argc, argv); /* wild card expansion if necessary */

        progname = (char *) basename (argv[0]);
        proglen = strlen (progname);
  
        /* Suppress .exe for MSDOS, OS/2 and VMS: */
        if (proglen > 4 && strequ (progname + proglen - 4, ".exe"))
                progname[proglen-4] = '\0';

        foreground = signal (SIGINT, SIG_IGN) != SIG_IGN;
        if (foreground)
                (void) signal (SIGINT, (sig_type)abort_fitscut);

        while ((optc = getopt_long (argc, argv, "hviVfpjlsex:y:r:c:a:t:",
                                    longopts, (int *) 0)) != EOF)
                {
                        switch (optc)
                                {
                                case 'v':
                                        verbose++;
                                        break;
                                case 'V':
                                        V = 1;
                                        break;
                                case 'h':
                                        h = 1;
                                        break;
                                case 'f':
                                        force = 1;
                                        break;
                                case 'i':
                                        Image.output_invert = 1;
                                        break;
                                case 'l':
                                        Image.output_scale = SCALE_LOG;
                                        break;
                                case 'e':
                                        Image.output_scale = SCALE_HISTEQ;
                                        break;
                                case 's':
                                        Image.output_scale = SCALE_SQRT;
                                        break;
                                case 'p':
                                        Image.output_type = OUTPUT_PNG;
                                        break;
                                case 'j':
                                        Image.output_type = OUTPUT_JPG;
                                        break;
                                case 25:
                                        Image.output_type = OUTPUT_JSON;
                                        break;
                                case 27:
                                        Image.output_type = OUTPUT_RANGE;
                                        break;
                                case 21:
                                        Image.jpeg_quality = strtod (optarg, (char **)NULL);
                                        break;
                                case 24:
                                        Image.useBadpix = 1;
                                        break;
                                case 28: /* badvalue */
                                        /* using strtod because strtof is not always declared */
                                        bad_data_value = (float) strtod (optarg, (char **)NULL);
                                        for (k = 0; k < MAX_CHANNELS; k++) {
                                                Image.bad_data_value[k] = bad_data_value;
                                                Image.badmin[k] = bad_data_value;
                                                Image.badmax[k] = bad_data_value;
                                        }
                                        break;
                                case 29:
                                        Image.useBsoften = 0;
                                        break;
                                case 'x':
                                        Image.input_x[0] = strtod (optarg, (char **)NULL);
                                        for (i = 1; i < MAX_CHANNELS; i++)
                                                Image.input_x[i] = Image.input_x[0];
                                        for (i = 0; i < MAX_CHANNELS; i++)
                                            Image.input_x_corner[i] = 0;
                                        break;
                                case 'y':
                                        Image.input_y[0] = strtod (optarg, (char **)NULL);
                                        for (i = 1; i < MAX_CHANNELS; i++)
                                                Image.input_y[i] = Image.input_y[0];
                                        for (i = 0; i < MAX_CHANNELS; i++)
                                            Image.input_y_corner[i] = 0;
                                        break;
                                case 'r':
                                        Image.nrowsref = strtol (optarg, (char **)NULL, 0);
                                        for (i = 0; i < MAX_CHANNELS; i++)
                                                Image.nrows[i] = Image.nrowsref;
                                        break;
                                case 'c':
                                        Image.ncolsref = strtol (optarg, (char **)NULL, 0);
                                        for (i = 0; i < MAX_CHANNELS; i++)
                                                Image.ncols[i] = Image.ncolsref;
                                        break;
                                case 't': /* palette */
                                        cmap_name = strdup (optarg);
                                        break;
                                case 'a': /* autoscale */
                                        if (strchr (optarg,',') != NULL) {
                                                /* we have a value pair */
                                                tmpstr = strdup (optarg);
                                                sptr = strtok (tmpstr, ",");
                                                if (sptr != NULL) {
                                                        Image.autoscale_percent_low[0] = strtod (sptr, (char **)NULL);
                                                        sptr = strtok (NULL, ",");
                                                        if (sptr != NULL)
                                                                Image.autoscale_percent_high[0] = strtod (sptr, (char **)NULL);
                                                }
                                                free (tmpstr);
                                        }
                                        else {
                                                Image.autoscale_percent_high[0] = strtod (optarg, (char **)NULL);
                                                Image.autoscale_percent_low[0] = 100.0 - Image.autoscale_percent_high[0];
                                        }

                                        if ((Image.autoscale_percent_high[0] < 100) &&
                                            (Image.autoscale_percent_high[0] > 0)) {
                                                if (Image.output_scale_mode != SCALE_MODE_FULL)
                                                    Image.output_scale_mode = SCALE_MODE_AUTO;
                                        }
                                        break;
                                case 19:
                                        Image.output_scale_mode = SCALE_MODE_FULL;
                                        break;
                                case 1: /* min */
                                        if (strchr (optarg, ',') != NULL) {
                                                /* we have a value for each channel */
                                                tmpstr = strdup (optarg);
                                                sptr = strtok (tmpstr, ",");
                                                if (sptr != NULL) {
                                                        Image.user_min[0] = strtod (sptr, (char **)NULL);
                                                        sptr = strtok (NULL, ",");
                                                        if (sptr != NULL) {
                                                                Image.user_min[1] = strtod (sptr, (char **)NULL);
                                                                user_min_count = 2;
                                                                sptr = strtok (NULL, ",");
                                                                if (sptr != NULL) {
                                                                        Image.user_min[2] = strtod (sptr, (char **)NULL);
                                                                        user_min_count = 3;
                                                                }
                                                        }
                                                }
                                                free (tmpstr);
                                        }
                                        else {
                                                Image.user_min[0] = strtod (optarg, (char **)NULL);
                                        }
                                        Image.user_min_set = TRUE;
                                        break;
                                case 2: /* max */
                                        /* we have a value for each channel */
                                        if (strchr (optarg, ',') != NULL) {
                                                tmpstr = strdup (optarg);
                                                sptr = strtok (tmpstr, ",");
                                                if (sptr != NULL) {
                                                        Image.user_max[0] = strtod (sptr, (char **)NULL);
                                                        sptr = strtok (NULL, ",");
                                                        if (sptr != NULL) {
                                                                Image.user_max[1] = strtod (sptr, (char **)NULL);
                                                                user_max_count = 2;
                                                                sptr = strtok (NULL, ",");
                                                                if (sptr != NULL) {
                                                                        Image.user_max[2] = strtod (sptr, (char **)NULL);
                                                                        user_max_count = 3;
                                                                }
                                                        }
                                                }
                                                free (tmpstr);
                                        }
                                        else {
                                                Image.user_max[0] = strtod (optarg, (char **)NULL);
                                        }
                                        Image.user_max_set = TRUE;
                                        break;
                                case 3: /* x0 */
                                        Image.input_x[0] = strtod (optarg, (char **)NULL);
                                        for (i = 1; i < MAX_CHANNELS; i++)
                                            Image.input_x[i] = Image.input_x[0];
                                        for (i = 0; i < MAX_CHANNELS; i++)
                                            Image.input_x_corner[i] = 1;
                                        break;
                                case 4: /* y0 */
                                        Image.input_y[0] = strtod (optarg, (char **)NULL);
                                        for (i = 1; i < MAX_CHANNELS; i++)
                                            Image.input_y[i] = Image.input_y[0];
                                        for (i = 0; i < MAX_CHANNELS; i++)
                                            Image.input_y_corner[i] = 1;
                                        break;
                                case 5: /* align */
                                        Image.output_alignment = ALIGN_REF;
                                        break;
                                case 6: /* compass */
                                        Image.output_compass = 1;
                                        break;
                                case 7: /* rate */
                                        Image.output_scale = SCALE_RATE;
                                        break;
                                case 8: /* factor */
                                        if (strchr (optarg,',') != NULL) {
                                                /* we have a value for each channel */
                                                tmpstr = strdup (optarg);
                                                sptr = strtok(tmpstr, ",");
                                                if (sptr != NULL) {
                                                        Image.user_scale_factor[0] = strtod (sptr, (char **)NULL);
                                                        sptr = strtok (NULL, ",");
                                                        if ( sptr != NULL) {
                                                                Image.user_scale_factor[1] = strtod (sptr, (char **)NULL);
                                                                sptr = strtok (NULL, ",");
                                                                if ( sptr != NULL) {
                                                                        Image.user_scale_factor[2] = strtod (sptr, (char **)NULL);
                                                                }
                                                        }
                                                }
                                                free (tmpstr);
                                        }
                                        else {
                                                Image.user_scale_factor[0] = strtod (optarg, (char **)NULL);
                                        }
                                        Image.user_scale_factor_set = TRUE;
                                        Image.output_scale = SCALE_FACTOR;
                                        break;
                                case 9:  /* red */
                                        Image.input_filename[0] = strdup (optarg);
                                        Image.channels = 3;
                                        break;
                                case 10: /* green */
                                        Image.input_filename[1] = strdup (optarg);
                                        Image.channels = 3;
                                        break;
                                case 11: /* blue */
                                        Image.input_filename[2] = strdup (optarg);
                                        Image.channels = 3;
                                        break;
                                case 30:  /* reference */
                                        Image.reference_filename = strdup (optarg);
                                        break;
                                case 23: /* quality/weight extension */
                                        if (strchr(optarg,',') != NULL) {
                                            /* we have a value for each channel */
                                            tmpstr = strdup(optarg);
                                            sptr = strtok(tmpstr, ",");
                                            if (sptr != NULL) {
                                                Image.qext[0] = strtol(sptr, (char **)NULL, 0);
                                                sptr = strtok(NULL, ",");
                                                if (sptr != NULL) {
                                                    Image.qext[1] = strtol(sptr, (char **)NULL, 0);
                                                    sptr = strtok(NULL, ",");
                                                    if (sptr != NULL) {
                                                        Image.qext[2] = strtol(sptr, (char **)NULL, 0);
                                                    } else {
                                                        Image.qext[2] = Image.qext[1];
                                                    }
                                                } else {
                                                    Image.qext[1] = Image.qext[0];
                                                    Image.qext[2] = Image.qext[0];
                                                }
                                            }
                                            free(tmpstr);
                                        } else {
                                            Image.qext[0] = strtol(optarg, (char **)NULL, 0);
                                            Image.qext[1] = Image.qext[0];
                                            Image.qext[2] = Image.qext[0];
                                        }
                                        Image.qext_set = TRUE;
                                        break;

                                case 12: /* all */
                                        for (i = 0; i < MAX_CHANNELS; i++) {
                                                Image.input_x[i] = 0;
                                                Image.input_y[i] = 0;
                                                Image.nrows[i] = MAGIC_SIZE_ALL_NUMBER;
                                                Image.ncols[i] = MAGIC_SIZE_ALL_NUMBER;
                                        }
                                        Image.nrowsref = MAGIC_SIZE_ALL_NUMBER;
                                        Image.ncolsref = MAGIC_SIZE_ALL_NUMBER;
                                        break;
                                case 13: /* zoom */
                                        Image.output_zoom[0] = strtod (optarg, (char **)NULL);
                                        /* HACK - FIXME */
                                        if (Image.output_zoom[0] > 1) {
                                                Image.output_zoom[0] = floor (Image.output_zoom[0]);
                                        }
                                        Image.output_zoom[1] = Image.output_zoom[0];
                                        Image.output_zoom[2] = Image.output_zoom[0];
                                        break;
                                case 20: /* output-size  */
                                        Image.output_size = strtol (optarg, (char **)NULL, 0);
                                        break;
                                case 14:
                                        Image.output_scale = SCALE_ASINH;
                                        break;
                                case 15:
                                        Image.output_scale = SCALE_LINEAR;
                                        break;
                                case 16: /* autoscale min */
                                        if (strchr (optarg, ',') != NULL) {
                                                /* we have a value for each channel */
                                                tmpstr = strdup (optarg);
                                                sptr = strtok (tmpstr, ",");
                                                if (sptr != NULL) {
                                                        Image.autoscale_percent_low[0] = strtod (sptr, (char **)NULL);
                                                        sptr = strtok (NULL, ",");
                                                        if (sptr != NULL) {
                                                                Image.autoscale_percent_low[1] = strtod (sptr, (char **)NULL);
                                                                autoscale_min_count = 2;
                                                                sptr = strtok (NULL, ",");
                                                                if (sptr != NULL) {
                                                                        Image.autoscale_percent_low[2] = strtod (sptr, (char **)NULL);
                                                                        autoscale_min_count = 3;
                                                                }
                                                        }
                                                }
                                                free (tmpstr);
                                        }
                                        else {
                                                Image.autoscale_percent_low[0] = strtod (optarg, (char **)NULL);
                                        }

                                        if (Image.output_scale_mode != SCALE_MODE_FULL)
                                            Image.output_scale_mode = SCALE_MODE_AUTO;
                                        break;
                                case 17: /* autoscale max */
                                        if (strchr (optarg, ',') != NULL) {
                                                /* we have a value for each channel */
                                                tmpstr = strdup (optarg);
                                                sptr = strtok (tmpstr, ",");
                                                if (sptr != NULL) {
                                                        Image.autoscale_percent_high[0] = strtod (sptr, (char **)NULL);
                                                        sptr = strtok (NULL, ",");
                                                        if (sptr != NULL) {
                                                                Image.autoscale_percent_high[1] = strtod (sptr, (char **)NULL);
                                                                autoscale_max_count = 2;
                                                                sptr = strtok (NULL, ",");
                                                                if (sptr != NULL) {
                                                                        Image.autoscale_percent_high[2] = strtod (sptr, (char **)NULL);
                                                                        autoscale_max_count = 3;
                                                                }
                                                        }
                                                }
                                                free (tmpstr);
                                        }
                                        else {
                                                Image.autoscale_percent_high[0] = strtod (optarg, (char **)NULL);
                                        }

                                        if (Image.output_scale_mode != SCALE_MODE_FULL)
                                            Image.output_scale_mode = SCALE_MODE_AUTO;
                                        break;
                                case 18: /* WCS input coordinates */
                                        for (i = 0; i < MAX_CHANNELS; i++) {
                                            Image.input_wcscoords[i] = 1;
                                        }
                                        break;
                                case 22: /* add HISTORY blurb to header */
                                        Image.input_blurbfile = strdup (optarg);
                                        Image.output_add_blurb = 1;
                                        break;

                                case 26: /* marker */
                                        Image.output_marker = 1;
                                        break;

                                default:
                                        lose = 1;
                                        break;
                                }
                }

        if (V) {
                /* Print version number.  */
                show_version ();
                if (! h)
                        exit (0);
        }

        if (h) {
                /* Print help info and exit.  */
                show_usage ();
                exit (0);
        }

        if (cmap_name != NULL) {
                /* translate name into a code */
                if (! strcasecmp (cmap_name, "heat"))
                        Image.output_colormap = CMAP_HEAT;
                else if (!strcasecmp (cmap_name, "cool"))
                        Image.output_colormap = CMAP_COOL;
                else if (!strcasecmp (cmap_name, "rainbow"))
                        Image.output_colormap = CMAP_RAINBOW;
                else if (!strcasecmp (cmap_name, "gray"))
                        Image.output_colormap = CMAP_GRAY;
                else if (!strcasecmp (cmap_name, "red"))
                        Image.output_colormap = CMAP_RED;
                else if (!strcasecmp (cmap_name, "green"))
                        Image.output_colormap = CMAP_GREEN;
                else if (!strcasecmp (cmap_name, "blue"))
                        Image.output_colormap = CMAP_BLUE;
                else {
                        fprintf (stderr, "Warning: palette %s unknown, using grayscale.\n", cmap_name);
                        Image.output_colormap = CMAP_GRAY;
                }
        }
        else {
                Image.output_colormap = CMAP_GRAY;
        }

        /* if the user didn't specify enough min/max values */
        for (k = user_min_count; k < MAX_CHANNELS; k++)
                Image.user_min[k] = Image.user_min[k-1];

        for (k = user_max_count; k < MAX_CHANNELS; k++)
                Image.user_max[k] = Image.user_max[k-1];

        for (k = autoscale_min_count; k < MAX_CHANNELS; k++)
                Image.autoscale_percent_low[k] = Image.autoscale_percent_low[k-1];

        for (k = autoscale_max_count; k < MAX_CHANNELS; k++)
                Image.autoscale_percent_high[k] = Image.autoscale_percent_high[k-1];

        if (Image.user_min_set && Image.user_max_set)
            Image.output_scale_mode = SCALE_MODE_USER;

        if (Image.output_add_blurb) {
            if (check_input_file(Image.input_blurbfile) != OK) {
                do_exit(1);
            }
        }

        arg_count = argc - optind;
        if ( (Image.input_filename[0] != NULL) || 
             (Image.input_filename[1] != NULL) || 
             (Image.input_filename[2] != NULL)) {
                if (check_input (&Image) != OK)
                        do_exit (1);
                if (arg_count >= 1) {
                        to_stdout = 0;
                        make_output_name (ofname, argv[optind++]);
                        check_output (ofname, Image.input_filename[0]);
                }
                else {
                        strcpy (ofname, "-");
                }
        }
        else {
                if (arg_count >= 1) {
                        Image.input_filename[0] = strdup (argv[optind++]);
                        if (arg_count == 1) {
                                Image.channels = 1;
                                if (check_input (&Image) == ERROR)
                                        do_exit (1);
                                strcpy (ofname, "-");
                        }
                        else if (arg_count == 2) {
                                to_stdout = 0;
                                Image.channels = 1;
                                if (check_input (&Image) == ERROR)
                                        do_exit (1);
                                make_output_name (ofname, argv[optind++]);
                                check_output (ofname,Image.input_filename[0]);
                        }
                        else if (arg_count == 3) {
                                Image.input_filename[1] = strdup (argv[optind++]);
                                Image.input_filename[2] = strdup (argv[optind++]);
                                Image.channels = 3;
                                if (check_input (&Image) == ERROR)
                                        do_exit (1);
                                strcpy (ofname,"-");
                        }
                        else if (arg_count == 4) {
                                Image.input_filename[1] = strdup (argv[optind++]);
                                Image.input_filename[2] = strdup (argv[optind++]);
                                to_stdout = 0;
                                Image.channels = 3;
                                if (check_input (&Image) == ERROR)
                                        do_exit(1);
                                make_output_name (ofname, argv[optind++]);
                                check_output (ofname, Image.input_filename[0]);
                                check_output (ofname, Image.input_filename[1]);
                                check_output (ofname, Image.input_filename[2]);
                        }
                        else {
                                show_usage ();
                                exit (0);
                        }

                } else {
                        show_usage ();
                        exit (1);
                }
        }

        if (to_stdout) {
                SET_BINARY_MODE (fileno (stdout));
        }
        Image.output_filename = strdup (ofname);
        treat_input (&Image);

        return OK;
}

