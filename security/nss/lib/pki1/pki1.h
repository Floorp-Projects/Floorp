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
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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

#ifndef PKI1_H
#define PKI1_H

#ifdef DEBUG
static const char PKI1_CVS_ID[] = "@(#) $RCSfile: pki1.h,v $ $Revision: 1.5 $ $Date: 2005/03/14 18:02:00 $";
#endif /* DEBUG */

/*
 * pki1.h
 *
 * This file contains the prototypes to the non-public NSS routines
 * relating to the PKIX part-1 objects.
 */

#ifndef PKI1T_H
#include "pki1t.h"
#endif /* PKI1T_H */

#ifndef NSSPKI1_H
#include "nsspki1.h"
#endif /* NSSPKI1_H */

PR_BEGIN_EXTERN_C

extern const NSSOID nss_builtin_oids[];
extern const PRUint32 nss_builtin_oid_count;

extern const nssAttributeTypeAliasTable nss_attribute_type_aliases[];
extern const PRUint32 nss_attribute_type_alias_count;

/*
 * NSSOID
 *
 * The non-public "methods" regarding this "object" are:
 *
 *  nssOID_CreateFromBER   -- constructor
 *  nssOID_CreateFromUTF8  -- constructor
 *  (there is no explicit destructor)
 * 
 *  nssOID_GetDEREncoding
 *  nssOID_GetUTF8Encoding
 *
 * In debug builds, the following non-public calls are also available:
 *
 *  nssOID_verifyPointer
 *  nssOID_getExplanation
 *  nssOID_getTaggedUTF8
 */

/*
 * nssOID_CreateFromBER
 *
 * This routine creates an NSSOID by decoding a BER- or DER-encoded
 * OID.  It may return NSS_OID_UNKNOWN upon error, in which case it 
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NSS_OID_UNKNOWN upon error
 *  An NSSOID upon success
 */

NSS_EXTERN NSSOID *
nssOID_CreateFromBER
(
  NSSBER *berOid
);

extern const NSSError NSS_ERROR_INVALID_BER;
extern const NSSError NSS_ERROR_NO_MEMORY;

/*
 * nssOID_CreateFromUTF8
 *
 * This routine creates an NSSOID by decoding a UTF8 string 
 * representation of an OID in dotted-number format.  The string may 
 * optionally begin with an octothorpe.  It may return NSS_OID_UNKNOWN
 * upon error, in which case it will have set an error on the error 
 * stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_UTF8
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NSS_OID_UNKNOWN upon error
 *  An NSSOID upon success
 */

NSS_EXTERN NSSOID *
nssOID_CreateFromUTF8
(
  NSSUTF8 *stringOid
);

extern const NSSError NSS_ERROR_INVALID_UTF8;
extern const NSSError NSS_ERROR_NO_MEMORY;

/*
 * nssOID_GetDEREncoding
 *
 * This routine returns the DER encoding of the specified NSSOID.
 * If the optional arena argument is non-null, the memory used will
 * be obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return return null upon error, in 
 * which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NSSOID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSOID
 */

NSS_EXTERN NSSDER *
nssOID_GetDEREncoding
(
  const NSSOID *oid,
  NSSDER *rvOpt,
  NSSArena *arenaOpt
);

/*
 * nssOID_GetUTF8Encoding
 *
 * This routine returns a UTF8 string containing the dotted-number 
 * encoding of the specified NSSOID.  If the optional arena argument 
 * is non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine
 * may return null upon error, in which case it will have set an error
 * on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NSSOID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 string containing the dotted-digit encoding of 
 *      this NSSOID
 */

NSS_EXTERN NSSUTF8 *
nssOID_GetUTF8Encoding
(
  const NSSOID *oid,
  NSSArena *arenaOpt
);

/*
 * nssOID_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid poitner to an NSSOID object, 
 * this routine will return PR_SUCCESS.  Otherwise, it will put an 
 * error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NSSOID
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

#ifdef DEBUG
NSS_EXTERN PRStatus
nssOID_verifyPointer
(
  const NSSOID *oid
);

extern const NSSError NSS_ERROR_INVALID_NSSOID;
#endif /* DEBUG */

/*
 * nssOID_getExplanation
 *
 * This method is only present in debug builds.
 *
 * This routine will return a static pointer to a UTF8-encoded string
 * describing (in English) the specified OID.  The memory pointed to
 * by the return value is not owned by the caller, and should not be
 * freed or modified.  Note that explanations are only provided for
 * the OIDs built into the NSS library; there is no way to specify an
 * explanation for dynamically created OIDs.  This routine is intended
 * only for use in debugging tools such as "derdump."  This routine
 * may return null upon error, in which case it will have placed an
 * error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NSSOID
 *
 * Return value:
 *  NULL upon error
 *  A static pointer to a readonly, non-caller-owned UTF8-encoded
 *    string explaining the specified OID.
 */

#ifdef DEBUG
NSS_EXTERN const NSSUTF8 *
nssOID_getExplanation
(
  NSSOID *oid
);

extern const NSSError NSS_ERROR_INVALID_NSSOID;
#endif /* DEBUG */

/*
 * nssOID_getTaggedUTF8
 *
 * This method is only present in debug builds.
 *
 * This routine will return a pointer to a caller-owned UTF8-encoded
 * string containing a tagged encoding of the specified OID.  Note
 * that OID (component) tags are only provided for the OIDs built 
 * into the NSS library; there is no way to specify tags for 
 * dynamically created OIDs.  This routine is intended for use in
 * debugging tools such as "derdump."   If the optional arena argument
 * is non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine 
 * may return return null upon error, in which case it will have set 
 * an error on the error stack.
 *
 * The error may be one of the following values
 *  NSS_ERROR_INVALID_NSSOID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 string containing the tagged encoding of
 *      this NSSOID
 */

#ifdef DEBUG
NSS_EXTERN NSSUTF8 *
nssOID_getTaggedUTF8
(
  NSSOID *oid,
  NSSArena *arenaOpt
);

extern const NSSError NSS_ERROR_INVALID_NSSOID;
extern const NSSError NSS_ERROR_NO_MEMORY;
#endif /* DEBUG */

/*
 * NSSATAV
 *
 * The non-public "methods" regarding this "object" are:
 *
 *  nssATAV_CreateFromBER   -- constructor
 *  nssATAV_CreateFromUTF8  -- constructor
 *  nssATAV_Create          -- constructor
 *
 *  nssATAV_Destroy
 *  nssATAV_GetDEREncoding
 *  nssATAV_GetUTF8Encoding
 *  nssATAV_GetType
 *  nssATAV_GetValue
 *  nssATAV_Compare
 *  nssATAV_Duplicate
 *
 * In debug builds, the following non-public call is also available:
 *
 *  nssATAV_verifyPointer
 */

/*
 * nssATAV_CreateFromBER
 * 
 * This routine creates an NSSATAV by decoding a BER- or DER-encoded
 * ATAV.  If the optional arena argument is non-null, the memory used 
 * will be obtained from that arena; otherwise, the memory will be 
 * obtained from the heap.  This routine may return NULL upon error, 
 * in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSATAV upon success
 */

NSS_EXTERN NSSATAV *
nssATAV_CreateFromBER
(
  NSSArena *arenaOpt,
  const NSSBER *berATAV
);

/*
 * nssATAV_CreateFromUTF8
 *
 * This routine creates an NSSATAV by decoding a UTF8 string in the
 * "equals" format, e.g., "c=US."  If the optional arena argument is 
 * non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine
 * may return NULL upon error, in which case it will have set an error 
 * on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_UNKNOWN_ATTRIBUTE
 *  NSS_ERROR_INVALID_UTF8
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSATAV upon success
 */

NSS_EXTERN NSSATAV *
nssATAV_CreateFromUTF8
(
  NSSArena *arenaOpt,
  const NSSUTF8 *stringATAV
);

/*
 * nssATAV_Create
 *
 * This routine creates an NSSATAV from the specified NSSOID and the
 * specified data. If the optional arena argument is non-null, the 
 * memory used will be obtained from that arena; otherwise, the memory
 * will be obtained from the heap.If the specified data length is zero, 
 * the data is assumed to be terminated by first zero byte; this allows 
 * UTF8 strings to be easily specified.  This routine may return NULL 
 * upon error, in which case it will have set an error on the error 
 * stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ARENA
 *  NSS_ERROR_INVALID_NSSOID
 *  NSS_ERROR_INVALID_POINTER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSATAV upon success
 */

NSS_EXTERN NSSATAV *
nssATAV_Create
(
  NSSArena *arenaOpt,
  const NSSOID *oid,
  const void *data,
  PRUint32 length
);

/*
 * nssATAV_Destroy
 *
 * This routine will destroy an ATAV object.  It should eventually be
 * called on all ATAVs created without an arena.  While it is not 
 * necessary to call it on ATAVs created within an arena, it is not an
 * error to do so.  This routine returns a PRStatus value; if
 * successful, it will return PR_SUCCESS.  If unsuccessful, it will
 * set an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssATAV_Destroy
(
  NSSATAV *atav
);

/*
 * nssATAV_GetDEREncoding
 *
 * This routine will DER-encode an ATAV object. If the optional arena
 * argument is non-null, the memory used will be obtained from that
 * arena; otherwise, the memory will be obtained from the heap.  This
 * routine may return null upon error, in which case it will have set
 * an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSATAV
 */

NSS_EXTERN NSSDER *
nssATAV_GetDEREncoding
(
  NSSATAV *atav,
  NSSArena *arenaOpt
);

/*
 * nssATAV_GetUTF8Encoding
 *
 * This routine returns a UTF8 string containing a string 
 * representation of the ATAV in "equals" notation (e.g., "o=Acme").  
 * If the optional arena argument is non-null, the memory used will be
 * obtained from that arena; otherwise, the memory will be obtained 
 * from the heap.  This routine may return null upon error, in which 
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 string containing the "equals" encoding of the 
 *      ATAV
 */

NSS_EXTERN NSSUTF8 *
nssATAV_GetUTF8Encoding
(
  NSSATAV *atav,
  NSSArena *arenaOpt
);

/*
 * nssATAV_GetType
 *
 * This routine returns the NSSOID corresponding to the attribute type
 * in the specified ATAV.  This routine may return NSS_OID_UNKNOWN 
 * upon error, in which case it will have set an error on the error
 * stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *
 * Return value:
 *  NSS_OID_UNKNOWN upon error
 *  An element of enum NSSOIDenum upon success
 */

NSS_EXTERN const NSSOID *
nssATAV_GetType
(
  NSSATAV *atav
);

/*
 * nssATAV_GetValue
 *
 * This routine returns a string containing the attribute value
 * in the specified ATAV.  If the optional arena argument is non-null,
 * the memory used will be obtained from that arena; otherwise, the
 * memory will be obtained from the heap.  This routine may return
 * NULL upon error, in which case it will have set an error upon the
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSItem containing the attribute value.
 */

NSS_EXTERN NSSUTF8 *
nssATAV_GetValue
(
  NSSATAV *atav,
  NSSArena *arenaOpt
);

/*
 * nssATAV_Compare
 *
 * This routine compares two ATAVs for equality.  For two ATAVs to be
 * equal, the attribute types must be the same, and the attribute 
 * values must have equal length and contents.  The result of the 
 * comparison will be stored at the location pointed to by the "equalp"
 * variable, which must point to a valid PRBool.  This routine may 
 * return PR_FAILURE upon error, in which case it will have set an 
 * error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_INVALID_ARGUMENT
 *
 * Return value:
 *  PR_FAILURE on error
 *  PR_SUCCESS upon a successful comparison (equal or not)
 */

NSS_EXTERN PRStatus
nssATAV_Compare
(
  NSSATAV *atav1,
  NSSATAV *atav2,
  PRBool *equalp
);

/*
 * nssATAV_Duplicate
 *
 * This routine duplicates the specified ATAV.  If the optional arena 
 * argument is non-null, the memory required will be obtained from
 * that arena; otherwise, the memory will be obtained from the heap.  
 * This routine may return NULL upon error, in which case it will have 
 * placed an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL on error
 *  A pointer to a new ATAV
 */

NSS_EXTERN NSSATAV *
nssATAV_Duplicate
(
  NSSATAV *atav,
  NSSArena *arenaOpt
);

/*
 * nssATAV_verifyPointer
 *
 * This method is only present in debug builds.
 *
 * If the specified pointer is a valid pointer to an NSSATAV object,
 * this routine will return PR_SUCCESS.  Otherwise, it will put an
 * error on the error stack and return PR_FAILRUE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NSSATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS if the pointer is valid
 *  PR_FAILURE if it isn't
 */

#ifdef DEBUG
NSS_EXTERN PRStatus
nssATAV_verifyPointer
(
  NSSATAV *atav
);
#endif /* DEBUG */

/*
 * NSSRDN
 *
 * The non-public "methods" regarding this "object" are:
 *
 *  nssRDN_CreateFromBER   -- constructor
 *  nssRDN_CreateFromUTF8  -- constructor
 *  nssRDN_Create          -- constructor
 *  nssRDN_CreateSimple    -- constructor
 *
 *  nssRDN_Destroy
 *  nssRDN_GetDEREncoding
 *  nssRDN_GetUTF8Encoding
 *  nssRDN_AddATAV
 *  nssRDN_GetATAVCount
 *  nssRDN_GetATAV
 *  nssRDN_GetSimpleATAV
 *  nssRDN_Compare
 *  nssRDN_Duplicate
 */

/*
 * nssRDN_CreateFromBER
 *
 * This routine creates an NSSRDN by decoding a BER- or DER-encoded 
 * RDN.  If the optional arena argument is non-null, the memory used 
 * will be obtained from that arena; otherwise, the memory will be 
 * obtained from the heap.  This routine may return NULL upon error, 
 * in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSRDN upon success
 */

NSS_EXTERN NSSRDN *
nssRDN_CreateFromBER
(
  NSSArena *arenaOpt,
  NSSBER *berRDN
);

/*
 * nssRDN_CreateFromUTF8
 *
 * This routine creates an NSSRDN by decoding an UTF8 string 
 * consisting of either a single ATAV in the "equals" format, e.g., 
 * "uid=smith," or one or more such ATAVs in parentheses, e.g., 
 * "(sn=Smith,ou=Sales)."  If the optional arena argument is non-null,
 * the memory used will be obtained from that arena; otherwise, the
 * memory will be obtained from the heap.  This routine may return
 * NULL upon error, in which case it will have set an error on the
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_UNKNOWN_ATTRIBUTE
 *  NSS_ERROR_INVALID_UTF8
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSRDN upon success
 */

NSS_EXTERN NSSRDN *
nssRDN_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *stringRDN
);

/*
 * nssRDN_Create
 *
 * This routine creates an NSSRDN from one or more NSSATAVs.  The
 * final argument to this routine must be NULL.  If the optional arena
 * argument is non-null, the memory used will be obtained from that
 * arena; otherwise, the memory will be obtained from the heap.  This
 * routine may return NULL upon error, in which case it will have set
 * an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ATAV
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSRDN upon success
 */

NSS_EXTERN NSSRDN *
nssRDN_Create
(
  NSSArena *arenaOpt,
  NSSATAV *atav1,
  ...
);

/*
 * nssRDN_CreateSimple
 *
 * This routine creates a simple NSSRDN from a single NSSATAV.  If the
 * optional arena argument is non-null, the memory used will be 
 * obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return NULL upon error, in which
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_ATAV
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSRDN upon success
 */

NSS_EXTERN NSSRDN *
nssRDN_CreateSimple
(
  NSSArena *arenaOpt,
  NSSATAV *atav
);

/*
 * nssRDN_Destroy
 *
 * This routine will destroy an RDN object.  It should eventually be
 * called on all RDNs created without an arena.  While it is not 
 * necessary to call it on RDNs created within an arena, it is not an
 * error to do so.  This routine returns a PRStatus value; if
 * successful, it will return PR_SUCCESS.  If unsuccessful, it will 
 * set an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDN
 *
 * Return value:
 *  PR_FAILURE upon failure
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssRDN_Destroy
(
  NSSRDN *rdn
);

/*
 * nssRDN_GetDEREncoding
 *
 * This routine will DER-encode an RDN object.  If the optional arena
 * argument is non-null, the memory used will be obtained from that
 * arena; otherwise, the memory will be obtained from the heap.  This
 * routine may return null upon error, in which case it will have set
 * an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDN
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSRDN
 */

NSS_EXTERN NSSDER *
nssRDN_GetDEREncoding
(
  NSSRDN *rdn,
  NSSArena *arenaOpt
);

/*
 * nssRDN_GetUTF8Encoding
 *
 * This routine returns a UTF8 string containing a string 
 * representation of the RDN.  A simple (one-ATAV) RDN will be simply
 * the string representation of that ATAV; a non-simple RDN will be in
 * parenthesised form.  If the optional arena argument is non-null, 
 * the memory used will be obtained from that arena; otherwise, the 
 * memory will be obtained from the heap.  This routine may return 
 * null upon error, in which case it will have set an error on the 
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDN
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 string
 */

NSS_EXTERN NSSUTF8 *
nssRDN_GetUTF8Encoding
(
  NSSRDN *rdn,
  NSSArena *arenaOpt
);

/*
 * nssRDN_AddATAV
 *
 * This routine adds an ATAV to the set of ATAVs in the specified RDN.
 * Remember that RDNs consist of an unordered set of ATAVs.  If the
 * RDN was created with a non-null arena argument, that same arena
 * will be used for any additional required memory.  If the RDN was 
 * created with a NULL arena argument, any additional memory will
 * be obtained from the heap.  This routine returns a PRStatus value;
 * it will return PR_SUCCESS upon success, and upon failure it will
 * set an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDN
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssRDN_AddATAV
(
  NSSRDN *rdn,
  NSSATAV *atav
);

/*
 * nssRDN_GetATAVCount
 *
 * This routine returns the cardinality of the set of ATAVs within
 * the specified RDN.  This routine may return 0 upon error, in which 
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDN
 *
 * Return value:
 *  0 upon error
 *  A positive number upon success
 */

NSS_EXTERN PRUint32
nssRDN_GetATAVCount
(
  NSSRDN *rdn
);

/*
 * nssRDN_GetATAV
 *
 * This routine returns a pointer to an ATAV that is a member of
 * the set of ATAVs within the specified RDN.  While the set of
 * ATAVs within an RDN is unordered, this routine will return
 * distinct values for distinct values of 'i' as long as the RDN
 * is not changed in any way.  The RDN may be changed by calling
 * NSSRDN_AddATAV.  The value of the variable 'i' is on the range
 * [0,c) where c is the cardinality returned from NSSRDN_GetATAVCount.
 * The caller owns the ATAV the pointer to which is returned.  If the
 * optional arena argument is non-null, the memory used will be 
 * obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return NULL upon error, in which 
 * case it will have set an error upon the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDN
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSATAV
 */

NSS_EXTERN NSSATAV *
nssRDN_GetATAV
(
  NSSRDN *rdn,
  NSSArena *arenaOpt,
  PRUint32 i
);

/*
 * nssRDN_GetSimpleATAV
 *
 * Most RDNs are actually very simple, with a single ATAV.  This 
 * routine will return the single ATAV from such an RDN.  The caller
 * owns the ATAV the pointer to which is returned.  If the optional
 * arena argument is non-null, the memory used will be obtained from
 * that arena; otherwise, the memory will be obtained from the heap.
 * This routine may return NULL upon error, including the case where
 * the set of ATAVs in the RDN is nonsingular.  Upon error, this
 * routine will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDN
 *  NSS_ERROR_RDN_NOT_SIMPLE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSATAV
 */

NSS_EXTERN NSSATAV *
nssRDN_GetSimpleATAV
(
  NSSRDN *rdn,
  NSSArena *arenaOpt
);

/*
 * nssRDN_Compare
 *
 * This routine compares two RDNs for equality.  For two RDNs to be
 * equal, they must have the same number of ATAVs, and every ATAV in
 * one must be equal to an ATAV in the other.  (Note that the sets
 * of ATAVs are unordered.)  The result of the comparison will be
 * stored at the location pointed to by the "equalp" variable, which
 * must point to a valid PRBool.  This routine may return PR_FAILURE
 * upon error, in which case it will have set an error on the error
 * stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDN
 *  NSS_ERROR_INVALID_ARGUMENT
 *
 * Return value:
 *  PR_FAILURE on error
 *  PR_SUCCESS upon a successful comparison (equal or not)
 */

NSS_EXTERN PRStatus
nssRDN_Compare
(
  NSSRDN *rdn1,
  NSSRDN *rdn2,
  PRBool *equalp
);

/*
 * nssRDN_Duplicate
 *
 * This routine duplicates the specified RDN.  If the optional arena
 * argument is non-null, the memory required will be obtained from
 * that arena; otherwise, the memory will be obtained from the heap.
 * This routine may return NULL upon error, in which case it will have
 * placed an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDN
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL on error
 *  A pointer to a new RDN
 */

NSS_EXTERN NSSRDN *
nssRDN_Duplicate
(
  NSSRDN *rdn,
  NSSArena *arenaOpt
);

/*
 * NSSRDNSeq
 *
 * The non-public "methods" regarding this "object" are:
 *
 *  nssRDNSeq_CreateFromBER   -- constructor
 *  nssRDNSeq_CreateFromUTF8  -- constructor
 *  nssRDNSeq_Create          -- constructor
 *
 *  nssRDNSeq_Destroy
 *  nssRDNSeq_GetDEREncoding
 *  nssRDNSeq_GetUTF8Encoding
 *  nssRDNSeq_AppendRDN
 *  nssRDNSeq_GetRDNCount
 *  nssRDNSeq_GetRDN
 *  nssRDNSeq_Compare
 *  nssRDNSeq_Duplicate
 *
 *  nssRDNSeq_EvaluateUTF8 -- not an object method
 */

/*
 * nssRDNSeq_CreateFromBER
 *
 * This routine creates an NSSRDNSeq by decoding a BER- or DER-encoded 
 * sequence of RDNs.  If the optional arena argument is non-null,
 * the memory used will be obtained from that arena; otherwise, the
 * memory will be obtained from the heap.  This routine may return 
 * NULL upon error, in which case it will have set an error on the
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSRDNSeq upon success
 */

NSS_EXTERN NSSRDNSeq *
nssRDNSeq_CreateFromBER
(
  NSSArena *arenaOpt,
  NSSBER *berRDNSeq
);

/*
 * nssRDNSeq_CreateFromUTF8
 *
 * This routine creates an NSSRDNSeq by decoding a UTF8 string
 * consisting of a comma-separated sequence of RDNs, such as
 * "(sn=Smith,ou=Sales),o=Acme,c=US."  If the optional arena argument
 * is non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine
 * may return NULL upon error, in which case it will have set an error
 * on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_UNKNOWN_ATTRIBUTE
 *  NSS_ERROR_INVALID_UTF8
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSRDNSeq upon success
 */

NSS_EXTERN NSSRDNSeq *
nssRDNSeq_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *stringRDNSeq
);

/*
 * nssRDNSeq_Create
 *
 * This routine creates an NSSRDNSeq from one or more NSSRDNs.  The
 * final argument to this routine must be NULL.  If the optional arena
 * argument is non-null, the memory used will be obtained from that
 * arena; otherwise, the memory will be obtained from the heap.  This
 * routine may return NULL upon error, in which case it will have set
 * an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_RDN
 *
 * Return value:
 *  NULL upon error
 *  A pointero to an NSSRDNSeq upon success
 */

NSS_EXTERN NSSRDNSeq *
nssRDNSeq_Create
(
  NSSArena *arenaOpt,
  NSSRDN *rdn1,
  ...
);

/*
 * nssRDNSeq_Destroy
 *
 * This routine will destroy an RDNSeq object.  It should eventually 
 * be called on all RDNSeqs created without an arena.  While it is not
 * necessary to call it on RDNSeqs created within an arena, it is not
 * an error to do so.  This routine returns a PRStatus value; if
 * successful, it will return PR_SUCCESS.  If unsuccessful, it will
 * set an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDNSEQ
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssRDNSeq_Destroy
(
  NSSRDNSeq *rdnseq
);

/*
 * nssRDNSeq_GetDEREncoding
 *
 * This routine will DER-encode an RDNSeq object.  If the optional 
 * arena argument is non-null, the memory used will be obtained from
 * that arena; otherwise, the memory will be obtained from the heap.
 * This routine may return null upon error, in which case it will have
 * set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDNSEQ
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSRDNSeq
 */

NSS_EXTERN NSSDER *
nssRDNSeq_GetDEREncoding
(
  NSSRDNSeq *rdnseq,
  NSSArena *arenaOpt
);

/*
 * nssRDNSeq_GetUTF8Encoding
 *
 * This routine returns a UTF8 string containing a string 
 * representation of the RDNSeq as a comma-separated sequence of RDNs.  
 * If the optional arena argument is non-null, the memory used will be
 * obtained from that arena; otherwise, the memory will be obtained 
 * from the heap.  This routine may return null upon error, in which 
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDNSEQ
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to the UTF8 string
 */

NSS_EXTERN NSSUTF8 *
nssRDNSeq_GetUTF8Encoding
(
  NSSRDNSeq *rdnseq,
  NSSArena *arenaOpt
);

/*
 * nssRDNSeq_AppendRDN
 *
 * This routine appends an RDN to the end of the existing RDN 
 * sequence.  If the RDNSeq was created with a non-null arena 
 * argument, that same arena will be used for any additional required
 * memory.  If the RDNSeq was created with a NULL arena argument, any
 * additional memory will be obtained from the heap.  This routine
 * returns a PRStatus value; it will return PR_SUCCESS upon success,
 * and upon failure it will set an error on the error stack and return
 * PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDNSEQ
 *  NSS_ERROR_INVALID_RDN
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure
 */

NSS_EXTERN PRStatus
nssRDNSeq_AppendRDN
(
  NSSRDNSeq *rdnseq,
  NSSRDN *rdn
);

/*
 * nssRDNSeq_GetRDNCount
 *
 * This routine returns the cardinality of the sequence of RDNs within
 * the specified RDNSeq.  This routine may return 0 upon error, in 
 * which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDNSEQ
 *
 * Return value:
 *  0 upon error
 *  A positive number upon success
 */

NSS_EXTERN PRUint32
nssRDNSeq_GetRDNCount
(
  NSSRDNSeq *rdnseq
);

/*
 * nssRDNSeq_GetRDN
 *
 * This routine returns a pointer to the i'th RDN in the sequence of
 * RDNs that make up the specified RDNSeq.  The sequence begins with
 * the top-level (e.g., "c=US") RDN.  The value of the variable 'i'
 * is on the range [0,c) where c is the cardinality returned from
 * NSSRDNSeq_GetRDNCount.  The caller owns the RDN the pointer to which
 * is returned.  If the optional arena argument is non-null, the memory
 * used will be obtained from that areana; otherwise, the memory will 
 * be obtained from the heap.  This routine may return NULL upon error,
 * in which case it will have set an error upon the error stack.  Note
 * that the usual UTF8 representation of RDN Sequences is from last
 * to first.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDNSEQ
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSRDN
 */

NSS_EXTERN NSSRDN *
nssRDNSeq_GetRDN
(
  NSSRDNSeq *rdnseq,
  NSSArena *arenaOpt,
  PRUint32 i
);

/*
 * nssRDNSeq_Compare
 *
 * This routine compares two RDNSeqs for equality.  For two RDNSeqs to 
 * be equal, they must have the same number of RDNs, and each RDN in
 * one sequence must be equal to the corresponding RDN in the other
 * sequence.  The result of the comparison will be stored at the
 * location pointed to by the "equalp" variable, which must point to a
 * valid PRBool.  This routine may return PR_FAILURE upon error, in
 * which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDNSEQ
 *  NSS_ERROR_INVALID_ARGUMENT
 *
 * Return value:
 *  PR_FAILURE on error
 *  PR_SUCCESS upon a successful comparison (equal or not)
 */

NSS_EXTERN PRStatus
nssRDNSeq_Compare
(
  NSSRDNSeq *rdnseq1,
  NSSRDNSeq *rdnseq2,
  PRBool *equalp
);

/*
 * nssRDNSeq_Duplicate
 *
 * This routine duplicates the specified RDNSeq.  If the optional arena
 * argument is non-null, the memory required will be obtained from that
 * arena; otherwise, the memory will be obtained from the heap.  This 
 * routine may return NULL upon error, in which case it will have 
 * placed an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_RDNSEQ
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a new RDNSeq
 */

NSS_EXTERN NSSRDNSeq *
nssRDNSeq_Duplicate
(
  NSSRDNSeq *rdnseq,
  NSSArena *arenaOpt
);

/*
 * nssRDNSeq_EvaluateUTF8
 *
 * This routine evaluates a UTF8 string, and returns PR_TRUE if the 
 * string contains the string representation of an RDNSeq.  This 
 * routine is used by the (directory) Name routines 
 * nssName_CreateFromUTF8 and nssName_EvaluateUTF8 to determine which
 * choice of directory name the string may encode.  This routine may 
 * return PR_FALSE upon error, but it subsumes that condition under the
 * general "string does not evaluate as an RDNSeq" state, and does not
 * set an error on the error stack.
 *
 * Return value:
 *  PR_TRUE if the string represents an RDNSeq
 *  PR_FALSE if otherwise
 */

NSS_EXTERN PRBool
nssRDNSeq_EvaluateUTF8
(
  NSSUTF8 *str
);

/*
 * NSSName
 *
 * The non-public "methods" regarding this "object" are:
 *
 * nssName_CreateFromBER   -- constructor
 * nssName_CreateFromUTF8  -- constructor
 * nssName_Create          -- constructor
 *
 * nssName_Destroy
 * nssName_GetDEREncoding
 * nssName_GetUTF8Encoding
 * nssName_GetChoice
 * nssName_GetRDNSequence
 * nssName_GetSpecifiedChoice
 * nssName_Compare
 * nssName_Duplicate
 *
 * nssName_GetUID
 * nssName_GetEmail
 * nssName_GetCommonName
 * nssName_GetOrganization
 * nssName_GetOrganizationalUnits
 * nssName_GetStateOrProvince
 * nssName_GetLocality
 * nssName_GetCountry
 * nssName_GetAttribute
 *
 * nssName_EvaluateUTF8 -- not an object method
 */

/*
 * nssName_CreateFromBER
 *
 * This routine creates an NSSName by decoding a BER- or DER-encoded 
 * (directory) Name.  If the optional arena argument is non-null,
 * the memory used will be obtained from that arena; otherwise, 
 * the memory will be obtained from the heap.  This routine may
 * return NULL upon error, in which case it will have set an error
 * on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSName upon success
 */

NSS_EXTERN NSSName *
nssName_CreateFromBER
(
  NSSArena *arenaOpt,
  NSSBER *berName
);

/*
 * nssName_CreateFromUTF8
 *
 * This routine creates an NSSName by decoding a UTF8 string 
 * consisting of the string representation of one of the choices of 
 * (directory) names.  Currently the only choice is an RDNSeq.  If the
 * optional arena argument is non-null, the memory used will be 
 * obtained from that arena; otherwise, the memory will be obtained 
 * from the heap.  The routine may return NULL upon error, in which
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_UTF8
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSName upon success
 */

NSS_EXTERN NSSName *
nssName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *stringName
);

/*
 * nssName_Create
 *
 * This routine creates an NSSName with the specified choice of
 * underlying name types.  The value of the choice variable must be
 * one of the values of the NSSNameChoice enumeration, and the type
 * of the arg variable must be as specified in the following table:
 *
 *   Choice                     Type
 *   ========================   ===========
 *   NSSNameChoiceRdnSequence   NSSRDNSeq *
 *
 * If the optional arena argument is non-null, the memory used will
 * be obtained from that arena; otherwise, the memory will be 
 * obtained from the heap.  This routine may return NULL upon error,
 * in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_CHOICE
 *  NSS_ERROR_INVALID_ARGUMENT
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSName upon success
 */

NSS_EXTERN NSSName *
nssName_Create
(
  NSSArena *arenaOpt,
  NSSNameChoice choice,
  void *arg
);

/*
 * nssName_Destroy
 *
 * This routine will destroy a Name object.  It should eventually be
 * called on all Names created without an arena.  While it is not
 * necessary to call it on Names created within an arena, it is not
 * an error to do so.  This routine returns a PRStatus value; if
 * successful, it will return PR_SUCCESS.  If unsuccessful, it will
 * set an error on the error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssName_Destroy
(
  NSSName *name
);

/*
 * nssName_GetDEREncoding
 *
 * This routine will DER-encode a name object.  If the optional arena
 * argument is non-null, the memory used will be obtained from that
 * arena; otherwise, the memory will be obtained from the heap.  This
 * routine may return null upon error, in which case it will have set
 * an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSName
 */

NSS_EXTERN NSSDER *
nssName_GetDEREncoding
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetUTF8Encoding
 *
 * This routine returns a UTF8 string containing a string 
 * representation of the Name in the format specified by the 
 * underlying name choice.  If the optional arena argument is non-null,
 * the memory used will be obtained from that arena; otherwise, the 
 * memory will be obtained from the heap.  This routine may return
 * NULL upon error, in which case it will have set an error on the 
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to the UTF8 string
 */

NSS_EXTERN NSSUTF8 *
nssName_GetUTF8Encoding
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetChoice
 *
 * This routine returns the type of the choice underlying the specified
 * name.  The return value will be a member of the NSSNameChoice 
 * enumeration.  This routine may return NSSNameChoiceInvalid upon 
 * error, in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *
 * Return value:
 *  NSSNameChoiceInvalid upon error
 *  An other member of the NSSNameChoice enumeration upon success
 */

NSS_EXTERN NSSNameChoice
nssName_GetChoice
(
  NSSName *name
);

/*
 * nssName_GetRDNSequence
 *
 * If the choice underlying the specified NSSName is that of an 
 * RDNSequence, this routine will return a pointer to that RDN
 * sequence.  Otherwise, this routine will place an error on the
 * error stack, and return NULL.  If the optional arena argument is
 * non-null, the memory required will be obtained from that arena;
 * otherwise, the memory will be obtained from the heap.  The
 * caller owns the returned pointer.  This routine may return NULL
 * upon error, in which case it will have set an error on the error
 * stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSRDNSeq
 */

NSS_EXTERN NSSRDNSeq *
nssName_GetRDNSequence
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetSpecifiedChoice
 *
 * If the choice underlying the specified NSSName matches the specified
 * choice, a caller-owned pointer to that underlying object will be
 * returned.  Otherwise, an error will be placed on the error stack and
 * NULL will be returned.  If the optional arena argument is non-null, 
 * the memory required will be obtained from that arena; otherwise, the
 * memory will be obtained from the heap.  The caller owns the returned
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer, which must be typecast
 */

NSS_EXTERN void *
nssName_GetSpecifiedChoice
(
  NSSName *name,
  NSSNameChoice choice,
  NSSArena *arenaOpt
);

/*
 * nssName_Compare
 *
 * This routine compares two Names for equality.  For two Names to be
 * equal, they must have the same choice of underlying types, and the
 * underlying values must be equal.  The result of the comparison will
 * be stored at the location pointed to by the "equalp" variable, which
 * must point to a valid PRBool. This routine may return PR_FAILURE
 * upon error, in which case it will have set an error on the error
 * stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_INVALID_ARGUMENT
 *
 * Return value:
 *  PR_FAILURE on error
 *  PR_SUCCESS upon a successful comparison (equal or not)
 */

NSS_EXTERN PRStatus
nssName_Compare
(
  NSSName *name1,
  NSSName *name2,
  PRBool *equalp
);

/*
 * nssName_Duplicate
 *
 * This routine duplicates the specified nssname.  If the optional
 * arena argument is non-null, the memory required will be obtained
 * from that arena; otherwise, the memory will be obtained from the
 * heap.  This routine may return NULL upon error, in which case it
 * will have placed an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a new NSSName
 */

NSS_EXTERN NSSName *
nssName_Duplicate
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetUID
 *
 * This routine will attempt to derive a user identifier from the
 * specified name, if the choices and content of the name permit.
 * If the Name consists of a Sequence of Relative Distinguished 
 * Names containing a UID attribute, the UID will be the value of 
 * that attribute.  Note that no UID attribute is defined in either 
 * PKIX or PKCS#9; rather, this seems to derive from RFC 1274, which 
 * defines the type as a caseIgnoreString.  We'll return a Directory 
 * String.  If the optional arena argument is non-null, the memory 
 * used will be obtained from that arena; otherwise, the memory will 
 * be obtained from the heap.  This routine may return NULL upon error,
 * in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_UID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String.
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssName_GetUID
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetEmail
 *
 * This routine will attempt to derive an email address from the
 * specified name, if the choices and content of the name permit.  
 * If the Name consists of a Sequence of Relative Distinguished 
 * Names containing either a PKIX email address or a PKCS#9 email
 * address, the result will be the value of that attribute.  If the
 * optional arena argument is non-null, the memory used will be
 * obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return NULL upon error, in which
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_EMAIL
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr IA5 String */
nssName_GetEmail
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetCommonName
 *
 * This routine will attempt to derive a common name from the
 * specified name, if the choices and content of the name permit.  
 * If the Name consists of a Sequence of Relative Distinguished Names
 * containing a PKIX Common Name, the result will be that name.  If 
 * the optional arena argument is non-null, the memory used will be 
 * obtained from that arena; otherwise, the memory will be obtained 
 * from the heap.  This routine may return NULL upon error, in which 
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_COMMON_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssName_GetCommonName
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetOrganization
 *
 * This routine will attempt to derive an organisation name from the
 * specified name, if the choices and content of the name permit.  
 * If Name consists of a Sequence of Relative Distinguished names 
 * containing a PKIX Organization, the result will be the value of 
 * that attribute.  If the optional arena argument is non-null, the 
 * memory used will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  This routine may return NULL upon 
 * error, in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_ORGANIZATION
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssName_GetOrganization
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetOrganizationalUnits
 *
 * This routine will attempt to derive a sequence of organisational 
 * unit names from the specified name, if the choices and content of 
 * the name permit.  If the Name consists of a Sequence of Relative 
 * Distinguished Names containing one or more organisational units,
 * the result will be the values of those attributes.  If the optional 
 * arena argument is non-null, the memory used will be obtained from 
 * that arena; otherwise, the memory will be obtained from the heap.  
 * This routine may return NULL upon error, in which case it will have 
 * set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_ORGANIZATIONAL_UNITS
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a null-terminated array of UTF8 Strings
 */

NSS_EXTERN NSSUTF8 ** /* XXX fgmr DirectoryString */
nssName_GetOrganizationalUnits
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetStateOrProvince
 *
 * This routine will attempt to derive a state or province name from 
 * the specified name, if the choices and content of the name permit.
 * If the Name consists of a Sequence of Relative Distinguished Names
 * containing a state or province, the result will be the value of 
 * that attribute.  If the optional arena argument is non-null, the 
 * memory used will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  This routine may return NULL upon 
 * error, in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_STATE_OR_PROVINCE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssName_GetStateOrProvince
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetLocality
 *
 * This routine will attempt to derive a locality name from the 
 * specified name, if the choices and content of the name permit.  If
 * the Name consists of a Sequence of Relative Distinguished names
 * containing a Locality, the result will be the value of that 
 * attribute.  If the optional arena argument is non-null, the memory 
 * used will be obtained from that arena; otherwise, the memory will 
 * be obtained from the heap.  This routine may return NULL upon error,
 * in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_LOCALITY
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssName_GetLocality
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetCountry
 *
 * This routine will attempt to derive a country name from the 
 * specified name, if the choices and content of the name permit.
 * If the Name consists of a Sequence of Relative Distinguished 
 * Names containing a Country, the result will be the value of
 * that attribute..  If the optional arena argument is non-null, 
 * the memory used will be obtained from that arena; otherwise, 
 * the memory will be obtained from the heap.  This routine may 
 * return NULL upon error, in which case it will have set an error 
 * on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_COUNTRY
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr PrintableString */
nssName_GetCountry
(
  NSSName *name,
  NSSArena *arenaOpt
);

/*
 * nssName_GetAttribute
 *
 * If the specified name consists of a Sequence of Relative 
 * Distinguished Names containing an attribute with the specified 
 * type, and the actual value of that attribute may be expressed 
 * with a Directory String, then the value of that attribute will 
 * be returned as a Directory String.  If the optional arena argument 
 * is non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine 
 * may return NULL upon error, in which case it will have set an error 
 * on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_NAME
 *  NSS_ERROR_NO_ATTRIBUTE
 *  NSS_ERROR_ATTRIBUTE_VALUE_NOT_STRING
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssName_GetAttribute
(
  NSSName *name,
  NSSOID *attribute,
  NSSArena *arenaOpt
);

/*
 * nssName_EvaluateUTF8
 *
 * This routine evaluates a UTF8 string, and returns PR_TRUE if the 
 * string contains the string representation of an NSSName.  This 
 * routine is used by the GeneralName routine 
 * nssGeneralName_CreateFromUTF8 to determine which choice of
 * general name the string may encode.  This routine may return 
 * PR_FALSE upon error, but it subsumes that condition under the 
 * general "string does not evaluate as a Name" state, and does not
 * set an error on the error stack.
 * 
 * Return value:
 *  PR_TRUE if the string represents a Name
 *  PR_FALSE otherwise
 */

NSS_EXTERN PRBool
nssName_EvaluateUTF8
(
  NSSUTF8 *str
);

/*
 * NSSGeneralName
 *
 * The non-public "methods" regarding this "object" are:
 *
 * nssGeneralName_CreateFromBER   -- constructor
 * nssGeneralName_CreateFromUTF8  -- constructor
 * nssGeneralName_Create          -- constructor
 *
 * nssGeneralName_Destroy
 * nssGeneralName_GetDEREncoding
 * nssGeneralName_GetUTF8Encoding
 * nssGeneralName_GetChoice
 * nssGeneralName_GetOtherName
 * nssGeneralName_GetRfc822Name
 * nssGeneralName_GetDNSName
 * nssGeneralName_GetX400Address
 * nssGeneralName_GetDirectoryName
 * nssGeneralName_GetEdiPartyName
 * nssGeneralName_GetUniformResourceIdentifier
 * nssGeneralName_GetIPAddress
 * nssGeneralName_GetRegisteredID
 * nssGeneralName_GetSpecifiedChoice
 * nssGeneralName_Compare
 * nssGeneralName_Duplicate
 *
 * nssGeneralName_GetUID
 * nssGeneralName_GetEmail
 * nssGeneralName_GetCommonName
 * nssGeneralName_GetOrganization
 * nssGeneralName_GetOrganizationalUnits
 * nssGeneralName_GetStateOrProvince
 * nssGeneralName_GetLocality
 * nssGeneralName_GetCountry
 * nssGeneralName_GetAttribute
 */

/*
 * nssGeneralName_CreateFromBER
 *
 * This routine creates an NSSGeneralName by decoding a BER- or DER-
 * encoded general name.  If the optional arena argument is non-null,
 * the memory used will be obtained from that arena; otherwise, the 
 * memory will be obtained from the heap.  This routine may return 
 * NULL upon error, in which case it will have set an error on the 
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSGeneralName upon success
 */

NSS_EXTERN NSSGeneralName *
nssGeneralName_CreateFromBER
(
  NSSArena *arenaOpt,
  NSSBER *berGeneralName
);

/*
 * nssGeneralName_CreateFromUTF8
 *
 * This routine creates an NSSGeneralName by decoding a UTF8 string
 * consisting of the string representation of one of the choices of
 * general names.  If the optional arena argument is non-null, the 
 * memory used will be obtained from that arena; otherwise, the memory
 * will be obtained from the heap.  The routine may return NULL upon
 * error, in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_UTF8
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSGeneralName upon success
 */

NSS_EXTERN NSSGeneralName *
nssGeneralName_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *stringGeneralName
);

/*
 * nssGeneralName_Create
 *
 * This routine creates an NSSGeneralName with the specified choice of
 * underlying name types.  The value of the choice variable must be one
 * of the values of the NSSGeneralNameChoice enumeration, and the type
 * of the arg variable must be as specified in the following table:
 *
 *   Choice                                         Type
 *   ============================================   =========
 *   NSSGeneralNameChoiceOtherName
 *   NSSGeneralNameChoiceRfc822Name
 *   NSSGeneralNameChoiceDNSName
 *   NSSGeneralNameChoiceX400Address
 *   NSSGeneralNameChoiceDirectoryName              NSSName *
 *   NSSGeneralNameChoiceEdiPartyName
 *   NSSGeneralNameChoiceUniformResourceIdentifier
 *   NSSGeneralNameChoiceIPAddress
 *   NSSGeneralNameChoiceRegisteredID
 *
 * If the optional arena argument is non-null, the memory used will
 * be obtained from that arena; otherwise, the memory will be 
 * obtained from the heap.  This routine may return NULL upon error,
 * in which case it will have set an error on the error stack.
 *
 * The error may be one fo the following values:
 *  NSS_ERROR_INVALID_CHOICE
 *  NSS_ERROR_INVALID_ARGUMENT
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSGeneralName upon success
 */

NSS_EXTERN NSSGeneralName *
nssGeneralName_Create
(
  NSSGeneralNameChoice choice,
  void *arg
);

/*
 * nssGeneralName_Destroy
 * 
 * This routine will destroy a General Name object.  It should 
 * eventually be called on all General Names created without an arena.
 * While it is not necessary to call it on General Names created within
 * an arena, it is not an error to do so.  This routine returns a
 * PRStatus value; if successful, it will return PR_SUCCESS. If 
 * usuccessful, it will set an error on the error stack and return 
 * PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *
 * Return value:
 *  PR_FAILURE upon failure
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssGeneralName_Destroy
(
  NSSGeneralName *generalName
);

/*
 * nssGeneralName_GetDEREncoding
 *
 * This routine will DER-encode a name object.  If the optional arena
 * argument is non-null, the memory used will be obtained from that
 * arena; otherwise, the memory will be obtained from the heap.  This
 * routine may return null upon error, in which case it will have set
 * an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSGeneralName
 */

NSS_EXTERN NSSDER *
nssGeneralName_GetDEREncoding
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetUTF8Encoding
 *
 * This routine returns a UTF8 string containing a string 
 * representation of the General Name in the format specified by the
 * underlying name choice.  If the optional arena argument is 
 * non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine
 * may return NULL upon error, in which case it will have set an error
 * on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 string
 */

NSS_EXTERN NSSUTF8 *
nssGeneralName_GetUTF8Encoding
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetChoice
 *
 * This routine returns the type of choice underlying the specified 
 * general name.  The return value will be a member of the 
 * NSSGeneralNameChoice enumeration.  This routine may return 
 * NSSGeneralNameChoiceInvalid upon error, in which case it will have 
 * set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *
 * Return value:
 *  NSSGeneralNameChoiceInvalid upon error
 *  An other member of the NSSGeneralNameChoice enumeration 
 */

NSS_EXTERN NSSGeneralNameChoice
nssGeneralName_GetChoice
(
  NSSGeneralName *generalName
);

/*
 * nssGeneralName_GetOtherName
 *
 * If the choice underlying the specified NSSGeneralName is that of an
 * Other Name, this routine will return a pointer to that Other name.
 * Otherwise, this routine will place an error on the error stack, and
 * return NULL.  If the optional arena argument is non-null, the memory
 * required will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  The caller owns the returned 
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSOtherName
 */

NSS_EXTERN NSSOtherName *
nssGeneralName_GetOtherName
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetRfc822Name
 *
 * If the choice underlying the specified NSSGeneralName is that of an
 * RFC 822 Name, this routine will return a pointer to that name.
 * Otherwise, this routine will place an error on the error stack, and
 * return NULL.  If the optional arena argument is non-null, the memory
 * required will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  The caller owns the returned 
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSRFC822Name
 */

NSS_EXTERN NSSRFC822Name *
nssGeneralName_GetRfc822Name
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetDNSName
 *
 * If the choice underlying the specified NSSGeneralName is that of a 
 * DNS Name, this routine will return a pointer to that DNS name.
 * Otherwise, this routine will place an error on the error stack, and
 * return NULL.  If the optional arena argument is non-null, the memory
 * required will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  The caller owns the returned 
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSDNSName
 */

NSS_EXTERN NSSDNSName *
nssGeneralName_GetDNSName
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetX400Address
 *
 * If the choice underlying the specified NSSGeneralName is that of an
 * X.400 Address, this routine will return a pointer to that Address.
 * Otherwise, this routine will place an error on the error stack, and
 * return NULL.  If the optional arena argument is non-null, the memory
 * required will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  The caller owns the returned 
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSX400Address
 */

NSS_EXTERN NSSX400Address *
nssGeneralName_GetX400Address
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetDirectoryName
 *
 * If the choice underlying the specified NSSGeneralName is that of a
 * (directory) Name, this routine will return a pointer to that name.
 * Otherwise, this routine will place an error on the error stack, and
 * return NULL.  If the optional arena argument is non-null, the memory
 * required will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  The caller owns the returned 
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSName
 */

NSS_EXTERN NSSName *
nssGeneralName_GetName
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetEdiPartyName
 *
 * If the choice underlying the specified NSSGeneralName is that of an
 * EDI Party Name, this routine will return a pointer to that name.
 * Otherwise, this routine will place an error on the error stack, and
 * return NULL.  If the optional arena argument is non-null, the memory
 * required will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  The caller owns the returned 
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSEdiPartyName
 */

NSS_EXTERN NSSEdiPartyName *
nssGeneralName_GetEdiPartyName
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetUniformResourceIdentifier
 *
 * If the choice underlying the specified NSSGeneralName is that of a
 * URI, this routine will return a pointer to that URI.
 * Otherwise, this routine will place an error on the error stack, and
 * return NULL.  If the optional arena argument is non-null, the memory
 * required will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  The caller owns the returned 
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSURI
 */

NSS_EXTERN NSSURI *
nssGeneralName_GetUniformResourceIdentifier
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetIPAddress
 *
 * If the choice underlying the specified NSSGeneralName is that of an
 * IP Address , this routine will return a pointer to that address.
 * Otherwise, this routine will place an error on the error stack, and
 * return NULL.  If the optional arena argument is non-null, the memory
 * required will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  The caller owns the returned 
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSIPAddress
 */

NSS_EXTERN NSSIPAddress *
nssGeneralName_GetIPAddress
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetRegisteredID
 *
 * If the choice underlying the specified NSSGeneralName is that of a
 * Registered ID, this routine will return a pointer to that ID.
 * Otherwise, this routine will place an error on the error stack, and
 * return NULL.  If the optional arena argument is non-null, the memory
 * required will be obtained from that arena; otherwise, the memory 
 * will be obtained from the heap.  The caller owns the returned 
 * pointer.  This routine may return NULL upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to an NSSRegisteredID
 */

NSS_EXTERN NSSRegisteredID *
nssGeneralName_GetRegisteredID
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetSpecifiedChoice
 *
 * If the choice underlying the specified NSSGeneralName matches the
 * specified choice, a caller-owned pointer to that underlying object
 * will be returned.  Otherwise, an error will be placed on the error
 * stack and NULL will be returned.  If the optional arena argument
 * is non-null, the memory required will be obtained from that arena;
 * otherwise, the memory will be obtained from the heap.  The caller
 * owns the returned pointer.  This routine may return NULL upon 
 * error, in which caes it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_WRONG_CHOICE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer, which must be typecast
 */

NSS_EXTERN void *
nssGeneralName_GetSpecifiedChoice
(
  NSSGeneralName *generalName,
  NSSGeneralNameChoice choice,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_Compare
 * 
 * This routine compares two General Names for equality.  For two 
 * General Names to be equal, they must have the same choice of
 * underlying types, and the underlying values must be equal.  The
 * result of the comparison will be stored at the location pointed
 * to by the "equalp" variable, which must point to a valid PRBool.
 * This routine may return PR_FAILURE upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following value:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_INVALID_ARGUMENT
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon a successful comparison (equal or not)
 */

NSS_EXTERN PRStatus
nssGeneralName_Compare
(
  NSSGeneralName *generalName1,
  NSSGeneralName *generalName2,
  PRBool *equalp
);

/*
 * nssGeneralName_Duplicate
 *
 * This routine duplicates the specified General Name.  If the optional
 * arena argument is non-null, the memory required will be obtained
 * from that arena; otherwise, the memory will be obtained from the
 * heap.  This routine may return NULL upon error, in which case it 
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a new NSSGeneralName
 */

NSS_EXTERN NSSGeneralName *
nssGeneralName_Duplicate
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetUID
 *
 * This routine will attempt to derive a user identifier from the
 * specified general name, if the choices and content of the name
 * permit.  If the General Name is a (directory) Name consisting
 * of a Sequence of Relative Distinguished Names containing a UID
 * attribute, the UID will be the value of that attribute.  Note
 * that no UID attribute is defined in either PKIX or PKCS#9; 
 * rather, this seems to derive from RFC 1274, which defines the
 * type as a caseIgnoreString.  We'll return a Directory String.
 * If the optional arena argument is non-null, the memory used
 * will be obtained from that arena; otherwise, the memory will be
 * obtained from the heap.  This routine may return NULL upon error,
 * in which case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_UID
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String.
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssGeneralName_GetUID
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetEmail
 *
 * This routine will attempt to derive an email address from the
 * specified general name, if the choices and content of the name
 * permit.  If the General Name is a (directory) Name consisting
 * of a Sequence of Relative Distinguished names containing either
 * a PKIX email address or a PKCS#9 email address, the result will
 * be the value of that attribute.  If the General Name is an RFC 822
 * Name, the result will be the string form of that name.  If the
 * optional arena argument is non-null, the memory used will be 
 * obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return NULL upon error, in which
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_EMAIL
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr IA5String */
nssGeneralName_GetEmail
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetCommonName
 *
 * This routine will attempt to derive a common name from the
 * specified general name, if the choices and content of the name
 * permit.  If the General Name is a (directory) Name consisting
 * of a Sequence of Relative Distinguished names containing a PKIX
 * Common Name, the result will be that name.  If the optional arena 
 * argument is non-null, the memory used will be obtained from that 
 * arena; otherwise, the memory will be obtained from the heap.  This 
 * routine may return NULL upon error, in which case it will have set 
 * an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_COMMON_NAME
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssGeneralName_GetCommonName
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetOrganization
 *
 * This routine will attempt to derive an organisation name from the
 * specified general name, if the choices and content of the name
 * permit.  If the General Name is a (directory) Name consisting
 * of a Sequence of Relative Distinguished names containing an
 * Organization, the result will be the value of that attribute.  
 * If the optional arena argument is non-null, the memory used will 
 * be obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return NULL upon error, in which 
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_ORGANIZATION
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssGeneralName_GetOrganization
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetOrganizationalUnits
 *
 * This routine will attempt to derive a sequence of organisational 
 * unit names from the specified general name, if the choices and 
 * content of the name permit.  If the General Name is a (directory) 
 * Name consisting of a Sequence of Relative Distinguished names 
 * containing one or more organisational units, the result will 
 * consist of those units.  If the optional arena  argument is non-
 * null, the memory used will be obtained from that arena; otherwise, 
 * the memory will be obtained from the heap.  This routine may return 
 * NULL upon error, in which case it will have set an error on the 
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_ORGANIZATIONAL_UNITS
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a null-terminated array of UTF8 Strings
 */

NSS_EXTERN NSSUTF8 ** /* XXX fgmr DirectoryString */
nssGeneralName_GetOrganizationalUnits
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetStateOrProvince
 *
 * This routine will attempt to derive a state or province name from 
 * the specified general name, if the choices and content of the name
 * permit.  If the General Name is a (directory) Name consisting
 * of a Sequence of Relative Distinguished names containing a state or 
 * province, the result will be the value of that attribute.  If the 
 * optional arena argument is non-null, the memory used will be 
 * obtained from that arena; otherwise, the memory will be obtained 
 * from the heap.  This routine may return NULL upon error, in which 
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_STATE_OR_PROVINCE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssGeneralName_GetStateOrProvince
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetLocality
 *
 * This routine will attempt to derive a locality name from 
 * the specified general name, if the choices and content of the name
 * permit.  If the General Name is a (directory) Name consisting
 * of a Sequence of Relative Distinguished names containing a Locality, 
 * the result will be the value of that attribute.  If the optional 
 * arena argument is non-null, the memory used will be obtained from 
 * that arena; otherwise, the memory will be obtained from the heap.  
 * This routine may return NULL upon error, in which case it will have 
 * set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_LOCALITY
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssGeneralName_GetLocality
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetCountry
 *
 * This routine will attempt to derive a country name from the 
 * specified general name, if the choices and content of the name 
 * permit.  If the General Name is a (directory) Name consisting of a
 * Sequence of Relative Distinguished names containing a Country, the 
 * result will be the value of that attribute.  If the optional 
 * arena argument is non-null, the memory used will be obtained from 
 * that arena; otherwise, the memory will be obtained from the heap.  
 * This routine may return NULL upon error, in which case it will have 
 * set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_COUNTRY
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr PrintableString */
nssGeneralName_GetCountry
(
  NSSGeneralName *generalName,
  NSSArena *arenaOpt
);

/*
 * nssGeneralName_GetAttribute
 *
 * If the specified general name is a (directory) name consisting
 * of a Sequence of Relative Distinguished Names containing an 
 * attribute with the specified type, and the actual value of that
 * attribute may be expressed with a Directory String, then the
 * value of that attribute will be returned as a Directory String.
 * If the optional arena argument is non-null, the memory used will
 * be obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return NULL upon error, in which
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_ATTRIBUTE
 *  NSS_ERROR_ATTRIBUTE_VALUE_NOT_STRING
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a UTF8 String
 */

NSS_EXTERN NSSUTF8 * /* XXX fgmr DirectoryString */
nssGeneralName_GetAttribute
(
  NSSGeneralName *generalName,
  NSSOID *attribute,
  NSSArena *arenaOpt
);

/*
 * NSSGeneralNameSeq
 *
 * The public "methods" regarding this "object" are:
 *
 *  nssGeneralNameSeq_CreateFromBER   -- constructor
 *  nssGeneralNameSeq_Create          -- constructor
 *
 *  nssGeneralNameSeq_Destroy
 *  nssGeneralNameSeq_GetDEREncoding
 *  nssGeneralNameSeq_AppendGeneralName
 *  nssGeneralNameSeq_GetGeneralNameCount
 *  nssGeneralNameSeq_GetGeneralName
 *  nssGeneralNameSeq_Compare
 *  nssGeneralnameSeq_Duplicate
 */

/*
 * nssGeneralNameSeq_CreateFromBER
 *
 * This routine creates a general name sequence by decoding a BER-
 * or DER-encoded GeneralNames.  If the optional arena argument is
 * non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine
 * may return NULL upon error, in which case it will have set an error
 * on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSGeneralNameSeq upon success
 */

NSS_EXTERN NSSGeneralNameSeq *
nssGeneralNameSeq_CreateFromBER
(
  NSSArena *arenaOpt,
  NSSBER *berGeneralNameSeq
);

/*
 * nssGeneralNameSeq_Create
 *
 * This routine creates an NSSGeneralNameSeq from one or more General
 * Names.  The final argument to this routine must be NULL.  If the
 * optional arena argument is non-null, the memory used will be 
 * obtained from that arena; otherwise, the memory will be obtained 
 * from the heap.  This routine may return NULL upon error, in which
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_NO_MEMORY
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSGeneralNameSeq upon success
 */

NSS_EXTERN NSSGeneralNameSeq *
nssGeneralNameSeq_Create
(
  NSSArena *arenaOpt,
  NSSGeneralName *generalName1,
  ...
);

/*
 * nssGeneralNameSeq_Destroy
 *
 * This routine will destroy an NSSGeneralNameSeq object.  It should
 * eventually be called on all NSSGeneralNameSeqs created without an
 * arena.  While it is not necessary to call it on NSSGeneralNameSeq's
 * created within an arena, it is not an error to do so.  This routine
 * returns a PRStatus value; if successful, it will return PR_SUCCESS.
 * If unsuccessful, it will set an error on the error stack and return
 * PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME_SEQ
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_EXTERN PRStatus
nssGeneralNameSeq_Destroy
(
  NSSGeneralNameSeq *generalNameSeq
);

/*
 * nssGeneralNameSeq_GetDEREncoding
 *
 * This routine will DER-encode an NSSGeneralNameSeq object.  If the
 * optional arena argument is non-null, the memory used will be 
 * obtained from that arena; otherwise, the memory will be obtained
 * from the heap.  This routine may return null upon error, in which
 * case it will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME_SEQ
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSGeneralNameSeq
 */

NSS_EXTERN NSSDER *
nssGeneralNameSeq_GetDEREncoding
(
  NSSGeneralNameSeq *generalNameSeq,
  NSSArena *arenaOpt
);

/*
 * nssGeneralNameSeq_AppendGeneralName
 *
 * This routine appends a General Name to the end of the existing
 * General Name Sequence.  If the sequence was created with a non-null
 * arena argument, that same arena will be used for any additional
 * required memory.  If the sequence was created with a NULL arena
 * argument, any additional memory will be obtained from the heap.
 * This routine returns a PRStatus value; it will return PR_SUCCESS
 * upon success, and upon failure it will set an error on the error
 * stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME_SEQ
 *  NSS_ERROR_INVALID_GENERAL_NAME
 *  NSS_ERROR_NO_MEMORY
 * 
 * Return value:
 *  PR_SUCCESS upon success
 *  PR_FAILURE upon failure.
 */

NSS_EXTERN PRStatus
nssGeneralNameSeq_AppendGeneralName
(
  NSSGeneralNameSeq *generalNameSeq,
  NSSGeneralName *generalName
);

/*
 * nssGeneralNameSeq_GetGeneralNameCount
 *
 * This routine returns the cardinality of the specified General name
 * Sequence.  This routine may return 0 upon error, in which case it
 * will have set an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME_SEQ
 *
 * Return value;
 *  0 upon error
 *  A positive number upon success
 */

NSS_EXTERN PRUint32
nssGeneralNameSeq_GetGeneralNameCount
(
  NSSGeneralNameSeq *generalNameSeq
);

/*
 * nssGeneralNameSeq_GetGeneralName
 *
 * This routine returns a pointer to the i'th General Name in the 
 * specified General Name Sequence.  The value of the variable 'i' is
 * on the range [0,c) where c is the cardinality returned from 
 * NSSGeneralNameSeq_GetGeneralNameCount.  The caller owns the General
 * Name the pointer to which is returned.  If the optional arena
 * argument is non-null, the memory used will be obtained from that
 * arena; otherwise, the memory will be obtained from the heap.  This
 * routine may return NULL upon error, in which case it will have set
 * an error upon the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME_SEQ
 *  NSS_ERROR_VALUE_OUT_OF_RANGE
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A caller-owned pointer to a General Name.
 */

NSS_EXTERN NSSGeneralName *
nssGeneralNameSeq_GetGeneralName
(
  NSSGeneralNameSeq *generalNameSeq,
  NSSArena *arenaOpt,
  PRUint32 i
);

/*
 * nssGeneralNameSeq_Compare
 *
 * This routine compares two General Name Sequences for equality.  For
 * two General Name Sequences to be equal, they must have the same
 * cardinality, and each General Name in one sequence must be equal to
 * the corresponding General Name in the other.  The result of the
 * comparison will be stored at the location pointed to by the "equalp"
 * variable, which must point to a valid PRBool.  This routine may 
 * return PR_FAILURE upon error, in which case it will have set an 
 * error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME_SEQ
 *  NSS_ERROR_INVALID_ARGUMENT
 *
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon a successful comparison (equal or not)
 */

NSS_EXTERN PRStatus
nssGeneralNameSeq_Compare
(
  NSSGeneralNameSeq *generalNameSeq1,
  NSSGeneralNameSeq *generalNameSeq2,
  PRBool *equalp
);

/*
 * nssGeneralNameSeq_Duplicate
 *
 * This routine duplicates the specified sequence of general names.  If
 * the optional arena argument is non-null, the memory required will be
 * obtained from that arena; otherwise, the memory will be obtained 
 * from the heap.  This routine may return NULL upon error, in which 
 * case it will have placed an error on the error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_GENERAL_NAME_SEQ
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to a new General Name Sequence.
 */

NSS_EXTERN NSSGeneralNameSeq *
nssGeneralNameSeq_Duplicate
(
  NSSGeneralNameSeq *generalNameSeq,
  NSSArena *arenaOpt
);

PR_END_EXTERN_C

#endif /* PKI1_H */
