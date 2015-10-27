/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TelemetryCommon_h__
#define TelemetryCommon_h__

#include "nsTHashtable.h"
#include "jsapi.h"

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

#endif // TelemetryCommon_h__
