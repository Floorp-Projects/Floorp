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

#ifndef NSSPKI1T_H
#define NSSPKI1T_H

#ifdef DEBUG
static const char NSSPKI1T_CVS_ID[] = "@(#) $RCSfile: nsspki1t.h,v $ $Revision: 1.3 $ $Date: 2005/01/20 02:25:49 $";
#endif /* DEBUG */

/*
 * nsspki1t.h
 *
 * This file contains the public type definitions for the PKIX part-1
 * objects.
 */

#ifndef NSSBASET_H
#include "nssbaset.h"
#endif /* NSSBASET_H */

PR_BEGIN_EXTERN_C

/*
 * OBJECT IDENTIFIER
 *
 * This is the basic OID that crops up everywhere.
 */

struct NSSOIDStr;
typedef struct NSSOIDStr NSSOID;

/*
 * AttributeTypeAndValue
 *
 * This structure contains an attribute type (indicated by an OID), 
 * and the type-specific value.  RelativeDistinguishedNamess consist
 * of a set of these.  These are distinct from Attributes (which have
 * SET of values), from AttributeDescriptions (which have qualifiers
 * on the types), and from AttributeValueAssertions (which assert a
 * a value comparison under some matching rule).
 */

struct NSSATAVStr;
typedef struct NSSATAVStr NSSATAV;

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
 */

struct NSSRDNStr;
typedef struct NSSRDNStr NSSRDN;

/*
 * RDNSequence
 *
 * This structure contains a sequence of RelativeDistinguishedName
 * objects.
 */

struct NSSRDNSeqStr;
typedef struct NSSRDNSeqStr NSSRDNSeq;

/*
 * Name
 *
 * This structure contains a union of the possible name formats,
 * which at the moment is limited to an RDNSequence.
 */

struct NSSNameStr;
typedef struct NSSNameStr NSSName;

/*
 * NameChoice
 *
 * This enumeration is used to specify choice within a name.
 */

enum NSSNameChoiceEnum {
  NSSNameChoiceInvalid = -1,
  NSSNameChoiceRdnSequence
};
typedef enum NSSNameChoiceEnum NSSNameChoice;

/*
 * GeneralName
 *
 * This structure contains a union of the possible general names,
 * of which there are several.
 */

struct NSSGeneralNameStr;
typedef struct NSSGeneralNameStr NSSGeneralName;

/*
 * GeneralNameChoice
 *
 * This enumerates the possible general name types.
 */

enum NSSGeneralNameChoiceEnum {
  NSSGeneralNameChoiceInvalid = -1,
  NSSGeneralNameChoiceOtherName = 0,
  NSSGeneralNameChoiceRfc822Name = 1,
  NSSGeneralNameChoiceDNSName = 2,
  NSSGeneralNameChoiceX400Address = 3,
  NSSGeneralNameChoiceDirectoryName = 4,
  NSSGeneralNameChoiceEdiPartyName = 5,
  NSSGeneralNameChoiceUniformResourceIdentifier = 6,
  NSSGeneralNameChoiceIPAddress = 7,
  NSSGeneralNameChoiceRegisteredID = 8
};
typedef enum NSSGeneralNameChoiceEnum NSSGeneralNameChoice;

/*
 * The "other" types of general names.
 */

struct NSSOtherNameStr;
typedef struct NSSOtherNameStr NSSOtherName;

struct NSSRFC822NameStr;
typedef struct NSSRFC822NameStr NSSRFC822Name;

struct NSSDNSNameStr;
typedef struct NSSDNSNameStr NSSDNSName;

struct NSSX400AddressStr;
typedef struct NSSX400AddressStr NSSX400Address;

struct NSSEdiPartyNameStr;
typedef struct NSSEdiPartyNameStr NSSEdiPartyName;

struct NSSURIStr;
typedef struct NSSURIStr NSSURI;

struct NSSIPAddressStr;
typedef struct NSSIPAddressStr NSSIPAddress;

struct NSSRegisteredIDStr;
typedef struct NSSRegisteredIDStr NSSRegisteredID;

/*
 * GeneralNameSeq
 *
 * This structure contains a sequence of GeneralName objects.
 * Note that the PKIX documents refer to this as "GeneralNames,"
 * which differs from "GeneralName" by only one letter.  To
 * try to reduce confusion, we expand the name slightly to
 * "GeneralNameSeq."
 */

struct NSSGeneralNameSeqStr;
typedef struct NSSGeneralNameSeqStr NSSGeneralNameSeq;

PR_END_EXTERN_C

#endif /* NSSPKI1T_H */
