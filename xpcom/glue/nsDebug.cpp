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
 * The Original Code is XPCOM
 *
 * The Initial Developer of the Original Code is Doug Turner <dougt@meer.net>
 *
 * Portions created by the Initial Developer are Copyright (C) 2003
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

#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsDebug.h"
#include "nsDebugImpl.h"

static nsIDebug* gDebugObject = nsnull;

static NS_METHOD FreeDebugObject(void)
{
    NS_IF_RELEASE(gDebugObject);
    return NS_OK;
}

#define ENSURE_DEBUGOBJECT \
  (gDebugObject ? PR_TRUE : (PRBool)(SetupDebugObject() != nsnull))

static nsIDebug* SetupDebugObject()
{
  NS_GetDebug(&gDebugObject);
  if (gDebugObject)
    NS_RegisterXPCOMExitRoutine(FreeDebugObject, 0);
  return gDebugObject;
}

#ifdef XPCOM_GLUE
nsresult GlueStartupDebug() 
{
  NS_GetDebug(&gDebugObject);
  if (!gDebugObject) 
    return NS_ERROR_FAILURE;
  return NS_OK;
}

void GlueShutdownDebug()
{
  NS_IF_RELEASE(gDebugObject);
}
#endif

NS_COM_GLUE void nsDebug::Abort(const char* aFile, PRIntn aLine)
{     
  if (!ENSURE_DEBUGOBJECT)
    return;    
  
  gDebugObject->Abort(aFile, aLine);
}     

NS_COM_GLUE void nsDebug::Break(const char* aFile, PRIntn aLine)
{ 
  if (!ENSURE_DEBUGOBJECT)
    return;    
  
  gDebugObject->Break(aFile, aLine);
}

NS_COM_GLUE void nsDebug::Warning(const char* aStr,
                             const char* aFile, 
                             PRIntn aLine)
{ 
  if (!ENSURE_DEBUGOBJECT)
    return;    
  gDebugObject->Warning(aStr, aFile, aLine);
}

NS_COM_GLUE void nsDebug::Assertion(const char* aStr, const char* aExpr,
                               const char* aFile, PRIntn aLine)
{ 
  if (!ENSURE_DEBUGOBJECT)
    return;    
  gDebugObject->Assertion(aStr, aExpr, aFile, aLine);
}
