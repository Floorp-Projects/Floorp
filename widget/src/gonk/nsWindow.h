/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Gonk.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsWindow_h
#define nsWindow_h

#include "nsBaseWidget.h"

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
    NS_IMETHOD IsVisible(bool & aState);
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
    NS_IMETHOD IsEnabled(bool *aState);
    NS_IMETHOD SetFocus(bool aRaise = false);
    NS_IMETHOD ConfigureChildren(const nsTArray<nsIWidget::Configuration>&);
    NS_IMETHOD Invalidate(const nsIntRect &aRect,
                          bool aIsSynchronous);
    NS_IMETHOD Update();
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
                        LayersBackend aBackendHint = LayerManager::LAYERS_NONE,
                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                        bool* aAllowRetaining = nsnull);
    gfxASurface* GetThebesSurface();

protected:
    nsWindow* mParent;
    bool mVisible;

    void BringToTop();
};

#endif /* nsWindow_h */
