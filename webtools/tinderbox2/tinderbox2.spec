%define _prefix /home/tinderbox2
%define _cgi_prefix /var/www/cgi-bin

# auto generate the version number based on the output of the date
# command.

%define _version %(eval "date '+%Y%m%d'")

Summary: Development Monitoring Tool
Name: tinderbox2
Version: %{_version}
Release: 1
Copyright: MPL
Group: Development/Tools
Source: cvs://:pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot:mozilla/webtools/tinderbox2/tinderbox2.tar.gz
Prefix: %{_prefix}
Buildroot: /var/tmp/%{name}-root
Requires: perl, apache, tinderbox2-local-conf

%package buildclient
Summary: tinderbox build client software
Group: Development/Tools
Requires: perl, tinderbox2-local-conf


%description

Tinderbox is the first program to allow developers and management to
see at a glance what is currently going on in all aspects of the
development process. The Tinderbox server prepares HTML pages which
display the history of many different development variables. It shows
at a glance the history of: whether the HEAD branch of the source code
builds and passes all automated tests, who has checked code changes
into the version control system, whether the source tree is open or
closed and when the state of the tree last changed, what trouble
tickets have been closed, notices and messages posted by the
developers or project manager.

%description buildclient

This package contains the executables needed on the buildmachine and
no exectuables which are intended for the web machine.

%prep
%setup -q -n tinderbox2


%build

# These executables are not used, and are causing problems for the
# dependency engines.

rm -f ./src/bin/bustagestats.cgi ./src/bin/gifsize

prefix='%{_prefix}' cgibin_prefix='%{_cgi_prefix}/%{name}' \
	./configure

make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_prefix}
mkdir -p $RPM_BUILD_ROOT/%{_cgi_prefix}/%{name}

make 	prefix=$RPM_BUILD_ROOT/%{_prefix} \
	cgibin_prefix=$RPM_BUILD_ROOT/%{_cgi_prefix}/%{name} \
	install


# Do not put any testing (if present) directories into the package.

rm -rf $RPM_BUILD_ROOT/%{_prefix}/local_conf

# however this package should own the directory

mkdir -p $RPM_BUILD_ROOT/%{_prefix}/local_conf

%clean
#rm -rf $RPM_BUILD_ROOT


%files buildclient
%defattr(-,build,build)
%{_prefix}/clientbin


%files
%defattr(-,apache,apache)
%{_prefix}/bin
%{_prefix}/lib
%{_prefix}/default_conf
%{_prefix}/local_conf
%{_cgi_prefix}/%{name}

