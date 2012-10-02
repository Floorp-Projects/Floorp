/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Stuff specific to S/MIME policy and interoperability.
 * Depends on PKCS7, but there should be no dependency the other way around.
 *
 * $Id: secmime.c,v 1.6 2012/04/25 14:50:06 gerv%gerv.net Exp $
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

typedef struct smime_cipher_map_struct {
    unsigned long cipher;
    SECOidTag algtag;
    SECItem *parms;
} smime_cipher_map;

/*
 * These are macros because I think some subsequent parameters,
 * like those for RC5, will want to use them, too, separately.
 */
#define SMIME_DER_INTVAL_16	SEC_ASN1_INTEGER, 0x01, 0x10
#define SMIME_DER_INTVAL_40	SEC_ASN1_INTEGER, 0x01, 0x28
#define SMIME_DER_INTVAL_64	SEC_ASN1_INTEGER, 0x01, 0x40
#define SMIME_DER_INTVAL_128	SEC_ASN1_INTEGER, 0x02, 0x00, 0x80

#ifdef SMIME_DOES_RC5	/* will be needed; quiet unused warning for now */
static unsigned char smime_int16[] = { SMIME_DER_INTVAL_16 };
#endif
static unsigned char smime_int40[] = { SMIME_DER_INTVAL_40 };
static unsigned char smime_int64[] = { SMIME_DER_INTVAL_64 };
static unsigned char smime_int128[] = { SMIME_DER_INTVAL_128 };

static SECItem smime_rc2p40 = { siBuffer, smime_int40, sizeof(smime_int40) };
static SECItem smime_rc2p64 = { siBuffer, smime_int64, sizeof(smime_int64) };
static SECItem smime_rc2p128 = { siBuffer, smime_int128, sizeof(smime_int128) };

static smime_cipher_map smime_cipher_maps[] = {
    { SMIME_RC2_CBC_40,		SEC_OID_RC2_CBC,	&smime_rc2p40 },
    { SMIME_RC2_CBC_64,		SEC_OID_RC2_CBC,	&smime_rc2p64 },
    { SMIME_RC2_CBC_128,	SEC_OID_RC2_CBC,	&smime_rc2p128 },
#ifdef SMIME_DOES_RC5
    { SMIME_RC5PAD_64_16_40,	SEC_OID_RC5_CBC_PAD,	&smime_rc5p40 },
    { SMIME_RC5PAD_64_16_64,	SEC_OID_RC5_CBC_PAD,	&smime_rc5p64 },
    { SMIME_RC5PAD_64_16_128,	SEC_OID_RC5_CBC_PAD,	&smime_rc5p128 },
#endif
    { SMIME_DES_CBC_56,		SEC_OID_DES_CBC,	NULL },
    { SMIME_DES_EDE3_168,	SEC_OID_DES_EDE3_CBC,	NULL }
};

/*
 * Note, the following value really just needs to be an upper bound
 * on the ciphers.
 */
static const int smime_symmetric_count = sizeof(smime_cipher_maps)
					 / sizeof(smime_cipher_map);

static unsigned long *smime_prefs, *smime_newprefs;
static int smime_current_pref_index = 0;
static PRBool smime_prefs_complete = PR_FALSE;
static PRBool smime_prefs_changed = PR_TRUE;

static unsigned long smime_policy_bits = 0;


static int
smime_mapi_by_cipher (unsigned long cipher)
{
    int i;

    for (i = 0; i < smime_symmetric_count; i++) {
	if (smime_cipher_maps[i].cipher == cipher)
	    break;
    }

    if (i == smime_symmetric_count)
	return -1;

    return i;
}


/*
 * this function locally records the user's preference
 */
SECStatus 
SECMIME_EnableCipher(long which, int on)
{
    unsigned long mask;

    if (smime_newprefs == NULL || smime_prefs_complete) {
	/*
	 * This is either the very first time, or we are starting over.
	 */
	smime_newprefs = (unsigned long*)PORT_ZAlloc (smime_symmetric_count
				      * sizeof(*smime_newprefs));
	if (smime_newprefs == NULL)
	    return SECFailure;
	smime_current_pref_index = 0;
	smime_prefs_complete = PR_FALSE;
    }

    mask = which & CIPHER_FAMILYID_MASK;
    if (mask == CIPHER_FAMILYID_MASK) {
    	/*
	 * This call signifies that all preferences have been set.
	 * Move "newprefs" over, after checking first whether or
	 * not the new ones are different from the old ones.
	 */
	if (smime_prefs != NULL) {
	    if (PORT_Memcmp (smime_prefs, smime_newprefs,
			     smime_symmetric_count * sizeof(*smime_prefs)) == 0)
		smime_prefs_changed = PR_FALSE;
	    else
		smime_prefs_changed = PR_TRUE;
	    PORT_Free (smime_prefs);
	}

	smime_prefs = smime_newprefs;
	smime_prefs_complete = PR_TRUE;
	return SECSuccess;
    }

    PORT_Assert (mask == CIPHER_FAMILYID_SMIME);
    if (mask != CIPHER_FAMILYID_SMIME) {
	/* XXX set an error! */
    	return SECFailure;
    }

    if (on) {
	PORT_Assert (smime_current_pref_index < smime_symmetric_count);
	if (smime_current_pref_index >= smime_symmetric_count) {
	    /* XXX set an error! */
	    return SECFailure;
	}

	smime_newprefs[smime_current_pref_index++] = which;
    }

    return SECSuccess;
}


/*
 * this function locally records the export policy
 */
SECStatus 
SECMIME_SetPolicy(long which, int on)
{
    unsigned long mask;

    PORT_Assert ((which & CIPHER_FAMILYID_MASK) == CIPHER_FAMILYID_SMIME);
    if ((which & CIPHER_FAMILYID_MASK) != CIPHER_FAMILYID_SMIME) {
	/* XXX set an error! */
    	return SECFailure;
    }

    which &= ~CIPHER_FAMILYID_MASK;

    PORT_Assert (which < 32);	/* bits in the long */
    if (which >= 32) {
	/* XXX set an error! */
    	return SECFailure;
    }

    mask = 1UL << which;

    if (on) {
    	smime_policy_bits |= mask;
    } else {
    	smime_policy_bits &= ~mask;
    }

    return SECSuccess;
}


/*
 * Based on the given algorithm (including its parameters, in some cases!)
 * and the given key (may or may not be inspected, depending on the
 * algorithm), find the appropriate policy algorithm specification
 * and return it.  If no match can be made, -1 is returned.
 */
static long
smime_policy_algorithm (SECAlgorithmID *algid, PK11SymKey *key)
{
    SECOidTag algtag;

    algtag = SECOID_GetAlgorithmTag (algid);
    switch (algtag) {
      case SEC_OID_RC2_CBC:
	{
	    unsigned int keylen_bits;

	    keylen_bits = PK11_GetKeyStrength (key, algid);
	    switch (keylen_bits) {
	      case 40:
		return SMIME_RC2_CBC_40;
	      case 64:
		return SMIME_RC2_CBC_64;
	      case 128:
		return SMIME_RC2_CBC_128;
	      default:
		break;
	    }
	}
	break;
      case SEC_OID_DES_CBC:
	return SMIME_DES_CBC_56;
      case SEC_OID_DES_EDE3_CBC:
	return SMIME_DES_EDE3_168;
#ifdef SMIME_DOES_RC5
      case SEC_OID_RC5_CBC_PAD:
	PORT_Assert (0);	/* XXX need to pull out parameters and match */
	break;
#endif
      default:
	break;
    }

    return -1;
}


static PRBool
smime_cipher_allowed (unsigned long which)
{
    unsigned long mask;

    which &= ~CIPHER_FAMILYID_MASK;
    PORT_Assert (which < 32);	/* bits per long (min) */
    if (which >= 32)
	return PR_FALSE;

    mask = 1UL << which;
    if ((mask & smime_policy_bits) == 0)
	return PR_FALSE;

    return PR_TRUE;
}


PRBool
SECMIME_DecryptionAllowed(SECAlgorithmID *algid, PK11SymKey *key)
{
    long which;

    which = smime_policy_algorithm (algid, key);
    if (which < 0)
	return PR_FALSE;

    return smime_cipher_allowed ((unsigned long)which);
}


/*
 * Does the current policy allow *any* S/MIME encryption (or decryption)?
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
SECMIME_EncryptionPossible (void)
{
    if (smime_policy_bits != 0)
	return PR_TRUE;

    return PR_FALSE;
}


/*
 * XXX Would like the "parameters" field to be a SECItem *, but the
 * encoder is having trouble with optional pointers to an ANY.  Maybe
 * once that is fixed, can change this back...
 */
typedef struct smime_capability_struct {
    unsigned long cipher;	/* local; not part of encoding */
    SECOidTag capIDTag;		/* local; not part of encoding */
    SECItem capabilityID;
    SECItem parameters;
} smime_capability;

static const SEC_ASN1Template smime_capability_template[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(smime_capability) },
    { SEC_ASN1_OBJECT_ID,
	  offsetof(smime_capability,capabilityID), },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY,
	  offsetof(smime_capability,parameters), },
    { 0, }
};

static const SEC_ASN1Template smime_capabilities_template[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, smime_capability_template }
};



static void
smime_fill_capability (smime_capability *cap)
{
    unsigned long cipher;
    SECOidTag algtag;
    int i;

    algtag = SECOID_FindOIDTag (&(cap->capabilityID));

    for (i = 0; i < smime_symmetric_count; i++) {
	if (smime_cipher_maps[i].algtag != algtag)
	    continue;
	/*
	 * XXX If SECITEM_CompareItem allowed NULLs as arguments (comparing
	 * 2 NULLs as equal and NULL and non-NULL as not equal), we could
	 * use that here instead of all of the following comparison code.
	 */
	if (cap->parameters.data != NULL) {
	    if (smime_cipher_maps[i].parms == NULL)
		continue;
	    if (cap->parameters.len != smime_cipher_maps[i].parms->len)
		continue;
	    if (PORT_Memcmp (cap->parameters.data,
			     smime_cipher_maps[i].parms->data,
			     cap->parameters.len) == 0)
		break;
	} else if (smime_cipher_maps[i].parms == NULL) {
	    break;
	}
    }

    if (i == smime_symmetric_count)
	cipher = 0;
    else
	cipher = smime_cipher_maps[i].cipher;

    cap->cipher = cipher;
    cap->capIDTag = algtag;
}


static long
smime_choose_cipher (CERTCertificate *scert, CERTCertificate **rcerts)
{
    PRArenaPool *poolp;
    long chosen_cipher;
    int *cipher_abilities;
    int *cipher_votes;
    int strong_mapi;
    int rcount, mapi, max;

    if (smime_policy_bits == 0) {
	PORT_SetError (SEC_ERROR_BAD_EXPORT_ALGORITHM);
	return -1;
    }

    chosen_cipher = SMIME_RC2_CBC_40;		/* the default, LCD */

    poolp = PORT_NewArena (1024);		/* XXX what is right value? */
    if (poolp == NULL)
	goto done;

    cipher_abilities = (int*)PORT_ArenaZAlloc (poolp,
					 smime_symmetric_count * sizeof(int));
    if (cipher_abilities == NULL)
	goto done;

    cipher_votes = (int*)PORT_ArenaZAlloc (poolp,
				     smime_symmetric_count * sizeof(int));
    if (cipher_votes == NULL)
	goto done;

    /*
     * XXX Should have a #define somewhere which specifies default
     * strong cipher.  (Or better, a way to configure.)
     */

    /* Make triple-DES the strong cipher. */
    strong_mapi = smime_mapi_by_cipher (SMIME_DES_EDE3_168);

    PORT_Assert (strong_mapi >= 0);

    for (rcount = 0; rcerts[rcount] != NULL; rcount++) {
	SECItem *profile;
	smime_capability **caps;
	int capi, pref;
	SECStatus dstat;

	pref = smime_symmetric_count;
	profile = CERT_FindSMimeProfile (rcerts[rcount]);
	if (profile != NULL && profile->data != NULL && profile->len > 0) {
	    caps = NULL;
	    dstat = SEC_QuickDERDecodeItem (poolp, &caps,
					smime_capabilities_template,
					profile);
	    if (dstat == SECSuccess && caps != NULL) {
		for (capi = 0; caps[capi] != NULL; capi++) {
		    smime_fill_capability (caps[capi]);
		    mapi = smime_mapi_by_cipher (caps[capi]->cipher);
		    if (mapi >= 0) {
			cipher_abilities[mapi]++;
			cipher_votes[mapi] += pref;
			--pref;
		    }
		}
	    }
	} else {
	    SECKEYPublicKey *key;
	    unsigned int pklen_bits;

	    /*
	     * XXX This is probably only good for RSA keys.  What I would
	     * really like is a function to just say;  Is the public key in
	     * this cert an export-length key?  Then I would not have to
	     * know things like the value 512, or the kind of key, or what
	     * a subjectPublicKeyInfo is, etc.
	     */
	    key = CERT_ExtractPublicKey (rcerts[rcount]);
	    if (key != NULL) {
		pklen_bits = SECKEY_PublicKeyStrength (key) * 8;
		SECKEY_DestroyPublicKey (key);

		if (pklen_bits > 512) {
		    cipher_abilities[strong_mapi]++;
		    cipher_votes[strong_mapi] += pref;
		}
	    }
	}
	if (profile != NULL)
	    SECITEM_FreeItem (profile, PR_TRUE);
    }

    max = 0;
    for (mapi = 0; mapi < smime_symmetric_count; mapi++) {
	if (cipher_abilities[mapi] != rcount)
	    continue;
	if (! smime_cipher_allowed (smime_cipher_maps[mapi].cipher))
	    continue;
	if (cipher_votes[mapi] > max) {
	    chosen_cipher = smime_cipher_maps[mapi].cipher;
	    max = cipher_votes[mapi];
	} /* XXX else if a tie, let scert break it? */
    }

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
	keysize = 128;
	break;
#ifdef SMIME_DOES_RC5
      case SMIME_RC5PAD_64_16_40:
      case SMIME_RC5PAD_64_16_64:
      case SMIME_RC5PAD_64_16_128:
	/* XXX See comment above; keysize is not enough... */
	PORT_Assert (0);
	PORT_SetError (SEC_ERROR_INVALID_ALGORITHM);
	keysize = -1;
	break;
#endif
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
 * Start an S/MIME encrypting context.
 *
 * "scert" is the cert for the sender.  It will be checked for validity.
 * "rcerts" are the certs for the recipients.  They will also be checked.
 *
 * "certdb" is the cert database to use for verifying the certs.
 * It can be NULL if a default database is available (like in the client).
 *
 * This function already does all of the stuff specific to S/MIME protocol
 * and local policy; the return value just needs to be passed to
 * SEC_PKCS7Encode() or to SEC_PKCS7EncoderStart() to create the encoded data,
 * and finally to SEC_PKCS7DestroyContentInfo().
 *
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
SEC_PKCS7ContentInfo *
SECMIME_CreateEncrypted(CERTCertificate *scert,
			CERTCertificate **rcerts,
			CERTCertDBHandle *certdb,
			SECKEYGetPasswordKey pwfn,
			void *pwfn_arg)
{
    SEC_PKCS7ContentInfo *cinfo;
    long cipher;
    SECOidTag encalg;
    int keysize;
    int mapi, rci;

    cipher = smime_choose_cipher (scert, rcerts);
    if (cipher < 0)
	return NULL;

    mapi = smime_mapi_by_cipher (cipher);
    if (mapi < 0)
	return NULL;

    /*
     * XXX This is stretching it -- CreateEnvelopedData should probably
     * take a cipher itself of some sort, because we cannot know what the
     * future will bring in terms of parameters for each type of algorithm.
     * For example, just an algorithm and keysize is *not* sufficient to
     * fully specify the usage of RC5 (which also needs to know rounds and
     * block size).  Work this out into a better API!
     */
    encalg = smime_cipher_maps[mapi].algtag;
    keysize = smime_keysize_by_cipher (cipher);
    if (keysize < 0)
	return NULL;

    cinfo = SEC_PKCS7CreateEnvelopedData (scert, certUsageEmailRecipient,
					  certdb, encalg, keysize,
					  pwfn, pwfn_arg);
    if (cinfo == NULL)
	return NULL;

    for (rci = 0; rcerts[rci] != NULL; rci++) {
	if (rcerts[rci] == scert)
	    continue;
	if (SEC_PKCS7AddRecipient (cinfo, rcerts[rci], certUsageEmailRecipient,
				   NULL) != SECSuccess) {
	    SEC_PKCS7DestroyContentInfo (cinfo);
	    return NULL;
	}
    }

    return cinfo;
}


static smime_capability **smime_capabilities;
static SECItem *smime_encoded_caps;


static SECStatus
smime_init_caps (void)
{
    smime_capability *cap;
    smime_cipher_map *map;
    SECOidData *oiddata;
    SECStatus rv;
    int i;

    if (smime_encoded_caps != NULL && (! smime_prefs_changed))
	return SECSuccess;

    if (smime_encoded_caps != NULL) {
	SECITEM_FreeItem (smime_encoded_caps, PR_TRUE);
	smime_encoded_caps = NULL;
    }

    if (smime_capabilities == NULL) {
	smime_capabilities = (smime_capability**)PORT_ZAlloc (
					  (smime_symmetric_count + 1)
					  * sizeof(smime_capability *));
	if (smime_capabilities == NULL)
	    return SECFailure;
    }

    rv = SECFailure;

    /* 
       The process of creating the encoded PKCS7 cipher capability list
       involves two basic steps: 

       (a) Convert our internal representation of cipher preferences 
           (smime_prefs) into an array containing cipher OIDs and 
	   parameter data (smime_capabilities). This step is
	   performed here.

       (b) Encode, using ASN.1, the cipher information in 
           smime_capabilities, leaving the encoded result in 
	   smime_encoded_caps.

       (In the process of performing (a), Lisa put in some optimizations
       which allow us to avoid needlessly re-populating elements in 
       smime_capabilities as we walk through smime_prefs.)
    */
    for (i = 0; i < smime_current_pref_index; i++) {
	int mapi;

	/* Get the next cipher preference in smime_prefs. */
	mapi = smime_mapi_by_cipher (smime_prefs[i]);
	if (mapi < 0)
	    break;

	/* Find the corresponding entry in the cipher map. */
	PORT_Assert (mapi < smime_symmetric_count);
	map = &(smime_cipher_maps[mapi]);

	/*
	 * Convert the next preference found in smime_prefs into an
	 * smime_capability.
	 */

	cap = smime_capabilities[i];
	if (cap == NULL) {
	    cap = (smime_capability*)PORT_ZAlloc (sizeof(smime_capability));
	    if (cap == NULL)
		break;
	    smime_capabilities[i] = cap;
	} else if (cap->cipher == smime_prefs[i]) {
	    continue;		/* no change to this one */
	}

	cap->capIDTag = map->algtag;
	oiddata = SECOID_FindOIDByTag (map->algtag);
	if (oiddata == NULL)
	    break;

	if (cap->capabilityID.data != NULL) {
	    SECITEM_FreeItem (&(cap->capabilityID), PR_FALSE);
	    cap->capabilityID.data = NULL;
	    cap->capabilityID.len = 0;
	}

	rv = SECITEM_CopyItem (NULL, &(cap->capabilityID), &(oiddata->oid));
	if (rv != SECSuccess)
	    break;

	if (map->parms == NULL) {
	    cap->parameters.data = NULL;
	    cap->parameters.len = 0;
	} else {
	    cap->parameters.data = map->parms->data;
	    cap->parameters.len = map->parms->len;
	}

	cap->cipher = smime_prefs[i];
    }

    if (i != smime_current_pref_index)
	return rv;

    while (i < smime_symmetric_count) {
	cap = smime_capabilities[i];
	if (cap != NULL) {
	    SECITEM_FreeItem (&(cap->capabilityID), PR_FALSE);
	    PORT_Free (cap);
	}
	smime_capabilities[i] = NULL;
	i++;
    }
    smime_capabilities[i] = NULL;

    smime_encoded_caps = SEC_ASN1EncodeItem (NULL, NULL, &smime_capabilities,
					     smime_capabilities_template);
    if (smime_encoded_caps == NULL)
	return SECFailure;

    return SECSuccess;
}


static SECStatus
smime_add_profile (CERTCertificate *cert, SEC_PKCS7ContentInfo *cinfo)
{
    PORT_Assert (smime_prefs_complete);
    if (! smime_prefs_complete)
	return SECFailure;

    /* For that matter, if capabilities haven't been initialized yet,
       do so now. */
    if (smime_encoded_caps == NULL || smime_prefs_changed) {
	SECStatus rv;

	rv = smime_init_caps();
	if (rv != SECSuccess)
	    return rv;

	PORT_Assert (smime_encoded_caps != NULL);
    }

    return SEC_PKCS7AddSignedAttribute (cinfo, SEC_OID_PKCS9_SMIME_CAPABILITIES,
					smime_encoded_caps);
}


/*
 * Start an S/MIME signing context.
 *
 * "scert" is the cert that will be used to sign the data.  It will be
 * checked for validity.
 *
 * "ecert" is the signer's encryption cert.  If it is different from
 * scert, then it will be included in the signed message so that the
 * recipient can save it for future encryptions.
 *
 * "certdb" is the cert database to use for verifying the cert.
 * It can be NULL if a default database is available (like in the client).
 * 
 * "digestalg" names the digest algorithm (e.g. SEC_OID_SHA1).
 * XXX There should be SECMIME functions for hashing, or the hashing should
 * be built into this interface, which we would like because we would
 * support more smartcards that way, and then this argument should go away.)
 *
 * "digest" is the actual digest of the data.  It must be provided in
 * the case of detached data or NULL if the content will be included.
 *
 * This function already does all of the stuff specific to S/MIME protocol
 * and local policy; the return value just needs to be passed to
 * SEC_PKCS7Encode() or to SEC_PKCS7EncoderStart() to create the encoded data,
 * and finally to SEC_PKCS7DestroyContentInfo().
 *
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */

SEC_PKCS7ContentInfo *
SECMIME_CreateSigned (CERTCertificate *scert,
		      CERTCertificate *ecert,
		      CERTCertDBHandle *certdb,
		      SECOidTag digestalg,
		      SECItem *digest,
		      SECKEYGetPasswordKey pwfn,
		      void *pwfn_arg)
{
    SEC_PKCS7ContentInfo *cinfo;
    SECStatus rv;

    /* See note in header comment above about digestalg. */
    /* Doesn't explain this. PORT_Assert (digestalg == SEC_OID_SHA1); */

    cinfo = SEC_PKCS7CreateSignedData (scert, certUsageEmailSigner,
				       certdb, digestalg, digest,
				       pwfn, pwfn_arg);
    if (cinfo == NULL)
	return NULL;

    if (SEC_PKCS7IncludeCertChain (cinfo, NULL) != SECSuccess) {
	SEC_PKCS7DestroyContentInfo (cinfo);
	return NULL;
    }

    /* if the encryption cert and the signing cert differ, then include
     * the encryption cert too.
     */
    /* it is ok to compare the pointers since we ref count, and the same
     * cert will always have the same pointer
     */
    if ( ( ecert != NULL ) && ( ecert != scert ) ) {
	rv = SEC_PKCS7AddCertificate(cinfo, ecert);
	if ( rv != SECSuccess ) {
	    SEC_PKCS7DestroyContentInfo (cinfo);
	    return NULL;
	}
    }
    /*
     * Add the signing time.  But if it fails for some reason,
     * may as well not give up altogether -- just assert.
     */
    rv = SEC_PKCS7AddSigningTime (cinfo);
    PORT_Assert (rv == SECSuccess);

    /*
     * Add the email profile.  Again, if it fails for some reason,
     * may as well not give up altogether -- just assert.
     */
    rv = smime_add_profile (ecert, cinfo);
    PORT_Assert (rv == SECSuccess);

    return cinfo;
}
