/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "cert.h"
#include "base64.h"
#include "mcom_db.h"
#include "certdb.h"

#ifdef STATIC_CERT_INIT
static char example_com_server_ca[] =
"MIICBTCCAW6gAwIBAgIBATANBgkqhkiG9w0BAQQFADA+MREwDwYICZIm9ZgeZAET"
"A2NvbTEVMBMGCAmSJvWYHmQBEwdFeGFtcGxlMRIwEAYDVQQDEwlTZXJ2ZXIgQ0Ew"
"HhcNMDAwMjAzMjIyMDA3WhcNMTAwNTAzMjIyMDA3WjA+MREwDwYICZIm9ZgeZAET"
"A2NvbTEVMBMGCAmSJvWYHmQBEwdFeGFtcGxlMRIwEAYDVQQDEwlTZXJ2ZXIgQ0Ew"
"gZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBALGiKEvTd2k4ZJbdAVWokfFlB6Hz"
"WJXveXm8+IgmFlgtAnicZI11z5wAutFRvDpun7WmRLgHxvEhU3tLoiACGYdGJXPw"
"+lI2pzHzFSd63B0qcA/NVAW3EOBJeaEFwy0jkUaCIki8qQV06g8RosNX/zv6a+OF"
"d5NMpS0fecK4fEvdAgMBAAGjEzARMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcN"
"AQEEBQADgYEAi5rFiG6afWS1PHigssk2LwAJws5cszPbVIeIMHCBbtu259V7uWts"
"gNxUPJRjeQBsK0ItAfinC0xxLeuMbRfIdZoRYv/OYDxCwGW7hUcNLi+fHlGnJNXH"
"TWaCRdOwkljnws4v8ABas2DYA/k7xUFAygkIJd9NtE29ZrdrWpfSavI=";

static char example_com_individual_ca[] =
"MIICDTCCAXagAwIBAgIBAjANBgkqhkiG9w0BAQQFADBCMREwDwYICZIm9ZgeZAET"
"A2NvbTEVMBMGCAmSJvWYHmQBEwdFeGFtcGxlMRYwFAYDVQQDEw1JbmRpdmlkdWFs"
"IENBMB4XDTAwMDIwMzIyMjE1NFoXDTEwMDUwMzIyMjE1NFowQjERMA8GCAmSJvWY"
"HmQBEwNjb20xFTATBggJkib1mB5kARMHRXhhbXBsZTEWMBQGA1UEAxMNSW5kaXZp"
"ZHVhbCBDQTCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEAu5syfboe93MOkGec"
"dOuJholyX42wcaH/RgnL3C/8NnZp9WWaTaguvn7KrbCj4TAMzu0pabUN8apB3J60"
"9C/FlixjXF7r73OzbyTCM5ja6/bPfmHMPmDl9l/9tKqhh+loFvRizXDaWSFRViDS"
"XvKNeQztwwAOpEAqnJwyTkn4FjECAwEAAaMTMBEwDwYDVR0TAQH/BAUwAwEB/zAN"
"BgkqhkiG9w0BAQQFAAOBgQB1XK+5pXdXYq3O3TC/ZY5LWlZ7zuoWUO75OpuMY7XF"
"iW/jeXbVT5IYZXoRGXJFGGaDmnAuK1/m6FTDhjSTG0XUmd5tg4aFieI+LY4rkYEv"
"mbJElxKabXl5hVD4mg2bwYlFY7XBmifTa1Ll3HDX3VZM0DC1bm4KCHBnY0qXjSYq"
"PA==";

static char example_com_objsign_ca[] =
"MIICETCCAXqgAwIBAgIBAzANBgkqhkiG9w0BAQQFADBEMREwDwYICZIm9ZgeZAET"
"A2NvbTEVMBMGCAmSJvWYHmQBEwdFeGFtcGxlMRgwFgYDVQQDEw9Db2RlIFNpZ25p"
"bmcgQ0EwHhcNMDAwMjAzMjIyMzEzWhcNMTAwNTAzMjIyMzEzWjBEMREwDwYICZIm"
"9ZgeZAETA2NvbTEVMBMGCAmSJvWYHmQBEwdFeGFtcGxlMRgwFgYDVQQDEw9Db2Rl"
"IFNpZ25pbmcgQ0EwgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBALcy76InmpM9"
"S9K2MlNSjusx6nkYWWbx7eDRTV+xhRPeDxW4t8jtKPqDF5LTusyM9WCI/nneqsIP"
"7iTSHpxlGx37J1VbqKX5fZsfJ3wKv6ZIylzeRuFY9MFypPA2UmVd1ACDOUB3YDvY"
"mrCVkOPEhjnZKbq4FfCpf8KNL2A5EBcZAgMBAAGjEzARMA8GA1UdEwEB/wQFMAMB"
"Af8wDQYJKoZIhvcNAQEEBQADgYEAI0IXzwgBRXvow3JQi8Y4YdG2wZc4BWRGW87x"
"2zOD7GOA0CWN149vb6rEchECykDsJj9LoBl6o1aRxk9WkIFnXmMOJSuJA+ilCe//"
"81a5OhKbe0p7ym6rh190BLwh2VePFeyabq6NipfZlN6qgWUzoepf+jVblufW/2EI"
"fbMSylc=";
#endif

/* This is the cert->certKey (serial number and issuer name) of
 * the cert that we want to revoke.
 */
static unsigned char revoked_system_principal_key[] = {
0x40, 0x18, 0xf2, 0x35, 0x86, 0x06, 0x78, 0xce, 0x87, 0x89,
0x0c, 0x5d, 0x68, 0x67, 0x33, 0x09, 0x30, 0x81, 0xc1, 0x31,
0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x16,
0x56, 0x65, 0x72, 0x69, 0x53, 0x69, 0x67, 0x6e, 0x20, 0x54,
0x72, 0x75, 0x73, 0x74, 0x20, 0x4e, 0x65, 0x74, 0x77, 0x6f,
0x72, 0x6b, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55, 0x04,
0x0b, 0x13, 0x0e, 0x56, 0x65, 0x72, 0x69, 0x53, 0x69, 0x67,
0x6e, 0x2c, 0x20, 0x49, 0x6e, 0x63, 0x2e, 0x31, 0x3a, 0x30,
0x38, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x31, 0x56, 0x65,
0x72, 0x69, 0x53, 0x69, 0x67, 0x6e, 0x20, 0x4f, 0x62, 0x6a,
0x65, 0x63, 0x74, 0x20, 0x53, 0x69, 0x67, 0x6e, 0x69, 0x6e,
0x67, 0x20, 0x43, 0x41, 0x20, 0x2d, 0x20, 0x43, 0x6c, 0x61,
0x73, 0x73, 0x20, 0x33, 0x20, 0x4f, 0x72, 0x67, 0x61, 0x6e,
0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x31, 0x49, 0x30,
0x47, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x40, 0x77, 0x77,
0x77, 0x2e, 0x76, 0x65, 0x72, 0x69, 0x73, 0x69, 0x67, 0x6e,
0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x43, 0x50, 0x53, 0x20, 0x49,
0x6e, 0x63, 0x6f, 0x72, 0x70, 0x2e, 0x62, 0x79, 0x20, 0x52,
0x65, 0x66, 0x2e, 0x20, 0x4c, 0x49, 0x41, 0x42, 0x49, 0x4c,
0x49, 0x54, 0x59, 0x20, 0x4c, 0x54, 0x44, 0x2e, 0x28, 0x63,
0x29, 0x39, 0x37, 0x20, 0x56, 0x65, 0x72, 0x69, 0x53, 0x69,
0x67, 0x6e
};

SECStatus
CERT_CheckForEvilCert(CERTCertificate *cert)
{
    if ( cert->certKey.len == sizeof(revoked_system_principal_key) ) {
        if ( PORT_Memcmp(cert->certKey.data,
                         revoked_system_principal_key,
                         sizeof(revoked_system_principal_key)) == 0 ) {
            return(SECFailure);
        }
    }

    return(SECSuccess);
}

#ifdef STATIC_CERT_INIT

#define DEFAULT_TRUST_FLAGS (CERTDB_VALID_CA | \
                             CERTDB_TRUSTED_CA | \
                             CERTDB_NS_TRUSTED_CA)

typedef enum {
    certUpdateNone,
    certUpdateAdd,
    certUpdateDelete,
    certUpdateAddTrust,
    certUpdateRemoveTrust,
    certUpdateSetTrust
} certUpdateOp;

typedef struct {
    char *cert;
    char *nickname;
    CERTCertTrust trust;
    int updateVersion;
    certUpdateOp op;
    CERTCertTrust trustDelta;
} certInitEntry;

static certInitEntry initialcerts[] = {
  {
    example_com_server_ca,
    "Example.com Server CA",
    { DEFAULT_TRUST_FLAGS | CERTDB_GOVT_APPROVED_CA, 0, 0 },
    1,
    certUpdateAdd,
    { 0, 0, 0 }
  },
  {
    example_com_server_ca,
    "Example.com Server CA",
    { DEFAULT_TRUST_FLAGS | CERTDB_GOVT_APPROVED_CA, 0, 0 },
    2,
    certUpdateAddTrust,
    { CERTDB_GOVT_APPROVED_CA, 0, 0 }
  },

  {
    example_com_individual_ca,
    "Example.com Individual CA",
    { 0, DEFAULT_TRUST_FLAGS, 0 },
    1,
    certUpdateAdd,
    { 0, 0, 0 }
  },
  {
    example_com_individual_ca,
    "Example.com Individual CA",
    { 0, DEFAULT_TRUST_FLAGS, 0 },
    2,
    certUpdateRemoveTrust,
    { 0, 0, DEFAULT_TRUST_FLAGS }
  },

  {
    example_com_objsign_ca,
    "Example.com Code Signing CA",
    { 0, 0, DEFAULT_TRUST_FLAGS },
    2,
    certUpdateAdd,
    { 0, 0, 0 }
  },

  {
        0, 0
  }
};


static SECStatus
ConvertAndCheckCertificate(CERTCertDBHandle *handle, char *asciicert,
                           char *nickname, CERTCertTrust *trust)
{
    SECItem sdder;
    SECStatus rv;
    CERTCertificate *cert;
    PRBool conflict;
    SECItem derSubject;

    /* First convert ascii to binary */
    rv = ATOB_ConvertAsciiToItem (&sdder, asciicert);
    if (rv != SECSuccess) {
        return(rv);
    }

    /*
    ** Inside the ascii is a Signed Certificate.
    */

    cert = NULL;

    /* make sure that no conflicts exist */
    conflict = SEC_CertDBKeyConflict(&sdder, handle);
    if ( conflict ) {
        goto done;
    }

    rv = CERT_NameFromDERCert(&sdder, &derSubject);
    if ( rv != SECSuccess ) {
        goto loser;
    }

    conflict = SEC_CertNicknameConflict(nickname, &derSubject, handle);
    if ( conflict ) {
        goto done;
    }

    cert = CERT_NewTempCertificate(handle, &sdder, NULL, PR_FALSE, PR_TRUE);
    if ( cert == NULL ) {
        goto loser;
    }

    rv = CERT_AddTempCertToPerm(cert, nickname, trust);

    CERT_DestroyCertificate(cert);

    if (rv == SECSuccess) {
        /*
        ** XXX should verify signatures too, if we have the certificate for
        ** XXX its issuer...
        */
    }

done:
    PORT_Free(sdder.data);
    return(rv);

loser:
    return(SECFailure);
}

#endif

extern void certdb_InitDBLock(void);

SECStatus
CERT_InitCertDB(CERTCertDBHandle *handle)
{
#ifdef STATIC_CERT_INIT
    SECStatus rv;
    certInitEntry *entry;
    certdb_InitDBLock();

    entry = initialcerts;

    while ( entry->cert != NULL) {
        if ( entry->op != certUpdateDelete ) {
            rv = ConvertAndCheckCertificate(handle, entry->cert,
                                            entry->nickname, &entry->trust);
            /* keep going */
        }

        entry++;
    }
done:
    CERT_SetDBContentVersion(CERT_DB_CONTENT_VERSION, handle);
    return(rv);
#else
    certdb_InitDBLock();
    CERT_SetDBContentVersion(0, handle);
    return(SECSuccess);
#endif
}

#ifdef STATIC_CERT_INIT
static CERTCertificate *
CertFromEntry(CERTCertDBHandle *handle, char *asciicert)
{
    SECItem sdder;
    SECStatus rv;
    CERTCertificate *cert;

    /* First convert ascii to binary */
    rv = ATOB_ConvertAsciiToItem (&sdder, asciicert);
    if (rv != SECSuccess) {
        return(NULL);
    }

    /*
    ** Inside the ascii is a Signed Certificate.
    */

    cert = CERT_NewTempCertificate(handle, &sdder, NULL, PR_FALSE, PR_TRUE);

    return(cert);
}
#endif

SECStatus
CERT_AddNewCerts(CERTCertDBHandle *handle)
{
#ifdef STATIC_CERT_INIT
    int oldversion;
    int newversion;
    certInitEntry *entry;
    CERTCertTrust tmptrust;
    SECStatus rv;
    CERTCertificate *cert;

    newversion = CERT_DB_CONTENT_VERSION;

    oldversion = CERT_GetDBContentVersion(handle);

    if ( newversion > oldversion ) {
        entry = initialcerts;

        while ( entry->cert != NULL ) {
            if ( entry->updateVersion > oldversion ) {
                switch ( entry->op ) {
                  default:
                    break;
                  case certUpdateAdd:
                    rv = ConvertAndCheckCertificate(handle, entry->cert,
                                                    entry->nickname,
                                                    &entry->trust);
                    break;
                  case certUpdateDelete:
                    cert = CertFromEntry(handle, entry->cert);
                    if ( cert != NULL ) {
                        if ( cert->isperm ) {
                            rv = SEC_DeletePermCertificate(cert);
                        }
                        CERT_DestroyCertificate(cert);
                    }
                    break;
                  case certUpdateAddTrust:
                    cert = CertFromEntry(handle, entry->cert);
                    if ( cert != NULL ) {
                        if ( cert->isperm ) {
                            tmptrust = *cert->trust;
                            tmptrust.sslFlags |= entry->trustDelta.sslFlags;
                            tmptrust.emailFlags |=
                                entry->trustDelta.emailFlags;
                            tmptrust.objectSigningFlags |=
                                entry->trustDelta.objectSigningFlags;
                            rv = CERT_ChangeCertTrust(handle, cert,
&tmptrust);
                        }
                        CERT_DestroyCertificate(cert);
                    }
                    break;
                  case certUpdateRemoveTrust:
                    cert = CertFromEntry(handle, entry->cert);
                    if ( cert != NULL ) {
                        if ( cert->isperm ) {
                            tmptrust = *cert->trust;
                            tmptrust.sslFlags &=
                                (~entry->trustDelta.sslFlags);
                            tmptrust.emailFlags &=
                                (~entry->trustDelta.emailFlags);
                            tmptrust.objectSigningFlags &=
                                (~entry->trustDelta.objectSigningFlags);
                            rv = CERT_ChangeCertTrust(handle, cert,
&tmptrust);
                        }
                        CERT_DestroyCertificate(cert);
                    }
                    break;
                  case certUpdateSetTrust:
                    cert = CertFromEntry(handle, entry->cert);
                    if ( cert != NULL ) {
                        if ( cert->isperm ) {
                            tmptrust = *cert->trust;
                            tmptrust.sslFlags = entry->trustDelta.sslFlags;
                            tmptrust.emailFlags =
                                entry->trustDelta.emailFlags;
                            tmptrust.objectSigningFlags =
                                entry->trustDelta.objectSigningFlags;
                            rv = CERT_ChangeCertTrust(handle, cert,
&tmptrust);
                        }
                        CERT_DestroyCertificate(cert);
                    }
                    break;
                }
            }

            entry++;
        }

        CERT_SetDBContentVersion(newversion, handle);
    }

#endif
    return(SECSuccess);
}
