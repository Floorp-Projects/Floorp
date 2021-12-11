Summary: Netscape Portable Runtime
Name: %{name}
Vendor: Sun Microsystems, Inc.
Version: %{version}
Release: %{release}
Copyright: Copyright 2005 Sun Microsystems, Inc.  All rights reserved.  Use is subject to license terms.  Also under other license(s) as shown at the Description field.
Distribution: Sun Java(TM) Enterprise System
URL: http://www.sun.com
Group: System Environment/Base
Source: %{name}-%{version}.tar.gz
ExclusiveOS: Linux
BuildRoot: /var/tmp/%{name}-root
        
%description

NSPR provides platform independence for non-GUI operating system
facilities. These facilities include threads, thread synchronization,
normal file and network I/O, interval timing and calendar time, basic
memory management (malloc and free) and shared library linking. 

See: http://www.mozilla.org/projects/nspr/about-nspr.html

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

%package devel
Summary: Development Libraries for the Netscape Portable Runtime
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
Header files for doing development with the Netscape Portable Runtime.

Under "MPL/GPL" license.

%prep
%setup -c

%build

%install
rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT
cd $RPM_BUILD_ROOT
tar xvzf $RPM_SOURCE_DIR/%{name}-%{version}.tar.gz

%clean
rm -rf $RPM_BUILD_ROOT
