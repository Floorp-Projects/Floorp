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

#include "jsec2rdf.h"
#include "fs2rdf.h"
#include "utils.h"
#include "nlcstore.h"
#include "vocabint.h"

NSPR_BEGIN_EXTERN_C

extern RDF gNCDB;
RDF gJSecDB = NULL;

JSec_Error
RDFJSec_InitPrivilegeDB()
{
  if (gJSecDB == 0) {
    gJSecDB = gNCDB;
  }
  return JSec_OK;
}

JSec_Error
RDFJSec_SavePrivilegeDB()
{
  return JSec_OK;
}

JSec_Error
RDFJSec_ClosePrivilegeDB()
{
  return JSec_OK;
}

#define JSEC_PRINCIPAL_URL "jsecprin:"

char *
RDFJSec_GetPrincipalURLString(char *principalID)
{
  size_t size = strlen(principalID);
  char* url = getMem(size+strlen(JSEC_PRINCIPAL_URL)+1);
  if (url == NULL) {
    return NULL;
  }
  sprintf(url, "%s%s", JSEC_PRINCIPAL_URL, principalID);
  strcpy(url, principalID);
  return url;
}

RDF_Cursor
RDFJSec_ListAllPrincipals()
{
  RDF_Cursor c = NULL;
  if (gNavCenter != NULL) {
    c = RDF_GetSources(gJSecDB, gNavCenter->RDF_JSec, 
                       gNavCenter->RDF_JSecPrincipal, 
                       RDF_RESOURCE_TYPE, TRUE);
  }
  return(c);
}

JSec_Principal
RDFJSec_NextPrincipal(RDF_Cursor c)
{
  if (c != NULL) {
    return RDF_NextValue(c);
  }
  return NULL;
}

RDF_Error
RDFJSec_ReleaseCursor(RDF_Cursor c)
{
  RDF_Error	err = 0;
  if (c != NULL) {
    err = RDF_DisposeCursor(c);
  }
  return (err);
}

JSec_Principal
RDFJSec_NewPrincipal(char* principalID)
{
  RDF_Resource principalUnit;
  char *url = RDFJSec_GetPrincipalURLString(principalID);
  if (url == NULL) {
    return NULL;
  }
  principalUnit = RDF_GetResource(NULL, url, FALSE);
  if (!principalUnit) {
    principalUnit = RDF_GetResource(NULL, url, TRUE);
  } 
  freeMem(url);
  return (JSec_Principal)principalUnit;
}

JSec_Error
RDFJSec_AddPrincipal(JSec_Principal pr)
{
  if ((pr == NULL) || (gNavCenter == NULL)) {
    return JSec_NullObject;
  }
  if (RDF_HasAssertion(gJSecDB, pr, gNavCenter->RDF_JSecPrincipal, gNavCenter->RDF_JSec, 
                       RDF_RESOURCE_TYPE, PR_FALSE)) {
    return JSec_OK;
  }

  setContainerp(pr, PR_TRUE);
  setResourceType(pr, JSEC_RT);
  RDF_Assert(gJSecDB, pr, gNavCenter->RDF_JSecPrincipal, gNavCenter->RDF_JSec, RDF_RESOURCE_TYPE);  
  return JSec_OK;
}

JSec_Error
RDFJSec_DeletePrincipal(JSec_Principal pr)
{
  RDF_Cursor c;
  if ((pr == NULL) || (gNavCenter == NULL)) {
    return JSec_NullObject;
  }
  /* Before deleting the principal, delete all the PrincipalUses for this principal. 
   */
  c = RDFJSec_ListAllPrincipalUses(pr);
  if (c != NULL) {
    JSec_PrincipalUse prUse;
    while ((prUse = RDFJSec_NextPrincipalUse(c)) != NULL) {
      RDFJSec_DeletePrincipalUse(pr, prUse);
    }
  }
  RDF_Unassert(gJSecDB, pr, gNavCenter->RDF_JSecPrincipal, gNavCenter->RDF_JSec, RDF_RESOURCE_TYPE);  
  return JSec_OK;
}

char *
RDFJSec_PrincipalID(JSec_Principal pr)
{
  char *url = resourceID(pr);
  char *ans = getMem(strlen(url)+1);
  int n;
  if (ans == NULL) {
    return NULL;
  }
  n = charSearch(':', url);
  if (n == -1) {
    strcpy(ans, url);
  } else {
    strcpy(ans, url+n+1);
  }
  return ans;
}

void *
RDFJSec_AttributeOfPrincipal(JSec_Principal pr, char* attributeType)
{
  RDF_Resource attributeResource = RDF_GetResource(NULL, attributeType, TRUE);
  void *attValue;
  RDF_Cursor c = NULL;
  c = RDF_GetTargets(gJSecDB, pr, attributeResource, RDF_STRING_TYPE,  TRUE);
  if (c == NULL) {
    return NULL;
  }
  attValue = RDF_NextValue(c);
  RDF_DisposeCursor(c);
  return attValue;
}

JSec_Error 
RDFJSec_SetPrincipalAttribute(JSec_Principal pr, char* attributeType, void* attValue)
{
  RDF_Resource attributeResource = RDF_GetResource(NULL, attributeType, TRUE);
  RDF_Assert(gJSecDB, pr, attributeResource, attValue, RDF_STRING_TYPE);  
  return JSec_OK;
}


RDF_Cursor
RDFJSec_ListAllPrincipalUses(JSec_Principal pr)
{
  RDF_Cursor	c = NULL;
  c = RDF_GetSources(gJSecDB, (RDF_Resource)pr, gCoreVocab->RDF_parent, RDF_RESOURCE_TYPE,  true);
  return(c);
}

JSec_PrincipalUse
RDFJSec_NextPrincipalUse(RDF_Cursor c)
{
  if (c != NULL) {
    return RDF_NextValue(c);
  }
  return NULL;
}

JSec_PrincipalUse
RDFJSec_NewPrincipalUse(JSec_Principal pr, JSec_Target tr, char* priv)
{
  RDF_Resource principalUseUnit;
  char *targetID = resourceID(tr);
  char *principalID = resourceID(pr);
  char *principalUseID = getMem(strlen(principalID)  + strlen(targetID) + 2);
  if (principalUseID == NULL) {
    return NULL;
  }
  sprintf(principalUseID, "%s!%s", principalID, targetID);
  principalUseUnit = RDF_GetResource(NULL, principalUseID, FALSE);
  if (!principalUseUnit) {
    principalUseUnit = RDF_GetResource(NULL, principalUseID, TRUE);
    RDFJSec_AddTargetToPrincipalUse(principalUseUnit, tr);
    RDFJSec_AddPrincipalUsePrivilege(principalUseUnit, priv);
  } 
  return principalUseUnit;
}

JSec_Error 
RDFJSec_AddPrincipalUse(JSec_Principal pr, JSec_PrincipalUse prUse)
{
  if ((pr == NULL) || (prUse == NULL)) {
    return JSec_NullObject;
  }
  setContainerp(prUse, PR_TRUE);
  setResourceType(prUse, JSEC_RT);
  RDF_Assert(gJSecDB, prUse, gCoreVocab->RDF_parent, pr, RDF_RESOURCE_TYPE);  
  return JSec_OK;
}

JSec_Error 
RDFJSec_DeletePrincipalUse (JSec_Principal pr, JSec_PrincipalUse prUse)
{
  JSec_Target tr;
  char *priv;
  if ((pr == NULL) || (prUse == NULL)) {
    return JSec_NullObject;
  }
  /* Before deleting the principal, delete all the PrincipalUses for this principal. 
   */
  tr = RDFJSec_TargetOfPrincipalUse(prUse);
  RDFJSec_DeleteTargetToPrincipalUse(prUse, tr);
  priv = RDFJSec_PrivilegeOfPrincipalUse(prUse);
  RDFJSec_DeletePrincipalUsePrivilege(prUse, priv);
  RDF_Unassert(gJSecDB, prUse, gCoreVocab->RDF_parent, pr, RDF_RESOURCE_TYPE);  
  return JSec_OK;
}

const char *
RDFJSec_PrincipalUseID(JSec_PrincipalUse prUse)
{
  return resourceID(prUse);
}

JSec_Error 
RDFJSec_AddPrincipalUsePrivilege (JSec_PrincipalUse prUse, char* priv)
{
  char *oldPriv;
  if ((prUse == NULL) || (priv == NULL) || (gNavCenter == NULL)) {
    return JSec_NullObject;
  }
  /* Each PrincipalUse can only have one Privilege. Thus delete the old privilege*/
  oldPriv = RDFJSec_PrivilegeOfPrincipalUse(prUse);
  RDFJSec_DeletePrincipalUsePrivilege(prUse, oldPriv);
  RDF_Assert(gJSecDB, prUse, gNavCenter->RDF_JSecAccess, priv, RDF_STRING_TYPE);  
  return JSec_OK;
}

JSec_Error 
RDFJSec_DeletePrincipalUsePrivilege (JSec_PrincipalUse prUse, char* priv)
{
  if ((prUse == NULL) || (priv == NULL) || (gNavCenter == NULL)) {
    return JSec_NullObject;
  }
  RDF_Unassert(gJSecDB, prUse, gNavCenter->RDF_JSecAccess, priv, RDF_STRING_TYPE);  
  return JSec_OK;
}

char *
RDFJSec_PrivilegeOfPrincipalUse (JSec_PrincipalUse prUse)
{
  RDF_Cursor c = NULL;
  char *privilege;
  if (gNavCenter == NULL) {
    return NULL;
  }
  c = RDF_GetTargets(gJSecDB, (RDF_Resource)prUse, gNavCenter->RDF_JSecAccess, RDF_STRING_TYPE,  TRUE);
  if (c == NULL) {
    return NULL;
  }
  privilege = RDF_NextValue(c);
  RDF_DisposeCursor(c);
  return privilege;
}

JSec_Error 
RDFJSec_AddTargetToPrincipalUse(JSec_PrincipalUse prUse, JSec_Target tr)
{
  JSec_Target oldTarget;
  if ((prUse == NULL) || (tr == NULL) || (gNavCenter == NULL)) {
    return JSec_NullObject;
  }
  /* Each PrincipalUse can only have one Target. Thus delete the old target */
  oldTarget = RDFJSec_TargetOfPrincipalUse(prUse);
  RDFJSec_DeleteTargetToPrincipalUse(prUse, oldTarget);
  RDF_Assert(gJSecDB, prUse, gNavCenter->RDF_JSecTarget, tr, RDF_RESOURCE_TYPE);  
  return JSec_OK;
}

JSec_Error 
RDFJSec_DeleteTargetToPrincipalUse(JSec_PrincipalUse prUse, JSec_Target tr)
{
  if ((prUse == NULL) || (tr == NULL) || (gNavCenter == NULL)) {
    return JSec_NullObject;
  }
  RDF_Unassert(gJSecDB, prUse, gNavCenter->RDF_JSecTarget, tr, RDF_RESOURCE_TYPE);  
  return JSec_OK;
}

JSec_Target
RDFJSec_TargetOfPrincipalUse (JSec_PrincipalUse prUse)
{
  RDF_Cursor	c = NULL;
  JSec_Target tr;
  if ((prUse == NULL) || (gNavCenter == NULL)) {
    return NULL;
  }
  c = RDF_GetTargets(gJSecDB, (RDF_Resource)prUse, gNavCenter->RDF_JSecTarget, RDF_RESOURCE_TYPE,  true);
  if (c == NULL) {
    return NULL;
  }
  tr = RDF_NextValue(c);
  RDF_DisposeCursor(c);
  return tr;
}

JSec_Target
RDFJSec_NewTarget(char* targetName, JSec_Principal pr)
{
  RDF_Resource tr;
  /* RDF_Resource prResource; */
  char *principalID = RDFJSec_PrincipalID(pr);
  char *targetID = getMem(strlen(targetName) + strlen(principalID) + 2);
  if (targetID == NULL) {
    return NULL;
  }

  if (gNavCenter == NULL) {
    return NULL;
  }

  sprintf(targetID, "%s!%s", targetName, principalID);
  tr = RDF_GetResource(NULL, targetID, FALSE);
  if (!tr) {
    tr = RDF_GetResource(NULL, targetID, TRUE);
    if (tr == NULL) {
      return NULL;
    }
    RDFJSec_SetTargetAttribute(tr, "targetName", targetName);
    RDF_Assert(gJSecDB, tr, gNavCenter->RDF_JSecPrincipal, pr, RDF_RESOURCE_TYPE);  
  } 
  return tr;
}

char *
RDFJSec_GetTargetName(JSec_Target tr)
{
  return RDFJSec_AttributeOfTarget(tr, "targetName");
}

char *
RDFJSec_AttributeOfTarget(JSec_Target tr, char* attributeType)
{
  RDF_Resource attributeResource = RDF_GetResource(NULL, attributeType, TRUE);
  char *attValue;
  RDF_Cursor c = NULL;
  c = RDF_GetTargets(gJSecDB, tr, attributeResource, RDF_STRING_TYPE,  TRUE);
  if (c == NULL) {
    return NULL;
  }
  attValue = RDF_NextValue(c);
  RDF_DisposeCursor(c);
  return attValue;
}

JSec_Error 
RDFJSec_SetTargetAttribute(JSec_Target tr, char* attributeType, char* attValue)
{
  RDF_Resource attributeResource = RDF_GetResource(NULL, attributeType, TRUE);
  RDF_Assert(gJSecDB, tr, attributeResource, attValue, RDF_STRING_TYPE);  
  return JSec_OK;
}

NSPR_END_EXTERN_C
