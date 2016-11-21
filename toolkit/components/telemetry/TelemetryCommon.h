/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryCommon_h__
#define TelemetryCommon_h__

#include "nsTHashtable.h"
#include "jsapi.h"
#include "nsIScriptError.h"

namespace mozilla {
namespace Telemetry {
namespace Common {

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

} // namespace Common
} // namespace Telemetry
} // namespace mozilla

#endif // TelemetryCommon_h__
