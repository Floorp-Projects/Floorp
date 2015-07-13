/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  static const unsigned int kVersion = 42;

  struct CIDEntry;

  typedef already_AddRefed<nsIFactory> (*GetFactoryProcPtr)(
    const Module& module, const CIDEntry& entry);

  typedef nsresult (*ConstructorProcPtr)(nsISupports* aOuter,
                                         const nsIID& aIID,
                                         void** aResult);

  typedef nsresult (*LoadFuncPtr)();
  typedef void (*UnloadFuncPtr)();

  /**
   * This selector allows CIDEntrys to be marked so that they're only loaded
   * into certain kinds of processes.
   */
  enum ProcessSelector
  {
    ANY_PROCESS = 0,
    MAIN_PROCESS_ONLY,
    CONTENT_PROCESS_ONLY
  };

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
    ProcessSelector processSelector;
  };

  struct ContractIDEntry
  {
    const char* contractid;
    nsID const* cid;
    ProcessSelector processSelector;
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
   * should be { nullptr }.
   */
  const CIDEntry* mCIDs;

  /**
   * An array of mappings from contractid to CID. The final entry should
   * be { nullptr }.
   */
  const ContractIDEntry* mContractIDs;

  /**
   * An array of category manager entries. The final entry should be
   * { nullptr }.
   */
  const CategoryEntry* mCategoryEntries;

  /**
   * When the component manager tries to get the factory for a CID, it first
   * checks for this module-level getfactory callback. If this function is
   * not implemented, it checks the CIDEntry getfactory callback. If that is
   * also nullptr, a generic factory is generated using the CIDEntry
   * constructor callback which must be non-nullptr.
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

} // namespace mozilla

#if defined(MOZILLA_INTERNAL_API)
#  define NSMODULE_NAME(_name) _name##_NSModule
#  if defined(_MSC_VER)
#    pragma section(".kPStaticModules$M", read)
#    pragma comment(linker, "/merge:.kPStaticModules=.rdata")
#    define NSMODULE_SECTION __declspec(allocate(".kPStaticModules$M"), dllexport)
#  elif defined(__GNUC__)
#    if defined(__ELF__)
#      define NSMODULE_SECTION __attribute__((section(".kPStaticModules"), visibility("protected")))
#    elif defined(__MACH__)
#      define NSMODULE_SECTION __attribute__((section("__DATA, .kPStaticModules"), visibility("default")))
#    elif defined (_WIN32)
#      define NSMODULE_SECTION __attribute__((section(".kPStaticModules"), dllexport))
#    endif
#  endif
#  if !defined(NSMODULE_SECTION)
#    error Do not know how to define sections.
#  endif
#  define NSMODULE_DEFN(_name) extern NSMODULE_SECTION mozilla::Module const *const NSMODULE_NAME(_name)
#else
#  define NSMODULE_NAME(_name) NSModule
#  define NSMODULE_DEFN(_name) extern "C" NS_EXPORT mozilla::Module const *const NSModule
#endif

#endif // mozilla_Module_h
