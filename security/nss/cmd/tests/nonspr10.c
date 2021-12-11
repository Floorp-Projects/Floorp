/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "keyhi.h"
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

int
main()
{
    return 0;
}
