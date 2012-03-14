/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Firefox.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#ifndef mozilla_Module_h
#define mozilla_Module_h

#include "nscore.h"
#include "nsID.h"
#include "nsIFactory.h"
#include "nsCOMPtr.h" // for already_AddRefed

namespace mozilla {

/**
 * A module implements one or more XPCOM components. This structure is used
 * for both binary and script modules, but the registration members
 * (cids/contractids/categoryentries) are unused for modules which are loaded
 * via a module loader.
 */
struct Module
{
  static const unsigned int kVersion = 14;

  struct CIDEntry;

  typedef already_AddRefed<nsIFactory> (*GetFactoryProcPtr)
    (const Module& module, const CIDEntry& entry);

  typedef nsresult (*ConstructorProcPtr)(nsISupports* aOuter,
                                         const nsIID& aIID,
                                         void** aResult);

  typedef nsresult (*LoadFuncPtr)();
  typedef void (*UnloadFuncPtr)();

  /**
   * The constructor callback is an implementation detail of the default binary
   * loader and may be null.
   */
  struct CIDEntry
  {
    const nsCID* cid;
    bool service;
    GetFactoryProcPtr getFactoryProc;
    ConstructorProcPtr constructorProc;
  };

  struct ContractIDEntry
  {
    const char* contractid;
    nsID const * cid;
  };

  struct CategoryEntry
  {
    const char* category;
    const char* entry;
    const char* value;
  };

  /**
   * Binary compatibility check, should be kModuleVersion.
   */
  unsigned int mVersion;

  /**
   * An array of CIDs (class IDs) implemented by this module. The final entry
   * should be { NULL }.
   */
  const CIDEntry* mCIDs;

  /**
   * An array of mappings from contractid to CID. The final entry should
   * be { NULL }.
   */
  const ContractIDEntry* mContractIDs;

  /**
   * An array of category manager entries. The final entry should be
   * { NULL }.
   */
  const CategoryEntry* mCategoryEntries;

  /**
   * When the component manager tries to get the factory for a CID, it first
   * checks for this module-level getfactory callback. If this function is
   * not implemented, it checks the CIDEntry getfactory callback. If that is
   * also NULL, a generic factory is generated using the CIDEntry constructor
   * callback which must be non-NULL.
   */
  GetFactoryProcPtr getFactoryProc;

  /**
   * Optional Function which are called when this module is loaded and
   * at shutdown. These are not C++ constructor/destructors to avoid
   * calling them too early in startup or too late in shutdown.
   */
  LoadFuncPtr loadProc;
  UnloadFuncPtr unloadProc;
};

} // namespace

#if defined(XPCOM_TRANSLATE_NSGM_ENTRY_POINT)
#  define NSMODULE_NAME(_name) _name##_NSModule
#  define NSMODULE_DECL(_name) extern mozilla::Module const *const NSMODULE_NAME(_name)
#  define NSMODULE_DEFN(_name) NSMODULE_DECL(_name)
#else
#  define NSMODULE_NAME(_name) NSModule
#  define NSMODULE_DEFN(_name) extern "C" NS_EXPORT mozilla::Module const *const NSModule
#endif

#endif // mozilla_Module_h
