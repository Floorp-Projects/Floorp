/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_INativePlacesEventCallback_h
#define mozilla_image_INativePlacesEventCallback_h

#include "mozilla/dom/PlacesObserversBinding.h"
#include "mozilla/WeakPtr.h"
#include "nsISupports.h"
#include "nsTArray.h"

namespace mozilla {
namespace places {

class INativePlacesEventCallback : public SupportsWeakPtr<INativePlacesEventCallback>
{
public:
  typedef dom::Sequence<OwningNonNull<dom::PlacesEvent>> PlacesEventSequence;

  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(INativePlacesEventCallback)
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void HandlePlacesEvent(const PlacesEventSequence& aEvents) = 0;

protected:
  virtual ~INativePlacesEventCallback() { }
};

} // namespace places
} // namespace mozilla

#endif // mozilla_image_INativePlacesEventCallback_h
