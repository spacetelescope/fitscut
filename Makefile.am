bin_PROGRAMS = fitscut

if HAVE_LIBWCS

wcs_SOURCES = wcs_align.c wcs_align.h

fitscut_LDADD = $(WCS_LIBS)

endif

fitscut_SOURCES = 	\
	blurb.c		\
	colormap.c	\
	draw.c		\
	extract.c	\
	file_check.c	\
	fitscut.c	\
	getopt1.c	\
	getopt.c	\
	histogram.c	\
	image_scale.c	\
	output_fits.c	\
	output_graphic.c	\
	output_json.c	\
	resize.c	\
	util.c		\
	colormap.h	\
	draw.h		\
	extract.h	\
	file_check.h	\
	fitscut.h	\
	getopt.h	\
	histogram.h	\
	image_scale.h	\
	output_fits.h	\
	output_graphic.h	\
	output_json.h	\
	resize.h	\
	util.h		\
	tailor.h	\
	revision.h	\
	$(wcs_SOURCES)

EXTRA_DIST = 		\
	AUTHORS		\
	ChangeLog	\
	COPYING		\
	INSTALL		\
	NEWS		\
	README		\
	THANKS		\
	TODO		\
	fitscut.spec.in \
	fitscut.spec	\
	test.fits

test: check
check: fitscut
	./fitscut -vv --x0=1 --y0=1 --columns=50 --rows=60 test.fits > _test.fits
	@LANG=""; export LANG; if test "-s _test.fits"; then \
	if test `wc -c < _test.fits` -eq 17280; then \
	   echo fitscut FITS test OK; \
	else \
	   echo FAILED fitscut FITS test: wrong size; \
	fi; \
	else \
	   echo FAILED fitscut FITS test: no output; \
	fi
	rm -f _test.fits
	./fitscut -vv --png --x0=1 --y0=1 --columns=50 --rows=60 test.fits > _test.png
	@LANG=""; export LANG; if test "-s _test.fits"; then \
	if [ -f "_test.png" ]; then \
	   echo fitscut PNG test OK; \
	else \
	   echo FAILED fitscut PNG test: wrong size; \
	fi; \
	else \
	   echo FAILED fitscut PNG test: no output; \
	fi
	rm -f _test.png
	./fitscut -vv --png --log-scale --x0=1 --y0=1 --columns=50 --rows=60 test.fits > _test.png
	@LANG=""; export LANG; if [ -s "_test.png" ]; then \
	if [ -f "_test.png" ]; then \
	   echo fitscut PNG log test OK; \
	else \
	   echo FAILED fitscut PNG log test: wrong size; \
	fi; \
	else \
	   echo FAILED fitscut PNG log test: no output; \
	fi
	rm -f _test.png
	./fitscut -vv --png --sqrt-scale --x0=1 --y0=1 --columns=50 --rows=60 test.fits > _test.png
	@LANG=""; export LANG; if [ -s "_test.png" ]; then \
	if [ -f "_test.png" ]; then \
	   echo fitscut PNG sqrt test OK; \
	else \
	   echo FAILED fitscut PNG sqrt test: wrong size; \
	fi; \
	else \
	   echo FAILED fitscut PNG sqrt test: no output; \
	fi
	rm -f _test.png
	./fitscut -vv --png --autoscale=98 --x0=1 --y0=1 --columns=50 --rows=60 test.fits > _test.png
	@LANG=""; export LANG; if [ -s "_test.png" ]; then \
	if [ -f "_test.png" ]; then \
	   echo fitscut PNG autoscale test OK; \
	else \
	   echo FAILED fitscut PNG autoscale test: wrong size; \
	fi; \
	else \
	   echo FAILED fitscut PNG autoscale  test: no output; \
	fi
	rm -f _test.png
	./fitscut -vv --png --autoscale=98 --all --zoom=2.0 test.fits > _test.png
	@LANG=""; export LANG; if [ -s "_test.png" ]; then \
	if [ -f "_test.png" ]; then \
	   echo fitscut PNG zoom test OK; \
	else \
	   echo FAILED fitscut PNG zoom test: wrong size; \
	fi; \
	else \
	   echo FAILED fitscut PNG zoom test: no output; \
	fi
	rm -f _test.png
	./fitscut -vv --png --autoscale=98 --all --asinh-scale test.fits > _test.png
	@LANG=""; export LANG; if [ -s "_test.png" ]; then \
	if [ -f "_test.png" ]; then \
	   echo fitscut PNG asinh test OK; \
	else \
	   echo FAILED fitscut PNG asinh test: wrong size; \
	fi; \
	else \
	   echo FAILED fitscut PNG asinh test: no output; \
	fi
	rm -f _test.png

