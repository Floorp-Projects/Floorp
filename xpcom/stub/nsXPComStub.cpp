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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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
#include "nsStringAPI.h"

/*
 * Stubs for nsXPCOM.h
 */

#undef NS_InitXPCOM2
extern "C" NS_EXPORT nsresult
NS_InitXPCOM2(nsIServiceManager **result,
              nsIFile *binDirectory,
              nsIDirectoryServiceProvider *dirProvider)
{
  return NS_InitXPCOM2_P(result, binDirectory, dirProvider);
}

#undef NS_ShutdownXPCOM
extern "C" NS_EXPORT nsresult
NS_ShutdownXPCOM(nsIServiceManager *svcMgr)
{
  return NS_ShutdownXPCOM_P(svcMgr);
}

#undef NS_GetServiceManager
extern "C" NS_EXPORT nsresult
NS_GetServiceManager(nsIServiceManager* *result)
{
  return NS_GetServiceManager_P(result);
}

#undef NS_GetComponentManager
extern "C" NS_EXPORT nsresult
NS_GetComponentManager(nsIComponentManager* *result)
{
  return NS_GetComponentManager_P(result);
}

#undef NS_GetComponentRegistrar
extern "C" NS_EXPORT nsresult
NS_GetComponentRegistrar(nsIComponentRegistrar* *result)
{
  return NS_GetComponentRegistrar_P(result);
}

#undef NS_GetMemoryManager
extern "C" NS_EXPORT nsresult
NS_GetMemoryManager(nsIMemory* *result)
{
  return NS_GetMemoryManager_P(result);
}

#undef NS_NewLocalFile
extern "C" NS_EXPORT nsresult
NS_NewLocalFile(const nsAString &path,
                PRBool followLinks,
                nsILocalFile **result)
{
  return NS_NewLocalFile_P(path, followLinks, result);
}

#undef NS_NewNativeLocalFile
extern "C" NS_EXPORT nsresult
NS_NewNativeLocalFile(const nsACString &path,
                      PRBool followLinks,
                      nsILocalFile **result)
{
  return NS_NewNativeLocalFile_P(path, followLinks, result);
}

#undef NS_GetDebug
extern "C" NS_EXPORT nsresult
NS_GetDebug(nsIDebug **result)
{
  return NS_GetDebug_P(result);
}

#undef NS_GetTraceRefcnt
extern "C" NS_EXPORT nsresult
NS_GetTraceRefcnt(nsITraceRefcnt **result)
{
  return NS_GetTraceRefcnt_P(result);
}

/*
 * Stubs for nsXPCOMPrivate.h
 */

#undef NS_RegisterXPCOMExitRoutine
extern "C" NS_EXPORT nsresult
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine, PRUint32 priority)
{
  return NS_RegisterXPCOMExitRoutine_P(exitRoutine, priority);
}

#undef NS_UnregisterXPCOMExitRoutine
extern "C" NS_EXPORT nsresult
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine)
{
  return NS_UnregisterXPCOMExitRoutine_P(exitRoutine);
}

#undef NS_GetFrozenFunctions
extern "C" NS_EXPORT nsresult
NS_GetFrozenFunctions(XPCOMFunctions *entryPoints, const char* libraryPath)
{
  return NS_GetFrozenFunctions_P(entryPoints, libraryPath);
}
