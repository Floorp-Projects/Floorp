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

/*
 * X.509 v3 Basic Constraints Extension 
 */

#include "prtypes.h"
#include "mcom_db.h"
#include "seccomon.h"
#include "secdert.h"
#include "secoidt.h"
#include "secasn1t.h"
#include "secasn1.h"
#include "certt.h"
#include "secder.h"
#include "prprf.h"
#include "secerr.h"

typedef struct EncodedContext{
    SECItem isCA;
    SECItem pathLenConstraint;
    SECItem encodedValue;
    PRArenaPool *arena;
}EncodedContext;

static const SEC_ASN1Template CERTBasicConstraintsTemplate[] = {
    { SEC_ASN1_SEQUENCE,
	  0, NULL, sizeof(EncodedContext) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_BOOLEAN,		/* XXX DER_DEFAULT */
	  offsetof(EncodedContext,isCA)},
    { SEC_ASN1_OPTIONAL | SEC_ASN1_INTEGER,
	  offsetof(EncodedContext,pathLenConstraint) },
    { 0, }
};

static unsigned char hexTrue = 0xff;
static unsigned char hexFalse = 0x00;

#define GEN_BREAK(status) rv = status; break;

SECStatus CERT_EncodeBasicConstraintValue
   (PRArenaPool *arena, CERTBasicConstraints *value, SECItem *encodedValue)
{
    EncodedContext encodeContext;
    PRArenaPool *our_pool = NULL;   
    SECStatus rv = SECSuccess;

    do {
	PORT_Memset (&encodeContext, 0, sizeof (encodeContext));
	if (!value->isCA && value->pathLenConstraint >= 0) {
	    PORT_SetError (SEC_ERROR_EXTENSION_VALUE_INVALID);
	    GEN_BREAK (SECFailure);
	}

        encodeContext.arena = arena;
	if (value->isCA == PR_TRUE) {
	    encodeContext.isCA.data =  &hexTrue ;
	    encodeContext.isCA.len = 1;
	}

	/* If the pathLenConstraint is less than 0, then it should be
	 * omitted from the encoding.
	 */
	if (value->isCA && value->pathLenConstraint >= 0) {
	    our_pool = PORT_NewArena (SEC_ASN1_DEFAULT_ARENA_SIZE);
	    if (our_pool == NULL) {
		PORT_SetError (SEC_ERROR_NO_MEMORY);
		GEN_BREAK (SECFailure);
	    }
	    if (SEC_ASN1EncodeUnsignedInteger
		(our_pool, &encodeContext.pathLenConstraint,
		 (unsigned long)value->pathLenConstraint) == NULL) {
		PORT_SetError (SEC_ERROR_NO_MEMORY);
		GEN_BREAK (SECFailure);
	    }
	}
	if (SEC_ASN1EncodeItem (arena, encodedValue, &encodeContext,
				CERTBasicConstraintsTemplate) == NULL) {
	    GEN_BREAK (SECFailure);
	}
    } while (0);
    if (our_pool)
	PORT_FreeArena (our_pool, PR_FALSE);
    return(rv);

}

SECStatus CERT_DecodeBasicConstraintValue
   (CERTBasicConstraints *value, SECItem *encodedValue)
{
    EncodedContext decodeContext;
    PRArenaPool *our_pool;
    SECStatus rv = SECSuccess;

    do {
	PORT_Memset (&decodeContext, 0, sizeof (decodeContext));
	/* initialize the value just in case we got "0x30 00", or when the
	   pathLenConstraint is omitted.
         */
	decodeContext.isCA.data =&hexFalse;
	decodeContext.isCA.len = 1;
	
	our_pool = PORT_NewArena (SEC_ASN1_DEFAULT_ARENA_SIZE);
	if (our_pool == NULL) {
	    PORT_SetError (SEC_ERROR_NO_MEMORY);
	    GEN_BREAK (SECFailure);
	}

	rv = SEC_ASN1DecodeItem
	     (our_pool, &decodeContext, CERTBasicConstraintsTemplate, encodedValue);
	if (rv == SECFailure)
	    break;
	
	value->isCA = (PRBool)(*decodeContext.isCA.data);
	if (decodeContext.pathLenConstraint.data == NULL) {
	    /* if the pathLenConstraint is not encoded, and the current setting
	      is CA, then the pathLenConstraint should be set to a negative number
	      for unlimited certificate path.
	     */
	    if (value->isCA)
		value->pathLenConstraint = CERT_UNLIMITED_PATH_CONSTRAINT;
	}
	else if (value->isCA)
	    value->pathLenConstraint = DER_GetUInteger (&decodeContext.pathLenConstraint);
	else {
	    /* here we get an error where the subject is not a CA, but
	       the pathLenConstraint is set */
	    PORT_SetError (SEC_ERROR_BAD_DER);
	    GEN_BREAK (SECFailure);
	    break;
	}
	 
    } while (0);
    PORT_FreeArena (our_pool, PR_FALSE);
    return (rv);

}
