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

#ifndef ASN1T_H
#define ASN1T_H

#ifdef DEBUG
static const char ASN1T_CVS_ID[] = "@(#) $RCSfile: asn1t.h,v $ $Revision: 1.4 $ $Date: 2005/01/20 02:25:44 $";
#endif /* DEBUG */

/*
 * asn1t.h
 *
 * This file contains the ASN.1 encoder/decoder types available 
 * internally within NSS.  It's not clear right now if this file
 * will be folded into baset.h or something, I just needed to
 * get this going.  At the moment, these types are wrappers for
 * the old types.
 */

#ifndef BASET_H
#include "baset.h"
#endif /* BASET_H */

#ifndef NSSASN1T_H
#include "nssasn1t.h"
#endif /* NSSASN1T_H */

#include "seccomon.h"
#include "secasn1t.h"

PR_BEGIN_EXTERN_C

/*
 * XXX fgmr
 *
 * This sort of bites.  Let's keep an eye on this, to make sure
 * we aren't stuck with it forever.
 */

struct nssASN1ItemStr {
  PRUint32 reserved;
  PRUint8 *data;
  PRUint32 size;
};

typedef struct nssASN1ItemStr nssASN1Item;

/*
 * I'm not documenting these here, since this'll require another
 * pass anyway.
 */

typedef SEC_ASN1Template nssASN1Template;

#define nssASN1_TAG_MASK               SEC_ASN1_TAG_MASK

#define nssASN1_TAGNUM_MASK            SEC_ASN1_TAGNUM_MASK
#define nssASN1_BOOLEAN                SEC_ASN1_BOOLEAN
#define nssASN1_INTEGER                SEC_ASN1_INTEGER
#define nssASN1_BIT_STRING             SEC_ASN1_BIT_STRING
#define nssASN1_OCTET_STRING           SEC_ASN1_OCTET_STRING
#define nssASN1_NULL                   SEC_ASN1_NULL
#define nssASN1_OBJECT_ID              SEC_ASN1_OBJECT_ID
#define nssASN1_OBJECT_DESCRIPTOR      SEC_ASN1_OBJECT_DESCRIPTOR
/* External type and instance-of type   0x08 */
#define nssASN1_REAL                   SEC_ASN1_REAL
#define nssASN1_ENUMERATED             SEC_ASN1_ENUMERATED
#define nssASN1_EMBEDDED_PDV           SEC_ASN1_EMBEDDED_PDV
#define nssASN1_UTF8_STRING            SEC_ASN1_UTF8_STRING
#define nssASN1_SEQUENCE               SEC_ASN1_SEQUENCE
#define nssASN1_SET                    SEC_ASN1_SET
#define nssASN1_NUMERIC_STRING         SEC_ASN1_NUMERIC_STRING
#define nssASN1_PRINTABLE_STRING       SEC_ASN1_PRINTABLE_STRING
#define nssASN1_T61_STRING             SEC_ASN1_T61_STRING
#define nssASN1_TELETEX_STRING         nssASN1_T61_STRING
#define nssASN1_VIDEOTEX_STRING        SEC_ASN1_VIDEOTEX_STRING
#define nssASN1_IA5_STRING             SEC_ASN1_IA5_STRING
#define nssASN1_UTC_TIME               SEC_ASN1_UTC_TIME
#define nssASN1_GENERALIZED_TIME       SEC_ASN1_GENERALIZED_TIME
#define nssASN1_GRAPHIC_STRING         SEC_ASN1_GRAPHIC_STRING
#define nssASN1_VISIBLE_STRING         SEC_ASN1_VISIBLE_STRING
#define nssASN1_GENERAL_STRING         SEC_ASN1_GENERAL_STRING
#define nssASN1_UNIVERSAL_STRING       SEC_ASN1_UNIVERSAL_STRING
/*                                      0x1d */
#define nssASN1_BMP_STRING             SEC_ASN1_BMP_STRING
#define nssASN1_HIGH_TAG_NUMBER        SEC_ASN1_HIGH_TAG_NUMBER

#define nssASN1_METHOD_MASK            SEC_ASN1_METHOD_MASK
#define nssASN1_PRIMITIVE              SEC_ASN1_PRIMITIVE
#define nssASN1_CONSTRUCTED            SEC_ASN1_CONSTRUCTED
                                                                
#define nssASN1_CLASS_MASK             SEC_ASN1_CLASS_MASK
#define nssASN1_UNIVERSAL              SEC_ASN1_UNIVERSAL
#define nssASN1_APPLICATION            SEC_ASN1_APPLICATION
#define nssASN1_CONTEXT_SPECIFIC       SEC_ASN1_CONTEXT_SPECIFIC
#define nssASN1_PRIVATE                SEC_ASN1_PRIVATE

#define nssASN1_OPTIONAL               SEC_ASN1_OPTIONAL 
#define nssASN1_EXPLICIT               SEC_ASN1_EXPLICIT 
#define nssASN1_ANY                    SEC_ASN1_ANY      
#define nssASN1_INLINE                 SEC_ASN1_INLINE   
#define nssASN1_POINTER                SEC_ASN1_POINTER  
#define nssASN1_GROUP                  SEC_ASN1_GROUP    
#define nssASN1_DYNAMIC                SEC_ASN1_DYNAMIC  
#define nssASN1_SKIP                   SEC_ASN1_SKIP     
#define nssASN1_INNER                  SEC_ASN1_INNER    
#define nssASN1_SAVE                   SEC_ASN1_SAVE     
#define nssASN1_MAY_STREAM             SEC_ASN1_MAY_STREAM
#define nssASN1_SKIP_REST              SEC_ASN1_SKIP_REST
#define nssASN1_CHOICE                 SEC_ASN1_CHOICE

#define nssASN1_SEQUENCE_OF            SEC_ASN1_SEQUENCE_OF 
#define nssASN1_SET_OF                 SEC_ASN1_SET_OF      
#define nssASN1_ANY_CONTENTS           SEC_ASN1_ANY_CONTENTS

typedef SEC_ASN1TemplateChooserPtr nssASN1ChooseTemplateFunction;

typedef SEC_ASN1DecoderContext nssASN1Decoder;
typedef SEC_ASN1EncoderContext nssASN1Encoder;

typedef enum {
  nssASN1EncodingPartIdentifier    = SEC_ASN1_Identifier,
  nssASN1EncodingPartLength        = SEC_ASN1_Length,
  nssASN1EncodingPartContents      = SEC_ASN1_Contents,
  nssASN1EncodingPartEndOfContents = SEC_ASN1_EndOfContents
} nssASN1EncodingPart;

typedef SEC_ASN1NotifyProc nssASN1NotifyFunction;

typedef SEC_ASN1WriteProc nssASN1EncoderWriteFunction;
typedef SEC_ASN1WriteProc nssASN1DecoderFilterFunction;

PR_END_EXTERN_C

#endif /* ASN1T_H */
