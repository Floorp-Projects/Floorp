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

#ifndef PKIX_H
#define PKIX_H

#ifdef DEBUG
static const char PKIX_CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/include/Attic/pkix.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:02:56 $ $Name:  $";
#endif /* DEBUG */

/*
 * pkix.h
 *
 * This file contains the prototypes for the private methods defined
 * for the PKIX part-1 objects.
 */

#ifndef NSSPKIX_H
#include "nsspkix.h"
#endif /* NSSPKIX_H */

#ifndef PKIXT_H
#include "pkixt.h"
#endif /* PKIXT_H */

#ifndef ASN1T_H
#include "asn1t.h"
#endif /* ASN1T_H */

PR_BEGIN_EXTERN_C

/*
 * Attribute
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Attribute       ::=     SEQUENCE {
 *          type            AttributeType,
 *          values  SET OF AttributeValue
 *                  -- at least one value is required -- }
 *
 * The private calls for the type:
 * 
 *  nssPKIXAttribute_Decode
 *  nssPKIXAttribute_Create
 *  nssPKIXAttribute_CreateFromArray
 *  nssPKIXAttribute_Destroy
 *  nssPKIXAttribute_Encode
 *  nssPKIXAttribute_GetType
 *  nssPKIXAttribute_SetType
 *  nssPKIXAttribute_GetValueCount
 *  nssPKIXAttribute_GetValues
 *  nssPKIXAttribute_SetValues
 *  nssPKIXAttribute_GetValue
 *  nssPKIXAttribute_SetValue
 *  nssPKIXAttribute_AddValue
 *  nssPKIXAttribute_RemoveValue
 *  nssPKIXAttribute_FindValue
 *  nssPKIXAttribute_Equal
 *  nssPKIXAttribute_Duplicate
 *
 * In debug builds, the following call is available:
 *
 *  nssPKIXAttribute_verifyPointer
 */

/*
 * nssPKIXAttribute_template
 *
 *
 */

extern const nssASN1Template nssPKIXAttribute_template[];

/*
 * nssPKIXAttribute_Decode
 *
 * This routine creates an NSSPKIXAttribute by decoding a BER-
 * or DER-encoded Attribute as defined in RFC 2459.  This
 * routine may return NULL upon error, in which case it will
 * have created an error stack.  If the optional arena argument
 * is non-NULL, that arena will be used for the required memory.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttribute upon success
 *  NULL upon failure.
 */

NSS_EXTERN NSSPKIXAttribute *
nssPKIXAttribute_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXAttribute_Create
 *
 * This routine creates an NSSPKIXAttribute from specified components.
 * This routine may return NULL upon error, in which case it will have
 * created an error stack.  If the optional arena argument is non-NULL,
 * that arena will be used for the required memory.  There must be at
 * least one attribute value specified.  The final argument must be
 * NULL, to indicate the end of the set of attribute values.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttribute upon success
 *  NULL upon failure.
 */

NSS_EXTERN NSSPKIXAttribute *
nssPKIXAttribute_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeType *typeOid,
  NSSPKIXAttributeValue *value1,
  ...
);

/*
 * nssPKIXAttribute_CreateFromArray
 *
 * This routine creates an NSSPKIXAttribute from specified components.
 * This routine may return NULL upon error, in which case it will have
 * created an error stack.  If the optional arena argument is non-NULL,
 * that arena will be used for the required memory.  There must be at
 * least one attribute value specified.  The final argument must be
 * NULL, to indicate the end of the set of attribute values.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttribute upon success
 *  NULL upon failure.
 */

NSS_EXTERN NSSPKIXAttribute *
nssPKIXAttribute_CreateFromArray
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeType *typeOid,
  PRUint32 count,
  NSSPKIXAttributeValue values[]
);

/*
 * nssPKIXAttribute_Destroy
 *
 * This routine destroys an NSSPKIXAttribute.  It should be called on
 * all such objects created without an arena.  It does not need to be
 * called for objects created with an arena, but it may be.  This
 * routine returns a PRStatus value.  Upon error, it will create an
 * error stack and return PR_FAILURE.
 *
 * The error value may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAttribute_Destroy
(
  NSSPKIXAttribute *attribute
);

/*
 * nssPKIXAttribute_Encode
 *
 * This routine returns the BER encoding of the specified 
 * NSSPKIXAttribute.  {usual rules about itemOpt and arenaOpt}
 * This routine indicates an error (NSS_ERROR_INVALID_DATA) 
 * if there are no attribute values (i.e., the last one was removed).
 *
 * The error value may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_DATA
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXAttribute_Encode
(
  NSSPKIXAttribute *attribute,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAttribute_GetType
 *
 * This routine returns the attribute type oid of the specified
 * NSSPKIXAttribute.
 *
 * The error value may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSPKIXAttributeType pointer upon success
 *  NULL upon failure.
 */

NSS_EXTERN NSSPKIXAttributeType *
nssPKIXAttribute_GetType
(
  NSSPKIXAttribute *attribute
);

/*
 * nssPKIXAttribute_SetType
 *
 * This routine sets the attribute type oid of the indicated
 * NSSPKIXAttribute to the specified value.  Since attributes
 * may be application-defined, no checking can be done on
 * either the correctness of the attribute type oid value nor
 * the suitability of the set of attribute values.
 *
 * The error value may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_OID
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAttribute_SetType
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeType *attributeType
);

/*
 * nssPKIXAttribute_GetValueCount
 *
 * This routine returns the number of attribute values present in
 * the specified NSSPKIXAttribute.  This routine returns a PRInt32.
 * Upon error, this routine returns -1.  This routine indicates an
 * error if the number of values cannot be expressed as a PRInt32.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXAttribute_GetValueCount
(
  NSSPKIXAttribute *attribute
);

/*
 * nssPKIXAttribute_GetValues
 *
 * This routine returns all of the attribute values in the specified
 * NSSPKIXAttribute.  If the optional pointer to an array of NSSItems
 * is non-null, then that array will be used and returned; otherwise,
 * an array will be allocated and returned.  If the limit is nonzero
 * (which is must be if the specified array is nonnull), then an
 * error is indicated if it is smaller than the value count.
 * {arenaOpt}
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  A valid pointer to an array of NSSItem's upon success
 *  NULL upon failure.
 */

NSS_EXTERN NSSPKIXAttributeValue *
nssPKIXAttribute_GetValues
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeValue rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAttribute_SetValues
 *
 * This routine sets all of the values of the specified 
 * NSSPKIXAttribute to the values in the specified NSSItem array.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAttribute_SetValues
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeValue values[],
  PRInt32 count
);

/*
 * nssPKIXAttribute_GetValue
 *
 * This routine returns the i'th attribute value of the set of
 * values in the specified NSSPKIXAttribute.  Although the set
 * is unordered, an arbitrary ordering will be maintained until
 * the data in the attribute is changed.  {usual comments about
 * itemOpt and arenaOpt}
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttributeValue upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttributeValue *
nssPKIXAttribute_GetValue
(
  NSSPKIXAttribute *attribute,
  PRInt32 i,
  NSSPKIXAttributeValue *itemOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAttribute_SetValue
 *
 * This routine sets the i'th attribute value {blah blah; copies
 * memory contents over..}
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAttribute_SetValue
(
  NSSPKIXAttribute *attribute,
  PRInt32 i,
  NSSPKIXAttributeValue *value
);

/*
 * nssPKIXAttribute_AddValue
 *
 * This routine adds the specified attribute value to the set in
 * the specified NSSPKIXAttribute.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAttribute_AddValue
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeValue *value
); 

/*
 * nssPKIXAttribute_RemoveValue
 *
 * This routine removes the i'th attribute value of the set in the
 * specified NSSPKIXAttribute.  An attempt to remove the last value
 * will fail.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_AT_MINIMUM
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAttribute_RemoveValue
(
  NSSPKIXAttribute *attribute,
  PRInt32 i
);

/*
 * nssPKIXAttribute_FindValue
 *
 * This routine searches the set of attribute values in the specified
 * NSSPKIXAttribute for the provided data.  If an exact match is found,
 * then that value's index is returned.  If an exact match is not 
 * found, -1 is returned.  If there is more than one exact match, one
 * index will be returned.  {notes about unorderdness of SET, etc}
 * If the index may not be represented as an integer, an error is
 * indicated.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value
 *  The index of the specified attribute value upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXAttribute_FindValue
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeValue *attributeValue
);

/*
 * nssPKIXAttribute_Equal
 *
 * This routine compares two NSSPKIXAttribute's for equality.
 * It returns PR_TRUE if they are equal, PR_FALSE otherwise.
 * This routine also returns PR_FALSE upon error; if you're
 * that worried about it, check for an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXAttribute_Equal
(
  NSSPKIXAttribute *one,
  NSSPKIXAttribute *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXAttribute_Duplicate
 *
 * This routine duplicates an NSSPKIXAttribute.  {arenaOpt}
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttribute upon success
 *  NULL upon failure.
 */

NSS_EXTERN NSSPKIXAttribute *
nssPKIXAttribute_Duplicate
(
  NSSPKIXAttribute *attribute,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXAttribute_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXAttribute
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXAttribute_verifyPointer
(
  NSSPKIXAttribute *p
);
#endif /* DEBUG */

/*
 * AttributeTypeAndValue
 *
 * This structure contains an attribute type (indicated by an OID), 
 * and the type-specific value.  RelativeDistinguishedNames consist
 * of a set of these.  These are distinct from Attributes (which have
 * SET of values), from AttributeDescriptions (which have qualifiers
 * on the types), and from AttributeValueAssertions (which assert a
 * a value comparison under some matching rule).
 *
 * From RFC 2459:
 *
 *  AttributeTypeAndValue           ::=     SEQUENCE {
 *          type    AttributeType,
 *          value   AttributeValue }
 * 
 * The private calls for the type:
 *
 *  nssPKIXAttributeTypeAndValue_Decode
 *  nssPKIXAttributeTypeAndValue_CreateFromUTF8
 *  nssPKIXAttributeTypeAndValue_Create
 *  nssPKIXAttributeTypeAndValue_Destroy
 *  nssPKIXAttributeTypeAndValue_Encode
 *  nssPKIXAttributeTypeAndValue_GetUTF8Encoding
 *  nssPKIXAttributeTypeAndValue_GetType
 *  nssPKIXAttributeTypeAndValue_SetType
 *  nssPKIXAttributeTypeAndValue_GetValue
 *  nssPKIXAttributeTypeAndValue_SetValue
 *  nssPKIXAttributeTypeAndValue_Equal
 *  nssPKIXAttributeTypeAndValue_Duplicate
 *
 * In debug builds, the following call is available:
 *
 *  nssPKIXAttributeTypeAndValue_verifyPointer
 */

/*
 * nssPKIXAttributeTypeAndValue_Decode
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttributeTypeAndValue upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttributeTypeAndValue *
nssPKIXAttributeTypeAndValue_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXAttributeTypeAndValue_CreateFromUTF8
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_UNKNOWN_ATTRIBUTE
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttributeTypeAndValue upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttributeTypeAndValue *
nssPKIXAttributeTypeAndValue_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *string
);

/*
 * nssPKIXAttributeTypeAndValue_Create
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttributeTypeAndValue upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttributeTypeAndValue *
nssPKIXAttributeTypeAndValue_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeType *typeOid,
  NSSPKIXAttributeValue *value
);

/*
 * nssPKIXAttributeTypeAndValue_Destroy
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAttributeTypeAndValue_Destroy
(
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * nssPKIXAttributeTypeAndValue_Encode
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXAttributeTypeAndValue_Encode
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAttributeTypeAndValue_GetUTF8Encoding
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXAttributeTypeAndValue_GetUTF8Encoding
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAttributeTypeAndValue_GetType
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSPKIXAttributeType pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttributeType *
nssPKIXAttributeTypeAndValue_GetType
(
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * nssPKIXAttributeTypeAndValue_SetType
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_INVALID_OID
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAttributeTypeAndValue_SetType
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSPKIXAttributeType *attributeType
);

/*
 * nssPKIXAttributeTypeAndValue_GetValue
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSAttributeValue upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttributeValue *
nssPKIXAttributeTypeAndValue_GetValue
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSPKIXAttributeValue *itemOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAttributeTypeAndValue_SetValue
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAttributeTypeAndValue_SetValue
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSPKIXAttributeValue *value
);

/*
 * nssPKIXAttributeTypeAndValue_Equal
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXAttributeTypeAndValue_Equal
(
  NSSPKIXAttributeTypeAndValue *atav1,
  NSSPKIXAttributeTypeAndValue *atav2,
  PRStatus *statusOpt
);

/*
 * nssPKIXAttributeTypeAndValue_Duplicate
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttributeTypeAndValue upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttributeTypeAndValue *
nssPKIXAttributeTypeAndValue_Duplicate
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXAttributeTypeAndValue_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXAttributeTypeAndValue
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE_TYPE_AND_VALUE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXAttributeTypeAndValue_verifyPointer
(
  NSSPKIXAttributeTypeAndValue *p
);
#endif /* DEBUG */

/*
 * X520Name
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520name        ::= CHOICE {
 *        teletexString         TeletexString (SIZE (1..ub-name)),
 *        printableString       PrintableString (SIZE (1..ub-name)),
 *        universalString       UniversalString (SIZE (1..ub-name)),
 *        utf8String            UTF8String (SIZE (1..ub-name)),
 *        bmpString             BMPString (SIZE(1..ub-name))   }
 *
 *
 *  ub-name INTEGER ::=     32768
 *
 * The private calls for this type:
 *
 *  nssPKIXX520Name_Decode
 *  nssPKIXX520Name_CreateFromUTF8
 *  nssPKIXX520Name_Create
 *  nssPKIXX520Name_Destroy
 *  nssPKIXX520Name_Encode
 *  nssPKIXX520Name_GetUTF8Encoding
 *  nssPKIXX520Name_Equal
 *  nssPKIXX520Name_Duplicate
 *
 * In debug builds, the following call is available:
 *
 *  nssPKIXX520Name_verifyPointer
 */

/*
 * nssPKIXX520Name_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520Name upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520Name *
nssPKIXX520Name_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX520Name_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520Name upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520Name *
nssPKIXX520Name_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX520Name_Create
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_STRING_TYPE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXX520Name upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520Name *
nssPKIXX520Name_Create
(
  NSSArena *arenaOpt,
  nssStringType type,
  NSSItem *data
);

/*
 * nssPKIXX520Name_Destroy
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520Name_Destroy
(
  NSSPKIXX520Name *name
);

/*
 * nssPKIXX520Name_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520Name_Encode
(
  NSSPKIXX520Name *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXX520Name_GetUTF8Encoding
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXX520Name_GetUTF8Encoding
(
  NSSPKIXX520Name *name,
  NSSArena *arenaOpt
);

/*
 * nssPKIXX520Name_Equal
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXX520Name_Equal
(
  NSSPKIXX520Name *name1,
  NSSPKIXX520Name *name2,
  PRStatus *statusOpt
);

/*
 * nssPKIXX520Name_Duplicate
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520Name upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520Name *
nssPKIXX520Name_Duplicate
(
  NSSPKIXX520Name *name,
  NSSArena *arenaOpt
);

#ifdef DEBUG

/*
 * nssPKIXX520Name_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXX520Name
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_X520_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXX520Name_verifyPointer
(
  NSSPKIXX520Name *p
);

#endif /* DEBUG */

/*
 * X520CommonName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520CommonName  ::=      CHOICE {
 *        teletexString         TeletexString (SIZE (1..ub-common-name)),
 *        printableString       PrintableString (SIZE (1..ub-common-name)),
 *        universalString       UniversalString (SIZE (1..ub-common-name)),
 *        utf8String            UTF8String (SIZE (1..ub-common-name)),
 *        bmpString             BMPString (SIZE(1..ub-common-name))   }
 * 
 * 
 *  ub-common-name  INTEGER ::=     64
 *
 * The private calls for this type:
 *
 *
 *  nssPKIXX520CommonName_Decode
 *  nssPKIXX520CommonName_CreateFromUTF8
 *  nssPKIXX520CommonName_Create
 *  nssPKIXX520CommonName_Destroy
 *  nssPKIXX520CommonName_Encode
 *  nssPKIXX520CommonName_GetUTF8Encoding
 *  nssPKIXX520CommonName_Equal
 *  nssPKIXX520CommonName_Duplicate
 *
 * In debug builds, the following call is available:
 *
 *  nssPKIXX520CommonName_verifyPointer
 */

/*
 * nssPKIXX520CommonName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520CommonName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520CommonName *
nssPKIXX520CommonName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX520CommonName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520CommonName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520CommonName *
nssPKIXX520CommonName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX520CommonName_Create
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_STRING_TYPE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXX520CommonName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520CommonName *
nssPKIXX520CommonName_Create
(
  NSSArena *arenaOpt,
  nssStringType type,
  NSSItem *data
);

/*
 * nssPKIXX520CommonName_Destroy
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520CommonName_Destroy
(
  NSSPKIXX520CommonName *name
);

/*
 * nssPKIXX520CommonName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520CommonName_Encode
(
  NSSPKIXX520CommonName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXX520CommonName_GetUTF8Encoding
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXX520CommonName_GetUTF8Encoding
(
  NSSPKIXX520CommonName *name,
  NSSArena *arenaOpt
);

/*
 * nssPKIXX520CommonName_Equal
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXX520CommonName_Equal
(
  NSSPKIXX520CommonName *name1,
  NSSPKIXX520CommonName *name2,
  PRStatus *statusOpt
);

/*
 * nssPKIXX520CommonName_Duplicate
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520CommonName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520CommonName *
nssPKIXX520CommonName_Duplicate
(
  NSSPKIXX520CommonName *name,
  NSSArena *arenaOpt
);

#ifdef DEBUG

/*
 * nssPKIXX520CommonName_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXX520CommonName
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_X520_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXX520CommonName_verifyPointer
(
  NSSPKIXX520CommonName *p
);

#endif /* DEBUG */

/*
 * X520LocalityName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520LocalityName ::= CHOICE {
 *        teletexString       TeletexString (SIZE (1..ub-locality-name)),
 *        printableString     PrintableString (SIZE (1..ub-locality-name)),
 *        universalString     UniversalString (SIZE (1..ub-locality-name)),
 *        utf8String          UTF8String (SIZE (1..ub-locality-name)),
 *        bmpString           BMPString (SIZE(1..ub-locality-name))   }
 * 
 * The private calls for this type:
 *
 *  nssPKIXX520LocalityName_Decode
 *  nssPKIXX520LocalityName_CreateFromUTF8
 *  nssPKIXX520LocalityName_Encode
 *
 */

/*
 * nssPKIXX520LocalityName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520LocalityName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520LocalityName *
nssPKIXX520LocalityName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX520LocalityName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520LocalityName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520LocalityName *
nssPKIXX520LocalityName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX520LocalityName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520LocalityName_Encode
(
  NSSPKIXX520LocalityName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * X520StateOrProvinceName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520StateOrProvinceName         ::= CHOICE {
 *        teletexString       TeletexString (SIZE (1..ub-state-name)),
 *        printableString     PrintableString (SIZE (1..ub-state-name)),
 *        universalString     UniversalString (SIZE (1..ub-state-name)),
 *        utf8String          UTF8String (SIZE (1..ub-state-name)),
 *        bmpString           BMPString (SIZE(1..ub-state-name))   }
 *
 * The private calls for this type:
 *
 *  nssPKIXX520StateOrProvinceName_Decode
 *  nssPKIXX520StateOrProvinceName_CreateFromUTF8
 *  nssPKIXX520StateOrProvinceName_Encode
 *
 */

/*
 * nssPKIXX520StateOrProvinceName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520StateOrProvinceName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520StateOrProvinceName *
nssPKIXX520StateOrProvinceName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX520StateOrProvinceName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520StateOrProvinceName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520StateOrProvinceName *
nssPKIXX520StateOrProvinceName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX520StateOrProvinceName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520StateOrProvinceName_Encode
(
  NSSPKIXX520StateOrProvinceName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * X520OrganizationName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520OrganizationName ::= CHOICE {
 *    teletexString     TeletexString (SIZE (1..ub-organization-name)),
 *    printableString   PrintableString (SIZE (1..ub-organization-name)),
 *    universalString   UniversalString (SIZE (1..ub-organization-name)),
 *    utf8String        UTF8String (SIZE (1..ub-organization-name)),
 *    bmpString         BMPString (SIZE(1..ub-organization-name))   }
 *
 * The private calls for this type:
 *
 *  nssPKIXX520OrganizationName_Decode
 *  nssPKIXX520OrganizationName_CreateFromUTF8
 *  nssPKIXX520OrganizationName_Encode
 *
 */

/*
 * nssPKIXX520OrganizationName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520OrganizationName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520OrganizationName *
nssPKIXX520OrganizationName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX520OrganizationName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520OrganizationName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520OrganizationName *
nssPKIXX520OrganizationName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX520OrganizationName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520OrganizationName_Encode
(
  NSSPKIXX520OrganizationName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * X520OrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520OrganizationalUnitName ::= CHOICE {
 *   teletexString    TeletexString (SIZE (1..ub-organizational-unit-name)),
 *   printableString        PrintableString
 *                        (SIZE (1..ub-organizational-unit-name)),
 *   universalString        UniversalString
 *                        (SIZE (1..ub-organizational-unit-name)),
 *   utf8String       UTF8String (SIZE (1..ub-organizational-unit-name)),
 *   bmpString        BMPString (SIZE(1..ub-organizational-unit-name))   }
 *
 * The private calls for this type:
 *
 *  nssPKIXX520OrganizationalUnitName_Decode
 *  nssPKIXX520OrganizationalUnitName_CreateFromUTF8
 *  nssPKIXX520OrganizationalUnitName_Encode
 *
 */

/*
 * nssPKIXX520OrganizationalUnitName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520OrganizationalUnitName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520OrganizationalUnitName *
nssPKIXX520OrganizationalUnitName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX520OrganizationalUnitName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520OrganizationalUnitName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520OrganizationalUnitName *
nssPKIXX520OrganizationalUnitName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX520OrganizationalUnitName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520OrganizationalUnitName_Encode
(
  NSSPKIXX520OrganizationalUnitName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * X520Title
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520Title ::=   CHOICE {
 *        teletexString         TeletexString (SIZE (1..ub-title)),
 *        printableString       PrintableString (SIZE (1..ub-title)),
 *        universalString       UniversalString (SIZE (1..ub-title)),
 *        utf8String            UTF8String (SIZE (1..ub-title)),
 *        bmpString             BMPString (SIZE(1..ub-title))   }
 *
 * The private calls for this type:
 *
 *  nssPKIXX520Title_Decode
 *  nssPKIXX520Title_CreateFromUTF8
 *  nssPKIXX520Title_Encode
 *
 */

/*
 * nssPKIXX520Title_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520Title upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520Title *
nssPKIXX520Title_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX520Title_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520Title upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520Title *
nssPKIXX520Title_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX520Title_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520Title_Encode
(
  NSSPKIXX520Title *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * X520dnQualifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520dnQualifier ::=     PrintableString
 *
 * The private calls for this type:
 *
 *  nssPKIXX520dnQualifier_Decode
 *  nssPKIXX520dnQualifier_CreateFromUTF8
 *  nssPKIXX520dnQualifier_Encode
 *
 */

/*
 * nssPKIXX520dnQualifier_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520dnQualifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520dnQualifier *
nssPKIXX520dnQualifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX520dnQualifier_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520dnQualifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520dnQualifier *
nssPKIXX520dnQualifier_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX520dnQualifier_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520dnQualifier_Encode
(
  NSSPKIXX520dnQualifier *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * X520countryName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X520countryName ::=     PrintableString (SIZE (2)) -- IS 3166 codes
 *
 * The private calls for this type:
 *
 *  nssPKIXX520countryName_Decode
 *  nssPKIXX520countryName_CreateFromUTF8
 *  nssPKIXX520countryName_Encode
 *
 */

/*
 * nssPKIXX520countryName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520countryName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520countryName *
nssPKIXX520countryName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX520countryName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520countryName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520countryName *
nssPKIXX520countryName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX520countryName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX520countryName_Encode
(
  NSSPKIXX520countryName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * Pkcs9email
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Pkcs9email ::= IA5String (SIZE (1..ub-emailaddress-length))
 *
 * The private calls for this type:
 *
 *  nssPKIXPkcs9email_Decode
 *  nssPKIXPkcs9email_CreateFromUTF8
 *  nssPKIXPkcs9email_Encode
 *
 */

/*
 * nssPKIXPkcs9email_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPkcs9email upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPkcs9email *
nssPKIXPkcs9email_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPkcs9email_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPkcs9email upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPkcs9email *
nssPKIXPkcs9email_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXPkcs9email_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPkcs9email_Encode
(
  NSSPKIXPkcs9email *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * Name
 *
 * This structure contains a union of the possible name formats,
 * which at the moment is limited to an RDNSequence.
 *
 * From RFC 2459:
 *
 *  Name            ::=   CHOICE { -- only one possibility for now --
 *                                   rdnSequence  RDNSequence }
 *
 * The private calls for this type:
 *
 *  nssPKIXName_Decode
 *  nssPKIXName_CreateFromUTF8
 *  nssPKIXName_Create
 *  nssPKIXName_CreateFromRDNSequence
 *  nssPKIXName_Destroy
 *  nssPKIXName_Encode
 *  nssPKIXName_GetUTF8Encoding
 *  nssPKIXName_GetChoice
 *  nssPKIXName_GetRDNSequence
 *  nssPKIXName_GetSpecifiedChoice {fgmr remove this}
{fgmr} _SetRDNSequence
{fgmr} _SetSpecifiedChoice
 *  nssPKIXName_Equal
 *  nssPKIXName_Duplicate
 *
 *  (here is where I had specific attribute value gettors in pki1)
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXName_verifyPointer
 *
 */

/*
 * nssPKIXName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXName *
nssPKIXName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXName_CreateFromUTF8
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_UNKNOWN_ATTRIBUTE
 *
 * Return value:
 *  A valid pointer to an NSSPKIXName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXName *
nssPKIXName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *string
);

/*
 * nssPKIXName_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_CHOICE
 *  NSS_ERROR_INVALID_ARGUMENT
 *
 * Return value:
 *  A valid pointer to an NSSPKIXName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXName *
nssPKIXName_Create
(
  NSSArena *arenaOpt,
  NSSPKIXNameChoice choice,
  void *arg
);

/*
 * nssPKIXName_CreateFromRDNSequence
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *
 * Return value:
 *  A valid pointer to an NSSPKIXName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXName *
nssPKIXName_CreateFromRDNSequence
(
  NSSArena *arenaOpt,
  NSSPKIXRDNSequence *rdnSequence
);

/*
 * nssPKIXName_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXName_Destroy
(
  NSSPKIXName *name
);

/*
 * nssPKIXName_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXName_Encode
(
  NSSPKIXName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXName_GetUTF8Encoding
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXName_GetUTF8Encoding
(
  NSSPKIXName *name,
  NSSArena *arenaOpt
);

/*
 * nssPKIXName_GetChoice
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME
 *
 * Return value:
 *  A valid element of the NSSPKIXNameChoice enumeration upon success
 *  The value NSSPKIXNameChoice_NSSinvalid (-1) upon error
 */

NSS_EXTERN NSSPKIXNameChoice
nssPKIXName_GetChoice
(
  NSSPKIXName *name
);

/*
 * nssPKIXName_GetRDNSequence
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_WRONG_CHOICE
 *
 * Return value:
 *  A pointer to a valid NSSPKIXRDNSequence upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRDNSequence *
nssPKIXName_GetRDNSequence
(
  NSSPKIXName *name,
  NSSArena *arenaOpt
);

/*
 * nssPKIXName_GetSpecifiedChoice
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_WRONG_CHOICE
 *
 * Return value:
 *  A valid pointer ...
 *  NULL upon failure
 */

NSS_EXTERN void *
nssPKIXName_GetSpecifiedChoice
(
  NSSPKIXName *name,
  NSSPKIXNameChoice choice,
  NSSArena *arenaOpt
);

/*
 * nssPKIXName_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXName_Equal
(
  NSSPKIXName *name1,
  NSSPKIXName *name2,
  PRStatus *statusOpt
);

/*
 * nssPKIXName_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXName *
nssPKIXName_Duplicate
(
  NSSPKIXName *name,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXName_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXName
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXName_verifyPointer
(
  NSSPKIXName *p
);
#endif /* DEBUG */

/*
 * RDNSequence
 *
 * This structure contains a sequence of RelativeDistinguishedName
 * objects.
 *
 * From RFC 2459:
 *
 *  RDNSequence     ::=   SEQUENCE OF RelativeDistinguishedName
 *
 * The private calls for this type:
 *
 *  nssPKIXRDNSequence_Decode
 *  nssPKIXRDNSequence_CreateFromUTF8
 *  nssPKIXRDNSequence_Create
 *  nssPKIXRDNSequence_CreateFromArray
 *  nssPKIXRDNSequence_Destroy
 *  nssPKIXRDNSequence_Encode
 *  nssPKIXRDNSequence_GetUTF8Encoding
 *  nssPKIXRDNSequence_GetRelativeDistinguishedNameCount
 *  nssPKIXRDNSequence_GetRelativeDistinguishedNames
 *  nssPKIXRDNSequence_SetRelativeDistinguishedNames
 *  nssPKIXRDNSequence_GetRelativeDistinguishedName
 *  nssPKIXRDNSequence_SetRelativeDistinguishedName
 *  nssPKIXRDNSequence_AppendRelativeDistinguishedName
 *  nssPKIXRDNSequence_InsertRelativeDistinguishedName
 *  nssPKIXRDNSequence_RemoveRelativeDistinguishedName
 *  nssPKIXRDNSequence_FindRelativeDistinguishedName
 *  nssPKIXRDNSequence_Equal
 *  nssPKIXRDNSequence_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXRDNSequence_verifyPointer
 *
 */

/*
 * nssPKIXRDNSequence_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRDNSequence upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRDNSequence *
nssPKIXRDNSequence_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXRDNSequence_CreateFromUTF8
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_UNKNOWN_ATTRIBUTE
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRDNSequence upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRDNSequence *
nssPKIXRDNSequence_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *string
);

/*
 * nssPKIXRDNSequence_CreateFromArray
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_RDN
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRDNSequence upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRDNSequence *
nssPKIXRDNSequence_CreateFromArray
(
  NSSArena *arenaOpt,
  PRUint32 count,
  NSSPKIXRelativeDistinguishedName *rdn1
);

/*
 * nssPKIXRDNSequence_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_RDN
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRDNSequence upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRDNSequence *
nssPKIXRDNSequence_Create
(
  NSSArena *arenaOpt,
  NSSPKIXRelativeDistinguishedName *rdn1,
  ...
);

/*
 * nssPKIXRDNSequence_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRDNSequence_Destroy
(
  NSSPKIXRDNSequence *rdnseq
);

/*
 * nssPKIXRDNSequence_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXRDNSequence_Encode
(
  NSSPKIXRDNSequence *rdnseq,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXRDNSequence_GetUTF8Encoding
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXRDNSequence_GetUTF8Encoding
(
  NSSPKIXRDNSequence *rdnseq,
  NSSArena *arenaOpt
);

/*
 * nssPKIXRDNSequence_GetRelativeDistinguishedNameCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXRDNSequence_GetRelativeDistinguishedNameCount
(
  NSSPKIXRDNSequence *rdnseq
);

/*
 * nssPKIXRDNSequence_GetRelativeDistinguishedNames
 *
 * This routine returns all of the relative distinguished names in the
 * specified RDN Sequence.  {...} If the array is allocated, or if the
 * specified one has extra space, the array will be null-terminated.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  A valid pointer to an array of NSSPKIXRelativeDistinguishedName 
 *      pointers upon success
 *  NULL upon failure.
 */

NSS_EXTERN NSSPKIXRelativeDistinguishedName **
nssPKIXRDNSequence_GetRelativeDistinguishedNames
(
  NSSPKIXRDNSequence *rdnseq,
  NSSPKIXRelativeDistinguishedName *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXRDNSequence_SetRelativeDistinguishedNames
 *
 * -- fgmr comments --
 * If the array pointer itself is null, the set is considered empty.
 * If the count is zero but the pointer nonnull, the array will be
 * assumed to be null-terminated.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_INVALID_PKIX_RDN
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRDNSequence_SetRelativeDistinguishedNames
(
  NSSPKIXRDNSequence *rdnseq,
  NSSPKIXRelativeDistinguishedName *rdns[],
  PRInt32 countOpt
);

/*
 * nssPKIXRDNSequence_GetRelativeDistinguishedName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRelativeDistinguishedName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRelativeDistinguishedName *
nssPKIXRDNSequence_GetRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXRDNSequence_SetRelativeDistinguishedName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRDNSequence_SetRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  PRInt32 i,
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * nssPKIXRDNSequence_AppendRelativeDistinguishedName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRDNSequence_AppendRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * nssPKIXRDNSequence_InsertRelativeDistinguishedName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRDNSequence_InsertRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  PRInt32 i,
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * nssPKIXRDNSequence_RemoveRelativeDistinguishedName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRDNSequence_RemoveRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  PRInt32 i
);

/*
 * nssPKIXRDNSequence_FindRelativeDistinguishedName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  The index of the specified attribute value upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXRDNSequence_FindRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * nssPKIXRDNSequence_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXRDNSequence_Equal
(
  NSSPKIXRDNSequence *one,
  NSSPKIXRDNSequence *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXRDNSequence_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRDNSequence upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRDNSequence *
nssPKIXRDNSequence_Duplicate
(
  NSSPKIXRDNSequence *rdnseq,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXRDNSequence_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXRDNSequence
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN_SEQUENCE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXRDNSequence_verifyPointer
(
  NSSPKIXRDNSequence *p
);
#endif /* DEBUG */

/*
 * DistinguishedName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DistinguishedName       ::=   RDNSequence
 *
 * This is just a public typedef; no new methods are required. {fgmr-- right?}
 */

/*
 * RelativeDistinguishedName
 *
 * This structure contains an unordered set of AttributeTypeAndValue 
 * objects.  RDNs are used to distinguish a set of objects underneath 
 * a common object.
 *
 * Often, a single ATAV is sufficient to make a unique distinction.
 * For example, if a company assigns its people unique uid values,
 * then in the Name "uid=smith,ou=People,o=Acme,c=US" the "uid=smith"
 * ATAV by itself forms an RDN.  However, sometimes a set of ATAVs is
 * needed.  For example, if a company needed to distinguish between
 * two Smiths by specifying their corporate divisions, then in the
 * Name "(cn=Smith,ou=Sales),ou=People,o=Acme,c=US" the parenthesised
 * set of ATAVs forms the RDN.
 *
 * From RFC 2459:
 *
 *  RelativeDistinguishedName  ::=
 *                      SET SIZE (1 .. MAX) OF AttributeTypeAndValue
 *
 * The private calls for this type:
 *
 *  nssPKIXRelativeDistinguishedName_Decode
 *  nssPKIXRelativeDistinguishedName_CreateFromUTF8
 *  nssPKIXRelativeDistinguishedName_Create
 *  nssPKIXRelativeDistinguishedName_CreateFromArray
 *  nssPKIXRelativeDistinguishedName_Destroy
 *  nssPKIXRelativeDistinguishedName_Encode
 *  nssPKIXRelativeDistinguishedName_GetUTF8Encoding
 *  nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValueCount
 *  nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValues
 *  nssPKIXRelativeDistinguishedName_SetAttributeTypeAndValues
 *  nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValue
 *  nssPKIXRelativeDistinguishedName_SetAttributeTypeAndValue
 *  nssPKIXRelativeDistinguishedName_AddAttributeTypeAndValue
 *  nssPKIXRelativeDistinguishedName_RemoveAttributeTypeAndValue
 *  nssPKIXRelativeDistinguishedName_FindAttributeTypeAndValue
 *  nssPKIXRelativeDistinguishedName_Equal
 *  nssPKIXRelativeDistinguishedName_Duplicate
 * 
 * fgmr: Logical additional functions include
 *
 *  NSSPKIXRelativeDistinguishedName_FindAttributeTypeAndValueByType
 *    returns PRInt32
 *  NSSPKIXRelativeDistinguishedName_FindAttributeTypeAndValuesByType
 *    returns array of PRInt32
 *  NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValueForType
 *    returns NSSPKIXAttributeTypeAndValue
 *  NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValuesForType
 *    returns array of NSSPKIXAttributeTypeAndValue
 *  NSSPKIXRelativeDistinguishedName_GetAttributeValueForType
 *    returns NSSPKIXAttributeValue
 *  NSSPKIXRelativeDistinguishedName_GetAttributeValuesForType
 *    returns array of NSSPKIXAttributeValue
 *
 * NOTE: the "return array" versions are only meaningful if an RDN may
 * contain multiple ATAVs with the same type.  Verify in the RFC if
 * this is possible or not.  If not, nuke those three functions.
 *
 * In debug builds, the following call is available:
 *
 *  nssPKIXRelativeDistinguishedName_verifyPointer
 *
 */

/*
 * nssPKIXRelativeDistinguishedName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRelativeDistinguishedName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRelativeDistinguishedName *
nssPKIXRelativeDistinguishedName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXRelativeDistinguishedName_CreateFromUTF8
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_UNKNOWN_ATTRIBUTE
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRelativeDistinguishedName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRelativeDistinguishedName *
nssPKIXRelativeDistinguishedName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *string
);

/*
 * nssPKIXRelativeDistinguishedName_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_ATAV
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRelativeDistinguishedName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRelativeDistinguishedName *
nssPKIXRelativeDistinguishedName_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeTypeAndValue *atav1,
  ...
);

/*
 * nssPKIXRelativeDistinguishedName_CreateFromArray
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_ATAV
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRelativeDistinguishedName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRelativeDistinguishedName *
nssPKIXRelativeDistinguishedName_CreateFromArray
(
  NSSArena *arenaOpt,
  PRUint32 count,
  NSSPKIXAttributeTypeAndValue *atavs
);

/*
 * nssPKIXRelativeDistinguishedName_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRelativeDistinguishedName_Destroy
(
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * nssPKIXRelativeDistinguishedName_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXRelativeDistinguishedName_Encode
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXRelativeDistinguishedName_GetUTF8Encoding
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXRelativeDistinguishedName_GetUTF8Encoding
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSArena *arenaOpt
);

/*
 * nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValueCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValueCount
(
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValues
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  A valid pointer to an array of NSSPKIXAttributeTypeAndValue 
 *      pointers upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttributeTypeAndValue **
nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValues
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXRelativeDistinguishedName_SetAttributeTypeAndValues
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_INVALID_ATAV
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRelativeDistinguishedName_SetAttributeTypeAndValues
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *atavs[],
  PRInt32 countOpt
);

/*
 * nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAttributeTypeAndValue upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttributeTypeAndValue *
nssPKIXRelativeDistinguishedName_GetAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXRelativeDistinguishedName_SetAttributeTypeAndValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRelativeDistinguishedName_SetAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  PRInt32 i,
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * nssPKIXRelativeDistinguishedName_AddAttributeTypeAndValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRelativeDistinguishedName_AddAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * nssPKIXRelativeDistinguishedName_RemoveAttributeTypeAndValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_AT_MINIMUM
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXRelativeDistinguishedName_RemoveAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  PRInt32 i
);

/*
 * nssPKIXRelativeDistinguishedName_FindAttributeTypeAndValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_INVALID_PKIX_ATAV
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  The index of the specified attribute value upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXRelativeDistinguishedName_FindAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * nssPKIXRelativeDistinguishedName_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXRelativeDistinguishedName_Equal
(
  NSSPKIXRelativeDistinguishedName *one,
  NSSPKIXRelativeDistinguishedName *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXRelativeDistinguishedName_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RDN
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXRelativeDistinguishedName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRelativeDistinguishedName *
nssPKIXRelativeDistinguishedName_Duplicate
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXRelativeDistinguishedName_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXRelativeDistinguishedName
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_RELATIVE_DISTINGUISHED_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXRelativeDistinguishedName_verifyPointer
(
  NSSPKIXRelativeDistinguishedName *p
);
#endif /* DEBUG */

/*
 * DirectoryString
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DirectoryString ::= CHOICE {
 *        teletexString             TeletexString (SIZE (1..MAX)),
 *        printableString           PrintableString (SIZE (1..MAX)),
 *        universalString           UniversalString (SIZE (1..MAX)),
 *        utf8String              UTF8String (SIZE (1..MAX)),
 *        bmpString               BMPString (SIZE(1..MAX))   }
 *
 * The private calls for this type:
 *
 *  nssPKIXDirectoryString_Decode
 *  nssPKIXDirectoryString_CreateFromUTF8
 *  nssPKIXDirectoryString_Encode
 *
 */

/*
 * nssPKIXDirectoryString_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDirectoryString upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDirectoryString *
nssPKIXDirectoryString_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXDirectoryString_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDirectoryString upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDirectoryString *
nssPKIXDirectoryString_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXDirectoryString_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_DIRECTORY_STRING
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXDirectoryString_Encode
(
  NSSPKIXDirectoryString *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * Certificate
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Certificate  ::=  SEQUENCE  {
 *       tbsCertificate       TBSCertificate,
 *       signatureAlgorithm   AlgorithmIdentifier,
 *       signature            BIT STRING  }
 *
 * The private calls for this type:
 *
 *  nssPKIXCertificate_Decode
 *  nssPKIXCertificate_Create
 *  nssPKIXCertificate_Destroy
 *  nssPKIXCertificate_Encode
 *  nssPKIXCertificate_GetTBSCertificate
 *  nssPKIXCertificate_SetTBSCertificate
 *  nssPKIXCertificate_GetAlgorithmIdentifier
 *  nssPKIXCertificate_SetAlgorithmIdentifier
 *  nssPKIXCertificate_GetSignature
 *  nssPKIXCertificate_SetSignature
 *  nssPKIXCertificate_Equal
 *  nssPKIXCertificate_Duplicate
 *
 *  { inherit TBSCertificate gettors? }
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXCertificate_verifyPointer
 *
 */

/*
 * nssPKIXCertificate_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificate *
nssPKIXCertificate_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXCertificate_Create
 *
 * -- fgmr comments --
 * { at this level we'll have to just accept a specified signature }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_PKIX_ALGID
 *  NSS_ERROR_INVALID_PKIX_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificate *
nssPKIXCertificate_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXAlgorithmIdentifier *algID,
  NSSItem *signature
);

/*
 * nssPKIXCertificate_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificate_Destroy
(
  NSSPKIXCertificate *cert
);

/*
 * nssPKIXCertificate_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXCertificate_Encode
(
  NSSPKIXCertificate *cert,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificate_GetTBSCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTBSCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTBSCertificate *
nssPKIXCertificate_GetTBSCertificate
(
  NSSPKIXCertificate *cert,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificate_SetTBSCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificate_SetTBSCertificate
(
  NSSPKIXCertificate *cert,
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * nssPKIXCertificate_GetAlgorithmIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAlgorithmIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAlgorithmIdentifier *
nssPKIXCertificate_GetAlgorithmIdentifier
(
  NSSPKIXCertificate *cert,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificate_SetAlgorithmIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificate_SetAlgorithmIdentifier
(
  NSSPKIXCertificate *cert,
  NSSPKIXAlgorithmIdentifier *algid,
);

/*
 * nssPKIXCertificate_GetSignature
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSItem *
nssPKIXCertificate_GetSignature
(
  NSSPKIXCertificate *cert,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificate_SetSignature
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificate_SetSignature
(
  NSSPKIXCertificate *cert,
  NSSItem *signature
);

/*
 * nssPKIXCertificate_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXCertificate_Equal
(
  NSSPKIXCertificate *one,
  NSSPKIXCertificate *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXCertificate_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificate *
nssPKIXCertificate_Duplicate
(
  NSSPKIXCertificate *cert,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXCertificate_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXCertificate
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXCertificate_verifyPointer
(
  NSSPKIXCertificate *p
);
#endif /* DEBUG */

/*
 * TBSCertificate
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TBSCertificate  ::=  SEQUENCE  {
 *       version         [0]  Version DEFAULT v1,
 *       serialNumber         CertificateSerialNumber,
 *       signature            AlgorithmIdentifier,
 *       issuer               Name,
 *       validity             Validity,
 *       subject              Name,
 *       subjectPublicKeyInfo SubjectPublicKeyInfo,
 *       issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
 *                            -- If present, version shall be v2 or v3
 *       subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
 *                            -- If present, version shall be v2 or v3
 *       extensions      [3]  Extensions OPTIONAL
 *                            -- If present, version shall be v3 --  }
 *
 * The private calls for this type:
 *
 *  nssPKIXTBSCertificate_Decode
 *  nssPKIXTBSCertificate_Create
 *  nssPKIXTBSCertificate_Destroy
 *  nssPKIXTBSCertificate_Encode
 *  nssPKIXTBSCertificate_GetVersion
 *  nssPKIXTBSCertificate_SetVersion
 *  nssPKIXTBSCertificate_GetSerialNumber
 *  nssPKIXTBSCertificate_SetSerialNumber
 *  nssPKIXTBSCertificate_GetSignature
 *  nssPKIXTBSCertificate_SetSignature
 *    { inherit algid gettors? }
 *  nssPKIXTBSCertificate_GetIssuer
 *  nssPKIXTBSCertificate_SetIssuer
 *    { inherit "helper" issuer gettors? }
 *  nssPKIXTBSCertificate_GetValidity
 *  nssPKIXTBSCertificate_SetValidity
 *    { inherit validity accessors }
 *  nssPKIXTBSCertificate_GetSubject
 *  nssPKIXTBSCertificate_SetSubject
 *  nssPKIXTBSCertificate_GetSubjectPublicKeyInfo
 *  nssPKIXTBSCertificate_SetSubjectPublicKeyInfo
 *  nssPKIXTBSCertificate_HasIssuerUniqueID
 *  nssPKIXTBSCertificate_GetIssuerUniqueID
 *  nssPKIXTBSCertificate_SetIssuerUniqueID
 *  nssPKIXTBSCertificate_RemoveIssuerUniqueID
 *  nssPKIXTBSCertificate_HasSubjectUniqueID
 *  nssPKIXTBSCertificate_GetSubjectUniqueID
 *  nssPKIXTBSCertificate_SetSubjectUniqueID
 *  nssPKIXTBSCertificate_RemoveSubjectUniqueID
 *  nssPKIXTBSCertificate_HasExtensions
 *  nssPKIXTBSCertificate_GetExtensions
 *  nssPKIXTBSCertificate_SetExtensions
 *  nssPKIXTBSCertificate_RemoveExtensions
 *    { extension accessors }
 *  nssPKIXTBSCertificate_Equal
 *  nssPKIXTBSCertificate_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXTBSCertificate_verifyPointer
 */

/*
 * nssPKIXTBSCertificate_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTBSCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTBSCertificate *
nssPKIXTBSCertificate_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTBSCertificate_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_VERSION
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_SERIAL_NUMBER
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_INVALID_ISSUER_NAME
 *  NSS_ERROR_INVALID_VALIDITY
 *  NSS_ERROR_INVALID_SUBJECT_NAME
 *  NSS_ERROR_INVALID_SUBJECT_PUBLIC_KEY_INFO
 *  NSS_ERROR_INVALID_ISSUER_UNIQUE_IDENTIFIER
 *  NSS_ERROR_INVALID_SUBJECT_UNIQUE_IDENTIFIER
 *  NSS_ERROR_INVALID_EXTENSION
 *
 * Return value:
 */

NSS_EXTERN NSSPKIXTBSCertificate *
nssPKIXTBSCertificate_Create
(
  NSSArena *arenaOpt,
  NSSPKIXVersion version,
  NSSPKIXCertificateSerialNumber *serialNumber,
  NSSPKIXAlgorithmIdentifier *signature,
  NSSPKIXName *issuer,
  NSSPKIXValidity *validity,
  NSSPKIXName *subject,
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSPKIXUniqueIdentifier *issuerUniqueID,
  NSSPKIXUniqueIdentifier *subjectUniqueID,
  NSSPKIXExtensions *extensions
);

/*
 * nssPKIXTBSCertificate_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_Destroy
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * nssPKIXTBSCertificate_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTBSCertificate_Encode
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_GetVersion
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  A valid element of the NSSPKIXVersion enumeration upon success
 *  NSSPKIXVersion_NSSinvalid (-1) upon failure
 */

NSS_EXTERN NSSPKIXVersion
nssPKIXTBSCertificate_GetVersion
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * nssPKIXTBSCertificate_SetVersion
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_PKIX_VERSION
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetVersion
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXVersion version
);

/*
 * nssPKIXTBSCertificate_GetSerialNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificateSerialNumber upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificateSerialNumber *
nssPKIXTBSCertificate_GetSerialNumber
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXCertificateSerialNumber *snOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_SetSerialNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetSerialNumber
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXCertificateSerialNumber *sn
);

/*
 * nssPKIXTBSCertificate_GetSignature
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAlgorithmIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAlgorithmIdentifier *
nssPKIXTBSCertificate_GetSignature
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_SetSignature
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetSignature
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXAlgorithmIdentifier *algID
);

/*
 *   { fgmr inherit algid gettors? }
 */

/*
 * nssPKIXTBSCertificate_GetIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXName *
nssPKIXTBSCertificate_GetIssuer
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_SetIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetIssuer
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXName *issuer
);

/*
 *   { inherit "helper" issuer gettors? }
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *
 * Return value:
 */

/*
 * nssPKIXTBSCertificate_GetValidity
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXValidity upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXValidity *
nssPKIXTBSCertificate_GetValidity
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_SetValidity
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetValidity
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXValidity *validity
);

/*
 *   { fgmr inherit validity accessors }
 */

/*
 * nssPKIXTBSCertificate_GetSubject
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXName *
nssPKIXTBSCertificate_GetSubject
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_SetSubject
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetSubject
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXName *subject
);

/*
 * nssPKIXTBSCertificate_GetSubjectPublicKeyInfo
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXSubjectPublicKeyInfo upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXSubjectPublicKeyInfo *
nssPKIXTBSCertificate_GetSubjectPublicKeyInfo
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_SetSubjectPublicKeyInfo
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetSubjectPublicKeyInfo
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXSubjectPublicKeyInfo *spki
);

/*
 * nssPKIXTBSCertificate_HasIssuerUniqueID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXTBSCertificate_HasIssuerUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  PRStatus *statusOpt
);

/*
 * nssPKIXTBSCertificate_GetIssuerUniqueID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_CERT_HAS_NO_ISSUER_UNIQUE_ID
 *
 * Return value:
 *  A valid pointer to an NSSPKIXUniqueIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXUniqueIdentifier *
nssPKIXTBSCertificate_GetIssuerUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXUniqueIdentifier *uidOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_SetIssuerUniqueID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetIssuerUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXUniqueIdentifier *uid
);

/*
 * nssPKIXTBSCertificate_RemoveIssuerUniqueID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_CERT_HAS_NO_ISSUER_UNIQUE_ID
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_RemoveIssuerUniqueID
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * nssPKIXTBSCertificate_HasSubjectUniqueID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXTBSCertificate_HasSubjectUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  PRStatus *statusOpt
);

/*
 * nssPKIXTBSCertificate_GetSubjectUniqueID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_CERT_HAS_NO_SUBJECT_UNIQUE_ID
 *
 * Return value:
 *  A valid pointer to an NSSPKIXUniqueIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXUniqueIdentifier *
nssPKIXTBSCertificate_GetSubjectUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXUniqueIdentifier *uidOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_SetSubjectUniqueID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetSubjectUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXUniqueIdentifier *uid
);

/*
 * nssPKIXTBSCertificate_RemoveSubjectUniqueID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_CERT_HAS_NO_SUBJECT_UNIQUE_ID
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_RemoveSubjectUniqueID
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * nssPKIXTBSCertificate_HasExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXTBSCertificate_HasExtensions
(
  NSSPKIXTBSCertificate *tbsCert,
  PRStatus *statusOpt
);

/*
 * nssPKIXTBSCertificate_GetExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_CERT_HAS_NO_EXTENSIONS
 *
 * Return value:
 *  A valid pointer to an NSSPKIXExtensions upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensions *
nssPKIXTBSCertificate_GetExtensions
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertificate_SetExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_PKIX_EXTENSIONS
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_SetExtensions
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXExtensions *extensions
);

/*
 * nssPKIXTBSCertificate_RemoveExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_CERT_HAS_NO_EXTENSIONS
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_RemoveExtensions
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 *   { extension accessors }
 */

/*
 * nssPKIXTBSCertificate_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXTBSCertificate_Equal
(
  NSSPKIXTBSCertificate *one,
  NSSPKIXTBSCertificate *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXTBSCertificate_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTBSCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTBSCertificate *
nssPKIXTBSCertificate_Duplicate
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXTBSCertificate_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXTBSCertificate
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERTIFICATE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertificate_verifyPointer
(
  NSSPKIXTBSCertificate *p
);
#endif /* DEBUG */

/*
 * CertificateSerialNumber
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CertificateSerialNumber  ::=  INTEGER
 *
 * This is just a typedef'd NSSBER; no methods are required.
 * {fgmr -- the asn.1 stuff should have routines to convert
 * integers to natural types when possible and vv.  we can
 * refer to them here..}
 */

/*
 * Validity
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Validity ::= SEQUENCE {
 *       notBefore      Time,
 *       notAfter       Time }
 *
 * The private calls for the type:
 *
 *  nssPKIXValidity_Decode
 *  nssPKIXValidity_Create
 *  nssPKIXValidity_Encode
 *  nssPKIXValidity_Destroy
 *  nssPKIXValidity_GetNotBefore
 *  nssPKIXValidity_SetNotBefore
 *  nssPKIXValidity_GetNotAfter
 *  nssPKIXValidity_SetNotAfter
 *  nssPKIXValidity_Equal
 *  nssPKIXValidity_Compare
 *  nssPKIXValidity_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXValidity_verifyPointer
 *
 */

/*
 * nssPKIXValidity_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXValidity upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXValidity *
nssPKIXValidity_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXValidity_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_TIME
 *
 * Return value:
 *  A valid pointer to an NSSPKIXValidity upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXValidity *
nssPKIXValidity_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTime notBefore,
  NSSPKIXTime notAfter
);

/*
 * nssPKIXValidity_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXValidity_Destroy
(
  NSSPKIXValidity *validity
);

/*
 * nssPKIXValidity_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXValidity_Encode
(
  NSSPKIXValidity *validity,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXValidity_GetNotBefore
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  A valid NSSPKIXTime upon success
 *  {we need to rethink NSSPKIXTime}
 */

NSS_EXTERN NSSPKIXTime
nssPKIXValidity_GetNotBefore
(
  NSSPKIXValidity *validity
);

/*
 * nssPKIXValidity_SetNotBefore
 *
 * -- fgmr comments --
 * {do we require that it be before the "notAfter" value?}
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *  NSS_ERROR_VALUE_TOO_LARGE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXValidity_SetNotBefore
(
  NSSPKIXValidity *validity,
  NSSPKIXTime notBefore
);

/*
 * nssPKIXValidity_GetNotAfter
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  A valid NSSPKIXTime upon success
 *  {we need to rethink NSSPKIXTime}
 */

NSS_EXTERN NSSPKIXTime
nssPKIXValidity_GetNotAfter
(
  NSSPKIXValidity *validity
);

/*
 * nssPKIXValidity_SetNotAfter
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *  NSS_ERROR_VALUE_TOO_SMALL
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXValidity_SetNotAfter
(
  NSSPKIXValidity *validity,
  NSSPKIXTime notAfter
);

/*
 * nssPKIXValidity_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXValidity_Equal
(
  NSSPKIXValidity *one,
  NSSPKIXValidity *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXValidity_Compare
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *
 * Return value:
 *   1 if the second is "greater" or later than the first
 *   0 if they are equal
 *  -1 if the first is "greater" or later than the first
 *  -2 upon failure
 */

NSS_EXTERN PRIntn
nssPKIXValidity_Compare
(
  NSSPKIXValidity *one,
  NSSPKIXValidity *two
);

/*
 * nssPKIXValidity_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXValidity upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXValidity *
nssPKIXValidity_Duplicate
(
  NSSPKIXValidity *validity,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXValidity_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXValidity
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_VALIDITY
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXValidity_verifyPointer
(
  NSSPKIXValidity *p
);
#endif /* DEBUG */

/*
 * Time
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Time ::= CHOICE {
 *       utcTime        UTCTime,
 *       generalTime    GeneralizedTime }
 *
 * The private calls for the type:
 *
 *  nssPKIXTime_Decode
 *  nssPKIXTime_CreateFromPRTime
 *  nssPKIXTime_CreateFromUTF8
 *  nssPKIXTime_Destroy
 *  nssPKIXTime_Encode
 *  nssPKIXTime_GetPRTime
 *  nssPKIXTime_GetUTF8Encoding
 *  nssPKIXTime_Equal
 *  nssPKIXTime_Duplicate
 *  nssPKIXTime_Compare
 *
 * In debug builds, the following call is available:
 *
 *  nssPKIXTime_verifyPointer
 *
 */

/*
 * nssPKIXTime_Decode
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTime upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTime *
nssPKIXTime_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTime_CreateFromPRTime
 *
 */

NSS_EXTERN NSSPKIXTime *
nssPKIXTime_CreateFromPRTime
(
  NSSArena *arenaOpt,
  PRTime prTime
);

/*
 * nssPKIXTime_CreateFromUTF8
 *
 */

NSS_EXTERN NSSPKIXTime *
nssPKIXTime_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXTime_Destroy
 *
 */

NSS_EXTERN PR_STATUS
nssPKIXTime_Destroy
(
  NSSPKIXTime *time
);

/*
 * nssPKIXTime_Encode
 *
 */

NSS_EXTERN NSSBER *
nssPKIXTime_Encode
(
  NSSPKIXTime *time,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTime_GetPRTime
 *
 * Returns a zero on error
 */

NSS_EXTERN PRTime
nssPKIXTime_GetPRTime
(
  NSSPKIXTime *time,
  PRStatus *statusOpt
);

/*
 * nssPKIXTime_GetUTF8Encoding
 *
 */

NSS_EXTERN NSSUTF8 *
nssPKXITime_GetUTF8Encoding
(
  NSSPKIXTime *time,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTime_Equal
 *
 */

NSS_EXTERN PRBool
nssPKXITime_Equal
(
  NSSPKXITime *time1,
  NSSPKXITime *time2,
  PRStatus *statusOpt
);

/*
 * nssPKIXTime_Duplicate
 *
 */

NSS_EXTERN NSSPKIXTime *
nssPKXITime_Duplicate
(
  NSSPKIXTime *time,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTime_Compare
 *
 * Usual result: -1, 0, 1
 *  Returns 0 on error
 */

NSS_EXTERN PRInt32
nssPKIXTime_Compare
(
  NSSPKIXTime *time1,
  NSSPKIXTime *tiem2,
  PRStatus *statusOpt
);

#ifdef DEBUG
/*
 * nssPKIXTime_verifyPointer
 *
 */

NSS_EXTERN PRStatus
nssPKIXTime_verifyPointer
(
  NSSPKIXTime *time
);
#endif /* DEBUG */

/*
 * UniqueIdentifier
 *
 * From RFC 2459:
 *
 *  UniqueIdentifier  ::=  BIT STRING
 *
 */

/*
 * SubjectPublicKeyInfo
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  SubjectPublicKeyInfo  ::=  SEQUENCE  {
 *       algorithm            AlgorithmIdentifier,
 *       subjectPublicKey     BIT STRING  }
 *
 * The private calls for the type:
 *
 *  nssPKIXSubjectPublicKeyInfo_Decode
 *  nssPKIXSubjectPublicKeyInfo_Create
 *  nssPKIXSubjectPublicKeyInfo_Encode
 *  nssPKIXSubjectPublicKeyInfo_Destroy
 *  nssPKIXSubjectPublicKeyInfo_GetAlgorithm
 *  nssPKIXSubjectPublicKeyInfo_SetAlgorithm
 *  nssPKIXSubjectPublicKeyInfo_GetSubjectPublicKey
 *  nssPKIXSubjectPublicKeyInfo_SetSubjectPublicKey
 *  nssPKIXSubjectPublicKeyInfo_Equal
 *  nssPKIXSubjectPublicKeyInfo_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXSubjectPublicKeyInfo_verifyPointer
 *
 */

/*
 * nssPKIXSubjectPublicKeyInfo_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXSubjectPublicKeyInfo upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXSubjectPublicKeyInfo *
nssPKIXSubjectPublicKeyInfo_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXSubjectPublicKeyInfo_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXSubjectPublicKeyInfo upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXSubjectPublicKeyInfo *
nssPKIXSubjectPublicKeyInfo_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAlgorithmIdentifier *algid,
  NSSItem *subjectPublicKey
);

/*
 * nssPKIXSubjectPublicKeyInfo_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXSubjectPublicKeyInfo_Destroy
(
  NSSPKIXSubjectPublicKeyInfo *spki
);

/*
 * nssPKIXSubjectPublicKeyInfo_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXSubjectPublicKeyInfo_Encode
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXSubjectPublicKeyInfo_GetAlgorithm
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAlgorithmIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAlgorithmIdentifier *
nssPKIXSubjectPublicKeyInfo_GetAlgorithm
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSArena *arenaOpt
);

/*
 * nssPKIXSubjectPublicKeyInfo_SetAlgorithm
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXSubjectPublicKeyInfo_SetAlgorithm
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSPKIXAlgorithmIdentifier *algid
);

/*
 * nssPKIXSubjectPublicKeyInfo_GetSubjectPublicKey
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSItem *
nssPKIXSubjectPublicKeyInfo_GetSubjectPublicKey
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSItem *spkOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXSubjectPublicKeyInfo_SetSubjectPublicKey
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXSubjectPublicKeyInfo_SetSubjectPublicKey
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSItem *spk
);

/*
 * nssPKIXSubjectPublicKeyInfo_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXSubjectPublicKeyInfo_Equal
(
  NSSPKIXSubjectPublicKeyInfo *one,
  NSSPKIXSubjectPublicKeyInfo *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXSubjectPublicKeyInfo_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXSubjectPublicKeyInfo upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXSubjectPublicKeyInfo *
nssPKIXSubjectPublicKeyInfo_Duplicate
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXSubjectPublicKeyInfo_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXSubjectPublicKeyInfo
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_PUBLIC_KEY_INFO
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXSubjectPublicKeyInfo_verifyPointer
(
  NSSPKIXSubjectPublicKeyInfo *p
);
#endif /* DEBUG */

/*
 * Extensions
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
 *
 */

/* { FGMR } */

/*
 * Extension
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  Extension  ::=  SEQUENCE  {
 *       extnID      OBJECT IDENTIFIER,
 *       critical    BOOLEAN DEFAULT FALSE,
 *       extnValue   OCTET STRING  }
 *
 */

/* { FGMR } */

/*
 * CertificateList
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CertificateList  ::=  SEQUENCE  {
 *       tbsCertList          TBSCertList,
 *       signatureAlgorithm   AlgorithmIdentifier,
 *       signature            BIT STRING  }
 *
 * The private calls for the type:
 *
 *  nssPKIXCertificateList_Decode
 *  nssPKIXCertificateList_Create
 *  nssPKIXCertificateList_Encode
 *  nssPKIXCertificateList_Destroy
 *  nssPKIXCertificateList_GetTBSCertList
 *  nssPKIXCertificateList_SetTBSCertList
 *  nssPKIXCertificateList_GetSignatureAlgorithm
 *  nssPKIXCertificateList_SetSignatureAlgorithm
 *  nssPKIXCertificateList_GetSignature
 *  nssPKIXCertificateList_SetSignature
 *  nssPKIXCertificateList_Equal
 *  nssPKIXCertificateList_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXCertificateList_verifyPointer
 *
 */

/*
 * nssPKIXCertificateList_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificateList upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificateList *
nssPKIXCertificateList_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXCertificateList_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificateList upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificateList *
nssPKIXCertificateList_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTBSCertList *tbsCertList,
  NSSPKIXAlgorithmIdentifier *sigAlg,
  NSSItem *signature
);

/*
 * nssPKIXCertificateList_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificateList_Destroy
(
  NSSPKIXCertificateList *certList
);

/*
 * nssPKIXCertificateList_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXCertificateList_Encode
(
  NSSPKIXCertificateList *certList,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificateList_GetTBSCertList
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTBSCertList upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTBSCertList *
nssPKIXCertificateList_GetTBSCertList
(
  NSSPKIXCertificateList *certList,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificateList_SetTBSCertList
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificateList_SetTBSCertList
(
  NSSPKIXCertificateList *certList,
  NSSPKIXTBSCertList *tbsCertList
);

/*
 * nssPKIXCertificateList_GetSignatureAlgorithm
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAlgorithmIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAlgorithmIdentifier *
nssPKIXCertificateList_GetSignatureAlgorithm
(
  NSSPKIXCertificateList *certList,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificateList_SetSignatureAlgorithm
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificateList_SetSignatureAlgorithm
(
  NSSPKIXCertificateList *certList,
  NSSPKIXAlgorithmIdentifier *sigAlg
);

/*
 * nssPKIXCertificateList_GetSignature
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSItem *
nssPKIXCertificateList_GetSignature
(
  NSSPKIXCertificateList *certList,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificateList_SetSignature
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificateList_SetSignature
(
  NSSPKIXCertificateList *certList,
  NSSItem *sig
);

/*
 * nssPKIXCertificateList_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXCertificateList_Equal
(
  NSSPKIXCertificateList *one,
  NSSPKIXCertificateList *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXCertificateList_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificateList upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificateList *
nssPKIXCertificateList_Duplicate
(
  NSSPKIXCertificateList *certList,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXCertificateList_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXCertificateList
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_LIST
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXCertificateList_verifyPointer
(
  NSSPKIXCertificateList *p
);
#endif /* DEBUG */

/*
 * TBSCertList
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TBSCertList  ::=  SEQUENCE  {
 *       version                 Version OPTIONAL,
 *                                    -- if present, shall be v2
 *       signature               AlgorithmIdentifier,
 *       issuer                  Name,
 *       thisUpdate              Time,
 *       nextUpdate              Time OPTIONAL,
 *       revokedCertificates     SEQUENCE OF SEQUENCE  {
 *            userCertificate         CertificateSerialNumber,
 *            revocationDate          Time,
 *            crlEntryExtensions      Extensions OPTIONAL
 *                                           -- if present, shall be v2
 *                                 }  OPTIONAL,
 *       crlExtensions           [0] Extensions OPTIONAL
 *                                           -- if present, shall be v2 -- }
 *
 * The private calls for the type:
 *
 *  nssPKIXTBSCertList_Decode
 *  nssPKIXTBSCertList_Create
 *  nssPKIXTBSCertList_Destroy
 *  nssPKIXTBSCertList_Encode
 *  nssPKIXTBSCertList_GetVersion
 *  nssPKIXTBSCertList_SetVersion
 *  nssPKIXTBSCertList_GetSignature
 *  nssPKIXTBSCertList_SetSignature
 *  nssPKIXTBSCertList_GetIssuer
 *  nssPKIXTBSCertList_SetIssuer
 *  nssPKIXTBSCertList_GetThisUpdate
 *  nssPKIXTBSCertList_SetThisUpdate
 *  nssPKIXTBSCertList_HasNextUpdate
 *  nssPKIXTBSCertList_GetNextUpdate
 *  nssPKIXTBSCertList_SetNextUpdate
 *  nssPKIXTBSCertList_RemoveNextUpdate
 *  nssPKIXTBSCertList_GetRevokedCertificates
 *  nssPKIXTBSCertList_SetRevokedCertificates
 *  nssPKIXTBSCertList_HasCrlExtensions
 *  nssPKIXTBSCertList_GetCrlExtensions
 *  nssPKIXTBSCertList_SetCrlExtensions
 *  nssPKIXTBSCertList_RemoveCrlExtensions
 *  nssPKIXTBSCertList_Equal
 *  nssPKIXTBSCertList_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXTBSCertList_verifyPointer
 *
 */

/*
 * nssPKIXTBSCertList_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTBSCertList upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTBSCertList *
nssPKIXTBSCertList_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTBSCertList_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_VERSION
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_INVALID_PKIX_TIME
 *  (something for the times being out of order?)
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATES
 *  NSS_ERROR_INVALID_PKIX_EXTENSIONS
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTBSCertList upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTBSCertList *
nssPKIXTBSCertList_Create
(
  NSSArena *arenaOpt,
  NSSPKIXVersion version,
  NSSPKIXAlgorithmIdentifier *signature,
  NSSPKIXName *issuer,
  NSSPKIXTime thisUpdate,
  NSSPKIXTime nextUpdateOpt,
  NSSPKIXrevokedCertificates *revokedCerts,
  NSSPKIXExtensions *crlExtensionsOpt
);

/*
 * nssPKIXTBSCertList_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_Destroy
(
  NSSPKIXTBSCertList *certList
);

/*
 * nssPKIXTBSCertList_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTBSCertList_Encode
(
  NSSPKIXTBSCertList *certList,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertList_GetVersion
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  A valid element of the NSSPKIXVersion enumeration upon success
 *  NSSPKIXVersion_NSSinvalid (-1) upon failure
 */

NSS_EXTERN NSSPKIXVersion
nssPKIXTBSCertList_GetVersion
(
  NSSPKIXTBSCertList *certList
);

/*
 * nssPKIXTBSCertList_SetVersion
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_PKIX_VERSION
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_SetVersion
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXVersion version
);

/*
 * nssPKIXTBSCertList_GetSignature
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAlgorithmIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAlgorithmIdentifier *
nssPKIXTBSCertList_GetSignature
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertList_SetSignature
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_SetSignature
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXAlgorithmIdentifier *algid,
);

/*
 * nssPKIXTBSCertList_GetIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXName *
nssPKIXTBSCertList_GetIssuer
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertList_SetIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_SetIssuer
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXName *issuer
);

/*
 * nssPKIXTBSCertList_GetThisUpdate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  A valid NSSPKIXTime upon success
 *  {we need to rethink NSSPKIXTime}
 */

NSS_EXTERN NSSPKIXTime
nssPKIXTBSCertList_GetThisUpdate
(
  NSSPKIXTBSCertList *certList
);

/*
 * nssPKIXTBSCertList_SetThisUpdate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_VALUE_TOO_LARGE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_SetThisUpdate
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXTime thisUpdate
);

/*
 * nssPKIXTBSCertList_HasNextUpdate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXTBSCertList_HasNextUpdate
(
  NSSPKIXTBSCertList *certList,
  PRStatus *statusOpt
);

/*
 * nssPKIXTBSCertList_GetNextUpdate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  A valid NSSPKIXTime upon success
 *  {we need to rethink NSSPKIXTime}
 */

NSS_EXTERN NSSPKIXTime
nssPKIXTBSCertList_GetNextUpdate
(
  NSSPKIXTBSCertList *certList
);

/*
 * nssPKIXTBSCertList_SetNextUpdate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_VALUE_TOO_SMALL
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_SetNextUpdate
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXTime nextUpdate
);

/*
 * nssPKIXTBSCertList_RemoveNextUpdate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_HAS_NO_NEXT_UPDATE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_RemoveNextUpdate
(
  NSSPKIXTBSCertList *certList
);

/*
 * nssPKIXTBSCertList_GetRevokedCertificates
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXrevokedCertificates upon succes
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXrevokedCertificates *
nssPKIXTBSCertList_GetRevokedCertificates
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertList_SetRevokedCertificates
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATES
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_SetRevokedCertificates
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXrevokedCertificates *revoked
);

/*
 * nssPKIXTBSCertList_HasCrlExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXTBSCertList_HasCrlExtensions
(
  NSSPKIXTBSCertList *certList,
  PRStatus *statusOpt
);

/*
 * nssPKIXTBSCertList_GetCrlExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXExtensions upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensions *
nssPKIXTBSCertList_GetCrlExtensions
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTBSCertList_SetCrlExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_PKIX_EXTENSIONS
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_SetCrlExtensions
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXExtensions *extensions
);

/*
 * nssPKIXTBSCertList_RemoveCrlExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_HAS_NO_EXTENSIONS
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_RemoveCrlExtensions
(
  NSSPKIXTBSCertList *certList
);

/*
 * nssPKIXTBSCertList_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXTBSCertList_Equal
(
  NSSPKIXTBSCertList *one,
  NSSPKIXTBSCertList *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXTBSCertList_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTBSCertList upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTBSCertList *
nssPKIXTBSCertList_Duplicate
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXTBSCertList_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXTBSCertList
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TBS_CERT_LIST
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXTBSCertList_verifyPointer
(
  NSSPKIXTBSCertList *p
);
#endif /* DEBUG */

/*
 * revokedCertificates
 *
 * This is a "helper type" to simplify handling of TBSCertList objects.
 *
 *       revokedCertificates     SEQUENCE OF SEQUENCE  {
 *            userCertificate         CertificateSerialNumber,
 *            revocationDate          Time,
 *            crlEntryExtensions      Extensions OPTIONAL
 *                                           -- if present, shall be v2
 *                                 }  OPTIONAL,
 *
 * The private calls for the type:
 *
 *  nssPKIXrevokedCertificates_Decode
 *  nssPKIXrevokedCertificates_Create
 *  nssPKIXrevokedCertificates_Encode
 *  nssPKIXrevokedCertificates_Destroy
 *  nssPKIXrevokedCertificates_GetRevokedCertificateCount
 *  nssPKIXrevokedCertificates_GetRevokedCertificates
 *  nssPKIXrevokedCertificates_SetRevokedCertificates
 *  nssPKIXrevokedCertificates_GetRevokedCertificate
 *  nssPKIXrevokedCertificates_SetRevokedCertificate
 *  nssPKIXrevokedCertificates_InsertRevokedCertificate
 *  nssPKIXrevokedCertificates_AppendRevokedCertificate
 *  nssPKIXrevokedCertificates_RemoveRevokedCertificate
 *  nssPKIXrevokedCertificates_Equal
 *  nssPKIXrevokedCertificates_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXrevokedCertificates_verifyPointer
 *
 */

/*
 * nssPKIXrevokedCertificates_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXrevokedCertificates upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXrevokedCertificates *
nssPKIXrevokedCertificates_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXrevokedCertificates_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATE
 *
 * Return value:
 *  A valid pointer to an NSSPKIXrevokedCertificates upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXrevokedCertificates *
nssPKIXrevokedCertificates_Create
(
  NSSArena *arenaOpt,
  NSSPKIXrevokedCertificate *rc1,
  ...
);

/*
 * nssPKIXrevokedCertificates_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificates_Destroy
(
  NSSPKIXrevokedCertificates *rcs
);

/*
 * nssPKIXrevokedCertificates_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXrevokedCertificates_Encode
(
  NSSPKIXrevokedCertificates *rcs,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXrevokedCertificates_GetRevokedCertificateCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXrevokedCertificates_GetRevokedCertificateCount
(
  NSSPKIXrevokedCertificates *rcs
);

/*
 * nssPKIXrevokedCertificates_GetRevokedCertificates
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  A valid pointer to an array of NSSPKIXrevokedCertificate pointers
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXrevokedCertificate **
nssPKIXrevokedCertificates_GetRevokedCertificates
(
  NSSPKIXrevokedCertificates *rcs,
  NSSPKIXrevokedCertificate *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXrevokedCertificates_SetRevokedCertificates
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificates_SetRevokedCertificates
(
  NSSPKIXrevokedCertificates *rcs,
  NSSPKIXrevokedCertificate *rc[],
  PRInt32 countOpt
);

/*
 * nssPKIXrevokedCertificates_GetRevokedCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXrevokedCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXrevokedCertificate *
nssPKIXrevokedCertificates_GetRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXrevokedCertificates_SetRevokedCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificates_SetRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i,
  NSSPKIXrevokedCertificate *rc
);

/*
 * nssPKIXrevokedCertificates_InsertRevokedCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificates_InsertRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i,
  NSSPKIXrevokedCertificate *rc
);

/*
 * nssPKIXrevokedCertificates_AppendRevokedCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificates_AppendRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i,
  NSSPKIXrevokedCertificate *rc
);

/*
 * nssPKIXrevokedCertificates_RemoveRevokedCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificates_RemoveRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i
);

/*
 * nssPKIXrevokedCertificates_FindRevokedCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATE
 *
 * Return value:
 *  The index of the specified revoked certificate upon success
 *  -1 upon failure
 */

NSS_EXTERN PRInt32
nssPKIXrevokedCertificates_FindRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  NSSPKIXrevokedCertificate *rc
);

/*
 * nssPKIXrevokedCertificates_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXrevokedCertificates_Equal
(
  NSSPKIXrevokedCertificates *one,
  NSSPKIXrevokedCertificates *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXrevokedCertificates_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_REVOKED_CERTIFICATES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXrevokedCertificates upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXrevokedCertificates *
nssPKIXrevokedCertificates_Duplicate
(
  NSSPKIXrevokedCertificates *rcs,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXrevokedCertificates_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXrevokedCertificates
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificates_verifyPointer
(
  NSSPKIXrevokedCertificates *p
);
#endif /* DEBUG */

/*
 * revokedCertificate
 *
 * This is a "helper type" to simplify handling of TBSCertList objects.
 *
 *                                           SEQUENCE  {
 *            userCertificate         CertificateSerialNumber,
 *            revocationDate          Time,
 *            crlEntryExtensions      Extensions OPTIONAL
 *                                           -- if present, shall be v2
 *                                 }  OPTIONAL,
 *
 * The private calls for this type:
 *
 *  nssPKIXrevokedCertificate_Decode
 *  nssPKIXrevokedCertificate_Create
 *  nssPKIXrevokedCertificate_Encode
 *  nssPKIXrevokedCertificate_Destroy
 *  nssPKIXrevokedCertificate_GetUserCertificate
 *  nssPKIXrevokedCertificate_SetUserCertificate
 *  nssPKIXrevokedCertificate_GetRevocationDate
 *  nssPKIXrevokedCertificate_SetRevocationDate
 *  nssPKIXrevokedCertificate_HasCrlEntryExtensions
 *  nssPKIXrevokedCertificate_GetCrlEntryExtensions
 *  nssPKIXrevokedCertificate_SetCrlEntryExtensions
 *  nssPKIXrevokedCertificate_RemoveCrlEntryExtensions
 *  nssPKIXrevokedCertificate_Equal
 *  nssPKIXrevokedCertificate_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXrevokedCertificate_verifyPointer
 *
 */


/*
 * nssPKIXrevokedCertificate_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXrevokedCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXrevokedCertificate *
nssPKIXrevokedCertificate_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXrevokedCertificate_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_SERIAL_NUMBER
 *  NSS_ERROR_INVALID_PKIX_TIME
 *  NSS_ERROR_INVALID_PKIX_EXTENSIONS
 *
 * Return value:
 *  A valid pointer to an NSSPKIXrevokedCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXrevokedCertificate *
nssPKIXrevokedCertificate_Create
(
  NSSArena *arenaOpt,
  NSSPKIXCertificateSerialNumber *userCertificate,
  NSSPKIXTime *revocationDate,
  NSSPKIXExtensions *crlEntryExtensionsOpt
);

/*
 * nssPKIXrevokedCertificate_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificate_Destroy
(
  NSSPKIXrevokedCertificate *rc
);

/*
 * nssPKIXrevokedCertificate_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXrevokedCertificate_Encode
(
  NSSPKIXrevokedCertificate *rc,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXrevokedCertificate_GetUserCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificateSerialNumber upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificateSerialNumber *
nssPKIXrevokedCertificate_GetUserCertificate
(
  NSSPKIXrevokedCertificate *rc,
  NSSArena *arenaOpt
);  

/*
 * nssPKIXrevokedCertificate_SetUserCertificate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_SERIAL_NUMBER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificate_SetUserCertificate
(
  NSSPKIXrevokedCertificate *rc,
  NSSPKIXCertificateSerialNumber *csn
);

/*
 * nssPKIXrevokedCertificate_GetRevocationDate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTime upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTime *
nssPKIXrevokedCertificate_GetRevocationDate
(
  NSSPKIXrevokedCertificate *rc,
  NSSArena *arenaOpt
);  

/*
 * nssPKIXrevokedCertificate_SetRevocationDate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_TIME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificate_SetRevocationDate
(
  NSSPKIXrevokedCertificate *rc,
  NSSPKIXTime *revocationDate
);

/*
 * nssPKIXrevokedCertificate_HasCrlEntryExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXrevokedCertificate_HasCrlEntryExtensions
(
  NSSPKIXrevokedCertificate *rc,
  PRStatus *statusOpt
);

/*
 * nssPKIXrevokedCertificate_GetCrlEntryExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_NO_EXTENSIONS
 *
 * Return value:
 *  A valid pointer to an NSSPKIXExtensions upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensions *
nssPKIXrevokedCertificate_GetCrlEntryExtensions
(
  NSSPKIXrevokedCertificate *rc,
  NSSArena *arenaOpt
);  

/*
 * nssPKIXrevokedCertificate_SetCrlEntryExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_EXTENSIONS
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificate_SetCrlEntryExtensions
(
  NSSPKIXrevokedCertificate *rc,
  NSSPKIXExtensions *crlEntryExtensions
);

/*
 * nssPKIXrevokedCertificate_RemoveCrlEntryExtensions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *  NSS_ERROR_NO_EXTENSIONS
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificate_RemoveCrlEntryExtensions
(
  NSSPKIXrevokedCertificate *rc
);

/*
 * nssPKIXrevokedCertificate_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXrevokedCertificate_Equal
(
  NSSPKIXrevokedCertificate *one,
  NSSPKIXrevokedCertificate *two,
  PRStatus *statusOpt
);

/*
 * nssPKIXrevokedCertificate_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *
 * Return value:
 *  A valid pointer to an NSSPKIXrevokedCertificate upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXrevokedCertificate *
nssPKIXrevokedCertificate_Duplicate
(
  NSSPKIXrevokedCertificate *rc,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXrevokedCertificate_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXrevokedCertificate
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REVOKED_CERTIFICATE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXrevokedCertificate_verifyPointer
(
  NSSPKIXrevokedCertificate *p
);
#endif /* DEBUG */

/*
 * AlgorithmIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 * (1988 syntax)
 *
 *  AlgorithmIdentifier  ::=  SEQUENCE  {
 *       algorithm               OBJECT IDENTIFIER,
 *       parameters              ANY DEFINED BY algorithm OPTIONAL  }
 *                                  -- contains a value of the type
 *                                  -- registered for use with the
 *                                  -- algorithm object identifier value
 *
 * The private calls for this type:
 *
 *  nssPKIXAlgorithmIdentifier_Decode
 *  nssPKIXAlgorithmIdentifier_Create
 *  nssPKIXAlgorithmIdentifier_Encode
 *  nssPKIXAlgorithmIdentifier_Destroy
 *  nssPKIXAlgorithmIdentifier_GetAlgorithm
 *  nssPKIXAlgorithmIdentifier_SetAlgorithm
 *  nssPKIXAlgorithmIdentifier_GetParameters
 *  nssPKIXAlgorithmIdentifier_SetParameters
 *  nssPKIXAlgorithmIdentifier_Compare
 *  nssPKIXAlgorithmIdentifier_Duplicate
 *    { algorithm-specific parameter types and accessors ? }
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXAlgorithmIdentifier_verifyPointer
 *
 */

/*
 * nssPKIXAlgorithmIdentifier_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAlgorithmIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAlgorithmIdentifier *
nssPKIXAlgorithmIdentifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXAlgorithmIdentifier_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_INVALID_POINTER
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAlgorithmIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAlgorithmIdentifier *
nssPKIXAlgorithmIdentifier_Create
(
  NSSArena *arenaOpt,
  NSSOID *algorithm,
  NSSItem *parameters
);

/*
 * nssPKIXAlgorithmIdentifier_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAlgorithmIdentifier_Destroy
(
  NSSPKIXAlgorithmIdentifier *algid
);

/*
 * nssPKIXAlgorithmIdentifier_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXAlgorithmIdentifier_Encode
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAlgorithmIdentifier_GetAlgorithm
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSOID pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSOID *
nssPKIXAlgorithmIdentifier_GetAlgorithm
(
  NSSPKIXAlgorithmIdentifier *algid
);

/*
 * nssPKIXAlgorithmIdentifier_SetAlgorithm
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_INVALID_OID
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAlgorithmIdentifier_SetAlgorithm
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSOID *algorithm
);

/*
 * nssPKIXAlgorithmIdentifier_GetParameters
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSItem *
nssPKIXAlgorithmIdentifier_GetParameters
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAlgorithmIdentifier_SetParameters
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAlgorithmIdentifier_SetParameters
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSItem *parameters
);

/*
 * nssPKIXAlgorithmIdentifier_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXAlgorithmIdentifier_Equal
(
  NSSPKIXAlgorithmIdentifier *algid1,
  NSSPKIXAlgorithmIdentifier *algid2,
  PRStatus *statusOpt
);

/*
 * nssPKIXAlgorithmIdentifier_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAlgorithmIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAlgorithmIdentifier *
nssPKIXAlgorithmIdentifier_Duplicate
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSArena *arenaOpt
);

/*
 *   { algorithm-specific parameter types and accessors ? }
 */

#ifdef DEBUG
/*
 * nssPKIXAlgorithmIdentifier_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXAlgorithmIdentifier
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ALGORITHM_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXAlgorithmIdentifier_verifyPointer
(
  NSSPKIXAlgorithmIdentifier *p
);
#endif /* DEBUG */

/*
 * ORAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ORAddress ::= SEQUENCE {
 *     built-in-standard-attributes BuiltInStandardAttributes,
 *     built-in-domain-defined-attributes
 *                          BuiltInDomainDefinedAttributes OPTIONAL,
 *     -- see also teletex-domain-defined-attributes
 *     extension-attributes ExtensionAttributes OPTIONAL }
 *  --      The OR-address is semantically absent from the OR-name if the
 *  --      built-in-standard-attribute sequence is empty and the
 *  --      built-in-domain-defined-attributes and extension-attributes are
 *  --      both omitted.
 *
 * The private calls for this type:
 *
 *  nssPKIXORAddress_Decode
 *  nssPKIXORAddress_Create
 *  nssPKIXORAddress_Destroy
 *  nssPKIXORAddress_Encode
 *  nssPKIXORAddress_GetBuiltInStandardAttributes
 *  nssPKIXORAddress_SetBuiltInStandardAttributes
 *  nssPKIXORAddress_HasBuiltInDomainDefinedAttributes
 *  nssPKIXORAddress_GetBuiltInDomainDefinedAttributes
 *  nssPKIXORAddress_SetBuiltInDomainDefinedAttributes
 *  nssPKIXORAddress_RemoveBuiltInDomainDefinedAttributes
 *  nssPKIXORAddress_HasExtensionsAttributes
 *  nssPKIXORAddress_GetExtensionsAttributes
 *  nssPKIXORAddress_SetExtensionsAttributes
 *  nssPKIXORAddress_RemoveExtensionsAttributes
 *  nssPKIXORAddress_Equal
 *  nssPKIXORAddress_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXORAddress_verifyPointer
 *
 */

/*
 * nssPKIXORAddress_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXORAddres upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXORAddress *
nssPKIXORAddress_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXORAddress_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_EXTENSIONS_ATTRIBUTES
 *
 * Return value:
 *  A valid pointer to an NSSPKIXORAddres upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXORAddress *
nssPKIXORAddress_Create
(
  NSSArena *arenaOpt,
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXBuiltInDomainDefinedAttributes *biddaOpt,
  NSSPKIXExtensionAttributes *eaOpt
);

/*
 * nssPKIXORAddress_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXORAddress_Destroy
(
  NSSPKIXORAddress *ora
);

/*
 * nssPKIXORAddress_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXORAddress_Encode
(
  NSSPKIXORAddress *ora,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXORAddress_GetBuiltInStandardAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInStandardAttributes upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInStandardAttributes *
nssPKIXORAddress_GetBuiltInStandardAttributes
(
  NSSPKIXORAddress *ora,
  NSSArena *arenaOpt
);

/*
 * nssPKIXORAddress_SetBuiltInStandardAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXORAddress_SetBuiltInStandardAttributes
(
  NSSPKIXORAddress *ora,
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXORAddress_HasBuiltInDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXORAddress_HasBuiltInDomainDefinedAttributes
(
  NSSPKIXORAddress *ora,
  PRStatus *statusOpt
);

/*
 * nssPKIXORAddress_GetBuiltInDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_NO_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInDomainDefinedAttributes upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInDomainDefinedAttributes *
nssPKIXORAddress_GetBuiltInDomainDefinedAttributes
(
  NSSPKIXORAddress *ora,
  NSSArena *arenaOpt
);

/*
 * nssPKIXORAddress_SetBuiltInDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXORAddress_SetBuiltInDomainDefinedAttributes
(
  NSSPKIXORAddress *ora,
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXORAddress_RemoveBuiltInDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_NO_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXORAddress_RemoveBuiltInDomainDefinedAttributes
(
  NSSPKIXORAddress *ora
);

/*
 * nssPKIXORAddress_HasExtensionsAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXORAddress_HasExtensionsAttributes
(
  NSSPKIXORAddress *ora,
  PRStatus *statusOpt
);

/*
 * nssPKIXORAddress_GetExtensionsAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_NO_EXTENSION_ATTRIBUTES
 *
 * Return value:
 *  A valid pointer to an NSSPKIXExtensionAttributes upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttributes *
nssPKIXORAddress_GetExtensionsAttributes
(
  NSSPKIXORAddress *ora,
  NSSArena *arenaOpt
);

/*
 * nssPKIXORAddress_SetExtensionsAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXORAddress_SetExtensionsAttributes
(
  NSSPKIXORAddress *ora,
  NSSPKIXExtensionAttributes *eaOpt
);

/*
 * nssPKIXORAddress_RemoveExtensionsAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_NO_EXTENSION_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXORAddress_RemoveExtensionsAttributes
(
  NSSPKIXORAddress *ora
);

/*
 * nssPKIXORAddress_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXORAddress_Equal
(
  NSSPKIXORAddress *ora1,
  NSSPKIXORAddress *ora2,
  PRStatus *statusOpt
);

/*
 * nssPKIXORAddress_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXORAddres upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXORAddress *
nssPKIXORAddress_Duplicate
(
  NSSPKIXORAddress *ora,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXORAddress_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXORAddress
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXORAddress_verifyPointer
(
  NSSPKIXORAddress *p
);
#endif /* DEBUG */

/*
 * BuiltInStandardAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BuiltInStandardAttributes ::= SEQUENCE {
 *     country-name CountryName OPTIONAL,
 *     administration-domain-name AdministrationDomainName OPTIONAL,
 *     network-address      [0] NetworkAddress OPTIONAL,
 *     -- see also extended-network-address
 *     terminal-identifier  [1] TerminalIdentifier OPTIONAL,
 *     private-domain-name  [2] PrivateDomainName OPTIONAL,
 *     organization-name    [3] OrganizationName OPTIONAL,
 *     -- see also teletex-organization-name
 *     numeric-user-identifier      [4] NumericUserIdentifier OPTIONAL,
 *     personal-name        [5] PersonalName OPTIONAL,
 *     -- see also teletex-personal-name
 *     organizational-unit-names    [6] OrganizationalUnitNames OPTIONAL
 *     -- see also teletex-organizational-unit-names -- }
 *
 * The private calls for this type:
 *
 *  nssPKIXBuiltInStandardAttributes_Decode
 *  nssPKIXBuiltInStandardAttributes_Create
 *  nssPKIXBuiltInStandardAttributes_Destroy
 *  nssPKIXBuiltInStandardAttributes_Encode
 *  nssPKIXBuiltInStandardAttributes_HasCountryName
 *  nssPKIXBuiltInStandardAttributes_GetCountryName
 *  nssPKIXBuiltInStandardAttributes_SetCountryName
 *  nssPKIXBuiltInStandardAttributes_RemoveCountryName
 *  nssPKIXBuiltInStandardAttributes_HasAdministrationDomainName
 *  nssPKIXBuiltInStandardAttributes_GetAdministrationDomainName
 *  nssPKIXBuiltInStandardAttributes_SetAdministrationDomainName
 *  nssPKIXBuiltInStandardAttributes_RemoveAdministrationDomainName
 *  nssPKIXBuiltInStandardAttributes_HasNetworkAddress
 *  nssPKIXBuiltInStandardAttributes_GetNetworkAddress
 *  nssPKIXBuiltInStandardAttributes_SetNetworkAddress
 *  nssPKIXBuiltInStandardAttributes_RemoveNetworkAddress
 *  nssPKIXBuiltInStandardAttributes_HasTerminalIdentifier
 *  nssPKIXBuiltInStandardAttributes_GetTerminalIdentifier
 *  nssPKIXBuiltInStandardAttributes_SetTerminalIdentifier
 *  nssPKIXBuiltInStandardAttributes_RemoveTerminalIdentifier
 *  nssPKIXBuiltInStandardAttributes_HasPrivateDomainName
 *  nssPKIXBuiltInStandardAttributes_GetPrivateDomainName
 *  nssPKIXBuiltInStandardAttributes_SetPrivateDomainName
 *  nssPKIXBuiltInStandardAttributes_RemovePrivateDomainName
 *  nssPKIXBuiltInStandardAttributes_HasOrganizationName
 *  nssPKIXBuiltInStandardAttributes_GetOrganizationName
 *  nssPKIXBuiltInStandardAttributes_SetOrganizationName
 *  nssPKIXBuiltInStandardAttributes_RemoveOrganizationName
 *  nssPKIXBuiltInStandardAttributes_HasNumericUserIdentifier
 *  nssPKIXBuiltInStandardAttributes_GetNumericUserIdentifier
 *  nssPKIXBuiltInStandardAttributes_SetNumericUserIdentifier
 *  nssPKIXBuiltInStandardAttributes_RemoveNumericUserIdentifier
 *  nssPKIXBuiltInStandardAttributes_HasPersonalName
 *  nssPKIXBuiltInStandardAttributes_GetPersonalName
 *  nssPKIXBuiltInStandardAttributes_SetPersonalName
 *  nssPKIXBuiltInStandardAttributes_RemovePersonalName
 *  nssPKIXBuiltInStandardAttributes_HasOrganizationLUnitNames
 *  nssPKIXBuiltInStandardAttributes_GetOrganizationLUnitNames
 *  nssPKIXBuiltInStandardAttributes_SetOrganizationLUnitNames
 *  nssPKIXBuiltInStandardAttributes_RemoveOrganizationLUnitNames
 *  nssPKIXBuiltInStandardAttributes_Equal
 *  nssPKIXBuiltInStandardAttributes_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXBuiltInStandardAttributes_verifyPointer
 *
 */

/*
 * nssPKIXBuiltInStandardAttributes_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInStandardAttributes upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInStandardAttributes *
nssPKIXBuiltInStandardAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXBuiltInStandardAttributes_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_COUNTRY_NAME
 *  NSS_ERROR_INVALID_PKIX_ADMINISTRATION_DOMAIN_NAME
 *  NSS_ERROR_INVALID_PKIX_NETWORK_ADDRESS
 *  NSS_ERROR_INVALID_PKIX_TERMINAL_IDENTIFIER
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_DOMAIN_NAME
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATION_NAME
 *  NSS_ERROR_INVALID_PKIX_NUMERIC_USER_IDENTIFIER
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInStandardAttributes upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInStandardAttributes *
nssPKIXBuiltInStandardAttributes_Create
(
  NSSArena *arenaOpt,
  NSSPKIXCountryName *countryNameOpt,
  NSSPKIXAdministrationDomainName *administrationDomainNameOpt,
  NSSPKIXNetworkAddress *networkAddressOpt,
  NSSPKIXTerminalIdentifier *terminalIdentifierOpt,
  NSSPKIXPrivateDomainName *privateDomainNameOpt,
  NSSPKIXOrganizationName *organizationNameOpt,
  NSSPKIXNumericUserIdentifier *numericUserIdentifierOpt,
  NSSPKIXPersonalName *personalNameOpt,
  NSSPKIXOrganizationalUnitNames *organizationalUnitNamesOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_Destroy
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXBuiltInStandardAttributes_Encode
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_HasCountryName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_HasCountryName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_GetCountryName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCountryName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCountryName *
nssPKIXBuiltInStandardAttributes_GetCountryName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_SetCountryName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_COUNTRY_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_SetCountryName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXCountryName *countryName
);

/*
 * nssPKIXBuiltInStandardAttributes_RemoveCountryName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_COUNTRY_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_RemoveCountryName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_HasAdministrationDomainName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_HasAdministrationDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_GetAdministrationDomainName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAdministrationDomainName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAdministrationDomainName *
nssPKIXBuiltInStandardAttributes_GetAdministrationDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_SetAdministrationDomainName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_ADMINISTRATION_DOMAIN_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_SetAdministrationDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXAdministrationDomainName *administrationDomainName
);

/*
 * nssPKIXBuiltInStandardAttributes_RemoveAdministrationDomainName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_ADMINISTRATION_DOMAIN_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_RemoveAdministrationDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_HasNetworkAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_HasNetworkAddress
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_GetNetworkAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXNetworkAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNetworkAddress *
nssPKIXBuiltInStandardAttributes_GetNetworkAddress
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_SetNetworkAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_NETWORK_ADDRESS
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_SetNetworkAddress
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXNetworkAddress *networkAddress
);

/*
 * nssPKIXBuiltInStandardAttributes_RemoveNetworkAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_NETWORK_ADDRESS
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_RemoveNetworkAddress
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_HasTerminalIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_HasTerminalIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_GetTerminalIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTerminalIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTerminalIdentifier *
nssPKIXBuiltInStandardAttributes_GetTerminalIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_SetTerminalIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_TERMINAL_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_SetTerminalIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXTerminalIdentifier *terminalIdentifier
);

/*
 * nssPKIXBuiltInStandardAttributes_RemoveTerminalIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_TERMINAL_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_RemoveTerminalIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_HasPrivateDomainName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_HasPrivateDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_GetPrivateDomainName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPrivateDomainName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPrivateDomainName *
nssPKIXBuiltInStandardAttributes_GetPrivateDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_SetPrivateDomainName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_DOMAIN_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_SetPrivateDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXPrivateDomainName *privateDomainName
);

/*
 * nssPKIXBuiltInStandardAttributes_RemovePrivateDomainName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_PRIVATE_DOMAIN_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_RemovePrivateDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_HasOrganizationName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_HasOrganizationName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_GetOrganizationName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationName *
nssPKIXBuiltInStandardAttributes_GetOrganizationName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_SetOrganizationName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATION_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_SetOrganizationName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXOrganizationName *organizationName
);

/*
 * nssPKIXBuiltInStandardAttributes_RemoveOrganizationName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_ORGANIZATION_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_RemoveOrganizationName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_HasNumericUserIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_HasNumericUserIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_GetNumericUserIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXNumericUserIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNumericUserIdentifier *
nssPKIXBuiltInStandardAttributes_GetNumericUserIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_SetNumericUserIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_NUMERIC_USER_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_SetNumericUserIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXNumericUserIdentifier *numericUserIdentifier
);

/*
 * nssPKIXBuiltInStandardAttributes_RemoveNumericUserIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_NUMERIC_USER_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_RemoveNumericUserIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_HasPersonalName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_HasPersonalName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_GetPersonalName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPersonalName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPersonalName *
nssPKIXBuiltInStandardAttributes_GetPersonalName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_SetPersonalName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_SetPersonalName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXPersonalName *personalName
);

/*
 * nssPKIXBuiltInStandardAttributes_RemovePersonalName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_RemovePersonalName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_HasOrganizationLUnitNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_HasOrganizationLUnitNames
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_GetOrganizationLUnitNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationalUnitNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationalUnitNames *
nssPKIXBuiltInStandardAttributes_GetOrganizationLUnitNames
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_SetOrganizationLUnitNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_SetOrganizationLUnitNames
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXOrganizationalUnitNames *organizationalUnitNames
);

/*
 * nssPKIXBuiltInStandardAttributes_RemoveOrganizationLUnitNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_ORGANIZATIONAL_UNIT_NAMES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_RemoveOrganizationLUnitNames
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * nssPKIXBuiltInStandardAttributes_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXBuiltInStandardAttributes_Equal
(
  NSSPKIXBuiltInStandardAttributes *bisa1,
  NSSPKIXBuiltInStandardAttributes *bisa2,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInStandardAttributes_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInStandardAttributes upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInStandardAttributes *
nssPKIXBuiltInStandardAttributes_Duplicate
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXBuiltInStandardAttributes_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXBuiltInStandardAttributes
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_STANDARD_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInStandardAttributes_verifyPointer
(
  NSSPKIXBuiltInStandardAttributes *p
);
#endif /* DEBUG */

/*
 * CountryName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CountryName ::= [APPLICATION 1] CHOICE {
 *     x121-dcc-code NumericString
 *                  (SIZE (ub-country-name-numeric-length)),
 *     iso-3166-alpha2-code PrintableString
 *                  (SIZE (ub-country-name-alpha-length)) }
 *
 * The private calls for this type:
 *
 *  nssPKIXCountryName_Decode
 *  nssPKIXCountryName_CreateFromUTF8
 *  nssPKIXCountryName_Encode
 *
 */

/*
 * nssPKIXCountryName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCountryName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCountryName *
nssPKIXCountryName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXCountryName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCountryName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCountryName *
nssPKIXCountryName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXCountryName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_COUNTRY_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXCountryName_Encode
(
  NSSPKIXCountryName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * AdministrationDomainName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AdministrationDomainName ::= [APPLICATION 2] CHOICE {
 *     numeric NumericString (SIZE (0..ub-domain-name-length)),
 *     printable PrintableString (SIZE (0..ub-domain-name-length)) }
 *
 * The private calls for this type:
 *
 *  nssPKIXAdministrationDomainName_Decode
 *  nssPKIXAdministrationDomainName_CreateFromUTF8
 *  nssPKIXAdministrationDomainName_Encode
 *
 */

/*
 * nssPKIXAdministrationDomainName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAdministrationDomainName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAdministrationDomainName *
nssPKIXAdministrationDomainName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXAdministrationDomainName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAdministrationDomainName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAdministrationDomainName *
nssPKIXAdministrationDomainName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXAdministrationDomainName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ADMINISTRATION_DOMAIN_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXAdministrationDomainName_Encode
(
  NSSPKIXAdministrationDomainName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * X121Address
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  X121Address ::= NumericString (SIZE (1..ub-x121-address-length))
 *
 * The private calls for this type:
 *
 *  nssPKIXX121Address_Decode
 *  nssPKIXX121Address_CreateFromUTF8
 *  nssPKIXX121Address_Encode
 *
 */

/*
 * nssPKIXX121Address_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX121Address upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX121Address *
nssPKIXX121Address_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXX121Address_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX121Address upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX121Address *
nssPKIXX121Address_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXX121Address_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_X121_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXX121Address_Encode
(
  NSSPKIXX121Address *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NetworkAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  NetworkAddress ::= X121Address  -- see also extended-network-address
 *
 * The private calls for this type:
 *
 *  nssPKIXNetworkAddress_Decode
 *  nssPKIXNetworkAddress_CreateFromUTF8
 *  nssPKIXNetworkAddress_Encode
 *
 */

/*
 * nssPKIXNetworkAddress_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNetworkAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNetworkAddress *
nssPKIXNetworkAddress_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXNetworkAddress_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNetworkAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNetworkAddress *
nssPKIXNetworkAddress_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXNetworkAddress_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NETWORK_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXNetworkAddress_Encode
(
  NSSPKIXNetworkAddress *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * TerminalIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TerminalIdentifier ::= PrintableString (SIZE (1..ub-terminal-id-length))
 *
 * The private calls for this type:
 *
 *  nssPKIXTerminalIdentifier_Decode
 *  nssPKIXTerminalIdentifier_CreateFromUTF8
 *  nssPKIXTerminalIdentifier_Encode
 *
 */

/*
 * nssPKIXTerminalIdentifier_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTerminalIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTerminalIdentifier *
nssPKIXTerminalIdentifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTerminalIdentifier_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTerminalIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTerminalIdentifier *
nssPKIXTerminalIdentifier_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXTerminalIdentifier_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TERMINAL_IDENTIFIER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTerminalIdentifier_Encode
(
  NSSPKIXTerminalIdentifier *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * PrivateDomainName
 *
 * -- fgmr comments -- 
 *
 *  PrivateDomainName ::= CHOICE {
 *     numeric NumericString (SIZE (1..ub-domain-name-length)),
 *     printable PrintableString (SIZE (1..ub-domain-name-length)) }
 *
 * The private calls for this type:
 *
 *  nssPKIXPrivateDomainName_Decode
 *  nssPKIXPrivateDomainName_CreateFromUTF8
 *  nssPKIXPrivateDomainName_Encode
 *
 */

/*
 * nssPKIXPrivateDomainName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPrivateDomainName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPrivateDomainName *
nssPKIXPrivateDomainName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPrivateDomainName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPrivateDomainName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPrivateDomainName *
nssPKIXPrivateDomainName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXPrivateDomainName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_DOMAIN_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPrivateDomainName_Encode
(
  NSSPKIXPrivateDomainName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * OrganizationName
 *
 * -- fgmr comments --
 *
 *  OrganizationName ::= PrintableString
 *                              (SIZE (1..ub-organization-name-length))
 * 
 * The private calls for this type:
 *
 *  nssPKIXOrganizationName_Decode
 *  nssPKIXOrganizationName_CreateFromUTF8
 *  nssPKIXOrganizationName_Encode
 *
 */

/*
 * nssPKIXOrganizationName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationName *
nssPKIXOrganizationName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXOrganizationName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationName *
nssPKIXOrganizationName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXOrganizationName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATION_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXOrganizationName_Encode
(
  NSSPKIXOrganizationName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NumericUserIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  NumericUserIdentifier ::= NumericString
 *                              (SIZE (1..ub-numeric-user-id-length))
 * 
 * The private calls for this type:
 *
 *  nssPKIXNumericUserIdentifier_Decode
 *  nssPKIXNumericUserIdentifier_CreateFromUTF8
 *  nssPKIXNumericUserIdentifier_Encode
 *
 */

/*
 * nssPKIXNumericUserIdentifier_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNumericUserIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNumericUserIdentifier *
nssPKIXNumericUserIdentifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXNumericUserIdentifier_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_UTF8
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNumericUserIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNumericUserIdentifier *
nssPKIXNumericUserIdentifier_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXNumericUserIdentifier_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NUMERIC_USER_IDENTIFIER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXNumericUserIdentifier_Encode
(
  NSSPKIXNumericUserIdentifier *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * PersonalName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PersonalName ::= SET {
 *     surname [0] PrintableString (SIZE (1..ub-surname-length)),
 *     given-name [1] PrintableString
 *                          (SIZE (1..ub-given-name-length)) OPTIONAL,
 *     initials [2] PrintableString (SIZE (1..ub-initials-length)) OPTIONAL,
 *     generation-qualifier [3] PrintableString
 *                  (SIZE (1..ub-generation-qualifier-length)) OPTIONAL }
 *
 * The private calls for this type:
 *
 *  nssPKIXPersonalName_Decode
 *  nssPKIXPersonalName_Create
 *  nssPKIXPersonalName_Destroy
 *  nssPKIXPersonalName_Encode
 *  nssPKIXPersonalName_GetSurname
 *  nssPKIXPersonalName_SetSurname
 *  nssPKIXPersonalName_HasGivenName
 *  nssPKIXPersonalName_GetGivenName
 *  nssPKIXPersonalName_SetGivenName
 *  nssPKIXPersonalName_RemoveGivenName
 *  nssPKIXPersonalName_HasInitials
 *  nssPKIXPersonalName_GetInitials
 *  nssPKIXPersonalName_SetInitials
 *  nssPKIXPersonalName_RemoveInitials
 *  nssPKIXPersonalName_HasGenerationQualifier
 *  nssPKIXPersonalName_GetGenerationQualifier
 *  nssPKIXPersonalName_SetGenerationQualifier
 *  nssPKIXPersonalName_RemoveGenerationQualifier
 *  nssPKIXPersonalName_Equal
 *  nssPKIXPersonalName_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXPersonalName_verifyPointer
 *
 */

/*
 * nssPKIXPersonalName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPersonalName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPersonalName *
nssPKIXPersonalName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPersonalName_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPersonalName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPersonalName *
nssPKIXPersonalName_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *surname,
  NSSUTF8 *givenNameOpt,
  NSSUTF8 *initialsOpt,
  NSSUTF8 *generationQualifierOpt
);

/*
 * nssPKIXPersonalName_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPersonalName_Destroy
(
  NSSPKIXPersonalName *personalName
);

/*
 * nssPKIXPersonalName_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPersonalName_Encode
(
  NSSPKIXPersonalName *personalName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPersonalName_GetSurname
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXPersonalName_GetSurname
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPersonalName_SetSurname
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPersonalName_SetSurname
(
  NSSPKIXPersonalName *personalName,
  NSSUTF8 *surname
);

/*
 * nssPKIXPersonalName_HasGivenName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPersonalName_HasGivenName
(
  NSSPKIXPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * nssPKIXPersonalName_GetGivenName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_HAS_NO_GIVEN_NAME
 *
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXPersonalName_GetGivenName
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPersonalName_SetGivenName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPersonalName_SetGivenName
(
  NSSPKIXPersonalName *personalName,
  NSSUTF8 *givenName
);

/*
 * nssPKIXPersonalName_RemoveGivenName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *  NSS_ERROR_HAS_NO_GIVEN_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPersonalName_RemoveGivenName
(
  NSSPKIXPersonalName *personalName
);

/*
 * nssPKIXPersonalName_HasInitials
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPersonalName_HasInitials
(
  NSSPKIXPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * nssPKIXPersonalName_GetInitials
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXPersonalName_GetInitials
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPersonalName_SetInitials
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPersonalName_SetInitials
(
  NSSPKIXPersonalName *personalName,
  NSSUTF8 *initials
);

/*
 * nssPKIXPersonalName_RemoveInitials
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPersonalName_RemoveInitials
(
  NSSPKIXPersonalName *personalName
);

/*
 * nssPKIXPersonalName_HasGenerationQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPersonalName_HasGenerationQualifier
(
  NSSPKIXPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * nssPKIXPersonalName_GetGenerationQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXPersonalName_GetGenerationQualifier
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPersonalName_SetGenerationQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPersonalName_SetGenerationQualifier
(
  NSSPKIXPersonalName *personalName,
  NSSUTF8 *generationQualifier
);

/*
 * nssPKIXPersonalName_RemoveGenerationQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPersonalName_RemoveGenerationQualifier
(
  NSSPKIXPersonalName *personalName
);

/*
 * nssPKIXPersonalName_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXPersonalName_Equal
(
  NSSPKIXPersonalName *personalName1,
  NSSPKIXPersonalName *personalName2,
  PRStatus *statusOpt
);

/*
 * nssPKIXPersonalName_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPersonalName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPersonalName *
nssPKIXPersonalName_Duplicate
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXPersonalName_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXPersonalName
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXPersonalName_verifyPointer
(
  NSSPKIXPersonalName *p
);
#endif /* DEBUG */

/*
 * OrganizationalUnitNames
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  OrganizationalUnitNames ::= SEQUENCE SIZE (1..ub-organizational-units)
 *                                          OF OrganizationalUnitName
 * 
 * The private calls for the type:
 *
 *  nssPKIXOrganizationalUnitNames_Decode
 *  nssPKIXOrganizationalUnitNames_Create
 *  nssPKIXOrganizationalUnitNames_Destroy
 *  nssPKIXOrganizationalUnitNames_Encode
 *  nssPKIXOrganizationalUnitNames_GetOrganizationalUnitNameCount
 *  nssPKIXOrganizationalUnitNames_GetOrganizationalUnitNames
 *  nssPKIXOrganizationalUnitNames_SetOrganizationalUnitNames
 *  nssPKIXOrganizationalUnitNames_GetOrganizationalUnitName
 *  nssPKIXOrganizationalUnitNames_SetOrganizationalUnitName
 *  nssPKIXOrganizationalUnitNames_InsertOrganizationalUnitName
 *  nssPKIXOrganizationalUnitNames_AppendOrganizationalUnitName
 *  nssPKIXOrganizationalUnitNames_RemoveOrganizationalUnitName
 *  nssPKIXOrganizationalUnitNames_FindOrganizationalUnitName
 *  nssPKIXOrganizationalUnitNames_Equal
 *  nssPKIXOrganizationalUnitNames_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXOrganizationalUnitNames_verifyPointer
 *
 */

/*
 * nssPKIXOrganizationalUnitNames_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationalUnitNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationalUnitNames *
nssPKIXOrganizationalUnitNames_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXOrganizationalUnitNames_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAME
 *
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationalUnitNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationalUnitNames *
nssPKIXOrganizationalUnitNames_Create
(
  NSSArena *arenaOpt,
  NSSPKIXOrganizationalUnitName *ou1,
  ...
);

/*
 * nssPKIXOrganizationalUnitNames_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXOrganizationalUnitNames_Destroy
(
  NSSPKIXOrganizationalUnitNames *ous
);

/*
 * nssPKIXOrganizationalUnitNames_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXOrganizationalUnitNames_Encode
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXOrganizationalUnitNames_GetOrganizationalUnitNameCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXOrganizationalUnitNames_GetOrganizationalUnitNameCount
(
  NSSPKIXOrganizationalUnitNames *ous
);

/*
 * nssPKIXOrganizationalUnitNames_GetOrganizationalUnitNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  A valid pointer to an array of NSSPKIXOrganizationalUnitName
 *      pointers upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationalUnitName **
nssPKIXOrganizationalUnitNames_GetOrganizationalUnitNames
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSPKIXOrganizationalUnitName *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXOrganizationalUnitNames_SetOrganizationalUnitNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXOrganizationalUnitNames_SetOrganizationalUnitNames
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSPKIXOrganizationalUnitName *ou[],
  PRInt32 count
);

/*
 * nssPKIXOrganizationalUnitNames_GetOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationalUnitName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationalUnitName *
nssPKIXOrganizationalUnitNames_GetOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXOrganizationalUnitNames_SetOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXOrganizationalUnitNames_SetOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSPKIXOrganizationalUnitName *ou
);

/*
 * nssPKIXOrganizationalUnitNames_InsertOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXOrganizationalUnitNames_InsertOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSPKIXOrganizationalUnitName *ou
);

/*
 * nssPKIXOrganizationalUnitNames_AppendOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXOrganizationalUnitNames_AppendOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSPKIXOrganizationalUnitName *ou
);

/*
 * nssPKIXOrganizationalUnitNames_RemoveOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXOrganizationalUnitNames_RemoveOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  PRInt32 i
);

/*
 * nssPKIXOrganizationalUnitNames_FindOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAME
 *
 * Return value:
 *  The index of the specified revoked certificate upon success
 *  -1 upon failure
 */

NSS_EXTERN PRIntn
nssPKIXOrganizationalUnitNames_FindOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSPKIXOrganizationalUnitName *ou
);

/*
 * nssPKIXOrganizationalUnitNames_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXOrganizationalUnitNames_Equal
(
  NSSPKIXOrganizationalUnitNames *ous1,
  NSSPKIXOrganizationalUnitNames *ous2,
  PRStatus *statusOpt
);

/*
 * nssPKIXOrganizationalUnitNames_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationalUnitNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationalUnitNames *
nssPKIXOrganizationalUnitNames_Duplicate
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXOrganizationalUnitNames_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXOrganizationalUnitNames
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAMES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXOrganizationalUnitNames_verifyPointer
(
  NSSPKIXOrganizationalUnitNames *p
);
#endif /* DEBUG */

/*
 * OrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  OrganizationalUnitName ::= PrintableString (SIZE
 *                          (1..ub-organizational-unit-name-length))
 *
 * The private calls for this type:
 *
 *  nssPKIXOrganizationalUnitName_Decode
 *  nssPKIXOrganizationalUnitName_CreateFromUTF8
 *  nssPKIXOrganizationalUnitName_Encode
 *
 */

/*
 * nssPKIXOrganizationalUnitName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationalUnitName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationalUnitName *
nssPKIXOrganizationalUnitName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXOrganizationalUnitName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXOrganizationalUnitName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXOrganizationalUnitName *
nssPKIXOrganizationalUnitName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXOrganizationalUnitName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXOrganizationalUnitName_Encode
(
  NSSPKIXOrganizationalUnitName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * BuiltInDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BuiltInDomainDefinedAttributes ::= SEQUENCE SIZE
 *                                  (1..ub-domain-defined-attributes) OF
 *                                  BuiltInDomainDefinedAttribute
 *
 * The private calls for this type:
 *
 *  nssPKIXBuiltInDomainDefinedAttributes_Decode
 *  nssPKIXBuiltInDomainDefinedAttributes_Create
 *  nssPKIXBuiltInDomainDefinedAttributes_Destroy
 *  nssPKIXBuiltInDomainDefinedAttributes_Encode
 *  nssPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributeCount
 *  nssPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributes
 *  nssPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttributes
 *  nssPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttribute
 *  nssPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttribute
 *  nssPKIXBuiltInDomainDefinedAttributes_InsertBuiltIndomainDefinedAttribute
 *  nssPKIXBuiltInDomainDefinedAttributes_AppendBuiltIndomainDefinedAttribute
 *  nssPKIXBuiltInDomainDefinedAttributes_RemoveBuiltIndomainDefinedAttribute
 *  nssPKIXBuiltInDomainDefinedAttributes_Equal
 *  nssPKIXBuiltInDomainDefinedAttributes_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXBuiltInDomainDefinedAttributes_verifyPointer
 *
 */

/*
 * nssPKIXBuiltInDomainDefinedAttributes_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInDomainDefinedAttributes
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInDomainDefinedAttributes *
nssPKIXBuiltInDomainDefinedAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInDomainDefinedAttributes
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInDomainDefinedAttributes *
nssPKIXBuiltInDomainDefinedAttributes_Create
(
  NSSArena *arenaOpt,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda1,
  ...
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttributes_Destroy
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXBuiltInDomainDefinedAttributes_Encode
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributeCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributeCount
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXBuiltInDomainDefinedAttribute
 *      pointers upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInDomainDefinedAttribute **
nssPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributes
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSPKIXBuiltInDomainDefinedAttribut *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttributes
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSPKIXBuiltInDomainDefinedAttribut *bidda[],
  PRInt32 count
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInDomainDefinedAttribute
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInDomainDefinedAttribute *
nssPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  PRInt32 i,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_InsertBuiltIndomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttributes_InsertBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  PRInt32 i,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_AppendBuiltIndomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttributes_AppendBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_RemoveBuiltIndomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttributes_RemoveBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  PRInt32 i
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_FindBuiltIndomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 * 
 * Return value:
 *  The index of the specified revoked certificate upon success
 *  -1 upon failure
 */

NSS_EXTERN PRInt32
nssPKIXBuiltInDomainDefinedAttributes_FindBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXBuiltInDomainDefinedAttributes_Equal
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas1,
  NSSPKIXBuiltInDomainDefinedAttributes *biddas2,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInDomainDefinedAttributes_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInDomainDefinedAttributes
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInDomainDefinedAttributes *
nssPKIXBuiltInDomainDefinedAttributes_Duplicate
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXBuiltInDomainDefinedAttributes_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXBuiltInDomainDefinedAttributes
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttributes_verifyPointer
(
  NSSPKIXBuiltInDomainDefinedAttributes *p
);
#endif /* DEBUG */

/*
 * BuiltInDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BuiltInDomainDefinedAttribute ::= SEQUENCE {
 *     type PrintableString (SIZE
 *                          (1..ub-domain-defined-attribute-type-length)),
 *     value PrintableString (SIZE
 *                          (1..ub-domain-defined-attribute-value-length))}
 *
 * The private calls for this type:
 *
 *  nssPKIXBuiltInDomainDefinedAttribute_Decode
 *  nssPKIXBuiltInDomainDefinedAttribute_Create
 *  nssPKIXBuiltInDomainDefinedAttribute_Destroy
 *  nssPKIXBuiltInDomainDefinedAttribute_Encode
 *  nssPKIXBuiltInDomainDefinedAttribute_GetType
 *  nssPKIXBuiltInDomainDefinedAttribute_SetType
 *  nssPKIXBuiltInDomainDefinedAttribute_GetValue
 *  nssPKIXBuiltInDomainDefinedAttribute_SetValue
 *  nssPKIXBuiltInDomainDefinedAttribute_Equal
 *  nssPKIXBuiltInDomainDefinedAttribute_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXBuiltInDomainDefinedAttribute_verifyPointer
 *
 */

/*
 * nssPKIXBuiltInDomainDefinedAttribute_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInDomainDefinedAttribute
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInDomainDefinedAttribute *
nssPKIXBuiltInDomainDefinedAttribute_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXBuiltInDomainDefinedAttribute_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInDomainDefinedAttribute
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInDomainDefinedAttribute *
nssPKIXBuiltInDomainDefinedAttribute_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *type,
  NSSUTF8 *value
);

/*
 * nssPKIXBuiltInDomainDefinedAttribute_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttribute_Destroy
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * nssPKIXBuiltInDomainDefinedAttribute_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXBuiltInDomainDefinedAttribute_Encode
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInDomainDefinedAttribute_GetType
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXBuiltInDomainDefinedAttribute_GetType
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInDomainDefinedAttribute_SetType
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttribute_SetType
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSUTF8 *type
);

/*
 * nssPKIXBuiltInDomainDefinedAttribute_GetValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXBuiltInDomainDefinedAttribute_GetValue
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBuiltInDomainDefinedAttribute_SetValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttribute_SetValue
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSUTF8 *value
);

/*
 * nssPKIXBuiltInDomainDefinedAttribute_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXBuiltInDomainDefinedAttribute_Equal
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda1,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda2,
  PRStatus *statusOpt
);

/*
 * nssPKIXBuiltInDomainDefinedAttribute_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBuiltInDomainDefinedAttribute
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBuiltInDomainDefinedAttribute *
nssPKIXBuiltInDomainDefinedAttribute_Duplicate
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXBuiltInDomainDefinedAttribute_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXBuiltInDomainDefinedAttribute
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BUILT_IN_DOMAIN_DEFINED_ATTRIBUTE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXBuiltInDomainDefinedAttribute_verifyPointer
(
  NSSPKIXBuiltInDomainDefinedAttribute *p
);
#endif /* DEBUG */

/*
 * ExtensionAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtensionAttributes ::= SET SIZE (1..ub-extension-attributes) OF
 *                          ExtensionAttribute
 *
 * The private calls for this type:
 *
 *  nssPKIXExtensionAttributes_Decode
 *  nssPKIXExtensionAttributes_Create
 *  nssPKIXExtensionAttributes_Destroy
 *  nssPKIXExtensionAttributes_Encode
 *  nssPKIXExtensionAttributes_GetExtensionAttributeCount
 *  nssPKIXExtensionAttributes_GetExtensionAttributes
 *  nssPKIXExtensionAttributes_SetExtensionAttributes
 *  nssPKIXExtensionAttributes_GetExtensionAttribute
 *  nssPKIXExtensionAttributes_SetExtensionAttribute
 *  nssPKIXExtensionAttributes_InsertExtensionAttribute
 *  nssPKIXExtensionAttributes_AppendExtensionAttribute
 *  nssPKIXExtensionAttributes_RemoveExtensionAttribute
 *  nssPKIXExtensionAttributes_FindExtensionAttribute
 *  nssPKIXExtensionAttributes_Equal
 *  nssPKIXExtensionAttributes_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXExtensionAttributes_verifyPointer
 *
 */

/*
 * nssPKIXExtensionAttributes_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtensionAttributes upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttributes *
nssPKIXExtensionAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXExtensionAttributes_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtensionAttributes upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttributes *
nssPKIXExtensionAttributes_Create
(
  NSSArena *arenaOpt,
  NSSPKIXExtensionAttribute ea1,
  ...
);

/*
 * nssPKIXExtensionAttributes_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttributes_Destroy
(
  NSSPKIXExtensionAttributes *eas
);

/*
 * nssPKIXExtensionAttributes_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXExtensionAttributes_Encode
(
  NSSPKIXExtensionAttributes *eas
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtensionAttributes_GetExtensionAttributeCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXExtensionAttributes_GetExtensionAttributeCount
(
  NSSPKIXExtensionAttributes *eas
);

/*
 * nssPKIXExtensionAttributes_GetExtensionAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXExtensionAttribute pointers
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttribute **
nssPKIXExtensionAttributes_GetExtensionAttributes
(
  NSSPKIXExtensionAttributes *eas,
  NSSPKIXExtensionAttribute *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtensionAttributes_SetExtensionAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 *  NSS_ERROR_INVALID_POINTER
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttributes_SetExtensionAttributes
(
  NSSPKIXExtensionAttributes *eas,
  NSSPKIXExtensionAttribute *ea[],
  PRInt32 count
);

/*
 * nssPKIXExtensionAttributes_GetExtensionAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtensionAttribute upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttribute *
nssPKIXExtensionAttributes_GetExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtensionAttributes_SetExtensionAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttributes_SetExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  PRInt32 i,
  NSSPKIXExtensionAttribute *ea
);

/*
 * nssPKIXExtensionAttributes_InsertExtensionAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttributes_InsertExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  PRInt32 i,
  NSSPKIXExtensionAttribute *ea
);

/*
 * nssPKIXExtensionAttributes_AppendExtensionAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttributes_AppendExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  NSSPKIXExtensionAttribute *ea
);

/*
 * nssPKIXExtensionAttributes_RemoveExtensionAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttributes_RemoveExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  PRInt32 i,
);

/*
 * nssPKIXExtensionAttributes_FindExtensionAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 * 
 * Return value:
 *  A nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXExtensionAttributes_FindExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  NSSPKIXExtensionAttribute *ea
);

/*
 * nssPKIXExtensionAttributes_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXExtensionAttributes_Equal
(
  NSSPKIXExtensionAttributes *eas1,
  NSSPKIXExtensionAttributes *eas2,
  PRStatus *statusOpt
);

/*
 * nssPKIXExtensionAttributes_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtensionAttributes upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttributes *
nssPKIXExtensionAttributes_Duplicate
(
  NSSPKIXExtensionAttributes *eas,
  NSSArena *arenaOpt
);

/*
 * fgmr
 * There should be accessors to search the ExtensionAttributes and
 * return the value for a specific value, etc.
 */

#ifdef DEBUG
/*
 * nssPKIXExtensionAttributes_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXExtensionAttributes
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttributes_verifyPointer
(
  NSSPKIXExtensionAttributes *p
);
#endif /* DEBUG */

/*
 * ExtensionAttribute
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtensionAttribute ::=  SEQUENCE {
 *     extension-attribute-type [0] INTEGER (0..ub-extension-attributes),
 *     extension-attribute-value [1]
 *                          ANY DEFINED BY extension-attribute-type }
 *
 * The private calls for this type:
 *
 *  nssPKIXExtensionAttribute_Decode
 *  nssPKIXExtensionAttribute_Create
 *  nssPKIXExtensionAttribute_Destroy
 *  nssPKIXExtensionAttribute_Encode
 *  nssPKIXExtensionAttribute_GetExtensionsAttributeType
 *  nssPKIXExtensionAttribute_SetExtensionsAttributeType
 *  nssPKIXExtensionAttribute_GetExtensionsAttributeValue
 *  nssPKIXExtensionAttribute_SetExtensionsAttributeValue
 *  nssPKIXExtensionAttribute_Equal
 *  nssPKIXExtensionAttribute_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXExtensionAttribute_verifyPointer
 *
 */

/*
 * nssPKIXExtensionAttribute_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtensionAttribute upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttribute *
nssPKIXExtensionAttribute_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXExtensionAttribute_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE_TYPE
 *  NSS_ERROR_INVALID_POINTER
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtensionAttribute upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttribute *
nssPKIXExtensionAttribute_Create
(
  NSSArena *arenaOpt,
  NSSPKIXExtensionAttributeType type,
  NSSItem *value
);

/*
 * nssPKIXExtensionAttribute_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttribute_Destroy
(
  NSSPKIXExtensionAttribute *ea
);

/*
 * nssPKIXExtensionAttribute_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXExtensionAttribute_Encode
(
  NSSPKIXExtensionAttribute *ea,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtensionAttribute_GetExtensionsAttributeType
 *
 * -- fgmr comments --
 * {One of these objects created from BER generated by a program
 * adhering to a later version of the PKIX standards might have
 * a value not mentioned in the enumeration definition.  This isn't
 * a bug.}
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 * 
 * Return value:
 *  A member of the NSSPKIXExtensionAttributeType enumeration
 *      upon success
 *  NSSPKIXExtensionAttributeType_NSSinvalid upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttributeType
nssPKIXExtensionAttribute_GetExtensionsAttributeType
(
  NSSPKIXExtensionAttribute *ea
);

/*
 * nssPKIXExtensionAttribute_SetExtensionsAttributeType
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttribute_SetExtensionsAttributeType
(
  NSSPKIXExtensionAttribute *ea,
  NSSPKIXExtensionAttributeType type
);

/*
 * nssPKIXExtensionAttribute_GetExtensionsAttributeValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSItem *
nssPKIXExtensionAttribute_GetExtensionsAttributeValue
(
  NSSPKIXExtensionAttribute *ea,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtensionAttribute_SetExtensionsAttributeValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttribute_SetExtensionsAttributeValue
(
  NSSPKIXExtensionAttribute *ea,
  NSSItem *value
);

/*
 * nssPKIXExtensionAttribute_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXExtensionAttribute_Equal
(
  NSSPKIXExtensionAttribute *ea1,
  NSSPKIXExtensionAttribute *ea2,
  PRStatus *statusOpt
);

/*
 * nssPKIXExtensionAttribute_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtensionAttribute upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtensionAttribute *
nssPKIXExtensionAttribute_Duplicate
(
  NSSPKIXExtensionAttribute *ea,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXExtensionAttribute_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXExtensionAttribute
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENSION_ATTRIBUTE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXExtensionAttribute_verifyPointer
(
  NSSPKIXExtensionAttribute *p
);
#endif /* DEBUG */

/*
 * CommonName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CommonName ::= PrintableString (SIZE (1..ub-common-name-length))
 *
 * The private calls for this type:
 *
 *  nssPKIXCommonName_Decode
 *  nssPKIXCommonName_CreateFromUTF8
 *  nssPKIXCommonName_Encode
 *
 */

/*
 * nssPKIXCommonName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCommonName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCommonName *
nssPKIXCommonName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXCommonName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCommonName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCommonName *
nssPKIXCommonName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXCommonName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_COMMON_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXCommonName_Encode
(
  NSSPKIXCommonName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * TeletexCommonName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexCommonName ::= TeletexString (SIZE (1..ub-common-name-length))
 *
 * The private calls for this type:
 *
 *  nssPKIXTeletexCommonName_Decode
 *  nssPKIXTeletexCommonName_CreateFromUTF8
 *  nssPKIXTeletexCommonName_Encode
 *
 */

/*
 * nssPKIXTeletexCommonName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexCommonName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexCommonName *
nssPKIXTeletexCommonName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTeletexCommonName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexCommonName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexCommonName *
nssPKIXTeletexCommonName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXTeletexCommonName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_COMMON_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTeletexCommonName_Encode
(
  NSSPKIXTeletexCommonName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * TeletexOrganizationName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexOrganizationName ::=
 *                  TeletexString (SIZE (1..ub-organization-name-length))
 *
 * The private calls for this type:
 *
 *  nssPKIXTeletexOrganizationName_Decode
 *  nssPKIXTeletexOrganizationName_CreateFromUTF8
 *  nssPKIXTeletexOrganizationName_Encode
 *
 */

/*
 * nssPKIXTeletexOrganizationName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexOrganizationName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexOrganizationName *
nssPKIXTeletexOrganizationName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTeletexOrganizationName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexOrganizationName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexOrganizationName *
nssPKIXTeletexOrganizationName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXTeletexOrganizationName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATION_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTeletexOrganizationName_Encode
(
  NSSPKIXTeletexOrganizationName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * TeletexPersonalName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexPersonalName ::= SET {
 *     surname [0] TeletexString (SIZE (1..ub-surname-length)),
 *     given-name [1] TeletexString
 *                  (SIZE (1..ub-given-name-length)) OPTIONAL,
 *     initials [2] TeletexString (SIZE (1..ub-initials-length)) OPTIONAL,
 *     generation-qualifier [3] TeletexString (SIZE
 *                  (1..ub-generation-qualifier-length)) OPTIONAL }
 *
 * The private calls for this type:
 *
 *  nssPKIXTeletexPersonalName_Decode
 *  nssPKIXTeletexPersonalName_Create
 *  nssPKIXTeletexPersonalName_Destroy
 *  nssPKIXTeletexPersonalName_Encode
 *  nssPKIXTeletexPersonalName_GetSurname
 *  nssPKIXTeletexPersonalName_SetSurname
 *  nssPKIXTeletexPersonalName_HasGivenName
 *  nssPKIXTeletexPersonalName_GetGivenName
 *  nssPKIXTeletexPersonalName_SetGivenName
 *  nssPKIXTeletexPersonalName_RemoveGivenName
 *  nssPKIXTeletexPersonalName_HasInitials
 *  nssPKIXTeletexPersonalName_GetInitials
 *  nssPKIXTeletexPersonalName_SetInitials
 *  nssPKIXTeletexPersonalName_RemoveInitials
 *  nssPKIXTeletexPersonalName_HasGenerationQualifier
 *  nssPKIXTeletexPersonalName_GetGenerationQualifier
 *  nssPKIXTeletexPersonalName_SetGenerationQualifier
 *  nssPKIXTeletexPersonalName_RemoveGenerationQualifier
 *  nssPKIXTeletexPersonalName_Equal
 *  nssPKIXTeletexPersonalName_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXTeletexPersonalName_verifyPointer
 *
 */

/*
 * nssPKIXTeletexPersonalName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexPersonalName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexPersonalName *
nssPKIXTeletexPersonalName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTeletexPersonalName_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexPersonalName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexPersonalName *
nssPKIXTeletexPersonalName_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *surname,
  NSSUTF8 *givenNameOpt,
  NSSUTF8 *initialsOpt,
  NSSUTF8 *generationQualifierOpt
);

/*
 * nssPKIXTeletexPersonalName_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexPersonalName_Destroy
(
  NSSPKIXTeletexPersonalName *personalName
);

/*
 * nssPKIXTeletexPersonalName_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTeletexPersonalName_Encode
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexPersonalName_GetSurname
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXTeletexPersonalName_GetSurname
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexPersonalName_SetSurname
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexPersonalName_SetSurname
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSUTF8 *surname
);

/*
 * nssPKIXTeletexPersonalName_HasGivenName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXTeletexPersonalName_HasGivenName
(
  NSSPKIXTeletexPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * nssPKIXTeletexPersonalName_GetGivenName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXTeletexPersonalName_GetGivenName
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexPersonalName_SetGivenName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexPersonalName_SetGivenName
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSUTF8 *givenName
);

/*
 * nssPKIXTeletexPersonalName_RemoveGivenName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexPersonalName_RemoveGivenName
(
  NSSPKIXTeletexPersonalName *personalName
);

/*
 * nssPKIXTeletexPersonalName_HasInitials
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXTeletexPersonalName_HasInitials
(
  NSSPKIXTeletexPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * nssPKIXTeletexPersonalName_GetInitials
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXTeletexPersonalName_GetInitials
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexPersonalName_SetInitials
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexPersonalName_SetInitials
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSUTF8 *initials
);

/*
 * nssPKIXTeletexPersonalName_RemoveInitials
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexPersonalName_RemoveInitials
(
  NSSPKIXTeletexPersonalName *personalName
);

/*
 * nssPKIXTeletexPersonalName_HasGenerationQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXTeletexPersonalName_HasGenerationQualifier
(
  NSSPKIXTeletexPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * nssPKIXTeletexPersonalName_GetGenerationQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXTeletexPersonalName_GetGenerationQualifier
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexPersonalName_SetGenerationQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexPersonalName_SetGenerationQualifier
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSUTF8 *generationQualifier
);

/*
 * nssPKIXTeletexPersonalName_RemoveGenerationQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexPersonalName_RemoveGenerationQualifier
(
  NSSPKIXTeletexPersonalName *personalName
);

/*
 * nssPKIXTeletexPersonalName_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXTeletexPersonalName_Equal
(
  NSSPKIXTeletexPersonalName *personalName1,
  NSSPKIXTeletexPersonalName *personalName2,
  PRStatus *statusOpt
);

/*
 * nssPKIXTeletexPersonalName_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexPersonalName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexPersonalName *
nssPKIXTeletexPersonalName_Duplicate
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXTeletexPersonalName_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXTeletexPersonalName
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_PERSONAL_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXTeletexPersonalName_verifyPointer
(
  NSSPKIXTeletexPersonalName *p
);
#endif /* DEBUG */

/*
 * TeletexOrganizationalUnitNames
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexOrganizationalUnitNames ::= SEQUENCE SIZE
 *          (1..ub-organizational-units) OF TeletexOrganizationalUnitName
 *
 * The private calls for the type:
 *
 *  nssPKIXTeletexOrganizationalUnitNames_Decode
 *  nssPKIXTeletexOrganizationalUnitNames_Create
 *  nssPKIXTeletexOrganizationalUnitNames_Destroy
 *  nssPKIXTeletexOrganizationalUnitNames_Encode
 *  nssPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNameCount
 *  nssPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNames
 *  nssPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitNames
 *  nssPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitName
 *  nssPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitName
 *  nssPKIXTeletexOrganizationalUnitNames_InsertTeletexOrganizationalUnitName
 *  nssPKIXTeletexOrganizationalUnitNames_AppendTeletexOrganizationalUnitName
 *  nssPKIXTeletexOrganizationalUnitNames_RemoveTeletexOrganizationalUnitName
 *  nssPKIXTeletexOrganizationalUnitNames_FindTeletexOrganizationalUnitName
 *  nssPKIXTeletexOrganizationalUnitNames_Equal
 *  nssPKIXTeletexOrganizationalUnitNames_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXTeletexOrganizationalUnitNames_verifyPointer
 *
 */

/*
 * nssPKIXTeletexOrganizationalUnitNames_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexOrganizationalUnitNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexOrganizationalUnitNames *
nssPKIXTeletexOrganizationalUnitNames_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAME
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexOrganizationalUnitNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexOrganizationalUnitNames *
nssPKIXTeletexOrganizationalUnitNames_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTeletexOrganizationalUnitName *ou1,
  ...
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexOrganizationalUnitNames_Destroy
(
  NSSPKIXTeletexOrganizationalUnitNames *ous
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTeletexOrganizationalUnitNames_Encode
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNameCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNameCount
(
  NSSPKIXTeletexOrganizationalUnitNames *ous
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  A valid pointer to an array of NSSPKIXTeletexOrganizationalUnitName
 *      pointers upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexOrganizationalUnitName **
nssPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNames
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSPKIXTeletexOrganizationalUnitName *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitNames
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSPKIXTeletexOrganizationalUnitName *ou[],
  PRInt32 count
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexOrganizationalUnitName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexOrganizationalUnitName *
nssPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSPKIXTeletexOrganizationalUnitName *ou
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_InsertTeletexOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexOrganizationalUnitNames_InsertTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSPKIXTeletexOrganizationalUnitName *ou
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_AppendTeletexOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexOrganizationalUnitNames_AppendTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSPKIXTeletexOrganizationalUnitName *ou
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_RemoveTeletexOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexOrganizationalUnitNames_RemoveTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  PRInt32 i
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_FindTeletexOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAME
 *
 * Return value:
 *  The index of the specified revoked certificate upon success
 *  -1 upon failure
 */

NSS_EXTERN PRInt32
nssPKIXTeletexOrganizationalUnitNames_FindTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSPKIXTeletexOrganizationalUnitName *ou
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXTeletexOrganizationalUnitNames_Equal
(
  NSSPKIXTeletexOrganizationalUnitNames *ous1,
  NSSPKIXTeletexOrganizationalUnitNames *ous2,
  PRStatus *statusOpt
);

/*
 * nssPKIXTeletexOrganizationalUnitNames_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAMES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexOrganizationalUnitNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexOrganizationalUnitNames *
nssPKIXTeletexOrganizationalUnitNames_Duplicate
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXTeletexOrganizationalUnitNames_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXTeletexOrganizationalUnitNames
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXTeletexOrganizationalUnitNames_verifyPointer
(
  NSSPKIXTeletexOrganizationalUnitNames *p
);
#endif /* DEBUG */

/*
 * TeletexOrganizationalUnitName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexOrganizationalUnitName ::= TeletexString
 *                          (SIZE (1..ub-organizational-unit-name-length))
 *  
 * The private calls for this type:
 *
 *  nssPKIXTeletexOrganizationalUnitName_Decode
 *  nssPKIXTeletexOrganizationalUnitName_CreateFromUTF8
 *  nssPKIXTeletexOrganizationalUnitName_Encode
 *
 */

/*
 * nssPKIXTeletexOrganizationalUnitName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexOrganizationalUnitName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexOrganizationalUnitName *
nssPKIXTeletexOrganizationalUnitName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTeletexOrganizationalUnitName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexOrganizationalUnitName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexOrganizationalUnitName *
nssPKIXTeletexOrganizationalUnitName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXTeletexOrganizationalUnitName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_ORGANIZATIONAL_UNIT_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTeletexOrganizationalUnitName_Encode
(
  NSSPKIXTeletexOrganizationalUnitName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * PDSName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PDSName ::= PrintableString (SIZE (1..ub-pds-name-length))
 *
 * The private calls for this type:
 *
 *  nssPKIXPDSName_Decode
 *  nssPKIXPDSName_CreateFromUTF8
 *  nssPKIXPDSName_Encode
 *
 */

/*
 * nssPKIXPDSName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPDSName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPDSName *
nssPKIXPDSName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPDSName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPDSName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPDSName *
nssPKIXPDSName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXPDSName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPDSName_Encode
(
  NSSPKIXPDSName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * PhysicalDeliveryCountryName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PhysicalDeliveryCountryName ::= CHOICE {
 *     x121-dcc-code NumericString (SIZE (ub-country-name-numeric-length)),
 *     iso-3166-alpha2-code PrintableString
 *                          (SIZE (ub-country-name-alpha-length)) }
 *
 * The private calls for this type:
 *
 *  nssPKIXPhysicalDeliveryCountryName_Decode
 *  nssPKIXPhysicalDeliveryCountryName_CreateFromUTF8
 *  nssPKIXPhysicalDeliveryCountryName_Encode
 *
 */

/*
 * nssPKIXPhysicalDeliveryCountryName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPhysicalDeliveryCountryName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPhysicalDeliveryCountryName *
nssPKIXPhysicalDeliveryCountryName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPhysicalDeliveryCountryName_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPhysicalDeliveryCountryName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPhysicalDeliveryCountryName *
nssPKIXPhysicalDeliveryCountryName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXPhysicalDeliveryCountryName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PHYSICAL_DELIVERY_COUNTRY_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPhysicalDeliveryCountryName_Encode
(
  NSSPKIXPhysicalDeliveryCountryName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * PostalCode
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PostalCode ::= CHOICE {
 *     numeric-code NumericString (SIZE (1..ub-postal-code-length)),
 *     printable-code PrintableString (SIZE (1..ub-postal-code-length)) }
 *
 * The private calls for this type:
 *
 *  nssPKIXPostalCode_Decode
 *  nssPKIXPostalCode_CreateFromUTF8
 *  nssPKIXPostalCode_Encode
 *
 */

/*
 * nssPKIXPostalCode_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPostalCode upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPostalCode *
nssPKIXPostalCode_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPostalCode_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPostalCode upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPostalCode *
nssPKIXPostalCode_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXPostalCode_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POSTAL_CODE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPostalCode_Encode
(
  NSSPKIXPostalCode *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * PDSParameter
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PDSParameter ::= SET {
 *     printable-string PrintableString
 *                  (SIZE(1..ub-pds-parameter-length)) OPTIONAL,
 *     teletex-string TeletexString
 *                  (SIZE(1..ub-pds-parameter-length)) OPTIONAL }
 *
 * The private calls for this type:
 *
 *  nssPKIXPDSParameter_Decode
 *  nssPKIXPDSParameter_CreateFromUTF8
 *  nssPKIXPDSParameter_Create
 *  nssPKIXPDSParameter_Delete
 *  nssPKIXPDSParameter_Encode
 *  nssPKIXPDSParameter_GetUTF8Encoding
 *  nssPKIXPDSParameter_HasPrintableString
 *  nssPKIXPDSParameter_GetPrintableString
 *  nssPKIXPDSParameter_SetPrintableString
 *  nssPKIXPDSParameter_RemovePrintableString
 *  nssPKIXPDSParameter_HasTeletexString
 *  nssPKIXPDSParameter_GetTeletexString
 *  nssPKIXPDSParameter_SetTeletexString
 *  nssPKIXPDSParameter_RemoveTeletexString
 *  nssPKIXPDSParameter_Equal
 *  nssPKIXPDSParameter_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXPDSParameter_verifyPointer
 */

/*
 * nssPKIXPDSParameter_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPDSParameter upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPDSParameter *
nssPKIXPDSParameter_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPDSParameter_CreateFromUTF8
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPDSParameter upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPDSParameter *
nssPKIXPDSParameter_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXPDSParameter_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPDSParameter upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPDSParameter *
nssPKIXPDSParameter_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *printableStringOpt,
  NSSUTF8 *teletexStringOpt
);

/*
 * nssPKIXPDSParameter_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPDSParameter_Destroy
(
  NSSPKIXPDSParameter *p
);

/*
 * nssPKIXPDSParameter_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPDSParameter_Encode
(
  NSSPKIXPDSParameter *p,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPDSParameter_GetUTF8Encoding
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXPDSParameter_GetUTF8Encoding
(
  NSSPKIXPDSParameter *p,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPDSParameter_HasPrintableString
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPDSParameter_HasPrintableString
(
  NSSPKIXPDSParameter *p,
  PRStatus *statusOpt
);

/*
 * nssPKIXPDSParameter_GetPrintableString
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXPDSParameter_GetPrintableString
(
  NSSPKIXPDSParameter *p,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPDSParameter_SetPrintableString
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPDSParameter_SetPrintableString
(
  NSSPKIXPDSParameter *p,
  NSSUTF8 *printableString
);

/*
 * nssPKIXPDSParameter_RemovePrintableString
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPDSParameter_RemovePrintableString
(
  NSSPKIXPDSParameter *p
);

/*
 * nssPKIXPDSParameter_HasTeletexString
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPDSParameter_HasTeletexString
(
  NSSPKIXPDSParameter *p,
  PRStatus *statusOpt
);

/*
 * nssPKIXPDSParameter_GetTeletexString
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXPDSParameter_GetTeletexString
(
  NSSPKIXPDSParameter *p,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPDSParameter_SetTeletexString
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPDSParameter_SetTeletexString
(
  NSSPKIXPDSParameter *p,
  NSSUTF8 *teletexString
);

/*
 * nssPKIXPDSParameter_RemoveTeletexString
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPDSParameter_RemoveTeletexString
(
  NSSPKIXPDSParameter *p
);

/*
 * nssPKIXPDSParameter_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXPDSParameter_Equal
(
  NSSPKIXPDSParameter *p1,
  NSSPKIXPDSParameter *p2,
  PRStatus *statusOpt
);

/*
 * nssPKIXPDSParameter_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPDSParameter upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPDSParameter *
nssPKIXPDSParameter_Duplicate
(
  NSSPKIXPDSParameter *p,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXPDSParameter_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXPDSParameter
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PDS_PARAMETER
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXPDSParameter_verifyPointer
(
  NSSPKIXPDSParameter *p
);
#endif /* DEBUG */

/*
 * fgmr: what about these PDS types?
 *
 * PhysicalDeliveryOfficeName
 *
 * PhysicalDeliveryOfficeNumber
 *
 * ExtensionORAddressComponents
 *
 * PhysicalDeliveryPersonalName
 *
 * PhysicalDeliveryOrganizationName
 *
 * ExtensionPhysicalDeliveryAddressComponents
 *
 * StreetAddress
 *
 * PostOfficeBoxAddress
 *
 * PosteRestanteAddress
 *
 * UniquePostalName
 *
 * LocalPostalAttributes
 *
 */

/*
 * UnformattedPostalAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  UnformattedPostalAddress ::= SET {
 *     printable-address SEQUENCE SIZE (1..ub-pds-physical-address-lines) OF
 *             PrintableString (SIZE (1..ub-pds-parameter-length)) OPTIONAL,
 *     teletex-string TeletexString
 *           (SIZE (1..ub-unformatted-address-length)) OPTIONAL }
 *
 * The private calls for the type:
 *
 *
 */

/*
 * ExtendedNetworkAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtendedNetworkAddress ::= CHOICE {
 *     e163-4-address SEQUENCE {
 *          number [0] NumericString (SIZE (1..ub-e163-4-number-length)),
 *          sub-address [1] NumericString
 *                  (SIZE (1..ub-e163-4-sub-address-length)) OPTIONAL },
 *     psap-address [0] PresentationAddress }
 *
 * The private calls for the type:
 *
 *  nssPKIXExtendedNetworkAddress_Decode
 *  nssPKIXExtendedNetworkAddress_Create
 *  nssPKIXExtendedNetworkAddress_Encode
 *  nssPKIXExtendedNetworkAddress_Destroy
 *  nssPKIXExtendedNetworkAddress_GetChoice
 *  nssPKIXExtendedNetworkAddress_Get
 *  nssPKIXExtendedNetworkAddress_GetE1634Address
 *  nssPKIXExtendedNetworkAddress_GetPsapAddress
 *  nssPKIXExtendedNetworkAddress_Set
 *  nssPKIXExtendedNetworkAddress_SetE163Address
 *  nssPKIXExtendedNetworkAddress_SetPsapAddress
 *  nssPKIXExtendedNetworkAddress_Equal
 *  nssPKIXExtendedNetworkAddress_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXExtendedNetworkAddress_verifyPointer
 *
 */

/*
 * nssPKIXExtendedNetworkAddress_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtendedNetworkAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtendedNetworkAddress *
nssPKIXExtendedNetworkAddress_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXExtendedNetworkAddress_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 *  NSS_ERROR_INVALID_PKIX_PRESENTATION_ADDRESS
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtendedNetworkAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtendedNetworkAddress *
nssPKIXExtendedNetworkAddress_Create
(
  NSSArena *arenaOpt,
  NSSPKIXExtendedNetworkAddressChoice choice,
  void *address
);

/*
 * nssPKIXExtendedNetworkAddress_CreateFromE1634Address
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtendedNetworkAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtendedNetworkAddress *
nssPKIXExtendedNetworkAddress_CreateFromE1634Address
(
  NSSArena *arenaOpt,
  NSSPKIXe1634Address *e1634address
);

/*
 * nssPKIXExtendedNetworkAddress_CreateFromPresentationAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_PRESENTATION_ADDRESS
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtendedNetworkAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtendedNetworkAddress *
nssPKIXExtendedNetworkAddress_CreateFromPresentationAddress
(
  NSSArena *arenaOpt,
  NSSPKIXPresentationAddress *presentationAddress
);

/*
 * nssPKIXExtendedNetworkAddress_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtendedNetworkAddress_Destroy
(
  NSSPKIXExtendedNetworkAddress *ena
);

/*
 * nssPKIXExtendedNetworkAddress_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXExtendedNetworkAddress_Encode
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtendedNetworkAddress_GetChoice
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 * 
 * Return value:
 *  A valid element of the NSSPKIXExtendedNetworkAddressChoice upon
 *      success
 *  The value nssPKIXExtendedNetworkAddress_NSSinvalid upon failure
 */

NSS_EXTERN NSSPKIXExtendedNetworkAddressChoice
nssPKIXExtendedNetworkAddress_GetChoice
(
  NSSPKIXExtendedNetworkAddress *ena
);

/*
 * nssPKIXExtendedNetworkAddress_GetSpecifiedChoice
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_WRONG_CHOICE
 * 
 * Return value:
 *  A pointer...
 *  NULL upon failure
 */

NSS_EXTERN void *
nssPKIXExtendedNetworkAddress_GetSpecifiedChoice
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSPKIXExtendedNetworkAddressChoice which,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtendedNetworkAddress_GetE1634Address
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_WRONG_CHOICE
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXe1643Address upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXe1643Address *
nssPKIXExtendedNetworkAddress_GetE1634Address
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtendedNetworkAddress_GetPresentationAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_WRONG_CHOICE
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPresentationAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPresentationAddress *
nssPKIXExtendedNetworkAddress_GetPresentationAddress
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtendedNetworkAddress_SetSpecifiedChoice
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 *  NSS_ERROR_INVALID_PKIX_PRESENTATION_ADDRESS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtendedNetworkAddress_SetSpecifiedChoice
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSPKIXExtendedNetworkAddressChoice which,
  void *address
);

/*
 * nssPKIXExtendedNetworkAddress_SetE163Address
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtendedNetworkAddress_SetE163Address
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSPKIXe1634Address *e1634address
);

/*
 * nssPKIXExtendedNetworkAddress_SetPresentationAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_PRESENTATION_ADDRESS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtendedNetworkAddress_SetPresentationAddress
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSPKIXPresentationAddress *presentationAddress
);

/*
 * nssPKIXExtendedNetworkAddress_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXExtendedNetworkAddress_Equal
(
  NSSPKIXExtendedNetworkAddress *ena1,
  NSSPKIXExtendedNetworkAddress *ena2,
  PRStatus *statusOpt
);

/*
 * nssPKIXExtendedNetworkAddress_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtendedNetworkAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtendedNetworkAddress *
nssPKIXExtendedNetworkAddress_Duplicate
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXExtendedNetworkAddress_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXExtendedNetworkAddress
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXExtendedNetworkAddress_verifyPointer
(
  NSSPKIXExtendedNetworkAddress *p
);
#endif /* DEBUG */

/*
 * e163-4-address
 *
 * Helper structure for ExtendedNetworkAddress.
 * -- fgmr comments --
 *
 * From RFC 2459:
 * 
 *     e163-4-address SEQUENCE {
 *          number [0] NumericString (SIZE (1..ub-e163-4-number-length)),
 *          sub-address [1] NumericString
 *                  (SIZE (1..ub-e163-4-sub-address-length)) OPTIONAL },
 * 
 * The private calls for the type:
 *
 *  nssPKIXe1634Address_Decode
 *  nssPKIXe1634Address_Create
 *  nssPKIXe1634Address_Destroy
 *  nssPKIXe1634Address_Encode
 *  nssPKIXe1634Address_GetNumber
 *  nssPKIXe1634Address_SetNumber
 *  nssPKIXe1634Address_HasSubAddress
 *  nssPKIXe1634Address_GetSubAddress
 *  nssPKIXe1634Address_SetSubAddress
 *  nssPKIXe1634Address_RemoveSubAddress
 *  nssPKIXe1634Address_Equal
 *  nssPKIXe1634Address_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXe1634Address_verifyPointer
 *
 */

/*
 * nssPKIXe1634Address_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXe1634Address upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXe1634Address *
nssPKIXe1634Address_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXe1634Address_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXe1634Address upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXe1634Address *
nssPKIXe1634Address_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *number,
  NSSUTF8 *subAddressOpt
);

/*
 * nssPKIXe1634Address_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXe1634Address_Destroy
(
  NSSPKIXe1634Address *e
);

/*
 * nssPKIXe1634Address_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXe1634Address_Encode
(
  NSSPKIXe1634Address *e,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXe1634Address_GetNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXe1634Address_GetNumber
(
  NSSPKIXe1634Address *e,
  NSSArena *arenaOpt
);

/*
 * nssPKIXe1634Address_SetNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_STRING
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXe1634Address_SetNumber
(
  NSSPKIXe1634Address *e,
  NSSUTF8 *number
);

/*
 * nssPKIXe1634Address_HasSubAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXe1634Address_HasSubAddress
(
  NSSPKIXe1634Address *e,
  PRStatus *statusOpt
);

/*
 * nssPKIXe1634Address_GetSubAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXe1634Address_GetSubAddress
(
  NSSPKIXe1634Address *e,
  NSSArena *arenaOpt
);

/*
 * nssPKIXe1634Address_SetSubAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_STRING
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXe1634Address_SetSubAddress
(
  NSSPKIXe1634Address *e,
  NSSUTF8 *subAddress
);

/*
 * nssPKIXe1634Address_RemoveSubAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXe1634Address_RemoveSubAddress
(
  NSSPKIXe1634Address *e
);

/*
 * nssPKIXe1634Address_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXe1634Address_Equal
(
  NSSPKIXe1634Address *e1,
  NSSPKIXe1634Address *e2,
  PRStatus *statusOpt
);

/*
 * nssPKIXe1634Address_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXe1634Address upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXe1634Address *
nssPKIXe1634Address_Duplicate
(
  NSSPKIXe1634Address *e,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXe1634Address_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXe1634Address
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_E163_4_ADDRESS
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXe1634Address_verifyPointer
(
  NSSPKIXe1634Address *p
);
#endif /* DEBUG */

/*
 * PresentationAddress
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PresentationAddress ::= SEQUENCE {
 *          pSelector       [0] EXPLICIT OCTET STRING OPTIONAL,
 *          sSelector       [1] EXPLICIT OCTET STRING OPTIONAL,
 *          tSelector       [2] EXPLICIT OCTET STRING OPTIONAL,
 *          nAddresses      [3] EXPLICIT SET SIZE (1..MAX) OF OCTET STRING }
 *
 * The private calls for the type:
 *
 *  nssPKIXPresentationAddress_Decode
 *  nssPKIXPresentationAddress_Create
 *  nssPKIXPresentationAddress_Destroy
 *  nssPKIXPresentationAddress_Encode
 *  nssPKIXPresentationAddress_HasPSelector
 *  nssPKIXPresentationAddress_GetPSelector
 *  nssPKIXPresentationAddress_SetPSelector
 *  nssPKIXPresentationAddress_RemovePSelector
 *  nssPKIXPresentationAddress_HasSSelector
 *  nssPKIXPresentationAddress_GetSSelector
 *  nssPKIXPresentationAddress_SetSSelector
 *  nssPKIXPresentationAddress_RemoveSSelector
 *  nssPKIXPresentationAddress_HasTSelector
 *  nssPKIXPresentationAddress_GetTSelector
 *  nssPKIXPresentationAddress_SetTSelector
 *  nssPKIXPresentationAddress_RemoveTSelector
 *  nssPKIXPresentationAddress_HasNAddresses
 *  nssPKIXPresentationAddress_GetNAddresses
 *  nssPKIXPresentationAddress_SetNAddresses
 *  nssPKIXPresentationAddress_RemoveNAddresses
 *{NAddresses must be more complex than that}
 *  nssPKIXPresentationAddress_Compare
 *  nssPKIXPresentationAddress_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  _verifyPointer
 *
 */

/*
 * TeletexDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexDomainDefinedAttributes ::= SEQUENCE SIZE
 *     (1..ub-domain-defined-attributes) OF TeletexDomainDefinedAttribute
 * 
 * The private calls for the type:
 *
 *  nssPKIXTeletexDomainDefinedAttributes_Decode
 *  nssPKIXTeletexDomainDefinedAttributes_Create
 *  nssPKIXTeletexDomainDefinedAttributes_Destroy
 *  nssPKIXTeletexDomainDefinedAttributes_Encode
 *  nssPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributeCount
 *  nssPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributes
 *  nssPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttributes
 *  nssPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttribute
 *  nssPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttribute
 *  nssPKIXTeletexDomainDefinedAttributes_InsertTeletexDomainDefinedAttribute
 *  nssPKIXTeletexDomainDefinedAttributes_AppendTeletexDomainDefinedAttribute
 *  nssPKIXTeletexDomainDefinedAttributes_RemoveTeletexDomainDefinedAttribute
 *  nssPKIXTeletexDomainDefinedAttributes_FindTeletexDomainDefinedAttribute
 *  nssPKIXTeletexDomainDefinedAttributes_Equal
 *  nssPKIXTeletexDomainDefinedAttributes_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXTeletexDomainDefinedAttributes_verifyPointer
 *
 */

/*
 * nssPKIXTeletexDomainDefinedAttributes_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexDomainDefinedAttributes
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexDomainDefinedAttributes *
nssPKIXTeletexDomainDefinedAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexDomainDefinedAttributes
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexDomainDefinedAttributes *
nssPKIXTeletexDomainDefinedAttributes_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTeletexDomainDefinedAttribute *tdda1,
  ...
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttributes_Destroy
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTeletexDomainDefinedAttributes_Encode
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributeCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributeCount
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXTeletexDomainDefinedAttribute
 *      pointers upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexDomainDefinedAttribute **
nssPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributes
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSPKIXTeletexDomainDefinedAttribute *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttributes
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSPKIXTeletexDomainDefinedAttribute *tdda[],
  PRInt32 count
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexDomainDefinedAttribute upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexDomainDefinedAttribute *
nssPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  PRInt32 i,
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_InsertTeletexDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttributes_InsertTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  PRInt32 i,
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_AppendTeletexDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttributes_AppendTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_RemoveTeletexDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttributes_RemoveTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  PRInt32 i
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_FindTeletexDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 * 
 * Return value:
 *  The nonnegative integer upon success
 *  -1 upon failure
 */

NSS_EXTERN PRInt32
nssPKIXTeletexDomainDefinedAttributes_FindTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXTeletexDomainDefinedAttributes_Equal
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas1,
  NSSPKIXTeletexDomainDefinedAttributes *tddas2,
  PRStatus *statusOpt
);

/*
 * nssPKIXTeletexDomainDefinedAttributes_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexDomainDefinedAttributes
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexDomainDefinedAttributes *
nssPKIXTeletexDomainDefinedAttributes_Duplicate
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXTeletexDomainDefinedAttributes_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXTeletexDomainDefinedAttributes
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttributes_verifyPointer
(
  NSSPKIXTeletexDomainDefinedAttributes *p
);
#endif /* DEBUG */

/*
 * TeletexDomainDefinedAttribute
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  TeletexDomainDefinedAttribute ::= SEQUENCE {
 *          type TeletexString
 *                 (SIZE (1..ub-domain-defined-attribute-type-length)),
 *          value TeletexString
 *                 (SIZE (1..ub-domain-defined-attribute-value-length)) }
 * 
 * The private calls for the type:
 *
 *  nssPKIXTeletexDomainDefinedAttribute_Decode
 *  nssPKIXTeletexDomainDefinedAttribute_CreateFromUTF8
 *  nssPKIXTeletexDomainDefinedAttribute_Create
 *  nssPKIXTeletexDomainDefinedAttribute_Destroy
 *  nssPKIXTeletexDomainDefinedAttribute_Encode
 *  nssPKIXTeletexDomainDefinedAttribute_GetUTF8Encoding
 *  nssPKIXTeletexDomainDefinedAttribute_GetType
 *  nssPKIXTeletexDomainDefinedAttribute_SetType
 *  nssPKIXTeletexDomainDefinedAttribute_GetValue
 *  nssPKIXTeletexDomainDefinedAttribute_GetValue
 *  nssPKIXTeletexDomainDefinedAttribute_Equal
 *  nssPKIXTeletexDomainDefinedAttribute_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXTeletexDomainDefinedAttribute_verifyPointer
 *
 */

/*
 * nssPKIXTeletexDomainDefinedAttribute_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexDomainDefinedAttribute
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexDomainDefinedAttribute *
nssPKIXTeletexDomainDefinedAttribute_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_CreateFromUTF8
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexDomainDefinedAttribute
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexDomainDefinedAttribute *
nssPKIXTeletexDomainDefinedAttribute_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexDomainDefinedAttribute
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexDomainDefinedAttribute *
nssPKIXTeletexDomainDefinedAttribute_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *type,
  NSSUTF8 *value
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttribute_Destroy
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXTeletexDomainDefinedAttribute_Encode
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_GetUTF8Encoding
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXTeletexDomainDefinedAttribute_GetUTF8Encoding
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_GetType
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXTeletexDomainDefinedAttribute_GetType
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_SetType
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttribute_SetType
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSUTF8 *type
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_GetValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXTeletexDomainDefinedAttribute_GetValue
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSArena *arenaOpt
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_SetValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_STRING
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttribute_SetValue
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSUTF8 *value
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXTeletexDomainDefinedAttribute_Equal
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda1,
  NSSPKIXTeletexDomainDefinedAttribute *tdda2,
  PRStatus *statusOpt
);

/*
 * nssPKIXTeletexDomainDefinedAttribute_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXTeletexDomainDefinedAttribute
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXTeletexDomainDefinedAttribute *
nssPKIXTeletexDomainDefinedAttribute_Duplicate
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXTeletexDomainDefinedAttribute_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXTeletexDomainDefinedAttribute
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_TELETEX_DOMAIN_DEFINED_ATTRIBUTE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXTeletexDomainDefinedAttribute_verifyPointer
(
  NSSPKIXTeletexDomainDefinedAttribute *p
);
#endif /* DEBUG */

/*
 * AuthorityKeyIdentifier
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AuthorityKeyIdentifier ::= SEQUENCE {
 *        keyIdentifier             [0] KeyIdentifier            OPTIONAL,
 *        authorityCertIssuer       [1] GeneralNames             OPTIONAL,
 *        authorityCertSerialNumber [2] CertificateSerialNumber  OPTIONAL }
 *      -- authorityCertIssuer and authorityCertSerialNumber shall both
 *      -- be present or both be absent
 *
 * The private calls for the type:
 *
 *  nssPKIXAuthorityKeyIdentifier_Decode
 *  nssPKIXAuthorityKeyIdentifier_Create
 *  nssPKIXAuthorityKeyIdentifier_Destroy
 *  nssPKIXAuthorityKeyIdentifier_Encode
 *  nssPKIXAuthorityKeyIdentifier_HasKeyIdentifier
 *  nssPKIXAuthorityKeyIdentifier_GetKeyIdentifier
 *  nssPKIXAuthorityKeyIdentifier_SetKeyIdentifier
 *  nssPKIXAuthorityKeyIdentifier_RemoveKeyIdentifier
 *  nssPKIXAuthorityKeyIdentifier_HasAuthorityCertIssuerAndSerialNumber
 *  nssPKIXAuthorityKeyIdentifier_RemoveAuthorityCertIssuerAndSerialNumber
 *  nssPKIXAuthorityKeyIdentifier_GetAuthorityCertIssuer
 *  nssPKIXAuthorityKeyIdentifier_GetAuthorityCertSerialNumber
 *  nssPKIXAuthorityKeyIdentifier_SetAuthorityCertIssuerAndSerialNumber
 *  nssPKIXAuthorityKeyIdentifier_Equal
 *  nssPKIXAuthorityKeyIdentifier_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXAuthorityKeyIdentifier_verifyPointer
 *
 */

/*
 * nssPKIXAuthorityKeyIdentifier_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAuthorityKeyIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAuthorityKeyIdentifier *
nssPKIXAuthorityKeyIdentifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXAuthorityKeyIdentifier_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_ARGUMENTS
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAuthorityKeyIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAuthorityKeyIdentifier *
nssPKIXAuthorityKeyIdentifier_Create
(
  NSSArena *arenaOpt,
  NSSPKIXKeyIdentifier *keyIdentifierOpt,
  NSSPKIXGeneralNames *authorityCertIssuerOpt,
  NSSPKIXCertificateSerialNumber *authorityCertSerialNumberOpt
);

/*
 * nssPKIXAuthorityKeyIdentifier_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityKeyIdentifier_Destroy
(
  NSSPKIXAuthorityKeyIdentifier *aki
);

/*
 * nssPKIXAuthorityKeyIdentifier_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXAuthorityKeyIdentifier_Encode
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAuthorityKeyIdentifier_HasKeyIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXAuthorityKeyIdentifier_HasKeyIdentifier
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  PRStatus *statusOpt
);

/*
 * nssPKIXAuthorityKeyIdentifier_GetKeyIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_HAS_NO_KEY_IDENTIFIER
 *
 * Return value:
 *  A valid pointer to an NSSPKIXKeyIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXKeyIdentifier *
nssPKIXAuthorityKeyIdentifier_GetKeyIdentifier
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSPKIXKeyIdentifier *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAuthorityKeyIdentifier_SetKeyIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityKeyIdentifier_SetKeyIdentifier
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSPKIXKeyIdentifier *keyIdentifier
);

/*
 * nssPKIXAuthorityKeyIdentifier_RemoveKeyIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *  NSS_ERROR_HAS_NO_KEY_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityKeyIdentifier_RemoveKeyIdentifier
(
  NSSPKIXAuthorityKeyIdentifier *aki
);

/*
 * nssPKIXAuthorityKeyIdentifier_HasAuthorityCertIssuerAndSerialNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *
 * Return value:
 *  PR_TRUE if it has them
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXAuthorityKeyIdentifier_HasAuthorityCertIssuerAndSerialNumber
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  PRStatus *statusOpt
);

/*
 * nssPKIXAuthorityKeyIdentifier_RemoveAuthorityCertIssuerAndSerialNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *  NSS_ERROR_HAS_NO_AUTHORITY_CERT_ISSUER_AND_SERIAL_NUMBER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityKeyIdentifier_RemoveAuthorityCertIssuerAndSerialNumber
(
  NSSPKIXAuthorityKeyIdentifier *aki
);

/*
 * nssPKIXAuthorityKeyIdentifier_GetAuthorityCertIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_HAS_NO_AUTHORITY_CERT_ISSUER_AND_SERIAL_NUMBER
 *
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralNames *
nssPKIXAuthorityKeyIdentifier_GetAuthorityCertIssuer
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAuthorityKeyIdentifier_GetAuthorityCertSerialNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_HAS_NO_AUTHORITY_CERT_ISSUER_AND_SERIAL_NUMBER
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificateSerialNumber upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificateSerialNumber *
nssPKIXAuthorityKeyIdentifier_GetAuthorityCertSerialNumber
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSPKIXCertificateSerialNumber *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAuthorityKeyIdentifier_SetAuthorityCertIssuerAndSerialNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityKeyIdentifier_SetAuthorityCertIssuerAndSerialNumber
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSPKIXGeneralNames *issuer,
  NSSPKIXCertificateSerialNumber *serialNumber
);

/*
 * nssPKIXAuthorityKeyIdentifier_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXAuthorityKeyIdentifier_Equal
(
  NSSPKIXAuthorityKeyIdentifier *aki1,
  NSSPKIXAuthorityKeyIdentifier *aki2,
  PRStatus *statusOpt
);

/*
 * nssPKIXAuthorityKeyIdentifier_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXAuthorityKeyIdentifier upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAuthorityKeyIdentifier *
nssPKIXAuthorityKeyIdentifier_Duplicate
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXAuthorityKeyIdentifier_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXAuthorityKeyIdentifier
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_KEY_IDENTIFIER
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityKeyIdentifier_verifyPointer
(
  NSSPKIXAuthorityKeyIdentifier *p
);
#endif /* DEBUG */

/*
 * KeyUsage
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  KeyUsage ::= BIT STRING {
 *       digitalSignature        (0),
 *       nonRepudiation          (1),
 *       keyEncipherment         (2),
 *       dataEncipherment        (3),
 *       keyAgreement            (4),
 *       keyCertSign             (5),
 *       cRLSign                 (6),
 *       encipherOnly            (7),
 *       decipherOnly            (8) }
 *
 * The private calls for the type:
 *
 *  nssPKIXKeyUsage_Decode
 *  nssPKIXKeyUsage_CreateFromUTF8
 *  nssPKIXKeyUsage_CreateFromValue
 *  nssPKIXKeyUsage_Destroy
 *  nssPKIXKeyUsage_Encode
 *  nssPKIXKeyUsage_GetUTF8Encoding
 *  nssPKIXKeyUsage_GetValue
 *  nssPKIXKeyUsage_SetValue
 *    { bitwise accessors? }
 *  nssPKIXKeyUsage_Equal
 *  nssPKIXKeyUsage_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXKeyUsage_verifyPointer
 *
 */

/*
 * nssPKIXKeyUsage_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXKeyUsage upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXKeyUsage *
nssPKIXKeyUsage_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXKeyUsage_CreateFromUTF8
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXKeyUsage upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXKeyUsage *
nssPKIXKeyUsage_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXKeyUsage_CreateFromValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE_VALUE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXKeyUsage upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXKeyUsage *
nssPKIXKeyUsage_CreateFromValue
(
  NSSArena *arenaOpt,
  NSSPKIXKeyUsageValue value
);

/*
 * nssPKIXKeyUsage_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXKeyUsage_Destroy
(
  NSSPKIXKeyUsage *keyUsage
);

/*
 * nssPKIXKeyUsage_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXKeyUsage_Encode
(
  NSSPKIXKeyUsage *keyUsage,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXKeyUsage_GetUTF8Encoding
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSUTF8 pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXKeyUsage_GetUTF8Encoding
(
  NSSPKIXKeyUsage *keyUsage,
  NSSArena *arenaOpt
);

/*
 * nssPKIXKeyUsage_GetValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE
 *
 * Return value:
 *  A set of NSSKeyUsageValue values OR-d together upon success
 *  NSSKeyUsage_NSSinvalid upon failure
 */

NSS_EXTERN NSSKeyUsageValue
nssPKIXKeyUsage_GetValue
(
  NSSPKIXKeyUsage *keyUsage
);

/*
 * nssPKIXKeyUsage_SetValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE_VALUE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXKeyUsage_SetValue
(
  NSSPKIXKeyUsage *keyUsage,
  NSSPKIXKeyUsageValue value
);

/*
 * nssPKIXKeyUsage_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXKeyUsage_Equal
(
  NSSPKIXKeyUsage *keyUsage1,
  NSSPKIXKeyUsage *keyUsage2,
  PRStatus *statusOpt
);

/*
 * nssPKIXKeyUsage_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXKeyUsage upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXKeyUsage *
nssPKIXKeyUsage_Duplicate
(
  NSSPKIXKeyUsage *keyUsage,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXKeyUsage_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXKeyUsage
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_KEY_USAGE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXKeyUsage_verifyPointer
(
  NSSPKIXKeyUsage *p
);
#endif /* DEBUG */

/*
 * PrivateKeyUsagePeriod
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PrivateKeyUsagePeriod ::= SEQUENCE {
 *       notBefore       [0]     GeneralizedTime OPTIONAL,
 *       notAfter        [1]     GeneralizedTime OPTIONAL }
 *       -- either notBefore or notAfter shall be present
 *
 * The private calls for the type:
 *
 *  nssPKIXPrivateKeyUsagePeriod_Decode
 *  nssPKIXPrivateKeyUsagePeriod_Create
 *  nssPKIXPrivateKeyUsagePeriod_Destroy
 *  nssPKIXPrivateKeyUsagePeriod_Encode
 *  nssPKIXPrivateKeyUsagePeriod_HasNotBefore
 *  nssPKIXPrivateKeyUsagePeriod_GetNotBefore
 *  nssPKIXPrivateKeyUsagePeriod_SetNotBefore
 *  nssPKIXPrivateKeyUsagePeriod_RemoveNotBefore
 *  nssPKIXPrivateKeyUsagePeriod_HasNotAfter
 *  nssPKIXPrivateKeyUsagePeriod_GetNotAfter
 *  nssPKIXPrivateKeyUsagePeriod_SetNotAfter
 *  nssPKIXPrivateKeyUsagePeriod_RemoveNotAfter
 *  nssPKIXPrivateKeyUsagePeriod_Equal
 *  nssPKIXPrivateKeyUsagePeriod_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXPrivateKeyUsagePeriod_verifyPointer
 *
 */

/*
 * nssPKIXPrivateKeyUsagePeriod_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPrivateKeyUsagePeriod upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPrivateKeyUsagePeriod *
nssPKIXPrivateKeyUsagePeriod_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPrivateKeyUsagePeriod_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_TIME
 *  NSS_ERROR_INVALID_ARGUMENTS
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPrivateKeyUsagePeriod upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPrivateKeyUsagePeriod *
nssPKIXPrivateKeyUsagePeriod_Create
(
  NSSArena *arenaOpt,
  NSSTime *notBeforeOpt,
  NSSTime *notAfterOpt
);

/*
 * nssPKIXPrivateKeyUsagePeriod_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPrivateKeyUsagePeriod_Destroy
(
  NSSPKIXPrivateKeyUsagePeriod *period
);

/*
 * nssPKIXPrivateKeyUsagePeriod_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_DATA
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPrivateKeyUsagePeriod_Encode
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPrivateKeyUsagePeriod_HasNotBefore
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPrivateKeyUsagePeriod_HasNotBefore
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  PRStatus *statusOpt
);

/*
 * nssPKIXPrivateKeyUsagePeriod_GetNotBefore
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_HAS_NO_NOT_BEFORE
 *
 * Return value:
 *  NSSTime {fgmr!}
 *  NULL upon failure
 */

NSS_EXTERN NSSTime *
nssPKIXPrivateKeyUsagePeriod_GetNotBefore
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPrivateKeyUsagePeriod_SetNotBefore
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *  NSS_ERROR_INVALID_TIME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPrivateKeyUsagePeriod_SetNotBefore
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSTime *notBefore
);

/*
 * nssPKIXPrivateKeyUsagePeriod_RemoveNotBefore
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *  NSS_ERROR_HAS_NO_NOT_BEFORE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPrivateKeyUsagePeriod_RemoveNotBefore
(
  NSSPKIXPrivateKeyUsagePeriod *period
);

/*
 * nssPKIXPrivateKeyUsagePeriod_HasNotAfter
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPrivateKeyUsagePeriod_HasNotAfter
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  PRStatus *statusOpt
);

/*
 * nssPKIXPrivateKeyUsagePeriod_GetNotAfter
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_HAS_NO_NOT_AFTER
 *
 * Return value:
 *  NSSTime {fgmr!}
 *  NULL upon failure
 */

NSS_EXTERN NSSTime *
nssPKIXPrivateKeyUsagePeriod_GetNotAfter
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPrivateKeyUsagePeriod_SetNotAfter
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *  NSS_ERROR_INVALID_TIME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPrivateKeyUsagePeriod_SetNotAfter
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSTime *notAfter
);

/*
 * nssPKIXPrivateKeyUsagePeriod_RemoveNotAfter
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *  NSS_ERROR_HAS_NO_NOT_AFTER
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPrivateKeyUsagePeriod_RemoveNotAfter
(
  NSSPKIXPrivateKeyUsagePeriod *period
);

/*
 * nssPKIXPrivateKeyUsagePeriod_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXPrivateKeyUsagePeriod_Equal
(
  NSSPKIXPrivateKeyUsagePeriod *period1,
  NSSPKIXPrivateKeyUsagePeriod *period2,
  PRStatus *statusOpt
);

/*
 * nssPKIXPrivateKeyUsagePeriod_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPrivateKeyUsagePeriod upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPrivateKeyUsagePeriod *
nssPKIXPrivateKeyUsagePeriod_Duplicate
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXPrivateKeyUsagePeriod_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXPrivateKeyUsagePeriod
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_PRIVATE_KEY_USAGE_PERIOD
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXPrivateKeyUsagePeriod_verifyPointer
(
  NSSPKIXPrivateKeyUsagePeriod *p
);
#endif /* DEBUG */

/*
 * CertificatePolicies
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CertificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
 *
 * The private calls for the type:
 *
 *  nssPKIXCertificatePolicies_Decode
 *  nssPKIXCertificatePolicies_Create
 *  nssPKIXCertificatePolicies_Destroy
 *  nssPKIXCertificatePolicies_Encode
 *  nssPKIXCertificatePolicies_GetPolicyInformationCount
 *  nssPKIXCertificatePolicies_GetPolicyInformations
 *  nssPKIXCertificatePolicies_SetPolicyInformations
 *  nssPKIXCertificatePolicies_GetPolicyInformation
 *  nssPKIXCertificatePolicies_SetPolicyInformation
 *  nssPKIXCertificatePolicies_InsertPolicyInformation
 *  nssPKIXCertificatePolicies_AppendPolicyInformation
 *  nssPKIXCertificatePolicies_RemovePolicyInformation
 *  nssPKIXCertificatePolicies_FindPolicyInformation
 *  nssPKIXCertificatePolicies_Equal
 *  nssPKIXCertificatePolicies_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXCertificatePolicies_verifyPointer
 *
 */

/*
 * nssPKIXCertificatePolicies_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificatePolicies upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificatePolicies *
nssPKIXCertificatePolicies_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXCertificatePolicies_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificatePolicies upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificatePolicies *
nssPKIXCertificatePolicies_Create
(
  NSSArena *arenaOpt,
  NSSPKIXPolicyInformation *pi1,
  ...
);

/*
 * nssPKIXCertificatePolicies_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificatePolicies_Destroy
(
  NSSPKIXCertificatePolicies *cp
);

/*
 * nssPKIXCertificatePolicies_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXCertificatePolicies_Encode
(
  NSSPKIXCertificatePolicies *cp,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificatePolicies_GetPolicyInformationCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXCertificatePolicies_GetPolicyInformationCount
(
  NSSPKIXCertificatePolicies *cp
);

/*
 * nssPKIXCertificatePolicies_GetPolicyInformations
 *
 * We regret the function name.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  A valid pointer to an array of NSSPKIXPolicyInformation pointers
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyInformation **
nssPKIXCertificatePolicies_GetPolicyInformations
(
  NSSPKIXCertificatePolicies *cp,
  NSSPKIXPolicyInformation *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificatePolicies_SetPolicyInformations
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificatePolicies_SetPolicyInformations
(
  NSSPKIXCertificatePolicies *cp,
  NSSPKIXPolicyInformation *pi[],
  PRInt32 count
);

/*
 * nssPKIXCertificatePolicies_GetPolicyInformation
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyInformation upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyInformation *
nssPKIXCertificatePolicies_GetPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCertificatePolicies_SetPolicyInformation
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificatePolicies_SetPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  PRInt32 i,
  NSSPKIXPolicyInformation *pi
);

/*
 * nssPKIXCertificatePolicies_InsertPolicyInformation
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificatePolicies_InsertPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  PRInt32 i,
  NSSPKIXPolicyInformation *pi
);

/*
 * nssPKIXCertificatePolicies_AppendPolicyInformation
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificatePolicies_AppendPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  NSSPKIXPolicyInformation *pi
);

/*
 * nssPKIXCertificatePolicies_RemovePolicyInformation
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCertificatePolicies_RemovePolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  PRInt32 i
);

/*
 * nssPKIXCertificatePolicies_FindPolicyInformation
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *
 * Return value:
 *  The nonnegative integer upon success
 *  -1 upon failure
 */

NSS_EXTERN PRInt32
nssPKIXCertificatePolicies_FindPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  NSSPKIXPolicyInformation *pi
);

/*
 * nssPKIXCertificatePolicies_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXCertificatePolicies_Equal
(
  NSSPKIXCertificatePolicies *cp1,
  NSSPKIXCertificatePolicies *cp2,
  PRStatus *statusOpt
);

/*
 * nssPKIXCertificatePolicies_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXCertificatePolicies upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertificatePolicies *
nssPKIXCertificatePolicies_Duplicate
(
  NSSPKIXCertificatePolicies *cp,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXCertificatePolicies_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXCertificatePolicies
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CERTIFICATE_POLICIES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXCertificatePolicies_verifyPointer
(
  NSSPKIXCertificatePolicies *p
);
#endif /* DEBUG */

/*
 * PolicyInformation
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PolicyInformation ::= SEQUENCE {
 *       policyIdentifier   CertPolicyId,
 *       policyQualifiers   SEQUENCE SIZE (1..MAX) OF
 *               PolicyQualifierInfo OPTIONAL }
 *
 * The private calls for the type:
 *
 *  nssPKIXPolicyInformation_Decode
 *  nssPKIXPolicyInformation_Create
 *  nssPKIXPolicyInformation_Destroy
 *  nssPKIXPolicyInformation_Encode
 *  nssPKIXPolicyInformation_GetPolicyIdentifier
 *  nssPKIXPolicyInformation_SetPolicyIdentifier
 *  nssPKIXPolicyInformation_GetPolicyQualifierCount
 *  nssPKIXPolicyInformation_GetPolicyQualifiers
 *  nssPKIXPolicyInformation_SetPolicyQualifiers
 *  nssPKIXPolicyInformation_GetPolicyQualifier
 *  nssPKIXPolicyInformation_SetPolicyQualifier
 *  nssPKIXPolicyInformation_InsertPolicyQualifier
 *  nssPKIXPolicyInformation_AppendPolicyQualifier
 *  nssPKIXPolicyInformation_RemovePolicyQualifier
 *  nssPKIXPolicyInformation_Equal
 *  nssPKIXPolicyInformation_Duplicate
 *    { and accessors by specific policy qualifier ID }
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXPolicyInformation_verifyPointer
 *
 */

/*
 * nssPKIXPolicyInformation_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyInformation upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyInformation *
nssPKIXPolicyInformation_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPolicyInformation_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyInformation upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyInformation *
nssPKIXPolicyInformation_Create
(
  NSSArena *arenaOpt,
  NSSPKIXCertPolicyId *id,
  NSSPKIXPolicyQualifierInfo *pqi1,
  ...
);

/*
 * nssPKIXPolicyInformation_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyInformation_Destroy
(
  NSSPKIXPolicyInformation *pi
);

/*
 * nssPKIXPolicyInformation_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPolicyInformation_Encode
(
  NSSPKIXPolicyInformation *pi,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyInformation_GetPolicyIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid OID upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertPolicyId *
nssPKIXPolicyInformation_GetPolicyIdentifier
(
  NSSPKIXPolicyInformation *pi
);

/*
 * nssPKIXPolicyInformation_SetPolicyIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyInformation_SetPolicyIdentifier
(
  NSSPKIXPolicyInformation *pi,
  NSSPKIXCertPolicyIdentifier *cpi
);

/*
 * nssPKIXPolicyInformation_GetPolicyQualifierCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXPolicyInformation_GetPolicyQualifierCount
(
  NSSPKIXPolicyInformation *pi
);

/*
 * nssPKIXPolicyInformation_GetPolicyQualifiers
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 *
 * Return value:
 *  A valid pointer to an array of NSSPKIXPolicyQualifierInfo pointers
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyQualifierInfo **
nssPKIXPolicyInformation_GetPolicyQualifiers
(
  NSSPKIXPolicyInformation *pi,
  NSSPKIXPolicyQualifierInfo *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyInformation_SetPolicyQualifiers
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyInformation_SetPolicyQualifiers
(
  NSSPKIXPolicyInformation *pi,
  NSSPKIXPolicyQualifierInfo *pqi[],
  PRInt32 count
);

/*
 * nssPKIXPolicyInformation_GetPolicyQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyQualifierInfo upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyQualifierInfo *
nssPKIXPolicyInformation_GetPolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyInformation_SetPolicyQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyInformation_SetPolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i,
  NSSPKIXPolicyQualifierInfo *pqi
);

/*
 * nssPKIXPolicyInformation_InsertPolicyQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyInformation_InsertPolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i,
  NSSPKIXPolicyQualifierInfo *pqi
);

/*
 * nssPKIXPolicyInformation_AppendPolicyQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyInformation_AppendPolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i,
  NSSPKIXPolicyQualifierInfo *pqi
);

/*
 * nssPKIXPolicyInformation_RemovePolicyQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyInformation_RemovePolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i
);

/*
 * nssPKIXPolicyInformation_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXPolicyInformation_Equal
(
  NSSPKIXPolicyInformation *pi1,
  NSSPKIXPolicyInformation *pi2,
  PRStatus *statusOpt
);

/*
 * nssPKIXPolicyInformation_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyInformation upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyInformation *
nssPKIXPolicyInformation_Duplicate
(
  NSSPKIXPolicyInformation *pi,
  NSSArena *arenaOpt
);

/*
 *   { and accessors by specific policy qualifier ID }
 *
 */

#ifdef DEBUG
/*
 * nssPKIXPolicyInformation_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXPolicyInformation
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_INFORMATION
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXPolicyInformation_verifyPointer
(
  NSSPKIXPolicyInformation *p
);
#endif /* DEBUG */

/*
 * PolicyQualifierInfo
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PolicyQualifierInfo ::= SEQUENCE {
 *         policyQualifierId  PolicyQualifierId,
 *         qualifier        ANY DEFINED BY policyQualifierId }
 *
 * The private calls for the type:
 *
 *  nssPKIXPolicyQualifierInfo_Decode
 *  nssPKIXPolicyQualifierInfo_Create
 *  nssPKIXPolicyQualifierInfo_Destroy
 *  nssPKIXPolicyQualifierInfo_Encode
 *  nssPKIXPolicyQualifierInfo_GetPolicyQualifierID
 *  nssPKIXPolicyQualifierInfo_SetPolicyQualifierID
 *  nssPKIXPolicyQualifierInfo_GetQualifier
 *  nssPKIXPolicyQualifierInfo_SetQualifier
 *  nssPKIXPolicyQualifierInfo_Equal
 *  nssPKIXPolicyQualifierInfo_Duplicate
 *    { and accessors by specific qualifier id/type }
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXPolicyQualifierInfo_verifyPointer
 *
 */

/*
 * nssPKIXPolicyQualifierInfo_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyQualifierInfo upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyQualifierInfo *
nssPKIXPolicyQualifierInfo_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPolicyQualifierInfo_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_ID
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyQualifierInfo upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyQualifierInfo *
nssPKIXPolicyQualifierInfo_Create
(
  NSSArena *arenaOpt,
  NSSPKIXPolicyQualifierId *policyQualifierId,
  NSSItem *qualifier
);

/*
 * nssPKIXPolicyQualifierInfo_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyQualifierInfo_Destroy
(
  NSSPKIXPolicyQualifierInfo *pqi
);

/*
 * nssPKIXPolicyQualifierInfo_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPolicyQualifierInfo_Encode
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyQualifierInfo_GetPolicyQualifierId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyQualifierId (an NSSOID) upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyQualifierId *
nssPKIXPolicyQualifierInfo_GetPolicyQualifierId
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyQualifierInfo_SetPolicyQualifierId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_ID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyQualifierInfo_SetPolicyQualifierId
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSPKIXPolicyQualifierId *pqid
);

/*
 * nssPKIXPolicyQualifierInfo_GetQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSItem *
nssPKIXPolicyQualifierInfo_GetQualifier
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyQualifierInfo_SetQualifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ITEM
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyQualifierInfo_SetQualifier
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSItem *qualifier
);

/*
 * nssPKIXPolicyQualifierInfo_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXPolicyQualifierInfo_Equal
(
  NSSPKIXPolicyQualifierInfo *pqi1,
  NSSPKIXPolicyQualifierInfo *pqi2,
  PRStatus *statusOpt
);

/*
 * nssPKIXPolicyQualifierInfo_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyQualifierInfo upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyQualifierInfo *
nssPKIXPolicyQualifierInfo_Duplicate
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSArena *arenaOpt
);

/*
 *   { and accessors by specific qualifier id/type }
 *
 */
#ifdef DEBUG
/*
 * nssPKIXPolicyQualifierInfo_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXPolicyQualifierInfo
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_QUALIFIER_INFO
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXPolicyQualifierInfo_verifyPointer
(
  NSSPKIXPolicyQualifierInfo *p
);
#endif /* DEBUG */

/*
 * CPSuri
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CPSuri ::= IA5String
 *
 * The private calls for this type:
 *
 *  nssPKIXCPSuri_Decode
 *  nssPKIXCPSuri_CreateFromUTF8
 *  nssPKIXCPSuri_Encode
 *
 */

/*
 * nssPKIXCPSuri_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCPSuri upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCPSuri *
nssPKIXCPSuri_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXCPSuri_CreateFromUTF8
 *
 * { basically just enforces the length and charset limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCPSuri upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCPSuri *
nssPKIXCPSuri_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXCPSuri_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CPS_URI
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXCPSuri_Encode
(
  NSSPKIXCPSuri *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * UserNotice
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  UserNotice ::= SEQUENCE {
 *       noticeRef        NoticeReference OPTIONAL,
 *       explicitText     DisplayText OPTIONAL}
 *
 * The private calls for this type:
 *
 *  nssPKIXUserNotice_Decode
 *  nssPKIXUserNotice_Create
 *  nssPKIXUserNotice_Destroy
 *  nssPKIXUserNotice_Encode
 *  nssPKIXUserNotice_HasNoticeRef
 *  nssPKIXUserNotice_GetNoticeRef
 *  nssPKIXUserNotice_SetNoticeRef
 *  nssPKIXUserNotice_RemoveNoticeRef
 *  nssPKIXUserNotice_HasExplicitText
 *  nssPKIXUserNotice_GetExplicitText
 *  nssPKIXUserNotice_SetExplicitText
 *  nssPKIXUserNotice_RemoveExplicitText
 *  nssPKIXUserNotice_Equal
 *  nssPKIXUserNotice_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXUserNotice_verifyPointer
 *
 */


/*
 * nssPKIXUserNotice_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXUserNotice upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXUserNotice *
nssPKIXUserNotice_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXUserNotice_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_INVALID_PKIX_DISPLAY_TEXT
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXUserNotice upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXUserNotice *
nssPKIXUserNotice_Create
(
  NSSArena *arenaOpt,
  NSSPKIXNoticeReference *noticeRef,
  NSSPKIXDisplayText *explicitText
);

/*
 * nssPKIXUserNotice_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXUserNotice_Destroy
(
  NSSPKIXUserNotice *userNotice
);

/*
 * nssPKIXUserNotice_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXUserNotice_Encode
(
  NSSPKIXUserNotice *userNotice,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXUserNotice_HasNoticeRef
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXUserNotice_HasNoticeRef
(
  NSSPKIXUserNotice *userNotice,
  PRStatus *statusOpt
);

/*
 * nssPKIXUserNotice_GetNoticeRef
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_HAS_NO_NOTICE_REF
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNoticeReference upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNoticeReference *
nssPKIXUserNotice_GetNoticeRef
(
  NSSPKIXUserNotice *userNotice,
  NSSArena *arenaOpt
);

/*
 * nssPKIXUserNotice_SetNoticeRef
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXUserNotice_SetNoticeRef
(
  NSSPKIXUserNotice *userNotice,
  NSSPKIXNoticeReference *noticeRef
);

/*
 * nssPKIXUserNotice_RemoveNoticeRef
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 *  NSS_ERROR_HAS_NO_NOTICE_REF
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXUserNotice_RemoveNoticeRef
(
  NSSPKIXUserNotice *userNotice
);

/*
 * nssPKIXUserNotice_HasExplicitText
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 * 
 * Return value:
 *  PR_TRUE if it has some
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXUserNotice_HasExplicitText
(
  NSSPKIXUserNotice *userNotice,
  PRStatus *statusOpt
);

/*
 * nssPKIXUserNotice_GetExplicitText
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_HAS_NO_EXPLICIT_TEXT
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDisplayText string upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDisplayText *
nssPKIXUserNotice_GetExplicitText
(
  NSSPKIXUserNotice *userNotice,
  NSSArena *arenaOpt
);

/*
 * nssPKIXUserNotice_SetExplicitText
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 *  NSS_ERROR_INVALID_PKIX_DISPLAY_TEXT
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXUserNotice_SetExplicitText
(
  NSSPKIXUserNotice *userNotice,
  NSSPKIXDisplayText *explicitText
);

/*
 * nssPKIXUserNotice_RemoveExplicitText
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 *  NSS_ERROR_HAS_NO_EXPLICIT_TEXT
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXUserNotice_RemoveExplicitText
(
  NSSPKIXUserNotice *userNotice
);

/*
 * nssPKIXUserNotice_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXUserNotice_Equal
(
  NSSPKIXUserNotice *userNotice1,
  NSSPKIXUserNotice *userNotice2,
  PRStatus *statusOpt
);

/*
 * nssPKIXUserNotice_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXUserNotice upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXUserNotice *
nssPKIXUserNotice_Duplicate
(
  NSSPKIXUserNotice *userNotice,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXUserNotice_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXUserNotice
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_USER_NOTICE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXUserNotice_verifyPointer
(
  NSSPKIXUserNotice *p
);
#endif /* DEBUG */

/*
 * NoticeReference
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  NoticeReference ::= SEQUENCE {
 *       organization     DisplayText,
 *       noticeNumbers    SEQUENCE OF INTEGER }
 *
 * The private calls for this type:
 *
 *  nssPKIXNoticeReference_Decode
 *  nssPKIXNoticeReference_Create
 *  nssPKIXNoticeReference_Destroy
 *  nssPKIXNoticeReference_Encode
 *  nssPKIXNoticeReference_GetOrganization
 *  nssPKIXNoticeReference_SetOrganization
 *  nssPKIXNoticeReference_GetNoticeNumberCount
 *  nssPKIXNoticeReference_GetNoticeNumbers
 *  nssPKIXNoticeReference_SetNoticeNumbers
 *  nssPKIXNoticeReference_GetNoticeNumber
 *  nssPKIXNoticeReference_SetNoticeNumber
 *  nssPKIXNoticeReference_InsertNoticeNumber
 *  nssPKIXNoticeReference_AppendNoticeNumber
 *  nssPKIXNoticeReference_RemoveNoticeNumber
 *    { maybe number-exists-p gettor? }
 *  nssPKIXNoticeReference_Equal
 *  nssPKIXNoticeReference_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXNoticeReference_verifyPointer
 *
 */

/*
 * nssPKIXNoticeReference_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNoticeReference upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNoticeReference *
nssPKIXNoticeReference_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXNoticeReference_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_DISPLAY_TEXT
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNoticeReference upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNoticeReference *
nssPKIXNoticeReference_Create
(
  NSSArena *arenaOpt,
  NSSPKIXDisplayText *organization,
  PRUint32 noticeCount,
  PRInt32 noticeNumber1,
  ...
);

/*
 * { fgmr -- or should the call that takes PRInt32 be _CreateFromValues,
 *  and the _Create call take NSSBER integers? }
 */

/*
 * nssPKIXNoticeReference_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNoticeReference_Destroy
(
  NSSPKIXNoticeReference *nr
);

/*
 * nssPKIXNoticeReference_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXNoticeReference_Encode
(
  NSSPKIXNoticeReference *nr,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXNoticeReference_GetOrganization
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDisplayText string upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDisplayText *
nssPKIXNoticeReference_GetOrganization
(
  NSSPKIXNoticeReference *nr,
  NSSArena *arenaOpt
);

/*
 * nssPKIXNoticeReference_SetOrganization
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_INVALID_PKIX_DISPLAY_TEXT
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNoticeReference_SetOrganization
(
  NSSPKIXNoticeReference *nr,
  NSSPKIXDisplayText *organization
);

/*
 * nssPKIXNoticeReference_GetNoticeNumberCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXNoticeReference_GetNoticeNumberCount
(
  NSSPKIXNoticeReference *nr
);

/*
 * nssPKIXNoticeReference_GetNoticeNumbers
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of PRInt32 values upon success
 *  NULL upon failure
 */

NSS_EXTERN PRInt32 *
nssPKIXNoticeReference_GetNoticeNumbers
(
  NSSPKIXNoticeReference *nr,
  PRInt32 rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXNoticeReference_SetNoticeNumbers
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNoticeReference_SetNoticeNumbers
(
  NSSPKIXNoticeReference *nr,
  PRInt32 noticeNumbers[],
  PRInt32 count
);

/*
 * nssPKIXNoticeReference_GetNoticeNumber
 *
 * -- fgmr comments --
 * Because there is no natural "invalid" notice number value, we must
 * pass the number by reference, and return an explicit status value.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_INVALID_POINTER
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNoticeReference_GetNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 i,
  PRInt32 *noticeNumberP
);
  
/*
 * nssPKIXNoticeReference_SetNoticeNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNoticeReference_SetNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 i,
  PRInt32 noticeNumber
);

/*
 * nssPKIXNoticeReference_InsertNoticeNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNoticeReference_InsertNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 i,
  PRInt32 noticeNumber
);

/*
 * nssPKIXNoticeReference_AppendNoticeNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNoticeReference_AppendNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 noticeNumber
);

/*
 * nssPKIXNoticeReference_RemoveNoticeNumber
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNoticeReference_RemoveNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 i
);

/*
 *   { maybe number-exists-p gettor? }
 */

/*
 * nssPKIXNoticeReference_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXNoticeReference_Equal
(
  NSSPKIXNoticeReference *nr1,
  NSSPKIXNoticeReference *nr2,
  PRStatus *statusOpt
);

/*
 * nssPKIXNoticeReference_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNoticeReference upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNoticeReference *
nssPKIXNoticeReference_Duplicate
(
  NSSPKIXNoticeReference *nr,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXNoticeReference_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXNoticeReference
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NOTICE_REFERENCE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXNoticeReference_verifyPointer
(
  NSSPKIXNoticeReference *p
);
#endif /* DEBUG */

/*
 * DisplayText
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DisplayText ::= CHOICE {
 *       visibleString    VisibleString  (SIZE (1..200)),
 *       bmpString        BMPString      (SIZE (1..200)),
 *       utf8String       UTF8String     (SIZE (1..200)) }
 * 
 * The private calls for this type:
 *
 *  nssPKIXDisplayText_Decode
 *  nssPKIXDisplayText_CreateFromUTF8
 *  nssPKIXDisplayText_Encode
 *
 */

/*
 * nssPKIXDisplayText_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDisplayText upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDisplayText *
nssPKIXDisplayText_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXDisplayText_CreateFromUTF8
 *
 * { basically just enforces the length and charset limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDisplayText upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDisplayText *
nssPKIXDisplayText_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXDisplayText_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISPLAY_TEXT
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXDisplayText_Encode
(
  NSSPKIXDisplayText *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * PolicyMappings
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PolicyMappings ::= SEQUENCE SIZE (1..MAX) OF SEQUENCE {
 *       issuerDomainPolicy      CertPolicyId,
 *       subjectDomainPolicy     CertPolicyId }
 *
 * The private calls for this type:
 *
 *  nssPKIXPolicyMappings_Decode
 *  nssPKIXPolicyMappings_Create
 *  nssPKIXPolicyMappings_Destroy
 *  nssPKIXPolicyMappings_Encode
 *  nssPKIXPolicyMappings_GetPolicyMappingCount
 *  nssPKIXPolicyMappings_GetPolicyMappings
 *  nssPKIXPolicyMappings_SetPolicyMappings
 *  nssPKIXPolicyMappings_GetPolicyMapping
 *  nssPKIXPolicyMappings_SetPolicyMapping
 *  nssPKIXPolicyMappings_InsertPolicyMapping
 *  nssPKIXPolicyMappings_AppendPolicyMapping
 *  nssPKIXPolicyMappings_RemovePolicyMapping
 *  nssPKIXPolicyMappings_FindPolicyMapping
 *  nssPKIXPolicyMappings_Equal
 *  nssPKIXPolicyMappings_Duplicate
 *  nssPKIXPolicyMappings_IssuerDomainPolicyExists
 *  nssPKIXPolicyMappings_SubjectDomainPolicyExists
 *  nssPKIXPolicyMappings_FindIssuerDomainPolicy
 *  nssPKIXPolicyMappings_FindSubjectDomainPolicy
 *  nssPKIXPolicyMappings_MapIssuerToSubject
 *  nssPKIXPolicyMappings_MapSubjectToIssuer
 *    { find's and map's: what if there's more than one? }
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXPolicyMappings_verifyPointer
 *
 */

/*
 * nssPKIXPolicyMappings_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyMappings upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyMappings *
nssPKIXPolicyMappings_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPolicyMappings_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyMappings upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyMappings *
nssPKIXPolicyMappings_Decode
(
  NSSArena *arenaOpt,
  NSSPKIXpolicyMapping *policyMapping1,
  ...
);

/*
 * nssPKIXPolicyMappings_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyMappings_Destroy
(
  NSSPKIXPolicyMappings *policyMappings
);

/*
 * nssPKIXPolicyMappings_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_DATA
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPolicyMappings_Encode
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyMappings_GetPolicyMappingCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXPolicyMappings_GetPolicyMappingCount
(
  NSSPKIXPolicyMappings *policyMappings
);

/*
 * nssPKIXPolicyMappings_GetPolicyMappings
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXpolicyMapping pointers upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXpolicyMapping **
nssPKIXPolicyMappings_GetPolicyMappings
(
  NSSPKIXPolicyMappings *policyMappings
  NSSPKIXpolicyMapping *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyMappings_SetPolicyMappings
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyMappings_SetPolicyMappings
(
  NSSPKIXPolicyMappings *policyMappings
  NSSPKIXpolicyMapping *policyMapping[]
  PRInt32 count
);

/*
 * nssPKIXPolicyMappings_GetPolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXpolicyMapping upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXpolicyMapping *
nssPKIXPolicyMappings_GetPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyMappings_SetPolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyMappings_SetPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  PRInt32 i,
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * nssPKIXPolicyMappings_InsertPolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyMappings_InsertPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  PRInt32 i,
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * nssPKIXPolicyMappings_AppendPolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyMappings_AppendPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * nssPKIXPolicyMappings_RemovePolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyMappings_RemovePolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  PRInt32 i
);

/*
 * nssPKIXPolicyMappings_FindPolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  The index of the specified policy mapping upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXPolicyMappings_FindPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * nssPKIXPolicyMappings_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXPolicyMappings_Equal
(
  NSSPKIXPolicyMappings *policyMappings1,
  NSSPKIXpolicyMappings *policyMappings2,
  PRStatus *statusOpt
);

/*
 * nssPKIXPolicyMappings_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyMappings upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyMappings *
nssPKIXPolicyMappings_Duplicate
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXPolicyMappings_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXPolicyMappings
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXPolicyMappings_verifyPointer
(
  NSSPKIXPolicyMappings *p
);
#endif /* DEBUG */

/*
 * nssPKIXPolicyMappings_IssuerDomainPolicyExists
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 * 
 * Return value:
 *  PR_TRUE if the specified domain policy OID exists
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPolicyMappings_IssuerDomainPolicyExists
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *issuerDomainPolicy,
  PRStatus *statusOpt
);

/*
 * nssPKIXPolicyMappings_SubjectDomainPolicyExists
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 * 
 * Return value:
 *  PR_TRUE if the specified domain policy OID exists
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPolicyMappings_SubjectDomainPolicyExists
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *subjectDomainPolicy,
  PRStatus *statusOpt
);

/*
 * nssPKIXPolicyMappings_FindIssuerDomainPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  The index of the specified policy mapping upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXPolicyMappings_FindIssuerDomainPolicy
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *issuerDomainPolicy
);

/*
 * nssPKIXPolicyMappings_FindSubjectDomainPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  The index of the specified policy mapping upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXPolicyMappings_FindSubjectDomainPolicy
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *issuerDomainPolicy
);

/*
 * nssPKIXPolicyMappings_MapIssuerToSubject
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 *  NSS_ERROR_NOT_FOUND
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCertPolicyId upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertPolicyId *
nssPKIXPolicyMappings_MapIssuerToSubject
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *issuerDomainPolicy
);

/*
 * nssPKIXPolicyMappings_MapSubjectToIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPINGS
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 *  NSS_ERROR_NOT_FOUND
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCertPolicyId upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCertPolicyId *
nssPKIXPolicyMappings_MapSubjectToIssuer
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *issuerDomainPolicy
);

/*
 * policyMapping
 *
 * Helper structure for PolicyMappings
 *
 *                                               SEQUENCE {
 *       issuerDomainPolicy      CertPolicyId,
 *       subjectDomainPolicy     CertPolicyId }
 *
 * The private calls for this type:
 *
 *  nssPKIXpolicyMapping_Decode
 *  nssPKIXpolicyMapping_Create
 *  nssPKIXpolicyMapping_Destroy
 *  nssPKIXpolicyMapping_Encode
 *  nssPKIXpolicyMapping_GetIssuerDomainPolicy
 *  nssPKIXpolicyMapping_SetIssuerDomainPolicy
 *  nssPKIXpolicyMapping_GetSubjectDomainPolicy
 *  nssPKIXpolicyMapping_SetSubjectDomainPolicy
 *  nssPKIXpolicyMapping_Equal
 *  nssPKIXpolicyMapping_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXpolicyMapping_verifyPointer
 *
 */

/*
 * nssPKIXpolicyMapping_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXpolicyMapping upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXpolicyMapping *
nssPKIXpolicyMapping_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXpolicyMapping_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXpolicyMapping upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXpolicyMapping *
nssPKIXpolicyMapping_Create
(
  NSSArena *arenaOpt,
  NSSPKIXCertPolicyId *issuerDomainPolicy,
  NSSPKIXCertPolicyId *subjectDomainPolicy
);

/*
 * nssPKIXpolicyMapping_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXpolicyMapping_Destroy
(
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * nssPKIXpolicyMapping_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_DATA
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXpolicyMapping_Encode
(
  NSSPKIXpolicyMapping *policyMapping,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXpolicyMapping_GetIssuerDomainPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCertPolicyId OID upon success
 *  NULL upon faliure
 */

NSS_EXTERN NSSPKIXCertPolicyId *
nssPKIXpolicyMapping_GetIssuerDomainPolicy
(
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * nssPKIXpolicyMapping_SetIssuerDomainPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXpolicyMapping_SetIssuerDomainPolicy
(
  NSSPKIXpolicyMapping *policyMapping,
  NSSPKIXCertPolicyId *issuerDomainPolicy
);

/*
 * nssPKIXpolicyMapping_GetSubjectDomainPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCertPolicyId OID upon success
 *  NULL upon faliure
 */

NSS_EXTERN NSSPKIXCertPolicyId *
nssPKIXpolicyMapping_GetSubjectDomainPolicy
(
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * nssPKIXpolicyMapping_SetSubjectDomainPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_INVALID_PKIX_CERT_POLICY_ID
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXpolicyMapping_SetSubjectDomainPolicy
(
  NSSPKIXpolicyMapping *policyMapping,
  NSSPKIXCertPolicyId *subjectDomainPolicy
);

/*
 * nssPKIXpolicyMapping_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXpolicyMapping_Equal
(
  NSSPKIXpolicyMapping *policyMapping1,
  NSSPKIXpolicyMapping *policyMapping2,
  PRStatus *statusOpt
);

/*
 * nssPKIXpolicyMapping_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXpolicyMapping upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXpolicyMapping *
nssPKIXpolicyMapping_Duplicate
(
  NSSPKIXpolicyMapping *policyMapping,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXpolicyMapping_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXpolicyMapping
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_MAPPING
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXpolicyMapping_verifyPointer
(
  NSSPKIXpolicyMapping *p
);
#endif /* DEBUG */

/*
 * GeneralName
 *
 * This structure contains a union of the possible general names,
 * of which there are several.
 *
 * From RFC 2459:
 *
 *  GeneralName ::= CHOICE {
 *       otherName                       [0]     AnotherName,
 *       rfc822Name                      [1]     IA5String,
 *       dNSName                         [2]     IA5String,
 *       x400Address                     [3]     ORAddress,
 *       directoryName                   [4]     Name,
 *       ediPartyName                    [5]     EDIPartyName,
 *       uniformResourceIdentifier       [6]     IA5String,
 *       iPAddress                       [7]     OCTET STRING,
 *       registeredID                    [8]     OBJECT IDENTIFIER }
 *
 * The private calls for this type:
 *
 *  nssPKIXGeneralName_Decode
 *  nssPKIXGeneralName_CreateFromUTF8
 *  nssPKIXGeneralName_Create
 *  nssPKIXGeneralName_CreateFromOtherName
 *  nssPKIXGeneralName_CreateFromRfc822Name
 *  nssPKIXGeneralName_CreateFromDNSName
 *  nssPKIXGeneralName_CreateFromX400Address
 *  nssPKIXGeneralName_CreateFromDirectoryName
 *  nssPKIXGeneralName_CreateFromEDIPartyName
 *  nssPKIXGeneralName_CreateFromUniformResourceIdentifier
 *  nssPKIXGeneralName_CreateFromIPAddress
 *  nssPKIXGeneralName_CreateFromRegisteredID
 *  nssPKIXGeneralName_Destroy
 *  nssPKIXGeneralName_Encode
 *  nssPKIXGeneralName_GetUTF8Encoding
 *  nssPKIXGeneralName_GetChoice
 *  nssPKIXGeneralName_GetOtherName
 *  nssPKIXGeneralName_GetRfc822Name
 *  nssPKIXGeneralName_GetDNSName
 *  nssPKIXGeneralName_GetX400Address
 *  nssPKIXGeneralName_GetDirectoryName
 *  nssPKIXGeneralName_GetEDIPartyName
 *  nssPKIXGeneralName_GetUniformResourceIdentifier
 *  nssPKIXGeneralName_GetIPAddress
 *  nssPKIXGeneralName_GetRegisteredID
 *  nssPKIXGeneralName_GetSpecifiedChoice
 *  nssPKIXGeneralName_Equal
 *  nssPKIXGeneralName_Duplicate
 *  (in pki1 I had specific attribute value gettors here too)
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXGeneralName_verifyPointer
 *
 */

/*
 * nssPKIXGeneralName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXGeneralName_CreateFromUTF8
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * nssPKIXGeneralName_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME_CHOICE
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 *  NSS_ERROR_INVALID_PKIX_IA5_STRING
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 *  NSS_ERROR_INVALID_PKIX_NAME
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 *  NSS_ERROR_INVALID_ITEM
 *  NSS_ERROR_INVALID_OID
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_Create
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralNameChoice choice,
  void *content
);

/*
 * nssPKIXGeneralName_CreateFromOtherName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromOtherName
(
  NSSArena *arenaOpt,
  NSSPKIXAnotherName *otherName
);

/*
 * nssPKIXGeneralName_CreateFromRfc822Name
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_IA5_STRING
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromRfc822Name
(
  NSSArena *arenaOpt,
  NSSUTF8 *rfc822Name
);

/*
 * nssPKIXGeneralName_CreateFromDNSName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_IA5_STRING
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromDNSName
(
  NSSArena *arenaOpt,
  NSSUTF8 *dNSName
);

/*
 * nssPKIXGeneralName_CreateFromX400Address
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_OR_ADDRESS
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromX400Address
(
  NSSArena *arenaOpt,
  NSSPKIXORAddress *x400Address
);

/*
 * nssPKIXGeneralName_CreateFromDirectoryName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_NAME
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromDirectoryName
(
  NSSArena *arenaOpt,
  NSSPKIXName *directoryName
);

/*
 * nssPKIXGeneralName_CreateFromEDIPartyName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromEDIPartyName
(
  NSSArena *arenaOpt,
  NSSPKIXEDIPartyName *ediPartyname
);

/*
 * nssPKIXGeneralName_CreateFromUniformResourceIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_IA5_STRING
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromUniformResourceIdentifier
(
  NSSArena *arenaOpt,
  NSSUTF8 *uniformResourceIdentifier
);

/*
 * nssPKIXGeneralName_CreateFromIPAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_ITEM
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromIPAddress
(
  NSSArena *arenaOpt,
  NSSItem *iPAddress
);

/*
 * nssPKIXGeneralName_CreateFromRegisteredID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_CreateFromRegisteredID
(
  NSSArena *arenaOpt,
  NSSOID *registeredID
);

/*
 * nssPKIXGeneralName_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralName_Destroy
(
  NSSPKIXGeneralName *generalName
);

/*
 * nssPKIXGeneralName_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXGeneralName_Encode
(
  NSSPKIXGeneralName *generalName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetUTF8Encoding
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXGeneralName_GetUTF8Encoding
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetChoice
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 * 
 * Return value:
 *  A valid NSSPKIXGeneralNameChoice value upon success
 *  NSSPKIXGeneralNameChoice_NSSinvalid upon failure
 */

NSS_EXTERN NSSPKIXGeneralNameChoice
nssPKIXGeneralName_GetChoice
(
  NSSPKIXGeneralName *generalName
);

/*
 * nssPKIXGeneralName_GetOtherName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAnotherName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAnotherName *
nssPKIXGeneralName_GetOtherName
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetRfc822Name
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXGeneralName_GetRfc822Name
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetDNSName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXGeneralName_GetDNSName
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetX400Address
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXORAddress upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXORAddress *
nssPKIXGeneralName_GetX400Address
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetDirectoryName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXName *
nssPKIXGeneralName_GetDirectoryName
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetEDIPartyName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXEDIPartyName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXEDIPartyName *
nssPKIXGeneralName_GetEDIPartyName
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetUniformResourceIdentifier
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXGeneralName_GetUniformResourceIdentifier
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetIPAddress
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSItem *
nssPKIXGeneralName_GetIPAddress
(
  NSSPKIXGeneralName *generalName,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_GetRegisteredID
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSOID upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSOID *
nssPKIXGeneralName_GetRegisteredID
(
  NSSPKIXGeneralName *generalName
);

/*
 * nssPKIXGeneralName_GetSpecifiedChoice
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN void *
nssPKIXGeneralName_GetSpecifiedChoice
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralName_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXGeneralName_Equal
(
  NSSPKIXGeneralName *generalName1,
  NSSPKIXGeneralName *generalName2,
  PRStatus *statusOpt
);

/*
 * nssPKIXGeneralName_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralName_Duplicate
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * (in pki1 I had specific attribute value gettors here too)
 *
 */

#ifdef DEBUG
/*
 * nssPKIXGeneralName_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXGeneralName
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXGeneralName_verifyPointer
(
  NSSPKIXGeneralName *p
);
#endif /* DEBUG */

/*
 * GeneralNames
 *
 * This structure contains a sequence of GeneralName objects.
 *
 * From RFC 2459:
 *
 *  GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
 *
 * The private calls for this type:
 *
 *  nssPKIXGeneralNames_Decode
 *  nssPKIXGeneralNames_Create
 *  nssPKIXGeneralNames_Destroy
 *  nssPKIXGeneralNames_Encode
 *  nssPKIXGeneralNames_GetGeneralNameCount
 *  nssPKIXGeneralNames_GetGeneralNames
 *  nssPKIXGeneralNames_SetGeneralNames
 *  nssPKIXGeneralNames_GetGeneralName
 *  nssPKIXGeneralNames_SetGeneralName
 *  nssPKIXGeneralNames_InsertGeneralName
 *  nssPKIXGeneralNames_AppendGeneralName
 *  nssPKIXGeneralNames_RemoveGeneralName
 *  nssPKIXGeneralNames_FindGeneralName
 *  nssPKIXGeneralNames_Equal
 *  nssPKIXGeneralNames_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXGeneralNames_verifyPointer
 *
 */

/*
 * nssPKIXGeneralNames_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralNames *
nssPKIXGeneralNames_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXGeneralNames_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralNames *
nssPKIXGeneralNames_Create
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralName *generalName1,
  ...
);

/*
 * nssPKIXGeneralNames_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralNames_Destroy
(
  NSSPKIXGeneralNames *generalNames
);

/*
 * nssPKIXGeneralNames_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXGeneralNames_Encode
(
  NSSPKIXGeneralNames *generalNames,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralNames_GetGeneralNameCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXGeneralNames_GetGeneralNameCount
(
  NSSPKIXGeneralNames *generalNames
);

/*
 * nssPKIXGeneralNames_GetGeneralNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXGeneralName pointers upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName **
nssPKIXGeneralNames_GetGeneralNames
(
  NSSPKIXGeneralNames *generalNames,
  NSSPKIXGeneralName *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralNames_SetGeneralNames
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralNames_SetGeneralNames
(
  NSSPKIXGeneralNames *generalNames,
  NSSPKIXGeneralName *generalName[],
  PRInt32 count
);

/*
 * nssPKIXGeneralNames_GetGeneralName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralNames_GetGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralNames_SetGeneralName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralNames_SetGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  PRInt32 i,
  NSSPKIXGeneralName *generalName
);

/*
 * nssPKIXGeneralNames_InsertGeneralName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralNames_InsertGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  PRInt32 i,
  NSSPKIXGeneralName *generalName
);

/*
 * nssPKIXGeneralNames_AppendGeneralName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralNames_AppendGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  NSSPKIXGeneralName *generalName
);

/*
 * nssPKIXGeneralNames_RemoveGeneralName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralNames_RemoveGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  PRInt32 i
);

/*
 * nssPKIXGeneralNames_FindGeneralName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  The index of the specified policy mapping upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXGeneralNames_FindGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  NSSPKIXGeneralName *generalName
);

/*
 * nssPKIXGeneralNames_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXGeneralNames_Equal
(
  NSSPKIXGeneralNames *generalNames1,
  NSSPKIXGeneralNames *generalNames2,
  PRStatus *statusOpt
);

/*
 * nssPKIXGeneralNames_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralNames *
nssPKIXGeneralNames_Duplicate
(
  NSSPKIXGeneralNames *generalNames,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXGeneralNames_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXGeneralNames
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXGeneralNames_verifyPointer
(
  NSSPKIXGeneralNames *p
);
#endif /* DEBUG */

/*
 * AnotherName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AnotherName ::= SEQUENCE {
 *       type-id    OBJECT IDENTIFIER,
 *       value      [0] EXPLICIT ANY DEFINED BY type-id }
 *
 * The private calls for this type:
 *
 *  nssPKIXAnotherName_Decode
 *  nssPKIXAnotherName_Create
 *  nssPKIXAnotherName_Destroy
 *  nssPKIXAnotherName_Encode
 *  nssPKIXAnotherName_GetTypeId
 *  nssPKIXAnotherName_SetTypeId
 *  nssPKIXAnotherName_GetValue
 *  nssPKIXAnotherName_SetValue
 *  nssPKIXAnotherName_Equal
 *  nssPKIXAnotherName_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXAnotherName_verifyPointer
 *
 */

/*
 * nssPKIXAnotherName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAnotherName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAnotherName *
nssPKIXAnotherName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXAnotherName_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_INVALID_ITEM
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAnotherName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAnotherName *
nssPKIXAnotherName_Create
(
  NSSArena *arenaOpt,
  NSSOID *typeId,
  NSSItem *value
);

/*
 * nssPKIXAnotherName_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAnotherName_Destroy
(
  NSSPKIXAnotherName *anotherName
);

/*
 * nssPKIXAnotherName_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXAnotherName_Encode
(
  NSSPKIXAnotherName *anotherName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAnotherName_GetTypeId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSOID upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSOID *
nssPKIXAnotherName_GetTypeId
(
  NSSPKIXAnotherName *anotherName
);

/*
 * nssPKIXAnotherName_SetTypeId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAnotherName_SetTypeId
(
  NSSPKIXAnotherName *anotherName,
  NSSOID *typeId
);

/*
 * nssPKIXAnotherName_GetValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSItem upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSItem *
nssPKIXAnotherName_GetValue
(
  NSSPKIXAnotherName *anotherName,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAnotherName_SetValue
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ITEM
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAnotherName_SetValue
(
  NSSPKIXAnotherName *anotherName,
  NSSItem *value
);

/*
 * nssPKIXAnotherName_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXAnotherName_Equal
(
  NSSPKIXAnotherName *anotherName1,
  NSSPKIXAnotherName *anotherName2,
  PRStatus *statusOpt
);

/*
 * nssPKIXAnotherName_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAnotherName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAnotherName *
nssPKIXAnotherName_Duplicate
(
  NSSPKIXAnotherName *anotherName,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXAnotherName_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXAnotherName
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ANOTHER_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXAnotherName_verifyPointer
(
  NSSPKIXAnotherName *p
);
#endif /* DEBUG */

/*
 * EDIPartyName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *
 *  EDIPartyName ::= SEQUENCE {
 *       nameAssigner            [0]     DirectoryString OPTIONAL,
 *       partyName               [1]     DirectoryString }
 *
 * The private calls for this type:
 *
 *  nssPKIXEDIPartyName_Decode
 *  nssPKIXEDIPartyName_Create
 *  nssPKIXEDIPartyName_Destroy
 *  nssPKIXEDIPartyName_Encode
 *  nssPKIXEDIPartyName_HasNameAssigner
 *  nssPKIXEDIPartyName_GetNameAssigner
 *  nssPKIXEDIPartyName_SetNameAssigner
 *  nssPKIXEDIPartyName_RemoveNameAssigner
 *  nssPKIXEDIPartyName_GetPartyName
 *  nssPKIXEDIPartyName_SetPartyName
 *  nssPKIXEDIPartyName_Equal
 *  nssPKIXEDIPartyName_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXEDIPartyName_verifyPointer
 *
 */

/*
 * nssPKIXEDIPartyName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXEDIPartyName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXEDIPartyName *
nssPKIXEDIPartyName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXEDIPartyName_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_STRING
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXEDIPartyName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXEDIPartyName *
nssPKIXEDIPartyName_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *nameAssignerOpt,
  NSSUTF8 *partyName
);

/*
 * nssPKIXEDIPartyName_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXEDIPartyName_Destroy
(
  NSSPKIXEDIPartyName *ediPartyName
);

/*
 * nssPKIXEDIPartyName_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXEDIPartyName_Encode
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXEDIPartyName_HasNameAssigner
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXEDIPartyName_HasNameAssigner
(
  NSSPKIXEDIPartyName *ediPartyName
);

/*
 * nssPKIXEDIPartyName_GetNameAssigner
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_HAS_NO_NAME_ASSIGNER
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 string upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXEDIPartyName_GetNameAssigner
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXEDIPartyName_SetNameAssigner
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_STRING
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXEDIPartyName_SetNameAssigner
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSUTF8 *nameAssigner
);

/*
 * nssPKIXEDIPartyName_RemoveNameAssigner
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 *  NSS_ERROR_HAS_NO_NAME_ASSIGNER
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXEDIPartyName_RemoveNameAssigner
(
  NSSPKIXEDIPartyName *ediPartyName
);

/*
 * nssPKIXEDIPartyName_GetPartyName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSUTF8 string upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
nssPKIXEDIPartyName_GetPartyName
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSArena *arenaOpt
);

/*
 * nssPKIXEDIPartyName_SetPartyName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_STRING
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXEDIPartyName_SetPartyName
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSUTF8 *partyName
);

/*
 * nssPKIXEDIPartyName_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXEDIPartyName_Equal
(
  NSSPKIXEDIPartyName *ediPartyName1,
  NSSPKIXEDIPartyName *ediPartyName2,
  PRStatus *statusOpt
);

/*
 * nssPKIXEDIPartyName_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXEDIPartyName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXEDIPartyName *
nssPKIXEDIPartyName_Duplicate
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXEDIPartyName_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXEDIPartyName
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EDI_PARTY_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXEDIPartyName_verifyPointer
(
  NSSPKIXEDIPartyName *p
);
#endif /* DEBUG */

/*
 * SubjectDirectoryAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  SubjectDirectoryAttributes ::= SEQUENCE SIZE (1..MAX) OF Attribute
 *
 * The private calls for this type:
 *
 *  nssPKIXSubjectDirectoryAttributes_Decode
 *  nssPKIXSubjectDirectoryAttributes_Create
 *  nssPKIXSubjectDirectoryAttributes_Destroy
 *  nssPKIXSubjectDirectoryAttributes_Encode
 *  nssPKIXSubjectDirectoryAttributes_GetAttributeCount
 *  nssPKIXSubjectDirectoryAttributes_GetAttributes
 *  nssPKIXSubjectDirectoryAttributes_SetAttributes
 *  nssPKIXSubjectDirectoryAttributes_GetAttribute
 *  nssPKIXSubjectDirectoryAttributes_SetAttribute
 *  nssPKIXSubjectDirectoryAttributes_InsertAttribute
 *  nssPKIXSubjectDirectoryAttributes_AppendAttribute
 *  nssPKIXSubjectDirectoryAttributes_RemoveAttribute
 *  nssPKIXSubjectDirectoryAttributes_FindAttribute
 *  nssPKIXSubjectDirectoryAttributes_Equal
 *  nssPKIXSubjectDirectoryAttributes_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXSubjectDirectoryAttributes_verifyPointer
 *
 */

/*
 * nssPKIXSubjectDirectoryAttributes_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXSubjectDirectoryAttributes upon 
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXSubjectDirectoryAttributes *
nssPKIXSubjectDirectoryAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXSubjectDirectoryAttributes_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXSubjectDirectoryAttributes upon 
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXSubjectDirectoryAttributes *
nssPKIXSubjectDirectoryAttributes_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAttribute *attribute1,
  ...
);

/*
 * nssPKIXSubjectDirectoryAttributes_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXSubjectDirectoryAttributes_Destroy
(
  NSSPKIXSubjectDirectoryAttributes *sda
);

/*
 * nssPKIXSubjectDirectoryAttributes_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXSubjectDirectoryAttributes_Encode
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXSubjectDirectoryAttributes_GetAttributeCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXSubjectDirectoryAttributes_GetAttributeCount
(
  NSSPKIXSubjectDirectoryAttributes *sda
);

/*
 * nssPKIXSubjectDirectoryAttributes_GetAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXAttribute pointers upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAttribute **
nssPKIXSubjectDirectoryAttributes_GetAttributes
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSPKIXAttribute *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXSubjectDirectoryAttributes_SetAttributes
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXSubjectDirectoryAttributes_SetAttributes
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSAttribute *attributes[],
  PRInt32 count
);

/*
 * nssPKIXSubjectDirectoryAttributes_GetAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAttribute upon success
 *  NULL upon error
 */

NSS_EXTERN NSSPKIXAttribute *
nssPKIXSubjectDirectoryAttributes_GetAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXSubjectDirectoryAttributes_SetAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXSubjectDirectoryAttributes_SetAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  PRInt32 i,
  NSSPKIXAttribute *attribute
);

/*
 * nssPKIXSubjectDirectoryAttributes_InsertAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXSubjectDirectoryAttributes_InsertAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  PRInt32 i,
  NSSPKIXAttribute *attribute
);

/*
 * nssPKIXSubjectDirectoryAttributes_AppendAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_INVALID_PKIX_ATTRIBUTE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXSubjectDirectoryAttributes_AppendAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSPKIXAttribute *attribute
);

/*
 * nssPKIXSubjectDirectoryAttributes_RemoveAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXSubjectDirectoryAttributes_RemoveAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  PRInt32 i
);

/*
 * nssPKIXSubjectDirectoryAttributes_FindAttribute
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_INVALID_ATTRIBUTE
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  The index of the specified policy mapping upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXSubjectDirectoryAttributes_FindAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSPKIXAttribute *attribute
);

/*
 * nssPKIXSubjectDirectoryAttributes_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXSubjectDirectoryAttributes_Equal
(
  NSSPKIXSubjectDirectoryAttributes *sda1,
  NSSPKIXSubjectDirectoryAttributes *sda2,
  PRStatus *statusOpt
);

/*
 * nssPKIXSubjectDirectoryAttributes_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXSubjectDirectoryAttributes upon 
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXSubjectDirectoryAttributes *
nssPKIXSubjectDirectoryAttributes_Duplicate
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXSubjectDirectoryAttributes_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXSubjectDirectoryAttributes
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_SUBJECT_DIRECTORY_ATTRIBUTES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXSubjectDirectoryAttributes_verifyPointer
(
  NSSPKIXSubjectDirectoryAttributes *p
);
#endif /* DEBUG */

/*
 * BasicConstraints
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  BasicConstraints ::= SEQUENCE {
 *       cA                      BOOLEAN DEFAULT FALSE,
 *       pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
 *
 * The private calls for this type:
 *
 *  nssPKIXBasicConstraints_Decode
 *  nssPKIXBasicConstraints_Create
 *  nssPKIXBasicConstraints_Destroy
 *  nssPKIXBasicConstraints_Encode
 *  nssPKIXBasicConstraints_GetCA
 *  nssPKIXBasicConstraints_SetCA 
 *  nssPKIXBasicConstraints_HasPathLenConstraint
 *  nssPKIXBasicConstraints_GetPathLenConstraint
 *  nssPKIXBasicConstraints_SetPathLenConstraint
 *  nssPKIXBasicConstraints_RemovePathLenConstraint
 *  nssPKIXBasicConstraints_Equal
 *  nssPKIXBasicConstraints_Duplicate
 *  nssPKIXBasicConstraints_CompareToPathLenConstraint
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXBasicConstraints_verifyPointer
 *
 */

/*
 * nssPKIXBasicConstraints_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBasicConstraints upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBasicConstraints *
nssPKIXBasicConstraints_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXBasicConstraints_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_VALUE
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBasicConstraints upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBasicConstraints *
nssPKIXBasicConstraints_Create
(
  NSSArena *arenaOpt,
  PRBool ca,
  PRInt32 pathLenConstraint
);

/*
 * nssPKIXBasicConstraints_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBasicConstraints_Destroy
(
  NSSPKIXBasicConstraints *basicConstraints
);

/*
 * nssPKIXBasicConstraints_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXBasicConstraints_Encode
(
  NSSPKIXBasicConstraints *basicConstraints,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXBasicConstraints_GetCA
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 * 
 * Return value:
 *  PR_TRUE if the CA bit is true
 *  PR_FALSE if it isn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBasicConstraints_GetCA
(
  NSSPKIXBasicConstraints *basicConstraints,
  PRStatus *statusOpt
);

/*
 * nssPKIXBasicConstraints_SetCA
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBasicConstraints_SetCA
(
  NSSPKIXBasicConstraints *basicConstraints,
  PRBool ca
);

/*
 * nssPKIXBasicConstraints_HasPathLenConstraint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXBasicConstraints_HasPathLenConstraint
(
  NSSPKIXBasicConstraints *basicConstraints,
  PRStatus *statusOpt
);

/*
 * nssPKIXBasicConstraints_GetPathLenConstraint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_PATH_LEN_CONSTRAINT
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXBasicConstraints_GetPathLenConstraint
(
  NSSPKIXBasicConstraints *basicConstraints
);

/*
 * nssPKIXBasicConstraints_SetPathLenConstraint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 *  NSS_ERROR_INVALID_VALUE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBasicConstraints_SetPathLenConstraint
(
  NSSPKIXBasicConstraints *basicConstraints,
  PRInt32 pathLenConstraint
);

/*
 * nssPKIXBasicConstraints_RemovePathLenConstraint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_PATH_LEN_CONSTRAINT
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXBasicConstraints_RemovePathLenConstraint
(
  NSSPKIXBasicConstraints *basicConstraints
);

/*
 * nssPKIXBasicConstraints_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXBasicConstraints_Equal
(
  NSSPKIXBasicConstraints *basicConstraints1,
  NSSPKIXBasicConstraints *basicConstraints2,
  PRStatus *statusOpt
);

/*
 * nssPKIXBasicConstraints_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXBasicConstraints upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXBasicConstraints *
nssPKIXBasicConstraints_Duplicate
(
  NSSPKIXBasicConstraints *basicConstraints,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXBasicConstraints_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXBasicConstraints
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXBasicConstraints_verifyPointer
(
  NSSPKIXBasicConstraints *p
);
#endif /* DEBUG */

/*
 * nssPKIXBasicConstraints_CompareToPathLenConstraint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_BASIC_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_PATH_LEN_CONSTRAINT
 * 
 * Return value:
 *  1 if the specified value is greater than the pathLenConstraint
 *  0 if the specified value equals the pathLenConstraint
 *  -1 if the specified value is less than the pathLenConstraint
 *  -2 upon error
 */

NSS_EXTERN PRInt32
nssPKIXBasicConstraints_CompareToPathLenConstraint
(
  NSSPKIXBasicConstraints *basicConstraints,
  PRInt32 value
);

/*
 * NameConstraints
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  NameConstraints ::= SEQUENCE {
 *       permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
 *       excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
 *
 * The private calls for this type:
 *
 *  nssPKIXNameConstraints_Decode
 *  nssPKIXNameConstraints_Create
 *  nssPKIXNameConstraints_Destroy
 *  nssPKIXNameConstraints_Encode
 *  nssPKIXNameConstraints_HasPermittedSubtrees
 *  nssPKIXNameConstraints_GetPermittedSubtrees
 *  nssPKIXNameConstraints_SetPermittedSubtrees
 *  nssPKIXNameConstraints_RemovePermittedSubtrees
 *  nssPKIXNameConstraints_HasExcludedSubtrees
 *  nssPKIXNameConstraints_GetExcludedSubtrees
 *  nssPKIXNameConstraints_SetExcludedSubtrees
 *  nssPKIXNameConstraints_RemoveExcludedSubtrees
 *  nssPKIXNameConstraints_Equal
 *  nssPKIXNameConstraints_Duplicate
 *    { and the comparator functions }
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXNameConstraints_verifyPointer
 *
 */

/*
 * nssPKIXNameConstraints_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNameConstraints upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNameConstraints *
nssPKIXNameConstraints_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXNameConstraints_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNameConstraints upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNameConstraints *
nssPKIXNameConstraints_Create
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralSubtrees *permittedSubtrees,
  NSSPKIXGeneralSubtrees *excludedSubtrees
);

/*
 * nssPKIXNameConstraints_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNameConstraints_Destroy
(
  NSSPKIXNameConstraints *nameConstraints
);

/*
 * nssPKIXNameConstraints_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXNameConstraints_Encode
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXNameConstraints_HasPermittedSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXNameConstraints_HasPermittedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  PRStatus *statusOpt
);

/*
 * nssPKIXNameConstraints_GetPermittedSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_PERMITTED_SUBTREES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralSubtrees upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtrees *
nssPKIXNameConstraints_GetPermittedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSArena *arenaOpt
);

/*
 * nssPKIXNameConstraints_SetPermittedSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNameConstraints_SetPermittedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSPKIXGeneralSubtrees *permittedSubtrees
);

/*
 * nssPKIXNameConstraints_RemovePermittedSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_PERMITTED_SUBTREES
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNameConstraints_RemovePermittedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints
);

/*
 * nssPKIXNameConstraints_HasExcludedSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXNameConstraints_HasExcludedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  PRStatus *statusOpt
);

/*
 * nssPKIXNameConstraints_GetExcludedSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_EXCLUDED_SUBTREES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralSubtrees upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtrees *
nssPKIXNameConstraints_GetExcludedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSArena *arenaOpt
);

/*
 * nssPKIXNameConstraints_SetExcludedSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNameConstraints_SetExcludedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSPKIXGeneralSubtrees *excludedSubtrees
);

/*
 * nssPKIXNameConstraints_RemoveExcludedSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_EXCLUDED_SUBTREES
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXNameConstraints_RemoveExcludedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints
);

/*
 * nssPKIXNameConstraints_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXNameConstraints_Equal
(
  NSSPKIXNameConstraints *nameConstraints1,
  NSSPKIXNameConstraints *nameConstraints2,
  PRStatus *statusOpt
);

/*
 * nssPKIXNameConstraints_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXNameConstraints upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXNameConstraints *
nssPKIXNameConstraints_Duplicate
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSArena *arenaOpt
);

/*
 *   { and the comparator functions }
 *
 */

#ifdef DEBUG
/*
 * nssPKIXNameConstraints_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXNameConstraints
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_NAME_CONSTRAINTS
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXNameConstraints_verifyPointer
(
  NSSPKIXNameConstraints *p
);
#endif /* DEBUG */

/*
 * GeneralSubtrees
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
 *
 * The private calls for this type:
 *
 *  nssPKIXGeneralSubtrees_Decode
 *  nssPKIXGeneralSubtrees_Create
 *  nssPKIXGeneralSubtrees_Destroy
 *  nssPKIXGeneralSubtrees_Encode
 *  nssPKIXGeneralSubtrees_GetGeneralSubtreeCount
 *  nssPKIXGeneralSubtrees_GetGeneralSubtrees
 *  nssPKIXGeneralSubtrees_SetGeneralSubtrees
 *  nssPKIXGeneralSubtrees_GetGeneralSubtree
 *  nssPKIXGeneralSubtrees_SetGeneralSubtree
 *  nssPKIXGeneralSubtrees_InsertGeneralSubtree
 *  nssPKIXGeneralSubtrees_AppendGeneralSubtree
 *  nssPKIXGeneralSubtrees_RemoveGeneralSubtree
 *  nssPKIXGeneralSubtrees_FindGeneralSubtree
 *  nssPKIXGeneralSubtrees_Equal
 *  nssPKIXGeneralSubtrees_Duplicate
 *    { and finders and comparators }
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXGeneralSubtrees_verifyPointer
 *
 */

/*
 * nssPKIXGeneralSubtrees_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralSubtrees upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtrees *
nssPKIXGeneralSubtrees_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXGeneralSubtrees_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralSubtrees upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtrees *
nssPKIXGeneralSubtrees_Create
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralSubtree *generalSubtree1,
  ...
);

/*
 * nssPKIXGeneralSubtrees_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtrees_Destroy
(
  NSSPKIXGeneralSubtrees *generalSubtrees
);

/*
 * nssPKIXGeneralSubtrees_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXGeneralSubtrees_Encode
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralSubtrees_GetGeneralSubtreeCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXGeneralSubtrees_GetGeneralSubtreeCount
(
  NSSPKIXGeneralSubtrees *generalSubtrees
);

/*
 * nssPKIXGeneralSubtrees_GetGeneralSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXGeneralSubtree pointers upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtree **
nssPKIXGeneralSubtrees_GetGeneralSubtrees
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSPKIXGeneralSubtree *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralSubtrees_SetGeneralSubtrees
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtrees_SetGeneralSubtrees
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSPKIXGeneralSubtree *generalSubtree[],
  PRInt32 count
);

/*
 * nssPKIXGeneralSubtrees_GetGeneralSubtree
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralSubtree upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtree *
nssPKIXGeneralSubtrees_GetGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralSubtrees_SetGeneralSubtree
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtrees_SetGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  PRInt32 i,
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * nssPKIXGeneralSubtrees_InsertGeneralSubtree
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtrees_InsertGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  PRInt32 i,
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * nssPKIXGeneralSubtrees_AppendGeneralSubtree
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtrees_AppendGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * nssPKIXGeneralSubtrees_RemoveGeneralSubtree
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtrees_RemoveGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  PRInt32 i
);

/*
 * nssPKIXGeneralSubtrees_FindGeneralSubtree
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  The index of the specified policy mapping upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXGeneralSubtrees_FindGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * nssPKIXGeneralSubtrees_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXGeneralSubtrees_Equal
(
  NSSPKIXGeneralSubtrees *generalSubtrees1,
  NSSPKIXGeneralSubtrees *generalSubtrees2,
  PRStatus *statusOpt
);

/*
 * nssPKIXGeneralSubtrees_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralSubtrees upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtrees *
nssPKIXGeneralSubtrees_Duplicate
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSArena *arenaOpt
);

/*
 *   { and finders and comparators }
 *
 */

#ifdef DEBUG
/*
 * nssPKIXGeneralSubtrees_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXGeneralSubtrees
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREES
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtrees_verifyPointer
(
  NSSPKIXGeneralSubtrees *p
);
#endif /* DEBUG */

/*
 * GeneralSubtree
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  GeneralSubtree ::= SEQUENCE {
 *       base                    GeneralName,
 *       minimum         [0]     BaseDistance DEFAULT 0,
 *       maximum         [1]     BaseDistance OPTIONAL }
 *
 * The private calls for this type:
 *
 *  nssPKIXGeneralSubtree_Decode
 *  nssPKIXGeneralSubtree_Create
 *  nssPKIXGeneralSubtree_Destroy
 *  nssPKIXGeneralSubtree_Encode
 *  nssPKIXGeneralSubtree_GetBase
 *  nssPKIXGeneralSubtree_SetBase
 *  nssPKIXGeneralSubtree_GetMinimum
 *  nssPKIXGeneralSubtree_SetMinimum
 *  nssPKIXGeneralSubtree_HasMaximum
 *  nssPKIXGeneralSubtree_GetMaximum
 *  nssPKIXGeneralSubtree_SetMaximum
 *  nssPKIXGeneralSubtree_RemoveMaximum
 *  nssPKIXGeneralSubtree_Equal
 *  nssPKIXGeneralSubtree_Duplicate
 *  nssPKIXGeneralSubtree_DistanceInRange
 *    {other tests and comparators}
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXGeneralSubtree_verifyPointer
 *
 */

/*
 * nssPKIXGeneralSubtree_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralSubtree upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtree *
nssPKIXGeneralSubtree_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXGeneralSubtree_Create
 *
 * -- fgmr comments --
 * The optional maximum value may be omitted by specifying -1.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_INVALID_VALUE
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralSubtree upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtree *
nssPKIXGeneralSubtree_Create
(
  NSSArena *arenaOpt,
  NSSPKIXBaseDistance minimum,
  NSSPKIXBaseDistance maximumOpt
);

/*
 * nssPKIXGeneralSubtree_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtree_Destroy
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * nssPKIXGeneralSubtree_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXGeneralSubtree_Encode
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralSubtree_GetBase
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXGeneralSubtree_GetBase
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSArena *arenaOpt
);

/*
 * nssPKIXGeneralSubtree_SetBase
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtree_SetBase
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSPKIXGeneralName *base
);

/*
 * nssPKIXGeneralSubtree_GetMinimum
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN NSSPKIXBaseDistance
nssPKIXGeneralSubtree_GetMinimum
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * nssPKIXGeneralSubtree_SetMinimum
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_INVALID_VALUE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtree_SetMinimum
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSPKIXBaseDistance *minimum
);

/*
 * nssPKIXGeneralSubtree_HasMaximum
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXGeneralSubtree_HasMaximum
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * nssPKIXGeneralSubtree_GetMaximum
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_HAS_NO_MAXIMUM_BASE_DISTANCE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN NSSPKIXBaseDistance
nssPKIXGeneralSubtree_GetMaximum
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * nssPKIXGeneralSubtree_SetMaximum
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_INVALID_VALUE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtree_SetMaximum
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSPKIXBaseDistance *maximum
);

/*
 * nssPKIXGeneralSubtree_RemoveMaximum
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_HAS_NO_MAXIMUM_BASE_DISTANCE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtree_RemoveMaximum
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * nssPKIXGeneralSubtree_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXGeneralSubtree_Equal
(
  NSSPKIXGeneralSubtree *generalSubtree1,
  NSSPKIXGeneralSubtree *generalSubtree2,
  PRStatus *statusOpt
);

/*
 * nssPKIXGeneralSubtree_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralSubtree upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralSubtree *
nssPKIXGeneralSubtree_Duplicate
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXGeneralSubtree_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXGeneralSubtree
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXGeneralSubtree_verifyPointer
(
  NSSPKIXGeneralSubtree *p
);
#endif /* DEBUG */

/*
 * nssPKIXGeneralSubtree_DistanceInRange
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_GENERAL_SUBTREE
 *  NSS_ERROR_INVALID_VALUE
 * 
 * Return value:
 *  PR_TRUE if the specified value is within the minimum and maximum
 *      base distances
 *  PR_FALSE if it isn't
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXGeneralSubtree_DistanceInRange
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSPKIXBaseDistance value,
  PRStatus *statusOpt
);

/*
 *   {other tests and comparators}
 *
 */

/*
 * PolicyConstraints
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  PolicyConstraints ::= SEQUENCE {
 *       requireExplicitPolicy           [0] SkipCerts OPTIONAL,
 *       inhibitPolicyMapping            [1] SkipCerts OPTIONAL }
 * 
 * The private calls for this type:
 *
 *  nssPKIXPolicyConstraints_Decode
 *  nssPKIXPolicyConstraints_Create
 *  nssPKIXPolicyConstraints_Destroy
 *  nssPKIXPolicyConstraints_Encode
 *  nssPKIXPolicyConstraints_HasRequireExplicitPolicy
 *  nssPKIXPolicyConstraints_GetRequireExplicitPolicy
 *  nssPKIXPolicyConstraints_SetRequireExplicitPolicy
 *  nssPKIXPolicyConstraints_RemoveRequireExplicitPolicy
 *  nssPKIXPolicyConstraints_HasInhibitPolicyMapping
 *  nssPKIXPolicyConstraints_GetInhibitPolicyMapping
 *  nssPKIXPolicyConstraints_SetInhibitPolicyMapping
 *  nssPKIXPolicyConstraints_RemoveInhibitPolicyMapping
 *  nssPKIXPolicyConstraints_Equal
 *  nssPKIXPolicyConstraints_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXPolicyConstraints_verifyPointer
 *
 */

/*
 * nssPKIXPolicyConstraints_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyConstraints upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyConstraints *
nssPKIXPolicyConstraints_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXPolicyConstraints_Create
 *
 * -- fgmr comments --
 * The optional values may be omitted by specifying -1.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_VALUE
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyConstraints upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyConstraints *
nssPKIXPolicyConstraints_Create
(
  NSSArena *arenaOpt,
  NSSPKIXSkipCerts requireExplicitPolicy,
  NSSPKIXSkipCerts inhibitPolicyMapping
);

/*
 * nssPKIXPolicyConstraints_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyConstraints_Destroy
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * nssPKIXPolicyConstraints_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXPolicyConstraints_Encode
(
  NSSPKIXPolicyConstraints *policyConstraints,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXPolicyConstraints_HasRequireExplicitPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPolicyConstraints_HasRequireExplicitPolicy
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * nssPKIXPolicyConstraints_GetRequireExplicitPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_REQUIRE_EXPLICIT_POLICY
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXPolicyConstraints_GetRequireExplicitPolicy
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * nssPKIXPolicyConstraints_SetRequireExplicitPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 *  NSS_ERROR_INVALID_VALUE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyConstraints_SetRequireExplicitPolicy
(
  NSSPKIXPolicyConstraints *policyConstraints,
  NSSPKIXSkipCerts requireExplicitPolicy
);

/*
 * nssPKIXPolicyConstraints_RemoveRequireExplicitPolicy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_REQUIRE_EXPLICIT_POLICY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyConstraints_RemoveRequireExplicitPolicy
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * nssPKIXPolicyConstraints_HasInhibitPolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXPolicyConstraints_HasInhibitPolicyMapping
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * nssPKIXPolicyConstraints_GetInhibitPolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_INHIBIT_POLICY_MAPPING
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXPolicyConstraints_GetInhibitPolicyMapping
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * nssPKIXPolicyConstraints_SetInhibitPolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 *  NSS_ERROR_INVALID_VALUE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyConstraints_SetInhibitPolicyMapping
(
  NSSPKIXPolicyConstraints *policyConstraints,
  NSSPKIXSkipCerts inhibitPolicyMapping
);

/*
 * nssPKIXPolicyConstraints_RemoveInhibitPolicyMapping
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 *  NSS_ERROR_HAS_NO_INHIBIT_POLICY_MAPPING
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXPolicyConstraints_RemoveInhibitPolicyMapping
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * nssPKIXPolicyConstraints_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXPolicyConstraints_Equal
(
  NSSPKIXPolicyConstraints *policyConstraints1,
  NSSPKIXPolicyConstraints *policyConstraints2,
  PRStatus *statusOpt
);

/*
 * nssPKIXPolicyConstraints_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXPolicyConstraints upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXPolicyConstraints *
nssPKIXPolicyConstraints_Duplicate
(
  NSSPKIXPolicyConstraints *policyConstraints,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXPolicyConstraints_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXPolicyConstraints
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_POLICY_CONSTRAINTS
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXPolicyConstraints_verifyPointer
(
  NSSPKIXPolicyConstraints *p
);
#endif /* DEBUG */

/*
 * CRLDistPointsSyntax
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CRLDistPointsSyntax ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint
 *
 * The private calls for this type:
 *
 *  nssPKIXCRLDistPointsSyntax_Decode
 *  nssPKIXCRLDistPointsSyntax_Create
 *  nssPKIXCRLDistPointsSyntax_Destroy
 *  nssPKIXCRLDistPointsSyntax_Encode
 *  nssPKIXCRLDistPointsSyntax_GetDistributionPointCount
 *  nssPKIXCRLDistPointsSyntax_GetDistributionPoints
 *  nssPKIXCRLDistPointsSyntax_SetDistributionPoints
 *  nssPKIXCRLDistPointsSyntax_GetDistributionPoint
 *  nssPKIXCRLDistPointsSyntax_SetDistributionPoint
 *  nssPKIXCRLDistPointsSyntax_InsertDistributionPoint
 *  nssPKIXCRLDistPointsSyntax_AppendDistributionPoint
 *  nssPKIXCRLDistPointsSyntax_RemoveDistributionPoint
 *  nssPKIXCRLDistPointsSyntax_FindDistributionPoint
 *  nssPKIXCRLDistPointsSyntax_Equal
 *  nssPKIXCRLDistPointsSyntax_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXCRLDistPointsSyntax_verifyPointer
 *
 */

/*
 * nssPKIXCRLDistPointsSyntax_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCRLDistPointsSyntax upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCRLDistPointsSyntax *
nssPKIXCRLDistPointsSyntax_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXCRLDistPointsSyntax_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCRLDistPointsSyntax upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCRLDistPointsSyntax *
nssPKIXCRLDistPointsSyntax_Create
(
  NSSArena *arenaOpt,
  NSSPKIXDistributionPoint *distributionPoint1,
  ...
);

/*
 * nssPKIXCRLDistPointsSyntax_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCRLDistPointsSyntax_Destroy
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax
);

/*
 * nssPKIXCRLDistPointsSyntax_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXCRLDistPointsSyntax_Encode
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCRLDistPointsSyntax_GetDistributionPointCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXCRLDistPointsSyntax_GetDistributionPointCount
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax
);

/*
 * nssPKIXCRLDistPointsSyntax_GetDistributionPoints
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXDistributionPoint pointers 
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPoints **
nssPKIXCRLDistPointsSyntax_GetDistributionPoints
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSDistributionPoint *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCRLDistPointsSyntax_SetDistributionPoints
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCRLDistPointsSyntax_SetDistributionPoints
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSDistributionPoint *distributionPoint[]
  PRInt32 count
);

/*
 * nssPKIXCRLDistPointsSyntax_GetDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPoint upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPoint *
nssPKIXCRLDistPointsSyntax_GetDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSArena *arenaOpt
);

/*
 * nssPKIXCRLDistPointsSyntax_SetDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCRLDistPointsSyntax_SetDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  PRInt32 i,
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXCRLDistPointsSyntax_InsertDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCRLDistPointsSyntax_InsertDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  PRInt32 i,
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXCRLDistPointsSyntax_AppendDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCRLDistPointsSyntax_AppendDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXCRLDistPointsSyntax_RemoveDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXCRLDistPointsSyntax_RemoveDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  PRInt32 i
);

/*
 * nssPKIXCRLDistPointsSyntax_FindDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  The index of the specified policy mapping upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXCRLDistPointsSyntax_FindDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXCRLDistPointsSyntax_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXCRLDistPointsSyntax_Equal
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax1,
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax2,
  PRStatus *statusOpt
);

/*
 * nssPKIXCRLDistPointsSyntax_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXCRLDistPointsSyntax upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXCRLDistPointsSyntax *
nssPKIXCRLDistPointsSyntax_Duplicate
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXCRLDistPointsSyntax_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXCRLDistPointsSyntax
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_CRL_DIST_POINTS_SYNTAX
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXCRLDistPointsSyntax_verifyPointer
(
  NSSPKIXCRLDistPointsSyntax *p
);
#endif /* DEBUG */

/*
 * DistributionPoint
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DistributionPoint ::= SEQUENCE {
 *       distributionPoint       [0]     DistributionPointName OPTIONAL,
 *       reasons                 [1]     ReasonFlags OPTIONAL,
 *       cRLIssuer               [2]     GeneralNames OPTIONAL }
 *
 * The private calls for this type:
 *
 *  nssPKIXDistributionPoint_Decode
 *  nssPKIXDistributionPoint_Create
 *  nssPKIXDistributionPoint_Destroy
 *  nssPKIXDistributionPoint_Encode
 *  nssPKIXDistributionPoint_HasDistributionPoint
 *  nssPKIXDistributionPoint_GetDistributionPoint
 *  nssPKIXDistributionPoint_SetDistributionPoint
 *  nssPKIXDistributionPoint_RemoveDistributionPoint
 *  nssPKIXDistributionPoint_HasReasons
 *  nssPKIXDistributionPoint_GetReasons
 *  nssPKIXDistributionPoint_SetReasons
 *  nssPKIXDistributionPoint_RemoveReasons
 *  nssPKIXDistributionPoint_HasCRLIssuer
 *  nssPKIXDistributionPoint_GetCRLIssuer
 *  nssPKIXDistributionPoint_SetCRLIssuer
 *  nssPKIXDistributionPoint_RemoveCRLIssuer
 *  nssPKIXDistributionPoint_Equal
 *  nssPKIXDistributionPoint_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXDistributionPoint_verifyPointer
 *
 */

/*
 * nssPKIXDistributionPoint_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPoint upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPoint *
nssPKIXDistributionPoint_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXDistributionPoint_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPoint upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPoint *
nssPKIXDistributionPoint_Create
(
  NSSArena *arenaOpt,
  NSSPKIXDistributionPointName *distributionPoint,
  NSSPKIXReasonFlags reasons,
  NSSPKIXGeneralNames *cRLIssuer
);

/*
 * nssPKIXDistributionPoint_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPoint_Destroy
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXDistributionPoint_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXDistributionPoint_Encode
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXDistributionPoint_HasDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXDistributionPoint_HasDistributionPoint
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXDistributionPoint_GetDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_DISTRIBUTION_POINT
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPointName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPointName *
nssPKIXDistributionPoint_GetDistributionPoint
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSArena *arenaOpt
);

/*
 * nssPKIXDistributionPoint_SetDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPoint_SetDistributionPoint
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSPKIXDistributionPointName *name
);

/*
 * nssPKIXDistributionPoint_RemoveDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPoint_RemoveDistributionPoint
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXDistributionPoint_HasReasons
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXDistributionPoint_HasReasons
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXDistributionPoint_GetReasons
 *
 * It is unlikely that the reason flags are all zero; so zero is
 * returned in error situations.
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_REASONS
 * 
 * Return value:
 *  A valid nonzero NSSPKIXReasonFlags value upon success
 *  A valid zero NSSPKIXReasonFlags if the value is indeed zero
 *  Zero upon error
 */

NSS_EXTERN NSSPKIXReasonFlags
nssPKIXDistributionPoint_GetReasons
(
  NSSPKIXDistributionPoint *distributionPoint,
  PRStatus *statusOpt
);

/*
 * nssPKIXDistributionPoint_SetReasons
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPoint_SetReasons
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSPKIXReasonFlags reasons
);

/*
 * nssPKIXDistributionPoint_RemoveReasons
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_REASONS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPoint_RemoveReasons
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXDistributionPoint_HasCRLIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXDistributionPoint_HasCRLIssuer
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXDistributionPoint_GetCRLIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_CRL_ISSUER
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralNames *
nssPKIXDistributionPoint_GetCRLIssuer
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSArena *arenaOpt
);

/*
 * nssPKIXDistributionPoint_SetCRLIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPoint_SetCRLIssuer
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSPKIXGeneralNames *cRLIssuer
);

/*
 * nssPKIXDistributionPoint_RemoveCRLIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_CRL_ISSUER
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPoint_RemoveCRLIssuer
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * nssPKIXDistributionPoint_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXDistributionPoint_Equal
(
  NSSPKIXDistributionPoint *distributionPoint1,
  NSSPKIXDistributionPoint *distributionPoint2,
  PRStatus *statusOpt
);

/*
 * nssPKIXDistributionPoint_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPoint upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPoint *
nssPKIXDistributionPoint_Duplicate
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXDistributionPoint_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXDistributionPoint
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPoint_verifyPointer
(
  NSSPKIXDistributionPoint *p
);
#endif /* DEBUG */

/*
 * DistributionPointName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  DistributionPointName ::= CHOICE {
 *       fullName                [0]     GeneralNames,
 *       nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }
 *
 * The private calls for this type:
 *
 *  nssPKIXDistributionPointName_Decode
 *  nssPKIXDistributionPointName_Create
 *  nssPKIXDistributionPointName_CreateFromFullName
 *  nssPKIXDistributionPointName_CreateFromNameRelativeToCRLIssuer
 *  nssPKIXDistributionPointName_Destroy
 *  nssPKIXDistributionPointName_Encode
 *  nssPKIXDistributionPointName_GetChoice
 *  nssPKIXDistributionPointName_GetFullName
 *  nssPKIXDistributionPointName_GetNameRelativeToCRLIssuer
 *  nssPKIXDistributionPointName_Equal
 *  nssPKIXDistributionPointName_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXDistributionPointName_verifyPointer
 *
 */

/*
 * nssPKIXDistributionPointName_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPointName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPointName *
nssPKIXDistributionPointName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXDistributionPointName_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME_CHOICE
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 *  NSS_ERROR_INVALID_PKIX_RELATIVE_DISTINGUISHED_NAME
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPointName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPointName *
nssPKIXDistributionPointName_Create
(
  NSSArena *arenaOpt,
  NSSPKIXDistributionPointNameChoice which,
  void *name
);

/*
 * nssPKIXDistributionPointName_CreateFromFullName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAMES
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPointName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPointName *
nssPKIXDistributionPointName_CreateFromFullName
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralNames *fullName
);

/*
 * nssPKIXDistributionPointName_CreateFromNameRelativeToCRLIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_RELATIVE_DISTINGUISHED_NAME
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPointName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPointName *
nssPKIXDistributionPointName_CreateFromNameRelativeToCRLIssuer
(
  NSSArena *arenaOpt,
  NSSPKIXRelativeDistinguishedName *nameRelativeToCRLIssuer
);

/*
 * nssPKIXDistributionPointName_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPointName_Destroy
(
  NSSPKIXDistributionPointName *dpn
);

/*
 * nssPKIXDistributionPointName_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXDistributionPointName_Encode
(
  NSSPKIXDistributionPointName *dpn,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXDistributionPointName_GetChoice
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 * 
 * Return value:
 *  A valid NSSPKIXDistributionPointNameChoice value upon success
 *  NSSPKIXDistributionPointNameChoice_NSSinvalid upon failure
 */

NSS_EXTERN NSSPKIXDistributionPointNameChoice
nssPKIXDistributionPointName_GetChoice
(
  NSSPKIXDistributionPointName *dpn
);

/*
 * nssPKIXDistributionPointName_GetFullName
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralNames upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralnames *
nssPKIXDistributionPointName_GetFullName
(
  NSSPKIXDistributionPointName *dpn,
  NSSArena *arenaOpt
);

/*
 * nssPKIXDistributionPointName_GetNameRelativeToCRLIssuer
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXRelativeDistinguishedName upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXRelativeDistinguishedName *
nssPKIXDistributionPointName_GetNameRelativeToCRLIssuer
(
  NSSPKIXDistributionPointName *dpn,
  NSSArena *arenaOpt
);

/*
 * nssPKIXDistributionPointName_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXDistributionPointName_Equal
(
  NSSPKIXDistributionPointName *dpn1,
  NSSPKIXDistributionPointName *dpn2,
  PRStatus *statusOpt
);

/*
 * nssPKIXDistributionPointName_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPointName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPointName *
nssPKIXDistributionPointName_Duplicate
(
  NSSPKIXDistributionPointName *dpn,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXDistributionPointName_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXDistributionPointName
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXDistributionPointName_verifyPointer
(
  NSSPKIXDistributionPointName *p
);
#endif /* DEBUG */

/*
 * ReasonFlags
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ReasonFlags ::= BIT STRING {
 *       unused                  (0),
 *       keyCompromise           (1),
 *       cACompromise            (2),
 *       affiliationChanged      (3),
 *       superseded              (4),
 *       cessationOfOperation    (5),
 *       certificateHold         (6) }
 *
 * The private calls for this type:
 *
 *  nssPKIXReasonFlags_Decode
 *  nssPKIXReasonFlags_Create
 *  nssPKIXReasonFlags_CreateFromMask
 *  nssPKIXReasonFlags_Destroy
 *  nssPKIXReasonFlags_Encode
 *  nssPKIXReasonFlags_GetMask
 *  nssPKIXReasonFlags_SetMask
 *  nssPKIXReasonFlags_Equal
 *  nssPKIXReasonFlags_Duplicate
 *    { bitwise accessors? }
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXReasonFlags_verifyPointer
 *
 */

/*
 * nssPKIXReasonFlags_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXReasonFlags upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXReasonFlags *
nssPKIXReasonFlags_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXReasonFlags_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *
 * Return value:
 *  A valid pointer to an NSSPKIXReasonFlags upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXReasonFlags *
nssPKIXReasonFlags_Create
(
  NSSArena *arenaOpt,
  PRBool keyCompromise,
  PRBool cACompromise,
  PRBool affiliationChanged,
  PRBool superseded,
  PRBool cessationOfOperation,
  PRBool certificateHold
);

/*
 * nssPKIXReasonFlags_CreateFromMask
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS_MASK
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXReasonFlags upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXReasonFlags *
nssPKIXReasonFlags_CreateFromMask
(
  NSSArena *arenaOpt,
  NSSPKIXReasonFlagsMask why
);

/*
 * nssPKIXReasonFlags_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXReasonFlags_Destroy
(
  NSSPKIXReasonFlags *reasonFlags
);

/*
 * nssPKIXReasonFlags_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXReasonFlags_Encode
(
  NSSPKIXReasonFlags *reasonFlags,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXReasonFlags_GetMask
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 * 
 * Return value:
 *  A valid mask of NSSPKIXReasonFlagsMask values upon success
 *  NSSPKIXReasonFlagsMask_NSSinvalid upon failure
 */

NSS_EXTERN NSSPKIXReasonFlagsMask
nssPKIXReasonFlags_GetMask
(
  NSSPKIXReasonFlags *reasonFlags
);

/*
 * nssPKIXReasonFlags_SetMask
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS_MASK
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXReasonFlags_SetMask
(
  NSSPKIXReasonFlags *reasonFlags,
  NSSPKIXReasonFlagsMask mask
);

/*
 * nssPKIXReasonFlags_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXReasonFlags_Equal
(
  NSSPKIXReasonFlags *reasonFlags1,
  NSSPKIXReasonFlags *reasonFlags2,
  PRStatus *statusOpt
);

/*
 * nssPKIXReasonFlags_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXReasonFlags upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXReasonFlags *
nssPKIXReasonFlags_Duplicate
(
  NSSPKIXReasonFlags *reasonFlags,
  NSSArena *arenaOpt
);

/*
 *   { bitwise accessors? }
 *
 */

#ifdef DEBUG
/*
 * nssPKIXReasonFlags_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXReasonFlags
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXReasonFlags_verifyPointer
(
  NSSPKIXReasonFlags *p
);
#endif /* DEBUG */

/*
 * ExtKeyUsageSyntax
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
 *
 * The private calls for this type:
 *
 *  nssPKIXExtKeyUsageSyntax_Decode
 *  nssPKIXExtKeyUsageSyntax_Create
 *  nssPKIXExtKeyUsageSyntax_Destroy
 *  nssPKIXExtKeyUsageSyntax_Encode
 *  nssPKIXExtKeyUsageSyntax_GetKeyPurposeIdCount
 *  nssPKIXExtKeyUsageSyntax_GetKeyPurposeIds
 *  nssPKIXExtKeyUsageSyntax_SetKeyPurposeIds
 *  nssPKIXExtKeyUsageSyntax_GetKeyPurposeId
 *  nssPKIXExtKeyUsageSyntax_SetKeyPurposeId
 *  nssPKIXExtKeyUsageSyntax_InsertKeyPurposeId
 *  nssPKIXExtKeyUsageSyntax_AppendKeyPurposeId
 *  nssPKIXExtKeyUsageSyntax_RemoveKeyPurposeId
 *  nssPKIXExtKeyUsageSyntax_FindKeyPurposeId
 *  nssPKIXExtKeyUsageSyntax_Equal
 *  nssPKIXExtKeyUsageSyntax_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXExtKeyUsageSyntax_verifyPointer
 *
 */

/*
 * nssPKIXExtKeyUsageSyntax_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtKeyUsageSyntax upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtKeyUsageSyntax *
nssPKIXExtKeyUsageSyntax_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXExtKeyUsageSyntax_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_KEY_PURPOSE_ID
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtKeyUsageSyntax upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtKeyUsageSyntax *
nssPKIXExtKeyUsageSyntax_Create
(
  NSSArena *arenaOpt,
  NSSPKIXKeyPurposeId *kpid1,
  ...
);

/*
 * nssPKIXExtKeyUsageSyntax_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtKeyUsageSyntax_Destroy
(
  NSSPKIXExtKeyUsageSyntax *eku
);

/*
 * nssPKIXExtKeyUsageSyntax_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXExtKeyUsageSyntax_Encode
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtKeyUsageSyntax_GetKeyPurposeIdCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXExtKeyUsageSyntax_GetKeyPurposeIdCount
(
  NSSPKIXExtKeyUsageSyntax *eku
);

/*
 * nssPKIXExtKeyUsageSyntax_GetKeyPurposeIds
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXKeyPurposeId pointers upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXKeyPurposeId **
nssPKIXExtKeyUsageSyntax_GetKeyPurposeIds
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSPKIXKeyPurposeId *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtKeyUsageSyntax_SetKeyPurposeIds
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_KEY_PURPOSE_ID
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtKeyUsageSyntax_SetKeyPurposeIds
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSPKIXKeyPurposeId *ids[],
  PRInt32 count
);

/*
 * nssPKIXExtKeyUsageSyntax_GetKeyPurposeId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXKeyPurposeId upon success
 *  NULL upon error
 */

NSS_EXTERN NSSPKIXKeyPurposeId *
nssPKIXExtKeyUsageSyntax_GetKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXExtKeyUsageSyntax_SetKeyPurposeId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_KEY_PURPOSE_ID
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtKeyUsageSyntax_SetKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  PRInt32 i,
  NSSPKIXKeyPurposeId *id
);

/*
 * nssPKIXExtKeyUsageSyntax_InsertKeyPurposeId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_KEY_PURPOSE_ID
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtKeyUsageSyntax_InsertKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  PRInt32 i,
  NSSPKIXKeyPurposeId *id
);

/*
 * nssPKIXExtKeyUsageSyntax_AppendKeyPurposeId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_KEY_PURPOSE_ID
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtKeyUsageSyntax_AppendKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSPKIXKeyPurposeId *id
);

/*
 * nssPKIXExtKeyUsageSyntax_RemoveKeyPurposeId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXExtKeyUsageSyntax_RemoveKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  PRInt32 i
);

/*
 * nssPKIXExtKeyUsageSyntax_FindKeyPurposeId
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_KEY_PURPOSE_ID
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  The index of the specified key purpose id upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXExtKeyUsageSyntax_FindKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSPKIXKeyPurposeId *id
);

/*
 * nssPKIXExtKeyUsageSyntax_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXExtKeyUsageSyntax_Equal
(
  NSSPKIXExtKeyUsageSyntax *eku1,
  NSSPKIXExtKeyUsageSyntax *eku2,
  PRStatus *statusOpt
);

/*
 * nssPKIXExtKeyUsageSyntax_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXExtKeyUsageSyntax upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXExtKeyUsageSyntax *
nssPKIXExtKeyUsageSyntax_Duplicate
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXExtKeyUsageSyntax_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXExtKeyUsageSyntax
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXT_KEY_USAGE_SYNTAX
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXExtKeyUsageSyntax_verifyPointer
(
  NSSPKIXExtKeyUsageSyntax *p
);
#endif /* DEBUG */

/*
 * AuthorityInfoAccessSyntax
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AuthorityInfoAccessSyntax  ::=
 *          SEQUENCE SIZE (1..MAX) OF AccessDescription
 *
 * The private calls for this type:
 *
 *  nssPKIXAuthorityInfoAccessSyntax_Decode
 *  nssPKIXAuthorityInfoAccessSyntax_Create
 *  nssPKIXAuthorityInfoAccessSyntax_Destroy
 *  nssPKIXAuthorityInfoAccessSyntax_Encode
 *  nssPKIXAuthorityInfoAccessSyntax_GetAccessDescriptionCount
 *  nssPKIXAuthorityInfoAccessSyntax_GetAccessDescriptions
 *  nssPKIXAuthorityInfoAccessSyntax_SetAccessDescriptions
 *  nssPKIXAuthorityInfoAccessSyntax_GetAccessDescription
 *  nssPKIXAuthorityInfoAccessSyntax_SetAccessDescription
 *  nssPKIXAuthorityInfoAccessSyntax_InsertAccessDescription
 *  nssPKIXAuthorityInfoAccessSyntax_AppendAccessDescription
 *  nssPKIXAuthorityInfoAccessSyntax_RemoveAccessDescription
 *  nssPKIXAuthorityInfoAccessSyntax_FindAccessDescription
 *  nssPKIXAuthorityInfoAccessSyntax_Equal
 *  nssPKIXAuthorityInfoAccessSyntax_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXAuthorityInfoAccessSyntax_verifyPointer
 *
 */

/*
 * nssPKIXAuthorityInfoAccessSyntax_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAuthorityInfoAccessSyntax upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAuthorityInfoAccessSyntax *
nssPKIXAuthorityInfoAccessSyntax_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAuthorityInfoAccessSyntax upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAuthorityInfoAccessSyntax *
nssPKIXAuthorityInfoAccessSyntax_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAccessDescription *ad1,
  ...
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityInfoAccessSyntax_Destroy
(
  NSSPKIXAuthorityInfoAccessSyntax *aias
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXAuthorityInfoAccessSyntax_Encode
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_GetAccessDescriptionCount
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  Nonnegative integer upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXAuthorityInfoAccessSyntax_GetAccessDescriptionCount
(
  NSSPKIXAuthorityInfoAccessSyntax *aias
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_GetAccessDescriptions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_ARRAY_TOO_SMALL
 * 
 * Return value:
 *  A valid pointer to an array of NSSPKIXAccessDescription pointers
 *      upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAccessDescription **
nssPKIXAuthorityInfoAccessSyntax_GetAccessDescriptions
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSPKIXAccessDescription *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_SetAccessDescriptions
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityInfoAccessSyntax_SetAccessDescriptions
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSPKIXAccessDescription *ad[],
  PRInt32 count
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_GetAccessDescription
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAccessDescription upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAccessDescription *
nssPKIXAuthorityInfoAccessSyntax_GetAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_SetAccessDescription
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityInfoAccessSyntax_SetAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  PRInt32 i,
  NSSPKIXAccessDescription *ad
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_InsertAccessDescription
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityInfoAccessSyntax_InsertAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  PRInt32 i,
  NSSPKIXAccessDescription *ad
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_AppendAccessDescription
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityInfoAccessSyntax_AppendAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSPKIXAccessDescription *ad
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_RemoveAccessDescription
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityInfoAccessSyntax_RemoveAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  PRInt32 i
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_FindAccessDescription
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 *  NSS_ERROR_NOT_FOUND
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 * 
 * Return value:
 *  The index of the specified policy mapping upon success
 *  -1 upon failure.
 */

NSS_EXTERN PRInt32
nssPKIXAuthorityInfoAccessSyntax_FindAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSPKIXAccessDescription *ad
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXAuthorityInfoAccessSyntax_Equal
(
  NSSPKIXAuthorityInfoAccessSyntax *aias1,
  NSSPKIXAuthorityInfoAccessSyntax *aias2,
  PRStatus *statusOpt
);

/*
 * nssPKIXAuthorityInfoAccessSyntax_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAuthorityInfoAccessSyntax upon
 *      success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAuthorityInfoAccessSyntax *
nssPKIXAuthorityInfoAccessSyntax_Duplicate
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXAuthorityInfoAccessSyntax_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXAuthorityInfoAccessSyntax
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_AUTHORITY_INFO_ACCESS_SYNTAX
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXAuthorityInfoAccessSyntax_verifyPointer
(
  NSSPKIXAuthorityInfoAccessSyntax *p
);
#endif /* DEBUG */

/*
 * AccessDescription
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  AccessDescription  ::=  SEQUENCE {
 *          accessMethod          OBJECT IDENTIFIER,
 *          accessLocation        GeneralName  }
 *
 * The private calls for this type:
 *
 *  nssPKIXAccessDescription_Decode
 *  nssPKIXAccessDescription_Create
 *  nssPKIXAccessDescription_Destroy
 *  nssPKIXAccessDescription_Encode
 *  nssPKIXAccessDescription_GetAccessMethod
 *  nssPKIXAccessDescription_SetAccessMethod
 *  nssPKIXAccessDescription_GetAccessLocation
 *  nssPKIXAccessDescription_SetAccessLocation
 *  nssPKIXAccessDescription_Equal
 *  nssPKIXAccessDescription_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXAccessDescription_verifyPointer
 *
 */

/*
 * nssPKIXAccessDescription_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAccessDescription upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAccessDescription *
nssPKIXAccessDescription_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXAccessDescription_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAccessDescription upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAccessDescription *
nssPKIXAccessDescription_Create
(
  NSSArena *arenaOpt,
  NSSOID *accessMethod,
  NSSPKIXGeneralName *accessLocation
);

/*
 * nssPKIXAccessDescription_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAccessDescription_Destroy
(
  NSSPKIXAccessDescription *ad
);

/*
 * nssPKIXAccessDescription_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXAccessDescription_Encode
(
  NSSPKIXAccessDescription *ad,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAccessDescription_GetAccessMethod
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSOID pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSOID *
nssPKIXAccessDescription_GetAccessMethod
(
  NSSPKIXAccessDescription *ad
);

/*
 * nssPKIXAccessDescription_SetAccessMethod
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 *  NSS_ERROR_INVALID_OID
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAccessDescription_SetAccessMethod
(
  NSSPKIXAccessDescription *ad,
  NSSOID *accessMethod
);

/*
 * nssPKIXAccessDescription_GetAccessLocation
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXGeneralName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXGeneralName *
nssPKIXAccessDescription_GetAccessLocation
(
  NSSPKIXAccessDescription *ad,
  NSSArena *arenaOpt
);

/*
 * nssPKIXAccessDescription_SetAccessLocation
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 *  NSS_ERROR_INVALID_PKIX_GENERAL_NAME
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXAccessDescription_SetAccessLocation
(
  NSSPKIXAccessDescription *ad,
  NSSPKIXGeneralName *accessLocation
);

/*
 * nssPKIXAccessDescription_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXAccessDescription_Equal
(
  NSSPKIXAccessDescription *ad1,
  NSSPKIXAccessDescription *ad2,
  PRStatus *statusOpt
);

/*
 * nssPKIXAccessDescription_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXAccessDescription upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXAccessDescription *
nssPKIXAccessDescription_Duplicate
(
  NSSPKIXAccessDescription *ad,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXAccessDescription_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXAccessDescription
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ACCESS_DESCRIPTION
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXAccessDescription_verifyPointer
(
  NSSPKIXAccessDescription *p
);
#endif /* DEBUG */

/*
 * IssuingDistributionPoint
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  IssuingDistributionPoint ::= SEQUENCE {
 *       distributionPoint       [0] DistributionPointName OPTIONAL,
 *       onlyContainsUserCerts   [1] BOOLEAN DEFAULT FALSE,
 *       onlyContainsCACerts     [2] BOOLEAN DEFAULT FALSE,
 *       onlySomeReasons         [3] ReasonFlags OPTIONAL,
 *       indirectCRL             [4] BOOLEAN DEFAULT FALSE }
 *
 * The private calls for this type:
 *
 *  nssPKIXIssuingDistributionPoint_Decode
 *  nssPKIXIssuingDistributionPoint_Create
 *  nssPKIXIssuingDistributionPoint_Destroy
 *  nssPKIXIssuingDistributionPoint_Encode
 *  nssPKIXIssuingDistributionPoint_HasDistributionPoint
 *  nssPKIXIssuingDistributionPoint_GetDistributionPoint
 *  nssPKIXIssuingDistributionPoint_SetDistributionPoint
 *  nssPKIXIssuingDistributionPoint_RemoveDistributionPoint
 *  nssPKIXIssuingDistributionPoint_GetOnlyContainsUserCerts
 *  nssPKIXIssuingDistributionPoint_SetOnlyContainsUserCerts
 *  nssPKIXIssuingDistributionPoint_GetOnlyContainsCACerts
 *  nssPKIXIssuingDistributionPoint_SetOnlyContainsCACerts
 *  nssPKIXIssuingDistributionPoint_HasOnlySomeReasons
 *  nssPKIXIssuingDistributionPoint_GetOnlySomeReasons
 *  nssPKIXIssuingDistributionPoint_SetOnlySomeReasons
 *  nssPKIXIssuingDistributionPoint_RemoveOnlySomeReasons
 *  nssPKIXIssuingDistributionPoint_GetIndirectCRL
 *  nssPKIXIssuingDistributionPoint_SetIndirectCRL
 *  nssPKIXIssuingDistributionPoint_Equal
 *  nssPKIXIssuingDistributionPoint_Duplicate
 * 
 * In debug builds, the following call is available:
 *
 *  nssPKIXIssuingDistributionPoint_verifyPointer
 *
 */

/*
 * nssPKIXIssuingDistributionPoint_Decode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXIssuingDistributionPoint upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXIssuingDistributionPoint *
nssPKIXIssuingDistributionPoint_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * nssPKIXIssuingDistributionPoint_Create
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXIssuingDistributionPoint upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXIssuingDistributionPoint *
nssPKIXIssuingDistributionPoint_Create
(
  NSSArena *arenaOpt,
  NSSPKIXDistributionPointName *distributionPointOpt,
  PRBool onlyContainsUserCerts,
  PRBool onlyContainsCACerts,
  NSSPKIXReasonFlags *onlySomeReasons
  PRBool indirectCRL
);

/*
 * nssPKIXIssuingDistributionPoint_Destroy
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXIssuingDistributionPoint_Destroy
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * nssPKIXIssuingDistributionPoint_Encode
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
nssPKIXIssuingDistributionPoint_Encode
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssPKIXIssuingDistributionPoint_HasDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXIssuingDistributionPoint_HasDistributionPoint
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * nssPKIXIssuingDistributionPoint_GetDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_DISTRIBUTION_POINT
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXDistributionPointName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXDistributionPointName *
nssPKIXIssuingDistributionPoint_GetDistributionPoint
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSArena *arenaOpt
);

/*
 * nssPKIXIssuingDistributionPoint_SetDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 *  NSS_ERROR_INVALID_PKIX_DISTRIBUTION_POINT_NAME
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXIssuingDistributionPoint_SetDistributionPoint
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSPKIXDistributionPointName *dpn
);

/*
 * nssPKIXIssuingDistributionPoint_RemoveDistributionPoint
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXIssuingDistributionPoint_RemoveDistributionPoint
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * nssPKIXIssuingDistributionPoint_GetOnlyContainsUserCerts
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if the onlyContainsUserCerts value is true
 *  PR_FALSE if it isn't
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXIssuingDistributionPoint_GetOnlyContainsUserCerts
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRStatus *statusOpt
);

/*
 * nssPKIXIssuingDistributionPoint_SetOnlyContainsUserCerts
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXIssuingDistributionPoint_SetOnlyContainsUserCerts
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRBool onlyContainsUserCerts
);

/*
 * nssPKIXIssuingDistributionPoint_GetOnlyContainsCACerts
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if the onlyContainsCACerts value is true
 *  PR_FALSE if it isn't
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXIssuingDistributionPoint_GetOnlyContainsCACerts
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRStatus *statusOpt
);

/*
 * nssPKIXIssuingDistributionPoint_SetOnlyContainsCACerts
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXIssuingDistributionPoint_SetOnlyContainsCACerts
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRBool onlyContainsCACerts
);

/*
 * nssPKIXIssuingDistributionPoint_HasOnlySomeReasons
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if it has one
 *  PR_FALSE if it doesn't
 *  PR_FALSE upon failure
 */

NSS_EXTERN PRBool
nssPKIXIssuingDistributionPoint_HasOnlySomeReasons
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * nssPKIXIssuingDistributionPoint_GetOnlySomeReasons
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_ONLY_SOME_REASONS
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXReasonFlags upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXReasonFlags *
nssPKIXIssuingDistributionPoint_GetOnlySomeReasons
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSArena *arenaOpt
);

/*
 * nssPKIXIssuingDistributionPoint_SetOnlySomeReasons
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 *  NSS_ERROR_INVALID_PKIX_REASON_FLAGS
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXIssuingDistributionPoint_SetOnlySomeReasons
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSPKIXReasonFlags *onlySomeReasons
);

/*
 * nssPKIXIssuingDistributionPoint_RemoveOnlySomeReasons
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 *  NSS_ERROR_HAS_NO_ONLY_SOME_REASONS
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXIssuingDistributionPoint_RemoveOnlySomeReasons
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * nssPKIXIssuingDistributionPoint_GetIndirectCRL
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if the indirectCRL value is true
 *  PR_FALSE if it isn't
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXIssuingDistributionPoint_GetIndirectCRL
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRStatus *statusOpt
);

/*
 * nssPKIXIssuingDistributionPoint_SetIndirectCRL
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssPKIXIssuingDistributionPoint_SetIndirectCRL
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRBool indirectCRL
);

/*
 * nssPKIXIssuingDistributionPoint_Equal
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 * 
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
nssPKIXIssuingDistributionPoint_Equal
(
  NSSPKIXIssuingDistributionPoint *idp1,
  NSSPKIXIssuingDistributionPoint *idp2,
  PRStatus *statusOpt
);

/*
 * nssPKIXIssuingDistributionPoint_Duplicate
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXIssuingDistributionPoint upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXIssuingDistributionPoint *
nssPKIXIssuingDistributionPoint_Duplicate
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSArena *arenaOpt
);

#ifdef DEBUG
/*
 * nssPKIXIssuingDistributionPoint_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSPKIXIssuingDistributionPoint
 * object, this routine will return PR_SUCCESS.  Otherwise, it will 
 * put an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_ISSUING_DISTRIBUTION_POINT
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

NSS_EXTERN PRStatus
nssPKIXIssuingDistributionPoint_verifyPointer
(
  NSSPKIXIssuingDistributionPoint *p
);
#endif /* DEBUG */

PR_END_EXTERN_C

#endif /* NSSPKIX_H */
