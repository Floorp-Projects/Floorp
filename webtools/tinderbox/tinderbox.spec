%define ver 19980831
%define perl /usr/bin/perl
%define cvsroot /cvsroot
%define gzip /usr/bin/gzip
%define uudecode /usr/bin/uudecode
%define bonsai ../bonsai
%define prefix /home/httpd/html/tinderbox
# This rpm is not relocateable
Summary: automated build tool
Name: tinderbox
Version: %{ver}
Release: 1
Copyright: NPL
Group: Networking/Admin
Source: %{name}-%{ver}.tar.gz
BuildRoot: /tmp/build-%{name}
Requires: bonsai
Packager: Christopher Seawood <cls@seawood.org>

%changelog

* Sun Aug 31 1998 Christopher Seawood <cls@seawood.org>
- Made rpm from cvs snapshot

%description
Essentially, Tinderbox is a detective tool. It allows you to see what
is happening in the source tree. It shows you who checked in what (by
asking Bonsai); what platforms have built successfully; what platforms
are broken and exactly how they are broken (the build logs); and the
state of the files that made up the build (cvsblame) so you can figure
out who broke the build, so you can do the most important thing, hold
them accountable for their actions.

%prep
%setup -n %{name}

%build

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p ${RPM_BUILD_ROOT}%{prefix}/{data,examples}
make install PERL=%perl UUDECODE=%uudecode GZIP=%gzip BONSAI=%bonsai CVSROOT=%cvsroot PREFIX=${RPM_BUILD_ROOT}%prefix

%post
echo "Remember to set the admin passwd via '%{bonsai}/data/trapdoor password > %{prefix}/data/passwd"

%clean 
rm -rf $RPM_BUILD_ROOT

%files
%defattr (-, nobody, nobody)
%doc README Makefile
%{prefix}

