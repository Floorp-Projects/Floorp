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
  mActor(aActor)
{
  // See ChannelDestroyed() in the header
  mActor->mWidget = this;
}

PluginWidgetProxy::~PluginWidgetProxy()
{
  PWLOG("PluginWidgetProxy::~PluginWidgetProxy()\n");
}

NS_IMETHODIMP
PluginWidgetProxy::Create(nsIWidget*        aParent,
                          nsNativeWidget    aNativeParent,
                          const nsIntRect&  aRect,
                          nsDeviceContext*  aContext,
                          nsWidgetInitData* aInitData)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetProxy::Create()\n");

  if (!mActor->SendCreate()) {
    NS_WARNING("failed to create chrome widget, plugins won't paint.");
  }

  BaseCreate(aParent, aRect, aContext, aInitData);

  mBounds = aRect;
  mEnabled = true;
  mVisible = true;

  mActor->SendResize(mBounds);

  return NS_OK;
}

NS_IMETHODIMP
PluginWidgetProxy::SetParent(nsIWidget* aNewParent)
{
  mParent = aNewParent;

  nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
  nsIWidget* parent = GetParent();
  if (parent) {
    parent->RemoveChild(this);
  }
  if (aNewParent) {
    aNewParent->AddChild(this);
  }
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
    mActor->SendShow(false);
    mActor->SendDestroy();
    mActor->mWidget = nullptr;
    mActor->Send__delete__(mActor);
    mActor = nullptr;
  }

  return PuppetWidget::Destroy();
}

NS_IMETHODIMP
PluginWidgetProxy::Show(bool aState)
{
  ENSURE_CHANNEL;
  mActor->SendShow(aState);
  mVisible = aState;
  return NS_OK;
}

NS_IMETHODIMP
PluginWidgetProxy::Invalidate(const nsIntRect& aRect)
{
  ENSURE_CHANNEL;
  mActor->SendInvalidate(aRect);
  return NS_OK;
}

void*
PluginWidgetProxy::GetNativeData(uint32_t aDataType)
{
  if (!mActor) {
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
  uintptr_t value = 0;
  mActor->SendGetNativePluginPort(&value);
  PWLOG("PluginWidgetProxy::GetNativeData %p\n", (void*)value);
  return (void*)value;
}

NS_IMETHODIMP
PluginWidgetProxy::Resize(double aWidth, double aHeight, bool aRepaint)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetProxy::Resize(%0.2f, %0.2f, %d)\n", aWidth, aHeight, aRepaint);
  nsIntRect oldBounds = mBounds;
  mBounds.SizeTo(nsIntSize(NSToIntRound(aWidth), NSToIntRound(aHeight)));
  mActor->SendResize(mBounds);
  if (!oldBounds.IsEqualEdges(mBounds) && mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
  }
  return NS_OK;
}

NS_IMETHODIMP
PluginWidgetProxy::Resize(double aX, double aY, double aWidth,
                          double aHeight, bool aRepaint)
{
  nsresult rv = Move(aX, aY);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return Resize(aWidth, aHeight, aRepaint);
}

NS_IMETHODIMP
PluginWidgetProxy::Move(double aX, double aY)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetProxy::Move(%0.2f, %0.2f)\n", aX, aY);
  mActor->SendMove(aX, aY);
  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowMoved(this, aX, aY);
  }
  return NS_OK;
}

NS_IMETHODIMP
PluginWidgetProxy::SetFocus(bool aRaise)
{
  ENSURE_CHANNEL;
  PWLOG("PluginWidgetProxy::SetFocus(%d)\n", aRaise);
  mActor->SendSetFocus(aRaise);
  return NS_OK;
}

nsresult
PluginWidgetProxy::SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                                       bool aIntersectWithExisting)
{
  ENSURE_CHANNEL;
  mActor->SendSetWindowClipRegion(aRects, aIntersectWithExisting);
  nsBaseWidget::SetWindowClipRegion(aRects, aIntersectWithExisting);
  return NS_OK;
}

}  // namespace widget
}  // namespace mozilla
