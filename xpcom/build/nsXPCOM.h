/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsXPCOM_h__
#define nsXPCOM_h__

// Map frozen functions to private symbol names if not using strict API.
#ifdef MOZILLA_INTERNAL_API
# define NS_InitXPCOM2               NS_InitXPCOM2_P
# define NS_InitXPCOM3               NS_InitXPCOM3_P
# define NS_ShutdownXPCOM            NS_ShutdownXPCOM_P
# define NS_GetServiceManager        NS_GetServiceManager_P
# define NS_GetComponentManager      NS_GetComponentManager_P
# define NS_GetComponentRegistrar    NS_GetComponentRegistrar_P
# define NS_GetMemoryManager         NS_GetMemoryManager_P
# define NS_NewLocalFile             NS_NewLocalFile_P
# define NS_NewNativeLocalFile       NS_NewNativeLocalFile_P
# define NS_GetDebug                 NS_GetDebug_P
# define NS_GetTraceRefcnt           NS_GetTraceRefcnt_P
# define NS_Alloc                    NS_Alloc_P
# define NS_Realloc                  NS_Realloc_P
# define NS_Free                     NS_Free_P
#endif

#include "nscore.h"
#include "nsXPCOMCID.h"

class nsAString;
class nsACString;

class nsIModule;
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
 * Every XPCOM component implements this function signature, which is the
 * only entrypoint XPCOM uses to the function.
 *
 * @status FROZEN
 */
typedef nsresult (PR_CALLBACK *nsGetModuleProc)(nsIComponentManager *aCompMgr,
                                                nsIFile* location,
                                                nsIModule** return_cobj);

/**
 * Initialises XPCOM. You must call one of the NS_InitXPCOM methods
 * before proceeding to use xpcom. The one exception is that you may
 * call NS_NewLocalFile to create a nsIFile.
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
 * @param binDirectory     The directory containing the component
 *                         registry and runtime libraries;
 *                         or use <CODE>nsnull</CODE> to use the working
 *                         directory.
 *
 * @param appFileLocationProvider The object to be used by Gecko that specifies
 *                         to Gecko where to find profiles, the component
 *                         registry preferences and so on; or use
 *                         <CODE>nsnull</CODE> for the default behaviour.
 *
 * @see NS_NewLocalFile
 * @see nsILocalFile
 * @see nsIDirectoryServiceProvider
 *
 * @return NS_OK for success;
 *         NS_ERROR_NOT_INITIALIZED if static globals were not initialized,
 *         which can happen if XPCOM is reloaded, but did not completly
 *         shutdown. Other error codes indicate a failure during
 *         initialisation.
 */
extern "C" NS_COM nsresult
NS_InitXPCOM2(nsIServiceManager* *result, 
              nsIFile* binDirectory,
              nsIDirectoryServiceProvider* appFileLocationProvider);

/**
 * Some clients of XPCOM have statically linked components (not dynamically
 * loaded component DLLs), which can be passed to NS_InitXPCOM3 using this
 * structure.
 *
 * @status FROZEN
 */
struct nsStaticModuleInfo {
  const char      *name;
  nsGetModuleProc  getModule;
};

/**
 * Initialises XPCOM with static components. You must call one of the
 * NS_InitXPCOM methods before proceeding to use xpcom. The one
 * exception is that you may call NS_NewLocalFile to create a nsIFile.
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
 * @param binDirectory     The directory containing the component
 *                         registry and runtime libraries;
 *                         or use <CODE>nsnull</CODE> to use the working
 *                         directory.
 *
 * @param appFileLocationProvider The object to be used by Gecko that specifies
 *                         to Gecko where to find profiles, the component
 *                         registry preferences and so on; or use
 *                         <CODE>nsnull</CODE> for the default behaviour.
 *
 * @param staticComponents An array of static components. Passing null is
 *                         Equivalent to NS_InitXPCOM2. These static components
 *                         will be registered before any other components.
 * @param componentCount   Number of elements in staticComponents
 *
 * @see NS_NewLocalFile
 * @see nsILocalFile
 * @see nsIDirectoryServiceProvider
 *
 * @return NS_OK for success;
 *         NS_ERROR_NOT_INITIALIZED if static globals were not initialized,
 *         which can happen if XPCOM is reloaded, but did not completly
 *         shutdown. Other error codes indicate a failure during
 *         initialisation.
 */
extern "C" NS_COM nsresult
NS_InitXPCOM3(nsIServiceManager* *result, 
              nsIFile* binDirectory,
              nsIDirectoryServiceProvider* appFileLocationProvider,
              nsStaticModuleInfo const *staticComponents,
              PRUint32 componentCount);

/**
 * Shutdown XPCOM. You must call this method after you are finished
 * using xpcom. 
 *
 * @status FROZEN
 *
 * @param servMgr           The service manager which was returned by NS_InitXPCOM.
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
 * may be called prior to NS_InitXPCOM.
 * 
 * @status FROZEN
 * 
 *   @param path       
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

/**
 * Allocates a block of memory of a particular size. If the memory cannot
 * be allocated (because of an out-of-memory condition), null is returned.
 *
 * @status FROZEN
 *
 * @param size   The size of the block to allocate
 * @result       The block of memory
 * @note         This function is thread-safe.
 */
extern "C" NS_COM void*
NS_Alloc(PRSize size);

/**
 * Reallocates a block of memory to a new size.
 *
 * @status FROZEN
 *
 * @param ptr     The block of memory to reallocate. This block must originally
                  have been allocated by NS_Alloc or NS_Realloc
 * @param size    The new size. If 0, frees the block like NS_Free
 * @result        The reallocated block of memory
 * @note          This function is thread-safe.
 *
 * If ptr is null, this function behaves like NS_Alloc.
 * If s is the size of the block to which ptr points, the first min(s, size)
 * bytes of ptr's block are copied to the new block. If the allocation
 * succeeds, ptr is freed and a pointer to the new block is returned. If the
 * allocation fails, ptr is not freed and null is returned. The returned
 * value may be the same as ptr.
 */
extern "C" NS_COM void*
NS_Realloc(void* ptr, PRSize size);

/**
 * Frees a block of memory. Null is a permissible value, in which case no
 * action is taken.
 *
 * @status FROZEN
 *
 * @param ptr   The block of memory to free. This block must originally have
 *              been allocated by NS_Alloc or NS_Realloc
 * @note        This function is thread-safe.
 */
extern "C" NS_COM void
NS_Free(void* ptr);


/**
 * Categories (in the category manager service) used by XPCOM:
 */

/**
 * A category which is read after component registration but before
 * the "xpcom-startup" notifications. Each category entry is treated
 * as the contract ID of a service which implements
 * nsIDirectoryServiceProvider. Each directory service provider is
 * installed in the global directory service.
 *
 * @status FROZEN
 */
#define XPCOM_DIRECTORY_PROVIDER_CATEGORY "xpcom-directory-providers"

/**
 * A category which is read after component registration but before
 * NS_InitXPCOM returns. Each category entry is treated as the contractID of
 * a service: each service is instantiated, and if it implements nsIObserver
 * the nsIObserver.observe method is called with the "xpcom-startup" topic.
 *
 * @status FROZEN
 */
#define NS_XPCOM_STARTUP_CATEGORY "xpcom-startup"


/**
 * Observer topics (in the observer service) used by XPCOM:
 */

/**
 * At XPCOM startup after component registration is complete, the
 * following topic is notified. In order to receive this notification,
 * component must register their contract ID in the category manager,
 *
 * @see NS_XPCOM_STARTUP_CATEGORY
 * @status FROZEN
 */
#define NS_XPCOM_STARTUP_OBSERVER_ID "xpcom-startup"

/**
 * At XPCOM shutdown, this topic is notified. All components must
 * release any interface references to objects in other modules when
 * this topic is notified.
 *
 * @status FROZEN
 */
#define NS_XPCOM_SHUTDOWN_OBSERVER_ID "xpcom-shutdown"

/**
 * This topic is notified when an entry was added to a category in the
 * category manager. The subject of the notification will be the name of
 * the added entry as an nsISupportsCString, and the data will be the
 * name of the category. The notification will occur on the main thread.
 *
 * @status FROZEN
 */
#define NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID \
  "xpcom-category-entry-added"

/**
 * This topic is notified when an entry was removed from a category in the
 * category manager. The subject of the notification will be the name of
 * the removed entry as an nsISupportsCString, and the data will be the
 * name of the category. The notification will occur on the main thread.
 *
 * @status FROZEN
 */
#define NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID \
  "xpcom-category-entry-removed"

/**
 * This topic is notified when an a category was cleared in the category
 * manager. The subject of the notification will be the category manager,
 * and the data will be the name of the cleared category.
 * The notification will occur on the main thread.
 *
 * @status FROZEN
 */
#define NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID "xpcom-category-cleared"

extern "C" NS_COM nsresult
NS_GetDebug(nsIDebug* *result);

extern "C" NS_COM nsresult
NS_GetTraceRefcnt(nsITraceRefcnt* *result);

#endif
