/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code mozilla.org code.
 *
 * The Initial Developer of the Original Code Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsBaseWidget.h"
#include "nsIScrollbar.h"

#include <gtk/gtkwidget.h>

#include "nsCommonWidget.h"

class nsScrollbar : public nsCommonWidget,
                    public nsIScrollbar {
public:
    nsScrollbar(PRBool aIsVertical);
    virtual ~nsScrollbar();

    NS_DECL_ISUPPORTS_INHERITED

    // nsIWidget
    NS_IMETHOD Create(nsIWidget        *aParent,
                      const nsRect     &aRect,
                      EVENT_CALLBACK   aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell      *aAppShell = nsnull,
                      nsIToolkit       *aToolkit = nsnull,
                      nsWidgetInitData *aInitData = nsnull);
    NS_IMETHOD Create(nsNativeWidget aParent,
                      const nsRect     &aRect,
                      EVENT_CALLBACK   aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell      *aAppShell = nsnull,
                      nsIToolkit       *aToolkit = nsnull,
                      nsWidgetInitData *aInitData = nsnull);
    NS_IMETHOD Destroy(void);
    NS_IMETHOD IsVisible(PRBool & aState);
    NS_IMETHOD ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY);
    NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD Enable(PRBool aState);
    NS_IMETHOD SetFocus(PRBool aRaise = PR_FALSE);
    virtual nsIFontMetrics* GetFont(void);
    NS_IMETHOD SetFont(const nsFont &aFont);
    NS_IMETHOD Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD Invalidate(const nsRect & aRect, PRBool aIsSynchronous);
    NS_IMETHOD Update();
    NS_IMETHOD SetColorMap(nsColorMap *aColorMap);
    NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    virtual void* GetNativeData(PRUint32 aDataType);
    NS_IMETHOD SetTitle(const nsString& aTitle);
    NS_IMETHOD SetMenuBar(nsIMenuBar * aMenuBar);
    NS_IMETHOD ShowMenuBar(PRBool aShow);
    NS_IMETHOD WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD BeginResizingChildren(void);
    NS_IMETHOD EndResizingChildren(void);
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener * aListener,
                                   PRBool aDoCapture,
                                   PRBool aConsumeRollupEvent);

    // nsIScrollbar
    NS_IMETHOD SetMaxRange(PRUint32 aEndRange);
    NS_IMETHOD GetMaxRange(PRUint32& aMaxRange);
    NS_IMETHOD SetPosition(PRUint32 aPos);
    NS_IMETHOD GetPosition(PRUint32& aPos);
    NS_IMETHOD SetThumbSize(PRUint32 aSize);
    NS_IMETHOD GetThumbSize(PRUint32& aSize);
    NS_IMETHOD SetLineIncrement(PRUint32 aSize);
    NS_IMETHOD GetLineIncrement(PRUint32& aSize);
    NS_IMETHOD SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                             PRUint32 aPosition, PRUint32 aLineIncrement);

    // from nsBaseWidget
    NS_IMETHOD         SetBounds        (const nsRect &aRect);

    nsresult NativeCreate(nsIWidget        *aParent,
                          nsNativeWidget    aNativeParent,
                          const nsRect     &aRect,
                          EVENT_CALLBACK    aHandleEventFunction,
                          nsIDeviceContext *aContext,
                          nsIAppShell      *aAppShell,
                          nsIToolkit       *aToolkit,
                          nsWidgetInitData *aInitData);

    // common widget
    void NativeResize(PRInt32 aWidth, PRInt32 aHeight, PRBool  aRepaint);

    void NativeResize(PRInt32 aX, PRInt32 aY,
                      PRInt32 aWidth, PRInt32 aHeight,
                      PRBool  aRepaint);

    void NativeShow  (PRBool  aAction);

    // Callbacks
    void OnValueChanged(void);

private:
    GtkWidget      *mWidget;
    GtkOrientation  mOrientation;
    GtkAdjustment  *mAdjustment;

    // We track these seperately because sometimes their values might
    // be different than the values stored in the adjustment.
    PRUint32 mMaxRange;
    PRUint32 mThumbSize;

    // Update the adjustment taking into account differences between
    // the adjustment's method and mozilla's method for measuing
    // infinity.
    void UpdateAdjustment(void);
};
