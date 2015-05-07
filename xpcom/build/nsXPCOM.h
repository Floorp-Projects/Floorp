/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXPCOM_h__
#define nsXPCOM_h__

#include "nscore.h"
#include "nsXPCOMCID.h"

#ifdef __cplusplus
#define DECL_CLASS(c) class c
#define DECL_STRUCT(c) struct c
#else
#define DECL_CLASS(c) typedef struct c c
#define DECL_STRUCT(c) typedef struct c c
#endif

DECL_CLASS(nsAString);
DECL_CLASS(nsACString);

DECL_CLASS(nsISupports);
DECL_CLASS(nsIModule);
DECL_CLASS(nsIComponentManager);
DECL_CLASS(nsIComponentRegistrar);
DECL_CLASS(nsIServiceManager);
DECL_CLASS(nsIFile);
DECL_CLASS(nsILocalFile);
DECL_CLASS(nsIDirectoryServiceProvider);
DECL_CLASS(nsIMemory);
DECL_CLASS(nsIDebug2);

#ifdef __cplusplus
namespace mozilla {
struct Module;
}
#endif

/**
 * Initialises XPCOM. You must call one of the NS_InitXPCOM methods
 * before proceeding to use xpcom. The one exception is that you may
 * call NS_NewLocalFile to create a nsIFile.
 *
 * @note Use <CODE>NS_NewLocalFile</CODE> or <CODE>NS_NewNativeLocalFile</CODE>
 *       to create the file object you supply as the bin directory path in this
 *       call. The function may be safely called before the rest of XPCOM or
 *       embedding has been initialised.
 *
 * @param aResult          The service manager.  You may pass null.
 *
 * @param aBinDirectory    The directory containing the component
 *                         registry and runtime libraries;
 *                         or use <CODE>nullptr</CODE> to use the working
 *                         directory.
 *
 * @param aAppFileLocationProvider The object to be used by Gecko that
 *                         specifies to Gecko where to find profiles, the
 *                         component registry preferences and so on; or use
 *                         <CODE>nullptr</CODE> for the default behaviour.
 *
 * @see NS_NewLocalFile
 * @see nsIFile
 * @see nsIDirectoryServiceProvider
 *
 * @return NS_OK for success;
 *         NS_ERROR_NOT_INITIALIZED if static globals were not initialized,
 *         which can happen if XPCOM is reloaded, but did not completly
 *         shutdown. Other error codes indicate a failure during
 *         initialisation.
 */
XPCOM_API(nsresult)
NS_InitXPCOM2(nsIServiceManager** aResult,
              nsIFile* aBinDirectory,
              nsIDirectoryServiceProvider* aAppFileLocationProvider);

/**
 * Shutdown XPCOM. You must call this method after you are finished
 * using xpcom.
 *
 * @param aServMgr          The service manager which was returned by NS_InitXPCOM.
 *                          This will release servMgr.  You may pass null.
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 */
XPCOM_API(nsresult) NS_ShutdownXPCOM(nsIServiceManager* aServMgr);


/**
 * Public Method to access to the service manager.
 *
 * @param aResult Interface pointer to the service manager
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 */
XPCOM_API(nsresult) NS_GetServiceManager(nsIServiceManager** aResult);

/**
 * Public Method to access to the component manager.
 *
 * @param aResult Interface pointer to the service
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 */
XPCOM_API(nsresult) NS_GetComponentManager(nsIComponentManager** aResult);


/**
 * Public Method to access to the component registration manager.
 *
 * @param aResult Interface pointer to the service
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 */
XPCOM_API(nsresult) NS_GetComponentRegistrar(nsIComponentRegistrar** aResult);

/**
 * Public Method to access to the memory manager.  See nsIMemory
 *
 * @param aResult Interface pointer to the memory manager
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during initialisation.
 */
XPCOM_API(nsresult) NS_GetMemoryManager(nsIMemory** aResult);

/**
 * Public Method to create an instance of a nsIFile.  This function
 * may be called prior to NS_InitXPCOM.
 *
 *   @param aPath
 *       A string which specifies a full file path to a
 *       location.  Relative paths will be treated as an
 *       error (NS_ERROR_FILE_UNRECOGNIZED_PATH).
 *       |NS_NewNativeLocalFile|'s path must be in the
 *       filesystem charset.
 *   @param aFollowLinks
 *       This attribute will determine if the nsLocalFile will auto
 *       resolve symbolic links.  By default, this value will be false
 *       on all non unix systems.  On unix, this attribute is effectively
 *       a noop.
 * @param aResult Interface pointer to a new instance of an nsIFile
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure.
 */

#ifdef __cplusplus

XPCOM_API(nsresult) NS_NewLocalFile(const nsAString& aPath,
                                    bool aFollowLinks,
                                    nsIFile** aResult);

XPCOM_API(nsresult) NS_NewNativeLocalFile(const nsACString& aPath,
                                          bool aFollowLinks,
                                          nsIFile** aResult);

#endif

/**
 * Allocator functions for the standalone glue.
 * Do not use outside the xpcom glue code.
 * Use moz_xmalloc/moz_xrealloc/free, or new/delete instead.
 */
#ifdef XPCOM_GLUE
/**
 * Allocates a block of memory of a particular size. If the memory cannot
 * be allocated (because of an out-of-memory condition), the process aborts.
 *
 * @param aSize  The size of the block to allocate
 * @result       The block of memory
 * @note         This function is thread-safe.
 */
XPCOM_API(void*) NS_Alloc(size_t aSize);

/**
 * Reallocates a block of memory to a new size.
 *
 * @param aPtr    The block of memory to reallocate. This block must originally
                  have been allocated by NS_Alloc or NS_Realloc
 * @param aSize   The new size. If 0, frees the block like NS_Free
 * @result        The reallocated block of memory
 * @note          This function is thread-safe.
 *
 * If aPtr is null, this function behaves like NS_Alloc.
 * If s is the size of the block to which aPtr points, the first min(s, size)
 * bytes of aPtr's block are copied to the new block. If the allocation
 * succeeds, aPtr is freed and a pointer to the new block is returned. If the
 * allocation fails, the process aborts.
 */
XPCOM_API(void*) NS_Realloc(void* aPtr, size_t aSize);

/**
 * Frees a block of memory. Null is a permissible value, in which case no
 * action is taken.
 *
 * @param aPtr  The block of memory to free. This block must originally have
 *              been allocated by NS_Alloc or NS_Realloc
 * @note        This function is thread-safe.
 */
XPCOM_API(void) NS_Free(void* aPtr);
#else
#define NS_Alloc moz_xmalloc
#define NS_Realloc moz_xrealloc
#define NS_Free free
#endif

/**
 * Support for warnings, assertions, and debugging breaks.
 */

enum
{
  NS_DEBUG_WARNING = 0,
  NS_DEBUG_ASSERTION = 1,
  NS_DEBUG_BREAK = 2,
  NS_DEBUG_ABORT = 3
};

/**
 * Print a runtime assertion. This function is available in both debug and
 * release builds.
 *
 * @note Based on the value of aSeverity and the XPCOM_DEBUG_BREAK
 * environment variable, this function may cause the application to
 * print the warning, print a stacktrace, break into a debugger, or abort
 * immediately.
 *
 * @param aSeverity A NS_DEBUG_* value
 * @param aStr   A readable error message (ASCII, may be null)
 * @param aExpr  The expression evaluated (may be null)
 * @param aFile  The source file containing the assertion (may be null)
 * @param aLine  The source file line number (-1 indicates no line number)
 */
XPCOM_API(void) NS_DebugBreak(uint32_t aSeverity,
                              const char* aStr, const char* aExpr,
                              const char* aFile, int32_t aLine);

/**
 * Perform a stack-walk to a debugging log under various
 * circumstances. Used to aid debugging of leaked object graphs.
 *
 * The NS_Log* functions are available in both debug and release
 * builds of XPCOM, but the output will be useless unless binary
 * debugging symbols for all modules in the stacktrace are available.
 */

/**
 * By default, refcount logging is enabled at NS_InitXPCOM and
 * refcount statistics are printed at NS_ShutdownXPCOM. NS_LogInit and
 * NS_LogTerm allow applications to enable logging earlier and delay
 * printing of logging statistics. They should always be used as a
 * matched pair.
 */
XPCOM_API(void) NS_LogInit();

XPCOM_API(void) NS_LogTerm();

/**
 * Log construction and destruction of objects. Processing tools can use the
 * stacktraces printed by these functions to identify objects that are being
 * leaked.
 *
 * @param aPtr          A pointer to the concrete object.
 * @param aTypeName     The class name of the type
 * @param aInstanceSize The size of the type
 */

XPCOM_API(void) NS_LogCtor(void* aPtr, const char* aTypeName,
                           uint32_t aInstanceSize);

XPCOM_API(void) NS_LogDtor(void* aPtr, const char* aTypeName,
                           uint32_t aInstanceSize);

/**
 * Log a stacktrace when an XPCOM object's refcount is incremented or
 * decremented. Processing tools can use the stacktraces printed by these
 * functions to identify objects that were leaked due to XPCOM references.
 *
 * @param aPtr          A pointer to the concrete object
 * @param aNewRefCnt    The new reference count.
 * @param aTypeName     The class name of the type
 * @param aInstanceSize The size of the type
 */
XPCOM_API(void) NS_LogAddRef(void* aPtr, nsrefcnt aNewRefCnt,
                             const char* aTypeName, uint32_t aInstanceSize);

XPCOM_API(void) NS_LogRelease(void* aPtr, nsrefcnt aNewRefCnt,
                              const char* aTypeName);

/**
 * Log reference counting performed by COMPtrs. Processing tools can
 * use the stacktraces printed by these functions to simplify reports
 * about leaked objects generated from the data printed by
 * NS_LogAddRef/NS_LogRelease.
 *
 * @param aCOMPtr the address of the COMPtr holding a strong reference
 * @param aObject the object being referenced by the COMPtr
 */

XPCOM_API(void) NS_LogCOMPtrAddRef(void* aCOMPtr, nsISupports* aObject);

XPCOM_API(void) NS_LogCOMPtrRelease(void* aCOMPtr, nsISupports* aObject);

/**
 * The XPCOM cycle collector analyzes and breaks reference cycles between
 * participating XPCOM objects. All objects in the cycle must implement
 * nsCycleCollectionParticipant to break cycles correctly.
 */

#ifdef __cplusplus

class nsCycleCollectionParticipant;
class nsCycleCollectingAutoRefCnt;

XPCOM_API(void) NS_CycleCollectorSuspect3(void* aPtr,
                                          nsCycleCollectionParticipant* aCp,
                                          nsCycleCollectingAutoRefCnt* aRefCnt,
                                          bool* aShouldDelete);

#endif

/**
 * Categories (in the category manager service) used by XPCOM:
 */

/**
 * A category which is read after component registration but before
 * the "xpcom-startup" notifications. Each category entry is treated
 * as the contract ID of a service which implements
 * nsIDirectoryServiceProvider. Each directory service provider is
 * installed in the global directory service.
 */
#define XPCOM_DIRECTORY_PROVIDER_CATEGORY "xpcom-directory-providers"

/**
 * A category which is read after component registration but before
 * NS_InitXPCOM returns. Each category entry is treated as the contractID of
 * a service: each service is instantiated, and if it implements nsIObserver
 * the nsIObserver.observe method is called with the "xpcom-startup" topic.
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
 */
#define NS_XPCOM_STARTUP_OBSERVER_ID "xpcom-startup"

/**
 * At XPCOM shutdown, this topic is notified just before "xpcom-shutdown".
 * Components should only use this to mark themselves as 'being destroyed'.
 * Nothing should be dispatched to any event loop.
 */
#define NS_XPCOM_WILL_SHUTDOWN_OBSERVER_ID "xpcom-will-shutdown"

/**
 * At XPCOM shutdown, this topic is notified. All components must
 * release any interface references to objects in other modules when
 * this topic is notified.
 */
#define NS_XPCOM_SHUTDOWN_OBSERVER_ID "xpcom-shutdown"

/**
 * This topic is notified when an entry was added to a category in the
 * category manager. The subject of the notification will be the name of
 * the added entry as an nsISupportsCString, and the data will be the
 * name of the category. The notification will occur on the main thread.
 */
#define NS_XPCOM_CATEGORY_ENTRY_ADDED_OBSERVER_ID \
  "xpcom-category-entry-added"

/**
 * This topic is notified when an entry was removed from a category in the
 * category manager. The subject of the notification will be the name of
 * the removed entry as an nsISupportsCString, and the data will be the
 * name of the category. The notification will occur on the main thread.
 */
#define NS_XPCOM_CATEGORY_ENTRY_REMOVED_OBSERVER_ID \
  "xpcom-category-entry-removed"

/**
 * This topic is notified when an a category was cleared in the category
 * manager. The subject of the notification will be the category manager,
 * and the data will be the name of the cleared category.
 * The notification will occur on the main thread.
 */
#define NS_XPCOM_CATEGORY_CLEARED_OBSERVER_ID "xpcom-category-cleared"

XPCOM_API(nsresult) NS_GetDebug(nsIDebug2** aResult);

#endif
