The contents of this file are subject to the Mozilla Public
License Version 1.1 (the "License"); you may not use this file
except in compliance with the License. You may obtain a copy of
the License at http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS
IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
implied. See the License for the specific language governing
rights and limitations under the License.

The Original Code is Netscape Security Services for Java.

The Initial Developer of the Original Code is Netscape
Communications Corporation.  Portions created by Netscape are 
Copyright (C) 1998-2000 Netscape Communications Corporation.  All
Rights Reserved.

Contributor(s):

Alternatively, the contents of this file may be used under the
terms of the GNU General Public License Version 2 or later (the
"GPL"), in which case the provisions of the GPL are applicable 
instead of those above.  If you wish to allow use of your 
version of this file only under the terms of the GPL and not to
allow others to use your version of this file under the MPL,
indicate your decision by deleting the provisions above and
replace them with the notice and other provisions required by
the GPL.  If you do not delete the provisions above, a recipient
may use your version of this file under either the MPL or the
GPL.

**********************************************************************

README.ninja contains instructions for building Ninja normally.
The following addenda tell how to build the version of Ninja that
is included with NSS 2.0.

1. Checkout ns/ninja with the tag NSS2_0_RELEASE instead of JSS_1_3.

2. Before building, do "setenv STANDALONE_LIBJSS 1".  This will cause
libjss.so/jss.dll to have NSPR embedded in it. Normal Ninja builds do
not embed NSPR.

3. To release to /m/dist, do NOT do "gmake release". First set the release
version to something suitable by doing something like
"setenv RELEASE_VERSION NSS_2_0". Then, in the top level directory of
ninja (ns/ninja), do "gmake nss_release".  This will create one of the
following outputs in /m/dist/ninja/<RELEASE_VERSION>:

UNIX
====
unixjss.tar
    - jss.zip
        - contains JSS classes for SSL and initialization
    - libjss.so
        - native implementation library, consists of security, NSPR, DBM,
	      and the native JSS code.

WINDOWS
=======
winjss.zip
    - jss.zip
        - same as in the UNIX version, contains JSS classes for SSL
          and initialization
    - jss.dll
        - native implementation library, consists of security, NSPR, DBM,
          and the native JSS code.
