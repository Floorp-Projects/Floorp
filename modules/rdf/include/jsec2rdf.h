/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef _JSEC_2_RDF_H_
#define _JSEC_2_RDF_H_

#include "rdf.h"

typedef int16 JSec_Error;

#define JSec_OK ((JSec_Error)0x0000)
#define JSec_NullObject ((JSec_Error)0x0001)

typedef RDF_Resource JSec_Principal;
typedef RDF_Resource JSec_PrincipalUse;
typedef RDF_Resource JSec_Target;

NSPR_BEGIN_EXTERN_C

PR_EXTERN(char*)              RDFJSec_GetPrincipalURLString(char *principalID);
PR_EXTERN(const char*)        RDFJSec_PrincipalUseID(JSec_PrincipalUse prUse);

PR_EXTERN(JSec_Error)         RDFJSec_InitPrivilegeDB(); /* just use gNCDB */
PR_EXTERN(JSec_Error)         RDFJSec_SavePrivilegeDB(); 
PR_EXTERN(JSec_Error)         RDFJSec_ClosePrivilegeDB();

PR_EXTERN(RDF_Cursor)         RDFJSec_ListAllPrincipals();
PR_EXTERN(JSec_Principal)     RDFJSec_NextPrincipal(RDF_Cursor c);
PR_EXTERN(JSec_Error)         RDFJSec_ReleaseCursor(RDF_Cursor c);

PR_EXTERN(JSec_Principal)     RDFJSec_NewPrincipal(char* principalID);
PR_EXTERN(JSec_Error)         RDFJSec_AddPrincipal(JSec_Principal pr);
PR_EXTERN(JSec_Error)         RDFJSec_DeletePrincipal(JSec_Principal pr);
PR_EXTERN(char*)              RDFJSec_PrincipalID(JSec_Principal pr);
PR_EXTERN(void*)              RDFJSec_AttributeOfPrincipal(JSec_Principal pr, char* attributeType);
PR_EXTERN(JSec_Error)         RDFJSec_SetPrincipalAttribute(JSec_Principal pr, char* attributeType, void* attValue);

PR_EXTERN(RDF_Cursor)         RDFJSec_ListAllPrincipalUses(JSec_Principal pr);
PR_EXTERN(JSec_PrincipalUse)  RDFJSec_NextPrincipalUse(RDF_Cursor c);

PR_EXTERN(JSec_PrincipalUse)  RDFJSec_NewPrincipalUse(JSec_Principal pr, JSec_Target tr, char* priv);
PR_EXTERN(JSec_Error)         RDFJSec_AddPrincipalUse(JSec_Principal pr, JSec_PrincipalUse prUse);
PR_EXTERN(JSec_Error)         RDFJSec_DeletePrincipalUse (JSec_Principal pr, JSec_PrincipalUse prUse);

PR_EXTERN(JSec_Error)         RDFJSec_AddPrincipalUsePrivilege (JSec_PrincipalUse prUse, char* priv);
PR_EXTERN(JSec_Error)         RDFJSec_DeletePrincipalUsePrivilege (JSec_PrincipalUse prUse, char* priv);
PR_EXTERN(char*)              RDFJSec_PrivilegeOfPrincipalUse (JSec_PrincipalUse p);

PR_EXTERN(JSec_Error)         RDFJSec_AddTargetToPrincipalUse(JSec_PrincipalUse prUse, JSec_Target tr);
PR_EXTERN(JSec_Error)         RDFJSec_DeleteTargetToPrincipalUse(JSec_PrincipalUse prUse, JSec_Target tr);
PR_EXTERN(JSec_Target)        RDFJSec_TargetOfPrincipalUse (JSec_PrincipalUse p);

PR_EXTERN(JSec_Target)        RDFJSec_NewTarget(char* targetName, JSec_Principal pr);
PR_EXTERN(char*)              RDFJSec_GetTargetName(JSec_Target tr);
PR_EXTERN(char*)              RDFJSec_AttributeOfTarget(JSec_Target tr, char* attributeType);
PR_EXTERN(JSec_Error)         RDFJSec_SetTargetAttribute(JSec_Target tr, char* attributeType, char* attValue);

NSPR_END_EXTERN_C

#endif /* _JSEC_2_RDF_H_ */
