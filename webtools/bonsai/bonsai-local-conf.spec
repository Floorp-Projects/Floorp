%define _prefix /var/www/cgi-bin/bonsai

# auto generate the version number based on the output of the date
# command.

%define _version %(eval "date '+%Y%m%d'")

Summary: Development monitoring tool
Name: bonsai-local-conf
Version: %{_version}
Release: 1
Copyright: MPL
Group: Development/Tools
Source: tar://bonsai_local_conf.tar.gz
Prefix: %{_prefix}
Buildroot: /var/tmp/%{name}-root

%description

The local configuration files for bonsai.  This package customizes
bonsai for the local use.  The bonsai package is genaric, this
package contains all the discriptions of the local system by providing
the data subdirectory files.


%prep
# empty prep

%build
#empty build

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_prefix}

cd $RPM_BUILD_ROOT/%{_prefix}
tar zxf %{_sourcedir}/bonsai_local_conf.tar.gz

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,apache,apache)
%{_prefix}/data/*
