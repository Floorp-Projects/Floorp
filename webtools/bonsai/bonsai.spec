%define _prefix /var/www/cgi-bin/bonsai

# auto generate the version number based on the output of the date
# command.

%define _version %(eval "date '+%Y%m%d'")

Summary: Web and SQL interface to CVS
Name: bonsai
Version: %{_version}
Release: 1
Copyright: MPL
Group: Development/Tools
Source: cvs://:pserver:anonymous@cvs-mirror.mozilla.org:/cvsroot:mozilla/webtools/bonsai/bonsai.tar.gz
Prereq: apache
Prefix: %{_prefix}
Buildroot: /var/tmp/%{name}-root

%description


%prep
%setup -q -n bonsai


%build

prefix='%{_prefix}' \
	./configure

make

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_prefix}

make 	PREFIX=$RPM_BUILD_ROOT/%{_prefix} \
	install

# the data directory needs to be group writable so that the cgi's can update
# files in it.  No other program needs to use this directory.

chmod 770 $RPM_BUILD_ROOT/%{_prefix}/data

# config files do not belong as part of this package,
# they have their own package

rm -rf $RPM_BUILD_ROOT/%{_prefix}/data/*


# the makefile makes two empty files by mistake

rm -rf $RPM_BUILD_ROOT/%{_prefix}/branchspam*

%clean
#rm -rf $RPM_BUILD_ROOT

# the data dir must be writable by the cgi process.
%files
%defattr(-,root,root)
%{_prefix}
%defattr(-,apache,apache)
%{_prefix}/data
