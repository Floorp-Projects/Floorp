/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * PKCS7 creation.
 *
 * $Id$
 */

#include "p7local.h"

#include "cert.h"
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
#include "pk11func.h"
#include "prtime.h"
#include "secerr.h"
#include "secder.h"
#include "secpkcs5.h"

const int NSS_PBE_DEFAULT_ITERATION_COUNT = 2000; /* used in p12e.c too */

static SECStatus
sec_pkcs7_init_content_info (SEC_PKCS7ContentInfo *cinfo, PRArenaPool *poolp,
			     SECOidTag kind, PRBool detached)
{
    void *thing;
    int version;
    SECItem *versionp;
    SECStatus rv;

    PORT_Assert (cinfo != NULL && poolp != NULL);
    if (cinfo == NULL || poolp == NULL)
	return SECFailure;

    cinfo->contentTypeTag = SECOID_FindOIDByTag (kind);
    PORT_Assert (cinfo->contentTypeTag
		 && cinfo->contentTypeTag->offset == kind);

    rv = SECITEM_CopyItem (poolp, &(cinfo->contentType),
			   &(cinfo->contentTypeTag->oid));
    if (rv != SECSuccess)
	return rv;

    if (detached)
	return SECSuccess;

    switch (kind) {
      default:
      case SEC_OID_PKCS7_DATA:
	thing = PORT_ArenaZAlloc (poolp, sizeof(SECItem));
	cinfo->content.data = (SECItem*)thing;
	versionp = NULL;
	version = -1;
	break;
      case SEC_OID_PKCS7_DIGESTED_DATA:
	thing = PORT_ArenaZAlloc (poolp, sizeof(SEC_PKCS7DigestedData));
	cinfo->content.digestedData = (SEC_PKCS7DigestedData*)thing;
	versionp = &(cinfo->content.digestedData->version);
	version = SEC_PKCS7_DIGESTED_DATA_VERSION;
	break;
      case SEC_OID_PKCS7_ENCRYPTED_DATA:
	thing = PORT_ArenaZAlloc (poolp, sizeof(SEC_PKCS7EncryptedData));
	cinfo->content.encryptedData = (SEC_PKCS7EncryptedData*)thing;
	versionp = &(cinfo->content.encryptedData->version);
	version = SEC_PKCS7_ENCRYPTED_DATA_VERSION;
	break;
      case SEC_OID_PKCS7_ENVELOPED_DATA:
	thing = PORT_ArenaZAlloc (poolp, sizeof(SEC_PKCS7EnvelopedData));
	cinfo->content.envelopedData = 
	  (SEC_PKCS7EnvelopedData*)thing;
	versionp = &(cinfo->content.envelopedData->version);
	version = SEC_PKCS7_ENVELOPED_DATA_VERSION;
	break;
      case SEC_OID_PKCS7_SIGNED_DATA:
	thing = PORT_ArenaZAlloc (poolp, sizeof(SEC_PKCS7SignedData));
	cinfo->content.signedData = 
	  (SEC_PKCS7SignedData*)thing;
	versionp = &(cinfo->content.signedData->version);
	version = SEC_PKCS7_SIGNED_DATA_VERSION;
	break;
      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
	thing = PORT_ArenaZAlloc(poolp,sizeof(SEC_PKCS7SignedAndEnvelopedData));
	cinfo->content.signedAndEnvelopedData = 
	  (SEC_PKCS7SignedAndEnvelopedData*)thing;
	versionp = &(cinfo->content.signedAndEnvelopedData->version);
	version = SEC_PKCS7_SIGNED_AND_ENVELOPED_DATA_VERSION;
	break;
    }

    if (thing == NULL)
	return SECFailure;

    if (versionp != NULL) {
	SECItem *dummy;

	PORT_Assert (version >= 0);
	dummy = SEC_ASN1EncodeInteger (poolp, versionp, version);
	if (dummy == NULL)
	    return SECFailure;
	PORT_Assert (dummy == versionp);
    }

    return SECSuccess;
}


static SEC_PKCS7ContentInfo *
sec_pkcs7_create_content_info (SECOidTag kind, PRBool detached,
			       SECKEYGetPasswordKey pwfn, void *pwfn_arg)
{
    SEC_PKCS7ContentInfo *cinfo;
    PRArenaPool *poolp;
    SECStatus rv;

    poolp = PORT_NewArena (1024);	/* XXX what is right value? */
    if (poolp == NULL)
	return NULL;

    cinfo = (SEC_PKCS7ContentInfo*)PORT_ArenaZAlloc (poolp, sizeof(*cinfo));
    if (cinfo == NULL) {
	PORT_FreeArena (poolp, PR_FALSE);
	return NULL;
    }

    cinfo->poolp = poolp;
    cinfo->pwfn = pwfn;
    cinfo->pwfn_arg = pwfn_arg;
    cinfo->created = PR_TRUE;
    cinfo->refCount = 1;

    rv = sec_pkcs7_init_content_info (cinfo, poolp, kind, detached);
    if (rv != SECSuccess) {
	PORT_FreeArena (poolp, PR_FALSE);
	return NULL;
    }

    return cinfo;
}


/*
 * Add a signer to a PKCS7 thing, verifying the signature cert first.
 * Any error returns SECFailure.
 *
 * XXX Right now this only adds the *first* signer.  It fails if you try
 * to add a second one -- this needs to be fixed.
 */
static SECStatus
sec_pkcs7_add_signer (SEC_PKCS7ContentInfo *cinfo,
		      CERTCertificate *     cert,
		      SECCertUsage          certusage,
		      CERTCertDBHandle *    certdb,
		      SECOidTag             digestalgtag,
		      SECItem *             digestdata)
{
    SEC_PKCS7SignerInfo *signerinfo, **signerinfos, ***signerinfosp;
    SECAlgorithmID      *digestalg,  **digestalgs,  ***digestalgsp;
    SECItem             *digest,     **digests,     ***digestsp;
    SECItem *            dummy;
    void *               mark;
    SECStatus            rv;
    SECOidTag            kind;

    kind = SEC_PKCS7ContentType (cinfo);
    switch (kind) {
      case SEC_OID_PKCS7_SIGNED_DATA:
	{
	    SEC_PKCS7SignedData *sdp;

	    sdp = cinfo->content.signedData;
	    digestalgsp = &(sdp->digestAlgorithms);
	    digestsp = &(sdp->digests);
	    signerinfosp = &(sdp->signerInfos);
	}
	break;
      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
	{
	    SEC_PKCS7SignedAndEnvelopedData *saedp;

	    saedp = cinfo->content.signedAndEnvelopedData;
	    digestalgsp = &(saedp->digestAlgorithms);
	    digestsp = &(saedp->digests);
	    signerinfosp = &(saedp->signerInfos);
	}
	break;
      default:
	return SECFailure;		/* XXX set an error? */
    }

    /*
     * XXX I think that CERT_VerifyCert should do this if *it* is passed
     * a NULL database.
     */
    if (certdb == NULL) {
	certdb = CERT_GetDefaultCertDB();
	if (certdb == NULL)
	    return SECFailure;		/* XXX set an error? */
    }

    if (CERT_VerifyCert (certdb, cert, PR_TRUE, certusage, PR_Now(),
			 cinfo->pwfn_arg, NULL) != SECSuccess)
	{
	/* XXX Did CERT_VerifyCert set an error? */
	return SECFailure;
    }

    /*
     * XXX This is the check that we do not already have a signer.
     * This is not what we really want -- we want to allow this
     * and *add* the new signer.
     */
    PORT_Assert (*signerinfosp == NULL
		 && *digestalgsp == NULL && *digestsp == NULL);
    if (*signerinfosp != NULL || *digestalgsp != NULL || *digestsp != NULL)
	return SECFailure;

    mark = PORT_ArenaMark (cinfo->poolp);

    signerinfo = (SEC_PKCS7SignerInfo*)PORT_ArenaZAlloc (cinfo->poolp, 
						  sizeof(SEC_PKCS7SignerInfo));
    if (signerinfo == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }

    dummy = SEC_ASN1EncodeInteger (cinfo->poolp, &signerinfo->version,
				   SEC_PKCS7_SIGNER_INFO_VERSION);
    if (dummy == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }
    PORT_Assert (dummy == &signerinfo->version);

    signerinfo->cert = CERT_DupCertificate (cert);
    if (signerinfo->cert == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }

    signerinfo->issuerAndSN = CERT_GetCertIssuerAndSN (cinfo->poolp, cert);
    if (signerinfo->issuerAndSN == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }

    rv = SECOID_SetAlgorithmID (cinfo->poolp, &signerinfo->digestAlg,
				digestalgtag, NULL);
    if (rv != SECSuccess) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }

    /*
     * Okay, now signerinfo is all set.  We just need to put it and its
     * companions (another copy of the digest algorithm, and the digest
     * itself if given) into the main structure.
     *
     * XXX If we are handling more than one signer, the following code
     * needs to look through the digest algorithms already specified
     * and see if the same one is there already.  If it is, it does not
     * need to be added again.  Also, if it is there *and* the digest
     * is not null, then the digest given should match the digest already
     * specified -- if not, that is an error.  Finally, the new signerinfo
     * should be *added* to the set already found.
     */

    signerinfos = (SEC_PKCS7SignerInfo**)PORT_ArenaAlloc (cinfo->poolp,
				   2 * sizeof(SEC_PKCS7SignerInfo *));
    if (signerinfos == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }
    signerinfos[0] = signerinfo;
    signerinfos[1] = NULL;

    digestalg = PORT_ArenaZAlloc (cinfo->poolp, sizeof(SECAlgorithmID));
    digestalgs = PORT_ArenaAlloc (cinfo->poolp, 2 * sizeof(SECAlgorithmID *));
    if (digestalg == NULL || digestalgs == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }
    rv = SECOID_SetAlgorithmID (cinfo->poolp, digestalg, digestalgtag, NULL);
    if (rv != SECSuccess) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }
    digestalgs[0] = digestalg;
    digestalgs[1] = NULL;

    if (digestdata != NULL) {
	digest = (SECItem*)PORT_ArenaAlloc (cinfo->poolp, sizeof(SECItem));
	digests = (SECItem**)PORT_ArenaAlloc (cinfo->poolp, 
					      2 * sizeof(SECItem *));
	if (digest == NULL || digests == NULL) {
	    PORT_ArenaRelease (cinfo->poolp, mark);
	    return SECFailure;
	}
	rv = SECITEM_CopyItem (cinfo->poolp, digest, digestdata);
	if (rv != SECSuccess) {
	    PORT_ArenaRelease (cinfo->poolp, mark);
	    return SECFailure;
	}
	digests[0] = digest;
	digests[1] = NULL;
    } else {
	digests = NULL;
    }

    *signerinfosp = signerinfos;
    *digestalgsp = digestalgs;
    *digestsp = digests;

    PORT_ArenaUnmark(cinfo->poolp, mark);
    return SECSuccess;
}


/*
 * Helper function for creating an empty signedData.
 */
static SEC_PKCS7ContentInfo *
sec_pkcs7_create_signed_data (SECKEYGetPasswordKey pwfn, void *pwfn_arg)
{
    SEC_PKCS7ContentInfo *cinfo;
    SEC_PKCS7SignedData *sigd;
    SECStatus rv;

    cinfo = sec_pkcs7_create_content_info (SEC_OID_PKCS7_SIGNED_DATA, PR_FALSE,
					   pwfn, pwfn_arg);
    if (cinfo == NULL)
	return NULL;

    sigd = cinfo->content.signedData;
    PORT_Assert (sigd != NULL);

    /*
     * XXX Might we want to allow content types other than data?
     * If so, via what interface?
     */
    rv = sec_pkcs7_init_content_info (&(sigd->contentInfo), cinfo->poolp,
				      SEC_OID_PKCS7_DATA, PR_TRUE);
    if (rv != SECSuccess) {
	SEC_PKCS7DestroyContentInfo (cinfo);
	return NULL;
    }

    return cinfo;
}


/*
 * Start a PKCS7 signing context.
 *
 * "cert" is the cert that will be used to sign the data.  It will be
 * checked for validity.
 *
 * "certusage" describes the signing usage (e.g. certUsageEmailSigner)
 * XXX Maybe SECCertUsage should be split so that our caller just says
 * "email" and *we* add the "signing" part -- otherwise our caller
 * could be lying about the usage; we do not want to allow encryption
 * certs for signing or vice versa.
 *
 * "certdb" is the cert database to use for verifying the cert.
 * It can be NULL if a default database is available (like in the client).
 * 
 * "digestalg" names the digest algorithm (e.g. SEC_OID_SHA1).
 *
 * "digest" is the actual digest of the data.  It must be provided in
 * the case of detached data or NULL if the content will be included.
 *
 * The return value can be passed to functions which add things to
 * it like attributes, then eventually to SEC_PKCS7Encode() or to
 * SEC_PKCS7EncoderStart() to create the encoded data, and finally to
 * SEC_PKCS7DestroyContentInfo().
 *
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
SEC_PKCS7ContentInfo *
SEC_PKCS7CreateSignedData (CERTCertificate *cert,
			   SECCertUsage certusage,
			   CERTCertDBHandle *certdb,
			   SECOidTag digestalg,
			   SECItem *digest,
 			   SECKEYGetPasswordKey pwfn, void *pwfn_arg)
{
    SEC_PKCS7ContentInfo *cinfo;
    SECStatus rv;

    cinfo = sec_pkcs7_create_signed_data (pwfn, pwfn_arg);
    if (cinfo == NULL)
	return NULL;

    rv = sec_pkcs7_add_signer (cinfo, cert, certusage, certdb,
			       digestalg, digest);
    if (rv != SECSuccess) {
	SEC_PKCS7DestroyContentInfo (cinfo);
	return NULL;
    }

    return cinfo;
}


static SEC_PKCS7Attribute *
sec_pkcs7_create_attribute (PRArenaPool *poolp, SECOidTag oidtag,
			    SECItem *value, PRBool encoded)
{
    SEC_PKCS7Attribute *attr;
    SECItem **values;
    void *mark;

    PORT_Assert (poolp != NULL);
    mark = PORT_ArenaMark (poolp);

    attr = (SEC_PKCS7Attribute*)PORT_ArenaAlloc (poolp, 
						 sizeof(SEC_PKCS7Attribute));
    if (attr == NULL)
	goto loser;

    attr->typeTag = SECOID_FindOIDByTag (oidtag);
    if (attr->typeTag == NULL)
	goto loser;

    if (SECITEM_CopyItem (poolp, &(attr->type),
			  &(attr->typeTag->oid)) != SECSuccess)
	goto loser;

    values = (SECItem**)PORT_ArenaAlloc (poolp, 2 * sizeof(SECItem *));
    if (values == NULL)
	goto loser;

    if (value != NULL) {
	SECItem *copy;

	copy = (SECItem*)PORT_ArenaAlloc (poolp, sizeof(SECItem));
	if (copy == NULL)
	    goto loser;

	if (SECITEM_CopyItem (poolp, copy, value) != SECSuccess)
	    goto loser;

	value = copy;
    }

    values[0] = value;
    values[1] = NULL;
    attr->values = values;
    attr->encoded = encoded;

    PORT_ArenaUnmark (poolp, mark);
    return attr;

loser:
    PORT_Assert (mark != NULL);
    PORT_ArenaRelease (poolp, mark);
    return NULL;
}


static SECStatus
sec_pkcs7_add_attribute (SEC_PKCS7ContentInfo *cinfo,
			 SEC_PKCS7Attribute ***attrsp,
			 SEC_PKCS7Attribute *attr)
{
    SEC_PKCS7Attribute **attrs;
    SECItem *ct_value;
    void *mark;

    PORT_Assert (SEC_PKCS7ContentType (cinfo) == SEC_OID_PKCS7_SIGNED_DATA);
    if (SEC_PKCS7ContentType (cinfo) != SEC_OID_PKCS7_SIGNED_DATA)
	return SECFailure;

    attrs = *attrsp;
    if (attrs != NULL) {
	int count;

	/*
	 * We already have some attributes, and just need to add this
	 * new one.
	 */

	/*
	 * We should already have the *required* attributes, which were
	 * created/added at the same time the first attribute was added.
	 */
	PORT_Assert (sec_PKCS7FindAttribute (attrs,
					     SEC_OID_PKCS9_CONTENT_TYPE,
					     PR_FALSE) != NULL);
	PORT_Assert (sec_PKCS7FindAttribute (attrs,
					     SEC_OID_PKCS9_MESSAGE_DIGEST,
					     PR_FALSE) != NULL);

	for (count = 0; attrs[count] != NULL; count++)
	    ;
	attrs = (SEC_PKCS7Attribute**)PORT_ArenaGrow (cinfo->poolp, attrs,
				(count + 1) * sizeof(SEC_PKCS7Attribute *),
				(count + 2) * sizeof(SEC_PKCS7Attribute *));
	if (attrs == NULL)
	    return SECFailure;

	attrs[count] = attr;
	attrs[count+1] = NULL;
	*attrsp = attrs;

	return SECSuccess;
    }

    /*
     * This is the first time an attribute is going in.
     * We need to create and add the required attributes, and then
     * we will also add in the one our caller gave us.
     */

    /*
     * There are 2 required attributes, plus the one our caller wants
     * to add, plus we always end with a NULL one.  Thus, four slots.
     */
    attrs = (SEC_PKCS7Attribute**)PORT_ArenaAlloc (cinfo->poolp, 
					   4 * sizeof(SEC_PKCS7Attribute *));
    if (attrs == NULL)
	return SECFailure;

    mark = PORT_ArenaMark (cinfo->poolp);

    /*
     * First required attribute is the content type of the data
     * being signed.
     */
    ct_value = &(cinfo->content.signedData->contentInfo.contentType);
    attrs[0] = sec_pkcs7_create_attribute (cinfo->poolp,
					   SEC_OID_PKCS9_CONTENT_TYPE,
					   ct_value, PR_FALSE);
    /*
     * Second required attribute is the message digest of the data
     * being signed; we leave the value NULL for now (just create
     * the place for it to go), and the encoder will fill it in later.
     */
    attrs[1] = sec_pkcs7_create_attribute (cinfo->poolp,
					   SEC_OID_PKCS9_MESSAGE_DIGEST,
					   NULL, PR_FALSE);
    if (attrs[0] == NULL || attrs[1] == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure; 
    }

    attrs[2] = attr;
    attrs[3] = NULL;
    *attrsp = attrs;

    PORT_ArenaUnmark (cinfo->poolp, mark);
    return SECSuccess;
}


/*
 * Add the signing time to the authenticated (i.e. signed) attributes
 * of "cinfo".  This is expected to be included in outgoing signed
 * messages for email (S/MIME) but is likely useful in other situations.
 *
 * This should only be added once; a second call will either do
 * nothing or replace an old signing time with a newer one.
 *
 * XXX This will probably just shove the current time into "cinfo"
 * but it will not actually get signed until the entire item is
 * processed for encoding.  Is this (expected to be small) delay okay?
 *
 * "cinfo" should be of type signedData (the only kind of pkcs7 data
 * that is allowed authenticated attributes); SECFailure will be returned
 * if it is not.
 */
SECStatus
SEC_PKCS7AddSigningTime (SEC_PKCS7ContentInfo *cinfo)
{
    SEC_PKCS7SignerInfo **signerinfos;
    SEC_PKCS7Attribute *attr;
    SECItem stime;
    SECStatus rv;
    int si;

    PORT_Assert (SEC_PKCS7ContentType (cinfo) == SEC_OID_PKCS7_SIGNED_DATA);
    if (SEC_PKCS7ContentType (cinfo) != SEC_OID_PKCS7_SIGNED_DATA)
	return SECFailure;

    signerinfos = cinfo->content.signedData->signerInfos;

    /* There has to be a signer, or it makes no sense. */
    if (signerinfos == NULL || signerinfos[0] == NULL)
	return SECFailure;

    rv = DER_EncodeTimeChoice(NULL, &stime, PR_Now());
    if (rv != SECSuccess)
	return rv;

    attr = sec_pkcs7_create_attribute (cinfo->poolp,
				       SEC_OID_PKCS9_SIGNING_TIME,
				       &stime, PR_FALSE);
    SECITEM_FreeItem (&stime, PR_FALSE);

    if (attr == NULL)
	return SECFailure;

    rv = SECSuccess;
    for (si = 0; signerinfos[si] != NULL; si++) {
	SEC_PKCS7Attribute *oattr;

	oattr = sec_PKCS7FindAttribute (signerinfos[si]->authAttr,
					SEC_OID_PKCS9_SIGNING_TIME, PR_FALSE);
	PORT_Assert (oattr == NULL);
	if (oattr != NULL)
	    continue;	/* XXX or would it be better to replace it? */

	rv = sec_pkcs7_add_attribute (cinfo, &(signerinfos[si]->authAttr),
				      attr);
	if (rv != SECSuccess)
	    break;	/* could try to continue, but may as well give up now */
    }

    return rv;
}


/*
 * Add the specified attribute to the authenticated (i.e. signed) attributes
 * of "cinfo" -- "oidtag" describes the attribute and "value" is the
 * value to be associated with it.  NOTE! "value" must already be encoded;
 * no interpretation of "oidtag" is done.  Also, it is assumed that this
 * signedData has only one signer -- if we ever need to add attributes
 * when there is more than one signature, we need a way to specify *which*
 * signature should get the attribute.
 *
 * XXX Technically, a signed attribute can have multiple values; if/when
 * we ever need to support an attribute which takes multiple values, we
 * either need to change this interface or create an AddSignedAttributeValue
 * which can be called subsequently, and would then append a value.
 *
 * "cinfo" should be of type signedData (the only kind of pkcs7 data
 * that is allowed authenticated attributes); SECFailure will be returned
 * if it is not.
 */
SECStatus
SEC_PKCS7AddSignedAttribute (SEC_PKCS7ContentInfo *cinfo,
			     SECOidTag oidtag,
			     SECItem *value)
{
    SEC_PKCS7SignerInfo **signerinfos;
    SEC_PKCS7Attribute *attr;

    PORT_Assert (SEC_PKCS7ContentType (cinfo) == SEC_OID_PKCS7_SIGNED_DATA);
    if (SEC_PKCS7ContentType (cinfo) != SEC_OID_PKCS7_SIGNED_DATA)
	return SECFailure;

    signerinfos = cinfo->content.signedData->signerInfos;

    /*
     * No signature or more than one means no deal.
     */
    if (signerinfos == NULL || signerinfos[0] == NULL || signerinfos[1] != NULL)
	return SECFailure;

    attr = sec_pkcs7_create_attribute (cinfo->poolp, oidtag, value, PR_TRUE);
    if (attr == NULL)
	return SECFailure;

    return sec_pkcs7_add_attribute (cinfo, &(signerinfos[0]->authAttr), attr);
}
 

/*
 * Mark that the signer certificates and their issuing chain should
 * be included in the encoded data.  This is expected to be used
 * in outgoing signed messages for email (S/MIME).
 *
 * "certdb" is the cert database to use for finding the chain.
 * It can be NULL, meaning use the default database.
 *
 * "cinfo" should be of type signedData or signedAndEnvelopedData;
 * SECFailure will be returned if it is not.
 */
SECStatus
SEC_PKCS7IncludeCertChain (SEC_PKCS7ContentInfo *cinfo,
			   CERTCertDBHandle *certdb)
{
    SECOidTag kind;
    SEC_PKCS7SignerInfo *signerinfo, **signerinfos;

    kind = SEC_PKCS7ContentType (cinfo);
    switch (kind) {
      case SEC_OID_PKCS7_SIGNED_DATA:
	signerinfos = cinfo->content.signedData->signerInfos;
	break;
      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
	signerinfos = cinfo->content.signedAndEnvelopedData->signerInfos;
	break;
      default:
	return SECFailure;		/* XXX set an error? */
    }

    if (signerinfos == NULL)		/* no signer, no certs? */
	return SECFailure;		/* XXX set an error? */

    if (certdb == NULL) {
	certdb = CERT_GetDefaultCertDB();
	if (certdb == NULL) {
	    PORT_SetError (SEC_ERROR_BAD_DATABASE);
	    return SECFailure;
	}
    }

    /* XXX Should it be an error if we find no signerinfo or no certs? */
    while ((signerinfo = *signerinfos++) != NULL) {
	if (signerinfo->cert != NULL)
	    /* get the cert chain.  don't send the root to avoid contamination
	     * of old clients with a new root that they don't trust
	     */
	    signerinfo->certList = CERT_CertChainFromCert (signerinfo->cert,
							   certUsageEmailSigner,
							   PR_FALSE);
    }

    return SECSuccess;
}


/*
 * Helper function to add a certificate chain for inclusion in the
 * bag of certificates in a signedData.
 */
static SECStatus
sec_pkcs7_add_cert_chain (SEC_PKCS7ContentInfo *cinfo,
			  CERTCertificate *cert,
			  CERTCertDBHandle *certdb)
{
    SECOidTag kind;
    CERTCertificateList *certlist, **certlists, ***certlistsp;
    int count;

    kind = SEC_PKCS7ContentType (cinfo);
    switch (kind) {
      case SEC_OID_PKCS7_SIGNED_DATA:
	{
	    SEC_PKCS7SignedData *sdp;

	    sdp = cinfo->content.signedData;
	    certlistsp = &(sdp->certLists);
	}
	break;
      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
	{
	    SEC_PKCS7SignedAndEnvelopedData *saedp;

	    saedp = cinfo->content.signedAndEnvelopedData;
	    certlistsp = &(saedp->certLists);
	}
	break;
      default:
	return SECFailure;		/* XXX set an error? */
    }

    if (certdb == NULL) {
	certdb = CERT_GetDefaultCertDB();
	if (certdb == NULL) {
	    PORT_SetError (SEC_ERROR_BAD_DATABASE);
	    return SECFailure;
	}
    }

    certlist = CERT_CertChainFromCert (cert, certUsageEmailSigner, PR_FALSE);
    if (certlist == NULL)
	return SECFailure;

    certlists = *certlistsp;
    if (certlists == NULL) {
	count = 0;
	certlists = (CERTCertificateList**)PORT_ArenaAlloc (cinfo->poolp,
				     2 * sizeof(CERTCertificateList *));
    } else {
	for (count = 0; certlists[count] != NULL; count++)
	    ;
	PORT_Assert (count);	/* should be at least one already */
	certlists = (CERTCertificateList**)PORT_ArenaGrow (cinfo->poolp, 
				 certlists,
				(count + 1) * sizeof(CERTCertificateList *),
				(count + 2) * sizeof(CERTCertificateList *));
    }

    if (certlists == NULL) {
	CERT_DestroyCertificateList (certlist);
	return SECFailure;
    }

    certlists[count] = certlist;
    certlists[count + 1] = NULL;

    *certlistsp = certlists;

    return SECSuccess;
}


/*
 * Helper function to add a certificate for inclusion in the bag of
 * certificates in a signedData.
 */
static SECStatus
sec_pkcs7_add_certificate (SEC_PKCS7ContentInfo *cinfo,
			   CERTCertificate *cert)
{
    SECOidTag kind;
    CERTCertificate **certs, ***certsp;
    int count;

    kind = SEC_PKCS7ContentType (cinfo);
    switch (kind) {
      case SEC_OID_PKCS7_SIGNED_DATA:
	{
	    SEC_PKCS7SignedData *sdp;

	    sdp = cinfo->content.signedData;
	    certsp = &(sdp->certs);
	}
	break;
      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
	{
	    SEC_PKCS7SignedAndEnvelopedData *saedp;

	    saedp = cinfo->content.signedAndEnvelopedData;
	    certsp = &(saedp->certs);
	}
	break;
      default:
	return SECFailure;		/* XXX set an error? */
    }

    cert = CERT_DupCertificate (cert);
    if (cert == NULL)
	return SECFailure;

    certs = *certsp;
    if (certs == NULL) {
	count = 0;
	certs = (CERTCertificate**)PORT_ArenaAlloc (cinfo->poolp, 
					      2 * sizeof(CERTCertificate *));
    } else {
	for (count = 0; certs[count] != NULL; count++)
	    ;
	PORT_Assert (count);	/* should be at least one already */
	certs = (CERTCertificate**)PORT_ArenaGrow (cinfo->poolp, certs,
				(count + 1) * sizeof(CERTCertificate *),
				(count + 2) * sizeof(CERTCertificate *));
    }

    if (certs == NULL) {
	CERT_DestroyCertificate (cert);
	return SECFailure;
    }

    certs[count] = cert;
    certs[count + 1] = NULL;

    *certsp = certs;

    return SECSuccess;
}


/*
 * Create a PKCS7 certs-only container.
 *
 * "cert" is the (first) cert that will be included.
 *
 * "include_chain" specifies whether the entire chain for "cert" should
 * be included.
 *
 * "certdb" is the cert database to use for finding the chain.
 * It can be NULL in when "include_chain" is false, or when meaning
 * use the default database.
 *
 * More certs and chains can be added via AddCertificate and AddCertChain.
 *
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
SEC_PKCS7ContentInfo *
SEC_PKCS7CreateCertsOnly (CERTCertificate *cert,
			  PRBool include_chain,
			  CERTCertDBHandle *certdb)
{
    SEC_PKCS7ContentInfo *cinfo;
    SECStatus rv;

    cinfo = sec_pkcs7_create_signed_data (NULL, NULL);
    if (cinfo == NULL)
	return NULL;

    if (include_chain)
	rv = sec_pkcs7_add_cert_chain (cinfo, cert, certdb);
    else
	rv = sec_pkcs7_add_certificate (cinfo, cert);

    if (rv != SECSuccess) {
	SEC_PKCS7DestroyContentInfo (cinfo);
	return NULL;
    }

    return cinfo;
}


/*
 * Add "cert" and its entire chain to the set of certs included in "cinfo".
 *
 * "certdb" is the cert database to use for finding the chain.
 * It can be NULL, meaning use the default database.
 *
 * "cinfo" should be of type signedData or signedAndEnvelopedData;
 * SECFailure will be returned if it is not.
 */
SECStatus
SEC_PKCS7AddCertChain (SEC_PKCS7ContentInfo *cinfo,
		       CERTCertificate *cert,
		       CERTCertDBHandle *certdb)
{
    SECOidTag kind;

    kind = SEC_PKCS7ContentType (cinfo);
    if (kind != SEC_OID_PKCS7_SIGNED_DATA
	&& kind != SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA)
	return SECFailure;		/* XXX set an error? */

    return sec_pkcs7_add_cert_chain (cinfo, cert, certdb);
}


/*
 * Add "cert" to the set of certs included in "cinfo".
 *
 * "cinfo" should be of type signedData or signedAndEnvelopedData;
 * SECFailure will be returned if it is not.
 */
SECStatus
SEC_PKCS7AddCertificate (SEC_PKCS7ContentInfo *cinfo, CERTCertificate *cert)
{
    SECOidTag kind;

    kind = SEC_PKCS7ContentType (cinfo);
    if (kind != SEC_OID_PKCS7_SIGNED_DATA
	&& kind != SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA)
	return SECFailure;		/* XXX set an error? */

    return sec_pkcs7_add_certificate (cinfo, cert);
}


static SECStatus
sec_pkcs7_init_encrypted_content_info (SEC_PKCS7EncryptedContentInfo *enccinfo,
				       PRArenaPool *poolp,
				       SECOidTag kind, PRBool detached,
				       SECOidTag encalg, int keysize)
{
    SECStatus rv;

    PORT_Assert (enccinfo != NULL && poolp != NULL);
    if (enccinfo == NULL || poolp == NULL)
	return SECFailure;

    /*
     * XXX Some day we may want to allow for other kinds.  That needs
     * more work and modifications to the creation interface, etc.
     * For now, allow but notice callers who pass in other kinds.
     * They are responsible for creating the inner type and encoding,
     * if it is other than DATA.
     */
    PORT_Assert (kind == SEC_OID_PKCS7_DATA);

    enccinfo->contentTypeTag = SECOID_FindOIDByTag (kind);
    PORT_Assert (enccinfo->contentTypeTag
	       && enccinfo->contentTypeTag->offset == kind);

    rv = SECITEM_CopyItem (poolp, &(enccinfo->contentType),
			   &(enccinfo->contentTypeTag->oid));
    if (rv != SECSuccess)
	return rv;

    /* Save keysize and algorithm for later. */
    enccinfo->keysize = keysize;
    enccinfo->encalg = encalg;

    return SECSuccess;
}


/*
 * Add a recipient to a PKCS7 thing, verifying their cert first.
 * Any error returns SECFailure.
 */
static SECStatus
sec_pkcs7_add_recipient (SEC_PKCS7ContentInfo *cinfo,
			 CERTCertificate *cert,
			 SECCertUsage certusage,
			 CERTCertDBHandle *certdb)
{
    SECOidTag kind;
    SEC_PKCS7RecipientInfo *recipientinfo, **recipientinfos, ***recipientinfosp;
    SECItem *dummy;
    void *mark;
    int count;

    kind = SEC_PKCS7ContentType (cinfo);
    switch (kind) {
      case SEC_OID_PKCS7_ENVELOPED_DATA:
	{
	    SEC_PKCS7EnvelopedData *edp;

	    edp = cinfo->content.envelopedData;
	    recipientinfosp = &(edp->recipientInfos);
	}
	break;
      case SEC_OID_PKCS7_SIGNED_ENVELOPED_DATA:
	{
	    SEC_PKCS7SignedAndEnvelopedData *saedp;

	    saedp = cinfo->content.signedAndEnvelopedData;
	    recipientinfosp = &(saedp->recipientInfos);
	}
	break;
      default:
	return SECFailure;		/* XXX set an error? */
    }

    /*
     * XXX I think that CERT_VerifyCert should do this if *it* is passed
     * a NULL database.
     */
    if (certdb == NULL) {
	certdb = CERT_GetDefaultCertDB();
	if (certdb == NULL)
	    return SECFailure;		/* XXX set an error? */
    }

    if (CERT_VerifyCert (certdb, cert, PR_TRUE, certusage, PR_Now(),
			 cinfo->pwfn_arg, NULL) != SECSuccess)
	{
	/* XXX Did CERT_VerifyCert set an error? */
	return SECFailure;
    }

    mark = PORT_ArenaMark (cinfo->poolp);

    recipientinfo = (SEC_PKCS7RecipientInfo*)PORT_ArenaZAlloc (cinfo->poolp,
				      sizeof(SEC_PKCS7RecipientInfo));
    if (recipientinfo == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }

    dummy = SEC_ASN1EncodeInteger (cinfo->poolp, &recipientinfo->version,
				   SEC_PKCS7_RECIPIENT_INFO_VERSION);
    if (dummy == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }
    PORT_Assert (dummy == &recipientinfo->version);

    recipientinfo->cert = CERT_DupCertificate (cert);
    if (recipientinfo->cert == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }

    recipientinfo->issuerAndSN = CERT_GetCertIssuerAndSN (cinfo->poolp, cert);
    if (recipientinfo->issuerAndSN == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }

    /*
     * Okay, now recipientinfo is all set.  We just need to put it into
     * the main structure.
     *
     * If this is the first recipient, allocate a new recipientinfos array;
     * otherwise, reallocate the array, making room for the new entry.
     */
    recipientinfos = *recipientinfosp;
    if (recipientinfos == NULL) {
	count = 0;
	recipientinfos = (SEC_PKCS7RecipientInfo **)PORT_ArenaAlloc (
					  cinfo->poolp,
					  2 * sizeof(SEC_PKCS7RecipientInfo *));
    } else {
	for (count = 0; recipientinfos[count] != NULL; count++)
	    ;
	PORT_Assert (count);	/* should be at least one already */
	recipientinfos = (SEC_PKCS7RecipientInfo **)PORT_ArenaGrow (
				 cinfo->poolp, recipientinfos,
				(count + 1) * sizeof(SEC_PKCS7RecipientInfo *),
				(count + 2) * sizeof(SEC_PKCS7RecipientInfo *));
    }

    if (recipientinfos == NULL) {
	PORT_ArenaRelease (cinfo->poolp, mark);
	return SECFailure;
    }

    recipientinfos[count] = recipientinfo;
    recipientinfos[count + 1] = NULL;

    *recipientinfosp = recipientinfos;

    PORT_ArenaUnmark (cinfo->poolp, mark);
    return SECSuccess;
}


/*
 * Start a PKCS7 enveloping context.
 *
 * "cert" is the cert for the recipient.  It will be checked for validity.
 *
 * "certusage" describes the encryption usage (e.g. certUsageEmailRecipient)
 * XXX Maybe SECCertUsage should be split so that our caller just says
 * "email" and *we* add the "recipient" part -- otherwise our caller
 * could be lying about the usage; we do not want to allow encryption
 * certs for signing or vice versa.
 *
 * "certdb" is the cert database to use for verifying the cert.
 * It can be NULL if a default database is available (like in the client).
 *
 * "encalg" specifies the bulk encryption algorithm to use (e.g. SEC_OID_RC2).
 *
 * "keysize" specifies the bulk encryption key size, in bits.
 *
 * The return value can be passed to functions which add things to
 * it like more recipients, then eventually to SEC_PKCS7Encode() or to
 * SEC_PKCS7EncoderStart() to create the encoded data, and finally to
 * SEC_PKCS7DestroyContentInfo().
 *
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
extern SEC_PKCS7ContentInfo *
SEC_PKCS7CreateEnvelopedData (CERTCertificate *cert,
			      SECCertUsage certusage,
			      CERTCertDBHandle *certdb,
			      SECOidTag encalg,
			      int keysize,
 			      SECKEYGetPasswordKey pwfn, void *pwfn_arg)
{
    SEC_PKCS7ContentInfo *cinfo;
    SEC_PKCS7EnvelopedData *envd;
    SECStatus rv;

    cinfo = sec_pkcs7_create_content_info (SEC_OID_PKCS7_ENVELOPED_DATA,
					   PR_FALSE, pwfn, pwfn_arg);
    if (cinfo == NULL)
	return NULL;

    rv = sec_pkcs7_add_recipient (cinfo, cert, certusage, certdb);
    if (rv != SECSuccess) {
	SEC_PKCS7DestroyContentInfo (cinfo);
	return NULL;
    }

    envd = cinfo->content.envelopedData;
    PORT_Assert (envd != NULL);

    /*
     * XXX Might we want to allow content types other than data?
     * If so, via what interface?
     */
    rv = sec_pkcs7_init_encrypted_content_info (&(envd->encContentInfo),
						cinfo->poolp,
						SEC_OID_PKCS7_DATA, PR_FALSE,
						encalg, keysize);
    if (rv != SECSuccess) {
	SEC_PKCS7DestroyContentInfo (cinfo);
	return NULL;
    }

    /* XXX Anything more to do here? */

    return cinfo;
}


/*
 * Add another recipient to an encrypted message.
 *
 * "cinfo" should be of type envelopedData or signedAndEnvelopedData;
 * SECFailure will be returned if it is not.
 *
 * "cert" is the cert for the recipient.  It will be checked for validity.
 *
 * "certusage" describes the encryption usage (e.g. certUsageEmailRecipient)
 * XXX Maybe SECCertUsage should be split so that our caller just says
 * "email" and *we* add the "recipient" part -- otherwise our caller
 * could be lying about the usage; we do not want to allow encryption
 * certs for signing or vice versa.
 *
 * "certdb" is the cert database to use for verifying the cert.
 * It can be NULL if a default database is available (like in the client).
 */
SECStatus
SEC_PKCS7AddRecipient (SEC_PKCS7ContentInfo *cinfo,
		       CERTCertificate *cert,
		       SECCertUsage certusage,
		       CERTCertDBHandle *certdb)
{
    return sec_pkcs7_add_recipient (cinfo, cert, certusage, certdb);
}


/*
 * Create an empty PKCS7 data content info.
 *
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
SEC_PKCS7ContentInfo *
SEC_PKCS7CreateData (void)
{
    return sec_pkcs7_create_content_info (SEC_OID_PKCS7_DATA, PR_FALSE,
					  NULL, NULL);
}


/*
 * Create an empty PKCS7 encrypted content info.
 *
 * "algorithm" specifies the bulk encryption algorithm to use.
 * 
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
SEC_PKCS7ContentInfo *
SEC_PKCS7CreateEncryptedData (SECOidTag algorithm, int keysize,
			      SECKEYGetPasswordKey pwfn, void *pwfn_arg)
{
    SEC_PKCS7ContentInfo *cinfo;
    SECAlgorithmID *algid;
    SEC_PKCS7EncryptedData *enc_data;
    SECStatus rv;

    cinfo = sec_pkcs7_create_content_info (SEC_OID_PKCS7_ENCRYPTED_DATA, 
					   PR_FALSE, pwfn, pwfn_arg);
    if (cinfo == NULL)
	return NULL;

    enc_data = cinfo->content.encryptedData;
    algid = &(enc_data->encContentInfo.contentEncAlg);

    if (!SEC_PKCS5IsAlgorithmPBEAlgTag(algorithm)) {
	rv = SECOID_SetAlgorithmID (cinfo->poolp, algid, algorithm, NULL);
    } else {
        /* Assume password-based-encryption.  
         * Note: we can't generate pkcs5v2 from this interface.
         * PK11_CreateBPEAlgorithmID generates pkcs5v2 by accepting
         * non-PBE oids and assuming that they are pkcs5v2 oids, but
         * NSS_CMSEncryptedData_Create accepts non-PBE oids as regular
         * CMS encrypted data, so we can't tell SEC_PKCS7CreateEncryptedtedData
         * to create pkcs5v2 PBEs */
	SECAlgorithmID *pbe_algid;
	pbe_algid = PK11_CreatePBEAlgorithmID(algorithm,
                                              NSS_PBE_DEFAULT_ITERATION_COUNT,
                                              NULL);
	if (pbe_algid == NULL) {
	    rv = SECFailure;
	} else {
	    rv = SECOID_CopyAlgorithmID (cinfo->poolp, algid, pbe_algid);
	    SECOID_DestroyAlgorithmID (pbe_algid, PR_TRUE);
	}
    }

    if (rv != SECSuccess) {
	SEC_PKCS7DestroyContentInfo (cinfo);
	return NULL;
    }

    rv = sec_pkcs7_init_encrypted_content_info (&(enc_data->encContentInfo),
						cinfo->poolp,
						SEC_OID_PKCS7_DATA, PR_FALSE,
						algorithm, keysize);
    if (rv != SECSuccess) {
	SEC_PKCS7DestroyContentInfo (cinfo);
	return NULL;
    }

    return cinfo;
}

