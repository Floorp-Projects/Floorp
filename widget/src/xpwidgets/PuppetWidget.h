/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

/**
 * This "puppet widget" isn't really a platform widget.  It's intended
 * to be used in widgetless rendering contexts, such as sandboxed
 * content processes.  If any "real" widgetry is needed, the request
 * is forwarded to and/or data received from elsewhere.
 */

#ifndef mozilla_widget_PuppetWidget_h__
#define mozilla_widget_PuppetWidget_h__

#include "nsBaseWidget.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"

class gfxASurface;

namespace mozilla {
namespace widget {

class PuppetWidget : public nsBaseWidget, public nsSupportsWeakReference
{
  typedef nsBaseWidget Base;

  // The width and height of the "widget" are clamped to this.
  static const size_t kMaxDimension;

public:
  PuppetWidget();
  virtual ~PuppetWidget();

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD Create(nsIWidget*        aParent,
                    nsNativeWidget    aNativeParent,
                    const nsIntRect&  aRect,
                    EVENT_CALLBACK    aHandleEventFunction,
                    nsIDeviceContext* aContext,
                    nsIAppShell*      aAppShell = nsnull,
                    nsIToolkit*       aToolkit = nsnull,
                    nsWidgetInitData* aInitData = nsnull);

  virtual already_AddRefed<nsIWidget>
  CreateChild(const nsIntRect  &aRect,
              EVENT_CALLBACK   aHandleEventFunction,
              nsIDeviceContext *aContext,
              nsIAppShell      *aAppShell = nsnull,
              nsIToolkit       *aToolkit = nsnull,
              nsWidgetInitData *aInitData = nsnull,
              PRBool           aForceUseIWidgetParent = PR_FALSE);

  NS_IMETHOD Destroy();

  NS_IMETHOD Show(PRBool aState);
  NS_IMETHOD IsVisible(PRBool& aState)
  { aState = mVisible; return NS_OK; }

  NS_IMETHOD ConstrainPosition(PRBool   /*ignored aAllowSlop*/,
                               PRInt32* aX,
                               PRInt32* aY)
  { *aX = kMaxDimension;  *aY = kMaxDimension;  return NS_OK; }

  // We're always at <0, 0>, and so ignore move requests.
  NS_IMETHOD Move(PRInt32 aX, PRInt32 aY)
  { return NS_OK; }

  NS_IMETHOD Resize(PRInt32 aWidth,
                    PRInt32 aHeight,
                    PRBool  aRepaint);
  NS_IMETHOD Resize(PRInt32 aX,
                    PRInt32 aY,
                    PRInt32 aWidth,
                    PRInt32 aHeight,
                    PRBool  aRepaint)
  // (we're always at <0, 0>)
  { return Resize(aWidth, aHeight, aRepaint); }

  // XXX/cjones: copying gtk behavior here; unclear what disabling a
  // widget is supposed to entail
  NS_IMETHOD Enable(PRBool aState)
  { mEnabled = aState;  return NS_OK; }
  NS_IMETHOD IsEnabled(PRBool *aState)
  { *aState = mEnabled;  return NS_OK; }

  NS_IMETHOD SetFocus(PRBool aRaise = PR_FALSE);

  // PuppetWidgets don't care about children.
  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
  { return NS_OK; }

  NS_IMETHOD Invalidate(const nsIntRect& aRect, PRBool aIsSynchronous);

  NS_IMETHOD Update();

  // This API is going away, steer clear.
  virtual void Scroll(const nsIntPoint& aDelta,
                      const nsTArray<nsIntRect>& aDestRects,
                      const nsTArray<Configuration>& aReconfigureChildren)
  { /* dead man walking */ }

  // PuppetWidgets don't have native data, as they're purely nonnative.
  virtual void* GetNativeData(PRUint32 aDataType)
  { return nsnull; }
  NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent)
  { return NS_ERROR_UNEXPECTED; }

  // PuppetWidgets don't have any concept of titles. 
  NS_IMETHOD SetTitle(const nsAString& aTitle)
  { return NS_ERROR_UNEXPECTED; }
  
  // PuppetWidgets are always at <0, 0>.
  virtual nsIntPoint WidgetToScreenOffset()
  { return nsIntPoint(0, 0); }

  NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus);

  NS_IMETHOD CaptureRollupEvents(nsIRollupListener* aListener, nsIMenuRollup* aMenuRollup,
                                 PRBool aDoCapture, PRBool aConsumeRollupEvent)
  { return NS_ERROR_UNEXPECTED; }

  //
  // nsBaseWidget methods we override
  //

//NS_IMETHOD              CaptureMouse(PRBool aCapture);
  virtual LayerManager*     GetLayerManager();
//  virtual nsIDeviceContext* GetDeviceContext();
  virtual gfxASurface*      GetThebesSurface();

private:
  nsresult DispatchPaintEvent();
  nsresult DispatchResizeEvent();

  void SetChild(PuppetWidget* aChild);

  class PaintTask : public nsRunnable {
  public:
    NS_DECL_NSIRUNNABLE
    PaintTask(PuppetWidget* widget) : mWidget(widget) {}
    void Revoke() { mWidget = nsnull; }
  private:
    PuppetWidget* mWidget;
  };

  // The "widget" to which we delegate events if we don't have an
  // event handler.
  nsRefPtr<PuppetWidget> mChild;
  nsIntRegion mDirtyRegion;
  nsRevocableEventPtr<PaintTask> mPaintTask;
  PRPackedBool mEnabled;
  PRPackedBool mVisible;
  // XXX/cjones: keeping this around until we teach LayerManager to do
  // retained-content-only transactions
  nsRefPtr<gfxASurface> mSurface;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_PuppetWidget_h__
