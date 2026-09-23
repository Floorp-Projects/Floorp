/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MacWebAppWidget.h"
#include "MacWebAppInput.h"
#include "MacWebAppPresentation.h"
#include "MacWebAppService.h"
#include "Protocol.h"
#include "mozilla/Monitor.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Services.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/layers/NativeLayerRootRemoteMacParent.h"
#include "mozilla/layers/PNativeLayerRemote.h"
#include "mozilla/widget/CocoaCompositorWidget.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "gfxPlatform.h"
#include "nsIObserverService.h"
#include "nsIWidgetListener.h"
#include "nsThreadUtils.h"
#import <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>
#include <algorithm>
#include <cmath>

namespace mozilla::widget {
using namespace layers;
using namespace floorp::shim;

static uint32_t sNextWindowId = 0;

static bool Send(const nsACString& aAppId, MessageType aType,
                 NSDictionary* aPayload) {
  NSData* bytes = EncodePayload(aPayload);
  RefPtr<MacWebAppService> service = MacWebAppService::GetSingleton();
  return bytes && service && service->SendControl(
      aAppId, uint32_t(aType),
      nsDependentCSubstring(static_cast<const char*>(bytes.bytes), bytes.length));
}

NS_IMPL_ISUPPORTS_INHERITED(MacWebAppWidget, nsIWidget, nsIObserver)

MacWebAppWidget::MacWebAppWidget(const nsACString& aAppId) : mAppId(aAppId) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_RELEASE_ASSERT(sNextWindowId != UINT32_MAX);
  mWindowId = ++sNextWindowId;
  mScale = NSScreen.mainScreen.backingScaleFactor ?: 1.0;
}

MacWebAppWidget::~MacWebAppWidget() {
  MOZ_ASSERT(!mObserving && !mRemoteRoot);
  if (mInput) mInput->OnDestroy();
  if (mLayerRoot) {
    mLayerRoot->SetPresentationSink(nullptr);
    mLayerRoot->SetLayers({});
  }
  if (mPresentation) mPresentation->Stop();
}

already_AddRefed<nsIWidget> MacWebAppWidget::CreateMacWebAppWindow() {
  RefPtr<MacWebAppWidget> widget = new MacWebAppWidget(mAppId);
  widget->mScale = mScale;
  return widget.forget();
}

nsresult MacWebAppWidget::Create(nsIWidget* aParent,
    const LayoutDeviceIntRect& aRect, const widget::InitData& aInitData) {
  if (aParent && (!aParent->IsMacWebAppWidget() ||
                  !static_cast<MacWebAppWidget*>(aParent)->GetMacWebAppId().Equals(mAppId))) {
    return NS_ERROR_INVALID_ARG;
  }
  if (aInitData.mWindowType != WindowType::TopLevel &&
      aInitData.mWindowType != WindowType::Dialog &&
      aInitData.mWindowType != WindowType::Popup) return NS_ERROR_NOT_IMPLEMENTED;
  BaseCreate(aParent, aInitData);
  mBounds = aRect;
  const double minimum = mWindowType == WindowType::Popup ? 1.0 : 64.0;
  mBounds.width = std::max(mBounds.width, int32_t(std::lround(minimum * mScale)));
  mBounds.height = std::max(mBounds.height, int32_t(std::lround(minimum * mScale)));
  mLayerRoot = NativeLayerRootCA::CreateForCALayer([CALayer layer]);
  mLayerRoot->SetBackingScale(mScale);
  mInput = new MacWebAppInput(this, [this](NSDictionary* aPayload) {
    if (mOnDestroyCalled || !mCreated) return;
    NSMutableDictionary* payload = [aPayload mutableCopy];
    payload[@"windowId"] = @(mWindowId);
    Send(mAppId, MessageType::EditorState, payload);
  });
  nsCOMPtr<nsIObserverService> observers = services::GetObserverService();
  if (!observers) return NS_ERROR_NOT_AVAILABLE;
  nsresult rv = observers->AddObserver(this, "floorp-web-app-shim-event", false);
  if (NS_FAILED(rv)) return rv;
  mObserving = true;
  if (!CreateNativeWindow()) {
    Destroy();
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

bool MacWebAppWidget::CreateNativeWindow() {
  if (mOnDestroyCalled || mCreated) return mCreated;
  auto* parent = GetParent();
  if (parent && (!parent->IsMacWebAppWidget() || parent->Destroyed() ||
      !static_cast<MacWebAppWidget*>(parent)->mCreated)) return false;
  if (mWindowType == WindowType::Popup && !parent) return false;
  NSString* kind = mWindowType == WindowType::Popup ? @"popup"
      : mWindowType == WindowType::Dialog ? @"dialog" : @"window";
  const double minimum = mWindowType == WindowType::Popup ? 1.0 : 64.0;
  NSString* title = [[NSString alloc] initWithCharacters:
      reinterpret_cast<const unichar*>(mTitle.BeginReading()) length:mTitle.Length()];
  NSMutableDictionary* creation = [@{
      @"windowId": @(mWindowId), @"title": title ?: @"", @"kind": kind,
      @"geometryRequestId": @(mGeometryRequestId),
      @"width": @(std::max(minimum, mBounds.width / mScale)),
      @"height": @(std::max(minimum, mBounds.height / mScale))} mutableCopy];
  if (parent) creation[@"parentWindowId"] = @(static_cast<MacWebAppWidget*>(parent)->mWindowId);
  if (!Send(mAppId, MessageType::CreateWindow, creation)) return false;
  mCreated = true;
  mPresentation = new MacWebAppPresentation(mAppId, mWindowId);
  mLayerRoot->SetPresentationSink(mPresentation);
  return true;
}

void MacWebAppWidget::SuspendNativeWindow() {
  mReconnectPending = mCreated || mReconnectPending;
  if (mCreated) mRestoreFocus = mKey;
  mCreated = false;
  mVisible = false;
  mKey = false;
  if (mLayerRoot) mLayerRoot->SetPresentationSink(nullptr);
  if (mPresentation) mPresentation->Stop();
  mPresentation = nullptr;
  if (mInput) mInput->OnShimDisconnected();
  if (mOnDestroyCalled) return;
  if (mWidgetListener) {
    mWidgetListener->WindowDeactivated();
    if (!mOnDestroyCalled && mWidgetListener) mWidgetListener->OcclusionStateChanged(true);
  }
}

bool MacWebAppWidget::RestoreNativeWindow() {
  if (mOnDestroyCalled || mRestoring) return false;
  if (!mReconnectPending) return mCreated;
  AutoRestore<bool> restoring(mRestoring);
  mRestoring = true;
  if (auto* parent = GetParent()) {
    if (!parent->IsMacWebAppWidget() ||
        !static_cast<MacWebAppWidget*>(parent)->RestoreNativeWindow()) return false;
  }
  if (!CreateNativeWindow()) return false;
  if (mGeometryRequestId == UINT32_MAX) return false;
  ++mGeometryRequestId;
  const double minimum = mWindowType == WindowType::Popup ? 1.0 : 64.0;
  if (!Send(mAppId, MessageType::ConfigureWindow, @{
        @"windowId": @(mWindowId), @"geometryRequestId": @(mGeometryRequestId),
        @"x": @(mBounds.x / mScale), @"y": @(mBounds.y / mScale),
        @"width": @(std::max(minimum, mBounds.width / mScale)),
        @"height": @(std::max(minimum, mBounds.height / mScale)),
        @"visible": @(mRequestedVisible), @"enabled": @(mEnabled),
        @"fullscreen": @(mSizeMode == nsSizeMode_Fullscreen),
        @"minimized": @(mSizeMode == nsSizeMode_Minimized),
        @"maximized": @(mSizeMode == nsSizeMode_Maximized)})) return false;
  mReconnectPending = false;
  mUpdateCursor = true;
  SetCursor(mCursor);
  if (mInput) mInput->UpdateEditorState();
  if (mOnDestroyCalled) return true;
  if (!mLayerRoot->CommitToScreen()) return false;
  if (mRestoreFocus) {
    RefPtr<MacWebAppWidget> self = this;
    NS_DispatchToMainThread(NS_NewRunnableFunction("MacWebAppWidget::RestoreFocus", [self] {
      if (self->mCreated && !self->Destroyed()) {
        Send(self->mAppId, MessageType::ActivateWindow, @{@"windowId": @(self->mWindowId)});
      }
    }));
  }
  return true;
}

void MacWebAppWidget::Destroy() {
  if (mOnDestroyCalled) return;
  mOnDestroyCalled = true;
  RefPtr<MacWebAppWidget> self = this;
  mReconnectPending = false;
  mVisible = false;
  mKey = false;
  if (mObserving) {
    if (nsCOMPtr<nsIObserverService> observers = services::GetObserverService()) {
      observers->RemoveObserver(this, "floorp-web-app-shim-event");
    }
    mObserving = false;
  }
  if (mCreated) {
    mCreated = false;
    Send(mAppId, MessageType::CloseWindow, @{@"windowId": @(mWindowId)});
  }
  if (mInput) mInput->OnDestroy();
  if (mLayerRoot) mLayerRoot->SetPresentationSink(nullptr);
  if (mPresentation) mPresentation->Stop();
  nsIWidget::OnDestroy();
  nsIWidget::Destroy();
  // NativeLayerRootCA requires an empty root before its last reference is lost.
  if (mLayerRoot) mLayerRoot->SetLayers({});
  mInput = nullptr;
  mPresentation = nullptr;
  mLayerRoot = nullptr;
}

void MacWebAppWidget::Show(bool aVisible) {
  mRequestedVisible = aVisible;
  mVisible = mCreated && aVisible;
  if (!mCreated) return;
  Send(mAppId, MessageType::ConfigureWindow,
       @{@"windowId": @(mWindowId), @"visible": @(aVisible)});
}
void MacWebAppWidget::Move(const DesktopPoint& aPoint) {
  UpdateGeometry(DesktopRect(float(aPoint.x), float(aPoint.y),
      mBounds.width / mScale, mBounds.height / mScale));
}
void MacWebAppWidget::Resize(const DesktopSize& aSize, bool) {
  UpdateGeometry(DesktopRect(mBounds.x / mScale, mBounds.y / mScale,
      aSize.width, aSize.height));
}
void MacWebAppWidget::Resize(const DesktopRect& aRect, bool) {
  UpdateGeometry(aRect);
}
void MacWebAppWidget::UpdateGeometry(const DesktopRect& aRect) {
  if (!std::isfinite(aRect.x) || !std::isfinite(aRect.y) ||
      !std::isfinite(aRect.width) || !std::isfinite(aRect.height) ||
      mGeometryRequestId == UINT32_MAX) return;
  const double minimum = mWindowType == WindowType::Popup ? 1.0 : 64.0;
  double x = std::clamp(double(aRect.x), -65536.0, 65536.0);
  double y = std::clamp(double(aRect.y), -65536.0, 65536.0);
  double width = std::clamp(double(aRect.width), minimum, 16384.0);
  double height = std::clamp(double(aRect.height), minimum, 16384.0);
  auto previous = mBounds;
  mBounds = LayoutDeviceIntRect(std::lround(x * mScale), std::lround(y * mScale),
      std::lround(width * mScale), std::lround(height * mScale));
  if (previous == mBounds) return;
  // Gecko's layout/window sizing APIs are synchronous. Publish the requested
  // geometry immediately, and reject older native notifications by generation.
  ++mGeometryRequestId;
  if (mCreated) {
    Send(mAppId, MessageType::ConfigureWindow, @{
        @"windowId": @(mWindowId), @"geometryRequestId": @(mGeometryRequestId),
        @"x": @(x), @"y": @(y), @"width": @(width), @"height": @(height)});
  }
  RefPtr<MacWebAppWidget> self = this;
  if (previous.TopLeft() != mBounds.TopLeft()) NotifyWindowMoved(mBounds.TopLeft());
  if (!mOnDestroyCalled && previous.Size() != mBounds.Size()) NotifySize();
}
void MacWebAppWidget::SetSizeMode(nsSizeMode aMode) {
  mSizeMode = aMode;
  if (!mCreated) return;
  Send(mAppId, MessageType::ConfigureWindow,
       @{@"windowId": @(mWindowId), @"fullscreen": @(aMode == nsSizeMode_Fullscreen),
         @"minimized": @(aMode == nsSizeMode_Minimized), @"maximized": @(aMode == nsSizeMode_Maximized)});
}
nsresult MacWebAppWidget::MakeFullScreen(bool aFullscreen) {
  SetSizeMode(aFullscreen ? nsSizeMode_Fullscreen : nsSizeMode_Normal);
  return NS_OK;
}
void MacWebAppWidget::Enable(bool aEnabled) {
  mEnabled = aEnabled;
  if (!mCreated) return;
  Send(mAppId, MessageType::ConfigureWindow,
       @{@"windowId": @(mWindowId), @"enabled": @(aEnabled)});
}
void MacWebAppWidget::SetFocus(Raise aRaise, dom::CallerType) {
  if (aRaise != Raise::Yes) return;
  if (!mCreated) { mRestoreFocus = true; return; }
  if (!mVisible && mSizeMode != nsSizeMode_Minimized) return;
  Send(mAppId, MessageType::ActivateWindow, @{@"windowId": @(mWindowId)});
}
nsresult MacWebAppWidget::SetTitle(const nsAString& aTitle) {
  mTitle.Assign(aTitle.BeginReading(), std::min<size_t>(aTitle.Length(), 512));
  mTitle.ReplaceChar(char16_t(0), char16_t(0xfffd));
  if (!mCreated) return NS_OK;
  NSString* title = [[NSString alloc] initWithCharacters:
      reinterpret_cast<const unichar*>(mTitle.BeginReading()) length:mTitle.Length()];
  return Send(mAppId, MessageType::SetWindowTitle,
              @{@"windowId": @(mWindowId), @"title": title ?: @""})
             ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}
void MacWebAppWidget::Invalidate(const LayoutDeviceIntRect&) {
  if (mWidgetListener) mWidgetListener->PaintWindow(this);
  if (mAttachedWidgetListener) mAttachedWidgetListener->PaintWindow(this);
}
void MacWebAppWidget::SetCursor(const Cursor& aCursor) {
  if (!mUpdateCursor && mCursor == aCursor) return;
  nsIWidget::SetCursor(aCursor);
  mUpdateCursor = false;
  if (!mCreated) return;
  NSString* name = @"default";
  switch (aCursor.mDefaultCursor) {
    case eCursor_select: name = @"text"; break;
    case eCursor_vertical_text: name = @"vertical-text"; break;
    case eCursor_hyperlink: name = @"pointer"; break;
    case eCursor_crosshair:
    case eCursor_cell: name = @"crosshair"; break;
    case eCursor_grab: name = @"grab"; break;
    case eCursor_grabbing: name = @"grabbing"; break;
    case eCursor_move:
    case eCursor_all_scroll: name = @"move"; break;
    case eCursor_copy: name = @"copy"; break;
    case eCursor_alias: name = @"alias"; break;
    case eCursor_context_menu: name = @"context-menu"; break;
    case eCursor_not_allowed:
    case eCursor_no_drop: name = @"not-allowed"; break;
    case eCursor_w_resize:
    case eCursor_e_resize:
    case eCursor_col_resize:
    case eCursor_ew_resize: name = @"ew-resize"; break;
    case eCursor_n_resize:
    case eCursor_s_resize:
    case eCursor_row_resize:
    case eCursor_ns_resize: name = @"ns-resize"; break;
    case eCursor_nw_resize:
    case eCursor_se_resize:
    case eCursor_nwse_resize: name = @"nwse-resize"; break;
    case eCursor_ne_resize:
    case eCursor_sw_resize:
    case eCursor_nesw_resize: name = @"nesw-resize"; break;
    case eCursor_zoom_in: name = @"zoom-in"; break;
    case eCursor_zoom_out: name = @"zoom-out"; break;
    case eCursor_none: name = @"none"; break;
    default: break;
  }
  Send(mAppId, MessageType::SetCursor,
       @{@"windowId": @(mWindowId), @"cursor": name});
}
void MacWebAppWidget::SetInputContext(const InputContext& aContext,
                                     const InputContextAction&) {
  mInputContext = aContext;
  if (mInput) mInput->UpdateEditorState();
}
TextEventDispatcherListener* MacWebAppWidget::GetNativeTextEventDispatcherListener() {
  return mInput.get();
}
bool MacWebAppWidget::GetEditCommands(NativeKeyBindingsType aType,
    const WidgetKeyboardEvent& aEvent, nsTArray<CommandInt>& aCommands) {
  return mInput && mInput->GetEditCommands(aType, aEvent, aCommands);
}
void MacWebAppWidget::NotifySize() {
  if (mCompositorDelegate) mCompositorDelegate->NotifyClientSizeChanged(mBounds.Size());
  if (mWidgetListener) mWidgetListener->WindowResized(this, mBounds.Size());
  if (mAttachedWidgetListener) mAttachedWidgetListener->WindowResized(this, mBounds.Size());
}

NS_IMETHODIMP MacWebAppWidget::Observe(nsISupports*, const char* aTopic,
                                      const char16_t* aData) {
  if (mOnDestroyCalled || !aData || strcmp(aTopic, "floorp-web-app-shim-event")) return NS_OK;
  NS_ConvertUTF16toUTF8 json(aData);
  NSDictionary* event = DecodePayload([NSData dataWithBytes:json.get() length:json.Length()]);
  NSString* appId = ReadString(event, @"appId", 256);
  if (!appId || !mAppId.Equals(appId.UTF8String)) return NS_OK;
  RefPtr<MacWebAppWidget> self = this;
  if ([event[@"type"] isEqual:@"disconnected"]) {
    SuspendNativeWindow();
    return NS_OK;
  }
  if ([event[@"type"] isEqual:@"connected"]) {
    if (mReconnectPending && !RestoreNativeWindow() && !mOnDestroyCalled) {
      if (nsCOMPtr<nsIObserverService> observers = services::GetObserverService()) {
        NS_ConvertUTF8toUTF16 appId(mAppId);
        observers->NotifyObservers(nullptr, "floorp-web-app-presentation-failed", appId.get());
      }
    }
    return NS_OK;
  }
  if (!mCreated) return NS_OK;
  uint32_t type, windowId;
  NSDictionary* payload = event[@"payload"];
  if (![payload isKindOfClass:NSDictionary.class] || !ReadUInt(event, @"type", &type) ||
      !ReadUInt(payload, @"windowId", &windowId) || windowId != mWindowId) return NS_OK;
  if (type == uint32_t(MessageType::Input)) {
    if (mEnabled && mInput) mInput->Handle(payload);
  } else if (type == uint32_t(MessageType::SurfaceReleased)) {
    uint32_t surface;
    if (mPresentation && ReadUInt(payload, @"surfaceId", &surface)) mPresentation->SurfaceReleased(surface);
  } else if (type == uint32_t(MessageType::FramePresented)) {
    uint32_t frame;
    if (mPresentation && ReadUInt(payload, @"frameId", &frame)) mPresentation->FramePresented(frame);
  } else if (type == uint32_t(MessageType::WindowChanged)) {
    NSString* kind = ReadString(payload, @"kind", 64);
    if ([kind isEqual:@"closeRequested"]) {
      if (mWidgetListener) mWidgetListener->RequestWindowClose(this);
      return NS_OK;
    }
    double width, height, scale, x, y;
    if (!ReadNumber(payload, @"width", &width, 1, 16384) ||
        !ReadNumber(payload, @"height", &height, 1, 16384) ||
        !ReadNumber(payload, @"scale", &scale, 0.5, 8)) return NS_OK;
    auto previous = mBounds;
    double previousScale = mScale;
    uint32_t geometryRequestId = 0;
    if (payload[@"geometryRequestId"] &&
        !ReadUInt(payload, @"geometryRequestId", &geometryRequestId, true)) return NS_OK;
    if (geometryRequestId >= mGeometryRequestId) {
      mScale = scale;
      mLayerRoot->SetBackingScale(scale);
      mBounds.width = std::lround(width * scale);
      mBounds.height = std::lround(height * scale);
      if (ReadNumber(payload, @"x", &x, -65536, 65536) &&
          ReadNumber(payload, @"y", &y, -65536, 65536)) {
        mBounds.x = std::lround(x * scale);
        mBounds.y = std::lround(y * scale);
      }
    }
    mVisible = [payload[@"visible"] boolValue];
    nsSizeMode mode = [payload[@"fullscreen"] boolValue] ? nsSizeMode_Fullscreen
        : [payload[@"minimized"] boolValue] ? nsSizeMode_Minimized
        : [payload[@"maximized"] boolValue] ? nsSizeMode_Maximized : nsSizeMode_Normal;
    if (mSizeMode != mode) {
      mSizeMode = mode;
      if (mWidgetListener) mWidgetListener->SizeModeChanged(mode);
    }
    if (mWidgetListener) {
      mWidgetListener->OcclusionStateChanged([payload[@"occluded"] boolValue]);
    }
    bool key = [payload[@"key"] boolValue];
    if (key != mKey && mWidgetListener) {
      mKey = key;
      if (key) mWidgetListener->WindowActivated();
      else mWidgetListener->WindowDeactivated();
    }
    if (previous.TopLeft() != mBounds.TopLeft()) NotifyWindowMoved(mBounds.TopLeft());
    if (!mOnDestroyCalled &&
        (previous.Size() != mBounds.Size() || previousScale != mScale)) NotifySize();
    if (mInput) mInput->UpdateEditorState();
  }
  return NS_OK;
}

void MacWebAppWidget::CreateCompositor(int aWidth, int aHeight) {
  (void)gfxPlatform::GetPlatform();
  nsIWidget::CreateCompositor(aWidth, aHeight);
}
void MacWebAppWidget::GetCompositorWidgetInitData(CompositorWidgetInitData* aData) {
  if (RefPtr<NativeLayerRootRemoteMacParent> actor = std::move(mRemoteRoot)) {
    CompositorThread()->Dispatch(NewRunnableMethod("MacWebAppWidget::CloseRemoteRoot",
        actor, &NativeLayerRootRemoteMacParent::Close));
  }
  auto info = gfx::GPUProcessManager::Get()->GPUEndpointProcInfo();
  if (info == ipc::EndpointProcInfo::Invalid()) info = ipc::EndpointProcInfo::Current();
  ipc::Endpoint<PNativeLayerRemoteParent> parent;
  ipc::Endpoint<PNativeLayerRemoteChild> child;
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(PNativeLayerRemote::CreateEndpoints(
      ipc::EndpointProcInfo::Current(), info, &parent, &child)));
  auto actor = MakeRefPtr<NativeLayerRootRemoteMacParent>(mLayerRoot);
  Monitor monitor("MacWebAppWidget::BindRemoteRoot");
  bool bound = false;
  CompositorThread()->Dispatch(NS_NewRunnableFunction("MacWebAppWidget::BindRemoteRoot", [&] {
    MOZ_ALWAYS_TRUE(parent.Bind(actor));
    MonitorAutoLock lock(monitor);
    bound = true;
    lock.Notify();
  }));
  {
    MonitorAutoLock lock(monitor);
    while (!bound) lock.Wait();
  }
  *aData = CocoaCompositorWidgetInitData(mBounds.Size(), std::move(child));
  mRemoteRoot = std::move(actor);
}
void MacWebAppWidget::DestroyCompositor() {
  if (RefPtr<NativeLayerRootRemoteMacParent> actor = std::move(mRemoteRoot)) {
    RefPtr<NativeLayerRootCA> root = mLayerRoot;
    CompositorThread()->Dispatch(NS_NewRunnableFunction(
        "MacWebAppWidget::CloseRemoteRoot", [actor, root] {
          actor->Close();
          // A queued remote commit can run after main-thread teardown. Keep the
          // root alive until the actor is closed, then clear its final layers.
          root->SetLayers({});
        }));
  }
  nsIWidget::DestroyCompositor();
}
void MacWebAppWidget::SetCompositorWidgetDelegate(CompositorWidgetDelegate* aDelegate) {
  mCompositorDelegate = aDelegate ? aDelegate->AsPlatformSpecificDelegate() : nullptr;
}
}  // namespace mozilla::widget
