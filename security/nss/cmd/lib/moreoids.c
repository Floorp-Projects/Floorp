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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "secoid.h"
#include "secmodt.h" /* for CKM_INVALID_MECHANISM */

#define OI(x) { siDEROID, (unsigned char *)x, sizeof x }
#define OD(oid,tag,desc,mech,ext) { OI(oid), tag, desc, mech, ext }
#define ODN(oid,desc) \
  { OI(oid), 0, desc, CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION }

#define OIDT static const unsigned char

/* OIW Security Special Interest Group defined algorithms. */
#define OIWSSIG   0x2B, 13, 3, 2

OIDT  oiwMD5RSA[] 	= { OIWSSIG,  3 };
OIDT  oiwDESCBC[] 	= { OIWSSIG,  7 };
OIDT  oiwRSAsig[] 	= { OIWSSIG, 11 };
OIDT  oiwDSA   [] 	= { OIWSSIG, 12 };
OIDT  oiwMD5RSAsig[] 	= { OIWSSIG, 25 };
OIDT  oiwSHA1  [] 	= { OIWSSIG, 26 };
OIDT  oiwDSASHA1[] 	= { OIWSSIG, 27 };
OIDT  oiwDSASHA1param[] = { OIWSSIG, 28 };
OIDT  oiwSHA1RSA[] 	= { OIWSSIG, 29 };


/* Microsoft OIDs.  (1 3 6 1 4 1 311 ... )   */
#define MICROSOFT 0x2B, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37

OIDT  mCTL[] 	= { MICROSOFT, 10, 3, 1 }; /* Cert Trust List signing */
OIDT  mTSS[] 	= { MICROSOFT, 10, 3, 2 }; /* Time Stamp Signing */
OIDT  mSGC[] 	= { MICROSOFT, 10, 3, 3 }; /* Server gated cryptography */
OIDT  mEFS[]	= { MICROSOFT, 10, 3, 4 }; /* Encrypted File System */
OIDT  mSMIME[]	= { MICROSOFT, 16, 4    }; /* SMIME encryption key prefs */

OIDT  mECRTT[]	= { MICROSOFT, 20, 2    }; /* Enrollment cert type xtn */
OIDT  mEAGNT[]	= { MICROSOFT, 20, 2, 1 }; /* Enrollment Agent         */
OIDT  mKPSCL[]	= { MICROSOFT, 20, 2, 2 }; /* KP SmartCard Logon       */
OIDT  mNTPN []	= { MICROSOFT, 20, 2, 3 }; /* NT Principal Name        */
OIDT  mCASRV[]	= { MICROSOFT, 21, 1    }; /* CertServ CA version      */

/* AOL OIDs     (1 3 6 1 4 1 1066 ... )   */
#define AOL 0x2B, 0x06, 0x01, 0x04, 0x01, 0x88, 0x2A

/* PKIX IDs     (1 3 6 1 5 5 7 ...)  */
#define ID_PKIX 0x2B, 6, 1, 5, 5, 7
/* PKIX Access Descriptors (methods for Authority Info Access Extns) */
#define ID_AD   ID_PKIX, 48

OIDT  padOCSP[]      = { ID_AD, 1 };  /* OCSP method */
OIDT  padCAissuer[]  = { ID_AD, 2 };  /* URI (for CRL ?) */
OIDT  padTimeStamp[] = { ID_AD, 3 };  /* time stamping */

/* ISO Cert Extension type OIDs (id-ce)  (2 5 29 ...) */
#define X500                    0x55
#define X520_ATTRIBUTE_TYPE     X500, 0x04
#define X500_ALG                X500, 0x08
#define X500_ALG_ENCRYPTION     X500_ALG, 0x01
#define ID_CE			X500, 29

OIDT cePlcyObs[] = { ID_CE,  3 };  /* Cert policies, obsolete. */
OIDT cePlcyCns[] = { ID_CE, 36 };  /* Cert policy constraints. */

/* US Company arc (2 16 840 1 ...) */
#define USCOM        0x60, 0x86, 0x48, 0x01
#define USGOV        USCOM, 0x65
#define USDOD        USGOV, 2
#define ID_INFOSEC   USDOD, 1

/* Verisign PKI OIDs (2 16 840 1 113733 1 ...) */
#define VERISIGN_PKI USCOM, 0x86, 0xf8, 0x45, 1
#define VERISIGN_XTN VERISIGN_PKI, 6
#define VERISIGN_POL VERISIGN_PKI, 7	/* Cert policies */
#define VERISIGN_TNET VERISIGN_POL, 23	/* Verisign Trust Network */

OIDT  vcx7[]	= { VERISIGN_XTN, 7 };	/* Cert Extension 7 (?) */
OIDT  vcp1[]	= { VERISIGN_TNET, 1 };	/* class 1 cert policy */
OIDT  vcp2[]	= { VERISIGN_TNET, 2 };	/* class 2 cert policy */
OIDT  vcp3[]	= { VERISIGN_TNET, 3 };	/* class 3 cert policy */
OIDT  vcp4[]	= { VERISIGN_TNET, 4 };	/* class 4 cert policy */


/* ------------------------------------------------------------------- */
static const SECOidData oids[] = {
/* OIW Security Special Interest Group OIDs */
    ODN( oiwMD5RSA,	  "OIWSecSIG MD5 with RSA"),
    ODN( oiwDESCBC,	  "OIWSecSIG DES CBC"),
    ODN( oiwRSAsig,	  "OIWSecSIG RSA signature"),
    ODN( oiwDSA   ,	  "OIWSecSIG DSA"),
    ODN( oiwMD5RSAsig,	  "OIWSecSIG MD5 with RSA signature"),
    ODN( oiwSHA1  ,	  "OIWSecSIG SHA1"),
    ODN( oiwDSASHA1,	  "OIWSecSIG DSA with SHA1"),
    ODN( oiwDSASHA1param, "OIWSecSIG DSA with SHA1 with params"),
    ODN( oiwSHA1RSA,	  "OIWSecSIG MD5 with RSA"),

/* Microsoft OIDs */
    ODN( mCTL,   "Microsoft Cert Trust List signing"), 
    ODN( mTSS,   "Microsoft Time Stamp signing"),
    ODN( mSGC,   "Microsoft SGC SSL server"),
    ODN( mEFS,   "Microsoft Encrypted File System"),
    ODN( mSMIME, "Microsoft SMIME preferences"),
    ODN( mECRTT, "Microsoft Enrollment Cert Type Extension"),
    ODN( mEAGNT, "Microsoft Enrollment Agent"),
    ODN( mKPSCL, "Microsoft KP SmartCard Logon"),
    ODN( mNTPN,  "Microsoft NT Principal Name"),
    ODN( mCASRV, "Microsoft CertServ CA version"),

/* PKIX OIDs */
    ODN( padOCSP,	"PKIX OCSP method"),
    ODN( padCAissuer,	"PKIX CA Issuer method"),
    ODN( padTimeStamp,	"PKIX Time Stamping method"),

/* ID_CE OIDs. */
    ODN( cePlcyObs,	"Certificate Policies (Obsolete)"),
    ODN( cePlcyCns,	"Certificate Policy Constraints"),

/* Verisign OIDs. */
    ODN( vcx7,		"Verisign Cert Extension 7 (?)"),
    ODN( vcp1,		"Verisign Class 1 Certificate Policy"),
    ODN( vcp2,		"Verisign Class 2 Certificate Policy"),
    ODN( vcp3,		"Verisign Class 3 Certificate Policy"),
    ODN( vcp4,		"Verisign Class 4 Certificate Policy"),

};

static const unsigned int numOids = (sizeof oids) / (sizeof oids[0]);

SECStatus
SECU_RegisterDynamicOids(void)
{
    unsigned int i;
    SECStatus rv = SECSuccess;

    for (i = 0; i < numOids; ++i) {
	SECOidTag tag = SECOID_AddEntry(&oids[i]);
	if (tag == SEC_OID_UNKNOWN) {
	    rv = SECFailure;
#ifdef DEBUG_DYN_OIDS
	    fprintf(stderr, "Add OID[%d] failed\n", i);
	} else {
	    fprintf(stderr, "Add OID[%d] returned tag %d\n", i, tag);
#endif
	}
    }
    return rv;
}
