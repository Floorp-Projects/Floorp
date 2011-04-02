# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Application packaging specific RPM Spec file.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   John Ford <jhford@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

%global __jar_repack %{nil}

#Use a consistent string to refer to the package by
%define pr_name "%{moz_app_displayname} %{moz_app_version}"

Name:           %{moz_app_name}
Version:        %{moz_numeric_app_version}
Release:        %{?moz_rpm_release:%{moz_rpm_release}}%{?buildid:.%{buildid}}
Summary:        %{pr_name}
Group:          Applications/Internet
License:        MPLv1.1 or GPLv2+ or LGPLv2+
Vendor:         Mozilla
URL:            http://www.mozilla.org/projects/firefox/
Source0:        %{name}.desktop
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

#AutoProv:       no

BuildRequires:  desktop-file-utils


%description
%{pr_name}.  This package was built from 
%{moz_source_repo}/rev/%{moz_source_stamp}

#We only want a subpackage for the SDK if the required
#files were generated.  Like the tests subpackage, we
#probably only need to conditionaly define the %files
#section.
%if %{?createdevel:1}
%package devel
Summary:    %{pr_name} SDK
Group:      Development/Libraries
requires:   %{name} = %{version}-%{release}
%description devel
%{pr_name} SDK libraries, headers and interface descriptions
%endif

%if %{?createtests:1}
%package tests
Summary:    %{pr_name} tests
Group:      Developement/Libraries
requires:   %{name} = %{version}-%{release}
%description tests
%{pr_name} test harness files and test cases
%endif

%prep
echo No-op prep


%build
echo No-op build


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
desktop-file-validate %{SOURCE0}
desktop-file-install --vendor mozilla \
    --dir $RPM_BUILD_ROOT%{_datadir}/applications \
    %{SOURCE0}
#In order to make branding work in a generic way, We find
#all the icons that are likely to be used for desktop files
#and install them appropriately
find %{moz_branding_directory} -name "default*.png" | tee icons.list
for i in $(cat icons.list) ; do
    size=$(echo $i | sed "s/.*default\([0-9]*\).png$/\1/")
    icondir=$RPM_BUILD_ROOT/%{_datadir}/icons/hicolor/${size}x${size}/apps/
    mkdir -p $icondir
    cp -a $i ${icondir}%{name}.png
done
rm icons.list #cleanup

%if %{?createtests:1}
#wastefully creates a zip file, but ensures that we stage all test suites
make package-tests
testdir=$RPM_BUILD_ROOT/%{_datadir}/%{_testsinstalldir}/tests
mkdir -p $testdir
cp -a dist/test-package-stage/* $testdir/
%endif

%clean
rm -rf $RPM_BUILD_ROOT


%post
#this is needed to get gnome-panel to update the icons
update-desktop-database &> /dev/null || :
touch --no-create %{_datadir}/icons/hicolor || :
if [ -x %{_bindir}/gtk-update-icon-cache ] ; then
    %{_bindir}/gtk-update-icon-cache --quiet ${_datadir}/icons/hicolor &> /dev/null || :
fi


%postun
#this is needed to get gnome-panel to update the icons
update-desktop-database &> /dev/null || :
touch --no-create %{_datadir}/icons/hicolor || :
if [ -x %{_bindir}/gtk-update-icon-cache ] ; then
    %{_bindir}/gtk-update-icon-cache --quiet ${_datadir}/icons/hicolor &> /dev/null || :
fi


%files
%defattr(-,root,root,-)
%{_installdir}
%{_bindir}
%{_datadir}/applications/
%{_datadir}/icons/
%doc


%if %{?createdevel:1}
%files devel
%defattr(-,root,root,-)
%{_includedir}
%{_sdkdir}
%{_idldir}
%endif


%if %{?createtests:1}
%files tests
%{_datadir}/%{_testsinstalldir}/tests/
%endif

#%changelog
#* %{name} %{version} %{moz_rpm_release}
#- Please see %{moz_source_repo}/shortlog/%{moz_source_stamp}
