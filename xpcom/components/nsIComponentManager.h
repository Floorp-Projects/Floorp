/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef nsIComponentManager_h__
#define nsIComponentManager_h__

#include "prtypes.h"
#include "nsCom.h"
#include "nsID.h"
#include "nsError.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIFileSpec.h"
#include "nsIEnumerator.h"

/*
 * Prototypes for dynamic library export functions. Your DLL/DSO needs to export
 * these methods to play in the component world.
 */

extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory);
extern "C" NS_EXPORT PRBool   NSCanUnload(nsISupports* aServMgr);
extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char *fullpath);
extern "C" NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char *fullpath);

typedef nsresult (*nsFactoryProc)(nsISupports* aServMgr,
                                  const nsCID &aClass,
                                  const char *aClassName,
                                  const char *aProgID,
                                  nsIFactory **aFactory);
typedef PRBool   (*nsCanUnloadProc)(nsISupports* aServMgr);
typedef nsresult (*nsRegisterProc)(nsISupports* aServMgr, const char *path);
typedef nsresult (*nsUnregisterProc)(nsISupports* aServMgr, const char *path);

#define NS_ICOMPONENTMANAGER_IID                     \
{ /* 8458a740-d5dc-11d2-92fb-00e09805570f */         \
    0x8458a740,                                      \
    0xd5dc,                                          \
    0x11d2,                                          \
    {0x92, 0xfb, 0x00, 0xe0, 0x98, 0x05, 0x57, 0x0f} \
}

#define NS_COMPONENTMANAGER_CID                      \
{ /* 91775d60-d5dc-11d2-92fb-00e09805570f */         \
    0x91775d60,                                      \
    0xd5dc,                                          \
    0x11d2,                                          \
    {0x92, 0xfb, 0x00, 0xe0, 0x98, 0x05, 0x57, 0x0f} \
}

/*
 * nsIComponentManager interface
 */

class nsIComponentManager : public nsISupports {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICOMPONENTMANAGER_IID)

  NS_IMETHOD FindFactory(const nsCID &aClass,
                         nsIFactory **aFactory) = 0;

  // Finds a class ID for a specific Program ID
  NS_IMETHOD ProgIDToCLSID(const char *aProgID,
                           nsCID *aClass) = 0;
  
  // Finds a Program ID for a specific class ID
  // caller frees the result with delete[]
  NS_IMETHOD CLSIDToProgID(nsCID *aClass,
                           char* *aClassName,
                           char* *aProgID) = 0;
  
  // Creates a class instance for a specific class ID
  NS_IMETHOD CreateInstance(const nsCID &aClass, 
                            nsISupports *aDelegate,
                            const nsIID &aIID,
                            void **aResult) = 0;

  // Convenience routine, creates a class instance for a specific ProgID
  NS_IMETHOD CreateInstance(const char *aProgID,
                            nsISupports *aDelegate,
                            const nsIID &aIID,
                            void **aResult) = 0;

  // Manually registry a factory for a class
  NS_IMETHOD RegisterFactory(const nsCID &aClass,
                             const char *aClassName,
                             const char *aProgID,
                             nsIFactory *aFactory,
                             PRBool aReplace) = 0;

  // Manually register a dynamically loaded component.
  // The libraryPersistentDescriptor is what gets passed to the library
  // self register function from ComponentManager. The format of this string
  // is the same as nsIFileSpec::GetPersistentDescriptorString()
  //
  // This function will go away in favour of RegisterComponentSpec. In fact,
  // it internally turns around and calls RegisterComponentSpec.
  NS_IMETHOD RegisterComponent(const nsCID &aClass,
                               const char *aClassName,
                               const char *aProgID,
                               const char *aLibraryPersistentDescriptor,
                               PRBool aReplace,
                               PRBool aPersist) = 0;

  // Register a component using its FileSpec as its identification
  // This is the more prevalent use.
  NS_IMETHOD RegisterComponentSpec(const nsCID &aClass,
                                   const char *aClassName,
                                   const char *aProgID,
                                   nsIFileSpec *aLibrary,
                                   PRBool aReplace,
                                   PRBool aPersist) = 0;
  
  // Register a component using its dllName. This could be a dll name with
  // no path so that LD_LIBRARY_PATH on unix or PATH on win can load it. Or
  // this could be a code fragment name on the Mac.
  NS_IMETHOD RegisterComponentLib(const nsCID &aClass,
                                  const char *aClassName,
                                  const char *aProgID,
                                  const char *adllName,
                                  PRBool aReplace,
                                  PRBool aPersist) = 0;
  
  // Manually unregister a factory for a class
  NS_IMETHOD UnregisterFactory(const nsCID &aClass,
                               nsIFactory *aFactory) = 0;

  // Manually unregister a dynamically loaded component
  NS_IMETHOD UnregisterComponent(const nsCID &aClass,
                                 const char *aLibrary) = 0;

  // Unload dynamically loaded factories that are not in use
  NS_IMETHOD FreeLibraries(void) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // DLL registration support
  // Autoregistration will try only files with these extensions.
  // All extensions are case insensitive.
  // ".dll",    // Windows
  // ".dso",    // Unix
  // ".so",     // Unix
  // ".sl",     // Unix: HP
  // ".shlb",	// Mac
  // ".dlm",    // new for all platforms
  //
  // Directory and fullname are what NSPR will accept. For eg.
  //	MAC		/Hard drive/mozilla/dist/bin
  // 	WIN		y:\Hard drive\mozilla\dist\bin (or) y:/Hard drive/mozilla/dist/bin
  //	UNIX	/Hard drive/mozilla/dist/bin
  //
  enum RegistrationTime {
	NS_Startup = 0,
	NS_Script = 1,
	NS_Timer = 2
  };

  NS_IMETHOD AutoRegister(RegistrationTime when, nsIFileSpec* directory) = 0;
  NS_IMETHOD AutoRegisterComponent(RegistrationTime when, nsIFileSpec *component) = 0;

  // Is the given CID currently registered?
  NS_IMETHOD IsRegistered(const nsCID &aClass,
                          PRBool *aRegistered) = 0;

  // Get an enumeration of all the CIDs
  NS_IMETHOD EnumerateCLSIDs(nsIEnumerator** aEmumerator) = 0;
    
  // Get an enumeration of all the ProgIDs
  NS_IMETHOD EnumerateProgIDs(nsIEnumerator** aEmumerator) = 0;

};

////////////////////////////////////////////////////////////////////////////////

extern NS_COM nsresult
NS_GetGlobalComponentManager(nsIComponentManager* *result);

////////////////////////////////////////////////////////////////////////////////
// Global Static Component Manager Methods
// (for when you need to link with xpcom)

class NS_COM nsComponentManager {
public:
  static nsresult Initialize(void);

  // Finds a factory for a specific class ID
  static nsresult FindFactory(const nsCID &aClass,
                              nsIFactory **aFactory);

  // Finds a class ID for a specific Program ID
  static nsresult ProgIDToCLSID(const char *aProgID,
                                nsCID *aClass);
  
  // Finds a Program ID for a specific class ID
  // caller frees the result with delete[]
  static nsresult CLSIDToProgID(nsCID *aClass,
                                char* *aClassName,
                                char* *aProgID);
  
  // Creates a class instance for a specific class ID
  static nsresult CreateInstance(const nsCID &aClass, 
                                 nsISupports *aDelegate,
                                 const nsIID &aIID,
                                 void **aResult);

  // Convenience routine, creates a class instance for a specific ProgID
  static nsresult CreateInstance(const char *aProgID,
                                 nsISupports *aDelegate,
                                 const nsIID &aIID,
                                 void **aResult);

  // Manually registry a factory for a class
  static nsresult RegisterFactory(const nsCID &aClass,
                                  const char *aClassName,
                                  const char *aProgID,
                                  nsIFactory *aFactory,
                                  PRBool aReplace);

  // Manually register a dynamically loaded component.
  // The libraryPersistentDescriptor is what gets passed to the library
  // self register function from ComponentManager. The format of this string
  // is the same as nsIFileSpec::GetPersistentDescriptorString()
  //
  // This function will go away in favour of RegisterComponentSpec. In fact,
  // it internally turns around and calls RegisterComponentSpec.
  static nsresult RegisterComponent(const nsCID &aClass,
                               const char *aClassName,
                               const char *aProgID,
                               const char *aLibraryPersistentDescriptor,
                               PRBool aReplace,
                               PRBool aPersist);

  // Register a component using its FileSpec as its identification
  // This is the more prevalent use.
  static nsresult RegisterComponentSpec(const nsCID &aClass,
                                   const char *aClassName,
                                   const char *aProgID,
                                   nsIFileSpec *aLibrary,
                                   PRBool aReplace,
                                   PRBool aPersist);

  // Register a component using its dllName. This could be a dll name with
  // no path so that LD_LIBRARY_PATH on unix or PATH on win can load it. Or
  // this could be a code fragment name on the Mac.
  static nsresult RegisterComponentLib(const nsCID &aClass,
                                       const char *aClassName,
                                       const char *aProgID,
                                       const char *adllName,
                                       PRBool aReplace,
                                       PRBool aPersist);
  

  // Manually unregister a factory for a class
  static nsresult UnregisterFactory(const nsCID &aClass,
                                    nsIFactory *aFactory);

  // Manually unregister a dynamically loaded component
  static nsresult UnregisterComponent(const nsCID &aClass,
                                      const char *aLibrary);

  // Unload dynamically loaded factories that are not in use
  static nsresult FreeLibraries(void);
  //////////////////////////////////////////////////////////////////////////////
  // DLL registration support

  // If directory is NULL, then AutoRegister will try registering components
  // in the default components directory which is got by
  // nsSpecialSystemDirectory(XPCOM_CurrentProcessComponentDirectory)
  static nsresult AutoRegister(nsIComponentManager::RegistrationTime when,
                               nsIFileSpec* directory);
  static nsresult AutoRegisterComponent(nsIComponentManager::RegistrationTime when,
                                        nsIFileSpec *component);

  // Is the given CID currently registered?
  static nsresult IsRegistered(const nsCID &aClass,
                               PRBool *aRegistered);

  // Get an enumeration of all the CIDs
  static nsresult EnumerateCLSIDs(nsIEnumerator** aEmumerator);
    
  // Get an enumeration of all the ProgIDs
  static nsresult EnumerateProgIDs(nsIEnumerator** aEmumerator);

};

////////////////////////////////////////////////////////////////////////////////

#endif
