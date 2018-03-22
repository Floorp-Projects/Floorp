/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShimInterfaceInfo.h"

#include "nsIDOMCustomEvent.h"
#ifdef MOZ_WEBRTC
#include "nsIDOMDataChannel.h"
#endif
#include "nsIDOMDOMCursor.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMDOMRequest.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMGeoPositionError.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNotifyPaintEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMOfflineResourceList.h"
#include "nsIDOMParser.h"
#include "nsIDOMRange.h"
#include "nsIDOMScreen.h"
#include "nsIDOMSerializer.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMXMLDocument.h"
#include "nsIDOMXULElement.h"
#include "nsIListBoxObject.h"
#include "nsIMessageManager.h"
#include "nsISelection.h"
#include "nsITreeBoxObject.h"
#include "nsIWebBrowserPersistable.h"

#include "mozilla/dom/CSSPrimitiveValueBinding.h"
#include "mozilla/dom/CSSStyleDeclarationBinding.h"
#include "mozilla/dom/CSSStyleSheetBinding.h"
#include "mozilla/dom/CSSValueBinding.h"
#include "mozilla/dom/CSSValueListBinding.h"
#include "mozilla/dom/CustomEventBinding.h"
#include "mozilla/dom/DOMCursorBinding.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/DOMParserBinding.h"
#include "mozilla/dom/DOMRequestBinding.h"
#include "mozilla/dom/DocumentBinding.h"
#include "mozilla/dom/DocumentFragmentBinding.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/FrameLoaderBinding.h"
#include "mozilla/dom/HTMLAnchorElementBinding.h"
#include "mozilla/dom/HTMLAreaElementBinding.h"
#include "mozilla/dom/HTMLButtonElementBinding.h"
#include "mozilla/dom/HTMLFrameSetElementBinding.h"
#include "mozilla/dom/HTMLHtmlElementBinding.h"
#include "mozilla/dom/HTMLInputElementBinding.h"
#include "mozilla/dom/ListBoxObjectBinding.h"
#include "mozilla/dom/MediaListBinding.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/NodeListBinding.h"
#include "mozilla/dom/NodeBinding.h"
#include "mozilla/dom/NotifyPaintEventBinding.h"
#include "mozilla/dom/EventBinding.h"
#include "mozilla/dom/OfflineResourceListBinding.h"
#include "mozilla/dom/PositionErrorBinding.h"
#include "mozilla/dom/RangeBinding.h"
#ifdef MOZ_WEBRTC
#include "mozilla/dom/RTCDataChannelBinding.h"
#endif
#include "mozilla/dom/ScreenBinding.h"
#include "mozilla/dom/SelectionBinding.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/StyleSheetBinding.h"
#include "mozilla/dom/StyleSheetListBinding.h"
#include "mozilla/dom/SVGElementBinding.h"
#include "mozilla/dom/TimeEventBinding.h"
#include "mozilla/dom/TreeBoxObjectBinding.h"
#include "mozilla/dom/UIEventBinding.h"
#include "mozilla/dom/XMLDocumentBinding.h"
#include "mozilla/dom/XMLSerializerBinding.h"
#include "mozilla/dom/XULDocumentBinding.h"
#include "mozilla/dom/XULElementBinding.h"

using namespace mozilla;

struct ComponentsInterfaceShimEntry {
  constexpr
  ComponentsInterfaceShimEntry(const char* aName, const nsIID& aIID,
                               const dom::NativePropertyHooks* aNativePropHooks)
    : geckoName(aName), iid(aIID), nativePropHooks(aNativePropHooks) {}

  const char *geckoName;
  const nsIID& iid;
  const dom::NativePropertyHooks* nativePropHooks;
};

#define DEFINE_SHIM_WITH_CUSTOM_INTERFACE(geckoName, domName) \
  { #geckoName, NS_GET_IID(geckoName), \
     mozilla::dom::domName ## Binding::sNativePropertyHooks }
#define DEFINE_SHIM(name) \
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOM ## name, name)

/**
 * These shim entries allow us to make old XPIDL interfaces implementing DOM
 * APIs as non-scriptable in order to save some runtime memory on Firefox OS,
 * without breaking the entries under Components.interfaces which might both
 * be used by our code and add-ons.  Specifically, the shim entries provide
 * the following:
 *
 * * Components.interfaces.nsIFoo entries.  These entries basically work
 *   almost exactly as the usual ones that you would get through the
 *   XPIDL machinery.  Specifically, they have the right name, they reflect
 *   the right IID, and they will work properly when passed to QueryInterface.
 *
 * * Components.interfaces.nsIFoo.CONSTANT values.  These entries will have
 *   the right name and the right value for most integer types.  Note that
 *   support for non-numerical constants is untested and will probably not
 *   work out of the box.
 *
 * FAQ:
 * * When should I add an entry to the list here?
 *   Only if you're making an XPIDL interfaces which has a corresponding
 *   WebIDL interface non-scriptable.
 * * When should I remove an entry from this list?
 *   If you are completely removing an XPIDL interface from the code base.  If
 *   you forget to do so, the compiler will remind you.
 * * How should I add an entry to the list here?
 *   First, make sure that the XPIDL interface in question is non-scriptable
 *   and also has a corresponding WebIDL interface.  Then, add two include
 *   entries above, one for the XPIDL interface and one for the WebIDL
 *   interface, and add a shim entry below.  If the name of the XPIDL
 *   interface only has an "nsIDOM" prefix prepended to the WebIDL name, you
 *   can use the DEFINE_SHIM macro and pass in the name of the WebIDL
 *   interface.  Otherwise, use DEFINE_SHIM_WITH_CUSTOM_INTERFACE.
 */

const ComponentsInterfaceShimEntry kComponentsInterfaceShimMap[] =
{
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIContentFrameMessageManager, ContentFrameMessageManager),
  DEFINE_SHIM(CustomEvent),
  DEFINE_SHIM(DOMCursor),
  DEFINE_SHIM(DOMException),
  DEFINE_SHIM(DOMRequest),
  DEFINE_SHIM(Document),
  DEFINE_SHIM(DocumentFragment),
  DEFINE_SHIM(Element),
  DEFINE_SHIM(Event),
  DEFINE_SHIM(EventTarget),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMGeoPositionError, PositionError),
  DEFINE_SHIM(HTMLInputElement),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIListBoxObject, ListBoxObject),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIMessageSender, MessageSender),
  DEFINE_SHIM(NodeList),
  DEFINE_SHIM(Node),
  DEFINE_SHIM(NotifyPaintEvent),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMNSEvent, Event),
  DEFINE_SHIM(OfflineResourceList),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMParser, DOMParser),
  DEFINE_SHIM(Range),
#ifdef MOZ_WEBRTC
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMDataChannel, RTCDataChannel),
#endif
  DEFINE_SHIM(Screen),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIDOMSerializer, XMLSerializer),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsITreeBoxObject, TreeBoxObject),
  DEFINE_SHIM(UIEvent),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsIWebBrowserPersistable, FrameLoader),
  DEFINE_SHIM(XMLDocument),
  DEFINE_SHIM(XULElement),
  DEFINE_SHIM_WITH_CUSTOM_INTERFACE(nsISelection, Selection),
};

#undef DEFINE_SHIM
#undef DEFINE_SHIM_WITH_CUSTOM_INTERFACE

NS_IMPL_ISUPPORTS(ShimInterfaceInfo, nsISupports, nsIInterfaceInfo)

already_AddRefed<ShimInterfaceInfo>
ShimInterfaceInfo::MaybeConstruct(const char* aName, JSContext* cx)
{
    RefPtr<ShimInterfaceInfo> info;
    for (uint32_t i = 0; i < ArrayLength(kComponentsInterfaceShimMap); ++i) {
        if (!strcmp(aName, kComponentsInterfaceShimMap[i].geckoName)) {
            const ComponentsInterfaceShimEntry& shimEntry =
                kComponentsInterfaceShimMap[i];
            info = new ShimInterfaceInfo(shimEntry.iid,
                                         shimEntry.geckoName,
                                         shimEntry.nativePropHooks);
            break;
        }
    }
    return info.forget();
}

ShimInterfaceInfo::ShimInterfaceInfo(const nsIID& aIID,
                                     const char* aName,
                                     const mozilla::dom::NativePropertyHooks* aNativePropHooks)
    : mIID(aIID)
    , mName(aName)
    , mNativePropHooks(aNativePropHooks)
{
}

NS_IMETHODIMP
ShimInterfaceInfo::GetName(char** aName)
{
    *aName = ToNewCString(mName);
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetInterfaceIID(nsIID** aIID)
{
    *aIID = mIID.Clone();
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::IsScriptable(bool* aRetVal)
{
    // This class should pretend that the interface is scriptable because
    // that's what nsJSIID assumes.
    *aRetVal = true;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::IsBuiltinClass(bool* aRetVal)
{
    *aRetVal = true;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::IsMainProcessScriptableOnly(bool* aRetVal)
{
    *aRetVal = false;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetParent(nsIInterfaceInfo** aParent)
{
    *aParent = nullptr;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetMethodCount(uint16_t* aCount)
{
    // Pretend we don't have any methods.
    *aCount = 0;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetConstantCount(uint16_t* aCount)
{
    // We assume that we never have interfaces with more than UINT16_MAX
    // constants defined on them.
    uint16_t count = 0;

    // NOTE: The structure of this loop must be kept in sync with the loop
    // in GetConstant.
    const mozilla::dom::NativePropertyHooks* propHooks = mNativePropHooks;
    do {
        const mozilla::dom::NativeProperties* props[] = {
            propHooks->mNativeProperties.regular,
            propHooks->mNativeProperties.chromeOnly
        };
        for (size_t i = 0; i < ArrayLength(props); ++i) {
            auto prop = props[i];
            if (prop && prop->HasConstants()) {
                for (auto cs = prop->Constants()->specs; cs->name; ++cs) {
                    // We have found one constant here.  We explicitly do not
                    // bother calling isEnabled() here because it's OK to define
                    // potentially extra constants on these shim interfaces.
                    ++count;
                }
            }
        }
    } while ((propHooks = propHooks->mProtoHooks));
    *aCount = count;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetMethodInfo(uint16_t aIndex, const nsXPTMethodInfo** aInfo)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetMethodInfoForName(const char* aName, uint16_t* aIndex, const nsXPTMethodInfo** aInfo)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetConstant(uint16_t aIndex, JS::MutableHandleValue aConstant,
                               char** aName)
{
    // We assume that we never have interfaces with more than UINT16_MAX
    // constants defined on them.
    uint16_t index = 0;

    // NOTE: The structure of this loop must be kept in sync with the loop
    // in GetConstantCount.
    const mozilla::dom::NativePropertyHooks* propHooks = mNativePropHooks;
    do {
        const mozilla::dom::NativeProperties* props[] = {
            propHooks->mNativeProperties.regular,
            propHooks->mNativeProperties.chromeOnly
        };
        for (size_t i = 0; i < ArrayLength(props); ++i) {
            auto prop = props[i];
            if (prop && prop->HasConstants()) {
                for (auto cs = prop->Constants()->specs; cs->name; ++cs) {
                    // We have found one constant here.  We explicitly do not
                    // bother calling isEnabled() here because it's OK to define
                    // potentially extra constants on these shim interfaces.
                    if (index == aIndex) {
                        aConstant.set(cs->value);
                        *aName = ToNewCString(nsDependentCString(cs->name));
                        return NS_OK;
                    }
                    ++index;
                }
            }
        }
    } while ((propHooks = propHooks->mProtoHooks));

    // aIndex was bigger than the number of constants we have.
    return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetInfoForParam(uint16_t aIndex, const nsXPTParamInfo* aParam, nsIInterfaceInfo** aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetIIDForParam(uint16_t aIndex, const nsXPTParamInfo* aParam, nsIID** aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetTypeForParam(uint16_t aInex, const nsXPTParamInfo* aParam, uint16_t aDimension, nsXPTType* aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetSizeIsArgNumberForParam(uint16_t aInex, const nsXPTParamInfo* aParam, uint16_t aDimension, uint8_t* aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetInterfaceIsArgNumberForParam(uint16_t aInex, const nsXPTParamInfo* aParam, uint8_t* aRetVal)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ShimInterfaceInfo::IsIID(const nsIID* aIID, bool* aRetVal)
{
    *aRetVal = mIID.Equals(*aIID);
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetNameShared(const char** aName)
{
    *aName = mName.get();
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::GetIIDShared(const nsIID** aIID)
{
    *aIID = &mIID;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::IsFunction(bool* aRetVal)
{
    *aRetVal = false;
    return NS_OK;
}

NS_IMETHODIMP
ShimInterfaceInfo::HasAncestor(const nsIID* aIID, bool* aRetVal)
{
    *aRetVal = false;
    return NS_OK;
}

NS_IMETHODIMP_(nsresult)
ShimInterfaceInfo::GetIIDForParamNoAlloc(uint16_t aIndex, const nsXPTParamInfo* aInfo, nsIID* aIID)
{
    MOZ_ASSERT(false, "This should never be called");
    return NS_ERROR_NOT_IMPLEMENTED;
}
