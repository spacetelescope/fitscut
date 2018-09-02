/* declarations for resize.c
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
 * $Id: resize.h,v 1.3 2004/04/21 20:13:10 mccannwj Exp $
 */

void resize_image_channel        (FitsCutImage *, int, int, int);
void resize_image_sample_enlarge (FitsCutImage *, FitsCutImage *, int);
void resize_image_sample_reduce  (FitsCutImage *, FitsCutImage *, int);
void image_get_row (FitsCutImage *, int, int, int, int, float*, int);
void image_set_row (FitsCutImage *, int, int, int, int, float*);
