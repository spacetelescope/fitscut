/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * fitscut.h -- declarations for fitscut
 * Copyright (C) 1999 William Jon McCann
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
 * $Id: fitscut.h,v 1.26 2004/04/29 22:22:30 mccannwj Exp $
 */

#include <stdarg.h>

#if defined(__STDC__) || defined(PROTO)
#  define OF(args)  args
#else
#  define OF(args)  ()
#endif

#ifndef RETSIGTYPE
#  define RETSIGTYPE void
#endif

#define strequ(s1, s2) (strcmp((s1),(s2)) == 0)

typedef RETSIGTYPE (*sig_type) OF((int));

#ifndef SET_BINARY_MODE
#  define SET_BINARY_MODE(fd)
#endif

#ifndef O_BINARY
#  define O_BINARY 0 /* creation mode for open() */
#endif

#if !defined(S_ISDIR) && defined(S_IFDIR)
#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG) && defined(S_IFREG)
#  define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

#ifndef MAX_PATH_LEN
#  define MAX_PATH_LEN   1024 /* max pathname length */
#endif

/* Separator for file name parts (see shorten_name()) */
#ifdef NO_MULTIPLE_DOTS
#  define PART_SEP "-"
#else
#  define PART_SEP "."
#endif

#define BYTE    unsigned char
#define WORD    unsigned short int
#define LWORD   unsigned long
#define SPACE   32
#define DEL     127
#define ICSIZE  100

#ifndef TRUE
#define TRUE (1==1)
#endif 	/* TRUE */

#ifndef FALSE
#define FALSE !(TRUE)
#endif	/* FALSE */

/* Return codes */
#define OK      0
#define ERROR   1
#define WARNING 2

#define tolow(c)  (isupper(c) ? (c)-'A'+'a' : (c)) /* force to lower case */

#define WARN(msg) {if (!quiet) fprintf msg ; \
		   if (exit_code == OK) exit_code = WARNING;}

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X,Y) ((X) < (Y) ? (Y) : (X))

typedef enum {
        OUTPUT_FITS = 0,
        OUTPUT_PNG,
        OUTPUT_JPG
} FitscutOutputFormat;

typedef enum {
        SCALE_LINEAR = 0,
        SCALE_LOG,
        SCALE_SQRT,
        SCALE_HISTEQ,
        SCALE_FACTOR,
        SCALE_RATE,
        SCALE_ASINH
} FitscutScaleType;

typedef enum {
        SCALE_MODE_MINMAX = 0,
        SCALE_MODE_AUTO,
        SCALE_MODE_USER
} FitscutScaleMode;

typedef enum {
        CMAP_GRAY = 0,
        CMAP_HEAT,
        CMAP_COOL,
        CMAP_RAINBOW,
        CMAP_RED,
        CMAP_GREEN,
        CMAP_BLUE
} FitscutColormapType;

typedef enum {
        ALIGN_NONE = 0,
        ALIGN_FIRST,
        ALIGN_NORTH,
        ALIGN_USER_WCS
} FitscutAlignType;

RETSIGTYPE abort_fitscut   (void);
void       do_exit         (int);
void       fitscut_error   (char *);
void       fitscut_message (int level, const char *format, ...);

#define NBINS 10000

extern int foreground;            /* set if program run in foreground */
extern int force;        /* don't ask questions, overwrite (-f) */
extern int to_stdout;    /* output to stdout (-c) */
extern int verbose;      /* be verbose (-v) */
extern int quiet;        /* be very quiet (-q) */
extern int exit_code;   /* program exit code */

#define MAX_CHANNELS 3

#define MAGIC_SIZE_ALL_NUMBER 999999

typedef struct fitscut_image {
        int output_type;
        int output_scale;
        int output_colormap;
        int output_scale_mode;
        int output_alignment;
        int output_compass;
        int output_invert;
        float output_zoom;
        char *input_filename[MAX_CHANNELS];
        int input_datatype[MAX_CHANNELS];
        char *output_filename;
        int channels;
        long x0[MAX_CHANNELS], y0[MAX_CHANNELS], xc[MAX_CHANNELS], yc[MAX_CHANNELS];
        long ncols[MAX_CHANNELS], nrows[MAX_CHANNELS];
        double user_min[MAX_CHANNELS];
        int user_min_set;
        double user_max[MAX_CHANNELS];
        int user_max_set;
        double user_scale_factor[MAX_CHANNELS];
        int user_scale_factor_set;
        double autoscale_percent_low[MAX_CHANNELS];
        double autoscale_percent_high[MAX_CHANNELS];
        double autoscale_min[MAX_CHANNELS], autoscale_max[MAX_CHANNELS];
        int autoscale_performed;
        double data_min[MAX_CHANNELS], data_max[MAX_CHANNELS];
        float *histogram[MAX_CHANNELS];
        float *data[MAX_CHANNELS];
        char *header[MAX_CHANNELS];
        int header_cards[MAX_CHANNELS];
#ifdef HAVE_LIBWCS
        struct WorldCoor *wcs[MAX_CHANNELS];
#endif
} FitsCutImage;
