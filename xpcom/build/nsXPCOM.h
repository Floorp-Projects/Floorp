/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsXPCOM_h__
#define nsXPCOM_h__

#include "nscore.h"
#include "nsXPCOMCID.h"

class nsAString;
class nsACString;

class nsIComponentManager;
class nsIComponentRegistrar;
class nsIServiceManager;
class nsIFile;
class nsILocalFile;
class nsIDirectoryServiceProvider;
class nsIMemory;
class nsIDebug;
class nsITraceRefcnt;

/**
 * Initialises XPCOM. You must call this method before proceeding 
 * to use xpcom. The one exception is that you may call 
 * NS_NewLocalFile to create a nsIFile.
 * 
 * @status FROZEN
 *
 * @note Use <CODE>NS_NewLocalFile</CODE> or <CODE>NS_NewNativeLocalFile</CODE> 
 *       to create the file object you supply as the bin directory path in this
 *       call. The function may be safely called before the rest of XPCOM or 
 *       embedding has been initialised.
 *
 * @param result           The service manager.  You may pass null.
 *
 * @param abinDirectory    The directory containing the component
 *                         registry and runtime libraries;
 *                         or use <CODE>nsnull</CODE> to use the working
 *                         directory.
 *
 * @param aAppFileLocProvider The object to be used by Gecko that specifies
 *                         to Gecko where to find profiles, the component
 *                         registry preferences and so on; or use
 *                         <CODE>nsnull</CODE> for the default behaviour.
 *
 * @see NS_NewLocalFile
 * @see nsILocalFile
 * @see nsIDirectoryServiceProvider
 *
 * @return NS_OK for success;
 *         NS_ERROR_NOT_INITIALIZED if static globals were not initialied, which
 *         can happen if XPCOM is reloaded, but did not completly shutdown.  
 *         other error codes indicate a failure during initialisation.
 *
 */
extern "C" NS_COM nsresult
NS_InitXPCOM2(nsIServiceManager* *result, 
              nsIFile* binDirectory,
              nsIDirectoryServiceProvider* appFileLocationProvider);
/**
 * Shutdown XPCOM. You must call this method after you are finished
 * using xpcom. 
 *
 * @status FROZEN
 *
 * @param servMgr           The service manager which was returned by NS_InitXPCOM2.  
 *                          This will release servMgr.  You may pass null.
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 *
 */
extern "C" NS_COM nsresult
NS_ShutdownXPCOM(nsIServiceManager* servMgr);


/**
 * Public Method to access to the service manager.
 * 
 * @status FROZEN
 * @param result Interface pointer to the service manager 
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 *
 */
extern "C" NS_COM nsresult
NS_GetServiceManager(nsIServiceManager* *result);

/**
 * Public Method to access to the component manager.
 * 
 * @status FROZEN
 * @param result Interface pointer to the service 
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 *
 */
extern "C" NS_COM nsresult
NS_GetComponentManager(nsIComponentManager* *result);

/**
 * Public Method to access to the component registration manager.
 * 
 * @status FROZEN
 * @param result Interface pointer to the service 
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 *
 */
extern "C" NS_COM nsresult
NS_GetComponentRegistrar(nsIComponentRegistrar* *result);

/**
 * Public Method to access to the memory manager.  See nsIMemory
 * 
 * @status FROZEN
 * @param result Interface pointer to the memory manager 
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 *
 */
extern "C" NS_COM nsresult
NS_GetMemoryManager(nsIMemory* *result);

/**
 * Public Method to create an instance of a nsILocalFile.  This function
 * may be called prior to NS_InitXPCOM2.  
 * 
 * @status FROZEN
 * 
 *   @param filePath       
 *       A string which specifies a full file path to a 
 *       location.  Relative paths will be treated as an
 *       error (NS_ERROR_FILE_UNRECOGNIZED_PATH).       
 *       |NS_NewNativeLocalFile|'s path must be in the 
 *       filesystem charset.
 *   @param followLinks
 *       This attribute will determine if the nsLocalFile will auto
 *       resolve symbolic links.  By default, this value will be false
 *       on all non unix systems.  On unix, this attribute is effectively
 *       a noop.  
 * @param result Interface pointer to a new instance of an nsILocalFile 
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure.
 */

extern "C" NS_COM nsresult
NS_NewLocalFile(const nsAString &path, 
                PRBool followLinks, 
                nsILocalFile* *result);

extern "C" NS_COM nsresult
NS_NewNativeLocalFile(const nsACString &path, 
                      PRBool followLinks, 
                      nsILocalFile* *result);


extern "C" NS_COM nsresult
NS_GetDebug(nsIDebug* *result);

extern "C" NS_COM nsresult
NS_GetTraceRefcnt(nsITraceRefcnt* *result);

#endif
