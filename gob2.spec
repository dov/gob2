%define  ver     2.0.21
%define  rel     1
%define  prefix  /usr

Summary: GOB2, The GObject Builder
Name: gob2
Version: %ver
Release: %rel
Copyright: GPL
Group: Development/Tools
Source: gob2-%{ver}.tar.gz
Url: http://www.5z.com/jirka/linux.html#gob
BuildRoot: /var/tmp/gob2-%{ver}-root
Prefix: %prefix

%description
GOB is a simple preprocessor for making GObject objects (glib objects).
It makes objects from a single file which has inline C code so that
you don't have to edit the generated files.  Syntax is somewhat inspired
by java and yacc.  It supports generating C++ code

%changelog
* Fri Sep 28 2001  George Lebl <jirka@5z.com>
- Updated for gob2
* Tue Feb 7 2000  George Lebl <jirka@5z.com>
- added %{prefix}/share/aclocal/* to %files
* Tue Dec 14 1999  George Lebl <jirka@5z.com>
- added the examples dir to the %doc
* Mon Aug 16 1999  George Lebl <jirka@5z.com>
- added gob.spec.in file

%prep
%setup

%build
./configure --prefix=%prefix

if [ "$SMP" != "" ]; then
  (make "MAKE=make -k -j $SMP"; exit 0)
  make
else
  make
fi

%install
rm -rf $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root)

%doc README AUTHORS COPYING NEWS TODO ChangeLog
%doc examples

%{prefix}/bin/*
%{prefix}/man/man1/*
%{prefix}/share/aclocal/*
