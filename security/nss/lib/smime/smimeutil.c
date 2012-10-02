/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Stuff specific to S/MIME policy and interoperability.
 *
 * $Id: smimeutil.c,v 1.25 2012/05/17 17:55:52 kaie%kuix.de Exp $
 */

#include "secmime.h"
#include "secoid.h"
#include "pk11func.h"
#include "ciferfam.h"	/* for CIPHER_FAMILY symbols */
#include "secasn1.h"
#include "secitem.h"
#include "cert.h"
#include "key.h"
#include "secerr.h"
#include "cms.h"
#include "nss.h"

SEC_ASN1_MKSUB(CERT_IssuerAndSNTemplate)
SEC_ASN1_MKSUB(SEC_OctetStringTemplate)
SEC_ASN1_CHOOSER_DECLARE(CERT_IssuerAndSNTemplate)

/* various integer's ASN.1 encoding */
static unsigned char asn1_int40[] = { SEC_ASN1_INTEGER, 0x01, 0x28 };
static unsigned char asn1_int64[] = { SEC_ASN1_INTEGER, 0x01, 0x40 };
static unsigned char asn1_int128[] = { SEC_ASN1_INTEGER, 0x02, 0x00, 0x80 };

/* RC2 algorithm parameters (used in smime_cipher_map) */
static SECItem param_int40 = { siBuffer, asn1_int40, sizeof(asn1_int40) };
static SECItem param_int64 = { siBuffer, asn1_int64, sizeof(asn1_int64) };
static SECItem param_int128 = { siBuffer, asn1_int128, sizeof(asn1_int128) };

/*
 * XXX Would like the "parameters" field to be a SECItem *, but the
 * encoder is having trouble with optional pointers to an ANY.  Maybe
 * once that is fixed, can change this back...
 */
typedef struct {
    SECItem capabilityID;
    SECItem parameters;
    long cipher;		/* optimization */
} NSSSMIMECapability;

static const SEC_ASN1Template NSSSMIMECapabilityTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(NSSSMIMECapability) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(NSSSMIMECapability,capabilityID), },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	  offsetof(NSSSMIMECapability,parameters), },
    { 0, }
};

static const SEC_ASN1Template NSSSMIMECapabilitiesTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, NSSSMIMECapabilityTemplate }
};

/*
 * NSSSMIMEEncryptionKeyPreference - if we find one of these, it needs to prompt us
 *  to store this and only this certificate permanently for the sender email address.
 */
typedef enum {
    NSSSMIMEEncryptionKeyPref_IssuerSN,
    NSSSMIMEEncryptionKeyPref_RKeyID,
    NSSSMIMEEncryptionKeyPref_SubjectKeyID
} NSSSMIMEEncryptionKeyPrefSelector;

typedef struct {
    NSSSMIMEEncryptionKeyPrefSelector selector;
    union {
	CERTIssuerAndSN			*issuerAndSN;
	NSSCMSRecipientKeyIdentifier	*recipientKeyID;
	SECItem				*subjectKeyID;
    } id;
} NSSSMIMEEncryptionKeyPreference;

extern const SEC_ASN1Template NSSCMSRecipientKeyIdentifierTemplate[];

static const SEC_ASN1Template smime_encryptionkeypref_template[] = {
    { SEC_ASN1_CHOICE,
	  offsetof(NSSSMIMEEncryptionKeyPreference,selector), NULL,
	  sizeof(NSSSMIMEEncryptionKeyPreference) },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0
          | SEC_ASN1_CONSTRUCTED,
	  offsetof(NSSSMIMEEncryptionKeyPreference,id.issuerAndSN),
	  SEC_ASN1_SUB(CERT_IssuerAndSNTemplate),
	  NSSSMIMEEncryptionKeyPref_IssuerSN },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | 1
          | SEC_ASN1_CONSTRUCTED,
	  offsetof(NSSSMIMEEncryptionKeyPreference,id.recipientKeyID),
	  NSSCMSRecipientKeyIdentifierTemplate,
	  NSSSMIMEEncryptionKeyPref_RKeyID },
    { SEC_ASN1_POINTER | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 2
          | SEC_ASN1_CONSTRUCTED,
	  offsetof(NSSSMIMEEncryptionKeyPreference,id.subjectKeyID),
	  SEC_ASN1_SUB(SEC_OctetStringTemplate),
	  NSSSMIMEEncryptionKeyPref_SubjectKeyID },
    { 0, }
};

/* smime_cipher_map - map of SMIME symmetric "ciphers" to algtag & parameters */
typedef struct {
    unsigned long cipher;
    SECOidTag algtag;
    SECItem *parms;
    PRBool enabled;	/* in the user's preferences */
    PRBool allowed;	/* per export policy */
} smime_cipher_map_entry;

/* global: list of supported SMIME symmetric ciphers, ordered roughly by increasing strength */
static smime_cipher_map_entry smime_cipher_map[] = {
/*    cipher			algtag			parms		enabled  allowed */
/*    ---------------------------------------------------------------------------------- */
    { SMIME_RC2_CBC_40,		SEC_OID_RC2_CBC,	&param_int40,	PR_TRUE, PR_TRUE },
    { SMIME_DES_CBC_56,		SEC_OID_DES_CBC,	NULL,		PR_TRUE, PR_TRUE },
    { SMIME_RC2_CBC_64,		SEC_OID_RC2_CBC,	&param_int64,	PR_TRUE, PR_TRUE },
    { SMIME_RC2_CBC_128,	SEC_OID_RC2_CBC,	&param_int128,	PR_TRUE, PR_TRUE },
    { SMIME_DES_EDE3_168,	SEC_OID_DES_EDE3_CBC,	NULL,		PR_TRUE, PR_TRUE },
    { SMIME_AES_CBC_128,	SEC_OID_AES_128_CBC,	NULL,		PR_TRUE, PR_TRUE },
    { SMIME_AES_CBC_256,	SEC_OID_AES_256_CBC,	NULL,		PR_TRUE, PR_TRUE }
};
static const int smime_cipher_map_count = sizeof(smime_cipher_map) / sizeof(smime_cipher_map_entry);

/*
 * smime_mapi_by_cipher - find index into smime_cipher_map by cipher
 */
static int
smime_mapi_by_cipher(unsigned long cipher)
{
    int i;

    for (i = 0; i < smime_cipher_map_count; i++) {
	if (smime_cipher_map[i].cipher == cipher)
	    return i;	/* bingo */
    }
    return -1;		/* should not happen if we're consistent, right? */
}

/*
 * NSS_SMIME_EnableCipher - this function locally records the user's preference
 */
SECStatus 
NSS_SMIMEUtil_EnableCipher(unsigned long which, PRBool on)
{
    unsigned long mask;
    int mapi;

    mask = which & CIPHER_FAMILYID_MASK;

    PORT_Assert (mask == CIPHER_FAMILYID_SMIME);
    if (mask != CIPHER_FAMILYID_SMIME)
	/* XXX set an error! */
    	return SECFailure;

    mapi = smime_mapi_by_cipher(which);
    if (mapi < 0)
	/* XXX set an error */
	return SECFailure;

    /* do we try to turn on a forbidden cipher? */
    if (!smime_cipher_map[mapi].allowed && on) {
	PORT_SetError (SEC_ERROR_BAD_EXPORT_ALGORITHM);
	return SECFailure;
    }

    if (smime_cipher_map[mapi].enabled != on)
	smime_cipher_map[mapi].enabled = on;

    return SECSuccess;
}


/*
 * this function locally records the export policy
 */
SECStatus 
NSS_SMIMEUtil_AllowCipher(unsigned long which, PRBool on)
{
    unsigned long mask;
    int mapi;

    mask = which & CIPHER_FAMILYID_MASK;

    PORT_Assert (mask == CIPHER_FAMILYID_SMIME);
    if (mask != CIPHER_FAMILYID_SMIME)
	/* XXX set an error! */
    	return SECFailure;

    mapi = smime_mapi_by_cipher(which);
    if (mapi < 0)
	/* XXX set an error */
	return SECFailure;

    if (smime_cipher_map[mapi].allowed != on)
	smime_cipher_map[mapi].allowed = on;

    return SECSuccess;
}

/*
 * Based on the given algorithm (including its parameters, in some cases!)
 * and the given key (may or may not be inspected, depending on the
 * algorithm), find the appropriate policy algorithm specification
 * and return it.  If no match can be made, -1 is returned.
 */
static SECStatus
nss_smime_get_cipher_for_alg_and_key(SECAlgorithmID *algid, PK11SymKey *key, unsigned long *cipher)
{
    SECOidTag algtag;
    unsigned int keylen_bits;
    unsigned long c;

    algtag = SECOID_GetAlgorithmTag(algid);
    switch (algtag) {
    case SEC_OID_RC2_CBC:
	keylen_bits = PK11_GetKeyStrength(key, algid);
	switch (keylen_bits) {
	case 40:
	    c = SMIME_RC2_CBC_40;
	    break;
	case 64:
	    c = SMIME_RC2_CBC_64;
	    break;
	case 128:
	    c = SMIME_RC2_CBC_128;
	    break;
	default:
	    return SECFailure;
	}
	break;
    case SEC_OID_DES_CBC:
	c = SMIME_DES_CBC_56;
	break;
    case SEC_OID_DES_EDE3_CBC:
	c = SMIME_DES_EDE3_168;
	break;
    case SEC_OID_AES_128_CBC:
	c = SMIME_AES_CBC_128;
	break;
    case SEC_OID_AES_256_CBC:
	c = SMIME_AES_CBC_256;
	break;
    default:
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
	return SECFailure;
    }
    *cipher = c;
    return SECSuccess;
}

static PRBool
nss_smime_cipher_allowed(unsigned long which)
{
    int mapi;

    mapi = smime_mapi_by_cipher(which);
    if (mapi < 0)
	return PR_FALSE;
    return smime_cipher_map[mapi].allowed;
}

PRBool
NSS_SMIMEUtil_DecryptionAllowed(SECAlgorithmID *algid, PK11SymKey *key)
{
    unsigned long which;

    if (nss_smime_get_cipher_for_alg_and_key(algid, key, &which) != SECSuccess)
	return PR_FALSE;

    return nss_smime_cipher_allowed(which);
}


/*
 * NSS_SMIME_EncryptionPossible - check if any encryption is allowed
 *
 * This tells whether or not *any* S/MIME encryption can be done,
 * according to policy.  Callers may use this to do nicer user interface
 * (say, greying out a checkbox so a user does not even try to encrypt
 * a message when they are not allowed to) or for any reason they want
 * to check whether S/MIME encryption (or decryption, for that matter)
 * may be done.
 *
 * It takes no arguments.  The return value is a simple boolean:
 *   PR_TRUE means encryption (or decryption) is *possible*
 *	(but may still fail due to other reasons, like because we cannot
 *	find all the necessary certs, etc.; PR_TRUE is *not* a guarantee)
 *   PR_FALSE means encryption (or decryption) is not permitted
 *
 * There are no errors from this routine.
 */
PRBool
NSS_SMIMEUtil_EncryptionPossible(void)
{
    int i;

    for (i = 0; i < smime_cipher_map_count; i++) {
	if (smime_cipher_map[i].allowed)
	    return PR_TRUE;
    }
    return PR_FALSE;
}


static int
nss_SMIME_FindCipherForSMIMECap(NSSSMIMECapability *cap)
{
    int i;
    SECOidTag capIDTag;

    /* we need the OIDTag here */
    capIDTag = SECOID_FindOIDTag(&(cap->capabilityID));

    /* go over all the SMIME ciphers we know and see if we find a match */
    for (i = 0; i < smime_cipher_map_count; i++) {
	if (smime_cipher_map[i].algtag != capIDTag)
	    continue;
	/*
	 * XXX If SECITEM_CompareItem allowed NULLs as arguments (comparing
	 * 2 NULLs as equal and NULL and non-NULL as not equal), we could
	 * use that here instead of all of the following comparison code.
	 */
	if (!smime_cipher_map[i].parms) { 
	    if (!cap->parameters.data || !cap->parameters.len)
		break;	/* both empty: bingo */
	    if (cap->parameters.len     == 2  &&
	        cap->parameters.data[0] == SEC_ASN1_NULL &&
		cap->parameters.data[1] == 0) 
		break;  /* DER NULL == NULL, bingo */
	} else if (cap->parameters.data != NULL && 
	    cap->parameters.len == smime_cipher_map[i].parms->len &&
	    PORT_Memcmp (cap->parameters.data, smime_cipher_map[i].parms->data,
			     cap->parameters.len) == 0)
	{
	    break;	/* both not empty, same length & equal content: bingo */
	}
    }

    if (i == smime_cipher_map_count)
	return 0;				/* no match found */
    return smime_cipher_map[i].cipher;	/* match found, point to cipher */
}

/*
 * smime_choose_cipher - choose a cipher that works for all the recipients
 *
 * "scert"  - sender's certificate
 * "rcerts" - recipient's certificates
 */
static long
smime_choose_cipher(CERTCertificate *scert, CERTCertificate **rcerts)
{
    PRArenaPool *poolp;
    long cipher;
    long chosen_cipher;
    int *cipher_abilities;
    int *cipher_votes;
    int weak_mapi;
    int strong_mapi;
    int aes128_mapi;
    int aes256_mapi;
    int rcount, mapi, max, i;

    chosen_cipher = SMIME_RC2_CBC_40;		/* the default, LCD */
    weak_mapi = smime_mapi_by_cipher(chosen_cipher);
    aes128_mapi = smime_mapi_by_cipher(SMIME_AES_CBC_128);
    aes256_mapi = smime_mapi_by_cipher(SMIME_AES_CBC_256);

    poolp = PORT_NewArena (1024);		/* XXX what is right value? */
    if (poolp == NULL)
	goto done;

    cipher_abilities = (int *)PORT_ArenaZAlloc(poolp, smime_cipher_map_count * sizeof(int));
    cipher_votes     = (int *)PORT_ArenaZAlloc(poolp, smime_cipher_map_count * sizeof(int));
    if (cipher_votes == NULL || cipher_abilities == NULL)
	goto done;

    /* Make triple-DES the strong cipher. */
    strong_mapi = smime_mapi_by_cipher (SMIME_DES_EDE3_168);

    /* walk all the recipient's certs */
    for (rcount = 0; rcerts[rcount] != NULL; rcount++) {
	SECItem *profile;
	NSSSMIMECapability **caps;
	int pref;

	/* the first cipher that matches in the user's SMIME profile gets
	 * "smime_cipher_map_count" votes; the next one gets "smime_cipher_map_count" - 1
	 * and so on. If every cipher matches, the last one gets 1 (one) vote */
	pref = smime_cipher_map_count;

	/* find recipient's SMIME profile */
	profile = CERT_FindSMimeProfile(rcerts[rcount]);

	if (profile != NULL && profile->data != NULL && profile->len > 0) {
	    /* we have a profile (still DER-encoded) */
	    caps = NULL;
	    /* decode it */
	    if (SEC_QuickDERDecodeItem(poolp, &caps,
                    NSSSMIMECapabilitiesTemplate, profile) == SECSuccess &&
		    caps != NULL)
	    {
		/* walk the SMIME capabilities for this recipient */
		for (i = 0; caps[i] != NULL; i++) {
		    cipher = nss_SMIME_FindCipherForSMIMECap(caps[i]);
		    mapi = smime_mapi_by_cipher(cipher);
		    if (mapi >= 0) {
			/* found the cipher */
			cipher_abilities[mapi]++;
			cipher_votes[mapi] += pref;
			--pref;
		    }
		}
	    }
	} else {
	    /* no profile found - so we can only assume that the user can do
	     * the mandatory algorithms which are RC2-40 (weak crypto) and
	     * 3DES (strong crypto), unless the user has an elliptic curve
	     * key.  For elliptic curve keys, RFC 5753 mandates support
	     * for AES 128 CBC. */
	    SECKEYPublicKey *key;
	    unsigned int pklen_bits;
	    KeyType key_type;

	    /*
	     * if recipient's public key length is > 512, vote for a strong cipher
	     * please not that the side effect of this is that if only one recipient
	     * has an export-level public key, the strong cipher is disabled.
	     *
	     * XXX This is probably only good for RSA keys.  What I would
	     * really like is a function to just say;  Is the public key in
	     * this cert an export-length key?  Then I would not have to
	     * know things like the value 512, or the kind of key, or what
	     * a subjectPublicKeyInfo is, etc.
	     */
	    key = CERT_ExtractPublicKey(rcerts[rcount]);
	    pklen_bits = 0;
	    if (key != NULL) {
		pklen_bits = SECKEY_PublicKeyStrengthInBits (key);
		key_type = SECKEY_GetPublicKeyType(key);
		SECKEY_DestroyPublicKey (key);
	    }

	    if (key_type == ecKey) {
		/* While RFC 5753 mandates support for AES-128 CBC, should use
		 * AES 256 if user's key provides more than 128 bits of
		 * security strength so that symmetric key is not weak link. */

		/* RC2-40 is not compatible with elliptic curve keys. */
		chosen_cipher = SMIME_DES_EDE3_168;
		if (pklen_bits > 256) {
		    cipher_abilities[aes256_mapi]++;
		    cipher_votes[aes256_mapi] += pref;
		    pref--;
		}
		cipher_abilities[aes128_mapi]++;
		cipher_votes[aes128_mapi] += pref;
		pref--;
		cipher_abilities[strong_mapi]++;
		cipher_votes[strong_mapi] += pref;
		pref--;
	    } else {
		if (pklen_bits > 512) {
		    /* cast votes for the strong algorithm */
		    cipher_abilities[strong_mapi]++;
		    cipher_votes[strong_mapi] += pref;
		    pref--;
		}

		/* always cast (possibly less) votes for the weak algorithm */
		cipher_abilities[weak_mapi]++;
		cipher_votes[weak_mapi] += pref;
	    } 
	}
	if (profile != NULL)
	    SECITEM_FreeItem(profile, PR_TRUE);
    }

    /* find cipher that is agreeable by all recipients and that has the most votes */
    max = 0;
    for (mapi = 0; mapi < smime_cipher_map_count; mapi++) {
	/* if not all of the recipients can do this, forget it */
	if (cipher_abilities[mapi] != rcount)
	    continue;
	/* if cipher is not enabled or not allowed by policy, forget it */
	if (!smime_cipher_map[mapi].enabled || !smime_cipher_map[mapi].allowed)
	    continue;
	/* now see if this one has more votes than the last best one */
	if (cipher_votes[mapi] >= max) {
	    /* if equal number of votes, prefer the ones further down in the list */
	    /* with the expectation that these are higher rated ciphers */
	    chosen_cipher = smime_cipher_map[mapi].cipher;
	    max = cipher_votes[mapi];
	}
    }
    /* if no common cipher was found, chosen_cipher stays at the default */

done:
    if (poolp != NULL)
	PORT_FreeArena (poolp, PR_FALSE);

    return chosen_cipher;
}

/*
 * XXX This is a hack for now to satisfy our current interface.
 * Eventually, with more parameters needing to be specified, just
 * looking up the keysize is not going to be sufficient.
 */
static int
smime_keysize_by_cipher (unsigned long which)
{
    int keysize;

    switch (which) {
      case SMIME_RC2_CBC_40:
	keysize = 40;
	break;
      case SMIME_RC2_CBC_64:
	keysize = 64;
	break;
      case SMIME_RC2_CBC_128:
      case SMIME_AES_CBC_128:
	keysize = 128;
	break;
      case SMIME_AES_CBC_256:
	keysize = 256;
	break;
      case SMIME_DES_CBC_56:
      case SMIME_DES_EDE3_168:
	/*
	 * These are special; since the key size is fixed, we actually
	 * want to *avoid* specifying a key size.
	 */
	keysize = 0;
	break;
      default:
	keysize = -1;
	break;
    }

    return keysize;
}

/*
 * NSS_SMIMEUtil_FindBulkAlgForRecipients - find bulk algorithm suitable for all recipients
 *
 * it would be great for UI purposes if there would be a way to find out which recipients
 * prevented a strong cipher from being used...
 */
SECStatus
NSS_SMIMEUtil_FindBulkAlgForRecipients(CERTCertificate **rcerts, SECOidTag *bulkalgtag, int *keysize)
{
    unsigned long cipher;
    int mapi;

    cipher = smime_choose_cipher(NULL, rcerts);
    mapi = smime_mapi_by_cipher(cipher);

    *bulkalgtag = smime_cipher_map[mapi].algtag;
    *keysize = smime_keysize_by_cipher(smime_cipher_map[mapi].cipher);

    return SECSuccess;
}

/*
 * NSS_SMIMEUtil_CreateSMIMECapabilities - get S/MIME capabilities for this instance of NSS
 *
 * scans the list of allowed and enabled ciphers and construct a PKCS9-compliant
 * S/MIME capabilities attribute value.
 *
 * XXX Please note that, in contradiction to RFC2633 2.5.2, the capabilities only include
 * symmetric ciphers, NO signature algorithms or key encipherment algorithms.
 *
 * "poolp" - arena pool to create the S/MIME capabilities data on
 * "dest" - SECItem to put the data in
 */
SECStatus
NSS_SMIMEUtil_CreateSMIMECapabilities(PLArenaPool *poolp, SECItem *dest)
{
    NSSSMIMECapability *cap;
    NSSSMIMECapability **smime_capabilities;
    smime_cipher_map_entry *map;
    SECOidData *oiddata;
    SECItem *dummy;
    int i, capIndex;

    /* if we have an old NSSSMIMECapability array, we'll reuse it (has the right size) */
    /* smime_cipher_map_count + 1 is an upper bound - we might end up with less */
    smime_capabilities = (NSSSMIMECapability **)PORT_ZAlloc((smime_cipher_map_count + 1)
				      * sizeof(NSSSMIMECapability *));
    if (smime_capabilities == NULL)
	return SECFailure;

    capIndex = 0;

    /* Add all the symmetric ciphers
     * We walk the cipher list backwards, as it is ordered by increasing strength,
     * we prefer the stronger cipher over a weaker one, and we have to list the
     * preferred algorithm first */
    for (i = smime_cipher_map_count - 1; i >= 0; i--) {
	/* Find the corresponding entry in the cipher map. */
	map = &(smime_cipher_map[i]);
	if (!map->enabled)
	    continue;

	/* get next SMIME capability */
	cap = (NSSSMIMECapability *)PORT_ZAlloc(sizeof(NSSSMIMECapability));
	if (cap == NULL)
	    break;
	smime_capabilities[capIndex++] = cap;

	oiddata = SECOID_FindOIDByTag(map->algtag);
	if (oiddata == NULL)
	    break;

	cap->capabilityID.data = oiddata->oid.data;
	cap->capabilityID.len = oiddata->oid.len;
	cap->parameters.data = map->parms ? map->parms->data : NULL;
	cap->parameters.len = map->parms ? map->parms->len : 0;
	cap->cipher = smime_cipher_map[i].cipher;
    }

    /* XXX add signature algorithms */
    /* XXX add key encipherment algorithms */

    smime_capabilities[capIndex] = NULL;	/* last one - now encode */
    dummy = SEC_ASN1EncodeItem(poolp, dest, &smime_capabilities, NSSSMIMECapabilitiesTemplate);

    /* now that we have the proper encoded SMIMECapabilities (or not),
     * free the work data */
    for (i = 0; smime_capabilities[i] != NULL; i++)
	PORT_Free(smime_capabilities[i]);
    PORT_Free(smime_capabilities);

    return (dummy == NULL) ? SECFailure : SECSuccess;
}

/*
 * NSS_SMIMEUtil_CreateSMIMEEncKeyPrefs - create S/MIME encryption key preferences attr value
 *
 * "poolp" - arena pool to create the attr value on
 * "dest" - SECItem to put the data in
 * "cert" - certificate that should be marked as preferred encryption key
 *          cert is expected to have been verified for EmailRecipient usage.
 */
SECStatus
NSS_SMIMEUtil_CreateSMIMEEncKeyPrefs(PLArenaPool *poolp, SECItem *dest, CERTCertificate *cert)
{
    NSSSMIMEEncryptionKeyPreference ekp;
    SECItem *dummy = NULL;
    PLArenaPool *tmppoolp = NULL;

    if (cert == NULL)
	goto loser;

    tmppoolp = PORT_NewArena(1024);
    if (tmppoolp == NULL)
	goto loser;

    /* XXX hardcoded IssuerSN choice for now */
    ekp.selector = NSSSMIMEEncryptionKeyPref_IssuerSN;
    ekp.id.issuerAndSN = CERT_GetCertIssuerAndSN(tmppoolp, cert);
    if (ekp.id.issuerAndSN == NULL)
	goto loser;

    dummy = SEC_ASN1EncodeItem(poolp, dest, &ekp, smime_encryptionkeypref_template);

loser:
    if (tmppoolp) PORT_FreeArena(tmppoolp, PR_FALSE);

    return (dummy == NULL) ? SECFailure : SECSuccess;
}

/*
 * NSS_SMIMEUtil_CreateSMIMEEncKeyPrefs - create S/MIME encryption key preferences attr value using MS oid
 *
 * "poolp" - arena pool to create the attr value on
 * "dest" - SECItem to put the data in
 * "cert" - certificate that should be marked as preferred encryption key
 *          cert is expected to have been verified for EmailRecipient usage.
 */
SECStatus
NSS_SMIMEUtil_CreateMSSMIMEEncKeyPrefs(PLArenaPool *poolp, SECItem *dest, CERTCertificate *cert)
{
    SECItem *dummy = NULL;
    PLArenaPool *tmppoolp = NULL;
    CERTIssuerAndSN *isn;

    if (cert == NULL)
	goto loser;

    tmppoolp = PORT_NewArena(1024);
    if (tmppoolp == NULL)
	goto loser;

    isn = CERT_GetCertIssuerAndSN(tmppoolp, cert);
    if (isn == NULL)
	goto loser;

    dummy = SEC_ASN1EncodeItem(poolp, dest, isn, SEC_ASN1_GET(CERT_IssuerAndSNTemplate));

loser:
    if (tmppoolp) PORT_FreeArena(tmppoolp, PR_FALSE);

    return (dummy == NULL) ? SECFailure : SECSuccess;
}

/*
 * NSS_SMIMEUtil_GetCertFromEncryptionKeyPreference -
 *				find cert marked by EncryptionKeyPreference attribute
 *
 * "certdb" - handle for the cert database to look in
 * "DERekp" - DER-encoded value of S/MIME Encryption Key Preference attribute
 *
 * if certificate is supposed to be found among the message's included certificates,
 * they are assumed to have been imported already.
 */
CERTCertificate *
NSS_SMIMEUtil_GetCertFromEncryptionKeyPreference(CERTCertDBHandle *certdb, SECItem *DERekp)
{
    PLArenaPool *tmppoolp = NULL;
    CERTCertificate *cert = NULL;
    NSSSMIMEEncryptionKeyPreference ekp;

    tmppoolp = PORT_NewArena(1024);
    if (tmppoolp == NULL)
	return NULL;

    /* decode DERekp */
    if (SEC_QuickDERDecodeItem(tmppoolp, &ekp, smime_encryptionkeypref_template,
                               DERekp) != SECSuccess)
	goto loser;

    /* find cert */
    switch (ekp.selector) {
    case NSSSMIMEEncryptionKeyPref_IssuerSN:
	cert = CERT_FindCertByIssuerAndSN(certdb, ekp.id.issuerAndSN);
	break;
    case NSSSMIMEEncryptionKeyPref_RKeyID:
    case NSSSMIMEEncryptionKeyPref_SubjectKeyID:
	/* XXX not supported yet - we need to be able to look up certs by SubjectKeyID */
	break;
    default:
	PORT_Assert(0);
    }
loser:
    if (tmppoolp) PORT_FreeArena(tmppoolp, PR_FALSE);

    return cert;
}

extern const char __nss_smime_rcsid[];
extern const char __nss_smime_sccsid[];

PRBool
NSSSMIME_VersionCheck(const char *importedVersion)
{
    /*
     * This is the secret handshake algorithm.
     *
     * This release has a simple version compatibility
     * check algorithm.  This release is not backward
     * compatible with previous major releases.  It is
     * not compatible with future major, minor, or
     * patch releases.
     */
    volatile char c; /* force a reference that won't get optimized away */

    c = __nss_smime_rcsid[0] + __nss_smime_sccsid[0]; 

    return NSS_VersionCheck(importedVersion);
}

const char *
NSSSMIME_GetVersion(void)
{
    return NSS_VERSION;
}
