/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
// <pre>
*/
#ifndef PREFAPI_H
#define PREFAPI_H

#include "nscore.h"
#include "PLDHashTable.h"

#ifdef __cplusplus
extern "C" {
#endif

// 1 MB should be enough for everyone.
static const uint32_t MAX_PREF_LENGTH = 1 * 1024 * 1024;
// Actually, 4kb should be enough for everyone.
static const uint32_t MAX_ADVISABLE_PREF_LENGTH = 4 * 1024;

typedef union
{
    char*       stringVal;
    int32_t     intVal;
    bool        boolVal;
} PrefValue;

/*
// <font color=blue>
// The Init function initializes the preference context and creates
// the preference hashtable.
// </font>
*/
void        PREF_Init();

/*
// Cleanup should be called at program exit to free the
// list of registered callbacks.
*/
void        PREF_Cleanup();
void        PREF_CleanupPrefs();

/*
// <font color=blue>
// Preference flags, including the native type of the preference. Changing any of these
// values will require modifying the code inside of PrefTypeFlags class.
// </font>
*/

enum class PrefType {
  Invalid = 0,
  String = 1,
  Int = 2,
  Bool = 3,
};

// Keep the type of the preference, as well as the flags guiding its behaviour.
class PrefTypeFlags
{
public:
  PrefTypeFlags() : mValue(AsInt(PrefType::Invalid)) {}
  explicit PrefTypeFlags(PrefType aType) : mValue(AsInt(aType)) {}
  PrefTypeFlags& Reset() { mValue = AsInt(PrefType::Invalid); return *this; }

  bool IsTypeValid() const { return !IsPrefType(PrefType::Invalid); }
  bool IsTypeString() const { return IsPrefType(PrefType::String); }
  bool IsTypeInt() const { return IsPrefType(PrefType::Int); }
  bool IsTypeBool() const { return IsPrefType(PrefType::Bool); }
  bool IsPrefType(PrefType type) const { return GetPrefType() == type; }

  PrefTypeFlags& SetPrefType(PrefType aType) {
    mValue = mValue - AsInt(GetPrefType()) + AsInt(aType);
    return *this;
  }
  PrefType GetPrefType() const {
    return (PrefType)(mValue & (AsInt(PrefType::String) |
                                AsInt(PrefType::Int) |
                                AsInt(PrefType::Bool)));
  }

  bool HasDefault() const { return mValue & PREF_FLAG_HAS_DEFAULT; }
  PrefTypeFlags& SetHasDefault(bool aSetOrUnset) { return SetFlag(PREF_FLAG_HAS_DEFAULT, aSetOrUnset); }

  bool HasStickyDefault() const { return mValue & PREF_FLAG_STICKY_DEFAULT; }
  PrefTypeFlags& SetHasStickyDefault(bool aSetOrUnset) { return SetFlag(PREF_FLAG_STICKY_DEFAULT, aSetOrUnset); }

  bool IsLocked() const { return mValue & PREF_FLAG_LOCKED; }
  PrefTypeFlags& SetLocked(bool aSetOrUnset) { return SetFlag(PREF_FLAG_LOCKED, aSetOrUnset); }

  bool HasUserValue() const { return mValue & PREF_FLAG_USERSET; }
  PrefTypeFlags& SetHasUserValue(bool aSetOrUnset) { return SetFlag(PREF_FLAG_USERSET, aSetOrUnset); }

private:
  static uint16_t AsInt(PrefType aType) { return (uint16_t)aType; }

  PrefTypeFlags& SetFlag(uint16_t aFlag, bool aSetOrUnset) {
    mValue = aSetOrUnset ? mValue | aFlag : mValue & ~aFlag;
    return *this;
  }

  // Pack both the value of type (PrefType) and flags into the same int.  This is why
  // the flag enum starts at 4, as PrefType occupies the bottom two bits.
  enum {
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
    PrefTypeFlags prefFlags; // This field goes first to minimize struct size on 64-bit.
    const char *key;
    PrefValue defaultPref;
    PrefValue userPref;
};

/*
// <font color=blue>
// Set the various types of preferences.  These functions take a dotted
// notation of the preference name (e.g. "browser.startup.homepage").
// Note that this will cause the preference to be saved to the file if
// it is different from the default.  In other words, these are used
// to set the _user_ preferences.
//
// If set_default is set to true however, it sets the default value.
// This will only affect the program behavior if the user does not have a value
// saved over it for the particular preference.  In addition, these will never
// be saved out to disk.
//
// Each set returns PREF_VALUECHANGED if the user value changed
// (triggering a callback), or PREF_NOERROR if the value was unchanged.
// </font>
*/
nsresult PREF_SetCharPref(const char *pref,const char* value, bool set_default = false);
nsresult PREF_SetIntPref(const char *pref,int32_t value, bool set_default = false);
nsresult PREF_SetBoolPref(const char *pref,bool value, bool set_default = false);

bool     PREF_HasUserPref(const char* pref_name);

/*
// <font color=blue>
// Get the various types of preferences.  These functions take a dotted
// notation of the preference name (e.g. "browser.startup.homepage")
//
// They also take a pointer to fill in with the return value and return an
// error value.  At the moment, this is simply an int but it may
// be converted to an enum once the global error strategy is worked out.
//
// They will perform conversion if the type doesn't match what was requested.
// (if it is reasonably possible)
// </font>
*/
nsresult PREF_GetIntPref(const char *pref,
                           int32_t * return_int, bool get_default);
nsresult PREF_GetBoolPref(const char *pref, bool * return_val, bool get_default);
/*
// <font color=blue>
// These functions are similar to the above "Get" version with the significant
// difference that the preference module will alloc the memory (e.g. XP_STRDUP) and
// the caller will need to be responsible for freeing it...
// </font>
*/
nsresult PREF_CopyCharPref(const char *pref, char ** return_buf, bool get_default);
/*
// <font color=blue>
// bool function that returns whether or not the preference is locked and therefore
// cannot be changed.
// </font>
*/
bool PREF_PrefIsLocked(const char *pref_name);

/*
// <font color=blue>
// Function that sets whether or not the preference is locked and therefore
// cannot be changed.
// </font>
*/
nsresult PREF_LockPref(const char *key, bool lockIt);

PrefType PREF_GetPrefType(const char *pref_name);

/*
 * Delete a branch of the tree
 */
nsresult PREF_DeleteBranch(const char *branch_name);

/*
 * Clears the given pref (reverts it to its default value)
 */
nsresult PREF_ClearUserPref(const char *pref_name);

/*
 * Clears all user prefs
 */
nsresult PREF_ClearAllUserPrefs();


/*
// <font color=blue>
// The callback function will get passed the pref_node which triggered the call
// and the void * instance_data which was passed to the register callback function.
// Return a non-zero result (nsresult) to pass an error up to the caller.
// </font>
*/
/* Temporarily conditionally compile PrefChangedFunc typedef.
** During migration from old libpref to nsIPref we need it in
** both header files.  Eventually prefapi.h will become a private
** file.  The two types need to be in sync for now.  Certain
** compilers were having problems with multiple definitions.
*/
#ifndef have_PrefChangedFunc_typedef
typedef void (*PrefChangedFunc) (const char *, void *);
#define have_PrefChangedFunc_typedef
#endif

/*
// <font color=blue>
// Register a callback.  This takes a node in the preference tree and will
// call the callback function if anything below that node is modified.
// Unregister returns PREF_NOERROR if a callback was found that
// matched all the parameters; otherwise it returns PREF_ERROR.
// </font>
*/
void PREF_RegisterCallback(const char* domain,
                           PrefChangedFunc callback, void* instance_data );
nsresult PREF_UnregisterCallback(const char* domain,
                                 PrefChangedFunc callback, void* instance_data );

/*
 * Used by nsPrefService as the callback function of the 'pref' parser
 */
void PREF_ReaderCallback( void *closure,
                          const char *pref,
                          PrefValue   value,
                          PrefType    type,
                          bool        isDefault,
                          bool        isStickyDefault);


/*
 * Callback whenever we change a preference
 */
typedef void (*PrefsDirtyFunc) ();
void PREF_SetDirtyCallback(PrefsDirtyFunc);

#ifdef __cplusplus
}
#endif
#endif
