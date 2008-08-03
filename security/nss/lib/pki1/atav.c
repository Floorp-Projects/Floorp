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

#ifdef DEBUG
static const char CVS_ID[] = "@(#) $RCSfile: atav.c,v $ $Revision: 1.8 $ $Date: 2005/01/20 02:25:49 $";
#endif /* DEBUG */

/*
 * atav.c
 *
 * This file contains the implementation of the PKIX part-1 object
 * AttributeTypeAndValue.
 */

#ifndef NSSBASE_H
#include "nssbase.h"
#endif /* NSSBASE_H */

#ifndef ASN1_H
#include "asn1.h"
#endif /* ASN1_H */

#ifndef PKI1_H
#include "pki1.h"
#endif /* PKI1_H */

/*
 * AttributeTypeAndValue
 *
 * From draft-ietf-pkix-ipki-part1-10:
 *
 *  AttributeTypeAndValue           ::=     SEQUENCE {
 *          type            ATTRIBUTE.&id ({SupportedAttributes}),
 *          value   ATTRIBUTE.&Type ({SupportedAttributes}{@type})}
 *  
 *  -- ATTRIBUTE information object class specification
 *  --  Note: This has been greatly simplified for PKIX !!
 *  
 *  ATTRIBUTE               ::=     CLASS {
 *          &Type,
 *          &id                     OBJECT IDENTIFIER UNIQUE }
 *  WITH SYNTAX {
 *          WITH SYNTAX &Type ID &id }
 *  
 * What this means is that the "type" of the value is determined by
 * the value of the oid.  If we hide the structure, our accessors
 * can (at least in debug builds) assert value semantics beyond what
 * the compiler can provide.  Since these things are only used in
 * RelativeDistinguishedNames, and since RDNs always contain a SET
 * of these things, we don't lose anything by hiding the structure
 * (and its size).
 */

struct NSSATAVStr {
  NSSBER ber;
  const NSSOID *oid;
  NSSUTF8 *value;
  nssStringType stringForm;
};

/*
 * NSSATAV
 *
 * The public "methods" regarding this "object" are:
 *
 *  NSSATAV_CreateFromBER   -- constructor
 *  NSSATAV_CreateFromUTF8  -- constructor
 *  NSSATAV_Create          -- constructor
 *
 *  NSSATAV_Destroy
 *  NSSATAV_GetDEREncoding
 *  NSSATAV_GetUTF8Encoding
 *  NSSATAV_GetType
 *  NSSATAV_GetValue
 *  NSSATAV_Compare
 *  NSSATAV_Duplicate
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
 * NSSATAV_CreateFromBER
 * 
 * This routine creates an NSSATAV by decoding a BER- or DER-encoded
 * ATAV.  If the optional arena argument is non-null, the memory used 
 * will be obtained from that arena; otherwise, the memory will be 
 * obtained from the heap.  This routine may return NULL upon error, 
 * in which case it will have created an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_BER
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSATAV upon success
 */

NSS_IMPLEMENT NSSATAV *
NSSATAV_CreateFromBER
(
  NSSArena *arenaOpt,
  NSSBER *berATAV
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSATAV *)NULL;
    }
  }

  /* 
   * NSSBERs can be created by the user, 
   * so no pointer-tracking can be checked.
   */

  if( (NSSBER *)NULL == berATAV ) {
    nss_SetError(NSS_ERROR_INVALID_BER);
    return (NSSATAV *)NULL;
  }

  if( (void *)NULL == berATAV->data ) {
    nss_SetError(NSS_ERROR_INVALID_BER);
    return (NSSATAV *)NULL;
  }
#endif /* DEBUG */

  return nssATAV_CreateFromBER(arenaOpt, berATAV);
}

/*
 * NSSATAV_CreateFromUTF8
 *
 * This routine creates an NSSATAV by decoding a UTF8 string in the
 * "equals" format, e.g., "c=US."  If the optional arena argument is 
 * non-null, the memory used will be obtained from that arena; 
 * otherwise, the memory will be obtained from the heap.  This routine
 * may return NULL upon error, in which case it will have created an
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_UNKNOWN_ATTRIBUTE
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSATAV upon success
 */

NSS_IMPLEMENT NSSATAV *
NSSATAV_CreateFromUTF8
(
  NSSArena *arenaOpt,
  NSSUTF8 *stringATAV
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSATAV *)NULL;
    }
  }

  /*
   * NSSUTF8s can be created by the user,
   * so no pointer-tracking can be checked.
   */

  if( (NSSUTF8 *)NULL == stringATAV ) {
    nss_SetError(NSS_ERROR_INVALID_UTF8);
    return (NSSATAV *)NULL;
  }
#endif /* DEBUG */

  return nssATAV_CreateFromUTF8(arenaOpt, stringATAV);
}

/*
 * NSSATAV_Create
 *
 * This routine creates an NSSATAV from the specified NSSOID and the
 * specified data. If the optional arena argument is non-null, the 
 * memory used will be obtained from that arena; otherwise, the memory
 * will be obtained from the heap.If the specified data length is zero, 
 * the data is assumed to be terminated by first zero byte; this allows 
 * UTF8 strings to be easily specified.  This routine may return NULL 
 * upon error, in which case it will have created an error stack.
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

NSS_IMPLEMENT NSSATAV *
NSSATAV_Create
(
  NSSArena *arenaOpt,
  const NSSOID *oid,
  const void *data,
  PRUint32 length
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSATAV *)NULL;
    }
  }

  if( PR_SUCCESS != nssOID_verifyPointer(oid) ) {
    return (NSSATAV *)NULL;
  }

  if( (const void *)NULL == data ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (NSSATAV *)NULL;
  }
#endif /* DEBUG */

  return nssATAV_Create(arenaOpt, oid, data, length);
}

/*
 * NSSATAV_Destroy
 *
 * This routine will destroy an ATAV object.  It should eventually be
 * called on all ATAVs created without an arena.  While it is not 
 * necessary to call it on ATAVs created within an arena, it is not an
 * error to do so.  This routine returns a PRStatus value; if
 * successful, it will return PR_SUCCESS.  If unsuccessful, it will
 * create an error stack and return PR_FAILURE.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  
 * Return value:
 *  PR_FAILURE upon error
 *  PR_SUCCESS upon success
 */

NSS_IMPLEMENT PRStatus
NSSATAV_Destroy
(
  NSSATAV *atav
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  return nssATAV_Destroy(atav);
}

/*
 * NSSATAV_GetDEREncoding
 *
 * This routine will DER-encode an ATAV object. If the optional arena
 * argument is non-null, the memory used will be obtained from that
 * arena; otherwise, the memory will be obtained from the heap.  This
 * routine may return null upon error, in which case it will have 
 * created an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  The DER encoding of this NSSATAV
 */

NSS_IMPLEMENT NSSDER *
NSSATAV_GetDEREncoding
(
  NSSATAV *atav,
  NSSArena *arenaOpt
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSDER *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSDER *)NULL;
    }
  }
#endif /* DEBUG */

  return nssATAV_GetDEREncoding(atav, arenaOpt);
}

/*
 * NSSATAV_GetUTF8Encoding
 *
 * This routine returns a UTF8 string containing a string 
 * representation of the ATAV in "equals" notation (e.g., "o=Acme").  
 * If the optional arena argument is non-null, the memory used will be
 * obtained from that arena; otherwise, the memory will be obtained 
 * from the heap.  This routine may return null upon error, in which 
 * case it will have created an error stack.
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

NSS_IMPLEMENT NSSUTF8 *
NSSATAV_GetUTF8Encoding
(
  NSSATAV *atav,
  NSSArena *arenaOpt
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSUTF8 *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSUTF8 *)NULL;
    }
  }
#endif /* DEBUG */

  return nssATAV_GetUTF8Encoding(atav, arenaOpt);
}

/*
 * NSSATAV_GetType
 *
 * This routine returns the NSSOID corresponding to the attribute type
 * in the specified ATAV.  This routine may return NSS_OID_UNKNOWN 
 * upon error, in which case it will have created an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *
 * Return value:
 *  NSS_OID_UNKNOWN upon error
 *  An element of enum NSSOIDenum upon success
 */

NSS_IMPLEMENT const NSSOID *
NSSATAV_GetType
(
  NSSATAV *atav
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSOID *)NULL;
  }
#endif /* DEBUG */

  return nssATAV_GetType(atav);
}

/*
 * NSSATAV_GetValue
 *
 * This routine returns a string containing the attribute value
 * in the specified ATAV.  If the optional arena argument is non-null,
 * the memory used will be obtained from that arena; otherwise, the
 * memory will be obtained from the heap.  This routine may return
 * NULL upon error, in which case it will have created an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSItem containing the attribute value.
 */

NSS_IMPLEMENT NSSUTF8 *
NSSATAV_GetValue
(
  NSSATAV *atav,
  NSSArena *arenaOpt
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSUTF8 *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSUTF8 *)NULL;
    }
  }
#endif /* DEBUG */

  return nssATAV_GetValue(atav, arenaOpt);
}

/*
 * NSSATAV_Compare
 *
 * This routine compares two ATAVs for equality.  For two ATAVs to be
 * equal, the attribute types must be the same, and the attribute 
 * values must have equal length and contents.  The result of the 
 * comparison will be stored at the location pointed to by the "equalp"
 * variable, which must point to a valid PRBool.  This routine may 
 * return PR_FAILURE upon error, in which case it will have created an
 * error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_INVALID_ARGUMENT
 *
 * Return value:
 *  PR_FAILURE on error
 *  PR_SUCCESS upon a successful comparison (equal or not)
 */

NSS_IMPLEMENT PRStatus
NSSATAV_Compare
(
  NSSATAV *atav1,
  NSSATAV *atav2,
  PRBool *equalp
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav1) ) {
    return PR_FAILURE;
  }

  if( PR_SUCCESS != nssATAV_verifyPointer(atav2) ) {
    return PR_FAILURE;
  }

  if( (PRBool *)NULL == equalp ) {
    nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
    return PR_FAILURE;
  }
#endif /* DEBUG */

  return nssATAV_Compare(atav1, atav2, equalp);
}

/*
 * NSSATAV_Duplicate
 *
 * This routine duplicates the specified ATAV.  If the optional arena 
 * argument is non-null, the memory required will be obtained from
 * that arena; otherwise, the memory will be obtained from the heap.  
 * This routine may return NULL upon error, in which case it will have 
 * created an error stack.
 *
 * The error may be one of the following values:
 *  NSS_ERROR_INVALID_ATAV
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL on error
 *  A pointer to a new ATAV
 */

NSS_IMPLEMENT NSSATAV *
NSSATAV_Duplicate
(
  NSSATAV *atav,
  NSSArena *arenaOpt
)
{
  nss_ClearErrorStack();

#ifdef DEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSATAV *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSATAV *)NULL;
    }
  }
#endif /* DEBUG */

  return nssATAV_Duplicate(atav, arenaOpt);
}

/*
 * The pointer-tracking code
 */

#ifdef DEBUG
extern const NSSError NSS_ERROR_INTERNAL_ERROR;

static nssPointerTracker atav_pointer_tracker;

static PRStatus
atav_add_pointer
(
  const NSSATAV *atav
)
{
  PRStatus rv;

  rv = nssPointerTracker_initialize(&atav_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    return rv;
  }

  rv = nssPointerTracker_add(&atav_pointer_tracker, atav);
  if( PR_SUCCESS != rv ) {
    NSSError e = NSS_GetError();
    if( NSS_ERROR_NO_MEMORY != e ) {
      nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    }

    return rv;
  }

  return PR_SUCCESS;
}

static PRStatus
atav_remove_pointer
(
  const NSSATAV *atav
)
{
  PRStatus rv;

  rv = nssPointerTracker_remove(&atav_pointer_tracker, atav);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
  }

  return rv;
}

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

NSS_IMPLEMENT PRStatus
nssATAV_verifyPointer
(
  NSSATAV *atav
)
{
  PRStatus rv;

  rv = nssPointerTracker_initialize(&atav_pointer_tracker);
  if( PR_SUCCESS != rv ) {
    return PR_FAILURE;
  }

  rv = nssPointerTracker_verify(&atav_pointer_tracker, atav);
  if( PR_SUCCESS != rv ) {
    nss_SetError(NSS_ERROR_INVALID_ATAV);
    return PR_FAILURE;
  }

  return PR_SUCCESS;
}
#endif /* DEBUG */

typedef struct {
  NSSBER oid;
  NSSBER value;
} atav_holder;

static const nssASN1Template nss_atav_template[] = {
  { nssASN1_SEQUENCE, 0, NULL, sizeof(atav_holder) },
  { nssASN1_OBJECT_ID, nsslibc_offsetof(atav_holder, oid), NULL, 0 },
  { nssASN1_ANY, nsslibc_offsetof(atav_holder, value), NULL, 0 },
  { 0, 0, NULL, 0 }
};

/*
 * There are several common attributes, with well-known type aliases
 * and value semantics.  This table lists the ones we recognize.
 */

struct nss_attribute_data_str {
  const NSSOID **oid;
  nssStringType stringType;
  PRUint32 minStringLength;
  PRUint32 maxStringLength; /* zero for no limit */
};

static const struct nss_attribute_data_str nss_attribute_data[] = {
  { &NSS_OID_X520_NAME,                     
    nssStringType_DirectoryString, 1, 32768 },
  { &NSS_OID_X520_COMMON_NAME,              
    nssStringType_DirectoryString, 1,    64 },
  { &NSS_OID_X520_SURNAME,                  
    nssStringType_DirectoryString, 1,    40 },
  { &NSS_OID_X520_GIVEN_NAME,               
    nssStringType_DirectoryString, 1,    16 },
  { &NSS_OID_X520_INITIALS,                 
    nssStringType_DirectoryString, 1,     5 },
  { &NSS_OID_X520_GENERATION_QUALIFIER,     
    nssStringType_DirectoryString, 1,     3 },
  { &NSS_OID_X520_DN_QUALIFIER,             
    nssStringType_PrintableString, 1,     0 },
  { &NSS_OID_X520_COUNTRY_NAME,             
    nssStringType_PrintableString, 2,     2 },
  { &NSS_OID_X520_LOCALITY_NAME,            
    nssStringType_DirectoryString, 1,   128 },
  { &NSS_OID_X520_STATE_OR_PROVINCE_NAME,   
    nssStringType_DirectoryString, 1,   128 },
  { &NSS_OID_X520_ORGANIZATION_NAME,        
    nssStringType_DirectoryString, 1,    64 },
  { &NSS_OID_X520_ORGANIZATIONAL_UNIT_NAME, 
    nssStringType_DirectoryString, 1,
    /*
     * Note, draft #11 defines both "32" and "64" for this maximum,
     * in two separate places.  Until it's settled, "conservative
     * in what you send."  We're always liberal in what we accept.
     */
                                         32 },
  { &NSS_OID_X520_TITLE,                    
    nssStringType_DirectoryString, 1,    64 },
  { &NSS_OID_RFC1274_EMAIL,                 
    nssStringType_PHGString,       1,   128 }
};

PRUint32 nss_attribute_data_quantity = 
  (sizeof(nss_attribute_data)/sizeof(nss_attribute_data[0]));

static nssStringType
nss_attr_underlying_string_form
(
  nssStringType type,
  void *data
)
{
  if( nssStringType_DirectoryString == type ) {
    PRUint8 tag = *(PRUint8 *)data;
    switch( tag & nssASN1_TAGNUM_MASK ) {
    case 20:
      /*
       * XXX fgmr-- we have to accept Latin-1 for Teletex; (see
       * below) but is T61 a suitable value for "Latin-1"?
       */
      return nssStringType_TeletexString;
    case 19:
      return nssStringType_PrintableString;
    case 28:
      return nssStringType_UniversalString;
    case 30:
      return nssStringType_BMPString;
    case 12:
      return nssStringType_UTF8String;
    default:
      return nssStringType_Unknown;
    }
  }

  return type;
}
    

/*
 * This routine decodes the attribute value, in a type-specific way.
 *
 */

static NSSUTF8 *
nss_attr_to_utf8
(
  NSSArena *arenaOpt,
  const NSSOID *oid,
  NSSItem *item,
  nssStringType *stringForm
)
{
  NSSUTF8 *rv = (NSSUTF8 *)NULL;
  PRUint32 i;
  const struct nss_attribute_data_str *which = 
    (struct nss_attribute_data_str *)NULL;
  PRUint32 len = 0;

  for( i = 0; i < nss_attribute_data_quantity; i++ ) {
    if( *(nss_attribute_data[ i ].oid) == oid ) {
      which = &nss_attribute_data[i];
      break;
    }
  }

  if( (struct nss_attribute_data_str *)NULL == which ) {
    /* Unknown OID.  Encode it as hex. */
    PRUint8 *c;
    PRUint8 *d = (PRUint8 *)item->data;
    PRUint32 amt = item->size;

    if( item->size >= 0x7FFFFFFF ) {
      nss_SetError(NSS_ERROR_INVALID_STRING);
      return (NSSUTF8 *)NULL;
    }

    len = 1 + (item->size * 2) + 1; /* '#' + hex + '\0' */
    rv = (NSSUTF8 *)nss_ZAlloc(arenaOpt, len);
    if( (NSSUTF8 *)NULL == rv ) {
      return (NSSUTF8 *)NULL;
    }

    c = (PRUint8 *)rv;
    *c++ = '#'; /* XXX fgmr check this */
    while( amt > 0 ) {
      static char hex[16] = "0123456789ABCDEF";
      *c++ = hex[ ((*d) & 0xf0) >> 4 ];
      *c++ = hex[ ((*d) & 0x0f)      ];
    }

    /* *c = '\0'; nss_ZAlloc, remember */

    *stringForm = nssStringType_Unknown; /* force exact comparison */
  } else {
    PRStatus status;
    rv = nssUTF8_CreateFromBER(arenaOpt, which->stringType, 
                               (NSSBER *)item);

    if( (NSSUTF8 *)NULL == rv ) {
      return (NSSUTF8 *)NULL;
    }

    len = nssUTF8_Length(rv, &status);
    if( PR_SUCCESS != status || len == 0 ) {
      nss_ZFreeIf(rv);
      return (NSSUTF8 *)NULL;
    }

    *stringForm = nss_attr_underlying_string_form(which->stringType,
                                                  item->data);
  }

  return rv;
}

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

NSS_IMPLEMENT NSSATAV *
nssATAV_CreateFromBER
(
  NSSArena *arenaOpt,
  const NSSBER *berATAV
)
{
  atav_holder holder;
  PRStatus status;
  NSSATAV *rv;

#ifdef NSSDEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSATAV *)NULL;
    }
  }

  /* 
   * NSSBERs can be created by the user, 
   * so no pointer-tracking can be checked.
   */

  if( (NSSBER *)NULL == berATAV ) {
    nss_SetError(NSS_ERROR_INVALID_BER);
    return (NSSATAV *)NULL;
  }

  if( (void *)NULL == berATAV->data ) {
    nss_SetError(NSS_ERROR_INVALID_BER);
    return (NSSATAV *)NULL;
  }
#endif /* NSSDEBUG */

  status = nssASN1_DecodeBER(arenaOpt, &holder, 
                             nss_atav_template, berATAV);
  if( PR_SUCCESS != status ) {
    return (NSSATAV *)NULL;
  }

  rv = nss_ZNEW(arenaOpt, NSSATAV);
  if( (NSSATAV *)NULL == rv ) {
    nss_ZFreeIf(holder.oid.data);
    nss_ZFreeIf(holder.value.data);
    return (NSSATAV *)NULL;
  }

  rv->oid = nssOID_CreateFromBER(&holder.oid);
  if( (NSSOID *)NULL == rv->oid ) {
    nss_ZFreeIf(rv);
    nss_ZFreeIf(holder.oid.data);
    nss_ZFreeIf(holder.value.data);
    return (NSSATAV *)NULL;
  }

  nss_ZFreeIf(holder.oid.data);

  rv->ber.data = nss_ZAlloc(arenaOpt, berATAV->size);
  if( (void *)NULL == rv->ber.data ) {
    nss_ZFreeIf(rv);
    nss_ZFreeIf(holder.value.data);
    return (NSSATAV *)NULL;
  }

  rv->ber.size = berATAV->size;
  (void)nsslibc_memcpy(rv->ber.data, berATAV->data, berATAV->size);

  rv->value = nss_attr_to_utf8(arenaOpt, rv->oid, &holder.value,
                               &rv->stringForm);
  if( (NSSUTF8 *)NULL == rv->value ) {
    nss_ZFreeIf(rv->ber.data);
    nss_ZFreeIf(rv);
    nss_ZFreeIf(holder.value.data);
    return (NSSATAV *)NULL;
  }

  nss_ZFreeIf(holder.value.data);

#ifdef DEBUG
  if( PR_SUCCESS != atav_add_pointer(rv) ) {
    nss_ZFreeIf(rv->ber.data);
    nss_ZFreeIf(rv->value);
    nss_ZFreeIf(rv);
    return (NSSATAV *)NULL;
  }
#endif /* DEBUG */

  return rv;
}

static PRBool
nss_atav_utf8_string_is_hex
(
  const NSSUTF8 *s
)
{
  /* All hex digits are ASCII, so this works */
  PRUint8 *p = (PRUint8 *)s;

  for( ; (PRUint8)0 != *p; p++ ) {
    if( (('0' <= *p) && (*p <= '9')) ||
        (('A' <= *p) && (*p <= 'F')) ||
        (('a' <= *p) && (*p <= 'f')) ) {
      continue;
    } else {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

static NSSUTF8
nss_atav_fromhex
(
  NSSUTF8 *d
)
{
  NSSUTF8 rv;

  if( d[0] <= '9' ) {
    rv = (d[0] - '0') * 16;
  } else if( d[0] >= 'a' ) {
    rv = (d[0] - 'a' + 10) * 16;
  } else {
    rv = (d[0] - 'A' + 10);
  }

  if( d[1] <= '9' ) {
    rv += (d[1] - '0');
  } else if( d[1] >= 'a' ) {
    rv += (d[1] - 'a' + 10);
  } else {
    rv += (d[1] - 'A' + 10);
  }

  return rv;
}

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
 *  NSS_ERROR_INVALID_STRING
 *  NSS_ERROR_NO_MEMORY
 *
 * Return value:
 *  NULL upon error
 *  A pointer to an NSSATAV upon success
 */

extern const NSSError NSS_ERROR_INTERNAL_ERROR;

NSS_IMPLEMENT NSSATAV *
nssATAV_CreateFromUTF8
(
  NSSArena *arenaOpt,
  const NSSUTF8 *stringATAV
)
{
  char *c;
  NSSUTF8 *type;
  NSSUTF8 *value;
  PRUint32 i;
  const NSSOID *oid = (NSSOID *)NULL;
  NSSATAV *rv;
  NSSItem xitem;

  xitem.data = (void *)NULL;

  for( c = (char *)stringATAV; '\0' != *c; c++ ) {
    if( '=' == *c ) {
#ifdef PEDANTIC
      /*
       * Theoretically, one could have an '=' in an 
       * attribute string alias.  We don't, yet, though.
       */
      if( (char *)stringATAV == c ) {
        nss_SetError(NSS_ERROR_INVALID_STRING);
        return (NSSATAV *)NULL;
      } else {
        if( '\\' == c[-1] ) {
          continue;
        }
      }
#endif /* PEDANTIC */
      break;
    }
  }

  if( '\0' == *c ) {
    nss_SetError(NSS_ERROR_INVALID_UTF8);
    return (NSSATAV *)NULL;
  } else {
    c++;
    value = (NSSUTF8 *)c;
  }

  i = ((NSSUTF8 *)c - stringATAV);
  type = (NSSUTF8 *)nss_ZAlloc((NSSArena *)NULL, i);
  if( (NSSUTF8 *)NULL == type ) {
    return (NSSATAV *)NULL;
  }

  (void)nsslibc_memcpy(type, stringATAV, i-1);

  c = (char *)stringATAV;
  if( (('0' <= *c) && (*c <= '9')) || ('#' == *c) ) {
    oid = nssOID_CreateFromUTF8(type);
    if( (NSSOID *)NULL == oid ) {
      nss_ZFreeIf(type);
      return (NSSATAV *)NULL;
    }
  } else {
    for( i = 0; i < nss_attribute_type_alias_count; i++ ) {
      PRStatus status;
      const nssAttributeTypeAliasTable *e = &nss_attribute_type_aliases[i];
      PRBool match = nssUTF8_CaseIgnoreMatch(type, e->alias, &status);
      if( PR_SUCCESS != status ) {
        nss_ZFreeIf(type);
        return (NSSATAV *)NULL;
      }
      if( PR_TRUE == match ) {
        oid = *(e->oid);
        break;
      }
    }

    if( (NSSOID *)NULL == oid ) {
      nss_ZFreeIf(type);
      nss_SetError(NSS_ERROR_UNKNOWN_ATTRIBUTE);
      return (NSSATAV *)NULL;
    }
  }

  nss_ZFreeIf(type);
  type = (NSSUTF8 *)NULL;

  rv = nss_ZNEW(arenaOpt, NSSATAV);
  if( (NSSATAV *)NULL == rv ) {
    return (NSSATAV *)NULL;
  }

  rv->oid = oid;

  if( '#' == *value ) { /* XXX fgmr.. was it '#'?  or backslash? */
    PRUint32 size;
    PRUint32 len;
    NSSUTF8 *c;
    NSSUTF8 *d;
    PRStatus status;
    /* It's in hex */

    value++;
    if( PR_TRUE != nss_atav_utf8_string_is_hex(value) ) {
      (void)nss_ZFreeIf(rv);
      nss_SetError(NSS_ERROR_INVALID_STRING);
      return (NSSATAV *)NULL;
    }

    size = nssUTF8_Size(value, &status);
    if( PR_SUCCESS != status ) {
      /* 
       * Only returns an error on bad pointer (nope) or string
       * too long.  The defined limits for known attributes are
       * small enough to fit in PRUint32, and when undefined we
       * get to apply our own practical limits.  Ergo, I say the 
       * string is invalid.
       */
      (void)nss_ZFreeIf(rv);
      nss_SetError(NSS_ERROR_INVALID_STRING);
      return (NSSATAV *)NULL;
    }

    if( ((size-1) & 1) ) {
      /* odd length */
      (void)nss_ZFreeIf(rv);
      nss_SetError(NSS_ERROR_INVALID_STRING);
      return (NSSATAV *)NULL;
    }

    len = (size-1)/2;

    rv->value = (NSSUTF8 *)nss_ZAlloc(arenaOpt, len+1);
    if( (NSSUTF8 *)NULL == rv->value ) {
      (void)nss_ZFreeIf(rv);
      return (NSSATAV *)NULL;
    }

    xitem.size = len;
    xitem.data = (void *)rv->value;

    for( c = rv->value, d = value; len--; c++, d += 2 ) {
      *c = nss_atav_fromhex(d);
    }

    *c = 0;
  } else {
    PRStatus status;
    PRUint32 i, len;
    PRUint8 *s;

    /*
     * XXX fgmr-- okay, this is a little wasteful, and should
     * probably be abstracted out a bit.  Later.
     */

    rv->value = nssUTF8_Duplicate(value, arenaOpt);
    if( (NSSUTF8 *)NULL == rv->value ) {
      (void)nss_ZFreeIf(rv);
      return (NSSATAV *)NULL;
    }

    len = nssUTF8_Size(rv->value, &status);
    if( PR_SUCCESS != status ) {
      (void)nss_ZFreeIf(rv->value);
      (void)nss_ZFreeIf(rv);
      return (NSSATAV *)NULL;
    }

    s = (PRUint8 *)rv->value;
    for( i = 0; i < len; i++ ) {
      if( '\\' == s[i] ) {
        (void)nsslibc_memcpy(&s[i], &s[i+1], len-i-1);
      }
    }
  }

  /* Now just BER-encode the baby and we're through.. */
  {
    const struct nss_attribute_data_str *which = 
      (struct nss_attribute_data_str *)NULL;
    PRUint32 i;
    NSSArena *a;
    NSSDER *oidder;
    NSSItem *vitem;
    atav_holder ah;
    NSSDER *status;

    for( i = 0; i < nss_attribute_data_quantity; i++ ) {
      if( *(nss_attribute_data[ i ].oid) == rv->oid ) {
        which = &nss_attribute_data[i];
        break;
      }
    }

    a = NSSArena_Create();
    if( (NSSArena *)NULL == a ) {
      (void)nss_ZFreeIf(rv->value);
      (void)nss_ZFreeIf(rv);
      return (NSSATAV *)NULL;
    }

    oidder = nssOID_GetDEREncoding(rv->oid, (NSSDER *)NULL, a);
    if( (NSSDER *)NULL == oidder ) {
      (void)NSSArena_Destroy(a);
      (void)nss_ZFreeIf(rv->value);
      (void)nss_ZFreeIf(rv);
      return (NSSATAV *)NULL;
    }

    if( (struct nss_attribute_data_str *)NULL == which ) {
      /*
       * We'll just have to take the user data as an octet stream.
       */
      if( (void *)NULL == xitem.data ) {
        /*
         * This means that an ATTR entry has been added to oids.txt,
         * but no corresponding entry has been added to the array
         * ns_attribute_data[] above.
         */
        nss_SetError(NSS_ERROR_INTERNAL_ERROR);
        (void)NSSArena_Destroy(a);
        (void)nss_ZFreeIf(rv->value);
        (void)nss_ZFreeIf(rv);
        return (NSSATAV *)NULL;
      }

      vitem = nssASN1_EncodeItem(a, (NSSDER *)NULL, &xitem, 
                                 nssASN1Template_OctetString, NSSASN1DER);
      if( (NSSItem *)NULL == vitem ) {
        (void)NSSArena_Destroy(a);
        (void)nss_ZFreeIf(rv->value);
        (void)nss_ZFreeIf(rv);
        return (NSSATAV *)NULL;
      }

      rv->stringForm = nssStringType_Unknown;
    } else {
      PRUint32 length = 0;
      PRStatus stat;
      
      length = nssUTF8_Length(rv->value, &stat);
      if( PR_SUCCESS != stat ) {
        (void)NSSArena_Destroy(a);
        (void)nss_ZFreeIf(rv->value);
        (void)nss_ZFreeIf(rv);
        return (NSSATAV *)NULL;
      }

      if( ((0 != which->minStringLength) && 
           (length < which->minStringLength)) ||
          ((0 != which->maxStringLength) &&
           (length > which->maxStringLength)) ) {
        nss_SetError(NSS_ERROR_INVALID_STRING);
        (void)NSSArena_Destroy(a);
        (void)nss_ZFreeIf(rv->value);
        (void)nss_ZFreeIf(rv);
        return (NSSATAV *)NULL;
      }

      vitem = nssUTF8_GetDEREncoding(a, which->stringType, rv->value);
      if( (NSSItem *)NULL == vitem ) {
        (void)NSSArena_Destroy(a);
        (void)nss_ZFreeIf(rv->value);
        (void)nss_ZFreeIf(rv);
        return (NSSATAV *)NULL;
      }

      if( nssStringType_DirectoryString == which->stringType ) {
        rv->stringForm = nssStringType_UTF8String;
      } else {
        rv->stringForm = which->stringType;
      }
    }

    ah.oid = *oidder;
    ah.value = *vitem;

    status = nssASN1_EncodeItem(arenaOpt, &rv->ber, &ah, 
                                nss_atav_template, NSSASN1DER);

    if( (NSSDER *)NULL == status ) {
      (void)NSSArena_Destroy(a);
      (void)nss_ZFreeIf(rv->value);
      (void)nss_ZFreeIf(rv);
      return (NSSATAV *)NULL;
    }

    (void)NSSArena_Destroy(a);
  }

  return rv;
}

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

NSS_IMPLEMENT NSSATAV *
nssATAV_Create
(
  NSSArena *arenaOpt,
  const NSSOID *oid,
  const void *data,
  PRUint32 length
)
{
#ifdef NSSDEBUG
  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSATAV *)NULL;
    }
  }

  if( PR_SUCCESS != nssOID_verifyPointer(oid) ) {
    return (NSSATAV *)NULL;
  }

  if( (const void *)NULL == data ) {
    nss_SetError(NSS_ERROR_INVALID_POINTER);
    return (NSSATAV *)NULL;
  }
#endif /* NSSDEBUG */

  /* XXX fgmr-- oops, forgot this one */
  return (NSSATAV *)NULL;
}

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

NSS_IMPLEMENT PRStatus
nssATAV_Destroy
(
  NSSATAV *atav
)
{
#ifdef NSSDEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return PR_FAILURE;
  }
#endif /* NSSDEBUG */

  (void)nss_ZFreeIf(atav->ber.data);
  (void)nss_ZFreeIf(atav->value);

#ifdef DEBUG
  if( PR_SUCCESS != atav_remove_pointer(atav) ) {
    return PR_FAILURE;
  }
#endif /* DEBUG */

  return PR_SUCCESS;
}

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

NSS_IMPLEMENT NSSDER *
nssATAV_GetDEREncoding
(
  NSSATAV *atav,
  NSSArena *arenaOpt
)
{
  NSSDER *rv;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSDER *)NULL;
  }
#endif /* NSSDEBUG */

  rv = nss_ZNEW(arenaOpt, NSSDER);
  if( (NSSDER *)NULL == rv ) {
    return (NSSDER *)NULL;
  }

  rv->data = nss_ZAlloc(arenaOpt, atav->ber.size);
  if( (void *)NULL == rv->data ) {
    (void)nss_ZFreeIf(rv);
    return (NSSDER *)NULL;
  }

  rv->size = atav->ber.size;
  if( NULL == nsslibc_memcpy(rv->data, atav->ber.data, rv->size) ) {
    (void)nss_ZFreeIf(rv->data);
    (void)nss_ZFreeIf(rv);
    return (NSSDER *)NULL;
  }

  return rv;
}

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

NSS_IMPLEMENT NSSUTF8 *
nssATAV_GetUTF8Encoding
(
  NSSATAV *atav,
  NSSArena *arenaOpt
)
{
  NSSUTF8 *rv;
  PRUint32 i;
  const NSSUTF8 *alias = (NSSUTF8 *)NULL;
  NSSUTF8 *oid;
  NSSUTF8 *value;
  PRUint32 oidlen;
  PRUint32 valuelen;
  PRUint32 totallen;
  PRStatus status;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSUTF8 *)NULL;
  }
#endif /* NSSDEBUG */

  for( i = 0; i < nss_attribute_type_alias_count; i++ ) {
    if( *(nss_attribute_type_aliases[i].oid) == atav->oid ) {
      alias = nss_attribute_type_aliases[i].alias;
      break;
    }
  }

  if( (NSSUTF8 *)NULL == alias ) {
    oid = nssOID_GetUTF8Encoding(atav->oid, (NSSArena *)NULL);
    if( (NSSUTF8 *)NULL == oid ) {
      return (NSSUTF8 *)NULL;
    }

    oidlen = nssUTF8_Size(oid, &status);
    if( PR_SUCCESS != status ) {
      (void)nss_ZFreeIf(oid);
      return (NSSUTF8 *)NULL;
    }
  } else {
    oidlen = nssUTF8_Size(alias, &status);
    if( PR_SUCCESS != status ) {
      return (NSSUTF8 *)NULL;
    }
    oid = (NSSUTF8 *)NULL;
  }

  value = nssATAV_GetValue(atav, (NSSArena *)NULL);
  if( (NSSUTF8 *)NULL == value ) {
    (void)nss_ZFreeIf(oid);
    return (NSSUTF8 *)NULL;
  }

  valuelen = nssUTF8_Size(value, &status);
  if( PR_SUCCESS != status ) {
    (void)nss_ZFreeIf(value);
    (void)nss_ZFreeIf(oid);
    return (NSSUTF8 *)NULL;
  }

  totallen = oidlen + valuelen - 1 + 1;
  rv = (NSSUTF8 *)nss_ZAlloc(arenaOpt, totallen);
  if( (NSSUTF8 *)NULL == rv ) {
    (void)nss_ZFreeIf(value);
    (void)nss_ZFreeIf(oid);
    return (NSSUTF8 *)NULL;
  }

  if( (NSSUTF8 *)NULL == alias ) {
    if( (void *)NULL == nsslibc_memcpy(rv, oid, oidlen-1) ) {
      (void)nss_ZFreeIf(rv);
      (void)nss_ZFreeIf(value);
      (void)nss_ZFreeIf(oid);
      return (NSSUTF8 *)NULL;
    }
  } else {
    if( (void *)NULL == nsslibc_memcpy(rv, alias, oidlen-1) ) {
      (void)nss_ZFreeIf(rv);
      (void)nss_ZFreeIf(value);
      return (NSSUTF8 *)NULL;
    }
  }

  rv[ oidlen-1 ] = '=';

  if( (void *)NULL == nsslibc_memcpy(&rv[oidlen], value, valuelen) ) {
    (void)nss_ZFreeIf(rv);
    (void)nss_ZFreeIf(value);
    (void)nss_ZFreeIf(oid);
    return (NSSUTF8 *)NULL;
  }

  return rv;
}

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
 *  A valid NSSOID pointer upon success
 */

NSS_IMPLEMENT const NSSOID *
nssATAV_GetType
(
  NSSATAV *atav
)
{
#ifdef NSSDEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSOID *)NULL;
  }
#endif /* NSSDEBUG */

  return atav->oid;
}

/*
 * nssATAV_GetValue
 *
 * This routine returns a NSSUTF8 string containing the attribute value
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

NSS_IMPLEMENT NSSUTF8 *
nssATAV_GetValue
(
  NSSATAV *atav,
  NSSArena *arenaOpt
)
{
#ifdef NSSDEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSUTF8 *)NULL;
  }
#endif /* NSSDEBUG */

  return nssUTF8_Duplicate(atav->value, arenaOpt);
}

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

NSS_IMPLEMENT PRStatus
nssATAV_Compare
(
  NSSATAV *atav1,
  NSSATAV *atav2,
  PRBool *equalp
)
{
  nssStringType comparison;
  PRUint32 len1;
  PRUint32 len2;
  PRStatus status;

#ifdef DEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav1) ) {
    return PR_FAILURE;
  }

  if( PR_SUCCESS != nssATAV_verifyPointer(atav2) ) {
    return PR_FAILURE;
  }

  if( (PRBool *)NULL == equalp ) {
    nss_SetError(NSS_ERROR_INVALID_ARGUMENT);
    return PR_FAILURE;
  }
#endif /* DEBUG */

  if( atav1->oid != atav2->oid ) {
    *equalp = PR_FALSE;
    return PR_SUCCESS;
  }

  if( atav1->stringForm != atav2->stringForm ) {
    if( (nssStringType_PrintableString == atav1->stringForm) ||
        (nssStringType_PrintableString == atav2->stringForm) ) {
      comparison = nssStringType_PrintableString;
    } else if( (nssStringType_PHGString == atav1->stringForm) ||
               (nssStringType_PHGString == atav2->stringForm) ) {
      comparison = nssStringType_PHGString;
    } else {
      comparison = atav1->stringForm;
    }
  } else {
    comparison = atav1->stringForm;
  }

  switch( comparison ) {
  case nssStringType_DirectoryString:
    nss_SetError(NSS_ERROR_INTERNAL_ERROR);
    return PR_FAILURE;
  case nssStringType_TeletexString:
    break;
  case nssStringType_PrintableString:
    *equalp = nssUTF8_PrintableMatch(atav1->value, atav2->value, &status);
    return status;
    /* Case-insensitive, with whitespace reduction */
    break;
  case nssStringType_UniversalString:
    break;
  case nssStringType_BMPString:
    break;
  case nssStringType_GeneralString:
    /* what to do here? */
    break;
  case nssStringType_UTF8String:
    break;
  case nssStringType_PHGString:
    /* Case-insensitive (XXX fgmr, actually see draft-11 pg. 21) */
    *equalp = nssUTF8_CaseIgnoreMatch(atav1->value, atav2->value, &status);
    return status;
  case nssStringType_Unknown:
    break;
  }
  
  len1 = nssUTF8_Size(atav1->value, &status);
  if( PR_SUCCESS != status ) {
    return PR_FAILURE;
  }

  len2 = nssUTF8_Size(atav2->value, &status);
  if( PR_SUCCESS != status ) {
    return PR_FAILURE;
  }

  if( len1 != len2 ) {
    *equalp = PR_FALSE;
    return PR_SUCCESS;
  }

  *equalp = nsslibc_memequal(atav1->value, atav2->value, len1, &status);
  return status;
}


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

NSS_IMPLEMENT NSSATAV *
nssATAV_Duplicate
(
  NSSATAV *atav,
  NSSArena *arenaOpt
)
{
  NSSATAV *rv;

#ifdef NSSDEBUG
  if( PR_SUCCESS != nssATAV_verifyPointer(atav) ) {
    return (NSSATAV *)NULL;
  }

  if( (NSSArena *)NULL != arenaOpt ) {
    if( PR_SUCCESS != nssArena_verifyPointer(arenaOpt) ) {
      return (NSSATAV *)NULL;
    }
  }
#endif /* NSSDEBUG */

  rv = nss_ZNEW(arenaOpt, NSSATAV);
  if( (NSSATAV *)NULL == rv ) {
    return (NSSATAV *)NULL;
  }

  rv->oid = atav->oid;
  rv->stringForm = atav->stringForm;
  rv->value = nssUTF8_Duplicate(atav->value, arenaOpt);
  if( (NSSUTF8 *)NULL == rv->value ) {
    (void)nss_ZFreeIf(rv);
    return (NSSATAV *)NULL;
  }

  rv->ber.data = nss_ZAlloc(arenaOpt, atav->ber.size);
  if( (void *)NULL == rv->ber.data ) {
    (void)nss_ZFreeIf(rv->value);
    (void)nss_ZFreeIf(rv);
    return (NSSATAV *)NULL;
  }

  rv->ber.size = atav->ber.size;
  if( NULL == nsslibc_memcpy(rv->ber.data, atav->ber.data, 
                                   atav->ber.size) ) {
    (void)nss_ZFreeIf(rv->ber.data);
    (void)nss_ZFreeIf(rv->value);
    (void)nss_ZFreeIf(rv);
    return (NSSATAV *)NULL;
  }

  return rv;
}
