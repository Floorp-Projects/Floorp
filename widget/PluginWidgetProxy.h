/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_RemotePlugin_h__
#define mozilla_widget_RemotePlugin_h__

#include "PuppetWidget.h"
#include "mozilla/dom/TabChild.h"

/*
 * PluginWidgetProxy is a nsIWidget wrapper we hand around in plugin and layout
 * code. It wraps a native widget it creates in the chrome process. Since this
 * is for plugins, only a limited set of the widget apis need to be overridden,
 * the rest of the implementation is in PuppetWidget or nsBaseWidget.
 */

namespace mozilla {
namespace plugins {
class PluginWidgetChild;
} // namespace plugins
namespace widget {

class PluginWidgetProxy final : public PuppetWidget
{
public:
  explicit PluginWidgetProxy(dom::TabChild* aTabChild,
                             mozilla::plugins::PluginWidgetChild* aChannel);

protected:
  virtual ~PluginWidgetProxy();

public:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIWidget
  using PuppetWidget::Create; // for Create signature not overridden here
  virtual MOZ_MUST_USE nsresult Create(nsIWidget* aParent,
                                       nsNativeWidget aNativeParent,
                                       const LayoutDeviceIntRect& aRect,
                                       nsWidgetInitData* aInitData = nullptr)
                                       override;
  virtual void Destroy() override;
  virtual nsresult SetFocus(bool aRaise = false) override;
  virtual void SetParent(nsIWidget* aNewParent) override;

  virtual nsIWidget* GetParent(void) override;
  virtual void* GetNativeData(uint32_t aDataType) override;
#if defined(XP_WIN)
  void SetNativeData(uint32_t aDataType, uintptr_t aVal) override;
#endif
  virtual nsTransparencyMode GetTransparencyMode() override
  { return eTransparencyOpaque; }
  virtual void GetWindowClipRegion(nsTArray<LayoutDeviceIntRect>* aRects) override;

public:
  /**
   * When tabs are closed PPluginWidget can terminate before plugin code is
   * finished tearing us down. When this happens plugin calls over mActor
   * fail triggering an abort in the content process. To protect against this
   * the connection tells us when it is torn down here so we can avoid making
   * calls while content finishes tearing us down.
   */
  void ChannelDestroyed() { mActor = nullptr; }

private:
  // Our connection with the chrome widget, created on PBrowser.
  mozilla::plugins::PluginWidgetChild* mActor;
  // PuppetWidget does not implement parent apis, but we need
  // them for plugin widgets.
  nsCOMPtr<nsIWidget> mParent;
  uintptr_t mCachedPluginPort;
};

} // namespace widget
} // namespace mozilla

#endif
