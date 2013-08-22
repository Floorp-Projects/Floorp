/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "nscore.h"
#include "nsdefs.h"
#include "prlog.h"
#include "nsAutoPtr.h"
#include "nsBaseWidget.h"
#include "nsWindowBase.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWindowDbg.h"
#include "WindowHook.h"
#include "TaskbarWindowPreview.h"
#include "nsIdleService.h"
#ifdef ACCESSIBILITY
#include "mozilla/a11y/Accessible.h"
#endif
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "Units.h"
#include "MetroInput.h"

#include "mozwrlbase.h"

#include <windows.system.h>
#include <windows.ui.core.h>
#include <Windows.ApplicationModel.core.h>
#include <Windows.ApplicationModel.h>
#include <Windows.Applicationmodel.Activation.h>


namespace mozilla {
namespace widget {
namespace winrt {

class FrameworkView;

} } }

class MetroWidget : public nsWindowBase,
                    public mozilla::layers::GeckoContentController
{
  typedef mozilla::widget::WindowHook WindowHook;
  typedef mozilla::widget::TaskbarWindowPreview TaskbarWindowPreview;
  typedef ABI::Windows::UI::Input::IPointerPoint IPointerPoint;
  typedef ABI::Windows::UI::Core::IPointerEventArgs IPointerEventArgs;
  typedef ABI::Windows::UI::Core::IKeyEventArgs IKeyEventArgs;
  typedef ABI::Windows::UI::Core::ICharacterReceivedEventArgs ICharacterReceivedEventArgs;
  typedef mozilla::widget::winrt::FrameworkView FrameworkView;

  static LRESULT CALLBACK
  StaticWindowProcedure(HWND aWnd, UINT aMsg, WPARAM aWParan, LPARAM aLParam);
  LRESULT WindowProcedure(HWND aWnd, UINT aMsg, WPARAM aWParan, LPARAM aLParam);

public:
  MetroWidget();
  virtual ~MetroWidget();

  NS_DECL_ISUPPORTS_INHERITED

  static HWND GetICoreWindowHWND() { return sICoreHwnd; }

  // nsWindowBase
  virtual bool DispatchWindowEvent(nsGUIEvent* aEvent) MOZ_OVERRIDE;
  virtual bool IsTopLevelWidget() MOZ_OVERRIDE { return true; }
  virtual nsWindowBase* GetParentWindowBase(bool aIncludeOwner) MOZ_OVERRIDE { return nullptr; }
  // InitEvent assumes physical coordinates and is used by shared win32 code. Do
  // not hand winrt event coordinates to this routine.
  virtual void InitEvent(nsGUIEvent& aEvent, nsIntPoint* aPoint = nullptr) MOZ_OVERRIDE;

  // nsBaseWidget
  virtual CompositorParent* NewCompositorParent(int aSurfaceWidth, int aSurfaceHeight);

  // nsIWidget interface
  NS_IMETHOD    Create(nsIWidget *aParent,
                       nsNativeWidget aNativeParent,
                       const nsIntRect &aRect,
                       nsDeviceContext *aContext,
                       nsWidgetInitData *aInitData = nullptr);
  NS_IMETHOD    Destroy();
  NS_IMETHOD    SetParent(nsIWidget *aNewParent);
  NS_IMETHOD    Show(bool bState);
  NS_IMETHOD    IsVisible(bool & aState);
  NS_IMETHOD    IsEnabled(bool *aState);
  NS_IMETHOD    GetBounds(nsIntRect &aRect);
  NS_IMETHOD    GetScreenBounds(nsIntRect &aRect);
  NS_IMETHOD    GetClientBounds(nsIntRect &aRect);
  NS_IMETHOD    Invalidate(bool aEraseBackground = false,
                bool aUpdateNCArea = false,
                bool aIncludeChildren = false);
  NS_IMETHOD    Invalidate(const nsIntRect & aRect);
  NS_IMETHOD    DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
  NS_IMETHOD    ConstrainPosition(bool aAllowSlop, int32_t *aX, int32_t *aY);
  NS_IMETHOD    Move(double aX, double aY);
  NS_IMETHOD    Resize(double aWidth, double aHeight, bool aRepaint);
  NS_IMETHOD    Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint);
  NS_IMETHOD    SetFocus(bool aRaise);
  NS_IMETHOD    Enable(bool aState);
  NS_IMETHOD    SetCursor(nsCursor aCursor);
  NS_IMETHOD    SetTitle(const nsAString& aTitle);
  NS_IMETHOD    CaptureRollupEvents(nsIRollupListener * aListener,
                                    bool aDoCapture);
  NS_IMETHOD    ReparentNativeWidget(nsIWidget* aNewParent);
  virtual nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                            int32_t aNativeKeyCode,
                                            uint32_t aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters);
  virtual nsresult SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                              uint32_t aNativeMessage,
                                              uint32_t aModifierFlags);
  virtual nsresult SynthesizeNativeMouseScrollEvent(nsIntPoint aPoint,
                                                    uint32_t aNativeMessage,
                                                    double aDeltaX,
                                                    double aDeltaY,
                                                    double aDeltaZ,
                                                    uint32_t aModifierFlags,
                                                    uint32_t aAdditionalFlags);
  virtual bool  HasPendingInputEvent();
  virtual double GetDefaultScaleInternal();
  float         GetDPI();
  mozilla::LayoutDeviceIntPoint CSSIntPointToLayoutDeviceIntPoint(const mozilla::CSSIntPoint &aCSSPoint);
  void          ChangedDPI();
  virtual bool  IsVisible() const;
  virtual bool  IsEnabled() const;
  // ShouldUseOffMainThreadCompositing is defined in base widget
  virtual bool  ShouldUseOffMainThreadCompositing();
  bool          ShouldUseMainThreadD3D10Manager();
  bool          ShouldUseBasicManager();
  bool          ShouldUseAPZC();
  virtual LayerManager* GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                        LayersBackend aBackendHint = mozilla::layers::LAYERS_NONE,
                                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                        bool* aAllowRetaining = nullptr);
  virtual void GetPreferredCompositorBackends(nsTArray<mozilla::layers::LayersBackend>& aHints) { aHints.AppendElement(mozilla::layers::LAYERS_D3D11); }

  // IME related interfaces
  NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction);
  NS_IMETHOD_(nsIWidget::InputContext) GetInputContext();
  NS_IMETHOD    NotifyIME(NotificationToIME aNotification) MOZ_OVERRIDE;
  NS_IMETHOD    GetToggledKeyState(uint32_t aKeyCode, bool* aLEDState);
  NS_IMETHOD    NotifyIMEOfTextChange(uint32_t aStart,
                                      uint32_t aOldEnd,
                                      uint32_t aNewEnd) MOZ_OVERRIDE;
  virtual nsIMEUpdatePreference GetIMEUpdatePreference() MOZ_OVERRIDE;

  // FrameworkView helpers
  void SizeModeChanged();
  void Activated(bool aActiveated);
  void Paint(const nsIntRegion& aInvalidRegion); 

  MetroWidget* MetroWidget::GetTopLevelWindow(bool aStopOnDialogOrPopup) { return this; }
  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
  virtual void* GetNativeData(uint32_t aDataType);
  virtual void  FreeNativeData(void * data, uint32_t aDataType);
  virtual nsIntPoint WidgetToScreenOffset();

  void UserActivity();

#ifdef ACCESSIBILITY
  mozilla::a11y::Accessible* DispatchAccessibleEvent(uint32_t aEventType);
  mozilla::a11y::Accessible* GetAccessible();
#endif // ACCESSIBILITY

  // needed for current nsIFilePicker
  void PickerOpen();
  void PickerClosed();
  bool DestroyCalled() { return false; }
  void SuppressBlurEvents(bool aSuppress);
  bool BlurEventsSuppressed();

  // needed for nsITaskbarWindowPreview
  bool HasTaskbarIconBeenCreated() { return false; }
  void SetHasTaskbarIconBeenCreated(bool created = true) { }
  already_AddRefed<nsITaskbarWindowPreview> GetTaskbarPreview() { return nullptr; }
  void SetTaskbarPreview(nsITaskbarWindowPreview *preview) { }
  WindowHook& GetWindowHook() { return mWindowHook; }

  void SetView(FrameworkView* aView);
  void FindMetroWindow();
  virtual void SetTransparencyMode(nsTransparencyMode aMode);
  virtual nsTransparencyMode GetTransparencyMode();

  nsresult RequestContentScroll();
  void RequestContentRepaintImplMainThread();

  // GeckoContentController interface impl
  virtual void RequestContentRepaint(const mozilla::layers::FrameMetrics& aFrameMetrics);
  virtual void HandleDoubleTap(const mozilla::CSSIntPoint& aPoint);
  virtual void HandleSingleTap(const mozilla::CSSIntPoint& aPoint);
  virtual void HandleLongTap(const mozilla::CSSIntPoint& aPoint);
  virtual void SendAsyncScrollDOMEvent(mozilla::layers::FrameMetrics::ViewID aScrollId, const mozilla::CSSRect &aContentRect, const mozilla::CSSSize &aScrollableSize);
  virtual void PostDelayedTask(Task* aTask, int aDelayMs);
  virtual void HandlePanBegin();
  virtual void HandlePanEnd();

  void SetMetroInput(mozilla::widget::winrt::MetroInput* aMetroInput)
  {
    mMetroInput = aMetroInput;
  }

protected:
  friend class FrameworkView;

  struct OleInitializeWrapper {
    HRESULT const hr;

    OleInitializeWrapper()
      : hr(::OleInitialize(NULL))
    {
    }

    ~OleInitializeWrapper() {
      if (SUCCEEDED(hr)) {
        ::OleFlushClipboard();
        ::OleUninitialize();
      }
    }
  };

  void SetSubclass();
  void RemoveSubclass();
  nsIWidgetListener* GetPaintListener();

  OleInitializeWrapper mOleInitializeWrapper;
  WindowHook mWindowHook;
  Microsoft::WRL::ComPtr<FrameworkView> mView;
  nsTransparencyMode mTransparencyMode;
  nsIntRegion mInvalidatedRegion;
  nsCOMPtr<nsIdleService> mIdleService;
  HWND mWnd;
  static HWND sICoreHwnd;
  WNDPROC mMetroWndProc;
  bool mTempBasicLayerInUse;
  Microsoft::WRL::ComPtr<mozilla::widget::winrt::MetroInput> mMetroInput;
  mozilla::layers::FrameMetrics mFrameMetrics;

public:
  static nsRefPtr<mozilla::layers::APZCTreeManager> sAPZC;
};
