/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef nsWindow_h
#define nsWindow_h

#include "InputData.h"
#include "nsBaseWidget.h"
#include "nsRegion.h"
#include "nsIIdleServiceInternal.h"
#include "Units.h"

class ANativeWindowBuffer;

namespace widget {
struct InputContext;
struct InputContextAction;
}

class nsScreenGonk;

class nsWindow : public nsBaseWidget
{
public:
    nsWindow();

    NS_DECL_ISUPPORTS_INHERITED

    static void DoDraw(void);
    static nsEventStatus DispatchKeyInput(mozilla::WidgetKeyboardEvent& aEvent);
    static void DispatchTouchInput(mozilla::MultiTouchInput& aInput);

    NS_IMETHOD Create(nsIWidget *aParent,
                      void *aNativeParent,
                      const nsIntRect &aRect,
                      nsWidgetInitData *aInitData);
    NS_IMETHOD Destroy(void);

    NS_IMETHOD Show(bool aState);
    virtual bool IsVisible() const;
    NS_IMETHOD ConstrainPosition(bool aAllowSlop,
                                 int32_t *aX,
                                 int32_t *aY);
    NS_IMETHOD Move(double aX,
                    double aY);
    NS_IMETHOD Resize(double aWidth,
                      double aHeight,
                      bool  aRepaint);
    NS_IMETHOD Resize(double aX,
                      double aY,
                      double aWidth,
                      double aHeight,
                      bool aRepaint);
    NS_IMETHOD Enable(bool aState);
    virtual bool IsEnabled() const;
    NS_IMETHOD SetFocus(bool aRaise = false);
    NS_IMETHOD ConfigureChildren(const nsTArray<nsIWidget::Configuration>&);
    NS_IMETHOD Invalidate(const nsIntRect &aRect);
    virtual void* GetNativeData(uint32_t aDataType);
    virtual void SetNativeData(uint32_t aDataType, uintptr_t aVal);
    NS_IMETHOD SetTitle(const nsAString& aTitle)
    {
        return NS_OK;
    }
    virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset();
    void DispatchTouchInputViaAPZ(mozilla::MultiTouchInput& aInput);
    void DispatchTouchEventForAPZ(const mozilla::MultiTouchInput& aInput,
                                  const ScrollableLayerGuid& aGuid,
                                  const uint64_t aInputBlockId,
                                  nsEventStatus aApzResponse);
    NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                             nsEventStatus& aStatus);
    virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                                TouchPointerState aPointerState,
                                                nsIntPoint aPointerScreenPoint,
                                                double aPointerPressure,
                                                uint32_t aPointerOrientation,
                                                nsIObserver* aObserver) override;

    NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener,
                                   bool aDoCapture)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent);

    NS_IMETHOD MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen = nullptr) /*override*/;

    virtual mozilla::TemporaryRef<mozilla::gfx::DrawTarget>
        StartRemoteDrawing() override;
    virtual void EndRemoteDrawing() override;

    virtual float GetDPI();
    virtual double GetDefaultScaleInternal();
    virtual mozilla::layers::LayerManager*
        GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                        LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                        bool* aAllowRetaining = nullptr);

    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();

    virtual uint32_t GetGLFrameBufferFormat() override;

    virtual nsIntRect GetNaturalBounds() override;
    virtual bool NeedsPaint();

    virtual Composer2D* GetComposer2D() override;

    void ConfigureAPZControllerThread() override;

protected:
    nsWindow* mParent;
    bool mVisible;
    InputContext mInputContext;
    nsCOMPtr<nsIIdleServiceInternal> mIdleService;
    // If we're using a BasicCompositor, these fields are temporarily
    // set during frame composition.  They wrap the hardware
    // framebuffer.
    mozilla::RefPtr<mozilla::gfx::DrawTarget> mFramebufferTarget;
    ANativeWindowBuffer* mFramebuffer;
    // If we're using a BasicCompositor, this is our window back
    // buffer.  The gralloc framebuffer driver expects us to draw the
    // entire framebuffer on every frame, but gecko expects the
    // windowing system to be tracking buffer updates for invalidated
    // regions.  We get stuck holding that bag.
    //
    // Only accessed on the compositor thread, except during
    // destruction.
    mozilla::RefPtr<mozilla::gfx::DrawTarget> mBackBuffer;

    virtual ~nsWindow();

    void BringToTop();

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();

private:
    // This is used by SynthesizeNativeTouchPoint to maintain state between
    // multiple synthesized points
    nsAutoPtr<mozilla::MultiTouchInput> mSynthesizedTouchInput;

    nsRefPtr<nsScreenGonk> mScreen;
};

#endif /* nsWindow_h */
