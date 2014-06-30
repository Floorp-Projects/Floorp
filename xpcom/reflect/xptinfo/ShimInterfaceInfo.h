/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ShimInterfaceInfo_h
#define ShimInterfaceInfo_h

#include "mozilla/Attributes.h"
#include "nsIInterfaceInfo.h"
#include "nsString.h"
#include "nsID.h"
#include "nsTArray.h"
#include "xptinfo.h"
#include "nsAutoPtr.h"
#include "js/RootingAPI.h"

namespace mozilla {
namespace dom {
struct ConstantSpec;
struct NativePropertyHooks;
}
}

class ShimInterfaceInfo MOZ_FINAL : public nsIInterfaceInfo
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFO

    // Construct a ShimInterfaceInfo object if we have a shim available for aName.
    // Otherwise, returns nullptr.
    static already_AddRefed<ShimInterfaceInfo>
    MaybeConstruct(const char* aName, JSContext* cx);

private:
    ShimInterfaceInfo(const nsIID& aIID,
                      const char* aName,
                      const mozilla::dom::NativePropertyHooks* aNativePropHooks);

    ~ShimInterfaceInfo() {}

private:
    nsIID mIID;
    nsAutoCString mName;
    const mozilla::dom::NativePropertyHooks* mNativePropHooks;
};

#endif
