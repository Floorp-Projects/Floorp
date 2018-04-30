/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryCommon_h__
#define TelemetryCommon_h__

#include "nsTHashtable.h"
#include "jsapi.h"
#include "nsIScriptError.h"
#include "nsXULAppAPI.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/TelemetryProcessEnums.h"

namespace mozilla {
namespace Telemetry {
namespace Common {

enum class RecordedProcessType : uint32_t {
  Main         = (1 << GeckoProcessType_Default),  // Also known as "parent process"
  Content      = (1 << GeckoProcessType_Content),
  Gpu          = (1 << GeckoProcessType_GPU),
  AllChildren  = 0xFFFFFFFF - 1,  // All the child processes (i.e. content, gpu, ...)
  All          = 0xFFFFFFFF       // All the processes
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(RecordedProcessType);

enum class SupportedProduct : uint32_t {
  Firefox    = (1 << 0),
  Fennec     = (1 << 1),
  Geckoview  = (1 << 2),
  All        = 0xFFFFFFFF       // All the products
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(SupportedProduct);

template<class EntryType>
class AutoHashtable : public nsTHashtable<EntryType>
{
public:
  explicit AutoHashtable(uint32_t initLength =
                         PLDHashTable::kDefaultInitialLength);
  typedef bool (*ReflectEntryFunc)(EntryType *entry, JSContext *cx, JS::Handle<JSObject*> obj);
  bool ReflectIntoJS(ReflectEntryFunc entryFunc, JSContext *cx, JS::Handle<JSObject*> obj);
};

template<class EntryType>
AutoHashtable<EntryType>::AutoHashtable(uint32_t initLength)
  : nsTHashtable<EntryType>(initLength)
{
}

/**
 * Reflect the individual entries of table into JS, usually by defining
 * some property and value of obj.  entryFunc is called for each entry.
 */
template<typename EntryType>
bool
AutoHashtable<EntryType>::ReflectIntoJS(ReflectEntryFunc entryFunc,
                                        JSContext *cx, JS::Handle<JSObject*> obj)
{
  for (auto iter = this->Iter(); !iter.Done(); iter.Next()) {
    if (!entryFunc(iter.Get(), cx, obj)) {
      return false;
    }
  }
  return true;
}

bool IsExpiredVersion(const char* aExpiration);
bool IsInDataset(uint32_t aDataset, uint32_t aContainingDataset);
bool CanRecordDataset(uint32_t aDataset, bool aCanRecordBase, bool aCanRecordExtended);
bool CanRecordInProcess(RecordedProcessType aProcesses, GeckoProcessType aProcess);
bool CanRecordInProcess(RecordedProcessType aProcesses, ProcessID aProcess);
bool CanRecordProduct(SupportedProduct aProducts);

/**
 * Return the number of milliseconds since process start using monotonic
 * timestamps (unaffected by system clock changes).
 *
 * @return NS_OK on success, NS_ERROR_NOT_AVAILABLE if TimeStamp doesn't have the data.
 */
nsresult MsSinceProcessStart(double* aResult);

/**
 * Dumps a log message to the Browser Console using the provided level.
 *
 * @param aLogLevel The level to use when displaying the message in the browser console
 *        (e.g. nsIScriptError::warningFlag, ...).
 * @param aMsg The text message to print to the console.
 */
void LogToBrowserConsole(uint32_t aLogLevel, const nsAString& aMsg);

/**
 * Get the name string for a ProcessID.
 * This is the name we use for the Telemetry payloads.
 */
const char* GetNameForProcessID(ProcessID process);

/**
 * Get the process id give a process name.
 *
 * @param aProcessName - the name of the process.
 * @returns {ProcessID} one value from ProcessID::* or ProcessID::Count if the
 *          name of the process was not found.
 */
ProcessID GetIDForProcessName(const char* aProcessName);

/**
 * Get the GeckoProcessType for a ProcessID.
 * Telemetry distinguishes between more process types than the GeckoProcessType,
 * so the mapping is not direct.
 */
GeckoProcessType GetGeckoProcessType(ProcessID process);

/**
 * Check if the passed telemetry identifier is valid.
 *
 * @param aStr The string identifier.
 * @param aMaxLength The maximum length of the identifier.
 * @param aAllowInfixPeriod Whether or not to allow infix dots.
 * @param aAllowInfixUnderscore Whether or not to allow infix underscores.
 * @returns true if the string validates correctly, false otherwise.
 */
bool
IsValidIdentifierString(const nsACString& aStr, const size_t aMaxLength,
                        const bool aAllowInfixPeriod, const bool aAllowInfixUnderscore);

/**
 * Convert the given UTF8 string to a JavaScript string.  The returned
 * string's contents will be the UTF16 conversion of the given string.
 *
 * @param cx The JS context.
 * @param aStr The UTF8 string.
 * @returns a JavaScript string.
 */
JSString*
ToJSString(JSContext* cx, const nsACString& aStr);

/**
 * Convert the given UTF16 string to a JavaScript string.
 *
 * @param cx The JS context.
 * @param aStr The UTF16 string.
 * @returns a JavaScript string.
 */
JSString*
ToJSString(JSContext* cx, const nsAString& aStr);

/**
 * Set the current product.
 *
 * On Firefox desktop, this method has no effect.
 * On Android it will determine if it is running Fennec or GeckoView
 */
void
SetCurrentProduct();

/**
 * Get an identifier for the current running product.
 *
 * @returns the product identifier
 */
SupportedProduct
GetCurrentProduct();

} // namespace Common
} // namespace Telemetry
} // namespace mozilla

#endif // TelemetryCommon_h__
