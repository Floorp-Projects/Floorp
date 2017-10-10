/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef prefapi_h
#define prefapi_h

#include "nscore.h"
#include "PLDHashTable.h"

#ifdef __cplusplus
extern "C" {
#endif

// 1 MB should be enough for everyone.
static const uint32_t MAX_PREF_LENGTH = 1 * 1024 * 1024;
// Actually, 4kb should be enough for everyone.
static const uint32_t MAX_ADVISABLE_PREF_LENGTH = 4 * 1024;

typedef union {
  char* mStringVal;
  int32_t mIntVal;
  bool mBoolVal;
} PrefValue;

// The Init function initializes the preference context and creates the
// preference hashtable.
void
PREF_Init();

// Cleanup should be called at program exit to free the list of registered
// callbacks.
void
PREF_Cleanup();
void
PREF_CleanupPrefs();

// Preference flags, including the native type of the preference. Changing any
// of these values will require modifying the code inside of PrefTypeFlags
// class.
enum class PrefType
{
  Invalid = 0,
  String = 1,
  Int = 2,
  Bool = 3,
};

// Keep the type of the preference, as well as the flags guiding its behaviour.
class PrefTypeFlags
{
public:
  PrefTypeFlags()
    : mValue(AsInt(PrefType::Invalid))
  {
  }

  explicit PrefTypeFlags(PrefType aType)
    : mValue(AsInt(aType))
  {
  }

  PrefTypeFlags& Reset()
  {
    mValue = AsInt(PrefType::Invalid);
    return *this;
  }

  bool IsTypeValid() const { return !IsPrefType(PrefType::Invalid); }
  bool IsTypeString() const { return IsPrefType(PrefType::String); }
  bool IsTypeInt() const { return IsPrefType(PrefType::Int); }
  bool IsTypeBool() const { return IsPrefType(PrefType::Bool); }
  bool IsPrefType(PrefType type) const { return GetPrefType() == type; }

  PrefTypeFlags& SetPrefType(PrefType aType)
  {
    mValue = mValue - AsInt(GetPrefType()) + AsInt(aType);
    return *this;
  }

  PrefType GetPrefType() const
  {
    return (PrefType)(mValue & (AsInt(PrefType::String) | AsInt(PrefType::Int) |
                                AsInt(PrefType::Bool)));
  }

  bool HasDefault() const { return mValue & PREF_FLAG_HAS_DEFAULT; }

  PrefTypeFlags& SetHasDefault(bool aSetOrUnset)
  {
    return SetFlag(PREF_FLAG_HAS_DEFAULT, aSetOrUnset);
  }

  bool HasStickyDefault() const { return mValue & PREF_FLAG_STICKY_DEFAULT; }

  PrefTypeFlags& SetHasStickyDefault(bool aSetOrUnset)
  {
    return SetFlag(PREF_FLAG_STICKY_DEFAULT, aSetOrUnset);
  }

  bool IsLocked() const { return mValue & PREF_FLAG_LOCKED; }

  PrefTypeFlags& SetLocked(bool aSetOrUnset)
  {
    return SetFlag(PREF_FLAG_LOCKED, aSetOrUnset);
  }

  bool HasUserValue() const { return mValue & PREF_FLAG_USERSET; }

  PrefTypeFlags& SetHasUserValue(bool aSetOrUnset)
  {
    return SetFlag(PREF_FLAG_USERSET, aSetOrUnset);
  }

private:
  static uint16_t AsInt(PrefType aType) { return (uint16_t)aType; }

  PrefTypeFlags& SetFlag(uint16_t aFlag, bool aSetOrUnset)
  {
    mValue = aSetOrUnset ? mValue | aFlag : mValue & ~aFlag;
    return *this;
  }

  // We pack both the value of type (PrefType) and flags into the same int. The
  // flag enum starts at 4 so that the PrefType can occupy the bottom two bits.
  enum
  {
    PREF_FLAG_LOCKED = 4,
    PREF_FLAG_USERSET = 8,
    PREF_FLAG_CONFIG = 16,
    PREF_FLAG_REMOTE = 32,
    PREF_FLAG_LILOCAL = 64,
    PREF_FLAG_HAS_DEFAULT = 128,
    PREF_FLAG_STICKY_DEFAULT = 256,
  };
  uint16_t mValue;
};

struct PrefHashEntry : PLDHashEntryHdr
{
  PrefTypeFlags mPrefFlags; // this field first to minimize 64-bit struct size
  const char* mKey;
  PrefValue mDefaultPref;
  PrefValue mUserPref;
};

// Set the various types of preferences. These functions take a dotted notation
// of the preference name (e.g. "browser.startup.homepage"). Note that this
// will cause the preference to be saved to the file if it is different from
// the default. In other words, these are used to set the _user_ preferences.
//
// If aSetDefault is set to true however, it sets the default value. This will
// only affect the program behavior if the user does not have a value saved
// over it for the particular preference. In addition, these will never be
// saved out to disk.
//
// Each set returns PREF_VALUECHANGED if the user value changed (triggering a
// callback), or PREF_NOERROR if the value was unchanged.
nsresult
PREF_SetCharPref(const char* aPref, const char* aVal, bool aSetDefault = false);
nsresult
PREF_SetIntPref(const char* aPref, int32_t aVal, bool aSetDefault = false);
nsresult
PREF_SetBoolPref(const char* aPref, bool aVal, bool aSetDefault = false);

bool
PREF_HasUserPref(const char* aPrefName);

// Get the various types of preferences. These functions take a dotted
// notation of the preference name (e.g. "browser.startup.homepage")
//
// They also take a pointer to fill in with the return value and return an
// error value. At the moment, this is simply an int but it may
// be converted to an enum once the global error strategy is worked out.
//
// They will perform conversion if the type doesn't match what was requested.
// (if it is reasonably possible)
nsresult
PREF_GetIntPref(const char* aPref, int32_t* aValueOut, bool aGetDefault);
nsresult
PREF_GetBoolPref(const char* aPref, bool* aValueOut, bool aGetDefault);

// These functions are similar to the above "Get" version with the significant
// difference that the preference module will alloc the memory (e.g. XP_STRDUP)
// and the caller will need to be responsible for freeing it...
nsresult
PREF_CopyCharPref(const char* aPref, char** aValueOut, bool aGetDefault);

// Bool function that returns whether or not the preference is locked and
// therefore cannot be changed.
bool
PREF_PrefIsLocked(const char* aPrefName);

// Function that sets whether or not the preference is locked and therefore
// cannot be changed.
nsresult
PREF_LockPref(const char* aKey, bool aLockIt);

PrefType
PREF_GetPrefType(const char* aPrefName);

// Delete a branch of the tree.
nsresult
PREF_DeleteBranch(const char* aBranchName);

// Clears the given pref (reverts it to its default value).
nsresult
PREF_ClearUserPref(const char* aPrefName);

// Clears all user prefs.
nsresult
PREF_ClearAllUserPrefs();

// The callback function will get passed the pref_node which triggered the call
// and the void* instance_data which was passed to the registered callback
// function. Return a non-zero result (nsresult) to pass an error up to the
// caller.
//
// Temporarily conditionally compile PrefChangedFunc typedef. During migration
// from old libpref to nsIPref we need it in both header files. Eventually
// prefapi.h will become a private file. The two types need to be in sync for
// now. Certain compilers were having problems with multiple definitions.
#ifndef have_PrefChangedFunc_typedef
typedef void (*PrefChangedFunc)(const char*, void*);
#define have_PrefChangedFunc_typedef
#endif

// Register a callback. This takes a node in the preference tree and will call
// the callback function if anything below that node is modified. Unregister
// returns PREF_NOERROR if a callback was found that matched all the
// parameters; otherwise it returns PREF_ERROR.
void
PREF_RegisterPriorityCallback(const char* aPrefNode,
                              PrefChangedFunc aCallback,
                              void* aData);
void
PREF_RegisterCallback(const char* aPrefNode,
                      PrefChangedFunc aCallback,
                      void* aData);
nsresult
PREF_UnregisterCallback(const char* aPrefNode,
                        PrefChangedFunc aCallback,
                        void* aData);

// Used by nsPrefService as the callback function of the prefs parser.
void
PREF_ReaderCallback(void* aClosure,
                    const char* aPref,
                    PrefValue aValue,
                    PrefType aType,
                    bool aIsDefault,
                    bool aIsStickyDefault);

// Callback for whenever we change a preference.
typedef void (*PrefsDirtyFunc)();
void PREF_SetDirtyCallback(PrefsDirtyFunc);

#ifdef __cplusplus
}
#endif

#endif // prefapi_h
