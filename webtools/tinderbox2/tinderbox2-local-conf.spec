%define _prefix /export/home/tinderbox2

# auto generate the version number based on the output of the date
# command.

%define _version %(eval "date '+%Y%m%e'")

Summary: Development monitoring tool
Name: tinderbox2-local-conf
Version: %{_version}
Release: 1
Copyright: MPL
Group: Development/Tools
Source: tar://tinderbox2_local_conf.tar.gz
Prefix: %{_prefix}
Buildroot: /var/tmp/%{name}-root

%description

The local configuration files for tinderbox2.  This package customizes
tinderbox2 for the local use.  The tinderbox2 package is genaric, this
package contains all the discriptions of the local system.


%prep
# empty prep

%build
#empty build

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_prefix}
mkdir -p $RPM_BUILD_ROOT/var/log/tinderbox2

cd $RPM_BUILD_ROOT/%{_prefix}
tar zxf %{_sourcedir}/tinderbox2_local_conf.tar.gz

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,tinderbox,root)
%{_prefix}/local_conf/*
/var/log/tinderbox2
