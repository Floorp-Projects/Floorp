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

#ifndef __nsCommonWidget_h__
#define __nsCommonWidget_h__

#include "nsBaseWidget.h"
#include "nsGUIEvent.h"
#include <gdk/gdkevents.h>

// make sure that logging is enabled before including prlog.h
#define FORCE_PR_LOG

#include "prlog.h"

extern PRLogModuleInfo *gWidgetLog;
extern PRLogModuleInfo *gWidgetFocusLog;
extern PRLogModuleInfo *gWidgetIMLog;
extern PRLogModuleInfo *gWidgetDrawLog;

#define LOG(args) PR_LOG(gWidgetLog, 4, args)
#define LOGFOCUS(args) PR_LOG(gWidgetFocusLog, 4, args)
#define LOGIM(args) PR_LOG(gWidgetIMLog, 4, args)
#define LOGDRAW(args) PR_LOG(gWidgetDrawLog, 4, args)

class nsCommonWidget : public nsBaseWidget {
public:
    nsCommonWidget();
    virtual ~nsCommonWidget();

    virtual nsIWidget *GetParent(void);

    void CommonCreate(nsIWidget *aParent, PRBool aListenForResizes);

    // event handling code
    void InitButtonEvent(nsMouseEvent &aEvent,
                         GdkEventButton *aGdkEvent);
    void InitMouseScrollEvent(nsMouseScrollEvent &aEvent,
                              GdkEventScroll *aGdkEvent);
    void InitKeyEvent(nsKeyEvent &aEvent, GdkEventKey *aGdkEvent);

    void DispatchGotFocusEvent(void);
    void DispatchLostFocusEvent(void);
    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchResizeEvent(nsRect &aRect, nsEventStatus &aStatus);

    NS_IMETHOD DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus);

    // virtual interfaces for some nsIWidget methods
    virtual void NativeResize(PRInt32 aWidth,
                              PRInt32 aHeight,
                              PRBool  aRepaint) = 0;

    virtual void NativeResize(PRInt32 aX,
                              PRInt32 aY,
                              PRInt32 aWidth,
                              PRInt32 aHeight,
                              PRBool  aRepaint) = 0;

    virtual void NativeShow  (PRBool  aAction) = 0;

    // Some of the nsIWidget methods
    NS_IMETHOD         Show             (PRBool aState);
    NS_IMETHOD         Resize           (PRInt32 aWidth,
                                         PRInt32 aHeight,
                                         PRBool  aRepaint);
    NS_IMETHOD         Resize           (PRInt32 aX,
                                         PRInt32 aY,
                                         PRInt32 aWidth,
                                         PRInt32 aHeight,
                                         PRBool   aRepaint);
    NS_IMETHOD         GetPreferredSize (PRInt32 &aWidth,
                                         PRInt32 &aHeight);
    NS_IMETHOD         SetPreferredSize (PRInt32 aWidth,
                                         PRInt32 aHeight);
    NS_IMETHOD         Enable           (PRBool  aState);
    NS_IMETHOD         IsEnabled        (PRBool *aState);

    // called when we are destroyed
    void OnDestroy(void);

    // called to check and see if a widget's dimensions are sane
    PRBool AreBoundsSane(void);

protected:
    nsCOMPtr<nsIWidget> mParent;
    // Is this a toplevel window?
    PRPackedBool        mIsTopLevel;
    // Has this widget been destroyed yet?
    PRPackedBool        mIsDestroyed;

    // This is a flag that tracks if we need to resize a widget or
    // window before we call |Show| on that widget.
    PRPackedBool        mNeedsResize;
    // Should we send resize events on all resizes?
    PRPackedBool        mListenForResizes;
    // This flag tracks if we're hidden or shown.
    PRPackedBool        mIsShown;
    PRPackedBool        mNeedsShow;
    // is this widget enabled?
    PRBool              mEnabled;
    // has the native window for this been created yet?
    PRBool              mCreated;
    // Has anyone set an x/y location for this widget yet? Toplevels
    // shouldn't be automatically set to 0,0 for first show.
    PRBool              mPlaced;

    // Preferred sizes
    PRUint32            mPreferredWidth;
    PRUint32            mPreferredHeight;
};

#endif /* __nsCommonWidget_h__ */
