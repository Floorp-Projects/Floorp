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

#include "nsBaseWidget.h"
#include "nsIIdleServiceInternal.h"

extern nsIntRect gScreenBounds;

namespace mozilla {
namespace gl {
class GLContext;
}
namespace layers {
class LayersManager;
}
}

namespace android {
class FramebufferNativeWindow;
}

namespace widget {
struct InputContext;
struct InputContextAction;
}

class nsWindow : public nsBaseWidget
{
public:
    nsWindow();
    virtual ~nsWindow();

    static void DoDraw(void);
    static nsEventStatus DispatchInputEvent(nsGUIEvent &aEvent);

    NS_IMETHOD Create(nsIWidget *aParent,
                      void *aNativeParent,
                      const nsIntRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsDeviceContext *aContext,
                      nsWidgetInitData *aInitData);
    NS_IMETHOD Destroy(void);

    NS_IMETHOD Show(bool aState);
    virtual bool IsVisible() const;
    NS_IMETHOD ConstrainPosition(bool aAllowSlop,
                                 PRInt32 *aX,
                                 PRInt32 *aY);
    NS_IMETHOD Move(PRInt32 aX,
                    PRInt32 aY);
    NS_IMETHOD Resize(PRInt32 aWidth,
                      PRInt32 aHeight,
                      bool  aRepaint);
    NS_IMETHOD Resize(PRInt32 aX,
                      PRInt32 aY,
                      PRInt32 aWidth,
                      PRInt32 aHeight,
                      bool aRepaint);
    NS_IMETHOD Enable(bool aState);
    virtual bool IsEnabled() const;
    NS_IMETHOD SetFocus(bool aRaise = false);
    NS_IMETHOD ConfigureChildren(const nsTArray<nsIWidget::Configuration>&);
    NS_IMETHOD Invalidate(const nsIntRect &aRect);
    virtual void* GetNativeData(PRUint32 aDataType);
    NS_IMETHOD SetTitle(const nsAString& aTitle)
    {
        return NS_OK;
    }
    virtual nsIntPoint WidgetToScreenOffset();
    NS_IMETHOD DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus);
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener,
                                   bool aDoCapture,
                                   bool aConsumeRollupEvent)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent);

    virtual float GetDPI();
    virtual mozilla::layers::LayerManager*
        GetLayerManager(PLayersChild* aShadowManager = nsnull,
                        LayersBackend aBackendHint = mozilla::layers::LAYERS_NONE,
                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                        bool* aAllowRetaining = nsnull);
    gfxASurface* GetThebesSurface();

    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();

    virtual PRUint32 GetGLFrameBufferFormat() MOZ_OVERRIDE;

    virtual nsIntRect GetNaturalBounds() MOZ_OVERRIDE;

protected:
    nsWindow* mParent;
    bool mVisible;
    nsIntRegion mDirtyRegion;
    InputContext mInputContext;
    nsCOMPtr<nsIIdleServiceInternal> mIdleService;

    void BringToTop();

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();
};

#endif /* nsWindow_h */
