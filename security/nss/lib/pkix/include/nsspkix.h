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

#ifndef NSSPKIX_H
#define NSSPKIX_H

#ifdef DEBUG
static const char NSSPKIX_CVS_ID[] = "@(#) $Source: /home/hwine/cvs_conversion/cvsroot/mozilla/security/nss/lib/pkix/include/Attic/nsspkix.h,v $ $Revision: 1.1 $ $Date: 2000/03/31 19:02:06 $ $Name:  $";
#endif /* DEBUG */

/*
 * nsspkix.h
 *
 * This file contains the prototypes for the public methods defined
 * for the PKIX part-1 objects.
 */

#ifndef NSSPKIXT_H
#include "nsspkixt.h"
#endif /* NSSPKIXT_H */

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
 * The public calls for the type:
 * 
 *  NSSPKIXAttribute_Decode
 *  NSSPKIXAttribute_Create
 *  NSSPKIXAttribute_CreateFromArray
 *  NSSPKIXAttribute_Destroy
 *  NSSPKIXAttribute_Encode
 *  NSSPKIXAttribute_GetType
 *  NSSPKIXAttribute_SetType
 *  NSSPKIXAttribute_GetValueCount
 *  NSSPKIXAttribute_GetValues
 *  NSSPKIXAttribute_SetValues
 *  NSSPKIXAttribute_GetValue
 *  NSSPKIXAttribute_SetValue
 *  NSSPKIXAttribute_AddValue
 *  NSSPKIXAttribute_RemoveValue
 *  NSSPKIXAttribute_FindValue
 *  NSSPKIXAttribute_Equal
 *  NSSPKIXAttribute_Duplicate
 *
 */

/*
 * NSSPKIXAttribute_Decode
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
NSSPKIXAttribute_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXAttribute_Create
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
NSSPKIXAttribute_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeType *typeOid,
  NSSPKIXAttributeValue *value1,
  ...
);

/*
 * NSSPKIXAttribute_CreateFromArray
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
NSSPKIXAttribute_CreateFromArray
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeType *typeOid,
  PRUint32 count,
  NSSPKIXAttributeValue values[]
);

/*
 * NSSPKIXAttribute_Destroy
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
NSSPKIXAttribute_Destroy
(
  NSSPKIXAttribute *attribute
);

/*
 * NSSPKIXAttribute_Encode
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
NSSPKIXAttribute_Encode
(
  NSSPKIXAttribute *attribute,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAttribute_GetType
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
NSSPKIXAttribute_GetType
(
  NSSPKIXAttribute *attribute
);

/*
 * NSSPKIXAttribute_SetType
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
NSSPKIXAttribute_SetType
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeType *attributeType
);

/*
 * NSSPKIXAttribute_GetValueCount
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
NSSPKIXAttribute_GetValueCount
(
  NSSPKIXAttribute *attribute
);

/*
 * NSSPKIXAttribute_GetValues
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
NSSPKIXAttribute_GetValues
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeValue rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAttribute_SetValues
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
NSSPKIXAttribute_SetValues
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeValue values[],
  PRInt32 count
);

/*
 * NSSPKIXAttribute_GetValue
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
NSSPKIXAttribute_GetValue
(
  NSSPKIXAttribute *attribute,
  PRInt32 i,
  NSSPKIXAttributeValue *itemOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAttribute_SetValue
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
NSSPKIXAttribute_SetValue
(
  NSSPKIXAttribute *attribute,
  PRInt32 i,
  NSSPKIXAttributeValue *value
);

/*
 * NSSPKIXAttribute_AddValue
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
NSSPKIXAttribute_AddValue
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeValue *value
); 

/*
 * NSSPKIXAttribute_RemoveValue
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
NSSPKIXAttribute_RemoveValue
(
  NSSPKIXAttribute *attribute,
  PRInt32 i
);

/*
 * NSSPKIXAttribute_FindValue
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
NSSPKIXAttribute_FindValue
(
  NSSPKIXAttribute *attribute,
  NSSPKIXAttributeValue *attributeValue
);

/*
 * NSSPKIXAttribute_Equal
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
NSSPKIXAttribute_Equal
(
  NSSPKIXAttribute *one,
  NSSPKIXAttribute *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXAttribute_Duplicate
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
NSSPKIXAttribute_Duplicate
(
  NSSPKIXAttribute *attribute,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXAttributeTypeAndValue_Decode
 *  NSSPKIXAttributeTypeAndValue_CreateFromUTF8
 *  NSSPKIXAttributeTypeAndValue_Create
 *  NSSPKIXAttributeTypeAndValue_Destroy
 *  NSSPKIXAttributeTypeAndValue_Encode
 *  NSSPKIXAttributeTypeAndValue_GetUTF8Encoding
 *  NSSPKIXAttributeTypeAndValue_GetType
 *  NSSPKIXAttributeTypeAndValue_SetType
 *  NSSPKIXAttributeTypeAndValue_GetValue
 *  NSSPKIXAttributeTypeAndValue_SetValue
 *  NSSPKIXAttributeTypeAndValue_Equal
 *  NSSPKIXAttributeTypeAndValue_Duplicate
 */

/*
 * NSSPKIXAttributeTypeAndValue_Decode
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
NSSPKIXAttributeTypeAndValue_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXAttributeTypeAndValue_CreateFromUTF8
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
NSSPKIXAttributeTypeAndValue_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *string
);

/*
 * NSSPKIXAttributeTypeAndValue_Create
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
NSSPKIXAttributeTypeAndValue_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeType *typeOid,
  NSSPKIXAttributeValue *value
);

/*
 * NSSPKIXAttributeTypeAndValue_Destroy
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
NSSPKIXAttributeTypeAndValue_Destroy
(
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * NSSPKIXAttributeTypeAndValue_Encode
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
NSSPKIXAttributeTypeAndValue_Encode
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAttributeTypeAndValue_GetUTF8Encoding
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
NSSPKIXAttributeTypeAndValue_GetUTF8Encoding
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAttributeTypeAndValue_GetType
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
NSSPKIXAttributeTypeAndValue_GetType
(
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * NSSPKIXAttributeTypeAndValue_SetType
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
NSSPKIXAttributeTypeAndValue_SetType
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSPKIXAttributeType *attributeType
);

/*
 * NSSPKIXAttributeTypeAndValue_GetValue
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
NSSPKIXAttributeTypeAndValue_GetValue
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSPKIXAttributeValue *itemOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAttributeTypeAndValue_SetValue
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
NSSPKIXAttributeTypeAndValue_SetValue
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSPKIXAttributeValue *value
);

/*
 * NSSPKIXAttributeTypeAndValue_Equal
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
NSSPKIXAttributeTypeAndValue_Equal
(
  NSSPKIXAttributeTypeAndValue *atav1,
  NSSPKIXAttributeTypeAndValue *atav2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXAttributeTypeAndValue_Duplicate
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
NSSPKIXAttributeTypeAndValue_Duplicate
(
  NSSPKIXAttributeTypeAndValue *atav,
  NSSArena *arenaOpt
);

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
 *  ub-name INTEGER ::=     32768
 *
 * The public calls for this type:
 *
 *  NSSPKIXX520Name_Decode
 *  NSSPKIXX520Name_CreateFromUTF8
 *  NSSPKIXX520Name_Create (?)
 *  NSSPKIXX520Name_Destroy
 *  NSSPKIXX520Name_Encode
 *  NSSPKIXX520Name_GetUTF8Encoding
 *  NSSPKIXX520Name_Equal
 *  NSSPKIXX520Name_Duplicate
 *
 * The public data for this type:
 *
 *  NSSPKIXX520Name_MAXIMUM_LENGTH
 *
 */

/*
 * NSSPKIXX520Name_Decode
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
NSSPKIXX520Name_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX520Name_CreateFromUTF8
 *
 * { basically just enforces the length limit }
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520Name upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520Name *
NSSPKIXX520Name_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX520Name_Create
 *
 * XXX fgmr: currently nssStringType is a private type.  Thus,
 * this public method should not exist.  I'm leaving this here
 * to remind us later what we want to decide.
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

/* 
 * NSS_EXTERN NSSPKIXX520Name *
 * NSSPKIXX520Name_Create
 * (
 *   NSSArena *arenaOpt,
 *   nssStringType type,
 *   NSSItem *data
 * );
 */

/*
 * NSSPKIXX520Name_Destroy
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
NSSPKIXX520Name_Destroy
(
  NSSPKIXX520Name *name
);

/*
 * NSSPKIXX520Name_Encode
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
NSSPKIXX520Name_Encode
(
  NSSPKIXX520Name *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXX520Name_GetUTF8Encoding
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
NSSPKIXX520Name_GetUTF8Encoding
(
  NSSPKIXX520Name *name,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXX520Name_Equal
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
NSSPKIXX520Name_Equal
(
  NSSPKIXX520Name *name1,
  NSSPKIXX520Name *name2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXX520Name_Duplicate
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
NSSPKIXX520Name_Duplicate
(
  NSSPKIXX520Name *name,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXX520Name_MAXIMUM_LENGTH
 *
 * From RFC 2459:
 *
 *  ub-name INTEGER ::=     32768
 */

extern const PRUint32 NSSPKIXX520Name_MAXIMUM_LENGTH;

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
 *  ub-common-name  INTEGER ::=     64
 * 
 * The public calls for this type:
 *
 *  NSSPKIXX520CommonName_Decode
 *  NSSPKIXX520CommonName_CreateFromUTF8
 *  NSSPKIXX520CommonName_Create (?)
 *  NSSPKIXX520CommonName_Destroy
 *  NSSPKIXX520CommonName_Encode
 *  NSSPKIXX520CommonName_GetUTF8Encoding
 *  NSSPKIXX520CommonName_Equal
 *  NSSPKIXX520CommonName_Duplicate
 *
 * The public data for this type:
 *
 *  NSSPKIXX520CommonName_MAXIMUM_LENGTH
 *
 */

/*
 * NSSPKIXX520CommonName_Decode
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
NSSPKIXX520CommonName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX520CommonName_Create
 *
 * XXX fgmr: currently nssStringType is a private type.  Thus,
 * this public method should not exist.  I'm leaving this here
 * to remind us later what we want to decide.
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

/* 
 * NSS_EXTERN NSSPKIXX520CommonName *
 * NSSPKIXX520CommonName_Create
 * (
 *   NSSArena *arenaOpt,
 *   nssStringType type,
 *   NSSItem *data
 * );
 */

/*
 * NSSPKIXX520CommonName_Destroy
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_COMMON_NAME
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN NSSBER *
NSSPKIXX520CommonName_Destroy
(
  NSSPKIXX520CommonName *name
);

/*
 * NSSPKIXX520CommonName_CreateFromUTF8
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
NSSPKIXX520CommonName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX520CommonName_Encode
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_COMMON_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSBER *
NSSPKIXX520CommonName_Encode
(
  NSSPKIXX520CommonName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXX520CommonName_GetUTF8Encoding
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_COMMON_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  A valid NSSBER pointer upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSUTF8 *
NSSPKIXX520CommonName_GetUTF8Encoding
(
  NSSPKIXX520CommonName *name,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXX520CommonName_Equal
 *
 *
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_COMMON_NAME
 *
 * Return value:
 *  PR_TRUE if the two objects have equal values
 *  PR_FALSE otherwise
 *  PR_FALSE upon error
 */

NSS_EXTERN PRBool
NSSPKIXX520CommonName_Equal
(
  NSSPKIXX520CommonName *name1,
  NSSPKIXX520CommonName *name2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXX520CommonName_Duplicate
 *
 * 
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_X520_COMMON_NAME
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ARENA
 * 
 * Return value:
 *  A valid pointer to an NSSPKIXX520CommonName upon success
 *  NULL upon failure
 */

NSS_EXTERN NSSPKIXX520CommonName *
NSSPKIXX520CommonName_Duplicate
(
  NSSPKIXX520CommonName *name,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXX520CommonName_MAXIMUM_LENGTH
 *
 * From RFC 2459:
 *
 *  ub-common-name  INTEGER ::=     64
 */

extern const PRUint32 NSSPKIXX520CommonName_MAXIMUM_LENGTH;

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
 * The public calls for this type:
 *
 *  NSSPKIXX520LocalityName_Decode
 *  NSSPKIXX520LocalityName_CreateFromUTF8
 *  NSSPKIXX520LocalityName_Encode
 *
 */

/*
 * NSSPKIXX520LocalityName_Decode
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
NSSPKIXX520LocalityName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX520LocalityName_CreateFromUTF8
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
NSSPKIXX520LocalityName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX520LocalityName_Encode
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
NSSPKIXX520LocalityName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXX520StateOrProvinceName_Decode
 *  NSSPKIXX520StateOrProvinceName_CreateFromUTF8
 *  NSSPKIXX520StateOrProvinceName_Encode
 *
 */

/*
 * NSSPKIXX520StateOrProvinceName_Decode
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
NSSPKIXX520StateOrProvinceName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX520StateOrProvinceName_CreateFromUTF8
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
NSSPKIXX520StateOrProvinceName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX520StateOrProvinceName_Encode
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
NSSPKIXX520StateOrProvinceName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXX520OrganizationName_Decode
 *  NSSPKIXX520OrganizationName_CreateFromUTF8
 *  NSSPKIXX520OrganizationName_Encode
 *
 */

/*
 * NSSPKIXX520OrganizationName_Decode
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
NSSPKIXX520OrganizationName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX520OrganizationName_CreateFromUTF8
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
NSSPKIXX520OrganizationName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX520OrganizationName_Encode
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
NSSPKIXX520OrganizationName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXX520OrganizationalUnitName_Decode
 *  NSSPKIXX520OrganizationalUnitName_CreateFromUTF8
 *  NSSPKIXX520OrganizationalUnitName_Encode
 *
 */

/*
 * NSSPKIXX520OrganizationalUnitName_Decode
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
NSSPKIXX520OrganizationalUnitName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX520OrganizationalUnitName_CreateFromUTF8
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
NSSPKIXX520OrganizationalUnitName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX520OrganizationalUnitName_Encode
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
NSSPKIXX520OrganizationalUnitName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXX520Title_Decode
 *  NSSPKIXX520Title_CreateFromUTF8
 *  NSSPKIXX520Title_Encode
 *
 */

/*
 * NSSPKIXX520Title_Decode
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
NSSPKIXX520Title_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX520Title_CreateFromUTF8
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
NSSPKIXX520Title_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX520Title_Encode
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
NSSPKIXX520Title_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXX520dnQualifier_Decode
 *  NSSPKIXX520dnQualifier_CreateFromUTF8
 *  NSSPKIXX520dnQualifier_Encode
 *
 */

/*
 * NSSPKIXX520dnQualifier_Decode
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
NSSPKIXX520dnQualifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX520dnQualifier_CreateFromUTF8
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
NSSPKIXX520dnQualifier_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX520dnQualifier_Encode
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
NSSPKIXX520dnQualifier_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXX520countryName_Decode
 *  NSSPKIXX520countryName_CreateFromUTF8
 *  NSSPKIXX520countryName_Encode
 *
 */

/*
 * NSSPKIXX520countryName_Decode
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
NSSPKIXX520countryName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX520countryName_CreateFromUTF8
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
NSSPKIXX520countryName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX520countryName_Encode
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
NSSPKIXX520countryName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXPkcs9email_Decode
 *  NSSPKIXPkcs9email_CreateFromUTF8
 *  NSSPKIXPkcs9email_Encode
 *
 */

/*
 * NSSPKIXPkcs9email_Decode
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
NSSPKIXPkcs9email_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPkcs9email_CreateFromUTF8
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
NSSPKIXPkcs9email_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXPkcs9email_Encode
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
NSSPKIXPkcs9email_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXName_Decode
 *  NSSPKIXName_CreateFromUTF8
 *  NSSPKIXName_Create
 *  NSSPKIXName_CreateFromRDNSequence
 *  NSSPKIXName_Destroy
 *  NSSPKIXName_Encode
 *  NSSPKIXName_GetUTF8Encoding
 *  NSSPKIXName_GetChoice
 *  NSSPKIXName_GetRDNSequence
 *  NSSPKIXName_GetSpecifiedChoice {fgmr remove this}
 *  NSSPKIXName_SetRDNSequence
 *  NSSPKIXName_SetSpecifiedChoice
 *  NSSPKIXName_Equal
 *  NSSPKIXName_Duplicate
 *
 *  (here is where I had specific attribute value gettors in pki1)
 *
 */

/*
 * NSSPKIXName_Decode
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
NSSPKIXName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXName_CreateFromUTF8
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
NSSPKIXName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *string
);

/*
 * NSSPKIXName_Create
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
NSSPKIXName_Create
(
  NSSArena *arenaOpt,
  NSSPKIXNameChoice choice,
  void *arg
);

/*
 * NSSPKIXName_CreateFromRDNSequence
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
NSSPKIXName_CreateFromRDNSequence
(
  NSSArena *arenaOpt,
  NSSPKIXRDNSequence *rdnSequence
);

/*
 * NSSPKIXName_Destroy
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
NSSPKIXName_Destroy
(
  NSSPKIXName *name
);

/*
 * NSSPKIXName_Encode
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
NSSPKIXName_Encode
(
  NSSPKIXName *name,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXName_GetUTF8Encoding
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
NSSPKIXName_GetUTF8Encoding
(
  NSSPKIXName *name,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXName_GetChoice
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
NSSPKIXName_GetChoice
(
  NSSPKIXName *name
);

/*
 * NSSPKIXName_GetRDNSequence
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
NSSPKIXName_GetRDNSequence
(
  NSSPKIXName *name,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXName_GetSpecifiedChoice
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
NSSPKIXName_GetSpecifiedChoice
(
  NSSPKIXName *name,
  NSSPKIXNameChoice choice,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXName_Equal
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
NSSPKIXName_Equal
(
  NSSPKIXName *name1,
  NSSPKIXName *name2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXName_Duplicate
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
NSSPKIXName_Duplicate
(
  NSSPKIXName *name,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXRDNSequence_Decode
 *  NSSPKIXRDNSequence_CreateFromUTF8
 *  NSSPKIXRDNSequence_Create
 *  NSSPKIXRDNSequence_CreateFromArray
 *  NSSPKIXRDNSequence_Destroy
 *  NSSPKIXRDNSequence_Encode
 *  NSSPKIXRDNSequence_GetUTF8Encoding
 *  NSSPKIXRDNSequence_GetRelativeDistinguishedNameCount
 *  NSSPKIXRDNSequence_GetRelativeDistinguishedNames
 *  NSSPKIXRDNSequence_SetRelativeDistinguishedNames
 *  NSSPKIXRDNSequence_GetRelativeDistinguishedName
 *  NSSPKIXRDNSequence_SetRelativeDistinguishedName
 *  NSSPKIXRDNSequence_AppendRelativeDistinguishedName
 *  NSSPKIXRDNSequence_InsertRelativeDistinguishedName
 *  NSSPKIXRDNSequence_RemoveRelativeDistinguishedName
 *  NSSPKIXRDNSequence_FindRelativeDistinguishedName
 *  NSSPKIXRDNSequence_Equal
 *  NSSPKIXRDNSequence_Duplicate
 *
 */

/*
 * NSSPKIXRDNSequence_Decode
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
NSSPKIXRDNSequence_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXRDNSequence_CreateFromUTF8
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
NSSPKIXRDNSequence_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *string
);

/*
 * NSSPKIXRDNSequence_Create
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
NSSPKIXRDNSequence_Create
(
  NSSArena *arenaOpt,
  NSSPKIXRelativeDistinguishedName *rdn1,
  ...
);

/*
 * NSSPKIXRDNSequence_CreateFromArray
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
NSSPKIXRDNSequence_Create
(
  NSSArena *arenaOpt,
  PRUint32 count,
  NSSPKIXRelativeDistinguishedName *rdns[]
);

/*
 * NSSPKIXRDNSequence_Destroy
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
NSSPKIXRDNSequence_Destroy
(
  NSSPKIXRDNSequence *rdnseq
);

/*
 * NSSPKIXRDNSequence_Encode
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
NSSPKIXRDNSequence_Encode
(
  NSSPKIXRDNSequence *rdnseq,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXRDNSequence_GetUTF8Encoding
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
NSSPKIXRDNSequence_GetUTF8Encoding
(
  NSSPKIXRDNSequence *rdnseq,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXRDNSequence_GetRelativeDistinguishedNameCount
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
NSSPKIXRDNSequence_GetRelativeDistinguishedNameCount
(
  NSSPKIXRDNSequence *rdnseq
);

/*
 * NSSPKIXRDNSequence_GetRelativeDistinguishedNames
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
NSSPKIXRDNSequence_GetRelativeDistinguishedNames
(
  NSSPKIXRDNSequence *rdnseq,
  NSSPKIXRelativeDistinguishedName *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXRDNSequence_SetRelativeDistinguishedNames
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
NSSPKIXRDNSequence_SetRelativeDistinguishedNames
(
  NSSPKIXRDNSequence *rdnseq,
  NSSPKIXRelativeDistinguishedName *rdns[],
  PRInt32 countOpt
);

/*
 * NSSPKIXRDNSequence_GetRelativeDistinguishedName
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
NSSPKIXRDNSequence_GetRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXRDNSequence_SetRelativeDistinguishedName
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
NSSPKIXRDNSequence_SetRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  PRInt32 i,
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * NSSPKIXRDNSequence_AppendRelativeDistinguishedName
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
NSSPKIXRDNSequence_AppendRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * NSSPKIXRDNSequence_InsertRelativeDistinguishedName
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
NSSPKIXRDNSequence_InsertRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  PRInt32 i,
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * NSSPKIXRDNSequence_RemoveRelativeDistinguishedName
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
NSSPKIXRDNSequence_RemoveRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  PRInt32 i
);

/*
 * NSSPKIXRDNSequence_FindRelativeDistinguishedName
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
NSSPKIXRDNSequence_FindRelativeDistinguishedName
(
  NSSPKIXRDNSequence *rdnseq,
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * NSSPKIXRDNSequence_Equal
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
NSSPKIXRDNSequence_Equal
(
  NSSPKIXRDNSequence *one,
  NSSPKIXRDNSequence *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXRDNSequence_Duplicate
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
NSSPKIXRDNSequence_Duplicate
(
  NSSPKIXRDNSequence *rdnseq,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXRelativeDistinguishedName_Decode
 *  NSSPKIXRelativeDistinguishedName_CreateFromUTF8
 *  NSSPKIXRelativeDistinguishedName_Create
 *  NSSPKIXRelativeDistinguishedName_CreateFromArray
 *  NSSPKIXRelativeDistinguishedName_Destroy
 *  NSSPKIXRelativeDistinguishedName_Encode
 *  NSSPKIXRelativeDistinguishedName_GetUTF8Encoding
 *  NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValueCount
 *  NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValues
 *  NSSPKIXRelativeDistinguishedName_SetAttributeTypeAndValues
 *  NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValue
 *  NSSPKIXRelativeDistinguishedName_SetAttributeTypeAndValue
 *  NSSPKIXRelativeDistinguishedName_AddAttributeTypeAndValue
 *  NSSPKIXRelativeDistinguishedName_RemoveAttributeTypeAndValue
 *  NSSPKIXRelativeDistinguishedName_FindAttributeTypeAndValue
 *  NSSPKIXRelativeDistinguishedName_Equal
 *  NSSPKIXRelativeDistinguishedName_Duplicate
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
 */

/*
 * NSSPKIXRelativeDistinguishedName_Decode
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
NSSPKIXRelativeDistinguishedName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXRelativeDistinguishedName_CreateFromUTF8
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
NSSPKIXRelativeDistinguishedName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *string
);

/*
 * NSSPKIXRelativeDistinguishedName_Create
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
NSSPKIXRelativeDistinguishedName_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAttributeTypeAndValue *atav1,
  ...
);

/*
 * NSSPKIXRelativeDistinguishedName_CreateFromArray
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
NSSPKIXRelativeDistinguishedName_CreateFromArray
(
  NSSArena *arenaOpt,
  PRUint32 count,
  NSSPKIXAttributeTypeAndValue *atavs[]
);

/*
 * NSSPKIXRelativeDistinguishedName_Destroy
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
NSSPKIXRelativeDistinguishedName_Destroy
(
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * NSSPKIXRelativeDistinguishedName_Encode
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
NSSPKIXRelativeDistinguishedName_Encode
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXRelativeDistinguishedName_GetUTF8Encoding
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
NSSPKIXRelativeDistinguishedName_GetUTF8Encoding
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValueCount
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
NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValueCount
(
  NSSPKIXRelativeDistinguishedName *rdn
);

/*
 * NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValues
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
NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValues
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXRelativeDistinguishedName_SetAttributeTypeAndValues
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
NSSPKIXRelativeDistinguishedName_SetAttributeTypeAndValues
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *atavs[],
  PRInt32 countOpt
);

/*
 * NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValue
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
NSSPKIXRelativeDistinguishedName_GetAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXRelativeDistinguishedName_SetAttributeTypeAndValue
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
NSSPKIXRelativeDistinguishedName_SetAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  PRInt32 i,
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * NSSPKIXRelativeDistinguishedName_AddAttributeTypeAndValue
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
NSSPKIXRelativeDistinguishedName_AddAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * NSSPKIXRelativeDistinguishedName_RemoveAttributeTypeAndValue
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
NSSPKIXRelativeDistinguishedName_RemoveAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  PRInt32 i
);

/*
 * NSSPKIXRelativeDistinguishedName_FindAttributeTypeAndValue
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
NSSPKIXRelativeDistinguishedName_FindAttributeTypeAndValue
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSPKIXAttributeTypeAndValue *atav
);

/*
 * NSSPKIXRelativeDistinguishedName_Equal
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
NSSPKIXRelativeDistinguishedName_Equal
(
  NSSPKIXRelativeDistinguishedName *one,
  NSSPKIXRelativeDistinguishedName *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXRelativeDistinguishedName_Duplicate
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
NSSPKIXRelativeDistinguishedName_Duplicate
(
  NSSPKIXRelativeDistinguishedName *rdn,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXDirectoryString_Decode
 *  NSSPKIXDirectoryString_CreateFromUTF8
 *  NSSPKIXDirectoryString_Encode
 *
 */

/*
 * NSSPKIXDirectoryString_Decode
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
NSSPKIXDirectoryString_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXDirectoryString_CreateFromUTF8
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
NSSPKIXDirectoryString_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXDirectoryString_Encode
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
NSSPKIXDirectoryString_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXCertificate_Decode
 *  NSSPKIXCertificate_Create
 *  NSSPKIXCertificate_Destroy
 *  NSSPKIXCertificate_Encode
 *  NSSPKIXCertificate_GetTBSCertificate
 *  NSSPKIXCertificate_SetTBSCertificate
 *  NSSPKIXCertificate_GetAlgorithmIdentifier
 *  NSSPKIXCertificate_SetAlgorithmIdentifier
 *  NSSPKIXCertificate_GetSignature
 *  NSSPKIXCertificate_SetSignature
 *  NSSPKIXCertificate_Equal
 *  NSSPKIXCertificate_Duplicate
 *
 *  { inherit TBSCertificate gettors? }
 *
 */

/*
 * NSSPKIXCertificate_Decode
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
NSSPKIXCertificate_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXCertificate_Create
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
NSSPKIXCertificate_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXAlgorithmIdentifier *algID,
  NSSItem *signature
);

/*
 * NSSPKIXCertificate_Destroy
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
NSSPKIXCertificate_Destroy
(
  NSSPKIXCertificate *cert
);

/*
 * NSSPKIXCertificate_Encode
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
NSSPKIXCertificate_Encode
(
  NSSPKIXCertificate *cert,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificate_GetTBSCertificate
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
NSSPKIXCertificate_GetTBSCertificate
(
  NSSPKIXCertificate *cert,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificate_SetTBSCertificate
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
NSSPKIXCertificate_SetTBSCertificate
(
  NSSPKIXCertificate *cert,
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * NSSPKIXCertificate_GetAlgorithmIdentifier
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
NSSPKIXCertificate_GetAlgorithmIdentifier
(
  NSSPKIXCertificate *cert,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificate_SetAlgorithmIdentifier
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
NSSPKIXCertificate_SetAlgorithmIdentifier
(
  NSSPKIXCertificate *cert,
  NSSPKIXAlgorithmIdentifier *algid,
);

/*
 * NSSPKIXCertificate_GetSignature
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
NSSPKIXCertificate_GetSignature
(
  NSSPKIXCertificate *cert,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificate_SetSignature
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
NSSPKIXCertificate_SetSignature
(
  NSSPKIXCertificate *cert,
  NSSItem *signature
);

/*
 * NSSPKIXCertificate_Equal
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
NSSPKIXCertificate_Equal
(
  NSSPKIXCertificate *one,
  NSSPKIXCertificate *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXCertificate_Duplicate
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
NSSPKIXCertificate_Duplicate
(
  NSSPKIXCertificate *cert,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXTBSCertificate_Decode
 *  NSSPKIXTBSCertificate_Create
 *  NSSPKIXTBSCertificate_Destroy
 *  NSSPKIXTBSCertificate_Encode
 *  NSSPKIXTBSCertificate_GetVersion
 *  NSSPKIXTBSCertificate_SetVersion
 *  NSSPKIXTBSCertificate_GetSerialNumber
 *  NSSPKIXTBSCertificate_SetSerialNumber
 *  NSSPKIXTBSCertificate_GetSignature
 *  NSSPKIXTBSCertificate_SetSignature
 *    { inherit algid gettors? }
 *  NSSPKIXTBSCertificate_GetIssuer
 *  NSSPKIXTBSCertificate_SetIssuer
 *    { inherit "helper" issuer gettors? }
 *  NSSPKIXTBSCertificate_GetValidity
 *  NSSPKIXTBSCertificate_SetValidity
 *    { inherit validity accessors }
 *  NSSPKIXTBSCertificate_GetSubject
 *  NSSPKIXTBSCertificate_SetSubject
 *  NSSPKIXTBSCertificate_GetSubjectPublicKeyInfo
 *  NSSPKIXTBSCertificate_SetSubjectPublicKeyInfo
 *  NSSPKIXTBSCertificate_HasIssuerUniqueID
 *  NSSPKIXTBSCertificate_GetIssuerUniqueID
 *  NSSPKIXTBSCertificate_SetIssuerUniqueID
 *  NSSPKIXTBSCertificate_RemoveIssuerUniqueID
 *  NSSPKIXTBSCertificate_HasSubjectUniqueID
 *  NSSPKIXTBSCertificate_GetSubjectUniqueID
 *  NSSPKIXTBSCertificate_SetSubjectUniqueID
 *  NSSPKIXTBSCertificate_RemoveSubjectUniqueID
 *  NSSPKIXTBSCertificate_HasExtensions
 *  NSSPKIXTBSCertificate_GetExtensions
 *  NSSPKIXTBSCertificate_SetExtensions
 *  NSSPKIXTBSCertificate_RemoveExtensions
 *    { extension accessors }
 *  NSSPKIXTBSCertificate_Equal
 *  NSSPKIXTBSCertificate_Duplicate
 */

/*
 * NSSPKIXTBSCertificate_Decode
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
NSSPKIXTBSCertificate_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTBSCertificate_Create
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
NSSPKIXTBSCertificate_Create
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
 * NSSPKIXTBSCertificate_Destroy
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
NSSPKIXTBSCertificate_Destroy
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * NSSPKIXTBSCertificate_Encode
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
NSSPKIXTBSCertificate_Encode
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_GetVersion
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
NSSPKIXTBSCertificate_GetVersion
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * NSSPKIXTBSCertificate_SetVersion
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
NSSPKIXTBSCertificate_SetVersion
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXVersion version
);

/*
 * NSSPKIXTBSCertificate_GetSerialNumber
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
NSSPKIXTBSCertificate_GetSerialNumber
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXCertificateSerialNumber *snOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_SetSerialNumber
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
NSSPKIXTBSCertificate_SetSerialNumber
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXCertificateSerialNumber *sn
);

/*
 * NSSPKIXTBSCertificate_GetSignature
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
NSSPKIXTBSCertificate_GetSignature
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_SetSignature
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
NSSPKIXTBSCertificate_SetSignature
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXAlgorithmIdentifier *algID
);

/*
 *   { fgmr inherit algid gettors? }
 */

/*
 * NSSPKIXTBSCertificate_GetIssuer
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
NSSPKIXTBSCertificate_GetIssuer
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_SetIssuer
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
NSSPKIXTBSCertificate_SetIssuer
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
 * NSSPKIXTBSCertificate_GetValidity
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
NSSPKIXTBSCertificate_GetValidity
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_SetValidity
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
NSSPKIXTBSCertificate_SetValidity
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXValidity *validity
);

/*
 *   { fgmr inherit validity accessors }
 */

/*
 * NSSPKIXTBSCertificate_GetSubject
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
NSSPKIXTBSCertificate_GetSubject
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_SetSubject
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
NSSPKIXTBSCertificate_SetSubject
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXName *subject
);

/*
 * NSSPKIXTBSCertificate_GetSubjectPublicKeyInfo
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
NSSPKIXTBSCertificate_GetSubjectPublicKeyInfo
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_SetSubjectPublicKeyInfo
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
NSSPKIXTBSCertificate_SetSubjectPublicKeyInfo
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXSubjectPublicKeyInfo *spki
);

/*
 * NSSPKIXTBSCertificate_HasIssuerUniqueID
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
NSSPKIXTBSCertificate_HasIssuerUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTBSCertificate_GetIssuerUniqueID
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
NSSPKIXTBSCertificate_GetIssuerUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXUniqueIdentifier *uidOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_SetIssuerUniqueID
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
NSSPKIXTBSCertificate_SetIssuerUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXUniqueIdentifier *uid
);

/*
 * NSSPKIXTBSCertificate_RemoveIssuerUniqueID
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
NSSPKIXTBSCertificate_RemoveIssuerUniqueID
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * NSSPKIXTBSCertificate_HasSubjectUniqueID
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
NSSPKIXTBSCertificate_HasSubjectUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTBSCertificate_GetSubjectUniqueID
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
NSSPKIXTBSCertificate_GetSubjectUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXUniqueIdentifier *uidOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_SetSubjectUniqueID
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
NSSPKIXTBSCertificate_SetSubjectUniqueID
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXUniqueIdentifier *uid
);

/*
 * NSSPKIXTBSCertificate_RemoveSubjectUniqueID
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
NSSPKIXTBSCertificate_RemoveSubjectUniqueID
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 * NSSPKIXTBSCertificate_HasExtensions
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
NSSPKIXTBSCertificate_HasExtensions
(
  NSSPKIXTBSCertificate *tbsCert,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTBSCertificate_GetExtensions
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
NSSPKIXTBSCertificate_GetExtensions
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertificate_SetExtensions
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
NSSPKIXTBSCertificate_SetExtensions
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSPKIXExtensions *extensions
);

/*
 * NSSPKIXTBSCertificate_RemoveExtensions
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
NSSPKIXTBSCertificate_RemoveExtensions
(
  NSSPKIXTBSCertificate *tbsCert
);

/*
 *   { extension accessors }
 */

/*
 * NSSPKIXTBSCertificate_Equal
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
NSSPKIXTBSCertificate_Equal
(
  NSSPKIXTBSCertificate *one,
  NSSPKIXTBSCertificate *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTBSCertificate_Duplicate
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
NSSPKIXTBSCertificate_Duplicate
(
  NSSPKIXTBSCertificate *tbsCert,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXValidity_Decode
 *  NSSPKIXValidity_Create
 *  NSSPKIXValidity_Encode
 *  NSSPKIXValidity_Destroy
 *  NSSPKIXValidity_GetNotBefore
 *  NSSPKIXValidity_SetNotBefore
 *  NSSPKIXValidity_GetNotAfter
 *  NSSPKIXValidity_SetNotAfter
 *  NSSPKIXValidity_Equal
 *  NSSPKIXValidity_Compare
 *  NSSPKIXValidity_Duplicate
 *
 */

/*
 * NSSPKIXValidity_Decode
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
NSSPKIXValidity_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXValidity_Create
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
NSSPKIXValidity_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTime notBefore,
  NSSPKIXTime notAfter
);

/*
 * NSSPKIXValidity_Destroy
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
NSSPKIXValidity_Destroy
(
  NSSPKIXValidity *validity
);

/*
 * NSSPKIXValidity_Encode
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
NSSPKIXValidity_Encode
(
  NSSPKIXValidity *validity,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXValidity_GetNotBefore
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
NSSPKIXValidity_GetNotBefore
(
  NSSPKIXValidity *validity
);

/*
 * NSSPKIXValidity_SetNotBefore
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
NSSPKIXValidity_SetNotBefore
(
  NSSPKIXValidity *validity,
  NSSPKIXTime notBefore
);

/*
 * NSSPKIXValidity_GetNotAfter
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
NSSPKIXValidity_GetNotAfter
(
  NSSPKIXValidity *validity
);

/*
 * NSSPKIXValidity_SetNotAfter
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
NSSPKIXValidity_SetNotAfter
(
  NSSPKIXValidity *validity,
  NSSPKIXTime notAfter
);

/*
 * NSSPKIXValidity_Equal
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
NSSPKIXValidity_Equal
(
  NSSPKIXValidity *one,
  NSSPKIXValidity *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXValidity_Compare
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
NSSPKIXValidity_Compare
(
  NSSPKIXValidity *one,
  NSSPKIXValidity *two
);

/*
 * NSSPKIXValidity_Duplicate
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
NSSPKIXValidity_Duplicate
(
  NSSPKIXValidity *validity,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXTime_Decode
 *  NSSPKIXTime_CreateFromPRTime
 *  NSSPKIXTime_CreateFromUTF8
 *  NSSPKIXTime_Destroy
 *  NSSPKIXTime_Encode
 *  NSSPKIXTime_GetPRTime
 *  NSSPKIXTime_GetUTF8Encoding
 *  NSSPKIXTime_Equal
 *  NSSPKIXTime_Duplicate
 *  NSSPKIXTime_Compare
 *  
 */

/*
 * NSSPKIXTime_Decode
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
NSSPKIXTime_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTime_CreateFromPRTime
 *
 */

NSS_EXTERN NSSPKIXTime *
NSSPKIXTime_CreateFromPRTime
(
  NSSArena *arenaOpt,
  PRTime prTime
);

/*
 * NSSPKIXTime_CreateFromUTF8
 *
 */

NSS_EXTERN NSSPKIXTime *
NSSPKIXTime_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXTime_Destroy
 *
 */

NSS_EXTERN PR_STATUS
NSSPKIXTime_Destroy
(
  NSSPKIXTime *time
);

/*
 * NSSPKIXTime_Encode
 *
 */

NSS_EXTERN NSSBER *
NSSPKIXTime_Encode
(
  NSSPKIXTime *time,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTime_GetPRTime
 *
 * Returns a zero on error
 */

NSS_EXTERN PRTime
NSSPKIXTime_GetPRTime
(
  NSSPKIXTime *time,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTime_GetUTF8Encoding
 *
 */

NSS_EXTERN NSSUTF8 *
NSSPKXITime_GetUTF8Encoding
(
  NSSPKIXTime *time,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTime_Equal
 *
 */

NSS_EXTERN PRBool
NSSPKXITime_Equal
(
  NSSPKXITime *time1,
  NSSPKXITime *time2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTime_Duplicate
 *
 */

NSS_EXTERN NSSPKIXTime *
NSSPKXITime_Duplicate
(
  NSSPKIXTime *time,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTime_Compare
 *
 * Usual result: -1, 0, 1
 *  Returns 0 on error
 */

NSS_EXTERN PRInt32
NSSPKIXTime_Compare
(
  NSSPKIXTime *time1,
  NSSPKIXTime *tiem2,
  PRStatus *statusOpt
);

/*
 * UniqueIdentifier
 *
 * -- fgmr comments --
 * Should we distinguish bitstrings from "regular" items/BERs?
 * It could be another typedef, but with a separate type to help
 * remind users about the factor of 8.  OR we could make it a
 * hard type, and require the use of some (trivial) converters.
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
 * The public calls for the type:
 *
 *  NSSPKIXSubjectPublicKeyInfo_Decode
 *  NSSPKIXSubjectPublicKeyInfo_Create
 *  NSSPKIXSubjectPublicKeyInfo_Encode
 *  NSSPKIXSubjectPublicKeyInfo_Destroy
 *  NSSPKIXSubjectPublicKeyInfo_GetAlgorithm
 *  NSSPKIXSubjectPublicKeyInfo_SetAlgorithm
 *  NSSPKIXSubjectPublicKeyInfo_GetSubjectPublicKey
 *  NSSPKIXSubjectPublicKeyInfo_SetSubjectPublicKey
 *  NSSPKIXSubjectPublicKeyInfo_Equal
 *  NSSPKIXSubjectPublicKeyInfo_Duplicate
 *
 */

/*
 * NSSPKIXSubjectPublicKeyInfo_Decode
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
NSSPKIXSubjectPublicKeyInfo_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXSubjectPublicKeyInfo_Create
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
NSSPKIXSubjectPublicKeyInfo_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAlgorithmIdentifier *algid,
  NSSItem *subjectPublicKey
);

/*
 * NSSPKIXSubjectPublicKeyInfo_Destroy
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
NSSPKIXSubjectPublicKeyInfo_Destroy
(
  NSSPKIXSubjectPublicKeyInfo *spki
);

/*
 * NSSPKIXSubjectPublicKeyInfo_Encode
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
NSSPKIXSubjectPublicKeyInfo_Encode
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXSubjectPublicKeyInfo_GetAlgorithm
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
NSSPKIXSubjectPublicKeyInfo_GetAlgorithm
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXSubjectPublicKeyInfo_SetAlgorithm
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
NSSPKIXSubjectPublicKeyInfo_SetAlgorithm
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSPKIXAlgorithmIdentifier *algid
);

/*
 * NSSPKIXSubjectPublicKeyInfo_GetSubjectPublicKey
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
NSSPKIXSubjectPublicKeyInfo_GetSubjectPublicKey
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSItem *spkOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXSubjectPublicKeyInfo_SetSubjectPublicKey
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
NSSPKIXSubjectPublicKeyInfo_SetSubjectPublicKey
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSItem *spk
);

/*
 * NSSPKIXSubjectPublicKeyInfo_Equal
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
NSSPKIXSubjectPublicKeyInfo_Equal
(
  NSSPKIXSubjectPublicKeyInfo *one,
  NSSPKIXSubjectPublicKeyInfo *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXSubjectPublicKeyInfo_Duplicate
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
NSSPKIXSubjectPublicKeyInfo_Duplicate
(
  NSSPKIXSubjectPublicKeyInfo *spki,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXCertificateList_Decode
 *  NSSPKIXCertificateList_Create
 *  NSSPKIXCertificateList_Encode
 *  NSSPKIXCertificateList_Destroy
 *  NSSPKIXCertificateList_GetTBSCertList
 *  NSSPKIXCertificateList_SetTBSCertList
 *  NSSPKIXCertificateList_GetSignatureAlgorithm
 *  NSSPKIXCertificateList_SetSignatureAlgorithm
 *  NSSPKIXCertificateList_GetSignature
 *  NSSPKIXCertificateList_SetSignature
 *  NSSPKIXCertificateList_Equal
 *  NSSPKIXCertificateList_Duplicate
 *
 */

/*
 * NSSPKIXCertificateList_Decode
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
NSSPKIXCertificateList_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXCertificateList_Create
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
NSSPKIXCertificateList_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTBSCertList *tbsCertList,
  NSSPKIXAlgorithmIdentifier *sigAlg,
  NSSItem *signature
);

/*
 * NSSPKIXCertificateList_Destroy
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
NSSPKIXCertificateList_Destroy
(
  NSSPKIXCertificateList *certList
);

/*
 * NSSPKIXCertificateList_Encode
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
NSSPKIXCertificateList_Encode
(
  NSSPKIXCertificateList *certList,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificateList_GetTBSCertList
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
NSSPKIXCertificateList_GetTBSCertList
(
  NSSPKIXCertificateList *certList,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificateList_SetTBSCertList
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
NSSPKIXCertificateList_SetTBSCertList
(
  NSSPKIXCertificateList *certList,
  NSSPKIXTBSCertList *tbsCertList
);

/*
 * NSSPKIXCertificateList_GetSignatureAlgorithm
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
NSSPKIXCertificateList_GetSignatureAlgorithm
(
  NSSPKIXCertificateList *certList,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificateList_SetSignatureAlgorithm
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
NSSPKIXCertificateList_SetSignatureAlgorithm
(
  NSSPKIXCertificateList *certList,
  NSSPKIXAlgorithmIdentifier *sigAlg
);

/*
 * NSSPKIXCertificateList_GetSignature
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
NSSPKIXCertificateList_GetSignature
(
  NSSPKIXCertificateList *certList,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificateList_SetSignature
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
NSSPKIXCertificateList_SetSignature
(
  NSSPKIXCertificateList *certList,
  NSSItem *sig
);

/*
 * NSSPKIXCertificateList_Equal
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
NSSPKIXCertificateList_Equal
(
  NSSPKIXCertificateList *one,
  NSSPKIXCertificateList *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXCertificateList_Duplicate
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
NSSPKIXCertificateList_Duplicate
(
  NSSPKIXCertificateList *certList,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXTBSCertList_Decode
 *  NSSPKIXTBSCertList_Create
 *  NSSPKIXTBSCertList_Destroy
 *  NSSPKIXTBSCertList_Encode
 *  NSSPKIXTBSCertList_GetVersion
 *  NSSPKIXTBSCertList_SetVersion
 *  NSSPKIXTBSCertList_GetSignature
 *  NSSPKIXTBSCertList_SetSignature
 *  NSSPKIXTBSCertList_GetIssuer
 *  NSSPKIXTBSCertList_SetIssuer
 *  NSSPKIXTBSCertList_GetThisUpdate
 *  NSSPKIXTBSCertList_SetThisUpdate
 *  NSSPKIXTBSCertList_HasNextUpdate
 *  NSSPKIXTBSCertList_GetNextUpdate
 *  NSSPKIXTBSCertList_SetNextUpdate
 *  NSSPKIXTBSCertList_RemoveNextUpdate
 *  NSSPKIXTBSCertList_GetRevokedCertificates
 *  NSSPKIXTBSCertList_SetRevokedCertificates
 *  NSSPKIXTBSCertList_HasCrlExtensions
 *  NSSPKIXTBSCertList_GetCrlExtensions
 *  NSSPKIXTBSCertList_SetCrlExtensions
 *  NSSPKIXTBSCertList_RemoveCrlExtensions
 *  NSSPKIXTBSCertList_Equal
 *  NSSPKIXTBSCertList_Duplicate
 *
 */

/*
 * NSSPKIXTBSCertList_Decode
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
NSSPKIXTBSCertList_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTBSCertList_Create
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
NSSPKIXTBSCertList_Create
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
 * NSSPKIXTBSCertList_Destroy
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
NSSPKIXTBSCertList_Destroy
(
  NSSPKIXTBSCertList *certList
);

/*
 * NSSPKIXTBSCertList_Encode
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
NSSPKIXTBSCertList_Encode
(
  NSSPKIXTBSCertList *certList,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertList_GetVersion
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
NSSPKIXTBSCertList_GetVersion
(
  NSSPKIXTBSCertList *certList
);

/*
 * NSSPKIXTBSCertList_SetVersion
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
NSSPKIXTBSCertList_SetVersion
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXVersion version
);

/*
 * NSSPKIXTBSCertList_GetSignature
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
NSSPKIXTBSCertList_GetSignature
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertList_SetSignature
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
NSSPKIXTBSCertList_SetSignature
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXAlgorithmIdentifier *algid,
);

/*
 * NSSPKIXTBSCertList_GetIssuer
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
NSSPKIXTBSCertList_GetIssuer
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertList_SetIssuer
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
NSSPKIXTBSCertList_SetIssuer
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXName *issuer
);

/*
 * NSSPKIXTBSCertList_GetThisUpdate
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
NSSPKIXTBSCertList_GetThisUpdate
(
  NSSPKIXTBSCertList *certList
);

/*
 * NSSPKIXTBSCertList_SetThisUpdate
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
NSSPKIXTBSCertList_SetThisUpdate
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXTime thisUpdate
);

/*
 * NSSPKIXTBSCertList_HasNextUpdate
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
NSSPKIXTBSCertList_HasNextUpdate
(
  NSSPKIXTBSCertList *certList,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTBSCertList_GetNextUpdate
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
NSSPKIXTBSCertList_GetNextUpdate
(
  NSSPKIXTBSCertList *certList
);

/*
 * NSSPKIXTBSCertList_SetNextUpdate
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
NSSPKIXTBSCertList_SetNextUpdate
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXTime nextUpdate
);

/*
 * NSSPKIXTBSCertList_RemoveNextUpdate
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
NSSPKIXTBSCertList_RemoveNextUpdate
(
  NSSPKIXTBSCertList *certList
);

/*
 * NSSPKIXTBSCertList_GetRevokedCertificates
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
NSSPKIXTBSCertList_GetRevokedCertificates
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertList_SetRevokedCertificates
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
NSSPKIXTBSCertList_SetRevokedCertificates
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXrevokedCertificates *revoked
);

/*
 * NSSPKIXTBSCertList_HasCrlExtensions
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
NSSPKIXTBSCertList_HasCrlExtensions
(
  NSSPKIXTBSCertList *certList,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTBSCertList_GetCrlExtensions
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
NSSPKIXTBSCertList_GetCrlExtensions
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTBSCertList_SetCrlExtensions
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
NSSPKIXTBSCertList_SetCrlExtensions
(
  NSSPKIXTBSCertList *certList,
  NSSPKIXExtensions *extensions
);

/*
 * NSSPKIXTBSCertList_RemoveCrlExtensions
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
NSSPKIXTBSCertList_RemoveCrlExtensions
(
  NSSPKIXTBSCertList *certList
);

/*
 * NSSPKIXTBSCertList_Equal
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
NSSPKIXTBSCertList_Equal
(
  NSSPKIXTBSCertList *one,
  NSSPKIXTBSCertList *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTBSCertList_Duplicate
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
NSSPKIXTBSCertList_Duplicate
(
  NSSPKIXTBSCertList *certList,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXrevokedCertificates_Decode
 *  NSSPKIXrevokedCertificates_Create
 *  NSSPKIXrevokedCertificates_Encode
 *  NSSPKIXrevokedCertificates_Destroy
 *  NSSPKIXrevokedCertificates_GetRevokedCertificateCount
 *  NSSPKIXrevokedCertificates_GetRevokedCertificates
 *  NSSPKIXrevokedCertificates_SetRevokedCertificates
 *  NSSPKIXrevokedCertificates_GetRevokedCertificate
 *  NSSPKIXrevokedCertificates_SetRevokedCertificate
 *  NSSPKIXrevokedCertificates_InsertRevokedCertificate
 *  NSSPKIXrevokedCertificates_AppendRevokedCertificate
 *  NSSPKIXrevokedCertificates_RemoveRevokedCertificate
 *  NSSPKIXrevokedCertificates_Equal
 *  NSSPKIXrevokedCertificates_Duplicate
 *
 */

/*
 * NSSPKIXrevokedCertificates_Decode
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
NSSPKIXrevokedCertificates_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXrevokedCertificates_Create
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
NSSPKIXrevokedCertificates_Create
(
  NSSArena *arenaOpt,
  NSSPKIXrevokedCertificate *rc1,
  ...
);

/*
 * NSSPKIXrevokedCertificates_Destroy
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
NSSPKIXrevokedCertificates_Destroy
(
  NSSPKIXrevokedCertificates *rcs
);

/*
 * NSSPKIXrevokedCertificates_Encode
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
NSSPKIXrevokedCertificates_Encode
(
  NSSPKIXrevokedCertificates *rcs,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXrevokedCertificates_GetRevokedCertificateCount
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
NSSPKIXrevokedCertificates_GetRevokedCertificateCount
(
  NSSPKIXrevokedCertificates *rcs
);

/*
 * NSSPKIXrevokedCertificates_GetRevokedCertificates
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
NSSPKIXrevokedCertificates_GetRevokedCertificates
(
  NSSPKIXrevokedCertificates *rcs,
  NSSPKIXrevokedCertificate *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXrevokedCertificates_SetRevokedCertificates
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
NSSPKIXrevokedCertificates_SetRevokedCertificates
(
  NSSPKIXrevokedCertificates *rcs,
  NSSPKIXrevokedCertificate *rc[],
  PRInt32 countOpt
);

/*
 * NSSPKIXrevokedCertificates_GetRevokedCertificate
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
NSSPKIXrevokedCertificates_GetRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXrevokedCertificates_SetRevokedCertificate
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
NSSPKIXrevokedCertificates_SetRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i,
  NSSPKIXrevokedCertificate *rc
);

/*
 * NSSPKIXrevokedCertificates_InsertRevokedCertificate
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
NSSPKIXrevokedCertificates_InsertRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i,
  NSSPKIXrevokedCertificate *rc
);

/*
 * NSSPKIXrevokedCertificates_AppendRevokedCertificate
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
NSSPKIXrevokedCertificates_AppendRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i,
  NSSPKIXrevokedCertificate *rc
);

/*
 * NSSPKIXrevokedCertificates_RemoveRevokedCertificate
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
NSSPKIXrevokedCertificates_RemoveRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  PRInt32 i
);

/*
 * NSSPKIXrevokedCertificates_FindRevokedCertificate
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
NSSPKIXrevokedCertificates_FindRevokedCertificate
(
  NSSPKIXrevokedCertificates *rcs,
  NSSPKIXrevokedCertificate *rc
);

/*
 * NSSPKIXrevokedCertificates_Equal
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
NSSPKIXrevokedCertificates_Equal
(
  NSSPKIXrevokedCertificates *one,
  NSSPKIXrevokedCertificates *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXrevokedCertificates_Duplicate
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
NSSPKIXrevokedCertificates_Duplicate
(
  NSSPKIXrevokedCertificates *rcs,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXrevokedCertificate_Decode
 *  NSSPKIXrevokedCertificate_Create
 *  NSSPKIXrevokedCertificate_Encode
 *  NSSPKIXrevokedCertificate_Destroy
 *  NSSPKIXrevokedCertificate_GetUserCertificate
 *  NSSPKIXrevokedCertificate_SetUserCertificate
 *  NSSPKIXrevokedCertificate_GetRevocationDate
 *  NSSPKIXrevokedCertificate_SetRevocationDate
 *  NSSPKIXrevokedCertificate_HasCrlEntryExtensions
 *  NSSPKIXrevokedCertificate_GetCrlEntryExtensions
 *  NSSPKIXrevokedCertificate_SetCrlEntryExtensions
 *  NSSPKIXrevokedCertificate_RemoveCrlEntryExtensions
 *  NSSPKIXrevokedCertificate_Equal
 *  NSSPKIXrevokedCertificate_Duplicate
 *
 */


/*
 * NSSPKIXrevokedCertificate_Decode
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
NSSPKIXrevokedCertificate_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXrevokedCertificate_Create
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
NSSPKIXrevokedCertificate_Create
(
  NSSArena *arenaOpt,
  NSSPKIXCertificateSerialNumber *userCertificate,
  NSSPKIXTime *revocationDate,
  NSSPKIXExtensions *crlEntryExtensionsOpt
);

/*
 * NSSPKIXrevokedCertificate_Destroy
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
NSSPKIXrevokedCertificate_Destroy
(
  NSSPKIXrevokedCertificate *rc
);

/*
 * NSSPKIXrevokedCertificate_Encode
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
NSSPKIXrevokedCertificate_Encode
(
  NSSPKIXrevokedCertificate *rc,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXrevokedCertificate_GetUserCertificate
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
NSSPKIXrevokedCertificate_GetUserCertificate
(
  NSSPKIXrevokedCertificate *rc,
  NSSArena *arenaOpt
);  

/*
 * NSSPKIXrevokedCertificate_SetUserCertificate
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
NSSPKIXrevokedCertificate_SetUserCertificate
(
  NSSPKIXrevokedCertificate *rc,
  NSSPKIXCertificateSerialNumber *csn
);

/*
 * NSSPKIXrevokedCertificate_GetRevocationDate
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
NSSPKIXrevokedCertificate_GetRevocationDate
(
  NSSPKIXrevokedCertificate *rc,
  NSSArena *arenaOpt
);  

/*
 * NSSPKIXrevokedCertificate_SetRevocationDate
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
NSSPKIXrevokedCertificate_SetRevocationDate
(
  NSSPKIXrevokedCertificate *rc,
  NSSPKIXTime *revocationDate
);

/*
 * NSSPKIXrevokedCertificate_HasCrlEntryExtensions
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
NSSPKIXrevokedCertificate_HasCrlEntryExtensions
(
  NSSPKIXrevokedCertificate *rc,
  PRStatus *statusOpt
);

/*
 * NSSPKIXrevokedCertificate_GetCrlEntryExtensions
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
NSSPKIXrevokedCertificate_GetCrlEntryExtensions
(
  NSSPKIXrevokedCertificate *rc,
  NSSArena *arenaOpt
);  

/*
 * NSSPKIXrevokedCertificate_SetCrlEntryExtensions
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
NSSPKIXrevokedCertificate_SetCrlEntryExtensions
(
  NSSPKIXrevokedCertificate *rc,
  NSSPKIXExtensions *crlEntryExtensions
);

/*
 * NSSPKIXrevokedCertificate_RemoveCrlEntryExtensions
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
NSSPKIXrevokedCertificate_RemoveCrlEntryExtensions
(
  NSSPKIXrevokedCertificate *rc
);

/*
 * NSSPKIXrevokedCertificate_Equal
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
NSSPKIXrevokedCertificate_Equal
(
  NSSPKIXrevokedCertificate *one,
  NSSPKIXrevokedCertificate *two,
  PRStatus *statusOpt
);

/*
 * NSSPKIXrevokedCertificate_Duplicate
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
NSSPKIXrevokedCertificate_Duplicate
(
  NSSPKIXrevokedCertificate *rc,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXAlgorithmIdentifier_Decode
 *  NSSPKIXAlgorithmIdentifier_Create
 *  NSSPKIXAlgorithmIdentifier_Encode
 *  NSSPKIXAlgorithmIdentifier_Destroy
 *  NSSPKIXAlgorithmIdentifier_GetAlgorithm
 *  NSSPKIXAlgorithmIdentifier_SetAlgorithm
 *  NSSPKIXAlgorithmIdentifier_GetParameters
 *  NSSPKIXAlgorithmIdentifier_SetParameters
 *  NSSPKIXAlgorithmIdentifier_Compare
 *  NSSPKIXAlgorithmIdentifier_Duplicate
 *    { algorithm-specific parameter types and accessors ? }
 *
 */

/*
 * NSSPKIXAlgorithmIdentifier_Decode
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
NSSPKIXAlgorithmIdentifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXAlgorithmIdentifier_Create
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
NSSPKIXAlgorithmIdentifier_Create
(
  NSSArena *arenaOpt,
  NSSOID *algorithm,
  NSSItem *parameters
);

/*
 * NSSPKIXAlgorithmIdentifier_Destroy
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
NSSPKIXAlgorithmIdentifier_Destroy
(
  NSSPKIXAlgorithmIdentifier *algid
);

/*
 * NSSPKIXAlgorithmIdentifier_Encode
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
NSSPKIXAlgorithmIdentifier_Encode
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAlgorithmIdentifier_GetAlgorithm
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
NSSPKIXAlgorithmIdentifier_GetAlgorithm
(
  NSSPKIXAlgorithmIdentifier *algid
);

/*
 * NSSPKIXAlgorithmIdentifier_SetAlgorithm
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
NSSPKIXAlgorithmIdentifier_SetAlgorithm
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSOID *algorithm
);

/*
 * NSSPKIXAlgorithmIdentifier_GetParameters
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
NSSPKIXAlgorithmIdentifier_GetParameters
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAlgorithmIdentifier_SetParameters
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
NSSPKIXAlgorithmIdentifier_SetParameters
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSItem *parameters
);

/*
 * NSSPKIXAlgorithmIdentifier_Equal
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
NSSPKIXAlgorithmIdentifier_Equal
(
  NSSPKIXAlgorithmIdentifier *algid1,
  NSSPKIXAlgorithmIdentifier *algid2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXAlgorithmIdentifier_Duplicate
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
NSSPKIXAlgorithmIdentifier_Duplicate
(
  NSSPKIXAlgorithmIdentifier *algid,
  NSSArena *arenaOpt
);

/*
 *   { algorithm-specific parameter types and accessors ? }
 */

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
 * The public calls for this type:
 *
 *  NSSPKIXORAddress_Decode
 *  NSSPKIXORAddress_Create
 *  NSSPKIXORAddress_Destroy
 *  NSSPKIXORAddress_Encode
 *  NSSPKIXORAddress_GetBuiltInStandardAttributes
 *  NSSPKIXORAddress_SetBuiltInStandardAttributes
 *  NSSPKIXORAddress_HasBuiltInDomainDefinedAttributes
 *  NSSPKIXORAddress_GetBuiltInDomainDefinedAttributes
 *  NSSPKIXORAddress_SetBuiltInDomainDefinedAttributes
 *  NSSPKIXORAddress_RemoveBuiltInDomainDefinedAttributes
 *  NSSPKIXORAddress_HasExtensionsAttributes
 *  NSSPKIXORAddress_GetExtensionsAttributes
 *  NSSPKIXORAddress_SetExtensionsAttributes
 *  NSSPKIXORAddress_RemoveExtensionsAttributes
 *  NSSPKIXORAddress_Equal
 *  NSSPKIXORAddress_Duplicate
 *
 */

/*
 * NSSPKIXORAddress_Decode
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
NSSPKIXORAddress_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXORAddress_Create
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
NSSPKIXORAddress_Create
(
  NSSArena *arenaOpt,
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXBuiltInDomainDefinedAttributes *biddaOpt,
  NSSPKIXExtensionAttributes *eaOpt
);

/*
 * NSSPKIXORAddress_Destroy
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
NSSPKIXORAddress_Destroy
(
  NSSPKIXORAddress *ora
);

/*
 * NSSPKIXORAddress_Encode
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
NSSPKIXORAddress_Encode
(
  NSSPKIXORAddress *ora,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXORAddress_GetBuiltInStandardAttributes
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
NSSPKIXORAddress_GetBuiltInStandardAttributes
(
  NSSPKIXORAddress *ora,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXORAddress_SetBuiltInStandardAttributes
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
NSSPKIXORAddress_SetBuiltInStandardAttributes
(
  NSSPKIXORAddress *ora,
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXORAddress_HasBuiltInDomainDefinedAttributes
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
NSSPKIXORAddress_HasBuiltInDomainDefinedAttributes
(
  NSSPKIXORAddress *ora,
  PRStatus *statusOpt
);

/*
 * NSSPKIXORAddress_GetBuiltInDomainDefinedAttributes
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
NSSPKIXORAddress_GetBuiltInDomainDefinedAttributes
(
  NSSPKIXORAddress *ora,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXORAddress_SetBuiltInDomainDefinedAttributes
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
NSSPKIXORAddress_SetBuiltInDomainDefinedAttributes
(
  NSSPKIXORAddress *ora,
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXORAddress_RemoveBuiltInDomainDefinedAttributes
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
NSSPKIXORAddress_RemoveBuiltInDomainDefinedAttributes
(
  NSSPKIXORAddress *ora
);

/*
 * NSSPKIXORAddress_HasExtensionsAttributes
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
NSSPKIXORAddress_HasExtensionsAttributes
(
  NSSPKIXORAddress *ora,
  PRStatus *statusOpt
);

/*
 * NSSPKIXORAddress_GetExtensionsAttributes
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
NSSPKIXORAddress_GetExtensionsAttributes
(
  NSSPKIXORAddress *ora,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXORAddress_SetExtensionsAttributes
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
NSSPKIXORAddress_SetExtensionsAttributes
(
  NSSPKIXORAddress *ora,
  NSSPKIXExtensionAttributes *eaOpt
);

/*
 * NSSPKIXORAddress_RemoveExtensionsAttributes
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
NSSPKIXORAddress_RemoveExtensionsAttributes
(
  NSSPKIXORAddress *ora
);

/*
 * NSSPKIXORAddress_Equal
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
NSSPKIXORAddress_Equal
(
  NSSPKIXORAddress *ora1,
  NSSPKIXORAddress *ora2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXORAddress_Duplicate
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
NSSPKIXORAddress_Duplicate
(
  NSSPKIXORAddress *ora,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXBuiltInStandardAttributes_Decode
 *  NSSPKIXBuiltInStandardAttributes_Create
 *  NSSPKIXBuiltInStandardAttributes_Destroy
 *  NSSPKIXBuiltInStandardAttributes_Encode
 *  NSSPKIXBuiltInStandardAttributes_HasCountryName
 *  NSSPKIXBuiltInStandardAttributes_GetCountryName
 *  NSSPKIXBuiltInStandardAttributes_SetCountryName
 *  NSSPKIXBuiltInStandardAttributes_RemoveCountryName
 *  NSSPKIXBuiltInStandardAttributes_HasAdministrationDomainName
 *  NSSPKIXBuiltInStandardAttributes_GetAdministrationDomainName
 *  NSSPKIXBuiltInStandardAttributes_SetAdministrationDomainName
 *  NSSPKIXBuiltInStandardAttributes_RemoveAdministrationDomainName
 *  NSSPKIXBuiltInStandardAttributes_HasNetworkAddress
 *  NSSPKIXBuiltInStandardAttributes_GetNetworkAddress
 *  NSSPKIXBuiltInStandardAttributes_SetNetworkAddress
 *  NSSPKIXBuiltInStandardAttributes_RemoveNetworkAddress
 *  NSSPKIXBuiltInStandardAttributes_HasTerminalIdentifier
 *  NSSPKIXBuiltInStandardAttributes_GetTerminalIdentifier
 *  NSSPKIXBuiltInStandardAttributes_SetTerminalIdentifier
 *  NSSPKIXBuiltInStandardAttributes_RemoveTerminalIdentifier
 *  NSSPKIXBuiltInStandardAttributes_HasPrivateDomainName
 *  NSSPKIXBuiltInStandardAttributes_GetPrivateDomainName
 *  NSSPKIXBuiltInStandardAttributes_SetPrivateDomainName
 *  NSSPKIXBuiltInStandardAttributes_RemovePrivateDomainName
 *  NSSPKIXBuiltInStandardAttributes_HasOrganizationName
 *  NSSPKIXBuiltInStandardAttributes_GetOrganizationName
 *  NSSPKIXBuiltInStandardAttributes_SetOrganizationName
 *  NSSPKIXBuiltInStandardAttributes_RemoveOrganizationName
 *  NSSPKIXBuiltInStandardAttributes_HasNumericUserIdentifier
 *  NSSPKIXBuiltInStandardAttributes_GetNumericUserIdentifier
 *  NSSPKIXBuiltInStandardAttributes_SetNumericUserIdentifier
 *  NSSPKIXBuiltInStandardAttributes_RemoveNumericUserIdentifier
 *  NSSPKIXBuiltInStandardAttributes_HasPersonalName
 *  NSSPKIXBuiltInStandardAttributes_GetPersonalName
 *  NSSPKIXBuiltInStandardAttributes_SetPersonalName
 *  NSSPKIXBuiltInStandardAttributes_RemovePersonalName
 *  NSSPKIXBuiltInStandardAttributes_HasOrganizationLUnitNames
 *  NSSPKIXBuiltInStandardAttributes_GetOrganizationLUnitNames
 *  NSSPKIXBuiltInStandardAttributes_SetOrganizationLUnitNames
 *  NSSPKIXBuiltInStandardAttributes_RemoveOrganizationLUnitNames
 *  NSSPKIXBuiltInStandardAttributes_Equal
 *  NSSPKIXBuiltInStandardAttributes_Duplicate
 *
 */

/*
 * NSSPKIXBuiltInStandardAttributes_Decode
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
NSSPKIXBuiltInStandardAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXBuiltInStandardAttributes_Create
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
NSSPKIXBuiltInStandardAttributes_Create
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
 * NSSPKIXBuiltInStandardAttributes_Destroy
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
NSSPKIXBuiltInStandardAttributes_Destroy
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_Encode
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
NSSPKIXBuiltInStandardAttributes_Encode
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_HasCountryName
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
NSSPKIXBuiltInStandardAttributes_HasCountryName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_GetCountryName
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
NSSPKIXBuiltInStandardAttributes_GetCountryName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_SetCountryName
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
NSSPKIXBuiltInStandardAttributes_SetCountryName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXCountryName *countryName
);

/*
 * NSSPKIXBuiltInStandardAttributes_RemoveCountryName
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
NSSPKIXBuiltInStandardAttributes_RemoveCountryName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_HasAdministrationDomainName
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
NSSPKIXBuiltInStandardAttributes_HasAdministrationDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_GetAdministrationDomainName
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
NSSPKIXBuiltInStandardAttributes_GetAdministrationDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_SetAdministrationDomainName
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
NSSPKIXBuiltInStandardAttributes_SetAdministrationDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXAdministrationDomainName *administrationDomainName
);

/*
 * NSSPKIXBuiltInStandardAttributes_RemoveAdministrationDomainName
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
NSSPKIXBuiltInStandardAttributes_RemoveAdministrationDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_HasNetworkAddress
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
NSSPKIXBuiltInStandardAttributes_HasNetworkAddress
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_GetNetworkAddress
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
NSSPKIXBuiltInStandardAttributes_GetNetworkAddress
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_SetNetworkAddress
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
NSSPKIXBuiltInStandardAttributes_SetNetworkAddress
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXNetworkAddress *networkAddress
);

/*
 * NSSPKIXBuiltInStandardAttributes_RemoveNetworkAddress
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
NSSPKIXBuiltInStandardAttributes_RemoveNetworkAddress
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_HasTerminalIdentifier
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
NSSPKIXBuiltInStandardAttributes_HasTerminalIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_GetTerminalIdentifier
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
NSSPKIXBuiltInStandardAttributes_GetTerminalIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_SetTerminalIdentifier
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
NSSPKIXBuiltInStandardAttributes_SetTerminalIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXTerminalIdentifier *terminalIdentifier
);

/*
 * NSSPKIXBuiltInStandardAttributes_RemoveTerminalIdentifier
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
NSSPKIXBuiltInStandardAttributes_RemoveTerminalIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_HasPrivateDomainName
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
NSSPKIXBuiltInStandardAttributes_HasPrivateDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_GetPrivateDomainName
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
NSSPKIXBuiltInStandardAttributes_GetPrivateDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_SetPrivateDomainName
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
NSSPKIXBuiltInStandardAttributes_SetPrivateDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXPrivateDomainName *privateDomainName
);

/*
 * NSSPKIXBuiltInStandardAttributes_RemovePrivateDomainName
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
NSSPKIXBuiltInStandardAttributes_RemovePrivateDomainName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_HasOrganizationName
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
NSSPKIXBuiltInStandardAttributes_HasOrganizationName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_GetOrganizationName
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
NSSPKIXBuiltInStandardAttributes_GetOrganizationName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_SetOrganizationName
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
NSSPKIXBuiltInStandardAttributes_SetOrganizationName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXOrganizationName *organizationName
);

/*
 * NSSPKIXBuiltInStandardAttributes_RemoveOrganizationName
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
NSSPKIXBuiltInStandardAttributes_RemoveOrganizationName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_HasNumericUserIdentifier
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
NSSPKIXBuiltInStandardAttributes_HasNumericUserIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_GetNumericUserIdentifier
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
NSSPKIXBuiltInStandardAttributes_GetNumericUserIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_SetNumericUserIdentifier
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
NSSPKIXBuiltInStandardAttributes_SetNumericUserIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXNumericUserIdentifier *numericUserIdentifier
);

/*
 * NSSPKIXBuiltInStandardAttributes_RemoveNumericUserIdentifier
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
NSSPKIXBuiltInStandardAttributes_RemoveNumericUserIdentifier
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_HasPersonalName
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
NSSPKIXBuiltInStandardAttributes_HasPersonalName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_GetPersonalName
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
NSSPKIXBuiltInStandardAttributes_GetPersonalName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_SetPersonalName
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
NSSPKIXBuiltInStandardAttributes_SetPersonalName
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXPersonalName *personalName
);

/*
 * NSSPKIXBuiltInStandardAttributes_RemovePersonalName
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
NSSPKIXBuiltInStandardAttributes_RemovePersonalName
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_HasOrganizationLUnitNames
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
NSSPKIXBuiltInStandardAttributes_HasOrganizationLUnitNames
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_GetOrganizationLUnitNames
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
NSSPKIXBuiltInStandardAttributes_GetOrganizationLUnitNames
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_SetOrganizationLUnitNames
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
NSSPKIXBuiltInStandardAttributes_SetOrganizationLUnitNames
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSPKIXOrganizationalUnitNames *organizationalUnitNames
);

/*
 * NSSPKIXBuiltInStandardAttributes_RemoveOrganizationLUnitNames
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
NSSPKIXBuiltInStandardAttributes_RemoveOrganizationLUnitNames
(
  NSSPKIXBuiltInStandardAttributes *bisa
);

/*
 * NSSPKIXBuiltInStandardAttributes_Equal
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
NSSPKIXBuiltInStandardAttributes_Equal
(
  NSSPKIXBuiltInStandardAttributes *bisa1,
  NSSPKIXBuiltInStandardAttributes *bisa2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInStandardAttributes_Duplicate
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
NSSPKIXBuiltInStandardAttributes_Duplicate
(
  NSSPKIXBuiltInStandardAttributes *bisa,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXCountryName_Decode
 *  NSSPKIXCountryName_CreateFromUTF8
 *  NSSPKIXCountryName_Encode
 *
 */

/*
 * NSSPKIXCountryName_Decode
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
NSSPKIXCountryName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXCountryName_CreateFromUTF8
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
NSSPKIXCountryName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXCountryName_Encode
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
NSSPKIXCountryName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXAdministrationDomainName_Decode
 *  NSSPKIXAdministrationDomainName_CreateFromUTF8
 *  NSSPKIXAdministrationDomainName_Encode
 *
 */

/*
 * NSSPKIXAdministrationDomainName_Decode
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
NSSPKIXAdministrationDomainName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXAdministrationDomainName_CreateFromUTF8
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
NSSPKIXAdministrationDomainName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXAdministrationDomainName_Encode
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
NSSPKIXAdministrationDomainName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXX121Address_Decode
 *  NSSPKIXX121Address_CreateFromUTF8
 *  NSSPKIXX121Address_Encode
 *
 */

/*
 * NSSPKIXX121Address_Decode
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
NSSPKIXX121Address_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXX121Address_CreateFromUTF8
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
NSSPKIXX121Address_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXX121Address_Encode
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
NSSPKIXX121Address_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXNetworkAddress_Decode
 *  NSSPKIXNetworkAddress_CreateFromUTF8
 *  NSSPKIXNetworkAddress_Encode
 *
 */

/*
 * NSSPKIXNetworkAddress_Decode
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
NSSPKIXNetworkAddress_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXNetworkAddress_CreateFromUTF8
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
NSSPKIXNetworkAddress_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXNetworkAddress_Encode
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
NSSPKIXNetworkAddress_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXTerminalIdentifier_Decode
 *  NSSPKIXTerminalIdentifier_CreateFromUTF8
 *  NSSPKIXTerminalIdentifier_Encode
 *
 */

/*
 * NSSPKIXTerminalIdentifier_Decode
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
NSSPKIXTerminalIdentifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTerminalIdentifier_CreateFromUTF8
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
NSSPKIXTerminalIdentifier_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXTerminalIdentifier_Encode
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
NSSPKIXTerminalIdentifier_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXPrivateDomainName_Decode
 *  NSSPKIXPrivateDomainName_CreateFromUTF8
 *  NSSPKIXPrivateDomainName_Encode
 *
 */

/*
 * NSSPKIXPrivateDomainName_Decode
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
NSSPKIXPrivateDomainName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPrivateDomainName_CreateFromUTF8
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
NSSPKIXPrivateDomainName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXPrivateDomainName_Encode
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
NSSPKIXPrivateDomainName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXOrganizationName_Decode
 *  NSSPKIXOrganizationName_CreateFromUTF8
 *  NSSPKIXOrganizationName_Encode
 *
 */

/*
 * NSSPKIXOrganizationName_Decode
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
NSSPKIXOrganizationName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXOrganizationName_CreateFromUTF8
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
NSSPKIXOrganizationName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXOrganizationName_Encode
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
NSSPKIXOrganizationName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXNumericUserIdentifier_Decode
 *  NSSPKIXNumericUserIdentifier_CreateFromUTF8
 *  NSSPKIXNumericUserIdentifier_Encode
 *
 */

/*
 * NSSPKIXNumericUserIdentifier_Decode
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
NSSPKIXNumericUserIdentifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXNumericUserIdentifier_CreateFromUTF8
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
NSSPKIXNumericUserIdentifier_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXNumericUserIdentifier_Encode
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
NSSPKIXNumericUserIdentifier_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXPersonalName_Decode
 *  NSSPKIXPersonalName_Create
 *  NSSPKIXPersonalName_Destroy
 *  NSSPKIXPersonalName_Encode
 *  NSSPKIXPersonalName_GetSurname
 *  NSSPKIXPersonalName_SetSurname
 *  NSSPKIXPersonalName_HasGivenName
 *  NSSPKIXPersonalName_GetGivenName
 *  NSSPKIXPersonalName_SetGivenName
 *  NSSPKIXPersonalName_RemoveGivenName
 *  NSSPKIXPersonalName_HasInitials
 *  NSSPKIXPersonalName_GetInitials
 *  NSSPKIXPersonalName_SetInitials
 *  NSSPKIXPersonalName_RemoveInitials
 *  NSSPKIXPersonalName_HasGenerationQualifier
 *  NSSPKIXPersonalName_GetGenerationQualifier
 *  NSSPKIXPersonalName_SetGenerationQualifier
 *  NSSPKIXPersonalName_RemoveGenerationQualifier
 *  NSSPKIXPersonalName_Equal
 *  NSSPKIXPersonalName_Duplicate
 *
 */

/*
 * NSSPKIXPersonalName_Decode
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
NSSPKIXPersonalName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPersonalName_Create
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
NSSPKIXPersonalName_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *surname,
  NSSUTF8 *givenNameOpt,
  NSSUTF8 *initialsOpt,
  NSSUTF8 *generationQualifierOpt
);

/*
 * NSSPKIXPersonalName_Destroy
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
NSSPKIXPersonalName_Destroy
(
  NSSPKIXPersonalName *personalName
);

/*
 * NSSPKIXPersonalName_Encode
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
NSSPKIXPersonalName_Encode
(
  NSSPKIXPersonalName *personalName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPersonalName_GetSurname
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
NSSPKIXPersonalName_GetSurname
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPersonalName_SetSurname
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
NSSPKIXPersonalName_SetSurname
(
  NSSPKIXPersonalName *personalName,
  NSSUTF8 *surname
);

/*
 * NSSPKIXPersonalName_HasGivenName
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
NSSPKIXPersonalName_HasGivenName
(
  NSSPKIXPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPersonalName_GetGivenName
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
NSSPKIXPersonalName_GetGivenName
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPersonalName_SetGivenName
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
NSSPKIXPersonalName_SetGivenName
(
  NSSPKIXPersonalName *personalName,
  NSSUTF8 *givenName
);

/*
 * NSSPKIXPersonalName_RemoveGivenName
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
NSSPKIXPersonalName_RemoveGivenName
(
  NSSPKIXPersonalName *personalName
);

/*
 * NSSPKIXPersonalName_HasInitials
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
NSSPKIXPersonalName_HasInitials
(
  NSSPKIXPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPersonalName_GetInitials
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
NSSPKIXPersonalName_GetInitials
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPersonalName_SetInitials
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
NSSPKIXPersonalName_SetInitials
(
  NSSPKIXPersonalName *personalName,
  NSSUTF8 *initials
);

/*
 * NSSPKIXPersonalName_RemoveInitials
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
NSSPKIXPersonalName_RemoveInitials
(
  NSSPKIXPersonalName *personalName
);

/*
 * NSSPKIXPersonalName_HasGenerationQualifier
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
NSSPKIXPersonalName_HasGenerationQualifier
(
  NSSPKIXPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPersonalName_GetGenerationQualifier
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
NSSPKIXPersonalName_GetGenerationQualifier
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPersonalName_SetGenerationQualifier
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
NSSPKIXPersonalName_SetGenerationQualifier
(
  NSSPKIXPersonalName *personalName,
  NSSUTF8 *generationQualifier
);

/*
 * NSSPKIXPersonalName_RemoveGenerationQualifier
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
NSSPKIXPersonalName_RemoveGenerationQualifier
(
  NSSPKIXPersonalName *personalName
);

/*
 * NSSPKIXPersonalName_Equal
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
NSSPKIXPersonalName_Equal
(
  NSSPKIXPersonalName *personalName1,
  NSSPKIXPersonalName *personalName2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPersonalName_Duplicate
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
NSSPKIXPersonalName_Duplicate
(
  NSSPKIXPersonalName *personalName,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXOrganizationalUnitNames_Decode
 *  NSSPKIXOrganizationalUnitNames_Create
 *  NSSPKIXOrganizationalUnitNames_Destroy
 *  NSSPKIXOrganizationalUnitNames_Encode
 *  NSSPKIXOrganizationalUnitNames_GetOrganizationalUnitNameCount
 *  NSSPKIXOrganizationalUnitNames_GetOrganizationalUnitNames
 *  NSSPKIXOrganizationalUnitNames_SetOrganizationalUnitNames
 *  NSSPKIXOrganizationalUnitNames_GetOrganizationalUnitName
 *  NSSPKIXOrganizationalUnitNames_SetOrganizationalUnitName
 *  NSSPKIXOrganizationalUnitNames_InsertOrganizationalUnitName
 *  NSSPKIXOrganizationalUnitNames_AppendOrganizationalUnitName
 *  NSSPKIXOrganizationalUnitNames_RemoveOrganizationalUnitName
 *  NSSPKIXOrganizationalUnitNames_FindOrganizationalUnitName
 *  NSSPKIXOrganizationalUnitNames_Equal
 *  NSSPKIXOrganizationalUnitNames_Duplicate
 *
 */

/*
 * NSSPKIXOrganizationalUnitNames_Decode
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
NSSPKIXOrganizationalUnitNames_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXOrganizationalUnitNames_Create
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
NSSPKIXOrganizationalUnitNames_Create
(
  NSSArena *arenaOpt,
  NSSPKIXOrganizationalUnitName *ou1,
  ...
);

/*
 * NSSPKIXOrganizationalUnitNames_Destroy
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
NSSPKIXOrganizationalUnitNames_Destroy
(
  NSSPKIXOrganizationalUnitNames *ous
);

/*
 * NSSPKIXOrganizationalUnitNames_Encode
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
NSSPKIXOrganizationalUnitNames_Encode
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXOrganizationalUnitNames_GetOrganizationalUnitNameCount
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
NSSPKIXOrganizationalUnitNames_GetOrganizationalUnitNameCount
(
  NSSPKIXOrganizationalUnitNames *ous
);

/*
 * NSSPKIXOrganizationalUnitNames_GetOrganizationalUnitNames
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
NSSPKIXOrganizationalUnitNames_GetOrganizationalUnitNames
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSPKIXOrganizationalUnitName *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXOrganizationalUnitNames_SetOrganizationalUnitNames
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
NSSPKIXOrganizationalUnitNames_SetOrganizationalUnitNames
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSPKIXOrganizationalUnitName *ou[],
  PRInt32 count
);

/*
 * NSSPKIXOrganizationalUnitNames_GetOrganizationalUnitName
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
NSSPKIXOrganizationalUnitNames_GetOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXOrganizationalUnitNames_SetOrganizationalUnitName
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
NSSPKIXOrganizationalUnitNames_SetOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSPKIXOrganizationalUnitName *ou
);

/*
 * NSSPKIXOrganizationalUnitNames_InsertOrganizationalUnitName
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
NSSPKIXOrganizationalUnitNames_InsertOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSPKIXOrganizationalUnitName *ou
);

/*
 * NSSPKIXOrganizationalUnitNames_AppendOrganizationalUnitName
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
NSSPKIXOrganizationalUnitNames_AppendOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSPKIXOrganizationalUnitName *ou
);

/*
 * NSSPKIXOrganizationalUnitNames_RemoveOrganizationalUnitName
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
NSSPKIXOrganizationalUnitNames_RemoveOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  PRInt32 i
);

/*
 * NSSPKIXOrganizationalUnitNames_FindOrganizationalUnitName
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
NSSPKIXOrganizationalUnitNames_FindOrganizationalUnitName
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSPKIXOrganizationalUnitName *ou
);

/*
 * NSSPKIXOrganizationalUnitNames_Equal
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
NSSPKIXOrganizationalUnitNames_Equal
(
  NSSPKIXOrganizationalUnitNames *ous1,
  NSSPKIXOrganizationalUnitNames *ous2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXOrganizationalUnitNames_Duplicate
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
NSSPKIXOrganizationalUnitNames_Duplicate
(
  NSSPKIXOrganizationalUnitNames *ous,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXOrganizationalUnitName_Decode
 *  NSSPKIXOrganizationalUnitName_CreateFromUTF8
 *  NSSPKIXOrganizationalUnitName_Encode
 *
 */

/*
 * NSSPKIXOrganizationalUnitName_Decode
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
NSSPKIXOrganizationalUnitName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXOrganizationalUnitName_CreateFromUTF8
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
NSSPKIXOrganizationalUnitName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXOrganizationalUnitName_Encode
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
NSSPKIXOrganizationalUnitName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXBuiltInDomainDefinedAttributes_Decode
 *  NSSPKIXBuiltInDomainDefinedAttributes_Create
 *  NSSPKIXBuiltInDomainDefinedAttributes_Destroy
 *  NSSPKIXBuiltInDomainDefinedAttributes_Encode
 *  NSSPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributeCount
 *  NSSPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributes
 *  NSSPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttributes
 *  NSSPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttribute
 *  NSSPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttribute
 *  NSSPKIXBuiltInDomainDefinedAttributes_InsertBuiltIndomainDefinedAttribute
 *  NSSPKIXBuiltInDomainDefinedAttributes_AppendBuiltIndomainDefinedAttribute
 *  NSSPKIXBuiltInDomainDefinedAttributes_RemoveBuiltIndomainDefinedAttribute
 *  NSSPKIXBuiltInDomainDefinedAttributes_Equal
 *  NSSPKIXBuiltInDomainDefinedAttributes_Duplicate
 *
 */

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_Decode
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
NSSPKIXBuiltInDomainDefinedAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_Create
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
NSSPKIXBuiltInDomainDefinedAttributes_Create
(
  NSSArena *arenaOpt,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda1,
  ...
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_Destroy
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
NSSPKIXBuiltInDomainDefinedAttributes_Destroy
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_Encode
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
NSSPKIXBuiltInDomainDefinedAttributes_Encode
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributeCount
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
NSSPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributeCount
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributes
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
NSSPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttributes
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSPKIXBuiltInDomainDefinedAttribut *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttributes
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
NSSPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttributes
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSPKIXBuiltInDomainDefinedAttribut *bidda[],
  PRInt32 count
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttribute
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
NSSPKIXBuiltInDomainDefinedAttributes_GetBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttribute
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
NSSPKIXBuiltInDomainDefinedAttributes_SetBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  PRInt32 i,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_InsertBuiltIndomainDefinedAttribute
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
NSSPKIXBuiltInDomainDefinedAttributes_InsertBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  PRInt32 i,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_AppendBuiltIndomainDefinedAttribute
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
NSSPKIXBuiltInDomainDefinedAttributes_AppendBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_RemoveBuiltIndomainDefinedAttribute
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
NSSPKIXBuiltInDomainDefinedAttributes_RemoveBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  PRInt32 i
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_FindBuiltIndomainDefinedAttribute
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
NSSPKIXBuiltInDomainDefinedAttributes_FindBuiltIndomainDefinedAttribute
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_Equal
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
NSSPKIXBuiltInDomainDefinedAttributes_Equal
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas1,
  NSSPKIXBuiltInDomainDefinedAttributes *biddas2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInDomainDefinedAttributes_Duplicate
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
NSSPKIXBuiltInDomainDefinedAttributes_Duplicate
(
  NSSPKIXBuiltInDomainDefinedAttributes *biddas,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXBuiltInDomainDefinedAttribute_Decode
 *  NSSPKIXBuiltInDomainDefinedAttribute_Create
 *  NSSPKIXBuiltInDomainDefinedAttribute_Destroy
 *  NSSPKIXBuiltInDomainDefinedAttribute_Encode
 *  NSSPKIXBuiltInDomainDefinedAttribute_GetType
 *  NSSPKIXBuiltInDomainDefinedAttribute_SetType
 *  NSSPKIXBuiltInDomainDefinedAttribute_GetValue
 *  NSSPKIXBuiltInDomainDefinedAttribute_SetValue
 *  NSSPKIXBuiltInDomainDefinedAttribute_Equal
 *  NSSPKIXBuiltInDomainDefinedAttribute_Duplicate
 *
 */

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_Decode
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
NSSPKIXBuiltInDomainDefinedAttribute_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_Create
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
NSSPKIXBuiltInDomainDefinedAttribute_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *type,
  NSSUTF8 *value
);

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_Destroy
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
NSSPKIXBuiltInDomainDefinedAttribute_Destroy
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda
);

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_Encode
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
NSSPKIXBuiltInDomainDefinedAttribute_Encode
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_GetType
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
NSSPKIXBuiltInDomainDefinedAttribute_GetType
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_SetType
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
NSSPKIXBuiltInDomainDefinedAttribute_SetType
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSUTF8 *type
);

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_GetValue
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
NSSPKIXBuiltInDomainDefinedAttribute_GetValue
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_SetValue
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
NSSPKIXBuiltInDomainDefinedAttribute_SetValue
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSUTF8 *value
);

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_Equal
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
NSSPKIXBuiltInDomainDefinedAttribute_Equal
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda1,
  NSSPKIXBuiltInDomainDefinedAttribute *bidda2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBuiltInDomainDefinedAttribute_Duplicate
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
NSSPKIXBuiltInDomainDefinedAttribute_Duplicate
(
  NSSPKIXBuiltInDomainDefinedAttribute *bidda,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXExtensionAttributes_Decode
 *  NSSPKIXExtensionAttributes_Create
 *  NSSPKIXExtensionAttributes_Destroy
 *  NSSPKIXExtensionAttributes_Encode
 *  NSSPKIXExtensionAttributes_GetExtensionAttributeCount
 *  NSSPKIXExtensionAttributes_GetExtensionAttributes
 *  NSSPKIXExtensionAttributes_SetExtensionAttributes
 *  NSSPKIXExtensionAttributes_GetExtensionAttribute
 *  NSSPKIXExtensionAttributes_SetExtensionAttribute
 *  NSSPKIXExtensionAttributes_InsertExtensionAttribute
 *  NSSPKIXExtensionAttributes_AppendExtensionAttribute
 *  NSSPKIXExtensionAttributes_RemoveExtensionAttribute
 *  NSSPKIXExtensionAttributes_FindExtensionAttribute
 *  NSSPKIXExtensionAttributes_Equal
 *  NSSPKIXExtensionAttributes_Duplicate
 *
 */

/*
 * NSSPKIXExtensionAttributes_Decode
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
NSSPKIXExtensionAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXExtensionAttributes_Create
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
NSSPKIXExtensionAttributes_Create
(
  NSSArena *arenaOpt,
  NSSPKIXExtensionAttribute ea1,
  ...
);

/*
 * NSSPKIXExtensionAttributes_Destroy
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
NSSPKIXExtensionAttributes_Destroy
(
  NSSPKIXExtensionAttributes *eas
);

/*
 * NSSPKIXExtensionAttributes_Encode
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
NSSPKIXExtensionAttributes_Encode
(
  NSSPKIXExtensionAttributes *eas
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtensionAttributes_GetExtensionAttributeCount
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
NSSPKIXExtensionAttributes_GetExtensionAttributeCount
(
  NSSPKIXExtensionAttributes *eas
);

/*
 * NSSPKIXExtensionAttributes_GetExtensionAttributes
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
NSSPKIXExtensionAttributes_GetExtensionAttributes
(
  NSSPKIXExtensionAttributes *eas,
  NSSPKIXExtensionAttribute *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtensionAttributes_SetExtensionAttributes
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
NSSPKIXExtensionAttributes_SetExtensionAttributes
(
  NSSPKIXExtensionAttributes *eas,
  NSSPKIXExtensionAttribute *ea[],
  PRInt32 count
);

/*
 * NSSPKIXExtensionAttributes_GetExtensionAttribute
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
NSSPKIXExtensionAttributes_GetExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtensionAttributes_SetExtensionAttribute
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
NSSPKIXExtensionAttributes_SetExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  PRInt32 i,
  NSSPKIXExtensionAttribute *ea
);

/*
 * NSSPKIXExtensionAttributes_InsertExtensionAttribute
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
NSSPKIXExtensionAttributes_InsertExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  PRInt32 i,
  NSSPKIXExtensionAttribute *ea
);

/*
 * NSSPKIXExtensionAttributes_AppendExtensionAttribute
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
NSSPKIXExtensionAttributes_AppendExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  NSSPKIXExtensionAttribute *ea
);

/*
 * NSSPKIXExtensionAttributes_RemoveExtensionAttribute
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
NSSPKIXExtensionAttributes_RemoveExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  PRInt32 i,
);

/*
 * NSSPKIXExtensionAttributes_FindExtensionAttribute
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
NSSPKIXExtensionAttributes_FindExtensionAttribute
(
  NSSPKIXExtensionAttributes *eas,
  NSSPKIXExtensionAttribute *ea
);

/*
 * NSSPKIXExtensionAttributes_Equal
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
NSSPKIXExtensionAttributes_Equal
(
  NSSPKIXExtensionAttributes *eas1,
  NSSPKIXExtensionAttributes *eas2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXExtensionAttributes_Duplicate
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
NSSPKIXExtensionAttributes_Duplicate
(
  NSSPKIXExtensionAttributes *eas,
  NSSArena *arenaOpt
);

/*
 * fgmr
 * There should be accessors to search the ExtensionAttributes and
 * return the value for a specific value, etc.
 */

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
 * The public calls for this type:
 *
 *  NSSPKIXExtensionAttribute_Decode
 *  NSSPKIXExtensionAttribute_Create
 *  NSSPKIXExtensionAttribute_Destroy
 *  NSSPKIXExtensionAttribute_Encode
 *  NSSPKIXExtensionAttribute_GetExtensionsAttributeType
 *  NSSPKIXExtensionAttribute_SetExtensionsAttributeType
 *  NSSPKIXExtensionAttribute_GetExtensionsAttributeValue
 *  NSSPKIXExtensionAttribute_SetExtensionsAttributeValue
 *  NSSPKIXExtensionAttribute_Equal
 *  NSSPKIXExtensionAttribute_Duplicate
 *
 */

/*
 * NSSPKIXExtensionAttribute_Decode
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
NSSPKIXExtensionAttribute_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXExtensionAttribute_Create
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
NSSPKIXExtensionAttribute_Create
(
  NSSArena *arenaOpt,
  NSSPKIXExtensionAttributeType type,
  NSSItem *value
);

/*
 * NSSPKIXExtensionAttribute_Destroy
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
NSSPKIXExtensionAttribute_Destroy
(
  NSSPKIXExtensionAttribute *ea
);

/*
 * NSSPKIXExtensionAttribute_Encode
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
NSSPKIXExtensionAttribute_Encode
(
  NSSPKIXExtensionAttribute *ea,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtensionAttribute_GetExtensionsAttributeType
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
NSSPKIXExtensionAttribute_GetExtensionsAttributeType
(
  NSSPKIXExtensionAttribute *ea
);

/*
 * NSSPKIXExtensionAttribute_SetExtensionsAttributeType
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
NSSPKIXExtensionAttribute_SetExtensionsAttributeType
(
  NSSPKIXExtensionAttribute *ea,
  NSSPKIXExtensionAttributeType type
);

/*
 * NSSPKIXExtensionAttribute_GetExtensionsAttributeValue
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
NSSPKIXExtensionAttribute_GetExtensionsAttributeValue
(
  NSSPKIXExtensionAttribute *ea,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtensionAttribute_SetExtensionsAttributeValue
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
NSSPKIXExtensionAttribute_SetExtensionsAttributeValue
(
  NSSPKIXExtensionAttribute *ea,
  NSSItem *value
);

/*
 * NSSPKIXExtensionAttribute_Equal
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
NSSPKIXExtensionAttribute_Equal
(
  NSSPKIXExtensionAttribute *ea1,
  NSSPKIXExtensionAttribute *ea2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXExtensionAttribute_Duplicate
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
NSSPKIXExtensionAttribute_Duplicate
(
  NSSPKIXExtensionAttribute *ea,
  NSSArena *arenaOpt
);

/*
 * CommonName
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CommonName ::= PrintableString (SIZE (1..ub-common-name-length))
 *
 * The public calls for this type:
 *
 *  NSSPKIXCommonName_Decode
 *  NSSPKIXCommonName_CreateFromUTF8
 *  NSSPKIXCommonName_Encode
 *
 */

/*
 * NSSPKIXCommonName_Decode
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
NSSPKIXCommonName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXCommonName_CreateFromUTF8
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
NSSPKIXCommonName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXCommonName_Encode
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
NSSPKIXCommonName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXTeletexCommonName_Decode
 *  NSSPKIXTeletexCommonName_CreateFromUTF8
 *  NSSPKIXTeletexCommonName_Encode
 *
 */

/*
 * NSSPKIXTeletexCommonName_Decode
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
NSSPKIXTeletexCommonName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTeletexCommonName_CreateFromUTF8
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
NSSPKIXTeletexCommonName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXTeletexCommonName_Encode
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
NSSPKIXTeletexCommonName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXTeletexOrganizationName_Decode
 *  NSSPKIXTeletexOrganizationName_CreateFromUTF8
 *  NSSPKIXTeletexOrganizationName_Encode
 *
 */

/*
 * NSSPKIXTeletexOrganizationName_Decode
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
NSSPKIXTeletexOrganizationName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTeletexOrganizationName_CreateFromUTF8
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
NSSPKIXTeletexOrganizationName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXTeletexOrganizationName_Encode
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
NSSPKIXTeletexOrganizationName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXTeletexPersonalName_Decode
 *  NSSPKIXTeletexPersonalName_Create
 *  NSSPKIXTeletexPersonalName_Destroy
 *  NSSPKIXTeletexPersonalName_Encode
 *  NSSPKIXTeletexPersonalName_GetSurname
 *  NSSPKIXTeletexPersonalName_SetSurname
 *  NSSPKIXTeletexPersonalName_HasGivenName
 *  NSSPKIXTeletexPersonalName_GetGivenName
 *  NSSPKIXTeletexPersonalName_SetGivenName
 *  NSSPKIXTeletexPersonalName_RemoveGivenName
 *  NSSPKIXTeletexPersonalName_HasInitials
 *  NSSPKIXTeletexPersonalName_GetInitials
 *  NSSPKIXTeletexPersonalName_SetInitials
 *  NSSPKIXTeletexPersonalName_RemoveInitials
 *  NSSPKIXTeletexPersonalName_HasGenerationQualifier
 *  NSSPKIXTeletexPersonalName_GetGenerationQualifier
 *  NSSPKIXTeletexPersonalName_SetGenerationQualifier
 *  NSSPKIXTeletexPersonalName_RemoveGenerationQualifier
 *  NSSPKIXTeletexPersonalName_Equal
 *  NSSPKIXTeletexPersonalName_Duplicate
 *
 */

/*
 * NSSPKIXTeletexPersonalName_Decode
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
NSSPKIXTeletexPersonalName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTeletexPersonalName_Create
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
NSSPKIXTeletexPersonalName_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *surname,
  NSSUTF8 *givenNameOpt,
  NSSUTF8 *initialsOpt,
  NSSUTF8 *generationQualifierOpt
);

/*
 * NSSPKIXTeletexPersonalName_Destroy
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
NSSPKIXTeletexPersonalName_Destroy
(
  NSSPKIXTeletexPersonalName *personalName
);

/*
 * NSSPKIXTeletexPersonalName_Encode
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
NSSPKIXTeletexPersonalName_Encode
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexPersonalName_GetSurname
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
NSSPKIXTeletexPersonalName_GetSurname
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexPersonalName_SetSurname
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
NSSPKIXTeletexPersonalName_SetSurname
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSUTF8 *surname
);

/*
 * NSSPKIXTeletexPersonalName_HasGivenName
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
NSSPKIXTeletexPersonalName_HasGivenName
(
  NSSPKIXTeletexPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTeletexPersonalName_GetGivenName
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
NSSPKIXTeletexPersonalName_GetGivenName
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexPersonalName_SetGivenName
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
NSSPKIXTeletexPersonalName_SetGivenName
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSUTF8 *givenName
);

/*
 * NSSPKIXTeletexPersonalName_RemoveGivenName
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
NSSPKIXTeletexPersonalName_RemoveGivenName
(
  NSSPKIXTeletexPersonalName *personalName
);

/*
 * NSSPKIXTeletexPersonalName_HasInitials
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
NSSPKIXTeletexPersonalName_HasInitials
(
  NSSPKIXTeletexPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTeletexPersonalName_GetInitials
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
NSSPKIXTeletexPersonalName_GetInitials
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexPersonalName_SetInitials
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
NSSPKIXTeletexPersonalName_SetInitials
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSUTF8 *initials
);

/*
 * NSSPKIXTeletexPersonalName_RemoveInitials
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
NSSPKIXTeletexPersonalName_RemoveInitials
(
  NSSPKIXTeletexPersonalName *personalName
);

/*
 * NSSPKIXTeletexPersonalName_HasGenerationQualifier
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
NSSPKIXTeletexPersonalName_HasGenerationQualifier
(
  NSSPKIXTeletexPersonalName *personalName,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTeletexPersonalName_GetGenerationQualifier
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
NSSPKIXTeletexPersonalName_GetGenerationQualifier
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexPersonalName_SetGenerationQualifier
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
NSSPKIXTeletexPersonalName_SetGenerationQualifier
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSUTF8 *generationQualifier
);

/*
 * NSSPKIXTeletexPersonalName_RemoveGenerationQualifier
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
NSSPKIXTeletexPersonalName_RemoveGenerationQualifier
(
  NSSPKIXTeletexPersonalName *personalName
);

/*
 * NSSPKIXTeletexPersonalName_Equal
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
NSSPKIXTeletexPersonalName_Equal
(
  NSSPKIXTeletexPersonalName *personalName1,
  NSSPKIXTeletexPersonalName *personalName2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTeletexPersonalName_Duplicate
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
NSSPKIXTeletexPersonalName_Duplicate
(
  NSSPKIXTeletexPersonalName *personalName,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXTeletexOrganizationalUnitNames_Decode
 *  NSSPKIXTeletexOrganizationalUnitNames_Create
 *  NSSPKIXTeletexOrganizationalUnitNames_Destroy
 *  NSSPKIXTeletexOrganizationalUnitNames_Encode
 *  NSSPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNameCount
 *  NSSPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNames
 *  NSSPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitNames
 *  NSSPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitName
 *  NSSPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitName
 *  NSSPKIXTeletexOrganizationalUnitNames_InsertTeletexOrganizationalUnitName
 *  NSSPKIXTeletexOrganizationalUnitNames_AppendTeletexOrganizationalUnitName
 *  NSSPKIXTeletexOrganizationalUnitNames_RemoveTeletexOrganizationalUnitName
 *  NSSPKIXTeletexOrganizationalUnitNames_FindTeletexOrganizationalUnitName
 *  NSSPKIXTeletexOrganizationalUnitNames_Equal
 *  NSSPKIXTeletexOrganizationalUnitNames_Duplicate
 *
 */

/*
 * NSSPKIXTeletexOrganizationalUnitNames_Decode
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
NSSPKIXTeletexOrganizationalUnitNames_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_Create
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
NSSPKIXTeletexOrganizationalUnitNames_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTeletexOrganizationalUnitName *ou1,
  ...
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_Destroy
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
NSSPKIXTeletexOrganizationalUnitNames_Destroy
(
  NSSPKIXTeletexOrganizationalUnitNames *ous
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_Encode
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
NSSPKIXTeletexOrganizationalUnitNames_Encode
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNameCount
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
NSSPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNameCount
(
  NSSPKIXTeletexOrganizationalUnitNames *ous
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNames
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
NSSPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitNames
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSPKIXTeletexOrganizationalUnitName *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitNames
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
NSSPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitNames
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSPKIXTeletexOrganizationalUnitName *ou[],
  PRInt32 count
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitName
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
NSSPKIXTeletexOrganizationalUnitNames_GetTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitName
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
NSSPKIXTeletexOrganizationalUnitNames_SetTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSPKIXTeletexOrganizationalUnitName *ou
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_InsertTeletexOrganizationalUnitName
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
NSSPKIXTeletexOrganizationalUnitNames_InsertTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  PRInt32 i,
  NSSPKIXTeletexOrganizationalUnitName *ou
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_AppendTeletexOrganizationalUnitName
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
NSSPKIXTeletexOrganizationalUnitNames_AppendTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSPKIXTeletexOrganizationalUnitName *ou
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_RemoveTeletexOrganizationalUnitName
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
NSSPKIXTeletexOrganizationalUnitNames_RemoveTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  PRInt32 i
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_FindTeletexOrganizationalUnitName
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
NSSPKIXTeletexOrganizationalUnitNames_FindTeletexOrganizationalUnitName
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSPKIXTeletexOrganizationalUnitName *ou
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_Equal
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
NSSPKIXTeletexOrganizationalUnitNames_Equal
(
  NSSPKIXTeletexOrganizationalUnitNames *ous1,
  NSSPKIXTeletexOrganizationalUnitNames *ous2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTeletexOrganizationalUnitNames_Duplicate
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
NSSPKIXTeletexOrganizationalUnitNames_Duplicate
(
  NSSPKIXTeletexOrganizationalUnitNames *ous,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXTeletexOrganizationalUnitName_Decode
 *  NSSPKIXTeletexOrganizationalUnitName_CreateFromUTF8
 *  NSSPKIXTeletexOrganizationalUnitName_Encode
 *
 */

/*
 * NSSPKIXTeletexOrganizationalUnitName_Decode
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
NSSPKIXTeletexOrganizationalUnitName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTeletexOrganizationalUnitName_CreateFromUTF8
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
NSSPKIXTeletexOrganizationalUnitName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXTeletexOrganizationalUnitName_Encode
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
NSSPKIXTeletexOrganizationalUnitName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXPDSName_Decode
 *  NSSPKIXPDSName_CreateFromUTF8
 *  NSSPKIXPDSName_Encode
 *
 */

/*
 * NSSPKIXPDSName_Decode
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
NSSPKIXPDSName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPDSName_CreateFromUTF8
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
NSSPKIXPDSName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXPDSName_Encode
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
NSSPKIXPDSName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXPhysicalDeliveryCountryName_Decode
 *  NSSPKIXPhysicalDeliveryCountryName_CreateFromUTF8
 *  NSSPKIXPhysicalDeliveryCountryName_Encode
 *
 */

/*
 * NSSPKIXPhysicalDeliveryCountryName_Decode
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
NSSPKIXPhysicalDeliveryCountryName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPhysicalDeliveryCountryName_CreateFromUTF8
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
NSSPKIXPhysicalDeliveryCountryName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXPhysicalDeliveryCountryName_Encode
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
NSSPKIXPhysicalDeliveryCountryName_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXPostalCode_Decode
 *  NSSPKIXPostalCode_CreateFromUTF8
 *  NSSPKIXPostalCode_Encode
 *
 */

/*
 * NSSPKIXPostalCode_Decode
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
NSSPKIXPostalCode_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPostalCode_CreateFromUTF8
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
NSSPKIXPostalCode_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXPostalCode_Encode
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
NSSPKIXPostalCode_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXPDSParameter_Decode
 *  NSSPKIXPDSParameter_CreateFromUTF8
 *  NSSPKIXPDSParameter_Create
 *  NSSPKIXPDSParameter_Delete
 *  NSSPKIXPDSParameter_Encode
 *  NSSPKIXPDSParameter_GetUTF8Encoding
 *  NSSPKIXPDSParameter_HasPrintableString
 *  NSSPKIXPDSParameter_GetPrintableString
 *  NSSPKIXPDSParameter_SetPrintableString
 *  NSSPKIXPDSParameter_RemovePrintableString
 *  NSSPKIXPDSParameter_HasTeletexString
 *  NSSPKIXPDSParameter_GetTeletexString
 *  NSSPKIXPDSParameter_SetTeletexString
 *  NSSPKIXPDSParameter_RemoveTeletexString
 *  NSSPKIXPDSParameter_Equal
 *  NSSPKIXPDSParameter_Duplicate
 */

/*
 * NSSPKIXPDSParameter_Decode
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
NSSPKIXPDSParameter_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPDSParameter_CreateFromUTF8
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
NSSPKIXPDSParameter_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXPDSParameter_Create
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
NSSPKIXPDSParameter_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *printableStringOpt,
  NSSUTF8 *teletexStringOpt
);

/*
 * NSSPKIXPDSParameter_Destroy
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
NSSPKIXPDSParameter_Destroy
(
  NSSPKIXPDSParameter *p
);

/*
 * NSSPKIXPDSParameter_Encode
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
NSSPKIXPDSParameter_Encode
(
  NSSPKIXPDSParameter *p,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPDSParameter_GetUTF8Encoding
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
NSSPKIXPDSParameter_GetUTF8Encoding
(
  NSSPKIXPDSParameter *p,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPDSParameter_HasPrintableString
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
NSSPKIXPDSParameter_HasPrintableString
(
  NSSPKIXPDSParameter *p,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPDSParameter_GetPrintableString
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
NSSPKIXPDSParameter_GetPrintableString
(
  NSSPKIXPDSParameter *p,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPDSParameter_SetPrintableString
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
NSSPKIXPDSParameter_SetPrintableString
(
  NSSPKIXPDSParameter *p,
  NSSUTF8 *printableString
);

/*
 * NSSPKIXPDSParameter_RemovePrintableString
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
NSSPKIXPDSParameter_RemovePrintableString
(
  NSSPKIXPDSParameter *p
);

/*
 * NSSPKIXPDSParameter_HasTeletexString
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
NSSPKIXPDSParameter_HasTeletexString
(
  NSSPKIXPDSParameter *p,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPDSParameter_GetTeletexString
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
NSSPKIXPDSParameter_GetTeletexString
(
  NSSPKIXPDSParameter *p,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPDSParameter_SetTeletexString
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
NSSPKIXPDSParameter_SetTeletexString
(
  NSSPKIXPDSParameter *p,
  NSSUTF8 *teletexString
);

/*
 * NSSPKIXPDSParameter_RemoveTeletexString
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
NSSPKIXPDSParameter_RemoveTeletexString
(
  NSSPKIXPDSParameter *p
);

/*
 * NSSPKIXPDSParameter_Equal
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
NSSPKIXPDSParameter_Equal
(
  NSSPKIXPDSParameter *p1,
  NSSPKIXPDSParameter *p2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPDSParameter_Duplicate
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
NSSPKIXPDSParameter_Duplicate
(
  NSSPKIXPDSParameter *p,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
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
 * The public calls for the type:
 *
 *  NSSPKIXExtendedNetworkAddress_Decode
 *  NSSPKIXExtendedNetworkAddress_Create
 *  NSSPKIXExtendedNetworkAddress_Destroy
 *  NSSPKIXExtendedNetworkAddress_Encode
 *  NSSPKIXExtendedNetworkAddress_GetChoice
 *  NSSPKIXExtendedNetworkAddress_Get
 *  NSSPKIXExtendedNetworkAddress_GetE1634Address
 *  NSSPKIXExtendedNetworkAddress_GetPsapAddress
 *  NSSPKIXExtendedNetworkAddress_Set
 *  NSSPKIXExtendedNetworkAddress_SetE163Address
 *  NSSPKIXExtendedNetworkAddress_SetPsapAddress
 *  NSSPKIXExtendedNetworkAddress_Equal
 *  NSSPKIXExtendedNetworkAddress_Duplicate
 *
 */

/*
 * NSSPKIXExtendedNetworkAddress_Decode
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
NSSPKIXExtendedNetworkAddress_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXExtendedNetworkAddress_Create
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
NSSPKIXExtendedNetworkAddress_Create
(
  NSSArena *arenaOpt,
  NSSPKIXExtendedNetworkAddressChoice choice,
  void *address
);

/*
 * NSSPKIXExtendedNetworkAddress_CreateFromE1634Address
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
NSSPKIXExtendedNetworkAddress_CreateFromE1634Address
(
  NSSArena *arenaOpt,
  NSSPKIXe1634Address *e1634address
);

/*
 * NSSPKIXExtendedNetworkAddress_CreateFromPresentationAddress
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
NSSPKIXExtendedNetworkAddress_CreateFromPresentationAddress
(
  NSSArena *arenaOpt,
  NSSPKIXPresentationAddress *presentationAddress
);

/*
 * NSSPKIXExtendedNetworkAddress_Destroy
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
NSSPKIXExtendedNetworkAddress_Destroy
(
  NSSPKIXExtendedNetworkAddress *ena
);

/*
 * NSSPKIXExtendedNetworkAddress_Encode
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
NSSPKIXExtendedNetworkAddress_Encode
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtendedNetworkAddress_GetChoice
 *
 * -- fgmr comments --
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_PKIX_EXTENDED_NETWORK_ADDRESS
 * 
 * Return value:
 *  A valid element of the NSSPKIXExtendedNetworkAddressChoice upon
 *      success
 *  The value NSSPKIXExtendedNetworkAddress_NSSinvalid upon failure
 */

NSS_EXTERN NSSPKIXExtendedNetworkAddressChoice
NSSPKIXExtendedNetworkAddress_GetChoice
(
  NSSPKIXExtendedNetworkAddress *ena
);

/*
 * NSSPKIXExtendedNetworkAddress_GetSpecifiedChoice
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
NSSPKIXExtendedNetworkAddress_GetSpecifiedChoice
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSPKIXExtendedNetworkAddressChoice which,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtendedNetworkAddress_GetE1634Address
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
NSSPKIXExtendedNetworkAddress_GetE1634Address
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtendedNetworkAddress_GetPresentationAddress
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
NSSPKIXExtendedNetworkAddress_GetPresentationAddress
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtendedNetworkAddress_SetSpecifiedChoice
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
NSSPKIXExtendedNetworkAddress_SetSpecifiedChoice
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSPKIXExtendedNetworkAddressChoice which,
  void *address
);

/*
 * NSSPKIXExtendedNetworkAddress_SetE163Address
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
NSSPKIXExtendedNetworkAddress_SetE163Address
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSPKIXe1634Address *e1634address
);

/*
 * NSSPKIXExtendedNetworkAddress_SetPresentationAddress
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
NSSPKIXExtendedNetworkAddress_SetPresentationAddress
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSPKIXPresentationAddress *presentationAddress
);

/*
 * NSSPKIXExtendedNetworkAddress_Equal
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
NSSPKIXExtendedNetworkAddress_Equal
(
  NSSPKIXExtendedNetworkAddress *ena1,
  NSSPKIXExtendedNetworkAddress *ena2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXExtendedNetworkAddress_Duplicate
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
NSSPKIXExtendedNetworkAddress_Duplicate
(
  NSSPKIXExtendedNetworkAddress *ena,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXe1634Address_Decode
 *  NSSPKIXe1634Address_Create
 *  NSSPKIXe1634Address_Destroy
 *  NSSPKIXe1634Address_Encode
 *  NSSPKIXe1634Address_GetNumber
 *  NSSPKIXe1634Address_SetNumber
 *  NSSPKIXe1634Address_HasSubAddress
 *  NSSPKIXe1634Address_GetSubAddress
 *  NSSPKIXe1634Address_SetSubAddress
 *  NSSPKIXe1634Address_RemoveSubAddress
 *  NSSPKIXe1634Address_Equal
 *  NSSPKIXe1634Address_Duplicate
 *
 */

/*
 * NSSPKIXe1634Address_Decode
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
NSSPKIXe1634Address_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXe1634Address_Create
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
NSSPKIXe1634Address_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *number,
  NSSUTF8 *subAddressOpt
);

/*
 * NSSPKIXe1634Address_Destroy
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
NSSPKIXe1634Address_Destroy
(
  NSSPKIXe1634Address *e
);

/*
 * NSSPKIXe1634Address_Encode
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
NSSPKIXe1634Address_Encode
(
  NSSPKIXe1634Address *e,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXe1634Address_GetNumber
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
NSSPKIXe1634Address_GetNumber
(
  NSSPKIXe1634Address *e,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXe1634Address_SetNumber
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
NSSPKIXe1634Address_SetNumber
(
  NSSPKIXe1634Address *e,
  NSSUTF8 *number
);

/*
 * NSSPKIXe1634Address_HasSubAddress
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
NSSPKIXe1634Address_HasSubAddress
(
  NSSPKIXe1634Address *e,
  PRStatus *statusOpt
);

/*
 * NSSPKIXe1634Address_GetSubAddress
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
NSSPKIXe1634Address_GetSubAddress
(
  NSSPKIXe1634Address *e,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXe1634Address_SetSubAddress
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
NSSPKIXe1634Address_SetSubAddress
(
  NSSPKIXe1634Address *e,
  NSSUTF8 *subAddress
);

/*
 * NSSPKIXe1634Address_RemoveSubAddress
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
NSSPKIXe1634Address_RemoveSubAddress
(
  NSSPKIXe1634Address *e
);

/*
 * NSSPKIXe1634Address_Equal
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
NSSPKIXe1634Address_Equal
(
  NSSPKIXe1634Address *e1,
  NSSPKIXe1634Address *e2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXe1634Address_Duplicate
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
NSSPKIXe1634Address_Duplicate
(
  NSSPKIXe1634Address *e,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXPresentationAddress_Decode
 *  NSSPKIXPresentationAddress_Create
 *  NSSPKIXPresentationAddress_Destroy
 *  NSSPKIXPresentationAddress_Encode
 *  NSSPKIXPresentationAddress_HasPSelector
 *  NSSPKIXPresentationAddress_GetPSelector
 *  NSSPKIXPresentationAddress_SetPSelector
 *  NSSPKIXPresentationAddress_RemovePSelector
 *  NSSPKIXPresentationAddress_HasSSelector
 *  NSSPKIXPresentationAddress_GetSSelector
 *  NSSPKIXPresentationAddress_SetSSelector
 *  NSSPKIXPresentationAddress_RemoveSSelector
 *  NSSPKIXPresentationAddress_HasTSelector
 *  NSSPKIXPresentationAddress_GetTSelector
 *  NSSPKIXPresentationAddress_SetTSelector
 *  NSSPKIXPresentationAddress_RemoveTSelector
 *  NSSPKIXPresentationAddress_HasNAddresses
 *  NSSPKIXPresentationAddress_GetNAddresses
 *  NSSPKIXPresentationAddress_SetNAddresses
 *  NSSPKIXPresentationAddress_RemoveNAddresses
 *{NAddresses must be more complex than that}
 *  NSSPKIXPresentationAddress_Compare
 *  NSSPKIXPresentationAddress_Duplicate
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
 * The public calls for the type:
 *
 *  NSSPKIXTeletexDomainDefinedAttributes_Decode
 *  NSSPKIXTeletexDomainDefinedAttributes_Create
 *  NSSPKIXTeletexDomainDefinedAttributes_Destroy
 *  NSSPKIXTeletexDomainDefinedAttributes_Encode
 *  NSSPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributeCount
 *  NSSPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributes
 *  NSSPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttributes
 *  NSSPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttribute
 *  NSSPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttribute
 *  NSSPKIXTeletexDomainDefinedAttributes_InsertTeletexDomainDefinedAttribute
 *  NSSPKIXTeletexDomainDefinedAttributes_AppendTeletexDomainDefinedAttribute
 *  NSSPKIXTeletexDomainDefinedAttributes_RemoveTeletexDomainDefinedAttribute
 *  NSSPKIXTeletexDomainDefinedAttributes_FindTeletexDomainDefinedAttribute
 *  NSSPKIXTeletexDomainDefinedAttributes_Equal
 *  NSSPKIXTeletexDomainDefinedAttributes_Duplicate
 *
 */

/*
 * NSSPKIXTeletexDomainDefinedAttributes_Decode
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
NSSPKIXTeletexDomainDefinedAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_Create
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
NSSPKIXTeletexDomainDefinedAttributes_Create
(
  NSSArena *arenaOpt,
  NSSPKIXTeletexDomainDefinedAttribute *tdda1,
  ...
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_Destroy
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
NSSPKIXTeletexDomainDefinedAttributes_Destroy
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_Encode
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
NSSPKIXTeletexDomainDefinedAttributes_Encode
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributeCount
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
NSSPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributeCount
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributes
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
NSSPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttributes
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSPKIXTeletexDomainDefinedAttribute *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttributes
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
NSSPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttributes
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSPKIXTeletexDomainDefinedAttribute *tdda[],
  PRInt32 count
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttribute
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
NSSPKIXTeletexDomainDefinedAttributes_GetTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttribute
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
NSSPKIXTeletexDomainDefinedAttributes_SetTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  PRInt32 i,
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_InsertTeletexDomainDefinedAttribute
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
NSSPKIXTeletexDomainDefinedAttributes_InsertTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  PRInt32 i,
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_AppendTeletexDomainDefinedAttribute
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
NSSPKIXTeletexDomainDefinedAttributes_AppendTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_RemoveTeletexDomainDefinedAttribute
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
NSSPKIXTeletexDomainDefinedAttributes_RemoveTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  PRInt32 i
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_FindTeletexDomainDefinedAttribute
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
NSSPKIXTeletexDomainDefinedAttributes_FindTeletexDomainDefinedAttribute
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_Equal
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
NSSPKIXTeletexDomainDefinedAttributes_Equal
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas1,
  NSSPKIXTeletexDomainDefinedAttributes *tddas2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTeletexDomainDefinedAttributes_Duplicate
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
NSSPKIXTeletexDomainDefinedAttributes_Duplicate
(
  NSSPKIXTeletexDomainDefinedAttributes *tddas,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXTeletexDomainDefinedAttribute_Decode
 *  NSSPKIXTeletexDomainDefinedAttribute_CreateFromUTF8
 *  NSSPKIXTeletexDomainDefinedAttribute_Create
 *  NSSPKIXTeletexDomainDefinedAttribute_Destroy
 *  NSSPKIXTeletexDomainDefinedAttribute_Encode
 *  NSSPKIXTeletexDomainDefinedAttribute_GetUTF8Encoding
 *  NSSPKIXTeletexDomainDefinedAttribute_GetType
 *  NSSPKIXTeletexDomainDefinedAttribute_SetType
 *  NSSPKIXTeletexDomainDefinedAttribute_GetValue
 *  NSSPKIXTeletexDomainDefinedAttribute_GetValue
 *  NSSPKIXTeletexDomainDefinedAttribute_Equal
 *  NSSPKIXTeletexDomainDefinedAttribute_Duplicate
 *
 */

/*
 * NSSPKIXTeletexDomainDefinedAttribute_Decode
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
NSSPKIXTeletexDomainDefinedAttribute_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_CreateFromUTF8
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
NSSPKIXTeletexDomainDefinedAttribute_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_Create
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
NSSPKIXTeletexDomainDefinedAttribute_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *type,
  NSSUTF8 *value
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_Destroy
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
NSSPKIXTeletexDomainDefinedAttribute_Destroy
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_Encode
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
NSSPKIXTeletexDomainDefinedAttribute_Encode
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_GetUTF8Encoding
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
NSSPKIXTeletexDomainDefinedAttribute_GetUTF8Encoding
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_GetType
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
NSSPKIXTeletexDomainDefinedAttribute_GetType
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_SetType
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
NSSPKIXTeletexDomainDefinedAttribute_SetType
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSUTF8 *type
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_GetValue
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
NSSPKIXTeletexDomainDefinedAttribute_GetValue
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_SetValue
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
NSSPKIXTeletexDomainDefinedAttribute_SetValue
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSUTF8 *value
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_Equal
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
NSSPKIXTeletexDomainDefinedAttribute_Equal
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda1,
  NSSPKIXTeletexDomainDefinedAttribute *tdda2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXTeletexDomainDefinedAttribute_Duplicate
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
NSSPKIXTeletexDomainDefinedAttribute_Duplicate
(
  NSSPKIXTeletexDomainDefinedAttribute *tdda,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXAuthorityKeyIdentifier_Decode
 *  NSSPKIXAuthorityKeyIdentifier_Create
 *  NSSPKIXAuthorityKeyIdentifier_Destroy
 *  NSSPKIXAuthorityKeyIdentifier_Encode
 *  NSSPKIXAuthorityKeyIdentifier_HasKeyIdentifier
 *  NSSPKIXAuthorityKeyIdentifier_GetKeyIdentifier
 *  NSSPKIXAuthorityKeyIdentifier_SetKeyIdentifier
 *  NSSPKIXAuthorityKeyIdentifier_RemoveKeyIdentifier
 *  NSSPKIXAuthorityKeyIdentifier_HasAuthorityCertIssuerAndSerialNumber
 *  NSSPKIXAuthorityKeyIdentifier_RemoveAuthorityCertIssuerAndSerialNumber
 *  NSSPKIXAuthorityKeyIdentifier_GetAuthorityCertIssuer
 *  NSSPKIXAuthorityKeyIdentifier_GetAuthorityCertSerialNumber
 *  NSSPKIXAuthorityKeyIdentifier_SetAuthorityCertIssuerAndSerialNumber
 *  NSSPKIXAuthorityKeyIdentifier_Equal
 *  NSSPKIXAuthorityKeyIdentifier_Duplicate
 *
 */

/*
 * NSSPKIXAuthorityKeyIdentifier_Decode
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
NSSPKIXAuthorityKeyIdentifier_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXAuthorityKeyIdentifier_Create
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
NSSPKIXAuthorityKeyIdentifier_Create
(
  NSSArena *arenaOpt,
  NSSPKIXKeyIdentifier *keyIdentifierOpt,
  NSSPKIXGeneralNames *authorityCertIssuerOpt,
  NSSPKIXCertificateSerialNumber *authorityCertSerialNumberOpt
);

/*
 * NSSPKIXAuthorityKeyIdentifier_Destroy
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
NSSPKIXAuthorityKeyIdentifier_Destroy
(
  NSSPKIXAuthorityKeyIdentifier *aki
);

/*
 * NSSPKIXAuthorityKeyIdentifier_Encode
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
NSSPKIXAuthorityKeyIdentifier_Encode
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAuthorityKeyIdentifier_HasKeyIdentifier
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
NSSPKIXAuthorityKeyIdentifier_HasKeyIdentifier
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  PRStatus *statusOpt
);

/*
 * NSSPKIXAuthorityKeyIdentifier_GetKeyIdentifier
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
NSSPKIXAuthorityKeyIdentifier_GetKeyIdentifier
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSPKIXKeyIdentifier *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAuthorityKeyIdentifier_SetKeyIdentifier
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
NSSPKIXAuthorityKeyIdentifier_SetKeyIdentifier
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSPKIXKeyIdentifier *keyIdentifier
);

/*
 * NSSPKIXAuthorityKeyIdentifier_RemoveKeyIdentifier
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
NSSPKIXAuthorityKeyIdentifier_RemoveKeyIdentifier
(
  NSSPKIXAuthorityKeyIdentifier *aki
);

/*
 * NSSPKIXAuthorityKeyIdentifier_HasAuthorityCertIssuerAndSerialNumber
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
NSSPKIXAuthorityKeyIdentifier_HasAuthorityCertIssuerAndSerialNumber
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  PRStatus *statusOpt
);

/*
 * NSSPKIXAuthorityKeyIdentifier_RemoveAuthorityCertIssuerAndSerialNumber
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
NSSPKIXAuthorityKeyIdentifier_RemoveAuthorityCertIssuerAndSerialNumber
(
  NSSPKIXAuthorityKeyIdentifier *aki
);

/*
 * NSSPKIXAuthorityKeyIdentifier_GetAuthorityCertIssuer
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
NSSPKIXAuthorityKeyIdentifier_GetAuthorityCertIssuer
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAuthorityKeyIdentifier_GetAuthorityCertSerialNumber
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
NSSPKIXAuthorityKeyIdentifier_GetAuthorityCertSerialNumber
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSPKIXCertificateSerialNumber *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAuthorityKeyIdentifier_SetAuthorityCertIssuerAndSerialNumber
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
NSSPKIXAuthorityKeyIdentifier_SetAuthorityCertIssuerAndSerialNumber
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSPKIXGeneralNames *issuer,
  NSSPKIXCertificateSerialNumber *serialNumber
);

/*
 * NSSPKIXAuthorityKeyIdentifier_Equal
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
NSSPKIXAuthorityKeyIdentifier_Equal
(
  NSSPKIXAuthorityKeyIdentifier *aki1,
  NSSPKIXAuthorityKeyIdentifier *aki2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXAuthorityKeyIdentifier_Duplicate
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
NSSPKIXAuthorityKeyIdentifier_Duplicate
(
  NSSPKIXAuthorityKeyIdentifier *aki,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXKeyUsage_Decode
 *  NSSPKIXKeyUsage_CreateFromUTF8
 *  NSSPKIXKeyUsage_CreateFromValue
 *  NSSPKIXKeyUsage_Destroy
 *  NSSPKIXKeyUsage_Encode
 *  NSSPKIXKeyUsage_GetUTF8Encoding
 *  NSSPKIXKeyUsage_GetValue
 *  NSSPKIXKeyUsage_SetValue
 *    { bitwise accessors? }
 *  NSSPKIXKeyUsage_Equal
 *  NSSPKIXKeyUsage_Duplicate
 *
 */

/*
 * NSSPKIXKeyUsage_Decode
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
NSSPKIXKeyUsage_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXKeyUsage_CreateFromUTF8
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
NSSPKIXKeyUsage_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXKeyUsage_CreateFromValue
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
NSSPKIXKeyUsage_CreateFromValue
(
  NSSArena *arenaOpt,
  NSSPKIXKeyUsageValue value
);

/*
 * NSSPKIXKeyUsage_Destroy
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
NSSPKIXKeyUsage_Destroy
(
  NSSPKIXKeyUsage *keyUsage
);

/*
 * NSSPKIXKeyUsage_Encode
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
NSSPKIXKeyUsage_Encode
(
  NSSPKIXKeyUsage *keyUsage,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXKeyUsage_GetUTF8Encoding
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
NSSPKIXKeyUsage_GetUTF8Encoding
(
  NSSPKIXKeyUsage *keyUsage,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXKeyUsage_GetValue
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
NSSPKIXKeyUsage_GetValue
(
  NSSPKIXKeyUsage *keyUsage
);

/*
 * NSSPKIXKeyUsage_SetValue
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
NSSPKIXKeyUsage_SetValue
(
  NSSPKIXKeyUsage *keyUsage,
  NSSPKIXKeyUsageValue value
);

/*
 * NSSPKIXKeyUsage_Equal
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
NSSPKIXKeyUsage_Equal
(
  NSSPKIXKeyUsage *keyUsage1,
  NSSPKIXKeyUsage *keyUsage2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXKeyUsage_Duplicate
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
NSSPKIXKeyUsage_Duplicate
(
  NSSPKIXKeyUsage *keyUsage,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXPrivateKeyUsagePeriod_Decode
 *  NSSPKIXPrivateKeyUsagePeriod_Create
 *  NSSPKIXPrivateKeyUsagePeriod_Destroy
 *  NSSPKIXPrivateKeyUsagePeriod_Encode
 *  NSSPKIXPrivateKeyUsagePeriod_HasNotBefore
 *  NSSPKIXPrivateKeyUsagePeriod_GetNotBefore
 *  NSSPKIXPrivateKeyUsagePeriod_SetNotBefore
 *  NSSPKIXPrivateKeyUsagePeriod_RemoveNotBefore
 *  NSSPKIXPrivateKeyUsagePeriod_HasNotAfter
 *  NSSPKIXPrivateKeyUsagePeriod_GetNotAfter
 *  NSSPKIXPrivateKeyUsagePeriod_SetNotAfter
 *  NSSPKIXPrivateKeyUsagePeriod_RemoveNotAfter
 *  NSSPKIXPrivateKeyUsagePeriod_Equal
 *  NSSPKIXPrivateKeyUsagePeriod_Duplicate
 *
 */

/*
 * NSSPKIXPrivateKeyUsagePeriod_Decode
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
NSSPKIXPrivateKeyUsagePeriod_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_Create
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
NSSPKIXPrivateKeyUsagePeriod_Create
(
  NSSArena *arenaOpt,
  NSSTime *notBeforeOpt,
  NSSTime *notAfterOpt
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_Destroy
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
NSSPKIXPrivateKeyUsagePeriod_Destroy
(
  NSSPKIXPrivateKeyUsagePeriod *period
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_Encode
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
NSSPKIXPrivateKeyUsagePeriod_Encode
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_HasNotBefore
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
NSSPKIXPrivateKeyUsagePeriod_HasNotBefore
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_GetNotBefore
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
NSSPKIXPrivateKeyUsagePeriod_GetNotBefore
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_SetNotBefore
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
NSSPKIXPrivateKeyUsagePeriod_SetNotBefore
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSTime *notBefore
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_RemoveNotBefore
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
NSSPKIXPrivateKeyUsagePeriod_RemoveNotBefore
(
  NSSPKIXPrivateKeyUsagePeriod *period
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_HasNotAfter
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
NSSPKIXPrivateKeyUsagePeriod_HasNotAfter
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_GetNotAfter
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
NSSPKIXPrivateKeyUsagePeriod_GetNotAfter
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_SetNotAfter
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
NSSPKIXPrivateKeyUsagePeriod_SetNotAfter
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSTime *notAfter
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_RemoveNotAfter
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
NSSPKIXPrivateKeyUsagePeriod_RemoveNotAfter
(
  NSSPKIXPrivateKeyUsagePeriod *period
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_Equal
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
NSSPKIXPrivateKeyUsagePeriod_Equal
(
  NSSPKIXPrivateKeyUsagePeriod *period1,
  NSSPKIXPrivateKeyUsagePeriod *period2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPrivateKeyUsagePeriod_Duplicate
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
NSSPKIXPrivateKeyUsagePeriod_Duplicate
(
  NSSPKIXPrivateKeyUsagePeriod *period,
  NSSArena *arenaOpt
);

/*
 * CertificatePolicies
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CertificatePolicies ::= SEQUENCE SIZE (1..MAX) OF PolicyInformation
 *
 * The public calls for the type:
 *
 *  NSSPKIXCertificatePolicies_Decode
 *  NSSPKIXCertificatePolicies_Create
 *  NSSPKIXCertificatePolicies_Destroy
 *  NSSPKIXCertificatePolicies_Encode
 *  NSSPKIXCertificatePolicies_GetPolicyInformationCount
 *  NSSPKIXCertificatePolicies_GetPolicyInformations
 *  NSSPKIXCertificatePolicies_SetPolicyInformations
 *  NSSPKIXCertificatePolicies_GetPolicyInformation
 *  NSSPKIXCertificatePolicies_SetPolicyInformation
 *  NSSPKIXCertificatePolicies_InsertPolicyInformation
 *  NSSPKIXCertificatePolicies_AppendPolicyInformation
 *  NSSPKIXCertificatePolicies_RemovePolicyInformation
 *  NSSPKIXCertificatePolicies_FindPolicyInformation
 *  NSSPKIXCertificatePolicies_Equal
 *  NSSPKIXCertificatePolicies_Duplicate
 *
 */

/*
 * NSSPKIXCertificatePolicies_Decode
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
NSSPKIXCertificatePolicies_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXCertificatePolicies_Create
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
NSSPKIXCertificatePolicies_Create
(
  NSSArena *arenaOpt,
  NSSPKIXPolicyInformation *pi1,
  ...
);

/*
 * NSSPKIXCertificatePolicies_Destroy
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
NSSPKIXCertificatePolicies_Destroy
(
  NSSPKIXCertificatePolicies *cp
);

/*
 * NSSPKIXCertificatePolicies_Encode
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
NSSPKIXCertificatePolicies_Encode
(
  NSSPKIXCertificatePolicies *cp,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificatePolicies_GetPolicyInformationCount
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
NSSPKIXCertificatePolicies_GetPolicyInformationCount
(
  NSSPKIXCertificatePolicies *cp
);

/*
 * NSSPKIXCertificatePolicies_GetPolicyInformations
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
NSSPKIXCertificatePolicies_GetPolicyInformations
(
  NSSPKIXCertificatePolicies *cp,
  NSSPKIXPolicyInformation *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificatePolicies_SetPolicyInformations
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
NSSPKIXCertificatePolicies_SetPolicyInformations
(
  NSSPKIXCertificatePolicies *cp,
  NSSPKIXPolicyInformation *pi[],
  PRInt32 count
);

/*
 * NSSPKIXCertificatePolicies_GetPolicyInformation
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
NSSPKIXCertificatePolicies_GetPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCertificatePolicies_SetPolicyInformation
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
NSSPKIXCertificatePolicies_SetPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  PRInt32 i,
  NSSPKIXPolicyInformation *pi
);

/*
 * NSSPKIXCertificatePolicies_InsertPolicyInformation
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
NSSPKIXCertificatePolicies_InsertPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  PRInt32 i,
  NSSPKIXPolicyInformation *pi
);

/*
 * NSSPKIXCertificatePolicies_AppendPolicyInformation
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
NSSPKIXCertificatePolicies_AppendPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  NSSPKIXPolicyInformation *pi
);

/*
 * NSSPKIXCertificatePolicies_RemovePolicyInformation
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
NSSPKIXCertificatePolicies_RemovePolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  PRInt32 i
);

/*
 * NSSPKIXCertificatePolicies_FindPolicyInformation
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
NSSPKIXCertificatePolicies_FindPolicyInformation
(
  NSSPKIXCertificatePolicies *cp,
  NSSPKIXPolicyInformation *pi
);

/*
 * NSSPKIXCertificatePolicies_Equal
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
NSSPKIXCertificatePolicies_Equal
(
  NSSPKIXCertificatePolicies *cp1,
  NSSPKIXCertificatePolicies *cp2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXCertificatePolicies_Duplicate
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
NSSPKIXCertificatePolicies_Duplicate
(
  NSSPKIXCertificatePolicies *cp,
  NSSArena *arenaOpt
);

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
 * The public calls for the type:
 *
 *  NSSPKIXPolicyInformation_Decode
 *  NSSPKIXPolicyInformation_Create
 *  NSSPKIXPolicyInformation_Destroy
 *  NSSPKIXPolicyInformation_Encode
 *  NSSPKIXPolicyInformation_GetPolicyIdentifier
 *  NSSPKIXPolicyInformation_SetPolicyIdentifier
 *  NSSPKIXPolicyInformation_GetPolicyQualifierCount
 *  NSSPKIXPolicyInformation_GetPolicyQualifiers
 *  NSSPKIXPolicyInformation_SetPolicyQualifiers
 *  NSSPKIXPolicyInformation_GetPolicyQualifier
 *  NSSPKIXPolicyInformation_SetPolicyQualifier
 *  NSSPKIXPolicyInformation_InsertPolicyQualifier
 *  NSSPKIXPolicyInformation_AppendPolicyQualifier
 *  NSSPKIXPolicyInformation_RemovePolicyQualifier
 *  NSSPKIXPolicyInformation_Equal
 *  NSSPKIXPolicyInformation_Duplicate
 *    { and accessors by specific policy qualifier ID }
 *
 */

/*
 * NSSPKIXPolicyInformation_Decode
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
NSSPKIXPolicyInformation_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPolicyInformation_Create
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
NSSPKIXPolicyInformation_Create
(
  NSSArena *arenaOpt,
  NSSPKIXCertPolicyId *id,
  NSSPKIXPolicyQualifierInfo *pqi1,
  ...
);

/*
 * NSSPKIXPolicyInformation_Destroy
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
NSSPKIXPolicyInformation_Destroy
(
  NSSPKIXPolicyInformation *pi
);

/*
 * NSSPKIXPolicyInformation_Encode
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
NSSPKIXPolicyInformation_Encode
(
  NSSPKIXPolicyInformation *pi,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyInformation_GetPolicyIdentifier
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
NSSPKIXPolicyInformation_GetPolicyIdentifier
(
  NSSPKIXPolicyInformation *pi
);

/*
 * NSSPKIXPolicyInformation_SetPolicyIdentifier
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
NSSPKIXPolicyInformation_SetPolicyIdentifier
(
  NSSPKIXPolicyInformation *pi,
  NSSPKIXCertPolicyIdentifier *cpi
);

/*
 * NSSPKIXPolicyInformation_GetPolicyQualifierCount
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
NSSPKIXPolicyInformation_GetPolicyQualifierCount
(
  NSSPKIXPolicyInformation *pi
);

/*
 * NSSPKIXPolicyInformation_GetPolicyQualifiers
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
NSSPKIXPolicyInformation_GetPolicyQualifiers
(
  NSSPKIXPolicyInformation *pi,
  NSSPKIXPolicyQualifierInfo *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyInformation_SetPolicyQualifiers
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
NSSPKIXPolicyInformation_SetPolicyQualifiers
(
  NSSPKIXPolicyInformation *pi,
  NSSPKIXPolicyQualifierInfo *pqi[],
  PRInt32 count
);

/*
 * NSSPKIXPolicyInformation_GetPolicyQualifier
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
NSSPKIXPolicyInformation_GetPolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyInformation_SetPolicyQualifier
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
NSSPKIXPolicyInformation_SetPolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i,
  NSSPKIXPolicyQualifierInfo *pqi
);

/*
 * NSSPKIXPolicyInformation_InsertPolicyQualifier
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
NSSPKIXPolicyInformation_InsertPolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i,
  NSSPKIXPolicyQualifierInfo *pqi
);

/*
 * NSSPKIXPolicyInformation_AppendPolicyQualifier
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
NSSPKIXPolicyInformation_AppendPolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i,
  NSSPKIXPolicyQualifierInfo *pqi
);

/*
 * NSSPKIXPolicyInformation_RemovePolicyQualifier
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
NSSPKIXPolicyInformation_RemovePolicyQualifier
(
  NSSPKIXPolicyInformation *pi,
  PRInt32 i
);

/*
 * NSSPKIXPolicyInformation_Equal
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
NSSPKIXPolicyInformation_Equal
(
  NSSPKIXPolicyInformation *pi1,
  NSSPKIXPolicyInformation *pi2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPolicyInformation_Duplicate
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
NSSPKIXPolicyInformation_Duplicate
(
  NSSPKIXPolicyInformation *pi,
  NSSArena *arenaOpt
);

/*
 *   { and accessors by specific policy qualifier ID }
 *
 */

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
 * The public calls for the type:
 *
 *  NSSPKIXPolicyQualifierInfo_Decode
 *  NSSPKIXPolicyQualifierInfo_Create
 *  NSSPKIXPolicyQualifierInfo_Destroy
 *  NSSPKIXPolicyQualifierInfo_Encode
 *  NSSPKIXPolicyQualifierInfo_GetPolicyQualifierID
 *  NSSPKIXPolicyQualifierInfo_SetPolicyQualifierID
 *  NSSPKIXPolicyQualifierInfo_GetQualifier
 *  NSSPKIXPolicyQualifierInfo_SetQualifier
 *  NSSPKIXPolicyQualifierInfo_Equal
 *  NSSPKIXPolicyQualifierInfo_Duplicate
 *    { and accessors by specific qualifier id/type }
 *
 */

/*
 * NSSPKIXPolicyQualifierInfo_Decode
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
NSSPKIXPolicyQualifierInfo_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPolicyQualifierInfo_Create
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
NSSPKIXPolicyQualifierInfo_Create
(
  NSSArena *arenaOpt,
  NSSPKIXPolicyQualifierId *policyQualifierId,
  NSSItem *qualifier
);

/*
 * NSSPKIXPolicyQualifierInfo_Destroy
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
NSSPKIXPolicyQualifierInfo_Destroy
(
  NSSPKIXPolicyQualifierInfo *pqi
);

/*
 * NSSPKIXPolicyQualifierInfo_Encode
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
NSSPKIXPolicyQualifierInfo_Encode
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyQualifierInfo_GetPolicyQualifierId
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
NSSPKIXPolicyQualifierInfo_GetPolicyQualifierId
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyQualifierInfo_SetPolicyQualifierId
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
NSSPKIXPolicyQualifierInfo_SetPolicyQualifierId
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSPKIXPolicyQualifierId *pqid
);

/*
 * NSSPKIXPolicyQualifierInfo_GetQualifier
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
NSSPKIXPolicyQualifierInfo_GetQualifier
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyQualifierInfo_SetQualifier
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
NSSPKIXPolicyQualifierInfo_SetQualifier
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSItem *qualifier
);

/*
 * NSSPKIXPolicyQualifierInfo_Equal
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
NSSPKIXPolicyQualifierInfo_Equal
(
  NSSPKIXPolicyQualifierInfo *pqi1,
  NSSPKIXPolicyQualifierInfo *pqi2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPolicyQualifierInfo_Duplicate
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
NSSPKIXPolicyQualifierInfo_Duplicate
(
  NSSPKIXPolicyQualifierInfo *pqi,
  NSSArena *arenaOpt
);

/*
 *   { and accessors by specific qualifier id/type }
 *
 */

/*
 * CPSuri
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CPSuri ::= IA5String
 *
 * The public calls for this type:
 *
 *  NSSPKIXCPSuri_Decode
 *  NSSPKIXCPSuri_CreateFromUTF8
 *  NSSPKIXCPSuri_Encode
 *
 */

/*
 * NSSPKIXCPSuri_Decode
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
NSSPKIXCPSuri_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXCPSuri_CreateFromUTF8
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
NSSPKIXCPSuri_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXCPSuri_Encode
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
NSSPKIXCPSuri_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXUserNotice_Decode
 *  NSSPKIXUserNotice_Create
 *  NSSPKIXUserNotice_Destroy
 *  NSSPKIXUserNotice_Encode
 *  NSSPKIXUserNotice_HasNoticeRef
 *  NSSPKIXUserNotice_GetNoticeRef
 *  NSSPKIXUserNotice_SetNoticeRef
 *  NSSPKIXUserNotice_RemoveNoticeRef
 *  NSSPKIXUserNotice_HasExplicitText
 *  NSSPKIXUserNotice_GetExplicitText
 *  NSSPKIXUserNotice_SetExplicitText
 *  NSSPKIXUserNotice_RemoveExplicitText
 *  NSSPKIXUserNotice_Equal
 *  NSSPKIXUserNotice_Duplicate
 *
 */


/*
 * NSSPKIXUserNotice_Decode
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
NSSPKIXUserNotice_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXUserNotice_Create
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
NSSPKIXUserNotice_Create
(
  NSSArena *arenaOpt,
  NSSPKIXNoticeReference *noticeRef,
  NSSPKIXDisplayText *explicitText
);

/*
 * NSSPKIXUserNotice_Destroy
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
NSSPKIXUserNotice_Destroy
(
  NSSPKIXUserNotice *userNotice
);

/*
 * NSSPKIXUserNotice_Encode
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
NSSPKIXUserNotice_Encode
(
  NSSPKIXUserNotice *userNotice,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXUserNotice_HasNoticeRef
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
NSSPKIXUserNotice_HasNoticeRef
(
  NSSPKIXUserNotice *userNotice,
  PRStatus *statusOpt
);

/*
 * NSSPKIXUserNotice_GetNoticeRef
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
NSSPKIXUserNotice_GetNoticeRef
(
  NSSPKIXUserNotice *userNotice,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXUserNotice_SetNoticeRef
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
NSSPKIXUserNotice_SetNoticeRef
(
  NSSPKIXUserNotice *userNotice,
  NSSPKIXNoticeReference *noticeRef
);

/*
 * NSSPKIXUserNotice_RemoveNoticeRef
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
NSSPKIXUserNotice_RemoveNoticeRef
(
  NSSPKIXUserNotice *userNotice
);

/*
 * NSSPKIXUserNotice_HasExplicitText
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
NSSPKIXUserNotice_HasExplicitText
(
  NSSPKIXUserNotice *userNotice,
  PRStatus *statusOpt
);

/*
 * NSSPKIXUserNotice_GetExplicitText
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
NSSPKIXUserNotice_GetExplicitText
(
  NSSPKIXUserNotice *userNotice,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXUserNotice_SetExplicitText
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
NSSPKIXUserNotice_SetExplicitText
(
  NSSPKIXUserNotice *userNotice,
  NSSPKIXDisplayText *explicitText
);

/*
 * NSSPKIXUserNotice_RemoveExplicitText
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
NSSPKIXUserNotice_RemoveExplicitText
(
  NSSPKIXUserNotice *userNotice
);

/*
 * NSSPKIXUserNotice_Equal
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
NSSPKIXUserNotice_Equal
(
  NSSPKIXUserNotice *userNotice1,
  NSSPKIXUserNotice *userNotice2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXUserNotice_Duplicate
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
NSSPKIXUserNotice_Duplicate
(
  NSSPKIXUserNotice *userNotice,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXNoticeReference_Decode
 *  NSSPKIXNoticeReference_Create
 *  NSSPKIXNoticeReference_Destroy
 *  NSSPKIXNoticeReference_Encode
 *  NSSPKIXNoticeReference_GetOrganization
 *  NSSPKIXNoticeReference_SetOrganization
 *  NSSPKIXNoticeReference_GetNoticeNumberCount
 *  NSSPKIXNoticeReference_GetNoticeNumbers
 *  NSSPKIXNoticeReference_SetNoticeNumbers
 *  NSSPKIXNoticeReference_GetNoticeNumber
 *  NSSPKIXNoticeReference_SetNoticeNumber
 *  NSSPKIXNoticeReference_InsertNoticeNumber
 *  NSSPKIXNoticeReference_AppendNoticeNumber
 *  NSSPKIXNoticeReference_RemoveNoticeNumber
 *    { maybe number-exists-p gettor? }
 *  NSSPKIXNoticeReference_Equal
 *  NSSPKIXNoticeReference_Duplicate
 *
 */

/*
 * NSSPKIXNoticeReference_Decode
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
NSSPKIXNoticeReference_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXNoticeReference_Create
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
NSSPKIXNoticeReference_Create
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
 * NSSPKIXNoticeReference_Destroy
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
NSSPKIXNoticeReference_Destroy
(
  NSSPKIXNoticeReference *nr
);

/*
 * NSSPKIXNoticeReference_Encode
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
NSSPKIXNoticeReference_Encode
(
  NSSPKIXNoticeReference *nr,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXNoticeReference_GetOrganization
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
NSSPKIXNoticeReference_GetOrganization
(
  NSSPKIXNoticeReference *nr,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXNoticeReference_SetOrganization
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
NSSPKIXNoticeReference_SetOrganization
(
  NSSPKIXNoticeReference *nr,
  NSSPKIXDisplayText *organization
);

/*
 * NSSPKIXNoticeReference_GetNoticeNumberCount
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
NSSPKIXNoticeReference_GetNoticeNumberCount
(
  NSSPKIXNoticeReference *nr
);

/*
 * NSSPKIXNoticeReference_GetNoticeNumbers
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
NSSPKIXNoticeReference_GetNoticeNumbers
(
  NSSPKIXNoticeReference *nr,
  PRInt32 rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXNoticeReference_SetNoticeNumbers
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
NSSPKIXNoticeReference_SetNoticeNumbers
(
  NSSPKIXNoticeReference *nr,
  PRInt32 noticeNumbers[],
  PRInt32 count
);

/*
 * NSSPKIXNoticeReference_GetNoticeNumber
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
NSSPKIXNoticeReference_GetNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 i,
  PRInt32 *noticeNumberP
);
  
/*
 * NSSPKIXNoticeReference_SetNoticeNumber
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
NSSPKIXNoticeReference_SetNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 i,
  PRInt32 noticeNumber
);

/*
 * NSSPKIXNoticeReference_InsertNoticeNumber
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
NSSPKIXNoticeReference_InsertNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 i,
  PRInt32 noticeNumber
);

/*
 * NSSPKIXNoticeReference_AppendNoticeNumber
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
NSSPKIXNoticeReference_AppendNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 noticeNumber
);

/*
 * NSSPKIXNoticeReference_RemoveNoticeNumber
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
NSSPKIXNoticeReference_RemoveNoticeNumber
(
  NSSPKIXNoticeReference *nr,
  PRInt32 i
);

/*
 *   { maybe number-exists-p gettor? }
 */

/*
 * NSSPKIXNoticeReference_Equal
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
NSSPKIXNoticeReference_Equal
(
  NSSPKIXNoticeReference *nr1,
  NSSPKIXNoticeReference *nr2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXNoticeReference_Duplicate
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
NSSPKIXNoticeReference_Duplicate
(
  NSSPKIXNoticeReference *nr,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXDisplayText_Decode
 *  NSSPKIXDisplayText_CreateFromUTF8
 *  NSSPKIXDisplayText_Encode
 *
 */

/*
 * NSSPKIXDisplayText_Decode
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
NSSPKIXDisplayText_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXDisplayText_CreateFromUTF8
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
NSSPKIXDisplayText_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXDisplayText_Encode
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
NSSPKIXDisplayText_Encode
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
 * The public calls for this type:
 *
 *  NSSPKIXPolicyMappings_Decode
 *  NSSPKIXPolicyMappings_Create
 *  NSSPKIXPolicyMappings_Destroy
 *  NSSPKIXPolicyMappings_Encode
 *  NSSPKIXPolicyMappings_GetPolicyMappingCount
 *  NSSPKIXPolicyMappings_GetPolicyMappings
 *  NSSPKIXPolicyMappings_SetPolicyMappings
 *  NSSPKIXPolicyMappings_GetPolicyMapping
 *  NSSPKIXPolicyMappings_SetPolicyMapping
 *  NSSPKIXPolicyMappings_InsertPolicyMapping
 *  NSSPKIXPolicyMappings_AppendPolicyMapping
 *  NSSPKIXPolicyMappings_RemovePolicyMapping
 *  NSSPKIXPolicyMappings_FindPolicyMapping
 *  NSSPKIXPolicyMappings_Equal
 *  NSSPKIXPolicyMappings_Duplicate
 *  NSSPKIXPolicyMappings_IssuerDomainPolicyExists
 *  NSSPKIXPolicyMappings_SubjectDomainPolicyExists
 *  NSSPKIXPolicyMappings_FindIssuerDomainPolicy
 *  NSSPKIXPolicyMappings_FindSubjectDomainPolicy
 *  NSSPKIXPolicyMappings_MapIssuerToSubject
 *  NSSPKIXPolicyMappings_MapSubjectToIssuer
 *    { find's and map's: what if there's more than one? }
 *
 */

/*
 * NSSPKIXPolicyMappings_Decode
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
NSSPKIXPolicyMappings_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPolicyMappings_Create
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
NSSPKIXPolicyMappings_Decode
(
  NSSArena *arenaOpt,
  NSSPKIXpolicyMapping *policyMapping1,
  ...
);

/*
 * NSSPKIXPolicyMappings_Destroy
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
NSSPKIXPolicyMappings_Destroy
(
  NSSPKIXPolicyMappings *policyMappings
);

/*
 * NSSPKIXPolicyMappings_Encode
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
NSSPKIXPolicyMappings_Encode
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyMappings_GetPolicyMappingCount
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
NSSPKIXPolicyMappings_GetPolicyMappingCount
(
  NSSPKIXPolicyMappings *policyMappings
);

/*
 * NSSPKIXPolicyMappings_GetPolicyMappings
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
NSSPKIXPolicyMappings_GetPolicyMappings
(
  NSSPKIXPolicyMappings *policyMappings
  NSSPKIXpolicyMapping *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyMappings_SetPolicyMappings
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
NSSPKIXPolicyMappings_SetPolicyMappings
(
  NSSPKIXPolicyMappings *policyMappings
  NSSPKIXpolicyMapping *policyMapping[]
  PRInt32 count
);

/*
 * NSSPKIXPolicyMappings_GetPolicyMapping
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
NSSPKIXPolicyMappings_GetPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyMappings_SetPolicyMapping
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
NSSPKIXPolicyMappings_SetPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  PRInt32 i,
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * NSSPKIXPolicyMappings_InsertPolicyMapping
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
NSSPKIXPolicyMappings_InsertPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  PRInt32 i,
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * NSSPKIXPolicyMappings_AppendPolicyMapping
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
NSSPKIXPolicyMappings_AppendPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * NSSPKIXPolicyMappings_RemovePolicyMapping
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
NSSPKIXPolicyMappings_RemovePolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  PRInt32 i
);

/*
 * NSSPKIXPolicyMappings_FindPolicyMapping
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
NSSPKIXPolicyMappings_FindPolicyMapping
(
  NSSPKIXPolicyMappings *policyMappings
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * NSSPKIXPolicyMappings_Equal
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
NSSPKIXPolicyMappings_Equal
(
  NSSPKIXPolicyMappings *policyMappings1,
  NSSPKIXpolicyMappings *policyMappings2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPolicyMappings_Duplicate
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
NSSPKIXPolicyMappings_Duplicate
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyMappings_IssuerDomainPolicyExists
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
NSSPKIXPolicyMappings_IssuerDomainPolicyExists
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *issuerDomainPolicy,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPolicyMappings_SubjectDomainPolicyExists
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
NSSPKIXPolicyMappings_SubjectDomainPolicyExists
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *subjectDomainPolicy,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPolicyMappings_FindIssuerDomainPolicy
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
NSSPKIXPolicyMappings_FindIssuerDomainPolicy
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *issuerDomainPolicy
);

/*
 * NSSPKIXPolicyMappings_FindSubjectDomainPolicy
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
NSSPKIXPolicyMappings_FindSubjectDomainPolicy
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *issuerDomainPolicy
);

/*
 * NSSPKIXPolicyMappings_MapIssuerToSubject
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
NSSPKIXPolicyMappings_MapIssuerToSubject
(
  NSSPKIXPolicyMappings *policyMappings,
  NSSPKIXCertPolicyId *issuerDomainPolicy
);

/*
 * NSSPKIXPolicyMappings_MapSubjectToIssuer
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
NSSPKIXPolicyMappings_MapSubjectToIssuer
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
 * The public calls for this type:
 *
 *  NSSPKIXpolicyMapping_Decode
 *  NSSPKIXpolicyMapping_Create
 *  NSSPKIXpolicyMapping_Destroy
 *  NSSPKIXpolicyMapping_Encode
 *  NSSPKIXpolicyMapping_GetIssuerDomainPolicy
 *  NSSPKIXpolicyMapping_SetIssuerDomainPolicy
 *  NSSPKIXpolicyMapping_GetSubjectDomainPolicy
 *  NSSPKIXpolicyMapping_SetSubjectDomainPolicy
 *  NSSPKIXpolicyMapping_Equal
 *  NSSPKIXpolicyMapping_Duplicate
 *
 */

/*
 * NSSPKIXpolicyMapping_Decode
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
NSSPKIXpolicyMapping_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXpolicyMapping_Create
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
NSSPKIXpolicyMapping_Create
(
  NSSArena *arenaOpt,
  NSSPKIXCertPolicyId *issuerDomainPolicy,
  NSSPKIXCertPolicyId *subjectDomainPolicy
);

/*
 * NSSPKIXpolicyMapping_Destroy
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
NSSPKIXpolicyMapping_Destroy
(
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * NSSPKIXpolicyMapping_Encode
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
NSSPKIXpolicyMapping_Encode
(
  NSSPKIXpolicyMapping *policyMapping,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXpolicyMapping_GetIssuerDomainPolicy
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
NSSPKIXpolicyMapping_GetIssuerDomainPolicy
(
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * NSSPKIXpolicyMapping_SetIssuerDomainPolicy
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
NSSPKIXpolicyMapping_SetIssuerDomainPolicy
(
  NSSPKIXpolicyMapping *policyMapping,
  NSSPKIXCertPolicyId *issuerDomainPolicy
);

/*
 * NSSPKIXpolicyMapping_GetSubjectDomainPolicy
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
NSSPKIXpolicyMapping_GetSubjectDomainPolicy
(
  NSSPKIXpolicyMapping *policyMapping
);

/*
 * NSSPKIXpolicyMapping_SetSubjectDomainPolicy
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
NSSPKIXpolicyMapping_SetSubjectDomainPolicy
(
  NSSPKIXpolicyMapping *policyMapping,
  NSSPKIXCertPolicyId *subjectDomainPolicy
);

/*
 * NSSPKIXpolicyMapping_Equal
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
NSSPKIXpolicyMapping_Equal
(
  NSSPKIXpolicyMapping *policyMapping1,
  NSSPKIXpolicyMapping *policyMapping2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXpolicyMapping_Duplicate
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
NSSPKIXpolicyMapping_Duplicate
(
  NSSPKIXpolicyMapping *policyMapping,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXGeneralName_Decode
 *  NSSPKIXGeneralName_CreateFromUTF8
 *  NSSPKIXGeneralName_Create
 *  NSSPKIXGeneralName_CreateFromOtherName
 *  NSSPKIXGeneralName_CreateFromRfc822Name
 *  NSSPKIXGeneralName_CreateFromDNSName
 *  NSSPKIXGeneralName_CreateFromX400Address
 *  NSSPKIXGeneralName_CreateFromDirectoryName
 *  NSSPKIXGeneralName_CreateFromEDIPartyName
 *  NSSPKIXGeneralName_CreateFromUniformResourceIdentifier
 *  NSSPKIXGeneralName_CreateFromIPAddress
 *  NSSPKIXGeneralName_CreateFromRegisteredID
 *  NSSPKIXGeneralName_Destroy
 *  NSSPKIXGeneralName_Encode
 *  NSSPKIXGeneralName_GetUTF8Encoding
 *  NSSPKIXGeneralName_GetChoice
 *  NSSPKIXGeneralName_GetOtherName
 *  NSSPKIXGeneralName_GetRfc822Name
 *  NSSPKIXGeneralName_GetDNSName
 *  NSSPKIXGeneralName_GetX400Address
 *  NSSPKIXGeneralName_GetDirectoryName
 *  NSSPKIXGeneralName_GetEDIPartyName
 *  NSSPKIXGeneralName_GetUniformResourceIdentifier
 *  NSSPKIXGeneralName_GetIPAddress
 *  NSSPKIXGeneralName_GetRegisteredID
 *  NSSPKIXGeneralName_GetSpecifiedChoice
 *  NSSPKIXGeneralName_Equal
 *  NSSPKIXGeneralName_Duplicate
 *  (in pki1 I had specific attribute value gettors here too)
 *
 */

/*
 * NSSPKIXGeneralName_Decode
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
NSSPKIXGeneralName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXGeneralName_CreateFromUTF8
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
NSSPKIXGeneralName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *utf8
);

/*
 * NSSPKIXGeneralName_Create
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
NSSPKIXGeneralName_Create
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralNameChoice choice,
  void *content
);

/*
 * NSSPKIXGeneralName_CreateFromOtherName
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
NSSPKIXGeneralName_CreateFromOtherName
(
  NSSArena *arenaOpt,
  NSSPKIXAnotherName *otherName
);

/*
 * NSSPKIXGeneralName_CreateFromRfc822Name
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
NSSPKIXGeneralName_CreateFromRfc822Name
(
  NSSArena *arenaOpt,
  NSSUTF8 *rfc822Name
);

/*
 * NSSPKIXGeneralName_CreateFromDNSName
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
NSSPKIXGeneralName_CreateFromDNSName
(
  NSSArena *arenaOpt,
  NSSUTF8 *dNSName
);

/*
 * NSSPKIXGeneralName_CreateFromX400Address
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
NSSPKIXGeneralName_CreateFromX400Address
(
  NSSArena *arenaOpt,
  NSSPKIXORAddress *x400Address
);

/*
 * NSSPKIXGeneralName_CreateFromDirectoryName
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
NSSPKIXGeneralName_CreateFromDirectoryName
(
  NSSArena *arenaOpt,
  NSSPKIXName *directoryName
);

/*
 * NSSPKIXGeneralName_CreateFromEDIPartyName
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
NSSPKIXGeneralName_CreateFromEDIPartyName
(
  NSSArena *arenaOpt,
  NSSPKIXEDIPartyName *ediPartyname
);

/*
 * NSSPKIXGeneralName_CreateFromUniformResourceIdentifier
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
NSSPKIXGeneralName_CreateFromUniformResourceIdentifier
(
  NSSArena *arenaOpt,
  NSSUTF8 *uniformResourceIdentifier
);

/*
 * NSSPKIXGeneralName_CreateFromIPAddress
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
NSSPKIXGeneralName_CreateFromIPAddress
(
  NSSArena *arenaOpt,
  NSSItem *iPAddress
);

/*
 * NSSPKIXGeneralName_CreateFromRegisteredID
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
NSSPKIXGeneralName_CreateFromRegisteredID
(
  NSSArena *arenaOpt,
  NSSOID *registeredID
);

/*
 * NSSPKIXGeneralName_Destroy
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
NSSPKIXGeneralName_Destroy
(
  NSSPKIXGeneralName *generalName
);

/*
 * NSSPKIXGeneralName_Encode
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
NSSPKIXGeneralName_Encode
(
  NSSPKIXGeneralName *generalName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetUTF8Encoding
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
NSSPKIXGeneralName_GetUTF8Encoding
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetChoice
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
NSSPKIXGeneralName_GetChoice
(
  NSSPKIXGeneralName *generalName
);

/*
 * NSSPKIXGeneralName_GetOtherName
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
NSSPKIXGeneralName_GetOtherName
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetRfc822Name
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
NSSPKIXGeneralName_GetRfc822Name
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetDNSName
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
NSSPKIXGeneralName_GetDNSName
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetX400Address
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
NSSPKIXGeneralName_GetX400Address
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetDirectoryName
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
NSSPKIXGeneralName_GetDirectoryName
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetEDIPartyName
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
NSSPKIXGeneralName_GetEDIPartyName
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetUniformResourceIdentifier
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
NSSPKIXGeneralName_GetUniformResourceIdentifier
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetIPAddress
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
NSSPKIXGeneralName_GetIPAddress
(
  NSSPKIXGeneralName *generalName,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_GetRegisteredID
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
NSSPKIXGeneralName_GetRegisteredID
(
  NSSPKIXGeneralName *generalName
);

/*
 * NSSPKIXGeneralName_GetSpecifiedChoice
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
NSSPKIXGeneralName_GetSpecifiedChoice
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralName_Equal
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
NSSPKIXGeneralName_Equal
(
  NSSPKIXGeneralName *generalName1,
  NSSPKIXGeneralName *generalName2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXGeneralName_Duplicate
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
NSSPKIXGeneralName_Duplicate
(
  NSSPKIXGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * (in pki1 I had specific attribute value gettors here too)
 *
 */

/*
 * GeneralNames
 *
 * This structure contains a sequence of GeneralName objects.
 *
 * From RFC 2459:
 *
 *  GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
 *
 * The public calls for this type:
 *
 *  NSSPKIXGeneralNames_Decode
 *  NSSPKIXGeneralNames_Create
 *  NSSPKIXGeneralNames_Destroy
 *  NSSPKIXGeneralNames_Encode
 *  NSSPKIXGeneralNames_GetGeneralNameCount
 *  NSSPKIXGeneralNames_GetGeneralNames
 *  NSSPKIXGeneralNames_SetGeneralNames
 *  NSSPKIXGeneralNames_GetGeneralName
 *  NSSPKIXGeneralNames_SetGeneralName
 *  NSSPKIXGeneralNames_InsertGeneralName
 *  NSSPKIXGeneralNames_AppendGeneralName
 *  NSSPKIXGeneralNames_RemoveGeneralName
 *  NSSPKIXGeneralNames_FindGeneralName
 *  NSSPKIXGeneralNames_Equal
 *  NSSPKIXGeneralNames_Duplicate
 *
 */

/*
 * NSSPKIXGeneralNames_Decode
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
NSSPKIXGeneralNames_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXGeneralNames_Create
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
NSSPKIXGeneralNames_Create
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralName *generalName1,
  ...
);

/*
 * NSSPKIXGeneralNames_Destroy
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
NSSPKIXGeneralNames_Destroy
(
  NSSPKIXGeneralNames *generalNames
);

/*
 * NSSPKIXGeneralNames_Encode
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
NSSPKIXGeneralNames_Encode
(
  NSSPKIXGeneralNames *generalNames,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralNames_GetGeneralNameCount
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
NSSPKIXGeneralNames_GetGeneralNameCount
(
  NSSPKIXGeneralNames *generalNames
);

/*
 * NSSPKIXGeneralNames_GetGeneralNames
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
NSSPKIXGeneralNames_GetGeneralNames
(
  NSSPKIXGeneralNames *generalNames,
  NSSPKIXGeneralName *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralNames_SetGeneralNames
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
NSSPKIXGeneralNames_SetGeneralNames
(
  NSSPKIXGeneralNames *generalNames,
  NSSPKIXGeneralName *generalName[],
  PRInt32 count
);

/*
 * NSSPKIXGeneralNames_GetGeneralName
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
NSSPKIXGeneralNames_GetGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralNames_SetGeneralName
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
NSSPKIXGeneralNames_SetGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  PRInt32 i,
  NSSPKIXGeneralName *generalName
);

/*
 * NSSPKIXGeneralNames_InsertGeneralName
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
NSSPKIXGeneralNames_InsertGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  PRInt32 i,
  NSSPKIXGeneralName *generalName
);

/*
 * NSSPKIXGeneralNames_AppendGeneralName
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
NSSPKIXGeneralNames_AppendGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  NSSPKIXGeneralName *generalName
);

/*
 * NSSPKIXGeneralNames_RemoveGeneralName
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
NSSPKIXGeneralNames_RemoveGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  PRInt32 i
);

/*
 * NSSPKIXGeneralNames_FindGeneralName
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
NSSPKIXGeneralNames_FindGeneralName
(
  NSSPKIXGeneralNames *generalNames,
  NSSPKIXGeneralName *generalName
);

/*
 * NSSPKIXGeneralNames_Equal
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
NSSPKIXGeneralNames_Equal
(
  NSSPKIXGeneralNames *generalNames1,
  NSSPKIXGeneralNames *generalNames2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXGeneralNames_Duplicate
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
NSSPKIXGeneralNames_Duplicate
(
  NSSPKIXGeneralNames *generalNames,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXAnotherName_Decode
 *  NSSPKIXAnotherName_Create
 *  NSSPKIXAnotherName_Destroy
 *  NSSPKIXAnotherName_Encode
 *  NSSPKIXAnotherName_GetTypeId
 *  NSSPKIXAnotherName_SetTypeId
 *  NSSPKIXAnotherName_GetValue
 *  NSSPKIXAnotherName_SetValue
 *  NSSPKIXAnotherName_Equal
 *  NSSPKIXAnotherName_Duplicate
 *
 */

/*
 * NSSPKIXAnotherName_Decode
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
NSSPKIXAnotherName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXAnotherName_Create
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
NSSPKIXAnotherName_Create
(
  NSSArena *arenaOpt,
  NSSOID *typeId,
  NSSItem *value
);

/*
 * NSSPKIXAnotherName_Destroy
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
NSSPKIXAnotherName_Destroy
(
  NSSPKIXAnotherName *anotherName
);

/*
 * NSSPKIXAnotherName_Encode
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
NSSPKIXAnotherName_Encode
(
  NSSPKIXAnotherName *anotherName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAnotherName_GetTypeId
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
NSSPKIXAnotherName_GetTypeId
(
  NSSPKIXAnotherName *anotherName
);

/*
 * NSSPKIXAnotherName_SetTypeId
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
NSSPKIXAnotherName_SetTypeId
(
  NSSPKIXAnotherName *anotherName,
  NSSOID *typeId
);

/*
 * NSSPKIXAnotherName_GetValue
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
NSSPKIXAnotherName_GetValue
(
  NSSPKIXAnotherName *anotherName,
  NSSItem *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAnotherName_SetValue
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
NSSPKIXAnotherName_SetValue
(
  NSSPKIXAnotherName *anotherName,
  NSSItem *value
);

/*
 * NSSPKIXAnotherName_Equal
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
NSSPKIXAnotherName_Equal
(
  NSSPKIXAnotherName *anotherName1,
  NSSPKIXAnotherName *anotherName2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXAnotherName_Duplicate
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
NSSPKIXAnotherName_Duplicate
(
  NSSPKIXAnotherName *anotherName,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXEDIPartyName_Decode
 *  NSSPKIXEDIPartyName_Create
 *  NSSPKIXEDIPartyName_Destroy
 *  NSSPKIXEDIPartyName_Encode
 *  NSSPKIXEDIPartyName_HasNameAssigner
 *  NSSPKIXEDIPartyName_GetNameAssigner
 *  NSSPKIXEDIPartyName_SetNameAssigner
 *  NSSPKIXEDIPartyName_RemoveNameAssigner
 *  NSSPKIXEDIPartyName_GetPartyName
 *  NSSPKIXEDIPartyName_SetPartyName
 *  NSSPKIXEDIPartyName_Equal
 *  NSSPKIXEDIPartyName_Duplicate
 *
 */

/*
 * NSSPKIXEDIPartyName_Decode
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
NSSPKIXEDIPartyName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXEDIPartyName_Create
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
NSSPKIXEDIPartyName_Create
(
  NSSArena *arenaOpt,
  NSSUTF8 *nameAssignerOpt,
  NSSUTF8 *partyName
);

/*
 * NSSPKIXEDIPartyName_Destroy
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
NSSPKIXEDIPartyName_Destroy
(
  NSSPKIXEDIPartyName *ediPartyName
);

/*
 * NSSPKIXEDIPartyName_Encode
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
NSSPKIXEDIPartyName_Encode
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXEDIPartyName_HasNameAssigner
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
NSSPKIXEDIPartyName_HasNameAssigner
(
  NSSPKIXEDIPartyName *ediPartyName
);

/*
 * NSSPKIXEDIPartyName_GetNameAssigner
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
NSSPKIXEDIPartyName_GetNameAssigner
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXEDIPartyName_SetNameAssigner
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
NSSPKIXEDIPartyName_SetNameAssigner
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSUTF8 *nameAssigner
);

/*
 * NSSPKIXEDIPartyName_RemoveNameAssigner
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
NSSPKIXEDIPartyName_RemoveNameAssigner
(
  NSSPKIXEDIPartyName *ediPartyName
);

/*
 * NSSPKIXEDIPartyName_GetPartyName
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
NSSPKIXEDIPartyName_GetPartyName
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXEDIPartyName_SetPartyName
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
NSSPKIXEDIPartyName_SetPartyName
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSUTF8 *partyName
);

/*
 * NSSPKIXEDIPartyName_Equal
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
NSSPKIXEDIPartyName_Equal
(
  NSSPKIXEDIPartyName *ediPartyName1,
  NSSPKIXEDIPartyName *ediPartyName2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXEDIPartyName_Duplicate
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
NSSPKIXEDIPartyName_Duplicate
(
  NSSPKIXEDIPartyName *ediPartyName,
  NSSArena *arenaOpt
);

/*
 * SubjectDirectoryAttributes
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  SubjectDirectoryAttributes ::= SEQUENCE SIZE (1..MAX) OF Attribute
 *
 * The public calls for this type:
 *
 *  NSSPKIXSubjectDirectoryAttributes_Decode
 *  NSSPKIXSubjectDirectoryAttributes_Create
 *  NSSPKIXSubjectDirectoryAttributes_Destroy
 *  NSSPKIXSubjectDirectoryAttributes_Encode
 *  NSSPKIXSubjectDirectoryAttributes_GetAttributeCount
 *  NSSPKIXSubjectDirectoryAttributes_GetAttributes
 *  NSSPKIXSubjectDirectoryAttributes_SetAttributes
 *  NSSPKIXSubjectDirectoryAttributes_GetAttribute
 *  NSSPKIXSubjectDirectoryAttributes_SetAttribute
 *  NSSPKIXSubjectDirectoryAttributes_InsertAttribute
 *  NSSPKIXSubjectDirectoryAttributes_AppendAttribute
 *  NSSPKIXSubjectDirectoryAttributes_RemoveAttribute
 *  NSSPKIXSubjectDirectoryAttributes_FindAttribute
 *  NSSPKIXSubjectDirectoryAttributes_Equal
 *  NSSPKIXSubjectDirectoryAttributes_Duplicate
 *
 */

/*
 * NSSPKIXSubjectDirectoryAttributes_Decode
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
NSSPKIXSubjectDirectoryAttributes_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXSubjectDirectoryAttributes_Create
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
NSSPKIXSubjectDirectoryAttributes_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAttribute *attribute1,
  ...
);

/*
 * NSSPKIXSubjectDirectoryAttributes_Destroy
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
NSSPKIXSubjectDirectoryAttributes_Destroy
(
  NSSPKIXSubjectDirectoryAttributes *sda
);

/*
 * NSSPKIXSubjectDirectoryAttributes_Encode
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
NSSPKIXSubjectDirectoryAttributes_Encode
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXSubjectDirectoryAttributes_GetAttributeCount
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
NSSPKIXSubjectDirectoryAttributes_GetAttributeCount
(
  NSSPKIXSubjectDirectoryAttributes *sda
);

/*
 * NSSPKIXSubjectDirectoryAttributes_GetAttributes
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
NSSPKIXSubjectDirectoryAttributes_GetAttributes
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSPKIXAttribute *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXSubjectDirectoryAttributes_SetAttributes
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
NSSPKIXSubjectDirectoryAttributes_SetAttributes
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSAttribute *attributes[],
  PRInt32 count
);

/*
 * NSSPKIXSubjectDirectoryAttributes_GetAttribute
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
NSSPKIXSubjectDirectoryAttributes_GetAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXSubjectDirectoryAttributes_SetAttribute
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
NSSPKIXSubjectDirectoryAttributes_SetAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  PRInt32 i,
  NSSPKIXAttribute *attribute
);

/*
 * NSSPKIXSubjectDirectoryAttributes_InsertAttribute
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
NSSPKIXSubjectDirectoryAttributes_InsertAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  PRInt32 i,
  NSSPKIXAttribute *attribute
);

/*
 * NSSPKIXSubjectDirectoryAttributes_AppendAttribute
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
NSSPKIXSubjectDirectoryAttributes_AppendAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSPKIXAttribute *attribute
);

/*
 * NSSPKIXSubjectDirectoryAttributes_RemoveAttribute
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
NSSPKIXSubjectDirectoryAttributes_RemoveAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  PRInt32 i
);

/*
 * NSSPKIXSubjectDirectoryAttributes_FindAttribute
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
NSSPKIXSubjectDirectoryAttributes_FindAttribute
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSPKIXAttribute *attribute
);

/*
 * NSSPKIXSubjectDirectoryAttributes_Equal
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
NSSPKIXSubjectDirectoryAttributes_Equal
(
  NSSPKIXSubjectDirectoryAttributes *sda1,
  NSSPKIXSubjectDirectoryAttributes *sda2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXSubjectDirectoryAttributes_Duplicate
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
NSSPKIXSubjectDirectoryAttributes_Duplicate
(
  NSSPKIXSubjectDirectoryAttributes *sda,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXBasicConstraints_Decode
 *  NSSPKIXBasicConstraints_Create
 *  NSSPKIXBasicConstraints_Destroy
 *  NSSPKIXBasicConstraints_Encode
 *  NSSPKIXBasicConstraints_GetCA
 *  NSSPKIXBasicConstraints_SetCA 
 *  NSSPKIXBasicConstraints_HasPathLenConstraint
 *  NSSPKIXBasicConstraints_GetPathLenConstraint
 *  NSSPKIXBasicConstraints_SetPathLenConstraint
 *  NSSPKIXBasicConstraints_RemovePathLenConstraint
 *  NSSPKIXBasicConstraints_Equal
 *  NSSPKIXBasicConstraints_Duplicate
 *  NSSPKIXBasicConstraints_CompareToPathLenConstraint
 *
 */

/*
 * NSSPKIXBasicConstraints_Decode
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
NSSPKIXBasicConstraints_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXBasicConstraints_Create
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
NSSPKIXBasicConstraints_Create
(
  NSSArena *arenaOpt,
  PRBool ca,
  PRInt32 pathLenConstraint
);

/*
 * NSSPKIXBasicConstraints_Destroy
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
NSSPKIXBasicConstraints_Destroy
(
  NSSPKIXBasicConstraints *basicConstraints
);

/*
 * NSSPKIXBasicConstraints_Encode
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
NSSPKIXBasicConstraints_Encode
(
  NSSPKIXBasicConstraints *basicConstraints,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBasicConstraints_GetCA
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
NSSPKIXBasicConstraints_GetCA
(
  NSSPKIXBasicConstraints *basicConstraints,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBasicConstraints_SetCA
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
NSSPKIXBasicConstraints_SetCA
(
  NSSPKIXBasicConstraints *basicConstraints,
  PRBool ca
);

/*
 * NSSPKIXBasicConstraints_HasPathLenConstraint
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
NSSPKIXBasicConstraints_HasPathLenConstraint
(
  NSSPKIXBasicConstraints *basicConstraints,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBasicConstraints_GetPathLenConstraint
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
NSSPKIXBasicConstraints_GetPathLenConstraint
(
  NSSPKIXBasicConstraints *basicConstraints
);

/*
 * NSSPKIXBasicConstraints_SetPathLenConstraint
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
NSSPKIXBasicConstraints_SetPathLenConstraint
(
  NSSPKIXBasicConstraints *basicConstraints,
  PRInt32 pathLenConstraint
);

/*
 * NSSPKIXBasicConstraints_RemovePathLenConstraint
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
NSSPKIXBasicConstraints_RemovePathLenConstraint
(
  NSSPKIXBasicConstraints *basicConstraints
);

/*
 * NSSPKIXBasicConstraints_Equal
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
NSSPKIXBasicConstraints_Equal
(
  NSSPKIXBasicConstraints *basicConstraints1,
  NSSPKIXBasicConstraints *basicConstraints2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXBasicConstraints_Duplicate
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
NSSPKIXBasicConstraints_Duplicate
(
  NSSPKIXBasicConstraints *basicConstraints,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXBasicConstraints_CompareToPathLenConstraint
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
NSSPKIXBasicConstraints_CompareToPathLenConstraint
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
 * The public calls for this type:
 *
 *  NSSPKIXNameConstraints_Decode
 *  NSSPKIXNameConstraints_Create
 *  NSSPKIXNameConstraints_Destroy
 *  NSSPKIXNameConstraints_Encode
 *  NSSPKIXNameConstraints_HasPermittedSubtrees
 *  NSSPKIXNameConstraints_GetPermittedSubtrees
 *  NSSPKIXNameConstraints_SetPermittedSubtrees
 *  NSSPKIXNameConstraints_RemovePermittedSubtrees
 *  NSSPKIXNameConstraints_HasExcludedSubtrees
 *  NSSPKIXNameConstraints_GetExcludedSubtrees
 *  NSSPKIXNameConstraints_SetExcludedSubtrees
 *  NSSPKIXNameConstraints_RemoveExcludedSubtrees
 *  NSSPKIXNameConstraints_Equal
 *  NSSPKIXNameConstraints_Duplicate
 *    { and the comparator functions }
 *
 */

/*
 * NSSPKIXNameConstraints_Decode
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
NSSPKIXNameConstraints_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXNameConstraints_Create
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
NSSPKIXNameConstraints_Create
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralSubtrees *permittedSubtrees,
  NSSPKIXGeneralSubtrees *excludedSubtrees
);

/*
 * NSSPKIXNameConstraints_Destroy
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
NSSPKIXNameConstraints_Destroy
(
  NSSPKIXNameConstraints *nameConstraints
);

/*
 * NSSPKIXNameConstraints_Encode
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
NSSPKIXNameConstraints_Encode
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXNameConstraints_HasPermittedSubtrees
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
NSSPKIXNameConstraints_HasPermittedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  PRStatus *statusOpt
);

/*
 * NSSPKIXNameConstraints_GetPermittedSubtrees
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
NSSPKIXNameConstraints_GetPermittedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXNameConstraints_SetPermittedSubtrees
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
NSSPKIXNameConstraints_SetPermittedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSPKIXGeneralSubtrees *permittedSubtrees
);

/*
 * NSSPKIXNameConstraints_RemovePermittedSubtrees
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
NSSPKIXNameConstraints_RemovePermittedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints
);

/*
 * NSSPKIXNameConstraints_HasExcludedSubtrees
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
NSSPKIXNameConstraints_HasExcludedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  PRStatus *statusOpt
);

/*
 * NSSPKIXNameConstraints_GetExcludedSubtrees
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
NSSPKIXNameConstraints_GetExcludedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXNameConstraints_SetExcludedSubtrees
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
NSSPKIXNameConstraints_SetExcludedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSPKIXGeneralSubtrees *excludedSubtrees
);

/*
 * NSSPKIXNameConstraints_RemoveExcludedSubtrees
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
NSSPKIXNameConstraints_RemoveExcludedSubtrees
(
  NSSPKIXNameConstraints *nameConstraints
);

/*
 * NSSPKIXNameConstraints_Equal
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
NSSPKIXNameConstraints_Equal
(
  NSSPKIXNameConstraints *nameConstraints1,
  NSSPKIXNameConstraints *nameConstraints2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXNameConstraints_Duplicate
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
NSSPKIXNameConstraints_Duplicate
(
  NSSPKIXNameConstraints *nameConstraints,
  NSSArena *arenaOpt
);

/*
 *   { and the comparator functions }
 *
 */

/*
 * GeneralSubtrees
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
 *
 * The public calls for this type:
 *
 *  NSSPKIXGeneralSubtrees_Decode
 *  NSSPKIXGeneralSubtrees_Create
 *  NSSPKIXGeneralSubtrees_Destroy
 *  NSSPKIXGeneralSubtrees_Encode
 *  NSSPKIXGeneralSubtrees_GetGeneralSubtreeCount
 *  NSSPKIXGeneralSubtrees_GetGeneralSubtrees
 *  NSSPKIXGeneralSubtrees_SetGeneralSubtrees
 *  NSSPKIXGeneralSubtrees_GetGeneralSubtree
 *  NSSPKIXGeneralSubtrees_SetGeneralSubtree
 *  NSSPKIXGeneralSubtrees_InsertGeneralSubtree
 *  NSSPKIXGeneralSubtrees_AppendGeneralSubtree
 *  NSSPKIXGeneralSubtrees_RemoveGeneralSubtree
 *  NSSPKIXGeneralSubtrees_FindGeneralSubtree
 *  NSSPKIXGeneralSubtrees_Equal
 *  NSSPKIXGeneralSubtrees_Duplicate
 *    { and finders and comparators }
 *
 */

/*
 * NSSPKIXGeneralSubtrees_Decode
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
NSSPKIXGeneralSubtrees_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXGeneralSubtrees_Create
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
NSSPKIXGeneralSubtrees_Create
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralSubtree *generalSubtree1,
  ...
);

/*
 * NSSPKIXGeneralSubtrees_Destroy
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
NSSPKIXGeneralSubtrees_Destroy
(
  NSSPKIXGeneralSubtrees *generalSubtrees
);

/*
 * NSSPKIXGeneralSubtrees_Encode
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
NSSPKIXGeneralSubtrees_Encode
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralSubtrees_GetGeneralSubtreeCount
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
NSSPKIXGeneralSubtrees_GetGeneralSubtreeCount
(
  NSSPKIXGeneralSubtrees *generalSubtrees
);

/*
 * NSSPKIXGeneralSubtrees_GetGeneralSubtrees
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
NSSPKIXGeneralSubtrees_GetGeneralSubtrees
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSPKIXGeneralSubtree *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralSubtrees_SetGeneralSubtrees
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
NSSPKIXGeneralSubtrees_SetGeneralSubtrees
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSPKIXGeneralSubtree *generalSubtree[],
  PRInt32 count
);

/*
 * NSSPKIXGeneralSubtrees_GetGeneralSubtree
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
NSSPKIXGeneralSubtrees_GetGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralSubtrees_SetGeneralSubtree
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
NSSPKIXGeneralSubtrees_SetGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  PRInt32 i,
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * NSSPKIXGeneralSubtrees_InsertGeneralSubtree
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
NSSPKIXGeneralSubtrees_InsertGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  PRInt32 i,
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * NSSPKIXGeneralSubtrees_AppendGeneralSubtree
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
NSSPKIXGeneralSubtrees_AppendGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * NSSPKIXGeneralSubtrees_RemoveGeneralSubtree
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
NSSPKIXGeneralSubtrees_RemoveGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  PRInt32 i
);

/*
 * NSSPKIXGeneralSubtrees_FindGeneralSubtree
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
NSSPKIXGeneralSubtrees_FindGeneralSubtree
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * NSSPKIXGeneralSubtrees_Equal
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
NSSPKIXGeneralSubtrees_Equal
(
  NSSPKIXGeneralSubtrees *generalSubtrees1,
  NSSPKIXGeneralSubtrees *generalSubtrees2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXGeneralSubtrees_Duplicate
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
NSSPKIXGeneralSubtrees_Duplicate
(
  NSSPKIXGeneralSubtrees *generalSubtrees,
  NSSArena *arenaOpt
);

/*
 *   { and finders and comparators }
 *
 */

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
 * The public calls for this type:
 *
 *  NSSPKIXGeneralSubtree_Decode
 *  NSSPKIXGeneralSubtree_Create
 *  NSSPKIXGeneralSubtree_Destroy
 *  NSSPKIXGeneralSubtree_Encode
 *  NSSPKIXGeneralSubtree_GetBase
 *  NSSPKIXGeneralSubtree_SetBase
 *  NSSPKIXGeneralSubtree_GetMinimum
 *  NSSPKIXGeneralSubtree_SetMinimum
 *  NSSPKIXGeneralSubtree_HasMaximum
 *  NSSPKIXGeneralSubtree_GetMaximum
 *  NSSPKIXGeneralSubtree_SetMaximum
 *  NSSPKIXGeneralSubtree_RemoveMaximum
 *  NSSPKIXGeneralSubtree_Equal
 *  NSSPKIXGeneralSubtree_Duplicate
 *  NSSPKIXGeneralSubtree_DistanceInRange
 *    {other tests and comparators}
 *
 */

/*
 * NSSPKIXGeneralSubtree_Decode
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
NSSPKIXGeneralSubtree_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXGeneralSubtree_Create
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
NSSPKIXGeneralSubtree_Create
(
  NSSArena *arenaOpt,
  NSSPKIXBaseDistance minimum,
  NSSPKIXBaseDistance maximumOpt
);

/*
 * NSSPKIXGeneralSubtree_Destroy
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
NSSPKIXGeneralSubtree_Destroy
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * NSSPKIXGeneralSubtree_Encode
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
NSSPKIXGeneralSubtree_Encode
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralSubtree_GetBase
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
NSSPKIXGeneralSubtree_GetBase
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralSubtree_SetBase
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
NSSPKIXGeneralSubtree_SetBase
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSPKIXGeneralName *base
);

/*
 * NSSPKIXGeneralSubtree_GetMinimum
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
NSSPKIXGeneralSubtree_GetMinimum
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * NSSPKIXGeneralSubtree_SetMinimum
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
NSSPKIXGeneralSubtree_SetMinimum
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSPKIXBaseDistance *minimum
);

/*
 * NSSPKIXGeneralSubtree_HasMaximum
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
NSSPKIXGeneralSubtree_HasMaximum
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * NSSPKIXGeneralSubtree_GetMaximum
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
NSSPKIXGeneralSubtree_GetMaximum
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * NSSPKIXGeneralSubtree_SetMaximum
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
NSSPKIXGeneralSubtree_SetMaximum
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSPKIXBaseDistance *maximum
);

/*
 * NSSPKIXGeneralSubtree_RemoveMaximum
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
NSSPKIXGeneralSubtree_RemoveMaximum
(
  NSSPKIXGeneralSubtree *generalSubtree
);

/*
 * NSSPKIXGeneralSubtree_Equal
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
NSSPKIXGeneralSubtree_Equal
(
  NSSPKIXGeneralSubtree *generalSubtree1,
  NSSPKIXGeneralSubtree *generalSubtree2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXGeneralSubtree_Duplicate
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
NSSPKIXGeneralSubtree_Duplicate
(
  NSSPKIXGeneralSubtree *generalSubtree,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXGeneralSubtree_DistanceInRange
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
NSSPKIXGeneralSubtree_DistanceInRange
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
 * The public calls for this type:
 *
 *  NSSPKIXPolicyConstraints_Decode
 *  NSSPKIXPolicyConstraints_Create
 *  NSSPKIXPolicyConstraints_Destroy
 *  NSSPKIXPolicyConstraints_Encode
 *  NSSPKIXPolicyConstraints_HasRequireExplicitPolicy
 *  NSSPKIXPolicyConstraints_GetRequireExplicitPolicy
 *  NSSPKIXPolicyConstraints_SetRequireExplicitPolicy
 *  NSSPKIXPolicyConstraints_RemoveRequireExplicitPolicy
 *  NSSPKIXPolicyConstraints_HasInhibitPolicyMapping
 *  NSSPKIXPolicyConstraints_GetInhibitPolicyMapping
 *  NSSPKIXPolicyConstraints_SetInhibitPolicyMapping
 *  NSSPKIXPolicyConstraints_RemoveInhibitPolicyMapping
 *  NSSPKIXPolicyConstraints_Equal
 *  NSSPKIXPolicyConstraints_Duplicate
 *
 */

/*
 * NSSPKIXPolicyConstraints_Decode
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
NSSPKIXPolicyConstraints_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXPolicyConstraints_Create
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
NSSPKIXPolicyConstraints_Create
(
  NSSArena *arenaOpt,
  NSSPKIXSkipCerts requireExplicitPolicy,
  NSSPKIXSkipCerts inhibitPolicyMapping
);

/*
 * NSSPKIXPolicyConstraints_Destroy
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
NSSPKIXPolicyConstraints_Destroy
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * NSSPKIXPolicyConstraints_Encode
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
NSSPKIXPolicyConstraints_Encode
(
  NSSPKIXPolicyConstraints *policyConstraints,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXPolicyConstraints_HasRequireExplicitPolicy
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
NSSPKIXPolicyConstraints_HasRequireExplicitPolicy
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * NSSPKIXPolicyConstraints_GetRequireExplicitPolicy
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
NSSPKIXPolicyConstraints_GetRequireExplicitPolicy
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * NSSPKIXPolicyConstraints_SetRequireExplicitPolicy
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
NSSPKIXPolicyConstraints_SetRequireExplicitPolicy
(
  NSSPKIXPolicyConstraints *policyConstraints,
  NSSPKIXSkipCerts requireExplicitPolicy
);

/*
 * NSSPKIXPolicyConstraints_RemoveRequireExplicitPolicy
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
NSSPKIXPolicyConstraints_RemoveRequireExplicitPolicy
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * NSSPKIXPolicyConstraints_HasInhibitPolicyMapping
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
NSSPKIXPolicyConstraints_HasInhibitPolicyMapping
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * NSSPKIXPolicyConstraints_GetInhibitPolicyMapping
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
NSSPKIXPolicyConstraints_GetInhibitPolicyMapping
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * NSSPKIXPolicyConstraints_SetInhibitPolicyMapping
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
NSSPKIXPolicyConstraints_SetInhibitPolicyMapping
(
  NSSPKIXPolicyConstraints *policyConstraints,
  NSSPKIXSkipCerts inhibitPolicyMapping
);

/*
 * NSSPKIXPolicyConstraints_RemoveInhibitPolicyMapping
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
NSSPKIXPolicyConstraints_RemoveInhibitPolicyMapping
(
  NSSPKIXPolicyConstraints *policyConstraints
);

/*
 * NSSPKIXPolicyConstraints_Equal
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
NSSPKIXPolicyConstraints_Equal
(
  NSSPKIXPolicyConstraints *policyConstraints1,
  NSSPKIXPolicyConstraints *policyConstraints2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXPolicyConstraints_Duplicate
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
NSSPKIXPolicyConstraints_Duplicate
(
  NSSPKIXPolicyConstraints *policyConstraints,
  NSSArena *arenaOpt
);

/*
 * CRLDistPointsSyntax
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  CRLDistPointsSyntax ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint
 *
 * The public calls for this type:
 *
 *  NSSPKIXCRLDistPointsSyntax_Decode
 *  NSSPKIXCRLDistPointsSyntax_Create
 *  NSSPKIXCRLDistPointsSyntax_Destroy
 *  NSSPKIXCRLDistPointsSyntax_Encode
 *  NSSPKIXCRLDistPointsSyntax_GetDistributionPointCount
 *  NSSPKIXCRLDistPointsSyntax_GetDistributionPoints
 *  NSSPKIXCRLDistPointsSyntax_SetDistributionPoints
 *  NSSPKIXCRLDistPointsSyntax_GetDistributionPoint
 *  NSSPKIXCRLDistPointsSyntax_SetDistributionPoint
 *  NSSPKIXCRLDistPointsSyntax_InsertDistributionPoint
 *  NSSPKIXCRLDistPointsSyntax_AppendDistributionPoint
 *  NSSPKIXCRLDistPointsSyntax_RemoveDistributionPoint
 *  NSSPKIXCRLDistPointsSyntax_FindDistributionPoint
 *  NSSPKIXCRLDistPointsSyntax_Equal
 *  NSSPKIXCRLDistPointsSyntax_Duplicate
 *
 */

/*
 * NSSPKIXCRLDistPointsSyntax_Decode
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
NSSPKIXCRLDistPointsSyntax_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXCRLDistPointsSyntax_Create
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
NSSPKIXCRLDistPointsSyntax_Create
(
  NSSArena *arenaOpt,
  NSSPKIXDistributionPoint *distributionPoint1,
  ...
);

/*
 * NSSPKIXCRLDistPointsSyntax_Destroy
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
NSSPKIXCRLDistPointsSyntax_Destroy
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax
);

/*
 * NSSPKIXCRLDistPointsSyntax_Encode
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
NSSPKIXCRLDistPointsSyntax_Encode
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCRLDistPointsSyntax_GetDistributionPointCount
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
NSSPKIXCRLDistPointsSyntax_GetDistributionPointCount
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax
);

/*
 * NSSPKIXCRLDistPointsSyntax_GetDistributionPoints
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
NSSPKIXCRLDistPointsSyntax_GetDistributionPoints
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSDistributionPoint *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCRLDistPointsSyntax_SetDistributionPoints
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
NSSPKIXCRLDistPointsSyntax_SetDistributionPoints
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSDistributionPoint *distributionPoint[]
  PRInt32 count
);

/*
 * NSSPKIXCRLDistPointsSyntax_GetDistributionPoint
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
NSSPKIXCRLDistPointsSyntax_GetDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXCRLDistPointsSyntax_SetDistributionPoint
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
NSSPKIXCRLDistPointsSyntax_SetDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  PRInt32 i,
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXCRLDistPointsSyntax_InsertDistributionPoint
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
NSSPKIXCRLDistPointsSyntax_InsertDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  PRInt32 i,
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXCRLDistPointsSyntax_AppendDistributionPoint
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
NSSPKIXCRLDistPointsSyntax_AppendDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXCRLDistPointsSyntax_RemoveDistributionPoint
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
NSSPKIXCRLDistPointsSyntax_RemoveDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  PRInt32 i
);

/*
 * NSSPKIXCRLDistPointsSyntax_FindDistributionPoint
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
NSSPKIXCRLDistPointsSyntax_FindDistributionPoint
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXCRLDistPointsSyntax_Equal
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
NSSPKIXCRLDistPointsSyntax_Equal
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax1,
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXCRLDistPointsSyntax_Duplicate
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
NSSPKIXCRLDistPointsSyntax_Duplicate
(
  NSSPKIXCRLDistPointsSyntax *crlDistPointsSyntax,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXDistributionPoint_Decode
 *  NSSPKIXDistributionPoint_Create
 *  NSSPKIXDistributionPoint_Destroy
 *  NSSPKIXDistributionPoint_Encode
 *  NSSPKIXDistributionPoint_HasDistributionPoint
 *  NSSPKIXDistributionPoint_GetDistributionPoint
 *  NSSPKIXDistributionPoint_SetDistributionPoint
 *  NSSPKIXDistributionPoint_RemoveDistributionPoint
 *  NSSPKIXDistributionPoint_HasReasons
 *  NSSPKIXDistributionPoint_GetReasons
 *  NSSPKIXDistributionPoint_SetReasons
 *  NSSPKIXDistributionPoint_RemoveReasons
 *  NSSPKIXDistributionPoint_HasCRLIssuer
 *  NSSPKIXDistributionPoint_GetCRLIssuer
 *  NSSPKIXDistributionPoint_SetCRLIssuer
 *  NSSPKIXDistributionPoint_RemoveCRLIssuer
 *  NSSPKIXDistributionPoint_Equal
 *  NSSPKIXDistributionPoint_Duplicate
 *
 */

/*
 * NSSPKIXDistributionPoint_Decode
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
NSSPKIXDistributionPoint_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXDistributionPoint_Create
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
NSSPKIXDistributionPoint_Create
(
  NSSArena *arenaOpt,
  NSSPKIXDistributionPointName *distributionPoint,
  NSSPKIXReasonFlags reasons,
  NSSPKIXGeneralNames *cRLIssuer
);

/*
 * NSSPKIXDistributionPoint_Destroy
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
NSSPKIXDistributionPoint_Destroy
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXDistributionPoint_Encode
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
NSSPKIXDistributionPoint_Encode
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXDistributionPoint_HasDistributionPoint
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
NSSPKIXDistributionPoint_HasDistributionPoint
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXDistributionPoint_GetDistributionPoint
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
NSSPKIXDistributionPoint_GetDistributionPoint
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXDistributionPoint_SetDistributionPoint
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
NSSPKIXDistributionPoint_SetDistributionPoint
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSPKIXDistributionPointName *name
);

/*
 * NSSPKIXDistributionPoint_RemoveDistributionPoint
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
NSSPKIXDistributionPoint_RemoveDistributionPoint
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXDistributionPoint_HasReasons
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
NSSPKIXDistributionPoint_HasReasons
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXDistributionPoint_GetReasons
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
NSSPKIXDistributionPoint_GetReasons
(
  NSSPKIXDistributionPoint *distributionPoint,
  PRStatus *statusOpt
);

/*
 * NSSPKIXDistributionPoint_SetReasons
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
NSSPKIXDistributionPoint_SetReasons
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSPKIXReasonFlags reasons
);

/*
 * NSSPKIXDistributionPoint_RemoveReasons
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
NSSPKIXDistributionPoint_RemoveReasons
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXDistributionPoint_HasCRLIssuer
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
NSSPKIXDistributionPoint_HasCRLIssuer
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXDistributionPoint_GetCRLIssuer
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
NSSPKIXDistributionPoint_GetCRLIssuer
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXDistributionPoint_SetCRLIssuer
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
NSSPKIXDistributionPoint_SetCRLIssuer
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSPKIXGeneralNames *cRLIssuer
);

/*
 * NSSPKIXDistributionPoint_RemoveCRLIssuer
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
NSSPKIXDistributionPoint_RemoveCRLIssuer
(
  NSSPKIXDistributionPoint *distributionPoint
);

/*
 * NSSPKIXDistributionPoint_Equal
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
NSSPKIXDistributionPoint_Equal
(
  NSSPKIXDistributionPoint *distributionPoint1,
  NSSPKIXDistributionPoint *distributionPoint2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXDistributionPoint_Duplicate
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
NSSPKIXDistributionPoint_Duplicate
(
  NSSPKIXDistributionPoint *distributionPoint,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXDistributionPointName_Decode
 *  NSSPKIXDistributionPointName_Create
 *  NSSPKIXDistributionPointName_CreateFromFullName
 *  NSSPKIXDistributionPointName_CreateFromNameRelativeToCRLIssuer
 *  NSSPKIXDistributionPointName_Destroy
 *  NSSPKIXDistributionPointName_Encode
 *  NSSPKIXDistributionPointName_GetChoice
 *  NSSPKIXDistributionPointName_GetFullName
 *  NSSPKIXDistributionPointName_GetNameRelativeToCRLIssuer
 *  NSSPKIXDistributionPointName_Equal
 *  NSSPKIXDistributionPointName_Duplicate
 *
 */

/*
 * NSSPKIXDistributionPointName_Decode
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
NSSPKIXDistributionPointName_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXDistributionPointName_Create
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
NSSPKIXDistributionPointName_Create
(
  NSSArena *arenaOpt,
  NSSPKIXDistributionPointNameChoice which,
  void *name
);

/*
 * NSSPKIXDistributionPointName_CreateFromFullName
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
NSSPKIXDistributionPointName_CreateFromFullName
(
  NSSArena *arenaOpt,
  NSSPKIXGeneralNames *fullName
);

/*
 * NSSPKIXDistributionPointName_CreateFromNameRelativeToCRLIssuer
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
NSSPKIXDistributionPointName_CreateFromNameRelativeToCRLIssuer
(
  NSSArena *arenaOpt,
  NSSPKIXRelativeDistinguishedName *nameRelativeToCRLIssuer
);

/*
 * NSSPKIXDistributionPointName_Destroy
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
NSSPKIXDistributionPointName_Destroy
(
  NSSPKIXDistributionPointName *dpn
);

/*
 * NSSPKIXDistributionPointName_Encode
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
NSSPKIXDistributionPointName_Encode
(
  NSSPKIXDistributionPointName *dpn,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXDistributionPointName_GetChoice
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
NSSPKIXDistributionPointName_GetChoice
(
  NSSPKIXDistributionPointName *dpn
);

/*
 * NSSPKIXDistributionPointName_GetFullName
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
NSSPKIXDistributionPointName_GetFullName
(
  NSSPKIXDistributionPointName *dpn,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXDistributionPointName_GetNameRelativeToCRLIssuer
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
NSSPKIXDistributionPointName_GetNameRelativeToCRLIssuer
(
  NSSPKIXDistributionPointName *dpn,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXDistributionPointName_Equal
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
NSSPKIXDistributionPointName_Equal
(
  NSSPKIXDistributionPointName *dpn1,
  NSSPKIXDistributionPointName *dpn2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXDistributionPointName_Duplicate
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
NSSPKIXDistributionPointName_Duplicate
(
  NSSPKIXDistributionPointName *dpn,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXReasonFlags_Decode
 *  NSSPKIXReasonFlags_Create
 *  NSSPKIXReasonFlags_CreateFromMask
 *  NSSPKIXReasonFlags_Destroy
 *  NSSPKIXReasonFlags_Encode
 *  NSSPKIXReasonFlags_GetMask
 *  NSSPKIXReasonFlags_SetMask
 *  NSSPKIXReasonFlags_Equal
 *  NSSPKIXReasonFlags_Duplicate
 *    { bitwise accessors? }
 *
 */

/*
 * NSSPKIXReasonFlags_Decode
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
NSSPKIXReasonFlags_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXReasonFlags_Create
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
NSSPKIXReasonFlags_Create
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
 * NSSPKIXReasonFlags_CreateFromMask
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
NSSPKIXReasonFlags_CreateFromMask
(
  NSSArena *arenaOpt,
  NSSPKIXReasonFlagsMask why
);

/*
 * NSSPKIXReasonFlags_Destroy
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
NSSPKIXReasonFlags_Destroy
(
  NSSPKIXReasonFlags *reasonFlags
);

/*
 * NSSPKIXReasonFlags_Encode
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
NSSPKIXReasonFlags_Encode
(
  NSSPKIXReasonFlags *reasonFlags,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXReasonFlags_GetMask
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
NSSPKIXReasonFlags_GetMask
(
  NSSPKIXReasonFlags *reasonFlags
);

/*
 * NSSPKIXReasonFlags_SetMask
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
NSSPKIXReasonFlags_SetMask
(
  NSSPKIXReasonFlags *reasonFlags,
  NSSPKIXReasonFlagsMask mask
);

/*
 * NSSPKIXReasonFlags_Equal
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
NSSPKIXReasonFlags_Equal
(
  NSSPKIXReasonFlags *reasonFlags1,
  NSSPKIXReasonFlags *reasonFlags2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXReasonFlags_Duplicate
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
NSSPKIXReasonFlags_Duplicate
(
  NSSPKIXReasonFlags *reasonFlags,
  NSSArena *arenaOpt
);

/*
 *   { bitwise accessors? }
 *
 */

/*
 * ExtKeyUsageSyntax
 *
 * -- fgmr comments --
 *
 * From RFC 2459:
 *
 *  ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
 *
 * The public calls for this type:
 *
 *  NSSPKIXExtKeyUsageSyntax_Decode
 *  NSSPKIXExtKeyUsageSyntax_Create
 *  NSSPKIXExtKeyUsageSyntax_Destroy
 *  NSSPKIXExtKeyUsageSyntax_Encode
 *  NSSPKIXExtKeyUsageSyntax_GetKeyPurposeIdCount
 *  NSSPKIXExtKeyUsageSyntax_GetKeyPurposeIds
 *  NSSPKIXExtKeyUsageSyntax_SetKeyPurposeIds
 *  NSSPKIXExtKeyUsageSyntax_GetKeyPurposeId
 *  NSSPKIXExtKeyUsageSyntax_SetKeyPurposeId
 *  NSSPKIXExtKeyUsageSyntax_InsertKeyPurposeId
 *  NSSPKIXExtKeyUsageSyntax_AppendKeyPurposeId
 *  NSSPKIXExtKeyUsageSyntax_RemoveKeyPurposeId
 *  NSSPKIXExtKeyUsageSyntax_FindKeyPurposeId
 *  NSSPKIXExtKeyUsageSyntax_Equal
 *  NSSPKIXExtKeyUsageSyntax_Duplicate
 *
 */

/*
 * NSSPKIXExtKeyUsageSyntax_Decode
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
NSSPKIXExtKeyUsageSyntax_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXExtKeyUsageSyntax_Create
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
NSSPKIXExtKeyUsageSyntax_Create
(
  NSSArena *arenaOpt,
  NSSPKIXKeyPurposeId *kpid1,
  ...
);

/*
 * NSSPKIXExtKeyUsageSyntax_Destroy
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
NSSPKIXExtKeyUsageSyntax_Destroy
(
  NSSPKIXExtKeyUsageSyntax *eku
);

/*
 * NSSPKIXExtKeyUsageSyntax_Encode
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
NSSPKIXExtKeyUsageSyntax_Encode
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtKeyUsageSyntax_GetKeyPurposeIdCount
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
NSSPKIXExtKeyUsageSyntax_GetKeyPurposeIdCount
(
  NSSPKIXExtKeyUsageSyntax *eku
);

/*
 * NSSPKIXExtKeyUsageSyntax_GetKeyPurposeIds
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
NSSPKIXExtKeyUsageSyntax_GetKeyPurposeIds
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSPKIXKeyPurposeId *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtKeyUsageSyntax_SetKeyPurposeIds
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
NSSPKIXExtKeyUsageSyntax_SetKeyPurposeIds
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSPKIXKeyPurposeId *ids[],
  PRInt32 count
);

/*
 * NSSPKIXExtKeyUsageSyntax_GetKeyPurposeId
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
NSSPKIXExtKeyUsageSyntax_GetKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXExtKeyUsageSyntax_SetKeyPurposeId
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
NSSPKIXExtKeyUsageSyntax_SetKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  PRInt32 i,
  NSSPKIXKeyPurposeId *id
);

/*
 * NSSPKIXExtKeyUsageSyntax_InsertKeyPurposeId
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
NSSPKIXExtKeyUsageSyntax_InsertKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  PRInt32 i,
  NSSPKIXKeyPurposeId *id
);

/*
 * NSSPKIXExtKeyUsageSyntax_AppendKeyPurposeId
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
NSSPKIXExtKeyUsageSyntax_AppendKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSPKIXKeyPurposeId *id
);

/*
 * NSSPKIXExtKeyUsageSyntax_RemoveKeyPurposeId
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
NSSPKIXExtKeyUsageSyntax_RemoveKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  PRInt32 i
);

/*
 * NSSPKIXExtKeyUsageSyntax_FindKeyPurposeId
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
NSSPKIXExtKeyUsageSyntax_FindKeyPurposeId
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSPKIXKeyPurposeId *id
);

/*
 * NSSPKIXExtKeyUsageSyntax_Equal
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
NSSPKIXExtKeyUsageSyntax_Equal
(
  NSSPKIXExtKeyUsageSyntax *eku1,
  NSSPKIXExtKeyUsageSyntax *eku2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXExtKeyUsageSyntax_Duplicate
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
NSSPKIXExtKeyUsageSyntax_Duplicate
(
  NSSPKIXExtKeyUsageSyntax *eku,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXAuthorityInfoAccessSyntax_Decode
 *  NSSPKIXAuthorityInfoAccessSyntax_Create
 *  NSSPKIXAuthorityInfoAccessSyntax_Destroy
 *  NSSPKIXAuthorityInfoAccessSyntax_Encode
 *  NSSPKIXAuthorityInfoAccessSyntax_GetAccessDescriptionCount
 *  NSSPKIXAuthorityInfoAccessSyntax_GetAccessDescriptions
 *  NSSPKIXAuthorityInfoAccessSyntax_SetAccessDescriptions
 *  NSSPKIXAuthorityInfoAccessSyntax_GetAccessDescription
 *  NSSPKIXAuthorityInfoAccessSyntax_SetAccessDescription
 *  NSSPKIXAuthorityInfoAccessSyntax_InsertAccessDescription
 *  NSSPKIXAuthorityInfoAccessSyntax_AppendAccessDescription
 *  NSSPKIXAuthorityInfoAccessSyntax_RemoveAccessDescription
 *  NSSPKIXAuthorityInfoAccessSyntax_FindAccessDescription
 *  NSSPKIXAuthorityInfoAccessSyntax_Equal
 *  NSSPKIXAuthorityInfoAccessSyntax_Duplicate
 *
 */

/*
 * NSSPKIXAuthorityInfoAccessSyntax_Decode
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
NSSPKIXAuthorityInfoAccessSyntax_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_Create
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
NSSPKIXAuthorityInfoAccessSyntax_Create
(
  NSSArena *arenaOpt,
  NSSPKIXAccessDescription *ad1,
  ...
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_Destroy
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
NSSPKIXAuthorityInfoAccessSyntax_Destroy
(
  NSSPKIXAuthorityInfoAccessSyntax *aias
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_Encode
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
NSSPKIXAuthorityInfoAccessSyntax_Encode
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_GetAccessDescriptionCount
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
NSSPKIXAuthorityInfoAccessSyntax_GetAccessDescriptionCount
(
  NSSPKIXAuthorityInfoAccessSyntax *aias
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_GetAccessDescriptions
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
NSSPKIXAuthorityInfoAccessSyntax_GetAccessDescriptions
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSPKIXAccessDescription *rvOpt[],
  PRInt32 limit,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_SetAccessDescriptions
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
NSSPKIXAuthorityInfoAccessSyntax_SetAccessDescriptions
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSPKIXAccessDescription *ad[],
  PRInt32 count
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_GetAccessDescription
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
NSSPKIXAuthorityInfoAccessSyntax_GetAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  PRInt32 i,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_SetAccessDescription
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
NSSPKIXAuthorityInfoAccessSyntax_SetAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  PRInt32 i,
  NSSPKIXAccessDescription *ad
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_InsertAccessDescription
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
NSSPKIXAuthorityInfoAccessSyntax_InsertAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  PRInt32 i,
  NSSPKIXAccessDescription *ad
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_AppendAccessDescription
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
NSSPKIXAuthorityInfoAccessSyntax_AppendAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSPKIXAccessDescription *ad
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_RemoveAccessDescription
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
NSSPKIXAuthorityInfoAccessSyntax_RemoveAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  PRInt32 i
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_FindAccessDescription
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
NSSPKIXAuthorityInfoAccessSyntax_FindAccessDescription
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSPKIXAccessDescription *ad
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_Equal
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
NSSPKIXAuthorityInfoAccessSyntax_Equal
(
  NSSPKIXAuthorityInfoAccessSyntax *aias1,
  NSSPKIXAuthorityInfoAccessSyntax *aias2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXAuthorityInfoAccessSyntax_Duplicate
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
NSSPKIXAuthorityInfoAccessSyntax_Duplicate
(
  NSSPKIXAuthorityInfoAccessSyntax *aias,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXAccessDescription_Decode
 *  NSSPKIXAccessDescription_Create
 *  NSSPKIXAccessDescription_Destroy
 *  NSSPKIXAccessDescription_Encode
 *  NSSPKIXAccessDescription_GetAccessMethod
 *  NSSPKIXAccessDescription_SetAccessMethod
 *  NSSPKIXAccessDescription_GetAccessLocation
 *  NSSPKIXAccessDescription_SetAccessLocation
 *  NSSPKIXAccessDescription_Equal
 *  NSSPKIXAccessDescription_Duplicate
 *
 */

/*
 * NSSPKIXAccessDescription_Decode
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
NSSPKIXAccessDescription_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXAccessDescription_Create
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
NSSPKIXAccessDescription_Create
(
  NSSArena *arenaOpt,
  NSSOID *accessMethod,
  NSSPKIXGeneralName *accessLocation
);

/*
 * NSSPKIXAccessDescription_Destroy
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
NSSPKIXAccessDescription_Destroy
(
  NSSPKIXAccessDescription *ad
);

/*
 * NSSPKIXAccessDescription_Encode
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
NSSPKIXAccessDescription_Encode
(
  NSSPKIXAccessDescription *ad,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAccessDescription_GetAccessMethod
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
NSSPKIXAccessDescription_GetAccessMethod
(
  NSSPKIXAccessDescription *ad
);

/*
 * NSSPKIXAccessDescription_SetAccessMethod
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
NSSPKIXAccessDescription_SetAccessMethod
(
  NSSPKIXAccessDescription *ad,
  NSSOID *accessMethod
);

/*
 * NSSPKIXAccessDescription_GetAccessLocation
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
NSSPKIXAccessDescription_GetAccessLocation
(
  NSSPKIXAccessDescription *ad,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXAccessDescription_SetAccessLocation
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
NSSPKIXAccessDescription_SetAccessLocation
(
  NSSPKIXAccessDescription *ad,
  NSSPKIXGeneralName *accessLocation
);

/*
 * NSSPKIXAccessDescription_Equal
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
NSSPKIXAccessDescription_Equal
(
  NSSPKIXAccessDescription *ad1,
  NSSPKIXAccessDescription *ad2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXAccessDescription_Duplicate
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
NSSPKIXAccessDescription_Duplicate
(
  NSSPKIXAccessDescription *ad,
  NSSArena *arenaOpt
);

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
 * The public calls for this type:
 *
 *  NSSPKIXIssuingDistributionPoint_Decode
 *  NSSPKIXIssuingDistributionPoint_Create
 *  NSSPKIXIssuingDistributionPoint_Destroy
 *  NSSPKIXIssuingDistributionPoint_Encode
 *  NSSPKIXIssuingDistributionPoint_HasDistributionPoint
 *  NSSPKIXIssuingDistributionPoint_GetDistributionPoint
 *  NSSPKIXIssuingDistributionPoint_SetDistributionPoint
 *  NSSPKIXIssuingDistributionPoint_RemoveDistributionPoint
 *  NSSPKIXIssuingDistributionPoint_GetOnlyContainsUserCerts
 *  NSSPKIXIssuingDistributionPoint_SetOnlyContainsUserCerts
 *  NSSPKIXIssuingDistributionPoint_GetOnlyContainsCACerts
 *  NSSPKIXIssuingDistributionPoint_SetOnlyContainsCACerts
 *  NSSPKIXIssuingDistributionPoint_HasOnlySomeReasons
 *  NSSPKIXIssuingDistributionPoint_GetOnlySomeReasons
 *  NSSPKIXIssuingDistributionPoint_SetOnlySomeReasons
 *  NSSPKIXIssuingDistributionPoint_RemoveOnlySomeReasons
 *  NSSPKIXIssuingDistributionPoint_GetIndirectCRL
 *  NSSPKIXIssuingDistributionPoint_SetIndirectCRL
 *  NSSPKIXIssuingDistributionPoint_Equal
 *  NSSPKIXIssuingDistributionPoint_Duplicate
 *
 */

/*
 * NSSPKIXIssuingDistributionPoint_Decode
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
NSSPKIXIssuingDistributionPoint_Decode
(
  NSSArena *arenaOpt,
  NSSBER *ber
);

/*
 * NSSPKIXIssuingDistributionPoint_Create
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
NSSPKIXIssuingDistributionPoint_Create
(
  NSSArena *arenaOpt,
  NSSPKIXDistributionPointName *distributionPointOpt,
  PRBool onlyContainsUserCerts,
  PRBool onlyContainsCACerts,
  NSSPKIXReasonFlags *onlySomeReasons
  PRBool indirectCRL
);

/*
 * NSSPKIXIssuingDistributionPoint_Destroy
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
NSSPKIXIssuingDistributionPoint_Destroy
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * NSSPKIXIssuingDistributionPoint_Encode
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
NSSPKIXIssuingDistributionPoint_Encode
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSASN1EncodingType encoding,
  NSSBER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXIssuingDistributionPoint_HasDistributionPoint
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
NSSPKIXIssuingDistributionPoint_HasDistributionPoint
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * NSSPKIXIssuingDistributionPoint_GetDistributionPoint
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
NSSPKIXIssuingDistributionPoint_GetDistributionPoint
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXIssuingDistributionPoint_SetDistributionPoint
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
NSSPKIXIssuingDistributionPoint_SetDistributionPoint
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSPKIXDistributionPointName *dpn
);

/*
 * NSSPKIXIssuingDistributionPoint_RemoveDistributionPoint
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
NSSPKIXIssuingDistributionPoint_RemoveDistributionPoint
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * NSSPKIXIssuingDistributionPoint_GetOnlyContainsUserCerts
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
NSSPKIXIssuingDistributionPoint_GetOnlyContainsUserCerts
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRStatus *statusOpt
);

/*
 * NSSPKIXIssuingDistributionPoint_SetOnlyContainsUserCerts
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
NSSPKIXIssuingDistributionPoint_SetOnlyContainsUserCerts
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRBool onlyContainsUserCerts
);

/*
 * NSSPKIXIssuingDistributionPoint_GetOnlyContainsCACerts
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
NSSPKIXIssuingDistributionPoint_GetOnlyContainsCACerts
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRStatus *statusOpt
);

/*
 * NSSPKIXIssuingDistributionPoint_SetOnlyContainsCACerts
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
NSSPKIXIssuingDistributionPoint_SetOnlyContainsCACerts
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRBool onlyContainsCACerts
);

/*
 * NSSPKIXIssuingDistributionPoint_HasOnlySomeReasons
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
NSSPKIXIssuingDistributionPoint_HasOnlySomeReasons
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * NSSPKIXIssuingDistributionPoint_GetOnlySomeReasons
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
NSSPKIXIssuingDistributionPoint_GetOnlySomeReasons
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSArena *arenaOpt
);

/*
 * NSSPKIXIssuingDistributionPoint_SetOnlySomeReasons
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
NSSPKIXIssuingDistributionPoint_SetOnlySomeReasons
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSPKIXReasonFlags *onlySomeReasons
);

/*
 * NSSPKIXIssuingDistributionPoint_RemoveOnlySomeReasons
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
NSSPKIXIssuingDistributionPoint_RemoveOnlySomeReasons
(
  NSSPKIXIssuingDistributionPoint *idp
);

/*
 * NSSPKIXIssuingDistributionPoint_GetIndirectCRL
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
NSSPKIXIssuingDistributionPoint_GetIndirectCRL
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRStatus *statusOpt
);

/*
 * NSSPKIXIssuingDistributionPoint_SetIndirectCRL
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
NSSPKIXIssuingDistributionPoint_SetIndirectCRL
(
  NSSPKIXIssuingDistributionPoint *idp,
  PRBool indirectCRL
);

/*
 * NSSPKIXIssuingDistributionPoint_Equal
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
NSSPKIXIssuingDistributionPoint_Equal
(
  NSSPKIXIssuingDistributionPoint *idp1,
  NSSPKIXIssuingDistributionPoint *idp2,
  PRStatus *statusOpt
);

/*
 * NSSPKIXIssuingDistributionPoint_Duplicate
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
NSSPKIXIssuingDistributionPoint_Duplicate
(
  NSSPKIXIssuingDistributionPoint *idp,
  NSSArena *arenaOpt
);

PR_END_EXTERN_C

#endif /* NSSPKIX_H */
