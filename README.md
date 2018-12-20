General Information
===================

fitscut is designed to extract cutouts from FITS image format files.
FITS, PNG, and JPEG output types are supported.

When multiple input files are specified and the output type is PNG or
JPEG the resulting image is an RGB color image.


Information on the FITS file format is available from
http://fits.gsfc.nasa.gov/.


A cgi interface to fitscut is available here: https://hla.stsci.edu/fitscutcgi_interface.html

Dependencies
============

In order to build fitscut you will need:

- CFITSIO library: <http://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html>

- LIBPNG: <http://www.libpng.org/pub/png/libpng.html>

- JPEG library: <http://www.ijg.org/>

- WCS Subroutine library: <http://tdc-www.harvard.edu/software/wcstools/>\
(The WCS library was originally optional but is required in this version of fitscut.)

Supported Platforms
===================

fitscut has been tested and is known to work on:

RedHat Linux 8, 9\
RedHat Enterprise Linux 3\
Fedora Core 1\
Solaris 8, 9
MacOS 10.12 (and other OSX versions)

Installation
============

Please see the INSTALL file.

This usually consists of:

```
  % tar xzvf fitscut-1.4.0.tar.gz                 # unpack the sources
  % cd fitscut-1.4.0                              # change to the directory
  % ./configure                                   # run the 'configure' script
  % make                                          # build fitscut
  [ Become root if necessary ]
  % make install                                  # install fitscut
```

The usual configure options are available (e.g., `--prefix=/your/install/location` to specify where the binary executable should be installed).
You can define the `CFLAGS` and `LDFLAGS` environment variables to specify the locations of include files and libraries before running
`./configure`.

-----
Copyright (C) 2001 William Jon McCann

