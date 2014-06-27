/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CERTUTIL_H
#define	_CERTUTIL_H

extern SECKEYPrivateKey *
CERTUTIL_GeneratePrivateKey(KeyType keytype,
                            PK11SlotInfo *slot, 
                            int rsasize,
                            int publicExponent,
                            char *noise,
                            SECKEYPublicKey **pubkeyp,
                            char *pqgFile,
                            PK11AttrFlags attrFlags,
                            CK_FLAGS opFlagsOn,
                            CK_FLAGS opFlagsOff,
                            secuPWData *pwdata);

extern char *progName;

enum certutilExtns {
    ext_keyUsage = 0,
    ext_basicConstraint,
    ext_authorityKeyID,
    ext_CRLDistPts,
    ext_NSCertType,
    ext_extKeyUsage,
    ext_authInfoAcc,
    ext_subjInfoAcc,
    ext_certPolicies,
    ext_policyMappings,
    ext_policyConstr,
    ext_inhibitAnyPolicy,
    ext_subjectKeyID,
    ext_nameConstraints,
    ext_subjectAltName,
    ext_End
};

typedef struct ExtensionEntryStr {
    PRBool activated;
    const char  *arg;
} ExtensionEntry;

typedef ExtensionEntry certutilExtnList[ext_End];

extern SECStatus
AddExtensions(void *extHandle, const char *emailAddrs, const char *dnsNames,
              certutilExtnList extList, const char *extGeneric);

extern SECStatus
GetOidFromString(PLArenaPool *arena, SECItem *to,
                 const char *from, size_t fromLen);

#endif	/* _CERTUTIL_H */

