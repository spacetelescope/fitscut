General Information
===================

fitscut is designed to extract cutouts from FITS image format files.
FITS, PNG, and JPEG output types are supported.  

When multiple input files are specified and the output type is PNG or
JPEG the resulting image is an RGB color image.


Information on the FITS file format is available from
http://fits.gsfc.nasa.gov/.


Dependencies
============

In order to build fitscut you will need:

  CFITSIO library  
    <http://heasarc.gsfc.nasa.gov/docs/software/fitsio/fitsio.html>

  LIBPNG
    <http://www.libpng.org/pub/png/libpng.html>

  JPEG library
    <http://www.ijg.org/>

And optionally:

  WCS Subroutine library
    <ftp://cfa-ftp.harvard.edu/pub/gsc/WCSTools/>
    

Supported Platforms
===================

fitscut has been tested and is known to work on:

  RedHat Linux 8, 9
  RedHat Enterprise Linux 3
  Fedora Core 1
  Solaris 8, 9

Installation
============

Please see the INSTALL file.

This usually consists of:

  % gzip -cd fitscut-1.4.0.tar.gz | tar xvf -     # unpack the sources
  % cd fitscut-1.4.0                              # change to the directory
  % ./configure                                   # run the 'configure' script
  % make                                          # build fitscut
  [ Become root if necessary ]
  % make install                                  # install fitscut

Contact
=======

Please send all comments and bug reports by electronic mail to:
William Jon McCann <mccann@jhu.edu>
