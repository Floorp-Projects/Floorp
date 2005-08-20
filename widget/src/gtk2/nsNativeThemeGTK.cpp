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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>  (Original Author)
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

#include "nsNativeThemeGTK.h"
#include "nsThemeConstants.h"
#include "nsDrawingSurfaceGTK.h"
#include "gtkdrawing.h"

#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIEventStateManager.h"
#include "nsIViewManager.h"
#include "nsINameSpaceManager.h"
#include "nsILookAndFeel.h"
#include "nsIDeviceContext.h"
#include "nsTransform2D.h"
#include "prlink.h"

#include <gdk/gdkprivate.h>

#include <gdk/gdkx.h>

NS_IMPL_ISUPPORTS1(nsNativeThemeGTK, nsITheme)

static int gLastXError;

nsNativeThemeGTK::nsNativeThemeGTK()
{
  if (moz_gtk_init() != MOZ_GTK_SUCCESS) {
    memset(mDisabledWidgetTypes, 0xff, sizeof(mDisabledWidgetTypes));
    return;
  }

  mDisabledAtom = do_GetAtom("disabled");
  mCheckedAtom = do_GetAtom("checked");
  mSelectedAtom = do_GetAtom("selected");
  mInputCheckedAtom = do_GetAtom("_moz-input-checked");
  mInputAtom = do_GetAtom("input");
  mFocusedAtom = do_GetAtom("focused");
  mFirstTabAtom = do_GetAtom("first-tab");
  mCurPosAtom = do_GetAtom("curpos");
  mMaxPosAtom = do_GetAtom("maxpos");

  memset(mDisabledWidgetTypes, 0, sizeof(mDisabledWidgetTypes));

  // Look up the symbol for gtk_style_get_prop_experimental
  PRLibrary* gtkLibrary;
  PRFuncPtr stylePropFunc = PR_FindFunctionSymbolAndLibrary("gtk_style_get_prop_experimental", &gtkLibrary);
  if (stylePropFunc) {
    moz_gtk_enable_style_props((style_prop_t) stylePropFunc);
    PR_UnloadLibrary(gtkLibrary);
  }
}

nsNativeThemeGTK::~nsNativeThemeGTK() {
  moz_gtk_shutdown();
}

static void GetPrimaryPresShell(nsIFrame* aFrame, nsIPresShell** aResult)
{
  *aResult = nsnull;

  if (!aFrame)
    return;
 
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  content->GetDocument(getter_AddRefs(doc));
  if (doc)
    doc->GetShellAt(0, aResult); // Addref happens here.
}

static void RefreshWidgetWindow(nsIFrame* aFrame)
{
  nsCOMPtr<nsIPresShell> shell;

  GetPrimaryPresShell(aFrame, getter_AddRefs(shell));
  if (!shell)
    return;

  nsCOMPtr<nsIViewManager> vm;
  shell->GetViewManager(getter_AddRefs(vm));
  if (!vm)
    return;
 
  vm->UpdateAllViews(NS_VMREFRESH_NO_SYNC);
}

static PRInt32 GetContentState(nsIFrame* aFrame)
{
  if (!aFrame)
    return 0;

  nsCOMPtr<nsIPresShell> shell;
  GetPrimaryPresShell(aFrame, getter_AddRefs(shell));
  if (!shell)
    return 0;

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIEventStateManager> esm;
  context->GetEventStateManager(getter_AddRefs(esm));
  PRInt32 flags = 0;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  esm->GetContentState(content, flags);
  return flags;
}

static PRBool CheckBooleanAttr(nsIFrame* aFrame, nsIAtom* aAtom)
{
  if (!aFrame)
    return PR_FALSE;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  nsAutoString attr;
  nsresult res = content->GetAttr(kNameSpaceID_None, aAtom, attr);
  if (res == NS_CONTENT_ATTR_NO_VALUE ||
      (res != NS_CONTENT_ATTR_NOT_THERE && attr.IsEmpty()))
    return PR_TRUE; // This handles the HTML case (an attr with no value is like a true val)
  return attr.EqualsIgnoreCase("true"); // This handles the XUL case.
}

static PRInt32 CheckIntegerAttr(nsIFrame *aFrame, nsIAtom *aAtom)
{
  if (!aFrame)
    return 0;

  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  if (!content)
    return 0;

  nsAutoString attr;
  content->GetAttr(kNameSpaceID_None, aAtom, attr);
  if (attr.IsEmpty())
    return 0;

  PRInt32 error;
  PRInt32 retval = attr.ToInteger(&error);
  return retval;
}

PRBool
nsNativeThemeGTK::IsDisabled(nsIFrame* aFrame)
{
  return CheckBooleanAttr(aFrame, mDisabledAtom);
}
  
static PRBool IsWidgetTypeDisabled(PRUint8* aDisabledVector, PRUint8 aWidgetType) {
  return aDisabledVector[aWidgetType >> 3] & (1 << (aWidgetType & 7));
}

static void SetWidgetTypeDisabled(PRUint8* aDisabledVector, PRUint8 aWidgetType) {
  aDisabledVector[aWidgetType >> 3] |= (1 << (aWidgetType & 7));
}

PRBool
nsNativeThemeGTK::GetGtkWidgetAndState(PRUint8 aWidgetType, nsIFrame* aFrame,
                                       GtkThemeWidgetType& aGtkWidgetType,
                                       GtkWidgetState* aState,
                                       gint* aWidgetFlags)
{
  if (aState) {
    if (!aFrame) {
      // reset the entire struct to zero
      memset(aState, 0, sizeof(GtkWidgetState));
    } else {
      // for dropdown textfields, look at the parent frame (the textbox)
      if (aWidgetType == NS_THEME_DROPDOWN_TEXTFIELD)
        aFrame->GetParent(&aFrame);

      PRInt32 eventState = GetContentState(aFrame);

      aState->active = (eventState & NS_EVENT_STATE_ACTIVE);

      if (aWidgetType == NS_THEME_TEXTFIELD ||
          aWidgetType == NS_THEME_DROPDOWN_TEXTFIELD ||
          aWidgetType == NS_THEME_RADIO_CONTAINER)
        aState->focused = CheckBooleanAttr(aFrame, mFocusedAtom);
      else
        aState->focused = (eventState & NS_EVENT_STATE_FOCUS);

      if (aWidgetType == NS_THEME_SCROLLBAR_THUMB_VERTICAL ||
          aWidgetType == NS_THEME_SCROLLBAR_THUMB_HORIZONTAL) {
        // for scrollbars we need to go up two to go from the thumb to
        // the slider to the actual scrollbar object
        nsIFrame *tmpFrame = aFrame;
        tmpFrame->GetParent(&tmpFrame);
        tmpFrame->GetParent(&tmpFrame);

        aState->curpos = CheckIntegerAttr(tmpFrame, mCurPosAtom);
        aState->maxpos = CheckIntegerAttr(tmpFrame, mMaxPosAtom);
      }

      aState->inHover = (eventState & NS_EVENT_STATE_HOVER);
      aState->disabled = IsDisabled(aFrame);
      aState->isDefault = FALSE; // XXX fix me
      aState->canDefault = FALSE; // XXX fix me
    }
  }

  switch (aWidgetType) {
  case NS_THEME_BUTTON:
  case NS_THEME_TOOLBAR_BUTTON:
    if (aWidgetFlags)
      *aWidgetFlags = (aWidgetType == NS_THEME_BUTTON) ? GTK_RELIEF_NORMAL : GTK_RELIEF_NONE;
    aGtkWidgetType = MOZ_GTK_BUTTON;
    break;
  case NS_THEME_CHECKBOX:
  case NS_THEME_RADIO:
    if (aWidgetFlags) {
      nsIAtom* atom = nsnull;

      if (aFrame) {
        // For XUL checkboxes and radio buttons, the state of the parent
        // determines our state.
        nsCOMPtr<nsIContent> content;
        aFrame->GetContent(getter_AddRefs(content));
        if (content->IsContentOfType(nsIContent::eXUL))
          aFrame->GetParent(&aFrame);
        else {
          nsCOMPtr<nsIAtom> tag;
          content->GetTag(getter_AddRefs(tag));
          if (tag == mInputAtom)
            atom = mInputCheckedAtom;
        }
      }

      if (!atom)
        atom = (aWidgetType == NS_THEME_CHECKBOX) ? mCheckedAtom : mSelectedAtom;
      *aWidgetFlags = CheckBooleanAttr(aFrame, atom);
    }

    aGtkWidgetType = (aWidgetType == NS_THEME_RADIO) ? MOZ_GTK_RADIOBUTTON : MOZ_GTK_CHECKBUTTON;
    break;
  case NS_THEME_SCROLLBAR_BUTTON_UP:
  case NS_THEME_SCROLLBAR_BUTTON_DOWN:
  case NS_THEME_SCROLLBAR_BUTTON_LEFT:
  case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    if (aWidgetFlags)
      *aWidgetFlags = GtkArrowType(aWidgetType - NS_THEME_SCROLLBAR_BUTTON_UP);
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_BUTTON;
    break;
  case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_TRACK_VERTICAL;
    break;
  case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL;
    break;
  case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_THUMB_VERTICAL;
    break;
  case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL;
    break;
  case NS_THEME_TOOLBAR_GRIPPER:
    aGtkWidgetType = MOZ_GTK_GRIPPER;
    break;
  case NS_THEME_DROPDOWN_TEXTFIELD:
  case NS_THEME_TEXTFIELD:
    aGtkWidgetType = MOZ_GTK_ENTRY;
    break;
  case NS_THEME_DROPDOWN_BUTTON:
    aGtkWidgetType = MOZ_GTK_DROPDOWN_ARROW;
    break;
  case NS_THEME_CHECKBOX_CONTAINER:
    aGtkWidgetType = MOZ_GTK_CHECKBUTTON_CONTAINER;
    break;
  case NS_THEME_RADIO_CONTAINER:
    aGtkWidgetType = MOZ_GTK_RADIOBUTTON_CONTAINER;
    break;
  case NS_THEME_TOOLBOX:
    aGtkWidgetType = MOZ_GTK_TOOLBAR;
    break;
  case NS_THEME_TOOLTIP:
    aGtkWidgetType = MOZ_GTK_TOOLTIP;
    break;
  case NS_THEME_STATUSBAR_PANEL:
    aGtkWidgetType = MOZ_GTK_FRAME;
    break;
  case NS_THEME_PROGRESSBAR:
  case NS_THEME_PROGRESSBAR_VERTICAL:
    aGtkWidgetType = MOZ_GTK_PROGRESSBAR;
    break;
  case NS_THEME_PROGRESSBAR_CHUNK:
  case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    aGtkWidgetType = MOZ_GTK_PROGRESS_CHUNK;
    break;
  case NS_THEME_TAB_PANELS:
    aGtkWidgetType = MOZ_GTK_TABPANELS;
    break;
  case NS_THEME_TAB:
  case NS_THEME_TAB_LEFT_EDGE:
  case NS_THEME_TAB_RIGHT_EDGE:
    {
      if (aWidgetFlags) {
        *aWidgetFlags = 0;

        if (aWidgetType == NS_THEME_TAB &&
            CheckBooleanAttr(aFrame, mSelectedAtom))
          *aWidgetFlags |= MOZ_GTK_TAB_SELECTED;
        else if (aWidgetType == NS_THEME_TAB_LEFT_EDGE)
          *aWidgetFlags |= MOZ_GTK_TAB_BEFORE_SELECTED;

      nsCOMPtr<nsIContent> content;
      aFrame->GetContent(getter_AddRefs(content));
      if (content->HasAttr(kNameSpaceID_None, mFirstTabAtom))
        *aWidgetFlags |= MOZ_GTK_TAB_FIRST;
      }

      aGtkWidgetType = MOZ_GTK_TAB;
    }
    break;
  default:
    return PR_FALSE;
  }

  return PR_TRUE;
}

static int
NativeThemeErrorHandler(Display* dpy, XErrorEvent* error) {
  gLastXError = error->error_code;
  return 0;
}

NS_IMETHODIMP
nsNativeThemeGTK::DrawWidgetBackground(nsIRenderingContext* aContext,
                                       nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aClipRect)
{
  GtkWidgetState state;
  GtkThemeWidgetType gtkWidgetType;
  gint flags;
  if (!GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, &state,
                            &flags))
    return NS_OK;

  nsDrawingSurfaceGTK* surface;
  aContext->GetDrawingSurface((nsDrawingSurface*)&surface);
  GdkWindow* window = (GdkWindow*) surface->GetDrawable();

  nsTransform2D* transformMatrix;
  aContext->GetCurrentTransform(transformMatrix);

  nsRect tr(aRect);
  transformMatrix->TransformCoord(&tr.x, &tr.y, &tr.width, &tr.height);
  GdkRectangle gdk_rect = {tr.x, tr.y, tr.width, tr.height};

  nsRect cr(aClipRect);
  transformMatrix->TransformCoord(&cr.x, &cr.y, &cr.width, &cr.height);
  GdkRectangle gdk_clip = {cr.x, cr.y, cr.width, cr.height};

  NS_ASSERTION(!IsWidgetTypeDisabled(mDisabledWidgetTypes, aWidgetType),
               "Trying to render an unsafe widget!");

  gLastXError = 0;
  XErrorHandler oldHandler = XSetErrorHandler(NativeThemeErrorHandler);

  moz_gtk_widget_paint(gtkWidgetType, window, &gdk_rect, &gdk_clip, &state,
                       flags);

  gdk_flush();
  XSetErrorHandler(oldHandler);

  if (gLastXError) {
#ifdef DEBUG
    printf("GTK theme failed for widget type %d, error was %d, state was "
           "[active=%d,focused=%d,inHover=%d,disabled=%d]\n",
           aWidgetType, gLastXError, state.active, state.focused,
           state.inHover, state.disabled);
#endif
    NS_WARNING("GTK theme failed; disabling unsafe widget");
    SetWidgetTypeDisabled(mDisabledWidgetTypes, aWidgetType);
    // force refresh of the window, because the widget was not
    // successfully drawn it must be redrawn using the default look
    RefreshWidgetWindow(aFrame);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::GetWidgetBorder(nsIDeviceContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType, nsMargin* aResult)
{
  aResult->top = aResult->left = 0;
  switch (aWidgetType) {
  case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
  case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    {
      gint trough_border;
      moz_gtk_get_scrollbar_metrics(nsnull, &trough_border,
                                    nsnull, nsnull, nsnull);
      aResult->top = aResult->left = trough_border;
    }
    break;
  case NS_THEME_TOOLBOX:
    // gtk has no toolbox equivalent.  So, although we map toolbox to
    // gtk's 'toolbar' for purposes of painting the widget background,
    // we don't use the toolbar border for toolbox.
    break;
  default:
    {
      GtkThemeWidgetType gtkWidgetType;
      if (GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, nsnull,
                               nsnull))
        moz_gtk_get_widget_border(gtkWidgetType, &aResult->left,
                                  &aResult->top);
    }
  }

  aResult->right = aResult->left;
  aResult->bottom = aResult->top;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::GetMinimumWidgetSize(nsIRenderingContext* aContext,
                                       nsIFrame* aFrame, PRUint8 aWidgetType,
                                       nsSize* aResult, PRBool* aIsOverridable)
{
  aResult->width = aResult->height = 0;
  *aIsOverridable = PR_TRUE;

  switch (aWidgetType) {
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
      {
        gint slider_width, stepper_size;
        moz_gtk_get_scrollbar_metrics(&slider_width, nsnull, &stepper_size,
                                      nsnull, nsnull);

        aResult->width = slider_width;
        aResult->height = stepper_size;
        *aIsOverridable = PR_FALSE;
      }
      break;
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      {
        gint slider_width, min_slider_size;
        moz_gtk_get_scrollbar_metrics(&slider_width, nsnull, nsnull, nsnull,
                                      &min_slider_size);

        if (aWidgetType == NS_THEME_SCROLLBAR_THUMB_VERTICAL) {
          aResult->width = slider_width;
          aResult->height = min_slider_size;
        } else {
          aResult->width = min_slider_size;
          aResult->height = slider_width;
        }

        *aIsOverridable = PR_FALSE;
      }
      break;
  case NS_THEME_DROPDOWN_BUTTON:
    {
      // First, get the minimum size for the button itself.
      moz_gtk_get_dropdown_arrow_size(&aResult->width, &aResult->height);
      *aIsOverridable = PR_FALSE;
    }
    break;
  case NS_THEME_CHECKBOX:
  case NS_THEME_RADIO:
  case NS_THEME_CHECKBOX_CONTAINER:
  case NS_THEME_RADIO_CONTAINER:
    {
      gint indicator_size, indicator_spacing;
      if (aWidgetType == NS_THEME_CHECKBOX ||
          aWidgetType == NS_THEME_CHECKBOX_CONTAINER)
        moz_gtk_checkbox_get_metrics(&indicator_size, &indicator_spacing);
      else
        moz_gtk_radio_get_metrics(&indicator_size, &indicator_spacing);

      // Hack alert: several themes have indicators larger than the default
      // 10px size, but don't set the indicator size property.  So, leave
      // a little slop room by making the minimum size 14px.

      aResult->width = aResult->height = MAX(indicator_size, 14);
      *aIsOverridable = PR_FALSE;
    }
    break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                     nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
  // Some widget types just never change state.
  if (aWidgetType == NS_THEME_TOOLBOX ||
      aWidgetType == NS_THEME_TOOLBAR ||
      aWidgetType == NS_THEME_STATUSBAR ||
      aWidgetType == NS_THEME_STATUSBAR_PANEL ||
      aWidgetType == NS_THEME_STATUSBAR_RESIZER_PANEL ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK ||
      aWidgetType == NS_THEME_PROGRESSBAR_CHUNK_VERTICAL ||
      aWidgetType == NS_THEME_PROGRESSBAR ||
      aWidgetType == NS_THEME_PROGRESSBAR_VERTICAL ||
      aWidgetType == NS_THEME_TOOLTIP) {
    *aShouldRepaint = PR_FALSE;
    return NS_OK;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = PR_TRUE;
  }
  else {
    // Check the attribute to see if it's relevant.  
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = PR_FALSE;
    if (aAttribute == mDisabledAtom || aAttribute == mCheckedAtom ||
        aAttribute == mSelectedAtom)
      *aShouldRepaint = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::ThemeChanged()
{
  memset(mDisabledWidgetTypes, 0, sizeof(mDisabledWidgetTypes));
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsNativeThemeGTK::ThemeSupportsWidget(nsIPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      PRUint8 aWidgetType)
{
  // Check for specific widgets to see if HTML has overridden the style.
  if (aFrame) {
    // For now don't support HTML.
    nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));
    if (content->IsContentOfType(nsIContent::eHTML))
      return PR_FALSE;
  }

  if (IsWidgetTypeDisabled(mDisabledWidgetTypes, aWidgetType))
    return PR_FALSE;

  switch (aWidgetType) {
  case NS_THEME_BUTTON:
  case NS_THEME_RADIO:
  case NS_THEME_CHECKBOX:
  case NS_THEME_TOOLBOX:
    // case NS_THEME_TOOLBAR:  (not in skin)
  case NS_THEME_TOOLBAR_BUTTON:
  case NS_THEME_TOOLBAR_DUAL_BUTTON: // so we can override the border with 0
    // case NS_THEME_TOOLBAR_DUAL_BUTTON_DROPDOWN:
    // case NS_THEME_TOOLBAR_SEPARATOR:
  case NS_THEME_TOOLBAR_GRIPPER:
  case NS_THEME_STATUSBAR:
  case NS_THEME_STATUSBAR_PANEL:
    // case NS_THEME_RESIZER:  (n/a for gtk)
    // case NS_THEME_LISTBOX:
    // case NS_THEME_LISTBOX_LISTITEM:
    // case NS_THEME_TREEVIEW:
    // case NS_THEME_TREEVIEW_TREEITEM:
    // case NS_THEME_TREEVIEW_TWISTY:
    // case NS_THEME_TREEVIEW_LINE:
    // case NS_THEME_TREEVIEW_HEADER:
    // case NS_THEME_TREEVIEW_HEADER_CELL:
    // case NS_THEME_TREEVIEW_HEADER_SORTARROW:
    // case NS_THEME_TREEVIEW_TWISTY_OPEN:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TAB:
    // case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_LEFT_EDGE:
    case NS_THEME_TAB_RIGHT_EDGE:
    case NS_THEME_TAB_PANELS:
  case NS_THEME_TOOLTIP:
    // case NS_THEME_SPINNER:
    // case NS_THEME_SPINNER_UP_BUTTON:
    // case NS_THEME_SPINNER_DOWN_BUTTON:
    // case NS_THEME_SCROLLBAR:  (n/a for gtk)
  case NS_THEME_SCROLLBAR_BUTTON_UP:
  case NS_THEME_SCROLLBAR_BUTTON_DOWN:
  case NS_THEME_SCROLLBAR_BUTTON_LEFT:
  case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
  case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
  case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
  case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
  case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    // case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:  (n/a for gtk)
    // case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:  (n/a for gtk)
  case NS_THEME_TEXTFIELD:
    // case NS_THEME_TEXTFIELD_CARET:
    // case NS_THEME_DROPDOWN:
  case NS_THEME_DROPDOWN_BUTTON:
    // case NS_THEME_DROPDOWN_TEXT:
  case NS_THEME_DROPDOWN_TEXTFIELD:
    // case NS_THEME_SLIDER:
    // case NS_THEME_SLIDER_THUMB:
    // case NS_THEME_SLIDER_THUMB_START:
    // case NS_THEME_SLIDER_THUMB_END:
    // case NS_THEME_SLIDER_TICK:
  case NS_THEME_CHECKBOX_CONTAINER:
  case NS_THEME_RADIO_CONTAINER:
    // case NS_THEME_WINDOW:
    // case NS_THEME_DIALOG:
    // case NS_THEME_MENU:
    // case NS_THEME_MENUBAR:
    return PR_TRUE;
  }

  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsNativeThemeGTK::WidgetIsContainer(PRUint8 aWidgetType)
{
  // XXXdwh At some point flesh all of this out.
  if (aWidgetType == NS_THEME_DROPDOWN_BUTTON || 
      aWidgetType == NS_THEME_RADIO ||
      aWidgetType == NS_THEME_CHECKBOX)
    return PR_FALSE;
  return PR_TRUE;
}
