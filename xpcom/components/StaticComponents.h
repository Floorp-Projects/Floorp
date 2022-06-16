/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StaticComponents_h
#define StaticComponents_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Module.h"
#include "mozilla/Span.h"
#include "nsID.h"
#include "nsStringFwd.h"
#include "nscore.h"

#include "mozilla/Components.h"
#include "StaticComponentData.h"

class nsIFactory;
class nsIUTF8StringEnumerator;
class nsISupports;
template <typename T, size_t N>
class AutoTArray;

namespace mozilla {
namespace xpcom {

struct ContractEntry;
struct StaticModule;

struct StaticCategoryEntry;
struct StaticCategory;

extern const StaticModule gStaticModules[kStaticModuleCount];

extern const ContractEntry gContractEntries[kContractCount];
extern uint8_t gInvalidContracts[kContractCount / 8 + 1];

extern const StaticCategory gStaticCategories[kStaticCategoryCount];
extern const StaticCategoryEntry gStaticCategoryEntries[];

template <size_t N>
static inline bool GetBit(const uint8_t (&aBits)[N], size_t aBit) {
  static constexpr size_t width = sizeof(aBits[0]) * 8;

  size_t idx = aBit / width;
  MOZ_ASSERT(idx < N);
  return aBits[idx] & (1 << (aBit % width));
}

template <size_t N>
static inline void SetBit(uint8_t (&aBits)[N], size_t aBit,
                          bool aValue = true) {
  static constexpr size_t width = sizeof(aBits[0]) * 8;

  size_t idx = aBit / width;
  MOZ_ASSERT(idx < N);
  if (aValue) {
    aBits[idx] |= 1 << (aBit % width);
  } else {
    aBits[idx] &= ~(1 << (aBit % width));
  }
}

/**
 * Represents a string entry in the static string table. Can be converted to a
 * nsCString using GetString() in StaticComponents.cpp.
 *
 * This is a struct rather than a pure offset primarily for the purposes of type
 * safety, but also so that it can easily be extended to include a static length
 * in the future, if efficiency concerns warrant it.
 */
struct StringOffset final {
  uint32_t mOffset;
};

/**
 * Represents an offset into the interfaces table.
 */
struct InterfaceOffset final {
  uint32_t mOffset;
};

/**
 * Represents a static component entry defined in a `Classes` list in an XPCOM
 * manifest. Handles creating instances of and caching service instances for
 * that class.
 */
struct StaticModule {
  nsID mCID;
  StringOffset mContractID;
  Module::ProcessSelector mProcessSelector;

  const nsID& CID() const { return mCID; }

  ModuleID ID() const { return ModuleID(this - gStaticModules); }

  /**
   * Returns this entry's index in the gStaticModules array.
   */
  size_t Idx() const { return size_t(ID()); }

  /**
   * Returns true if this component's corresponding contract ID is expected to
   * be overridden at runtime. If so, it should always be looked up by its
   * ContractID() when retrieving its service instance.
   */
  bool Overridable() const;

  /**
   * If this entry is overridable, returns its associated contract ID string.
   * The component should always be looked up by this contract ID when
   * retrieving its service instance.
   *
   * Note: This may *only* be called if Overridable() returns true.
   */
  nsCString ContractID() const;

  /**
   * Returns true if this entry is active. Typically this will only return false
   * if the entry's process selector does not match this process.
   */
  bool Active() const;

  already_AddRefed<nsIFactory> GetFactory() const;

  nsresult CreateInstance(const nsIID& aIID, void** aResult) const;

  GetServiceHelper GetService() const;
  GetServiceHelper GetService(nsresult*) const;

  nsISupports* ServiceInstance() const;
  void SetServiceInstance(already_AddRefed<nsISupports> aInst) const;
};

/**
 * Represents a static mapping between a contract ID string and a StaticModule
 * entry.
 */
struct ContractEntry final {
  StringOffset mContractID;
  ModuleID mModuleID;

  size_t Idx() const { return this - gContractEntries; }

  nsCString ContractID() const;

  const StaticModule& Module() const {
    return gStaticModules[size_t(mModuleID)];
  }

  /**
   * Returns true if this entry's underlying module is active, and its contract
   * ID matches the given contract ID string. This is used by the PerfectHash
   * function to determine whether to return a result for this entry.
   */
  bool Matches(const nsACString& aContractID) const;

  /**
   * Returns true if this entry has been invalidated, and should be ignored.
   *
   * Contract IDs may be overwritten at runtime. When that happens for a static
   * contract ID, we mark its entry invalid, and ignore it thereafter.
   */
  bool Invalid() const { return GetBit(gInvalidContracts, Idx()); }

  /**
   * Marks this entry invalid (or unsets the invalid bit if aInvalid is false),
   * after which it will be ignored in contract ID lookup attempts. See
   * `Invalid()` above.
   */
  void SetInvalid(bool aInvalid = true) const {
    return SetBit(gInvalidContracts, Idx(), aInvalid);
  }
};

/**
 * Represents a declared category manager entry declared in an XPCOM manifest.
 *
 * The entire set of static category entries is read at startup and loaded into
 * the category manager's dynamic hash tables, so there is memory and
 * initialization overhead for each entry in these tables. This may be further
 * optimized in the future to reduce some of that overhead.
 */
struct StaticCategoryEntry final {
  StringOffset mEntry;
  StringOffset mValue;
  Module::BackgroundTasksSelector mBackgroundTasksSelector;
  Module::ProcessSelector mProcessSelector;

  nsCString Entry() const;
  nsCString Value() const;
  bool Active() const;
};

struct StaticCategory final {
  StringOffset mName;
  uint16_t mStart;
  uint16_t mCount;

  nsCString Name() const;

  const StaticCategoryEntry* begin() const {
    return &gStaticCategoryEntries[mStart];
  }
  const StaticCategoryEntry* end() const {
    return &gStaticCategoryEntries[mStart + mCount];
  }
};

struct JSServiceEntry final {
  using InterfaceList = AutoTArray<const nsIID*, 4>;

  static const JSServiceEntry* Lookup(const nsACString& aName);

  StringOffset mName;
  ModuleID mModuleID;

  InterfaceOffset mInterfaceOffset;
  uint8_t mInterfaceCount;

  nsCString Name() const;

  const StaticModule& Module() const {
    return gStaticModules[size_t(mModuleID)];
  }

  InterfaceList Interfaces() const;
};

class StaticComponents final {
 public:
  static const StaticModule* LookupByCID(const nsID& aCID);

  static const StaticModule* LookupByContractID(const nsACString& aContractID);

  /**
   * Marks a static contract ID entry invalid (or unsets the invalid bit if
   * aInvalid is false). See `CategoryEntry::Invalid()`.
   */
  static bool InvalidateContractID(const nsACString& aContractID,
                                   bool aInvalid = true);

  static already_AddRefed<nsIUTF8StringEnumerator> GetComponentJSMs();

  static Span<const JSServiceEntry> GetJSServices();

  /**
   * Calls any module unload from manifests whose components have been loaded.
   */
  static void Shutdown();
};

}  // namespace xpcom
}  // namespace mozilla

#endif  // defined StaticComponents_h
