/* -*- mode:C; indent-tabs-mode:nil; tab-width:8; c-basic-offset:8; -*-
 *
 * Fitscut drawing functions
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
 * $Id: draw.c,v 1.6 2004/04/21 21:28:47 mccannwj Exp $
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

#ifdef	STDC_HEADERS
#include <stdlib.h>
#else	/* Not STDC_HEADERS */
extern void exit ();
extern char *malloc ();
#endif	/* STDC_HEADERS */

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "fitscut.h"
#include "image_scale.h"
#include "draw.h"

#ifndef DEGTORAD
#define DEGTORAD(x) ((x)*M_PI/180.0)
#endif 

static void
rotate_vector (float *invec, float angle, float *outvec)
{
        float cosa, sina;
        float temp;

        cosa = cos (DEGTORAD (angle));
        sina = sin (DEGTORAD (angle));
 
        /* use a temp variable in case invec and outvec are the same */
        temp = invec[0] * cosa + invec[1] * sina;
        outvec[1] = -invec[0] * sina + invec[1] * cosa;
        outvec[0] = temp;
}

static void
ImageSetPixel (FitsCutImage *Image, int channel, int x, int y, float value)
{
        float *arrayp;
        int ncols = Image->ncols[channel];

        arrayp = Image->data[channel];

        arrayp[ncols * y + x] = value;
}

static int
drawCompareInt (const void *a, const void *b)
{
        return (*(const int *)a) - (*(const int *)b);
}

void
draw_filled_polygon (FitsCutImage *Image, int channel, drawPointPtr p, int n, int c)
{
        /* adapted from gdImageFilledPolygon */
        int i;
        int y;
        int miny, maxy;
        int x1, y1;
        int x2, y2;
        int ind1, ind2;
        int ints;
        int *polyInts;

        if (!n)
                return;

        polyInts = (int *) malloc (sizeof (int) * n);

        miny = p[0].y;
        maxy = p[0].y;
        for (i = 1; i < n; i++) {
                if (p[i].y < miny)
                        miny = p[i].y;
                if (p[i].y > maxy)
                        maxy = p[i].y;
        }
        /* Fix in 1.3: count a vertex only once */
        for (y = miny; y <= maxy; y++) {
                /*1.4		int interLast = 0; */
                /*		int dirLast = 0; */
                /*		int interFirst = 1; */
                ints = 0;
                for (i = 0; (i < n); i++) {
                        if (!i) {
                                ind1 = n - 1;
                                ind2 = 0;
                        } else {
                                ind1 = i - 1;
                                ind2 = i;
                        }
                        y1 = p[ind1].y;
                        y2 = p[ind2].y;
                        if (y1 < y2) {
                                x1 = p[ind1].x;
                                x2 = p[ind2].x;
                        } else if (y1 > y2) {
                                y2 = p[ind1].y;
                                y1 = p[ind2].y;
                                x2 = p[ind1].x;
                                x1 = p[ind2].x;
                        } else {
                                continue;
                        }
                        if ((y >= y1) && (y < y2)) {
                                polyInts[ints++] = (y-y1) * (x2-x1) / (y2-y1) + x1;
                        } else if ((y == maxy) && (y > y1) && (y <= y2)) {
                                polyInts[ints++] = (y-y1) * (x2-x1) / (y2-y1) + x1;
                        }
                }
                qsort (polyInts, ints, sizeof (int), drawCompareInt);
    
                for (i = 0; (i < (ints)); i += 2) {
                        draw_line (Image, channel, polyInts[i], y,
                                  polyInts[i+1], y, c);
                }
        }
}

void
draw_line (FitsCutImage *Image, int channel, int x1, int y1, int x2, int y2, int value)
{
        /* adapted from GD gdImageLine */
        int dx, dy, incr1, incr2, d, x, y, xend, yend, xdirflag, ydirflag;
        dx = abs (x2-x1);
        dy = abs (y2-y1);
        if (dy <= dx) {
                d = 2 * dy - dx;
                incr1 = 2 * dy;
                incr2 = 2 * (dy - dx);
                if (x1 > x2) {
                        x = x2;
                        y = y2;
                        ydirflag = (-1);
                        xend = x1;
                } else {
                        x = x1;
                        y = y1;
                        ydirflag = 1;
                        xend = x2;
                }
                ImageSetPixel (Image, channel, x, y, value);
                if (((y2 - y1) * ydirflag) > 0) {
                        while (x < xend) {
                                x++;
                                if (d <0) {
                                        d += incr1;
                                } else {
                                        y++;
                                        d+=incr2;
                                }
                                ImageSetPixel (Image, channel, x, y, value);
                        }
                } else {
                        while (x < xend) {
                                x++;
                                if (d <0) {
                                        d+=incr1;
                                } else {
                                        y--;
                                        d+=incr2;
                                }
                                ImageSetPixel (Image, channel, x, y, value);
                        }
                }		
        } else {
                d = 2 * dx - dy;
                incr1 = 2 * dx;
                incr2 = 2 * (dx - dy);
                if (y1 > y2) {
                        y = y2;
                        x = x2;
                        yend = y1;
                        xdirflag = (-1);
                } else {
                        y = y1;
                        x = x1;
                        yend = y2;
                        xdirflag = 1;
                }
                ImageSetPixel (Image, channel, x, y, value);
                if (((x2 - x1) * xdirflag) > 0) {
                        while (y < yend) {
                                y++;
                                if (d <0) {
                                        d+=incr1;
                                } else {
                                        x++;
                                        d+=incr2;
                                }
                                ImageSetPixel (Image, channel, x, y, value);
                        }
                } 
                else {
                        while (y < yend) {
                                y++;
                                if (d <0) {
                                        d+=incr1;
                                } else {
                                        x--;
                                        d+=incr2;
                                }
                                ImageSetPixel (Image, channel, x, y, value);
                        }
                }
        }
}

static void
draw_arrow (FitsCutImage *Image,
            int channel,
            int x,
            int y,
            float angle, 
            int length,
            int width,
            int tip_length,
            int value)
{
        float tip_angle;
        float vec[2], outvec[2];
        int num_tip_points;
        int x0, y0, x1, y1, tipx, tipy;
        drawPointPtr arrow;
        drawPointPtr shaft;

        tip_angle = 20.0;
  
        /* rotate from horizontal */
        vec[0] = length;
        vec[1] = 0;
        rotate_vector (vec,angle,outvec);
        tipx = floor (x + outvec[0] + 0.5);
        tipy = floor (y + outvec[1] + 0.5);

        /* find arrow tips */
        vec[0] = -tip_length;
        vec[1] = 0;
        rotate_vector (vec, angle + tip_angle, outvec);
        x0 = floor (tipx + outvec[0] + 0.5);
        y0 = floor (tipy + outvec[1] + 0.5);
        rotate_vector (vec, angle - tip_angle, outvec);
        x1 = floor (tipx + outvec[0] + 0.5);
        y1 = floor (tipy + outvec[1] + 0.5);
        if (tip_length > 10) {
                num_tip_points = (tip_length > 10) ? 4 : 3;

                arrow = (drawPoint *) malloc (sizeof (drawPoint) * num_tip_points);

                arrow[0].x = tipx;
                arrow[0].y = tipy;
                arrow[1].x = x0;
                arrow[1].y = y0;

                arrow[num_tip_points-1].x = x1;
                arrow[num_tip_points-1].y = y1;

                if (num_tip_points > 3) {
                        /* find the new tip of the arrow shaft */
                        vec[0] = length - tip_length * 0.8;
                        vec[1] = 0;
                        rotate_vector (vec,angle,outvec);
                        tipx = floor (x + outvec[0] + 0.5);
                        tipy = floor (y + outvec[1] + 0.5);
                        arrow[2].x = tipx;
                        arrow[2].y = tipy;
                }

                /* draw the arrow head */
                draw_filled_polygon (Image, channel, arrow, num_tip_points, value);
                free (arrow);
        }
        else {
                draw_line (Image, channel, x0, y0, tipx, tipy, value);
                draw_line (Image, channel, x1, y1, tipx, tipy, value);
        }

        /* draw arrow shaft */
        if (width > 2) {
                shaft = (drawPoint *) malloc (sizeof (drawPoint) * 4);
                vec[0] = 0;
                vec[1] = width / 2;
                rotate_vector (vec, angle, outvec);
                shaft[0].x = x + outvec[0];
                shaft[0].y = y + outvec[1];
                shaft[1].x = tipx + outvec[0];
                shaft[1].y = tipy + outvec[1];
                vec[0] = 0;
                vec[1] = -width / 2;
                rotate_vector (vec, angle, outvec);
                shaft[3].x = x + outvec[0];
                shaft[3].y = y + outvec[1];
                shaft[2].x = tipx + outvec[0];
                shaft[2].y = tipy + outvec[1];
                draw_filled_polygon (Image, channel, shaft, 4, value);
                free (shaft);
        }
        else {
                draw_line (Image, channel, x, y, tipx, tipy, value);
        }
  
}

void
draw_wcs_compass (FitsCutImage *Image, float north_pa, float east_pa)
{
        float arrow_length;
        long center[2];
        int channel = 0;
        int draw_channels;
        float east_size_factor = 0.6;
        float line_value = 255;
        float arrow_size_factor;
        int thickness;
        int tip_size;
        int min_size;
        center[0] = Image->ncols[0] * 0.8;
        center[1] = Image->nrows[0] * 0.8;

        min_size = (Image->ncols[0] > Image->nrows[0]) ? Image->nrows[0] : Image->ncols[0];

        if (min_size > 1024)
                arrow_size_factor = 0.05;
        else if (min_size > 512)
                arrow_size_factor = 0.10;
        else if (min_size > 128)
                arrow_size_factor = 0.20;
        else
                arrow_size_factor = 0.30;

        arrow_length = arrow_size_factor * min_size;

        thickness = 2; /*floor(arrow_length * 0.05 + 0.5);*/
        if (thickness < 1)
                thickness = 1;

        fitscut_message (1, "drawing WCS compass north: %f east: %f thickness: %d\n",
                         north_pa, east_pa, thickness);

        tip_size = floor (arrow_length * 0.2 + 0.5);

        draw_channels = (Image->channels > 1) ? 2 : 1;
        for (channel = 0; channel < draw_channels; channel++) {
                /* draw north line */
                draw_arrow (Image, channel, center[0], center[1], north_pa,
                            arrow_length, thickness, tip_size, line_value);
    
                /* draw east line */
                draw_arrow (Image, channel, center[0], center[1], east_pa,
                            arrow_length * east_size_factor, thickness, tip_size, line_value);
        }
}
