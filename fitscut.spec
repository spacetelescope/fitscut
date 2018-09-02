Summary: A tool for making cutouts and color images from FITS files
Name: fitscut
Version: 1.4.4
Release: 1
License: GPL
Group: Applications/Multimedia
Source: fitscut-%{version}.tar.gz
Buildroot: %{_tmppath}/%{name}-%{version}-root
BuildPrereq: libjpeg-devel, libpng-devel, cfitsio-devel

%description 

%prep
%setup -q

%build
%configure

make \
	CC=%{__cc} \
	CFLAGS="$RPM_OPT_FLAGS"

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT
make

mkdir -p $RPM_BUILD_ROOT%{_bindir}

%makeinstall

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/*

%changelog
* Tue May 11 2004 William Jon McCann <mccann@jhu.edu>
- Initial version
