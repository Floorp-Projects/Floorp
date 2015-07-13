/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PluginWidgetProxy.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/plugins/PluginWidgetChild.h"
#include "nsDebug.h"

#define PWLOG(...)
// #define PWLOG(...) printf_stderr(__VA_ARGS__)

/* static */
already_AddRefed<nsIWidget>
nsIWidget::CreatePluginProxyWidget(TabChild* aTabChild,
                                   mozilla::plugins::PluginWidgetChild* aActor)
{
  nsCOMPtr<nsIWidget> widget =
    new mozilla::widget::PluginWidgetProxy(aTabChild, aActor);
  return widget.forget();
}

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS_INHERITED(PluginWidgetProxy, PuppetWidget, nsIWidget)

#define ENSURE_CHANNEL do {                                   \
  if (!mActor) {                                              \
    NS_WARNING("called on an invalid channel.");              \
    return NS_ERROR_FAILURE;                                  \
  }                                                           \
} while (0)

PluginWidgetProxy::PluginWidgetProxy(dom::TabChild* aTabChild,
                                     mozilla::plugins::PluginWidgetChild* aActor) :
  PuppetWidget(aTabChild),
  mActor(aActor),
  mCachedPluginPort(0)
{
  // See ChannelDestroyed() in the header
  mActor->SetWidget(this);
}

PluginWidgetProxy::~PluginWidgetProxy()
{
  PWLOG("PluginWidgetProxy::~PluginWidgetProxy()\n");
}

NS_IMETHODIMP
PluginWidgetProxy::Create(nsIWidget*        aParent,
                          nsNativeWidget    aNativeParent,
                          const nsIntRect&  aRect,
                          nsWidgetInitData* aInitData)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetProxy::Create()\n");

  nsresult rv = NS_ERROR_UNEXPECTED;
  mActor->SendCreate(&rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to create chrome widget, plugins won't paint.");
    return rv;
  }

  BaseCreate(aParent, aRect, aInitData);

  mBounds = aRect;
  mEnabled = true;
  mVisible = true;

  return NS_OK;
}

NS_IMETHODIMP
PluginWidgetProxy::SetParent(nsIWidget* aNewParent)
{
  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  nsIWidget* parent = GetParent();
  if (parent) {
    parent->RemoveChild(this);
  }
  if (aNewParent) {
    aNewParent->AddChild(this);
  }
  mParent = aNewParent;
  return NS_OK;
}

nsIWidget*
PluginWidgetProxy::GetParent(void)
{
  return mParent.get();
}

NS_IMETHODIMP
PluginWidgetProxy::Destroy()
{
  PWLOG("PluginWidgetProxy::Destroy()\n");

  if (mActor) {
    // Communicate that the layout widget has been torn down before the sub
    // protocol.
    mActor->ProxyShutdown();
    mActor = nullptr;
  }

  return PuppetWidget::Destroy();
}

void
PluginWidgetProxy::GetWindowClipRegion(nsTArray<nsIntRect>* aRects)
{
  if (mClipRects && mClipRectCount) {
    aRects->AppendElements(mClipRects.get(), mClipRectCount);
  }
}

void*
PluginWidgetProxy::GetNativeData(uint32_t aDataType)
{
  if (!mActor) {
    return nullptr;
  }
  auto tab = static_cast<mozilla::dom::TabChild*>(mActor->Manager());
  if (tab && tab->IsDestroyed()) {
    return nullptr;
  }
  switch (aDataType) {
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_SHAREABLE_WINDOW:
      break;
    default:
      NS_WARNING("PluginWidgetProxy::GetNativeData received request for unsupported data type.");
      return nullptr;
  }
  // The parent side window handle or xid never changes so we can
  // cache this for our lifetime.
  if (mCachedPluginPort) {
    return (void*)mCachedPluginPort;
  }
  mActor->SendGetNativePluginPort(&mCachedPluginPort);
  PWLOG("PluginWidgetProxy::GetNativeData %p\n", (void*)mCachedPluginPort);
  return (void*)mCachedPluginPort;
}

#if defined(XP_WIN)
void
PluginWidgetProxy::SetNativeData(uint32_t aDataType, uintptr_t aVal)
{
  if (!mActor) {
    return;
  }

  auto tab = static_cast<mozilla::dom::TabChild*>(mActor->Manager());
  if (tab && tab->IsDestroyed()) {
    return;
  }

  switch (aDataType) {
    case NS_NATIVE_CHILD_WINDOW:
      mActor->SendSetNativeChildWindow(aVal);
      break;
    default:
      NS_ERROR("SetNativeData called with unsupported data type.");
  }
}
#endif

NS_IMETHODIMP
PluginWidgetProxy::SetFocus(bool aRaise)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetProxy::SetFocus(%d)\n", aRaise);
  mActor->SendSetFocus(aRaise);
  return NS_OK;
}

} // namespace widget
} // namespace mozilla
