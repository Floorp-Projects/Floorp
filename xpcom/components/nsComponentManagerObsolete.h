/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is XPCOM.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#ifndef nsComponentManagerObsolete_h___
#define nsComponentManagerObsolete_h___

#include "nsIComponentManager.h"
#include "nsIComponentManagerObsolete.h"

class nsIEnumerator;
class nsIFactory;
class nsIFile;
////////////////////////////////////////////////////////////////////
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Functions, classes, interfaces and types in this file  are 
// obsolete.  Use at your own risk.  
// Please see nsIComponentManager.idl for the supported interface
// to the component manager.
//
////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////
// God save me from this evilness.  Below is a very bad 
// function.  Its out var is really a nsIComponentManagerObsolete
// but it has been cast to a nsIComponentManager.
// The reason for such uglyness is that this function is require for
// backward compatiblity of some plugins.  This funciton will 
// be removed at some point.
////////////////////////////////////////////////////////////////////

extern NS_COM nsresult
NS_GetGlobalComponentManager(nsIComponentManager* *result);




class NS_COM nsComponentManager {
public:
  static nsresult Initialize(void);

  // Finds a factory for a specific class ID
  static nsresult FindFactory(const nsCID &aClass,
                              nsIFactory **aFactory);

  // Get the singleton class object that implements the CID aClass
  static nsresult GetClassObject(const nsCID &aClass, const nsIID &aIID,
                                 void **aResult);

  // Get the singleton class object that implements the CID aClass
  static nsresult GetClassObjectByContractID(const char *aContractID,
                                             const nsIID &aIID,
                                             void **aResult);

  // Finds a class ID for a specific Program ID
  static nsresult ContractIDToClassID(const char *aContractID,
                                nsCID *aClass);
  
  // Finds a Program ID for a specific class ID
  // caller frees the result with delete[]
  static nsresult CLSIDToContractID(nsCID *aClass,
                                char* *aClassName,
                                char* *aContractID);
  
  // Creates a class instance for a specific class ID
  static nsresult CreateInstance(const nsCID &aClass, 
                                 nsISupports *aDelegate,
                                 const nsIID &aIID,
                                 void **aResult);

  // Convenience routine, creates a class instance for a specific ContractID
  static nsresult CreateInstance(const char *aContractID,
                                 nsISupports *aDelegate,
                                 const nsIID &aIID,
                                 void **aResult);

  // Manually registry a factory for a class
  static nsresult RegisterFactory(const nsCID &aClass,
                                  const char *aClassName,
                                  const char *aContractID,
                                  nsIFactory *aFactory,
                                  PRBool aReplace);

  // Manually register a dynamically loaded component.
  // The libraryPersistentDescriptor is what gets passed to the library
  // self register function from ComponentManager. The format of this string
  // is the same as nsIFile::GetPath()
  //
  // This function will go away in favour of RegisterComponentSpec. In fact,
  // it internally turns around and calls RegisterComponentSpec.
  static nsresult RegisterComponent(const nsCID &aClass,
                               const char *aClassName,
                               const char *aContractID,
                               const char *aLibraryPersistentDescriptor,
                               PRBool aReplace,
                               PRBool aPersist);

  // Register a component using its FileSpec as its identification
  // This is the more prevalent use.
  static nsresult RegisterComponentSpec(const nsCID &aClass,
                                   const char *aClassName,
                                   const char *aContractID,
                                   nsIFile *aLibrary,
                                   PRBool aReplace,
                                   PRBool aPersist);

  // Register a component using its dllName. This could be a dll name with
  // no path so that LD_LIBRARY_PATH on unix or PATH on win can load it. Or
  // this could be a code fragment name on the Mac.
  static nsresult RegisterComponentLib(const nsCID &aClass,
                                       const char *aClassName,
                                       const char *aContractID,
                                       const char *adllName,
                                       PRBool aReplace,
                                       PRBool aPersist);
  

  // Manually unregister a factory for a class
  static nsresult UnregisterFactory(const nsCID &aClass,
                                    nsIFactory *aFactory);

  // Manually unregister a dynamically loaded component
  static nsresult UnregisterComponent(const nsCID &aClass,
                                      const char *aLibrary);

  // Manually unregister a dynamically loaded component
  static nsresult UnregisterComponentSpec(const nsCID &aClass,
                                          nsIFile *aLibrarySpec);

  // Unload dynamically loaded factories that are not in use
  static nsresult FreeLibraries(void);
  //////////////////////////////////////////////////////////////////////////////
  // DLL registration support

  // If directory is NULL, then AutoRegister will try registering components
  // in the default components directory.
  static nsresult AutoRegister(PRInt32 when, nsIFile* directory);
  static nsresult AutoRegisterComponent(PRInt32 when, nsIFile *component);
  static nsresult AutoUnregisterComponent(PRInt32 when, nsIFile *component);

  // Is the given CID currently registered?
  static nsresult IsRegistered(const nsCID &aClass,
                               PRBool *aRegistered);

  // Get an enumeration of all the CIDs
  static nsresult EnumerateCLSIDs(nsIEnumerator** aEmumerator);
    
  // Get an enumeration of all the ContractIDs
  static nsresult EnumerateContractIDs(nsIEnumerator** aEmumerator);

};


#endif
