/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

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
