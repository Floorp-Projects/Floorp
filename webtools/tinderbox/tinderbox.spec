# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License
# at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See 
# the License for the specific language governing rights and 
# limitations under the License.
# 
# The Original Code is the Tinderbox CVS Tool rpm spec file.
#
# The Initial Developer of this code under the MPL is Christopher
# Seawood, <cls@seawood.org>.  Portions created by Christopher Seawood 
# are Copyright (C) 1998 Christopher Seawood.  All Rights Reserved.

%define ver SNAP
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
BuildRoot: /var/tmp/build-%{name}
Requires: bonsai
Packager: Christopher Seawood <cls@seawood.org>

%changelog
* Thu Nov 12 1998 Christopher Seawood <cls@seawood.org>
- Replaced ver with SNAP

* Mon Oct 26 1998 Christopher Seawood <cls@seawood.org>
- Added MPL header

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

