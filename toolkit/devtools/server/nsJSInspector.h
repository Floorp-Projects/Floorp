/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMPONENTS_JSINSPECTOR_H
#define COMPONENTS_JSINSPECTOR_H

#include "nsIJSInspector.h"
#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "js/Value.h"
#include "js/RootingAPI.h"

namespace mozilla {
namespace jsinspector {

class nsJSInspector MOZ_FINAL : public nsIJSInspector
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsJSInspector)
  NS_DECL_NSIJSINSPECTOR

  nsJSInspector();

private:
  ~nsJSInspector();

  uint32_t mNestedLoopLevel;
  nsTArray<JS::Heap<JS::Value> > mRequestors;
  JS::Heap<JS::Value> mLastRequestor;
};

}
}

#endif
