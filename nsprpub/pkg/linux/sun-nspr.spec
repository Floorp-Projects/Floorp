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

***** BEGIN LICENSE BLOCK *****
Version: MPL 1.1/GPL 2.0/LGPL 2.1

The contents of this file are subject to the Mozilla Public License Version
1.1 (the "License"); you may not use this file except in compliance with
the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the
License.

The Original Code is the Netscape security libraries.

The Initial Developer of the Original Code is
Netscape Communications Corporation.
Portions created by the Initial Developer are Copyright (C) 1994-2000
the Initial Developer. All Rights Reserved.

Contributor(s):

Alternatively, the contents of this file may be used under the terms of
either the GNU General Public License Version 2 or later (the "GPL"), or
the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
in which case the provisions of the GPL or the LGPL are applicable instead
of those above. If you wish to allow use of your version of this file only
under the terms of either the GPL or the LGPL, and not to allow others to
use your version of this file under the terms of the MPL, indicate your
decision by deleting the provisions above and replace them with the notice
and other provisions required by the GPL or the LGPL. If you do not delete
the provisions above, a recipient may use your version of this file under
the terms of any one of the MPL, the GPL or the LGPL.

***** END LICENSE BLOCK *****

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

%files
%defattr(-,root,root)
%dir /opt
%dir /opt/sun
%dir /opt/sun/private
%dir /opt/sun/private/lib
/opt/sun/private/lib/libnspr4.so
/opt/sun/private/lib/libplc4.so
/opt/sun/private/lib/libplds4.so

%files devel
%defattr(-,root,root)
%dir /opt
%dir /opt/sun
%dir /opt/sun/private
%dir /opt/sun/private/include
%dir /opt/sun/private/include/nspr
%dir /opt/sun/private/include/nspr/obsolete
/opt/sun/private/include/nspr/prcpucfg.h
/opt/sun/private/include/nspr/obsolete/protypes.h
/opt/sun/private/include/nspr/nspr.h
/opt/sun/private/include/nspr/pratom.h
/opt/sun/private/include/nspr/prbit.h
/opt/sun/private/include/nspr/prclist.h
/opt/sun/private/include/nspr/prcmon.h
/opt/sun/private/include/nspr/prcountr.h
/opt/sun/private/include/nspr/prcvar.h
/opt/sun/private/include/nspr/prdtoa.h
/opt/sun/private/include/nspr/prenv.h
/opt/sun/private/include/nspr/prerr.h
/opt/sun/private/include/nspr/prerror.h
/opt/sun/private/include/nspr/prinet.h
/opt/sun/private/include/nspr/prinit.h
/opt/sun/private/include/nspr/prinrval.h
/opt/sun/private/include/nspr/prio.h
/opt/sun/private/include/nspr/pripcsem.h
/opt/sun/private/include/nspr/prlink.h
/opt/sun/private/include/nspr/prlock.h
/opt/sun/private/include/nspr/prlog.h
/opt/sun/private/include/nspr/prlong.h
/opt/sun/private/include/nspr/prmem.h
/opt/sun/private/include/nspr/prmon.h
/opt/sun/private/include/nspr/prmwait.h
/opt/sun/private/include/nspr/prnetdb.h
/opt/sun/private/include/nspr/prolock.h
/opt/sun/private/include/nspr/prpdce.h
/opt/sun/private/include/nspr/prprf.h
/opt/sun/private/include/nspr/prproces.h
/opt/sun/private/include/nspr/prrng.h
/opt/sun/private/include/nspr/prrwlock.h
/opt/sun/private/include/nspr/prshma.h
/opt/sun/private/include/nspr/prshm.h
/opt/sun/private/include/nspr/prsystem.h
/opt/sun/private/include/nspr/prthread.h
/opt/sun/private/include/nspr/prtime.h
/opt/sun/private/include/nspr/prtpool.h
/opt/sun/private/include/nspr/prtrace.h
/opt/sun/private/include/nspr/prtypes.h
/opt/sun/private/include/nspr/prvrsion.h
/opt/sun/private/include/nspr/prwin16.h
/opt/sun/private/include/nspr/plarenas.h
/opt/sun/private/include/nspr/plarena.h
/opt/sun/private/include/nspr/plhash.h
/opt/sun/private/include/nspr/plbase64.h
/opt/sun/private/include/nspr/plerror.h
/opt/sun/private/include/nspr/plgetopt.h
/opt/sun/private/include/nspr/plresolv.h
/opt/sun/private/include/nspr/plstr.h

%changelog
* Sat Jan 18 2003 Kirk Erickson <kirk.erickson@sun.com>
- http://bugzilla.mozilla.org/show_bug.cgi?id=189501 
