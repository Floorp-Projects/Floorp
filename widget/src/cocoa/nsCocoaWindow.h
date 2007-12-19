/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *   Colin Barrett <cbarrett@mozilla.com>
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

#ifndef nsCocoaWindow_h_
#define nsCocoaWindow_h_

#undef DARWIN

#import <Cocoa/Cocoa.h>

#include "nsBaseWidget.h"
#include "nsPIWidgetCocoa.h"

class nsCocoaWindow;
class nsChildView;


@interface NSApplication (Undocumented)

// It's sometimes necessary to explicitly remove a window from the "window
// cache" in order to deactivate it.  The "window cache" is an undocumented
// subsystem, all of whose methods are included in the NSWindowCache category
// of the NSApplication class (in header files generated using class-dump).
- (void)_removeWindowFromCache:(NSWindow *)aWindow;

@end


@interface NSWindow (Undocumented)

// If a window has been explicitly removed from the "window cache" (to
// deactivate it), it's sometimes necessary to "reset" it to reactivate it
// (and put it back in the "window cache").  One way to do this, which Apple
// often uses, is to set the "window number" to '-1' and then back to its
// original value.
- (void)_setWindowNumber:(int)aNumber;

// If we set the window's stylemask to be textured, the corners on the bottom of
// the window are rounded by default. We use this private method to make
// the corners square again, a la Safari.
- (void)setBottomCornerRounded:(BOOL)rounded;

@end


@interface PopupWindow : NSWindow
{
@private
  BOOL mIsContextMenu;
}

- (id)initWithContentRect:(NSRect)contentRect styleMask:(unsigned int)styleMask
      backing:(NSBackingStoreType)bufferingType defer:(BOOL)deferCreation;
- (BOOL)isContextMenu;
- (void)setIsContextMenu:(BOOL)flag;

@end


@interface BorderlessWindow : NSWindow
{
}

- (BOOL)canBecomeKeyWindow;
- (BOOL)canBecomeMainWindow;

@end


@interface WindowDelegate : NSObject
{
  nsCocoaWindow* mGeckoWindow; // [WEAK] (we are owned by the window)
}
- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind;
- (void)windowDidResize:(NSNotification*)aNotification;
- (void)sendFocusEvent:(PRUint32)eventType;
- (nsCocoaWindow*)geckoWidget;
@end


// NSColor subclass that allows us to draw separate colors both in the titlebar 
// and for background of the window.
@interface TitlebarAndBackgroundColor : NSColor
{
  NSColor *mTitlebarColor;
  NSColor *mBackgroundColor;
  NSWindow *mWindow; // [WEAK] (we are owned by the window)
  float mTitlebarHeight;
}

- (id)initWithTitlebarColor:(NSColor*)aTitlebarColor 
         andBackgroundColor:(NSColor*)aBackgroundColor
                  forWindow:(NSWindow*)aWindow;

// Pass nil here to get the default appearance.
- (void)setTitlebarColor:(NSColor*)aColor;
- (NSColor*)titlebarColor;

- (void)setBackgroundColor:(NSColor*)aColor;
- (NSColor*)backgroundColor;

- (NSWindow*)window;
- (float)titlebarHeight;
@end

// NSWindow subclass for handling windows with toolbars.
@interface ToolbarWindow : NSWindow
{
  TitlebarAndBackgroundColor *mColor;
}
- (void)setTitlebarColor:(NSColor*)aColor;
- (NSColor*)titlebarColor;
// This method is also available on NSWindows (via a category), and is the 
// preferred way to check the background color of a window.
- (NSColor*)windowBackgroundColor;
@end

class nsCocoaWindow : public nsBaseWidget, public nsPIWidgetCocoa
{
private:
  
  typedef nsBaseWidget Inherited;

public:

    nsCocoaWindow();
    virtual ~nsCocoaWindow();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSPIWIDGETCOCOA
      
    NS_IMETHOD              Create(nsNativeWidget aParent,
                                   const nsRect &aRect,
                                   EVENT_CALLBACK aHandleEventFunction,
                                   nsIDeviceContext *aContext,
                                   nsIAppShell *aAppShell = nsnull,
                                   nsIToolkit *aToolkit = nsnull,
                                   nsWidgetInitData *aInitData = nsnull);

    NS_IMETHOD              Create(nsIWidget* aParent,
                                   const nsRect &aRect,
                                   EVENT_CALLBACK aHandleEventFunction,
                                   nsIDeviceContext *aContext,
                                   nsIAppShell *aAppShell = nsnull,
                                   nsIToolkit *aToolkit = nsnull,
                                   nsWidgetInitData *aInitData = nsnull);

    NS_IMETHOD              Destroy();
     // Utility method for implementing both Create(nsIWidget ...) and
     // Create(nsNativeWidget...)

    virtual nsresult        StandardCreate(nsIWidget *aParent,
                                    const nsRect &aRect,
                                    EVENT_CALLBACK aHandleEventFunction,
                                    nsIDeviceContext *aContext,
                                    nsIAppShell *aAppShell,
                                    nsIToolkit *aToolkit,
                                    nsWidgetInitData *aInitData,
                                    nsNativeWidget aNativeWindow = nsnull);

    NS_IMETHOD              Show(PRBool aState);
    NS_IMETHOD              AddMouseListener(nsIMouseListener * aListener);
    NS_IMETHOD              AddEventListener(nsIEventListener * aListener);
    NS_IMETHOD              Enable(PRBool aState);
    NS_IMETHOD              IsEnabled(PRBool *aState);
    NS_IMETHOD              SetModal(PRBool aState);
    NS_IMETHOD              IsVisible(PRBool & aState);
    NS_IMETHOD              SetFocus(PRBool aState=PR_FALSE);
    NS_IMETHOD              SetMenuBar(nsIMenuBar * aMenuBar);
    virtual nsIMenuBar*     GetMenuBar();
    NS_IMETHOD              ShowMenuBar(PRBool aShow);
    NS_IMETHOD WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    
    virtual void* GetNativeData(PRUint32 aDataType) ;

    NS_IMETHOD              ConstrainPosition(PRBool aAllowSlop,
                                              PRInt32 *aX, PRInt32 *aY);
    NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                        nsIWidget *aWidget, PRBool aActivate);
    NS_IMETHOD              SetSizeMode(PRInt32 aMode);

    NS_IMETHOD              Resize(PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD              GetScreenBounds(nsRect &aRect);
    virtual PRBool          OnPaint(nsPaintEvent &event);
    void                    ReportSizeEvent(NSRect *overrideRect = nsnull);

    NS_IMETHOD              SetTitle(const nsAString& aTitle);

    NS_IMETHOD Invalidate(const nsRect & aRect, PRBool aIsSynchronous);
    NS_IMETHOD Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD Update();
    NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *alCipRect) { return NS_OK; }
    NS_IMETHOD SetColorMap(nsColorMap *aColorMap) { return NS_OK; }
    NS_IMETHOD BeginResizingChildren(void) { return NS_OK; }
    NS_IMETHOD EndResizingChildren(void) { return NS_OK; }
    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight) { return NS_OK; }
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight) { return NS_OK; }
    NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus) ;
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent);
    NS_IMETHOD GetAttention(PRInt32 aCycleCount);
    NS_IMETHOD GetHasTransparentBackground(PRBool& aTransparent);
    NS_IMETHOD SetHasTransparentBackground(PRBool aTransparent);
    NS_IMETHOD SetWindowTitlebarColor(nscolor aColor);

    virtual gfxASurface* GetThebesSurface();

    // be notified that a some form of drag event needs to go into Gecko
    virtual PRBool DragEvent(unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers);

    // Helpers to prevent recursive resizing during live-resize
    PRBool IsResizing () const { return mIsResizing; }
    void StartResizing () { mIsResizing = PR_TRUE; }
    void StopResizing () { mIsResizing = PR_FALSE; }
    
    // nsIKBStateControl interface
    NS_IMETHOD ResetInputState();
    
    void MakeBackgroundTransparent(PRBool aTransparent);

    NS_IMETHOD BeginSecureKeyboardInput();
    NS_IMETHOD EndSecureKeyboardInput();

protected:
  
  nsIWidget*           mParent;         // if we're a popup, this is our parent [WEAK]
  NSWindow*            mWindow;         // our cocoa window [STRONG]
  WindowDelegate*      mDelegate;       // our delegate for processing window msgs [STRONG]
  nsCOMPtr<nsIMenuBar> mMenuBar;
  NSWindow*            mSheetWindowParent; // if this is a sheet, this is the NSWindow it's attached to
  nsChildView*         mPopupContentView; // if this is a popup, this is its content widget

  PRPackedBool         mIsResizing;     // we originated the resize, prevent infinite recursion
  PRPackedBool         mWindowMadeHere; // true if we created the window, false for embedding
  PRPackedBool         mVisible;        // Whether or not we're visible.
  PRPackedBool         mSheetNeedsShow; // if this is a sheet, are we waiting to be shown?
  PRPackedBool         mModal;
};


#endif // nsCocoaWindow_h_
