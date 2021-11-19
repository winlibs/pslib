# Note that this is NOT a relocatable package
%define ver      0.4.6
%define prefix   /usr

Summary: Library for creating PostScript
Name: pslib
Version: %ver
Release: 1
License: LGPL
Group: Development/Libraries
Source: http://prdownloads.sourceforge.net/pslib/pslib-%{ver}.tar.gz
BuildRoot: /var/tmp/pslib-%{PACKAGE_VERSION}-root

URL: http://pslib.sourceforge.net/
Docdir: %{prefix}/doc

%description
This library allows to create PostScript files.

%package devel
Summary: Libraries, includes, etc. to develop PostScript applications
Group: Development/Libraries
Requires: pslib = %{version}

%description devel
Libraries, include files, etc you can use to develop PostScript applications.

%changelog

%prep
%setup

%build
# Needed for snapshot releases.
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%{prefix} --mandir=%{prefix}/share/man
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{prefix} --mandir=%{prefix}/share/man
fi

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)

%doc AUTHORS ChangeLog NEWS README COPYING
%{prefix}/lib/lib*.so.*
%attr(-,root,root) %{prefix}/share/locale/*/LC_MESSAGES/*

%files devel
%defattr(-, root, root)

%{prefix}/lib/lib*.so
%{prefix}/lib/*a
%{prefix}/lib/*la
%{prefix}/include/*
%{prefix}/lib/pkgconfig/*
%{prefix}/share/pslib/*
%doc %{prefix}/share/man/man3/*.3*
