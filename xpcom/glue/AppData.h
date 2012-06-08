/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AppData_h
#define mozilla_AppData_h

#include "nsXREAppData.h"
#include "nscore.h"
#include "nsStringGlue.h"

namespace mozilla {

// Like nsXREAppData, but releases all strong refs/allocated memory
// in the destructor.
class NS_COM_GLUE ScopedAppData : public nsXREAppData
{
public:
  ScopedAppData() { Zero(); this->size = sizeof(*this); }

  ScopedAppData(const nsXREAppData* aAppData);

  void Zero() { memset(this, 0, sizeof(*this)); }

  ~ScopedAppData();
};

/**
 * Given "str" is holding a string allocated with NS_Alloc, or null:
 * replace the value in "str" with a new value.
 *
 * @param newvalue Null is permitted. The string is cloned with
 *                 NS_strdup
 */
void SetAllocatedString(const char *&str, const char *newvalue);

/**
 * Given "str" is holding a string allocated with NS_Alloc, or null:
 * replace the value in "str" with a new value.
 *
 * @param newvalue If "newvalue" is the empty string, "str" will be set
 *                 to null.
 */
void SetAllocatedString(const char *&str, const nsACString &newvalue);

template<class T>
void SetStrongPtr(T *&ptr, T* newvalue)
{
  NS_IF_RELEASE(ptr);
  ptr = newvalue;
  NS_IF_ADDREF(ptr);
}

} // namespace mozilla

#endif
