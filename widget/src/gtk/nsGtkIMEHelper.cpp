/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Frank Tang <ftang@netsape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <gdk/gdkx.h>

#include "nsGUIEvent.h"
#include "nsGtkIMEHelper.h"
#include "nsIUnicodeDecoder.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIServiceManager.h"
#include "nsIPref.h"
#include <X11/Xatom.h>

#include "nsWindow.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

#ifdef USE_XIM
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

nsIMEStatus *nsIMEGtkIC::gStatus = 0;
#endif // USE_XIM 

nsGtkIMEHelper* nsGtkIMEHelper::gSingleton = nsnull;

nsGtkIMEHelper* nsGtkIMEHelper::GetSingleton()
{
  if(! gSingleton)
    gSingleton = new nsGtkIMEHelper();
  NS_ASSERTION(gSingleton, "do not have singleton");
  return gSingleton;  
}

/*static*/ void nsGtkIMEHelper::Shutdown()
{
  if (gSingleton) {
    delete gSingleton;
    gSingleton = nsnull;
  }
}
//-----------------------------------------------------------------------
MOZ_DECL_CTOR_COUNTER(nsGtkIMEHelper)

nsGtkIMEHelper::nsGtkIMEHelper()
{
  MOZ_COUNT_CTOR(nsGtkIMEHelper);
  SetupUnicodeDecoder(); 
}
nsGtkIMEHelper::~nsGtkIMEHelper()
{
  MOZ_COUNT_DTOR(nsGtkIMEHelper);
  NS_IF_RELEASE(mDecoder);
}
//-----------------------------------------------------------------------
nsresult
nsGtkIMEHelper::ConvertToUnicode( const char* aSrc, PRInt32* aSrcLen,
  PRUnichar* aDest, PRInt32* aDestLen)
{
  NS_ASSERTION(mDecoder, "do not have mDecoder");
  if(! mDecoder)
     return NS_ERROR_ABORT;
  return mDecoder->Convert(aSrc, aSrcLen, aDest, aDestLen);
}

void
nsGtkIMEHelper::ResetDecoder()
{
  NS_ASSERTION(mDecoder, "do not have mDecoder");
  if(mDecoder) {
    mDecoder->Reset();
  }
}

//-----------------------------------------------------------------------
void nsGtkIMEHelper::SetupUnicodeDecoder()
{
  mDecoder = nsnull;
  nsresult result = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPlatformCharset> platform = 
           do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &result);
  if (platform && NS_SUCCEEDED(result)) {
    nsAutoString charset;
    charset.AssignWithConversion("");
    result = platform->GetCharset(kPlatformCharsetSel_Menu, charset);
    if (NS_FAILED(result) || (charset.Length() == 0)) {
      charset.AssignWithConversion("ISO-8859-1");   // default
    }
    nsICharsetConverterManager* manager = nsnull;
    nsresult res = nsServiceManager::
      GetService(kCharsetConverterManagerCID,
                 NS_GET_IID(nsICharsetConverterManager),
                 (nsISupports**)&manager);
    if (manager && NS_SUCCEEDED(res)) {
      manager->GetUnicodeDecoder(&charset, &mDecoder);
      nsServiceManager::ReleaseService(kCharsetConverterManagerCID, manager);
    }
  }
  NS_ASSERTION(mDecoder, "cannot get decoder");
}

PRInt32
nsGtkIMEHelper::MultiByteToUnicode(const char *aMbSrc,
                                   const PRInt32 aMbSrcLen,
                                   PRUnichar **aUniDes,
                                   PRInt32 *aUniDesLen)
{
  nsresult res;
  PRInt32 srcLeft;
  PRUnichar *uniCharLeft;
  PRInt32 uniCharSize = 0;

  if (nsGtkIMEHelper::GetSingleton()) {
    if (!*aUniDes || *aUniDesLen == 0) {
      *aUniDesLen = 128;
      *aUniDes = new PRUnichar[*aUniDesLen];
    }
    for (;;) {
      if (*aUniDes == nsnull) {
        uniCharSize = 0;
        break;
      }
      uniCharLeft = *aUniDes;
      uniCharSize = *aUniDesLen - 1;
      srcLeft = aMbSrcLen;
      res = nsGtkIMEHelper::GetSingleton()->ConvertToUnicode(
		       (char *)aMbSrc, &srcLeft, uniCharLeft, &uniCharSize);
      if (NS_ERROR_ABORT == res) {
        uniCharSize = 0;
        break;
      }
      if (srcLeft == aMbSrcLen && uniCharSize < *aUniDesLen - 1) {
        break;
      }
      nsGtkIMEHelper::GetSingleton()->ResetDecoder();
      *aUniDesLen += 32;
      if (aUniDes) {
        delete[] *aUniDes;
      }
      *aUniDes = new PRUnichar[*aUniDesLen];
    }
  }
  return uniCharSize;
}

#ifdef USE_XIM
nsIMEPreedit::nsIMEPreedit()
{
  mCaretPosition = 0;
  mIMECompUnicode = new nsAutoString();
  mIMECompAttr = new nsCAutoString();
  mCompositionUniString = 0;
  mCompositionUniStringSize = 0;
}

nsIMEPreedit::~nsIMEPreedit()
{
  mCaretPosition = 0;
  delete mIMECompUnicode;
  delete mIMECompAttr;
  if (mCompositionUniString) {
    delete[] mCompositionUniString;
  }
  mCompositionUniString = 0;
  mCompositionUniStringSize = 0;
}

void nsIMEPreedit::Reset()
{
  mCaretPosition = 0;
  mIMECompUnicode->SetCapacity(0);
  mIMECompUnicode->mLength = 0;
  mIMECompAttr->SetCapacity(0);
  mIMECompAttr->mLength = 0;
}

PRUnichar*
nsIMEPreedit::GetPreeditString() const {
  return (PRUnichar *) mIMECompUnicode->get();
}

char*
nsIMEPreedit::GetPreeditFeedback() const {
  return (char*)mIMECompAttr->get();
}

int nsIMEPreedit::GetPreeditLength() const {
  return mIMECompUnicode->Length();
}

void nsIMEPreedit::SetPreeditString(const XIMText *aText,
                                    const PRInt32 aChangeFirst,
                                    const PRInt32 aChangeLength)
{
  PRInt32 composeUniStringLen = 0;
  char *preeditStr = 0;
  int preeditLen = 0;
  XIMFeedback *preeditFeedback = 0;

  if (aText){
    if (aText->encoding_is_wchar) {
      if (aText->string.wide_char) {
        int len = wcstombs(NULL, aText->string.wide_char, aText->length);
        if (len != -1) {
          preeditStr = new char [len + 1];
          wcstombs(preeditStr, aText->string.wide_char, len);
          preeditStr[len] = 0;
        }
      }
    } else {
      preeditStr = aText->string.multi_byte;
    }
    preeditLen = aText->length;
    preeditFeedback = aText->feedback;
  }

  if (preeditStr && nsGtkIMEHelper::GetSingleton()) {
    composeUniStringLen =
      nsGtkIMEHelper::GetSingleton()->MultiByteToUnicode(
				preeditStr,
				nsCRT::strlen(preeditStr),
				&(mCompositionUniString),
				&(mCompositionUniStringSize));
    if (aText && aText->encoding_is_wchar) {
      delete [] preeditStr;
    }
  }

  if (composeUniStringLen != preeditLen) {
    Reset();
    return;
  }
  if (aChangeLength) {
    mIMECompUnicode->Cut(aChangeFirst, aChangeLength);
    mIMECompAttr->Cut(aChangeFirst, aChangeLength);
  }

  if (composeUniStringLen) {
    mIMECompUnicode->Insert(mCompositionUniString,
                            aChangeFirst,
                            composeUniStringLen);
    char *feedbackAttr = new char[composeUniStringLen];
    char *pFeedbackAttr;
    for (pFeedbackAttr = feedbackAttr;
         pFeedbackAttr < &feedbackAttr[composeUniStringLen];
         pFeedbackAttr++) {
      switch (*preeditFeedback++) {
      case XIMReverse:
        *pFeedbackAttr = NS_TEXTRANGE_SELECTEDRAWTEXT;
        break;
      case XIMUnderline:
        *pFeedbackAttr = NS_TEXTRANGE_CONVERTEDTEXT;
        break;
      case XIMHighlight:
        *pFeedbackAttr = NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
        break;
      default:
        *pFeedbackAttr = NS_TEXTRANGE_RAWINPUT;
      }
    }
    mIMECompAttr->Insert((const char*)feedbackAttr,
                         aChangeFirst,
                         composeUniStringLen);
    delete [] feedbackAttr;
  }
}

#define	START_OFFSET(I)	\
    (*aTextRangeListResult)[I].mStartOffset

#define	END_OFFSET(I)	\
    (*aTextRangeListResult)[I].mEndOffset

#define	SET_FEEDBACK(F, I) (*aTextRangeListResult)[I].mRangeType = F

void
nsIMEPreedit::IMSetTextRange(const PRInt32 aLen,
                             const char *aFeedback,
                             PRUint32 *aTextRangeListLengthResult,
                             nsTextRangeArray *aTextRangeListResult)
{
  int i;
  char *feedbackPtr = (char*)aFeedback;

  int count = 1;

  /* count number of feedback */
  char f = *feedbackPtr;
  for (i = 0; i < aLen; i++, feedbackPtr++) {
    if (f != *feedbackPtr) {
      f = *feedbackPtr;
      count++;
    }
  }

  /* for NS_TEXTRANGE_CARETPOSITION */
  count++;

  /* alloc nsTextRange */
  feedbackPtr = (char*)aFeedback;
  *aTextRangeListLengthResult = count;
  *aTextRangeListResult = new nsTextRange[count];

  count = 0;

  /* Set NS_TEXTRANGE_CARETPOSITION */
  (*aTextRangeListResult)[count].mRangeType = NS_TEXTRANGE_CARETPOSITION;
  START_OFFSET(count) = aLen;
  END_OFFSET(count) = aLen;

  if (aLen == 0){
    return;
  }

  count++;

  f = *feedbackPtr;
  SET_FEEDBACK(f, count);

  START_OFFSET(count) = 0;
  (*aTextRangeListResult)[count].mStartOffset = 0;

  /* set TextRange */
  for (i = 0; i < aLen; i++, feedbackPtr++) {
    if (f != *feedbackPtr) {
      END_OFFSET(count) = i;
      f = *feedbackPtr;
      count++;
      SET_FEEDBACK(f, count);
      START_OFFSET(count) = i;
    }
  }
  END_OFFSET(count) = aLen;

#ifdef  NOISY_XIM
  printf("IMSetTextRange()\n");
  for (i = 0; i < *textRangeListLengthResult; i++) {
    printf("	i=%d start=%d end=%d attr=%d\n",
	   i,
	   (*textRangeListResult)[i].mStartOffset,
	   (*textRangeListResult)[i].mEndOffset,
	   (*textRangeListResult)[i].mRangeType);
  }
#endif /* NOISY_XIM */
}

extern "C" {
  extern void
    _XRegisterFilterByType(Display *display, Window window,
                         int start_type, int end_type, 
                         Bool (*filter)(Display*, Window,
                                        XEvent*, XPointer),
                         XPointer client_data);
  extern void
    _XUnregisterFilter(Display *display, Window window,
                         Bool (*filter)(Display*, Window,
                                        XEvent*, XPointer),
                         XPointer client_data);
  extern int _XDefaultError(Display*, XErrorEvent*);
}

void
nsIMEStatus::DestroyNative() {
}

nsIMEStatus::nsIMEStatus() {
  mWidth = 0;
  mHeight = 0;
  mFontset = 0;
  CreateNative();
}

nsIMEStatus::nsIMEStatus(GdkFont *aFontset) {
  mWidth = 0;
  mHeight = 0;
  mFontset = 0;
  if (aFontset->type == GDK_FONT_FONTSET) {
    mFontset = (XFontSet) GDK_FONT_XFONT(aFontset);
  }
  CreateNative();
}

nsIMEStatus::~nsIMEStatus() {
  DestroyNative();
}

void
nsIMEStatus::SetFont(GdkFont *aFontset) {
  if (!mAttachedWindow) {
    return;
  }
  nsIMEGtkIC *xic = mAttachedWindow->IMEGetInputContext(PR_FALSE);
  if (xic && xic->mStatusText) {
    if (aFontset->type == GDK_FONT_FONTSET) {
      mFontset = (XFontSet) GDK_FONT_XFONT(aFontset);
      resize(xic->mStatusText);
    }
  }
}

void
AdjustPlacementInsideScreen(Display * dpy, Window win,
                            int x, int y,
                            int width, int height,
                            int * ret_x, int * ret_y) {
  XWindowAttributes attr;
  int dpy_width;
  int dpy_height;
  int screen_num;

  width += 20;
  height += 20;

  if (XGetWindowAttributes(dpy, win, &attr) > 0) {
    screen_num = XScreenNumberOfScreen(attr.screen);
  } else {
    screen_num = 0;
  }

  dpy_width = DisplayWidth(dpy, screen_num);
  dpy_height = DisplayHeight(dpy, screen_num);

  if (dpy_width < (x + width)) {
    if (width <= dpy_width) {
      *ret_x = (dpy_width - width);
    } else {
      *ret_x = 0;
    }
  } else {
    *ret_x = x;
  }

  if (dpy_height < (y + height)) {
    if (height <= dpy_height) {
      *ret_y = (dpy_height - height);
    } else {
      *ret_y = 0;
    }
  } else {
    *ret_y = y;
  }
}

int
validateCoordinates(Display * display, Window w,
                    int *x, int *y) {
  XWindowAttributes attr;
  int newx, newy;
  if (XGetWindowAttributes(display, w, &attr) > 0) {
    AdjustPlacementInsideScreen(display, w, *x, *y,
                                attr.width, attr.height,
                                &newx, &newy);
    *x = newx;
    *y = newy;
  }
  return 0;
}

void
nsIMEStatus::hide() {
  Display *display = GDK_DISPLAY();
  int screen = DefaultScreen(display);
  XWindowAttributes win_att;
  if (XGetWindowAttributes(display, mIMStatusWindow,
                           &win_att) > 0) {
    if (win_att.map_state != IsUnmapped) {
      XWithdrawWindow(display, mIMStatusWindow, screen);
    }
  }
  return;
}

void
nsIMEStatus::UnregisterClientFilter(Window aWindow) {
  Display *display = GDK_DISPLAY();
  _XUnregisterFilter(display, aWindow,
                     client_filter, (XPointer)this);
}

void
nsIMEStatus::RegisterClientFilter(Window aWindow) {
  Display *display = GDK_DISPLAY();
  _XRegisterFilterByType(display, aWindow,
                         ConfigureNotify, ConfigureNotify,
                         client_filter,
                         (XPointer)this);
  _XRegisterFilterByType(display, aWindow,
                         DestroyNotify, DestroyNotify,
                         client_filter,
                         (XPointer)this);
}

// when referred nsWindow is destroyed, reset to 0
void
nsIMEStatus::resetParentWindow(nsWindow *aWindow) {
    if (mAttachedWindow == aWindow) {
      mAttachedWindow = 0;
    }
}

void
nsIMEStatus::setParentWindow(nsWindow *aWindow) {
  GdkWindow *gdkWindow = (GdkWindow*)aWindow->GetNativeData(NS_NATIVE_WINDOW);
  GdkWindow *newParentGdk = gdk_window_get_toplevel(gdkWindow);

  // mAttachedWindow is set to 0 when target window is destroyed
  // set aWindow to this even if newParentGdk == mParent case
  // keep focused (target) Window as mAttachedWindow
  mAttachedWindow = aWindow;

  if (newParentGdk != mParent) {
    hide();
    if (mParent) {
      UnregisterClientFilter(GDK_WINDOW_XWINDOW(mParent));
    }
    mParent = newParentGdk;
    if (mIMStatusWindow) {
      Display *display = GDK_DISPLAY();
      XSetTransientForHint(display, mIMStatusWindow,
                           GDK_WINDOW_XWINDOW(newParentGdk));
      RegisterClientFilter(GDK_WINDOW_XWINDOW(newParentGdk));
    }
  }
}

Bool
nsIMEStatus::client_filter(Display *aDisplay, Window aWindow,
                            XEvent *aEvent, XPointer aClientData) {
  nsIMEStatus *thiswindow = (nsIMEStatus*)aClientData;
  if (thiswindow && NULL != aEvent) {
    if (ConfigureNotify == aEvent->type) {
      thiswindow->show();
    } else if (DestroyNotify == aEvent->type) {
      thiswindow->UnregisterClientFilter(aWindow);
      thiswindow->hide();
      thiswindow->mAttachedWindow = 0;
    }
  }
  return False;
}

Bool
nsIMEStatus::repaint_filter(Display *aDisplay, Window aWindow,
                            XEvent *aEvent, XPointer aClientData) {
  if (aEvent->xexpose.count != 0) return True;
  nsIMEStatus *thiswindow = (nsIMEStatus*)aClientData;
  if (thiswindow && thiswindow->mAttachedWindow) {
    nsIMEGtkIC *xic = thiswindow->mAttachedWindow->IMEGetInputContext(PR_FALSE);
    if (xic && xic->mStatusText) {
      if(nsCRT::strlen(xic->mStatusText) == 0) {
        thiswindow->hide();
      } else {
        thiswindow->setText(xic->mStatusText);
      }
    }
  }
  return True;
}

// referring to gdk_wm_protocols_filter in gtk+/gdk/gdkevent.c
Bool
nsIMEStatus::clientmessage_filter(Display *aDisplay, Window aWindow,
                                  XEvent *aEvent, XPointer aClientData) {
  Atom wm_delete_window = XInternAtom(aDisplay, "WM_DELETE_WINDOW", False);
  if ((Atom)aEvent->xclient.data.l[0] == wm_delete_window) {
    // The delete window request specifies a window
    // to delete. We don't actually destroy the
    // window because "it is only a request"
    return True;
  }
  return False;
}

void
nsIMEStatus::CreateNative() {
  mGC = 0;
  mParent = 0;
  mAttachedWindow = 0;

  Display *display = GDK_DISPLAY();

  if (!mFontset) {
    const char *base_font_name = "-*-*-*-*-*-*-16-*-*-*-*-*-*-*";
    char **missing_list;
    int missing_count;
    char *def_string;
    mFontset = XCreateFontSet(display, base_font_name, &missing_list,
                            &missing_count, &def_string);
  }

  if (!mFontset) {
#ifdef DEBUG
    printf("Error : XCreateFontSet() !\n");
#endif
    return;
  }
  int screen = DefaultScreen(display);
  unsigned long bpixel = BlackPixel(display, screen);
  unsigned long fpixel = WhitePixel(display, screen);
  Window root = RootWindow(display, screen);

  XFontSetExtents *fse;
  fse = XExtentsOfFontSet(mFontset);
  mHeight = fse->max_logical_extent.height;
  mHeight += (fse->max_ink_extent.height + fse->max_ink_extent.y);
  
  if (mWidth == 0) mWidth = 1;
  if (mHeight == 0) mHeight = 1;
  mIMStatusWindow = XCreateSimpleWindow(display, root, 0, 0,
                                        mWidth, mHeight, 2,
                                        bpixel, fpixel);
  if (!mIMStatusWindow) return;

  _XRegisterFilterByType(display, mIMStatusWindow,
                         Expose, Expose,
                         repaint_filter,
                         (XPointer)this);
  _XRegisterFilterByType(display, mIMStatusWindow,
                         ClientMessage, ClientMessage,
                         clientmessage_filter,
                         (XPointer)this);

  // For XIM status window, we must watch wm_delete_window only,
  // but not wm_take_focus, since the window shoud not take focus.
  // 
  // gdk_window_new forces all toplevel windows to watch both of wm
  // protocols, which is one of the reasons we cannot use gdk for
  // creating and managing the XIM status window.
  Atom wm_window_protocols[1];
  Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
  wm_window_protocols[0] = wm_delete_window;
  XSetWMProtocols(display, mIMStatusWindow,
                  wm_window_protocols, 1);
  remove_decoration();

  // The XIM status window does not expect keyboard input, so we need to
  // set wm_input_hint of XWMhints to False. However, gdk_window_new()
  // always set it to True, and there is no way to override it. The attempt
  // to set input hint to False after we do gdk_window_new() did not work
  // well either in GNOME or CDE environment.
  XWMHints wm_hints;
  wm_hints.flags = InputHint;
  wm_hints.input = False;
  XSetWMHints(display, mIMStatusWindow, &wm_hints);

  XStoreName(display, mIMStatusWindow, "Mozilla IM Status");

  XClassHint class_hint;
  class_hint.res_name = "mozilla-im-status";
  class_hint.res_class = "MozillaImStatus";
  XSetClassHint(display, mIMStatusWindow, &class_hint);

  long mask = ExposureMask;
  XSelectInput(display, mIMStatusWindow, mask);
}

void
nsIMEStatus::resize(const char *aString) {
  Display *display = GDK_DISPLAY();
  if (!aString || !aString[0]) return;
  int len = nsCRT::strlen(aString);

  int width = XmbTextEscapement(mFontset, aString, len);

  if (!width) return;

  XWindowChanges changes;
  int mask = CWWidth;
  changes.width = width;
  XConfigureWindow(display, mIMStatusWindow,
                   mask, &changes);
  mWidth = width;
  return;
}

// public
void
nsIMEStatus::show() {
  if (!mAttachedWindow) {
    return;
  }

  nsIMEGtkIC *xic = mAttachedWindow->IMEGetInputContext(PR_FALSE);

  if (!xic || !xic->mStatusText || !nsCRT::strlen(xic->mStatusText)) {
    // don't map if text is ""
    return;
  }

  Display *display = GDK_DISPLAY();

  if (!mIMStatusWindow) {
    CreateNative();
  }

  XWindowAttributes win_att;
  int               wx, wy;
  Window            w_none;
  Window            parent = GDK_WINDOW_XWINDOW(mParent);

  // parent window is destroyed
  if (!parent || ((GdkWindowPrivate*)mParent)->destroyed) {
      return;
  }

  // parent window exists but isn't mapped yet
  if (XGetWindowAttributes(display, parent,
                           &win_att) > 0) {
    if (win_att.map_state == IsUnmapped) {
      return;
    }
  }

  if (XGetWindowAttributes(display, parent, &win_att) > 0) {
    XTranslateCoordinates(display, parent,
                          win_att.root,
                          -(win_att.border_width),
                          -(win_att.border_width),
                          &wx, &wy, &w_none);
    wy += win_att.height;

    // adjust position
    validateCoordinates(display,mIMStatusWindow,
                        &wx, &wy);

    // set position
    // Linux window manager doesn't work properly
    // when only XConfigureWindow() is called
    XSizeHints                   xsh;
    (void) memset(&xsh, 0, sizeof(xsh));
    xsh.flags |= USPosition;
    xsh.x = wx;
    xsh.y = wy;
    XSetWMNormalHints(display, mIMStatusWindow, &xsh);

    XWindowChanges changes;
    int mask = CWX|CWY;
    changes.x = wx;
    changes.y = wy;
    XConfigureWindow(display, mIMStatusWindow,
                     mask, &changes);
  }

  if (XGetWindowAttributes(display, mIMStatusWindow,
                           &win_att) > 0) {
    if (win_att.map_state == IsUnmapped) {
      XMapWindow(display, mIMStatusWindow);
    }
  }
  return;
}

void
nsIMEStatus::setText(const char *aText) {
  Display *display = GDK_DISPLAY();
  if (!aText) return;

  int len = nsCRT::strlen(aText);

  if (mGC == 0) {
    XGCValues values;
    unsigned long values_mask;

    int screen = DefaultScreen(display);
    unsigned long bpixel = BlackPixel(display, screen);
    unsigned long fpixel = WhitePixel(display, screen);
    values.foreground = bpixel;
    values.background = fpixel;
    values_mask = GCBackground | GCForeground;
    mGC = XCreateGC(display, mIMStatusWindow, values_mask, &values);
  }
  XClearArea(display, mIMStatusWindow, 0, 0, 0, 0, False);

  resize(aText);

  XFontSetExtents *fse;
  fse = XExtentsOfFontSet(mFontset);
  int bottom_margin = fse->max_logical_extent.height/6;
  int y = fse->max_logical_extent.height - bottom_margin;
  XmbDrawString(display, mIMStatusWindow, mFontset, mGC,
                0, y, aText, len);
  return;
}

static Atom ol_del_atom = (Atom)0;
static Atom ol_del_atom_list[3];
static int ol_atom_inx = 0;
static Atom mwm_del_atom = (Atom)0;

void
nsIMEStatus::getAtoms() {
  Display *display = GDK_DISPLAY();
  if (!mwm_del_atom) {
    mwm_del_atom = XInternAtom(display, "_MOTIF_WM_HINTS", True);
  }
  if (!ol_del_atom) {
    ol_del_atom = XInternAtom(display, "_OL_DECOR_DEL", True);
    ol_del_atom_list[ol_atom_inx++] =
	    XInternAtom(display, "_OL_DECOR_RESIZE", True);
    ol_del_atom_list[ol_atom_inx++] =
	    XInternAtom(display, "_OL_DECOR_HEADER", True);
  }
}

#define MWM_DECOR_BORDER	(1L << 1)

void
nsIMEStatus::remove_decoration() {
  Display *display = GDK_DISPLAY();
  struct _mwmhints {
    unsigned long flags, func, deco;
    long input_mode;
    unsigned long status;
  } mwm_del_hints;
    
  getAtoms();

  if (mwm_del_atom != None) {
    mwm_del_hints.flags = 1L << 1; /* flags for decoration */
    mwm_del_hints.deco = MWM_DECOR_BORDER;
    XChangeProperty(display, mIMStatusWindow,
                    mwm_del_atom, mwm_del_atom, 32,
                    PropModeReplace,
                    (unsigned char *)&mwm_del_hints, 5);
  }
  if (ol_del_atom != None) {
    XChangeProperty(display, mIMStatusWindow, ol_del_atom, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)ol_del_atom_list,
                    ol_atom_inx);
  }
}


/* callbacks */
int
nsIMEGtkIC::preedit_start_cbproc(XIC xic, XPointer client_data,
                                 XPointer call_data)
{
  nsIMEGtkIC *thisXIC = (nsIMEGtkIC*)client_data;
  if (!thisXIC) return 0;
  nsWindow *fwin = thisXIC->mFocusWindow;
  if (!fwin) return 0;

  if (!thisXIC->mPreedit) {
    thisXIC->mPreedit = new nsIMEPreedit();
  }
  thisXIC->mPreedit->Reset();
  fwin->ime_preedit_start();
  return 0;
}

int
nsIMEGtkIC::preedit_draw_cbproc(XIC xic, XPointer client_data,
                                XPointer call_data_p)
{
  nsIMEGtkIC *thisXIC = (nsIMEGtkIC*)client_data;
  if (!thisXIC) return 0;
  nsWindow *fwin = thisXIC->mFocusWindow;
  if (!fwin) return 0;

  XIMPreeditDrawCallbackStruct *call_data =
    (XIMPreeditDrawCallbackStruct *) call_data_p;
  XIMText *text = (XIMText *) call_data->text;

  if (!thisXIC->mPreedit) {
    thisXIC->mPreedit = new nsIMEPreedit();
  }
  thisXIC->mPreedit->SetPreeditString(text,
                                      call_data->chg_first,
                                      call_data->chg_length);
  fwin->ime_preedit_draw(thisXIC);
  return 0;
}

int
nsIMEGtkIC::preedit_done_cbproc(XIC xic, XPointer client_data,
                                XPointer call_data_p)
{
  nsIMEGtkIC *thisXIC = (nsIMEGtkIC*)client_data;
  if (!thisXIC) return 0;
  nsWindow *fwin = thisXIC->mFocusWindow;
  if (!fwin) return 0;

  fwin->ime_preedit_done();
  return 0;
}

int
nsIMEGtkIC::status_draw_cbproc(XIC xic, XPointer client_data,
                               XPointer call_data_p)
{
  nsIMEGtkIC *thisXIC = (nsIMEGtkIC*)client_data;
  if (!thisXIC) return 0;
  nsWindow *fwin = thisXIC->mFocusWindow;
  if (!fwin) return 0;
  if (!gStatus) return 0;
  nsWindow *win = gStatus->mAttachedWindow;
  if (!win) return 0;
  nsIMEGtkIC *ic = win->IMEGetInputContext(PR_FALSE);
  PRBool update = PR_TRUE;

  // sometimes the focused IC != this IC happens when focus
  // is changed. To avoid this case, check current IC of
  // gStatus and thisXIC
  if (thisXIC != ic) {
    // do not update status, just store status string
    // for the correct IC
    update = PR_FALSE;
  }

  XIMStatusDrawCallbackStruct *call_data =
    (XIMStatusDrawCallbackStruct *) call_data_p;

  if (call_data->type == XIMTextType) {
    XIMText *text = (XIMText *) call_data->data.text;
    if (!text || !text->length) {
      // turn conversion off
      thisXIC->SetStatusText("");
      if (update) {
        gStatus->setText("");
        gStatus->hide();
      }
    } else {
      char *statusStr = 0;
      if (text->encoding_is_wchar) {
        if (text->string.wide_char) {
          int len = wcstombs(NULL, text->string.wide_char, text->length);
          if (len != -1) {
            statusStr = new char [len + 1];
            wcstombs(statusStr, text->string.wide_char, len);
            statusStr[len] = 0;
          }
        }
      } else {
        statusStr = text->string.multi_byte;
      }
      // turn conversion on
      thisXIC->SetStatusText(statusStr);
      if (update) {
        gStatus->setText(statusStr);
        gStatus->show();
      }
      if (statusStr && text->encoding_is_wchar) {
        delete [] statusStr;
      }
    }
  }
  return 0;
}

nsWindow *
nsIMEGtkIC::GetFocusWindow()
{
  return mFocusWindow;
}

// workaround for kinput2/over-the-spot/ic-per-shell
//   http://bugzilla.mozilla.org/show_bug.cgi?id=28022
//
// When ic is created with focusWindow is shell widget
// (not per widget), kinput2 never updates the pre-edit
// position until
//  - focusWindow is changed
// Unfortunately it does not affect to the position of
// status window.
//
// we don't have any solution for status window position because
// kinput2 updates only when
//  - client window is changed
//  - turn conversion on/off
// we don't have any interface to do those from here.
// there is one workaround for status window that to set
// the following
//   *OverTheSpotConversion.modeLocation: bottomleft

void
nsIMEGtkIC::SetFocusWindow(nsWindow * aFocusWindow)
{
  mFocusWindow = aFocusWindow;
  GdkWindow *gdkWindow = (GdkWindow*)aFocusWindow->GetNativeData(NS_NATIVE_WINDOW);
  if (!gdkWindow) return;

  if (mInputStyle & GDK_IM_STATUS_CALLBACKS) {
    if (gStatus) {
      gStatus->setParentWindow(aFocusWindow);
    }
  }

  gdk_im_begin((GdkIC *) mIC, gdkWindow);

  if (mInputStyle & GDK_IM_PREEDIT_POSITION) {
    SetPreeditArea(0, 0,
		   (int)((GdkWindowPrivate*)gdkWindow)->width,
		   (int)((GdkWindowPrivate*)gdkWindow)->height);
  }

  if (mInputStyle & GDK_IM_STATUS_CALLBACKS) {
    if (gStatus && mStatusText) {
      gStatus->setText(mStatusText);
      gStatus->show();
    }
  }
}

void
nsIMEGtkIC::ResetStatusWindow(nsWindow *aWindow)
{
  if (gStatus) {
    gStatus->resetParentWindow(aWindow);
  }
}

void
nsIMEGtkIC::UnsetFocusWindow()
{
  gdk_im_end();
}

nsIMEGtkIC *nsIMEGtkIC::GetXIC(nsWindow * aFocusWindow, GdkFont *aFontSet)
{
  return nsIMEGtkIC::GetXIC(aFocusWindow, aFontSet, 0);
}

nsIMEGtkIC *nsIMEGtkIC::GetXIC(nsWindow * aFocusWindow,
                               GdkFont *aFontSet, GdkFont *aStatusFontSet)
{
  nsIMEGtkIC *newic = new nsIMEGtkIC(aFocusWindow, aFontSet, aStatusFontSet);
  if (!newic->mIC || !newic->mIC->xic) {
    delete newic;
    return nsnull;
  }
  return newic;
}

nsIMEGtkIC::~nsIMEGtkIC()
{
  /* XSetTransientForHint does not work for popup-shell, workaround */
  if (gStatus) {
    gStatus->hide();
  }

  if (mPreedit) {
    delete mPreedit;
  }

  if (mIC) {
    // destroy a real XIC
    gdk_ic_destroy((GdkIC *) mIC);
  }

  if (mIC_backup) {
    // destroy a dummy XIC, see the comment in nsIMEGtkIC constructor.
    gdk_ic_destroy((GdkIC *) mIC_backup);
  }

  if (mStatusText) {
    nsCRT::free(mStatusText);
  }

  mIC = 0;
  mIC_backup = 0;
  mPreedit = 0;
  mFocusWindow = 0;
  mStatusText = 0;
}

// xim.input_style:
//
// "xim.input_style" preference is for switching XIM input style.
// We can use the easy understanding and well-known words like
// "on-the-spot"

#define PREF_XIM_INPUTSTYLE		"xim.input_style"
#define	VAL_INPUTSTYLE_ONTHESPOT	"on-the-spot"	/* default */
#define	VAL_INPUTSTYLE_OVERTHESPOT	"over-the-spot"
#define	VAL_INPUTSTYLE_SEPARATE		"separate"
#define	VAL_INPUTSTYLE_NONE		"none"

// xim.preedit.input_style
// xim.status.input_style
//
// "xim.status.input_style" and "xim.preedit.input_style" preferences
// will be overwrote the setting of PREF_XIM_INPUTSTYLE when specfied.
// These preferences are only for special purpose. e.g. debugging

#define PREF_XIM_PREEDIT	"xim.preedit.input_style"
#define PREF_XIM_STATUS		"xim.status.input_style"

#define	VAL_PREEDIT_CALLBACKS	"callbacks"		/* default */
#define	VAL_PREEDIT_POSITION	"position"
#define	VAL_PREEDIT_NOTHING	"nothing"
#define	VAL_PREEDIT_NONE	"none"
#define	VAL_STATUS_CALLBACKS	"callbacks"		/* default */
#define	VAL_STATUS_NOTHING	"nothing"
#define	VAL_STATUS_NONE		"none"

#define SUPPORTED_PREEDIT (GDK_IM_PREEDIT_CALLBACKS |   \
                         GDK_IM_PREEDIT_POSITION |      \
                         GDK_IM_PREEDIT_NOTHING |       \
                         GDK_IM_PREEDIT_NONE)

#define SUPPORTED_STATUS (GDK_IM_STATUS_CALLBACKS       | \
                        GDK_IM_STATUS_NOTHING           | \
                        GDK_IM_STATUS_NONE)

#ifdef sun
static XErrorHandler gdk_error_handler = (XErrorHandler)nsnull;

static int
XIMErrorHandler(Display *dpy, XErrorEvent *event) {
  if (event->error_code == BadWindow) {
    char buffer[128];
    char number[32];
    if (event->request_code < 128) {
      sprintf(number, "%d", event->request_code);
      XGetErrorDatabaseText(dpy, "XRequest", number,
                            "", buffer, 128);
      if (!strcmp(buffer, "X_SendEvent") ||
          /*
            The below conditions should only happen when ic is
            destroyed after focus_window has been already
            destroyed.
          */
          !strcmp(buffer, "X_ChangeWindowAttributes") ||
          !strcmp(buffer, "X_GetWindowAttributes"))
        return 0;
    }
  }
  _XDefaultError(dpy, event);
  return 0;
}
#endif

GdkIMStyle
nsIMEGtkIC::GetInputStyle() {
#ifdef sun
  // set error handler only once
  if (gdk_error_handler == (XErrorHandler)NULL)
    gdk_error_handler = XSetErrorHandler(XIMErrorHandler);
#endif
  GdkIMStyle style;
  GdkIMStyle ret_style = (GdkIMStyle)0;

  PRInt32 ivalue = 0;
  nsresult rv;

  GdkIMStyle prefered_preedit_style = (GdkIMStyle) SUPPORTED_PREEDIT;
  GdkIMStyle prefered_status_style = (GdkIMStyle) SUPPORTED_STATUS;

#ifdef HPUX 
  prefered_preedit_style = (GdkIMStyle) GDK_IM_PREEDIT_POSITION;
  prefered_status_style = (GdkIMStyle) GDK_IM_STATUS_NOTHING;
  style = gdk_im_decide_style((GdkIMStyle)(prefered_preedit_style | prefered_status_style));
  if (style) {
    ret_style = style;
  } else {
    style = gdk_im_decide_style((GdkIMStyle) (SUPPORTED_PREEDIT | SUPPORTED_STATUS));
    if (style) {
      ret_style = style;
    } else {
      ret_style = (GdkIMStyle)(GDK_IM_PREEDIT_NONE|GDK_IM_STATUS_NONE);
    }
  }
  return ret_style;
#endif

  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
  if (!NS_FAILED(rv) && (prefs)) {
    char *input_style;
    rv = prefs->CopyCharPref(PREF_XIM_INPUTSTYLE, &input_style);
    if (NS_SUCCEEDED(rv) && input_style[0]) {
      if (!nsCRT::strcmp(input_style, VAL_INPUTSTYLE_ONTHESPOT)) {
        prefered_preedit_style = (GdkIMStyle) GDK_IM_PREEDIT_CALLBACKS;
        prefered_status_style = (GdkIMStyle) GDK_IM_STATUS_CALLBACKS;
      } else if (!nsCRT::strcmp(input_style, VAL_INPUTSTYLE_OVERTHESPOT)) {
        prefered_preedit_style = (GdkIMStyle) GDK_IM_PREEDIT_POSITION;
        prefered_status_style = (GdkIMStyle) GDK_IM_STATUS_NOTHING;
      } else if (!nsCRT::strcmp(input_style, VAL_INPUTSTYLE_SEPARATE)) {
        prefered_preedit_style = (GdkIMStyle) GDK_IM_PREEDIT_NOTHING;
        prefered_status_style = (GdkIMStyle) GDK_IM_STATUS_NOTHING;
      } else if (!nsCRT::strcmp(input_style, VAL_INPUTSTYLE_NONE)) {
        prefered_preedit_style = (GdkIMStyle) GDK_IM_PREEDIT_NONE;
        prefered_status_style = (GdkIMStyle) GDK_IM_STATUS_NONE;
      }
      nsCRT::free(input_style);
    }
    /* if PREF_XIM_PREEDIT and PREF_XIM_STATUS are defined, use
       those values */
    char *preeditstyle_type;
    rv = prefs->CopyCharPref(PREF_XIM_PREEDIT, &preeditstyle_type);
    if (NS_SUCCEEDED(rv) && preeditstyle_type[0]) {
      if (!nsCRT::strcmp(preeditstyle_type, VAL_PREEDIT_CALLBACKS)) {
        ivalue = GDK_IM_PREEDIT_CALLBACKS;
      } else if (!nsCRT::strcmp(preeditstyle_type, VAL_PREEDIT_POSITION)) {
        ivalue = GDK_IM_PREEDIT_POSITION;
      } else if (!nsCRT::strcmp(preeditstyle_type, VAL_PREEDIT_NOTHING)) {
        ivalue = GDK_IM_PREEDIT_NOTHING;
      } else if (!nsCRT::strcmp(preeditstyle_type, VAL_PREEDIT_NONE)) {
        ivalue = GDK_IM_PREEDIT_NONE;
      } else {
        ivalue = 0;
      }
      if (ivalue) {
        prefered_preedit_style = (GdkIMStyle) ivalue;
      }
      nsCRT::free(preeditstyle_type);
    }
    char *statusstyle_type;
    rv = prefs->CopyCharPref(PREF_XIM_STATUS, &statusstyle_type);
    if (NS_SUCCEEDED(rv) && statusstyle_type[0]) {
      if (!nsCRT::strcmp(statusstyle_type, VAL_STATUS_CALLBACKS)) {
        ivalue = GDK_IM_STATUS_CALLBACKS;
      } else if (!nsCRT::strcmp(statusstyle_type, VAL_STATUS_NOTHING)) {
        ivalue = GDK_IM_STATUS_NOTHING;
      } else if (!nsCRT::strcmp(statusstyle_type, VAL_STATUS_NONE)) {
        ivalue = GDK_IM_STATUS_NONE;
      } else {
        ivalue = 0;
      }
      if (ivalue) {
        prefered_status_style = (GdkIMStyle) ivalue;
      }
      nsCRT::free(statusstyle_type);
    }
  }
  style = gdk_im_decide_style((GdkIMStyle)(prefered_preedit_style | prefered_status_style));
  if (style) {
    ret_style = style;
  } else {
    style = gdk_im_decide_style((GdkIMStyle) (SUPPORTED_PREEDIT | SUPPORTED_STATUS));
    if (style) {
      ret_style = style;
    } else {
      ret_style = (GdkIMStyle)(GDK_IM_PREEDIT_NONE|GDK_IM_STATUS_NONE);
    }
  }
  return ret_style;
}

PRInt32
nsIMEGtkIC::ResetIC(PRUnichar **aUnichar, PRInt32 *aUnisize)
{
  if (IsPreeditComposing() == PR_FALSE) {
    return 0;
  }

  if (!mPreedit) {
    mPreedit = new nsIMEPreedit();
  }
  mPreedit->Reset();

#if XlibSpecificationRelease >= 6
  /* restore conversion state after resetting ic later */
  XIMPreeditState preedit_state = XIMPreeditUnKnown;
  XVaNestedList preedit_attr;

  PRBool is_preedit_state = PR_FALSE;
  preedit_attr = XVaCreateNestedList(0,
                                     XNPreeditState, &preedit_state,
                                     0);
  if (!XGetICValues(mIC->xic,
                    XNPreeditAttributes, preedit_attr,
                    NULL)) {
    is_preedit_state = PR_TRUE;
  }
  XFree(preedit_attr);
#endif

  PRInt32 uniCharSize = 0;
  char *uncommitted_text = XmbResetIC(mIC->xic);
  if (uncommitted_text && uncommitted_text[0]) {
    PRInt32 uncommitted_len = nsCRT::strlen(uncommitted_text);
    uniCharSize = nsGtkIMEHelper::GetSingleton()->MultiByteToUnicode(
				  uncommitted_text, uncommitted_len,
				  aUnichar,
				  aUnisize);
  }
#if XlibSpecificationRelease >= 6
  preedit_attr = XVaCreateNestedList(0,
                                     XNPreeditState, preedit_state,
                                     0);
  if (is_preedit_state) {
    XSetICValues(mIC->xic,
                 XNPreeditAttributes, preedit_attr,
                 NULL);
  }
  XFree(preedit_attr);
#endif
  return uniCharSize;
}

PRBool
nsIMEGtkIC::IsPreeditComposing()
{
  if (mInputStyle & GDK_IM_PREEDIT_CALLBACKS) {
    if (mPreedit && mPreedit->GetPreeditLength()) {
      return PR_TRUE;
    } else {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

GdkFont*
nsIMEGtkIC::GetPreeditFont() {
  mIC->mask = GDK_IC_PREEDIT_FONTSET; // hack
  GdkFont *fontset = 0;
  GdkICAttr *attr = gdk_ic_attr_new();
  if (attr) {
    GdkICAttributesType attrMask = GDK_IC_PREEDIT_FONTSET;
    gdk_ic_get_attr((GdkIC*)mIC, attr, attrMask);
    fontset = attr->preedit_fontset;
    gdk_ic_attr_destroy(attr);
  }
  return fontset;
}

void
nsIMEGtkIC::SetPreeditFont(GdkFont *aFontset) {
  GdkICAttr *attr = gdk_ic_attr_new();
  if (attr) {
    attr->preedit_fontset = aFontset;
    GdkICAttributesType attrMask = GDK_IC_PREEDIT_FONTSET;
    gdk_ic_set_attr((GdkIC*)mIC, attr, attrMask);
    gdk_ic_attr_destroy(attr);
  }
}

void
nsIMEGtkIC::SetStatusText(char *aText) {
  if (!aText) {
    return;
  }
  if (mStatusText) {
    if (!nsCRT::strcmp(aText, mStatusText)) {
      return;
    }
    nsCRT::free(mStatusText);
  }
  mStatusText = nsCRT::strdup(aText);
}

void
nsIMEGtkIC::SetStatusFont(GdkFont *aFontset) {
  if (mInputStyle & GDK_IM_STATUS_CALLBACKS) {
    if (!gStatus) {
      gStatus = new nsIMEStatus(aFontset);
    } else {
      gStatus->SetFont(aFontset);
    }
  } else {
    GdkICAttr *attr = gdk_ic_attr_new();
    if (attr) {
      attr->preedit_fontset = aFontset;
      GdkICAttributesType attrMask = GDK_IC_STATUS_FONTSET;
      gdk_ic_set_attr((GdkIC*)mIC, attr, attrMask);
      gdk_ic_attr_destroy(attr);
    }
  }
}

void
nsIMEGtkIC::SetPreeditSpotLocation(unsigned long aX, unsigned long aY) {
  GdkICAttr *attr = gdk_ic_attr_new();
  if (attr) {
    GdkICAttributesType attrMask = GDK_IC_SPOT_LOCATION;
    attr->spot_location.x = aX;
    attr->spot_location.y = aY;
    gdk_ic_set_attr((GdkIC*)mIC, attr, attrMask);
    gdk_ic_attr_destroy(attr);
  }
}

void
nsIMEGtkIC::SetPreeditArea(int aX, int aY, int aW, int aH) {
  GdkICAttr *attr = gdk_ic_attr_new();
  if (attr) {
    GdkICAttributesType attrMask = GDK_IC_PREEDIT_AREA;
    attr->preedit_area.x = aX;
    attr->preedit_area.y = aY;
    attr->preedit_area.width = aW;
    attr->preedit_area.height = aH;
    gdk_ic_set_attr((GdkIC*)mIC, attr, attrMask);
    gdk_ic_attr_destroy(attr);
  }
}

nsIMEGtkIC::nsIMEGtkIC(nsWindow *aFocusWindow, GdkFont *aFontSet)
{
  ::nsIMEGtkIC(aFocusWindow, aFontSet, 0);
}

nsIMEGtkIC::nsIMEGtkIC(nsWindow *aFocusWindow, GdkFont *aFontSet,
						GdkFont *aStatusFontSet)
{
  mFocusWindow = 0;
  mIC = 0;
  mIC_backup = 0;
  mPreedit = 0;
  mStatusText = 0;

  GdkWindow *gdkWindow = (GdkWindow *) aFocusWindow->GetNativeData(NS_NATIVE_WINDOW);
  if (!gdkWindow) {
    return;
  }
  if (!gdk_im_ready()) return;  // nothing to do

  mInputStyle = GetInputStyle();
  if (!mInputStyle) {
    return;
  }

  GdkICAttr *attr = gdk_ic_attr_new();
  GdkICAttributesType attrmask = GDK_IC_ALL_REQ;

  attr->style = mInputStyle;
  attr->client_window = gdkWindow;

  attrmask = (GdkICAttributesType)(attrmask | GDK_IC_PREEDIT_COLORMAP);
  attr->preedit_colormap = ((GdkWindowPrivate*)gdkWindow)->colormap;
  attrmask = (GdkICAttributesType) (attrmask | GDK_IC_PREEDIT_POSITION_REQ);

  if (!(mInputStyle & GDK_IM_PREEDIT_CALLBACKS)) {
    attr->preedit_area.width = ((GdkWindowPrivate*)gdkWindow)->width;
    attr->preedit_area.height = ((GdkWindowPrivate*)gdkWindow)->height;
    attr->preedit_area.x = 0;
    attr->preedit_area.y = 0;
    attrmask = (GdkICAttributesType) (attrmask | GDK_IC_PREEDIT_AREA);
  }
  if (aFontSet) {
    attr->preedit_fontset = aFontSet;
    attrmask = (GdkICAttributesType) (attrmask | GDK_IC_PREEDIT_FONTSET);
  }

  if (aStatusFontSet) {
    if (mInputStyle & GDK_IM_STATUS_CALLBACKS) {
      if (!gStatus) {
        gStatus = new nsIMEStatus(aStatusFontSet);
      }
    } else {
      attr->status_fontset = aStatusFontSet;
      attrmask = (GdkICAttributesType) (attrmask | GDK_IC_STATUS_FONTSET);
    }
  }

  GdkICPrivate *IC = (GdkICPrivate *)gdk_ic_new(attr, attrmask);

  // If we destroy on-the-spot XIC during the conversion ON mode,
  // kinput2 never turns conversion ON for any other XIC. This seems
  // to be a bug of kinput2.
  // To prevent this problem, we create a dummy XIC here, which 
  // never becomes conversion ON, and is only used to be destroyed after
  // the real active XIC is destroyed in ~nsIMEGtkIC.

  if (mInputStyle & GDK_IM_PREEDIT_CALLBACKS ||
      mInputStyle & GDK_IM_STATUS_CALLBACKS) {
    // don't need to set actuall callbacks for this xic
    mIC_backup = (GdkICPrivate *)gdk_ic_new(attr, attrmask);
  }

  gdk_ic_attr_destroy(attr);

  if (!IC || !((GdkICPrivate *) IC)->xic) {
    return;
  }
  mIC = IC;

  XIC xic = ((GdkICPrivate *) IC)->xic;

  /* set callbacks here */
  if (mInputStyle & GDK_IM_PREEDIT_CALLBACKS) {
    XVaNestedList preedit_attr;

    XIMCallback1 preedit_start_cb;
    XIMCallback1 preedit_draw_cb;
    XIMCallback1 preedit_done_cb;

    preedit_start_cb.client_data = (char *)this;
    preedit_start_cb.callback = preedit_start_cbproc;
    preedit_draw_cb.client_data = (char *)this;
    preedit_draw_cb.callback = preedit_draw_cbproc;
    preedit_done_cb.client_data = (char *)this;
    preedit_done_cb.callback = preedit_done_cbproc;

    preedit_attr =
      XVaCreateNestedList(0,
                          XNPreeditStartCallback, &preedit_start_cb,
                          XNPreeditDrawCallback, &preedit_draw_cb,
                          XNPreeditDoneCallback, &preedit_done_cb,
                          0);
    XSetICValues(xic,
                 XNPreeditAttributes, preedit_attr,
                 0);
    XFree(preedit_attr);
  }

  if (mInputStyle & GDK_IM_STATUS_CALLBACKS) {
    XIMCallback1 status_draw_cb;

    XVaNestedList status_attr;

    status_draw_cb.client_data = (char *)this;
    status_draw_cb.callback = status_draw_cbproc;

    status_attr =
      XVaCreateNestedList(0,
                          XNStatusDrawCallback, &status_draw_cb,
                          0);
    XSetICValues(xic,
                 XNStatusAttributes, status_attr,
                 0);
    XFree(status_attr);

    if (mInputStyle & GDK_IM_STATUS_CALLBACKS) {
      if (!gStatus) {
        gStatus = new nsIMEStatus();
      }
      SetStatusText("");
    }
  }
  return;
}
#endif // USE_XIM 
