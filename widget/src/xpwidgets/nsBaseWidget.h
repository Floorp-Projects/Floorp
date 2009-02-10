/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#ifndef nsBaseWidget_h__
#define nsBaseWidget_h__

#include "nsRect.h"
#include "nsIWidget.h"
#include "nsIEventListener.h"
#include "nsIToolkit.h"
#include "nsIAppShell.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"

class nsIContent;
class nsAutoRollup;

/**
 * Common widget implementation used as base class for native
 * or crossplatform implementations of Widgets. 
 * All cross-platform behavior that all widgets need to implement 
 * should be placed in this class. 
 * (Note: widget implementations are not required to use this
 * class, but it gives them a head start.)
 */

class nsBaseWidget : public nsIWidget
{
  friend class nsAutoRollup;

public:
  nsBaseWidget();
  virtual ~nsBaseWidget();
  
  NS_DECL_ISUPPORTS
  
  NS_IMETHOD              PreCreateWidget(nsWidgetInitData *aWidgetInitData) { return NS_OK;}
  
  // nsIWidget interface
  NS_IMETHOD              CaptureMouse(PRBool aCapture);
  NS_IMETHOD              Validate();
  NS_IMETHOD              InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous);
  NS_IMETHOD              GetClientData(void*& aClientData);
  NS_IMETHOD              SetClientData(void* aClientData);
  NS_IMETHOD              Destroy();
  NS_IMETHOD              SetParent(nsIWidget* aNewParent);
  virtual nsIWidget*      GetParent(void);
  virtual nsIWidget*      GetTopLevelWidget(PRInt32* aLevelsUp = NULL);
  virtual nsIWidget*      GetSheetWindowParent(void);
  virtual void            AddChild(nsIWidget* aChild);
  virtual void            RemoveChild(nsIWidget* aChild);

  NS_IMETHOD              SetZIndex(PRInt32 aZIndex);
  NS_IMETHOD              GetZIndex(PRInt32* aZIndex);
  NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                      nsIWidget *aWidget, PRBool aActivate);

  NS_IMETHOD              SetSizeMode(PRInt32 aMode);
  NS_IMETHOD              GetSizeMode(PRInt32* aMode);

  virtual nscolor         GetForegroundColor(void);
  NS_IMETHOD              SetForegroundColor(const nscolor &aColor);
  virtual nscolor         GetBackgroundColor(void);
  NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
  virtual nsCursor        GetCursor();
  NS_IMETHOD              SetCursor(nsCursor aCursor);
  NS_IMETHOD              SetCursor(imgIContainer* aCursor,
                                    PRUint32 aHotspotX, PRUint32 aHotspotY);
  NS_IMETHOD              GetWindowType(nsWindowType& aWindowType);
  NS_IMETHOD              SetWindowType(nsWindowType aWindowType);
  virtual void            SetTransparencyMode(nsTransparencyMode aMode);
  virtual nsTransparencyMode GetTransparencyMode();
  NS_IMETHOD              SetWindowShadowStyle(PRInt32 aStyle);
  NS_IMETHOD              HideWindowChrome(PRBool aShouldHide);
  NS_IMETHOD              MakeFullScreen(PRBool aFullScreen);
  virtual nsIRenderingContext* GetRenderingContext();
  virtual nsIDeviceContext* GetDeviceContext();
  virtual nsIToolkit*     GetToolkit();  
  virtual gfxASurface*    GetThebesSurface();
  NS_IMETHOD              SetModal(PRBool aModal); 
  NS_IMETHOD              ModalEventFilter(PRBool aRealEvent, void *aEvent,
                            PRBool *aForWindow);
  NS_IMETHOD              SetWindowClass(const nsAString& xulWinType);
  NS_IMETHOD              SetBorderStyle(nsBorderStyle aBorderStyle); 
  NS_IMETHOD              AddEventListener(nsIEventListener * aListener);
  NS_IMETHOD              SetBounds(const nsIntRect &aRect);
  NS_IMETHOD              GetBounds(nsIntRect &aRect);
  NS_IMETHOD              GetClientBounds(nsIntRect &aRect);
  NS_IMETHOD              GetScreenBounds(nsIntRect &aRect);
  NS_IMETHOD              GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight);
  NS_IMETHOD              ScrollRect(nsIntRect &aRect, PRInt32 aDx, PRInt32 aDy);
  NS_IMETHOD              ScrollWidgets(PRInt32 aDx, PRInt32 aDy);
  NS_IMETHOD              EnableDragDrop(PRBool aEnable);
  NS_IMETHOD              GetAttention(PRInt32 aCycleCount);
  NS_IMETHOD              GetLastInputEventTime(PRUint32& aTime);
  NS_IMETHOD              SetIcon(const nsAString &anIconSpec);
  NS_IMETHOD              BeginSecureKeyboardInput();
  NS_IMETHOD              EndSecureKeyboardInput();
  NS_IMETHOD              SetWindowTitlebarColor(nscolor aColor, PRBool aActive);
  virtual PRBool          ShowsResizeIndicator(nsIntRect* aResizerRect);
  virtual void            ConvertToDeviceCoordinates(nscoord  &aX,nscoord &aY) {}
  virtual void            FreeNativeData(void * data, PRUint32 aDataType) {}
  NS_IMETHOD              BeginResizeDrag(nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical);
  virtual nsresult        ActivateNativeMenuItemAt(const nsAString& indexString) { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult        ForceUpdateNativeMenuAt(const nsAString& indexString) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              ResetInputState() { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              SetIMEOpenState(PRBool aState) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              GetIMEOpenState(PRBool* aState) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              SetIMEEnabled(PRUint32 aState) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              GetIMEEnabled(PRUint32* aState) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              CancelIMEComposition() { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              GetToggledKeyState(PRUint32 aKeyCode, PRBool* aLEDState) { return NS_ERROR_NOT_IMPLEMENTED; }

protected:

  virtual void            ResolveIconName(const nsAString &aIconName,
                                          const nsAString &aIconSuffix,
                                          nsILocalFile **aResult);
  virtual void            OnDestroy();
  virtual void            BaseCreate(nsIWidget *aParent,
                                     const nsIntRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell,
                                     nsIToolkit *aToolkit,
                                     nsWidgetInitData *aInitData);

  virtual nsIContent* GetLastRollup()
  {
    return mLastRollup;
  }

  virtual nsresult SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                            PRInt32 aNativeKeyCode,
                                            PRUint32 aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters)
  { return NS_ERROR_UNEXPECTED; }

protected: 
  void*             mClientData;
  EVENT_CALLBACK    mEventCallback;
  nsIDeviceContext  *mContext;
  nsIToolkit        *mToolkit;
  nsIEventListener  *mEventListener;
  nscolor           mBackground;
  nscolor           mForeground;
  nsCursor          mCursor;
  nsWindowType      mWindowType;
  nsBorderStyle     mBorderStyle;
  PRPackedBool      mIsShiftDown;
  PRPackedBool      mIsControlDown;
  PRPackedBool      mIsAltDown;
  PRPackedBool      mIsDestroying;
  PRPackedBool      mOnDestroyCalled;
  nsIntRect         mBounds;
  nsIntRect*        mOriginalBounds;
  PRInt32           mZIndex;
  nsSizeMode        mSizeMode;

  // the last rolled up popup. Only set this when an nsAutoRollup is in scope,
  // so it can be cleared automatically.
  static nsIContent* mLastRollup;
    
    // Enumeration of the methods which are accessible on the "main GUI thread"
    // via the CallMethod(...) mechanism...
    // see nsSwitchToUIThread
  enum {
    CREATE       = 0x0101,
    CREATE_NATIVE,
    DESTROY, 
    SET_FOCUS,
    SET_CURSOR,
    CREATE_HACK
  };

#ifdef DEBUG
protected:
  static nsAutoString debug_GuiEventToString(nsGUIEvent * aGuiEvent);
  static PRBool debug_WantPaintFlashing();

  static void debug_DumpInvalidate(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsIntRect *     aRect,
                                   PRBool                aIsSynchronous,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID);

  static void debug_DumpEvent(FILE *                aFileOut,
                              nsIWidget *           aWidget,
                              nsGUIEvent *          aGuiEvent,
                              const nsCAutoString & aWidgetName,
                              PRInt32               aWindowID);
  
  static void debug_DumpPaintEvent(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   nsPaintEvent *        aPaintEvent,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID);

  static PRBool debug_GetCachedBoolPref(const char* aPrefName);
#endif
};

// A situation can occur when a mouse event occurs over a menu label while the
// menu popup is already open. The expected behaviour is to close the popup.
// This happens by calling nsIRollupListener::Rollup before the mouse event is
// processed. However, in cases where the mouse event is not consumed, this
// event will then get targeted at the menu label causing the menu to open
// again. To prevent this, we store in mLastRollup a reference to the popup
// that was closed during the Rollup call, and prevent this popup from
// reopening while processing the mouse event.
// mLastRollup should only be set while an nsAutoRollup is in scope;
// when it goes out of scope mLastRollup is cleared automatically.
// As mLastRollup is static, it can be retrieved by calling
// nsIWidget::GetLastRollup on any widget.
class nsAutoRollup
{
  PRBool wasClear;

  public:

  nsAutoRollup();
  ~nsAutoRollup();
};

#endif // nsBaseWidget_h__
