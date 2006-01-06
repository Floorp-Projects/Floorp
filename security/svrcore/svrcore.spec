%define nspr_version 4.6
%define nss_version 3.11

Summary:          Svrcore - development files for secure PIN handling using NSS crypto
Name:             svrcore-devel
Version:          4.0.1
Release:          1
License:          MPL/GPL/LGPL
URL:              http://www.mozilla.org/projects/security/pki/
Group:            Development/Libraries
Requires:         nspr-devel >= %{nspr_version}, nss-devel >= %{nspr_version}
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:    nspr-devel >= %{nspr_version}, nss-devel >= %{nss_version}
BuildRequires:    pkgconfig
BuildRequires:    gawk
Provides:         svrcore-devel

Source0:          %{name}-%{version}.tar.gz

%description
svrcore provides applications with several ways to handle secure PIN storage
e.g. in an application that must be restarted, but needs the PIN to unlock
the private key and other crypto material, without user intervention.  svrcore
uses the facilities provided by NSS.

%prep
%setup -q

%build

# Enable compiler optimizations and disable debugging code
BUILD_OPT=1
export BUILD_OPT

# Generate symbolic info for debuggers
XCFLAGS=$RPM_OPT_FLAGS
export XCFLAGS

#export NSPR_INCLUDE_DIR=`nspr-config --includedir`
#export NSPR_LIB_DIR=`nspr-config --libdir`

PKG_CONFIG_ALLOW_SYSTEM_LIBS=1
PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1

export PKG_CONFIG_ALLOW_SYSTEM_LIBS
export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS

NSPR_INCLUDE_DIR=`/usr/bin/pkg-config --cflags-only-I nspr | sed 's/-I//'`

export NSPR_INCLUDE_DIR

NSS_INCLUDE_DIR=`/usr/bin/pkg-config --cflags-only-I nss | sed 's/-I//'`

export NSS_INCLUDE_DIR

%ifarch x86_64 ppc64 ia64 s390x
USE_64=1
export USE_64
%endif

cd mozilla/security/svrcore
# This make assumes the build is still using the mozilla/security/coreconf stuff,
# which does all kinds of crazy stuff with copying files around, looking for
# dependencies, etc.
make EXPORTS="" RELEASE="" REQUIRES="" MODULE="" IMPORTS="" OBJDIR=. INSTALL=true

# Set up our package file
%{__mkdir_p} $RPM_BUILD_ROOT/%{_libdir}/pkgconfig
%{__cat} svrcore.pc.in | sed -e "s,%%libdir%%,%{_libdir},g" \
                          -e "s,%%prefix%%,%{_prefix},g" \
                          -e "s,%%exec_prefix%%,%{_prefix},g" \
                          -e "s,%%includedir%%,%{_includedir}/nss3,g" \
                          -e "s,%%NSPR_VERSION%%,%{nspr_version},g" \
                          -e "s,%%NSS_VERSION%%,%{nss_version},g" \
                          -e "s,%%SVRCORE_VERSION%%,%{version},g" > \
                          $RPM_BUILD_ROOT/%{_libdir}/pkgconfig/svrcore.pc

VMAJOR=`echo %{version}|cut -f1 -d.`
VMINOR=`echo %{version}|cut -f2 -d.`
VPATCH=`echo %{version}|cut -f3 -d.`

%{__mkdir_p} $RPM_BUILD_ROOT/%{_bindir}
%{__cat} svrcore-config.in | sed -e "s,@libdir@,%{_libdir},g" \
                          -e "s,@prefix@,%{_prefix},g" \
                          -e "s,@exec_prefix@,%{_prefix},g" \
                          -e "s,@includedir@,%{_includedir},g" \
                          -e "s,@MOD_MAJOR_VERSION@,$VMAJOR,g" \
                          -e "s,@MOD_MINOR_VERSION@,$VMINOR,g" \
                          -e "s,@MOD_PATCH_VERSION@,$VPATCH,g" \
                          > $RPM_BUILD_ROOT/%{_bindir}/svrcore-config

chmod 755 $RPM_BUILD_ROOT/%{_bindir}/svrcore-config


%install

# There is no make install target so we'll do it ourselves.

%{__mkdir_p} $RPM_BUILD_ROOT/%{_includedir}
%{__mkdir_p} $RPM_BUILD_ROOT/%{_libdir}
%{__mkdir_p} $RPM_BUILD_ROOT/%{_bindir}

cd mozilla/security/svrcore
# Copy the binary libraries we want
for file in libsvrcore.a
do
  %{__install} -m 644 $file $RPM_BUILD_ROOT/%{_libdir}
done

# Copy the include files
for file in svrcore.h
do
  %{__install} -m 644 $file $RPM_BUILD_ROOT/%{_includedir}
done


%clean
%{__rm} -rf $RPM_BUILD_ROOT


%post
/sbin/ldconfig >/dev/null 2>/dev/null


%postun
/sbin/ldconfig >/dev/null 2>/dev/null


%files
%defattr(0644,root,root)
%{_libdir}/pkgconfig/svrcore.pc
%{_libdir}/libsvrcore.a
%attr(0755,root,root) %{_bindir}/svrcore-config
%{_includedir}/svrcore.h

%changelog
* Mon Dec 19 2005 Rich Megginson <rmeggins@redhat.com> - 4.01-1
- Initial revision

