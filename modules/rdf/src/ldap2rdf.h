/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef	_RDF_LDAP2RDF_H_
#define	_RDF_LDAP2RDF_H_


#include "rdf-int.h"

#ifdef MOZ_LDAP
#include "ldap.h"
#include "xpgetstr.h"
#include "htmldlgs.h"
#endif



/* ldap2rdf.c data structures and defines */

#define	ADMIN_ID	"uid=rjc, ou=People, o=airius.com"
#define	ADMIN_PW	"netscape"



/* ldap2rdf.c function prototypes */

XP_BEGIN_PROTOS

RDFT		MakeLdapStore (char* url);
RDF_Error	LdapInit (RDFT ntr);
void		ldap2rdfInit (RDFT rdf);
Assertion	ldaparg1 (RDF_Resource u);
Assertion	setldaparg1 (RDF_Resource u, Assertion as);
Assertion	ldaparg2 (RDF_Resource u);
Assertion	setldaparg2 (RDF_Resource u, Assertion as);
PRBool		ldapAssert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
PRBool		ldapUnassert (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		ldapDBAdd (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		ldapDBRemove (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type);
PRBool		ldapHasAssertion (RDFT rdf, RDF_Resource u, RDF_Resource s, void* v, RDF_ValueType type, PRBool tv);
void *		ldapGetSlotValue (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type, PRBool inversep,  PRBool tv);
RDF_Cursor	ldapGetSlotValues (RDFT rdf, RDF_Resource u, RDF_Resource s, RDF_ValueType type,  PRBool inversep, PRBool tv);
void *		ldapNextValue (RDFT mcf, RDF_Cursor c);
RDF_Error	ldapDisposeCursor (RDFT mcf, RDF_Cursor c);
RDF_Resource	ldapNewContainer(char *id);
int		ldapModifyEntry (RDFT rdf, RDF_Resource parent, RDF_Resource child, PRBool addFlag);
PRBool		ldapAddChild (RDFT rdf, RDF_Resource parent, RDF_Resource child);
PRBool		ldapRemoveChild (RDFT rdf, RDF_Resource parent, RDF_Resource child);
void		possiblyAccessldap(RDFT rdf, RDF_Resource u, RDF_Resource s, PRBool inversep);
PRBool		ldapContainerp (RDF_Resource u);

XP_END_PROTOS

#endif
