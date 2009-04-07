/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This test verifies that NSS public headers can be compiled with no
 * NSPR 1.0 support.
 */

#define NO_NSPR_10_SUPPORT 1

#include "base64.h"
#include "blapit.h"
#include "cert.h"
#include "certdb.h"
#include "certt.h"
#include "ciferfam.h"
#include "cmmf.h"
#include "cmmft.h"
#include "cms.h"
#include "cmsreclist.h"
#include "cmst.h"
#include "crmf.h"
#include "crmft.h"
#include "cryptohi.h"
#include "cryptoht.h"
#include "ecl-exp.h"
#include "hasht.h"
#include "key.h"
#include "keyhi.h"
#include "keyt.h"
#include "keythi.h"
#include "nss.h"
#include "nssb64.h"
#include "nssb64t.h"
#include "nssbase.h"
#include "nssbaset.h"
#include "nssckbi.h"
#include "nssilckt.h"
#include "nssilock.h"
#include "nsslocks.h"
#include "nssrwlk.h"
#include "nssrwlkt.h"
#include "ocsp.h"
#include "ocspt.h"
#include "p12.h"
#include "p12plcy.h"
#include "p12t.h"
#include "pk11func.h"
#include "pk11pqg.h"
#include "pk11priv.h"
#include "pk11pub.h"
#include "pk11sdr.h"
#include "pkcs11.h"
#include "pkcs11t.h"
#include "pkcs12.h"
#include "pkcs12t.h"
#include "pkcs7t.h"
#include "portreg.h"
#include "preenc.h"
#include "secasn1.h"
#include "secasn1t.h"
#include "seccomon.h"
#include "secder.h"
#include "secdert.h"
#include "secdig.h"
#include "secdigt.h"
#include "secerr.h"
#include "sechash.h"
#include "secitem.h"
#include "secmime.h"
#include "secmod.h"
#include "secmodt.h"
#include "secoid.h"
#include "secoidt.h"
#include "secpkcs5.h"
#include "secpkcs7.h"
#include "secport.h"
#include "shsign.h"
#include "smime.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include "sslt.h"

int main()
{
    return 0;
}
