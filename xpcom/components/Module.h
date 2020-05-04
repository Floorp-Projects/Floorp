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
#include "nsCOMPtr.h"  // for already_AddRefed

namespace mozilla {

/**
 * A module implements one or more XPCOM components. This structure is used
 * for both binary and script modules, but the registration members
 * (cids/contractids/categoryentries) are unused for modules which are loaded
 * via a module loader.
 */
struct Module {
  static const unsigned int kVersion = 78;

  struct CIDEntry;

  typedef already_AddRefed<nsIFactory> (*GetFactoryProcPtr)(
      const Module& module, const CIDEntry& entry);

  typedef nsresult (*ConstructorProcPtr)(nsISupports* aOuter, const nsIID& aIID,
                                         void** aResult);

  typedef nsresult (*LoadFuncPtr)();
  typedef void (*UnloadFuncPtr)();

  /**
   * This selector allows CIDEntrys to be marked so that they're only loaded
   * into certain kinds of processes. Selectors can be combined.
   */
  // Note: This must be kept in sync with the selector matching in
  // nsComponentManager.cpp.
  enum ProcessSelector {
    ANY_PROCESS = 0x0,
    MAIN_PROCESS_ONLY = 0x1,
    CONTENT_PROCESS_ONLY = 0x2,

    /**
     * By default, modules are not loaded in the GPU process, even if
     * ANY_PROCESS is specified. This flag enables a module in the
     * GPU process.
     */
    ALLOW_IN_GPU_PROCESS = 0x4,
    ALLOW_IN_VR_PROCESS = 0x8,
    ALLOW_IN_SOCKET_PROCESS = 0x10,
    ALLOW_IN_RDD_PROCESS = 0x20,
    ALLOW_IN_GPU_AND_VR_PROCESS = ALLOW_IN_GPU_PROCESS | ALLOW_IN_VR_PROCESS,
    ALLOW_IN_GPU_AND_SOCKET_PROCESS =
        ALLOW_IN_GPU_PROCESS | ALLOW_IN_SOCKET_PROCESS,
    ALLOW_IN_GPU_VR_AND_SOCKET_PROCESS =
        ALLOW_IN_GPU_PROCESS | ALLOW_IN_VR_PROCESS | ALLOW_IN_SOCKET_PROCESS,
    ALLOW_IN_RDD_AND_SOCKET_PROCESS =
        ALLOW_IN_RDD_PROCESS | ALLOW_IN_SOCKET_PROCESS,
    ALLOW_IN_GPU_RDD_AND_SOCKET_PROCESS =
        ALLOW_IN_GPU_PROCESS | ALLOW_IN_RDD_PROCESS | ALLOW_IN_SOCKET_PROCESS,
    ALLOW_IN_GPU_RDD_VR_AND_SOCKET_PROCESS =
        ALLOW_IN_GPU_PROCESS | ALLOW_IN_RDD_PROCESS | ALLOW_IN_VR_PROCESS |
        ALLOW_IN_SOCKET_PROCESS
  };

  static constexpr size_t kMaxProcessSelector =
      size_t(ProcessSelector::ALLOW_IN_GPU_RDD_VR_AND_SOCKET_PROCESS);

  /**
   * The constructor callback is an implementation detail of the default binary
   * loader and may be null.
   */
  struct CIDEntry {
    const nsCID* cid;
    bool service;
    GetFactoryProcPtr getFactoryProc;
    ConstructorProcPtr constructorProc;
    ProcessSelector processSelector;
  };

  struct ContractIDEntry {
    const char* contractid;
    nsID const* cid;
    ProcessSelector processSelector;
  };

  struct CategoryEntry {
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

  /**
   * Optional flags which control whether the module loads on a process-type
   * basis.
   */
  ProcessSelector selector;
};

}  // namespace mozilla

#endif  // mozilla_Module_h
