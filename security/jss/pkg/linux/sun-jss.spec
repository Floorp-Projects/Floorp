Summary: Network Security Services for Java
Name: NAME_REPLACE
Vendor: Sun Microsystems, Inc.
Version: VERSION_REPLACE
Release: RELEASE_REPLACE
Copyright: Copyright 2004 Sun Microsystems, Inc.  All rights reserved.  Use is subject to license terms.  Also under other license(s) as shown at the Description field.
Distribution: Sun Java(TM) Enterprise System
URL: http://www.sun.com
Group: System Environment/Base
Source: %{name}-%{version}.tar.gz
ExclusiveOS: Linux
BuildRoot: %_topdir/%{name}-root

Requires: sun-nspr >= 4.3, sun-nss >= 3.9
        
%description
Network Security Services for Java (JSS) is a set of libraries designed
to support cross-platform development of security-enabled server
applications. Applications built with JSS can support SSL v2
and v3, TLS, PKCS #5, PKCS #7, PKCS #11, PKCS #12, S/MIME,
X.509 v3 certificates, and other security standards.  See:
http://www.mozilla.org/projects/security/pki/jss/

Under "MPL/GPL" license.

%package devel
Summary: Development Libraries for Network Security Services for Java
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
Header files for doing development with Network Security Services for Java.

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
