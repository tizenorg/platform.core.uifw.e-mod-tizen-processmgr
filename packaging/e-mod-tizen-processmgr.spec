%bcond_with x
%bcond_with wayland

Name: e-mod-tizen-processmgr
Version: 0.1.3
Release: 1
Summary: The enlightenment processmgr module for Tizen
Group: Graphics & UI Framework/Other
Source0: %{name}-%{version}.tar.gz
License: BSD-2-Clause
BuildRequires: pkgconfig(enlightenment)
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(edbus)
%if %{with x}
BuildRequires: pkgconfig(x11)
Requires: libX11
%endif

%global TZ_SYS_RO_SHARE  %{?TZ_SYS_RO_SHARE:%TZ_SYS_RO_SHARE}%{!?TZ_SYS_RO_SHARE:/usr/share}

%description
This package is a processmgr module for enlightenment.

%prep
%setup -q

%build
export GC_SECTIONS_FLAGS="-fdata-sections -ffunction-sections -Wl,--gc-sections"
export CFLAGS+=" -Wall -Werror-implicit-function-declaration -g -fPIC -Werror -rdynamic ${GC_SECTIONS_FLAGS} -DE_LOGGING=1"
export LDFLAGS+=" -Wl,--hash-style=both -Wl,--as-needed -Wl,--rpath=/usr/lib"

%if %{with wayland}
%reconfigure --enable-wayland-only
%else
%reconfigure 
%endif

make

%install
rm -rf %{buildroot}

# for license notification
mkdir -p %{buildroot}/%{TZ_SYS_RO_SHARE}/license
cp -a %{_builddir}/%{buildsubdir}/COPYING %{buildroot}/%{TZ_SYS_RO_SHARE}/license/%{name}

# install
make install DESTDIR=%{buildroot}

# clear useless textual files
find  %{buildroot}%{_libdir}/enlightenment/modules/%{name} -name *.la | xargs rm

%files
%defattr(-,root,root,-)
%{_libdir}/enlightenment/modules/e-mod-tizen-processmgr
%{TZ_SYS_RO_SHARE}/license/%{name}
