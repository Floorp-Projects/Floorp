Summary: Netscape Portable Runtime
Name: %{name}
Vendor: Sun Microsystems
Version: %{version}
Release: %{release}
Copyright: MPL/GPL
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

%package devel
Summary: Development Libraries for the Netscape Portable Runtime
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
Header files for doing development with the Netscape Portable Runtime.

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
%dir /usr
%dir /usr/lib
%dir /usr/lib/mps
/usr/lib/mps/libnspr4.so
/usr/lib/mps/libplc4.so
/usr/lib/mps/libplds4.so

%files devel
%defattr(-,root,root)
/usr/lib/mps/libnspr4.a
/usr/lib/mps/libplc4.a
/usr/lib/mps/libplds4.a
%dir /usr
%dir /usr/include
%dir /usr/include/mps
%dir /usr/include/mps/nspr
%dir /usr/include/mps/nspr/obsolete
%dir /usr/include/mps/nspr/private
/usr/include/mps/nspr/private/pprio.h
/usr/include/mps/nspr/private/pprthred.h
/usr/include/mps/nspr/private/prpriv.h
/usr/include/mps/nspr/prcpucfg.h
/usr/include/mps/nspr/obsolete/pralarm.h
/usr/include/mps/nspr/obsolete/probslet.h
/usr/include/mps/nspr/obsolete/protypes.h
/usr/include/mps/nspr/obsolete/prsem.h
/usr/include/mps/nspr/nspr.h
/usr/include/mps/nspr/pratom.h
/usr/include/mps/nspr/prbit.h
/usr/include/mps/nspr/prclist.h
/usr/include/mps/nspr/prcmon.h
/usr/include/mps/nspr/prcountr.h
/usr/include/mps/nspr/prcvar.h
/usr/include/mps/nspr/prdtoa.h
/usr/include/mps/nspr/prenv.h
/usr/include/mps/nspr/prerr.h
/usr/include/mps/nspr/prerror.h
/usr/include/mps/nspr/prinet.h
/usr/include/mps/nspr/prinit.h
/usr/include/mps/nspr/prinrval.h
/usr/include/mps/nspr/prio.h
/usr/include/mps/nspr/pripcsem.h
/usr/include/mps/nspr/prlink.h
/usr/include/mps/nspr/prlock.h
/usr/include/mps/nspr/prlog.h
/usr/include/mps/nspr/prlong.h
/usr/include/mps/nspr/prmem.h
/usr/include/mps/nspr/prmon.h
/usr/include/mps/nspr/prmwait.h
/usr/include/mps/nspr/prnetdb.h
/usr/include/mps/nspr/prolock.h
/usr/include/mps/nspr/prpdce.h
/usr/include/mps/nspr/prprf.h
/usr/include/mps/nspr/prproces.h
/usr/include/mps/nspr/prrng.h
/usr/include/mps/nspr/prrwlock.h
/usr/include/mps/nspr/prshma.h
/usr/include/mps/nspr/prshm.h
/usr/include/mps/nspr/prsystem.h
/usr/include/mps/nspr/prthread.h
/usr/include/mps/nspr/prtime.h
/usr/include/mps/nspr/prtpool.h
/usr/include/mps/nspr/prtrace.h
/usr/include/mps/nspr/prtypes.h
/usr/include/mps/nspr/prvrsion.h
/usr/include/mps/nspr/prwin16.h
/usr/include/mps/nspr/plarenas.h
/usr/include/mps/nspr/plarena.h
/usr/include/mps/nspr/plhash.h
/usr/include/mps/nspr/plbase64.h
/usr/include/mps/nspr/plerror.h
/usr/include/mps/nspr/plgetopt.h
/usr/include/mps/nspr/plresolv.h
/usr/include/mps/nspr/plstr.h

%changelog
* Sat Jan 18 2003 Kirk Erickson <kirk.erickson@sun.com>
- http://bugzilla.mozilla.org/show_bug.cgi?id=189501 
