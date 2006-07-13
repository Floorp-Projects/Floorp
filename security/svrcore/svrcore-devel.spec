%define nspr_version 4.6
%define nss_version 3.11

Summary:          Svrcore - development files for secure PIN handling using NSS crypto
Name:             svrcore-devel
Version:          4.0.2
Release:          3%{?dist}
License:          MPL/GPL/LGPL
URL:              http://www.mozilla.org/projects/security/pki/
Group:            Development/Libraries
Requires:         nspr-devel >= %{nspr_version}
Requires:         nss-devel >= %{nss_version}
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:    nspr-devel >= %{nspr_version}
BuildRequires:    nss-devel >= %{nss_version}
BuildRequires:    pkgconfig

Source0:          ftp://ftp.mozilla.org/pub/mozilla.org/directory/svrcore/releases/4.0.2/%{name}-%{version}.tar.gz

%description
svrcore provides applications with several ways to handle secure PIN storage
e.g. in an application that must be restarted, but needs the PIN to unlock
the private key and other crypto material, without user intervention.  svrcore
uses the facilities provided by NSS.

%prep
%setup -q

%build

# Enable compiler optimizations and disable debugging code
export BUILD_OPT=1

# Generate symbolic info for debuggers
export XCFLAGS="$RPM_OPT_FLAGS"

export PKG_CONFIG_ALLOW_SYSTEM_LIBS=1
export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1

export NSPR_INCLUDE_DIR=`/usr/bin/pkg-config --variable=includedir nspr`
export NSS_INCLUDE_DIR=`/usr/bin/pkg-config --variable=includedir nss`

%ifarch x86_64 ppc64 ia64 s390x
export USE_64=1
%endif

cd mozilla/security/svrcore
# This make assumes the build is still using the mozilla/security/coreconf stuff,
# which does all kinds of crazy stuff with copying files around, looking for
# dependencies, etc.
make EXPORTS="" RELEASE="" REQUIRES="" MODULE="" IMPORTS="" OBJDIR=. INSTALL=true

%install
%{__rm} -rf $RPM_BUILD_ROOT

# Set up our package file
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/pkgconfig
%{__cat} mozilla/security/svrcore/svrcore.pc.in | sed -e "s,%%libdir%%,%{_libdir},g" \
                          -e "s,%%prefix%%,%{_prefix},g" \
                          -e "s,%%exec_prefix%%,%{_prefix},g" \
                          -e "s,%%includedir%%,%{_includedir},g" \
                          -e "s,%%NSPR_VERSION%%,%{nspr_version},g" \
                          -e "s,%%NSS_VERSION%%,%{nss_version},g" \
                          -e "s,%%SVRCORE_VERSION%%,%{version},g" > \
                          $RPM_BUILD_ROOT%{_libdir}/pkgconfig/%{name}.pc

# There is no make install target so we'll do it ourselves.

%{__mkdir_p} $RPM_BUILD_ROOT%{_includedir}
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}

cd mozilla/security/svrcore
# Copy the binary libraries we want
for file in libsvrcore.a
do
  %{__install} -m 644 $file $RPM_BUILD_ROOT%{_libdir}
done

# Copy the include files
for file in svrcore.h
do
  %{__install} -m 644 $file $RPM_BUILD_ROOT%{_includedir}
done


%clean
%{__rm} -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc mozilla/security/svrcore/LICENSE mozilla/security/svrcore/README
%{_libdir}/pkgconfig/%{name}.pc
%{_libdir}/libsvrcore.a
%{_includedir}/svrcore.h

%changelog
* Thu Jul 13 2006 Rich Megginson <rmeggins@redhat.com> - 4.0.2-3
- Bump spec rev to 3
- Remove unneeded buildrequires perl, gawk, sed
- Remove leading / from path macros
- Remove provides for package name - done automatically
- Move pkgconfig file stuff under install
- Added LICENSE and README under docs

* Mon Jun 26 2006 Rich Megginson <rmeggins@redhat.com> - 4.0.2-2
- Bump spec rev to 2 due to change of spec file name from svrcore
- to svrcore-devel to comply with fedora packaging guidelines

* Thu Jun 22 2006 Rich Megginson <rmeggins@redhat.com> - 4.0.2-1
- Bump rev to 4.0.2; now using HEAD of mozilla/security/coreconf
- which includes the coreconf-location.patch, so got rid of patch

* Tue Apr 18 2006 Rich Megginson <rmeggins@redhat.com> - 4.0.1-3
- Use pkg-config --variable=includedir to get include dirs

* Wed Feb  1 2006 Rich <rmeggins@redhat.com> - 4.0.1-2
- Requires nss version was wrong

* Wed Jan 11 2006 Rich Megginson <rmeggins@redhat.com> - 4.01-1
- Removed svrcore-config - use pkg-config instead

* Mon Dec 19 2005 Rich Megginson <rmeggins@redhat.com> - 4.01-1
- Initial revision
