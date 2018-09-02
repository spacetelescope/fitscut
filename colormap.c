/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * Colormap functions
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
 * $Id: colormap.c,v 1.5 2004/04/21 20:13:09 mccannwj Exp $
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

#include "fitscut.h"
#include "colormap.h"
#include "util.h"

#ifdef DMALLOC
#include <dmalloc.h>
#define DMALLOC_FUNC_CHECK 1
#endif

static void
load_color (char *str, long nresult, float **list)
{
        float *x, *y;
        long npoints;

        npoints = parse_coordinates (str, &x, &y);
        *list = interpolate_points (nresult, x, y, npoints);
}

static void
heat_color_map_def (char **red, char **green, char **blue)
{
        char *redStr = "(0,0)(.34,1)(1,1)";
        char *greenStr = "(0,0)(1,1)";
        char *blueStr = "(0,0)(.65,0)(.98,1)(1,1)";
        *red = redStr;
        *green = greenStr;
        *blue = blueStr;
}

static void
cool_color_map_def (char **red, char **green, char **blue)
{
        char *redStr = "(0,0)(.29,0)(.76,.1)(1,1)";
        char *greenStr = "(0,0)(.22,0)(.96,1)(1,1)";
        char *blueStr = "(0,0)(.53,1)(1,1)";
        *red = redStr;
        *green = greenStr;
        *blue = blueStr;
}

static void
rainbow_color_map_def (char **red, char **green, char **blue)
{
        char *redStr = "(0,1)(.2,0)(.6,0)(.8,1)(1,1)";
        char *greenStr = "(0,0)(.2,0)(.4,1)(.8,1)(1,0)";
        char *blueStr = "(0,1)(.4,1)(.6,0)(1,0)";  
        *red=redStr;
        *green=greenStr;
        *blue=blueStr;
}

static void
red_color_map_def (char **red, char **green, char **blue)
{
        char *redStr = "(0,0)(1,1)";
        char *greenStr = "(0,0)(0,0)";
        char *blueStr = "(0,0)(0,0)";
        *red = redStr;
        *green = greenStr;
        *blue = blueStr;
}

static void
green_color_map_def (char **red, char **green, char **blue)
{
        char *redStr = "(0,0)(0,0)";
        char *greenStr = "(0,0)(1,1)";
        char *blueStr = "(0,0)(0,0)";
        *red = redStr;
        *green = greenStr;
        *blue = blueStr;
}

static void
blue_color_map_def (char **red, char **green, char **blue)
{
        char *redStr = "(0,0)(0,0)";
        char *greenStr = "(0,0)(0,0)";
        char *blueStr = "(0,0)(1,1)";
        *red = redStr;
        *green = greenStr;
        *blue = blueStr;
}

static ColorMap *
get_colormap (int type, long nvals)
{
        float *red, *green, *blue;
        char *redStr, *greenStr, *blueStr;
        ColorMap *CM;

        CM = (ColorMap *) malloc (sizeof (ColorMap));
        switch (type) {
        case CMAP_COOL:
                cool_color_map_def (&redStr, &greenStr, &blueStr);
                break;
        case CMAP_RAINBOW:
                rainbow_color_map_def (&redStr, &greenStr, &blueStr);
                break;
        case CMAP_RED:
                red_color_map_def (&redStr, &greenStr, &blueStr);
                break;
        case CMAP_GREEN:
                green_color_map_def (&redStr, &greenStr, &blueStr);
                break;
        case CMAP_BLUE:
                blue_color_map_def (&redStr, &greenStr, &blueStr);
                break;
        case CMAP_HEAT:
        default:
                heat_color_map_def (&redStr, &greenStr, &blueStr);
                break;
        }

        load_color (redStr, nvals, &red);
        load_color (greenStr, nvals, &green);
        load_color (blueStr, nvals, &blue);
        CM->red = red;
        CM->green = green;
        CM->blue = blue;

        /* always put white at the top */
        CM->red[nvals-1] = 1;
        CM->blue[nvals-1] = 1;
        CM->green[nvals-1] = 1;

        return (CM);
}

png_color *
get_png_palette (int type, long nvals)
{
        int i;
        ColorMap *CM;
        png_color *palette;
        long max_val;
        float f;

        palette = (png_color *) malloc (sizeof (png_color) * nvals);
  
        CM = get_colormap (type, nvals);

        /* scale the map channels */
        max_val = nvals - 1;
        for (i = 0; i < nvals; i++) {
                f = CM->red[i] * max_val;
                palette[i].red = (unsigned char) f;
                f = CM->green[i] * max_val;
                palette[i].green = (unsigned char) f;
                f = CM->blue[i] * max_val;
                palette[i].blue = (unsigned char) f;
        }

        return palette;
}
