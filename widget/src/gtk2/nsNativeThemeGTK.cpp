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
extern "C" {
#include "gtkdrawing.h"
}

#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIEventStateManager.h"
#include "nsINameSpaceManager.h"
#include "nsILookAndFeel.h"
#include "nsIDeviceContext.h"
#include "nsTransform2D.h"

#include <gdk/gdkprivate.h>

NS_IMPL_ISUPPORTS1(nsNativeThemeGTK, nsITheme)

GtkWidget* gButtonWidget = nsnull;
GtkWidget* gCheckboxWidget = nsnull;
GtkWidget* gScrollbarWidget = nsnull;

nsNativeThemeGTK::nsNativeThemeGTK()
  : mProtoWindow(nsnull),
    mProtoLayout(nsnull)
{
  NS_INIT_ISUPPORTS();
  mDisabledAtom = getter_AddRefs(NS_NewAtom("disabled"));
  mCheckedAtom = getter_AddRefs(NS_NewAtom("checked"));
}

nsNativeThemeGTK::~nsNativeThemeGTK() {
}

static void GetPrimaryPresShell(nsIFrame* aFrame, nsIPresShell** aResult)
{
  *aResult = nsnull;

  if (!aFrame)
    return;
 
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  content->GetDocument(*getter_AddRefs(doc));
  if (doc)
    doc->GetShellAt(0, aResult); // Addref happens here.
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

static PRBool HasAttrValue(nsIContent* aContent, nsIAtom* aAtom, const char* aStr)
{
  nsAutoString attr;
  aContent->GetAttr(kNameSpaceID_None, aAtom, attr);
  return attr.EqualsIgnoreCase(aStr);
}

static PRBool CheckBooleanAttr(nsIFrame* aFrame, nsIAtom* aAtom)
{
  if (!aFrame)
    return PR_FALSE;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  nsAutoString attr;
  nsresult res = content->GetAttr(kNameSpaceID_None, aAtom, attr);
  if (res == NS_CONTENT_ATTR_NO_VALUE)
    return PR_TRUE; // This handles the HTML case (an attr with no value is like a true val)
  return attr.EqualsIgnoreCase("true"); // This handles the XUL case.
}

PRBool nsNativeThemeGTK::IsDisabled(nsIFrame* aFrame)
{
  return CheckBooleanAttr(aFrame, mDisabledAtom);
}
  
PRBool nsNativeThemeGTK::IsChecked(nsIFrame* aFrame)
{
  if (!aFrame)
    return NS_OK;
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  nsAutoString checked;
  nsresult res = content->GetAttr(kNameSpaceID_None, mCheckedAtom, checked);
  if (res == NS_CONTENT_ATTR_NO_VALUE)
    return PR_TRUE; // XXXdwh Can the HTML form control's checked property differ
                    // from the checked attribute?  If so, will need an IsContentofType
                    // HTML followed by a QI to nsIDOMHTMLInputElement to grab the prop.
  return checked.EqualsIgnoreCase("true"); // This handles the XUL case.
}

PRBool nsNativeThemeGTK::IsSelected(nsIFrame* aFrame)
{
  return CheckBooleanAttr(aFrame, mSelectedAtom);
}


nsresult
GetSystemColor(PRUint8 aWidgetType, nsILookAndFeel::nsColorID& aColorID)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_TAB: {
      aColorID = nsILookAndFeel::eColor_buttontext;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

nsresult 
GetSystemFont(PRUint8 aWidgetType, nsSystemFontID& aFont)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON:
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_TAB: {
      aFont = eSystemFont_Button;
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

void
nsNativeThemeGTK::GetGtkWidgetState(nsIFrame* aFrame, GtkWidgetState* aState)
{
  if (!aFrame) {
    aState->active = PR_FALSE;
    aState->focused = PR_FALSE;
    aState->inHover = PR_FALSE;
    aState->disabled = PR_FALSE;
    aState->isDefault = PR_FALSE;
    aState->canDefault = PR_FALSE;
  } else {
    PRInt32 eventState = GetContentState(aFrame);
    aState->active = (eventState & NS_EVENT_STATE_ACTIVE);
    aState->focused = (eventState & NS_EVENT_STATE_FOCUS);
    aState->inHover = (eventState & NS_EVENT_STATE_HOVER);
    aState->disabled = IsDisabled(aFrame);
    aState->isDefault = PR_FALSE; // XXX fix me
    aState->canDefault = PR_FALSE; // XXX fix me
  }
}

NS_IMETHODIMP
nsNativeThemeGTK::DrawWidgetBackground(nsIRenderingContext* aContext,
                                       nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aClipRect)
{
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

  switch (aWidgetType) {

  case NS_THEME_BUTTON:
    {
      EnsureButtonWidget();

      GtkWidgetState buttonState;
      GetGtkWidgetState(aFrame, &buttonState);
      
      moz_gtk_button_paint(window, gButtonWidget->style, &gdk_rect, &gdk_clip, &buttonState);
    }
    break;

  case NS_THEME_CHECKBOX:
    {
      EnsureCheckBoxWidget();
      
      if (aFrame) {
        // For XUL checkboxes and radio buttons, the state of the parent
        // determines our state.
        nsCOMPtr<nsIContent> content;
        aFrame->GetContent(getter_AddRefs(content));
        if (content->IsContentOfType(nsIContent::eXUL))
          aFrame->GetParent(&aFrame);
      }
      
      GtkToggleButtonState checkBoxState;
      GetGtkWidgetState(aFrame, (GtkWidgetState*)&checkBoxState);
      checkBoxState.selected = CheckBooleanAttr(aFrame, mCheckedAtom);
      
      printf("paint checkbox: gdk_rect=(%d,%d,%d,%d), gdk_clip=(%d,%d,%d,%d)\n",
             gdk_rect.x, gdk_rect.y, gdk_rect.width, gdk_rect.height,
             gdk_clip.y, gdk_clip.y, gdk_clip.width, gdk_clip.height);
      moz_gtk_checkbox_paint(window, gCheckboxWidget->style, &gdk_rect, &gdk_clip, &checkBoxState);
    }
    break;

  case NS_THEME_SCROLLBAR_BUTTON_UP:
  case NS_THEME_SCROLLBAR_BUTTON_DOWN:
  case NS_THEME_SCROLLBAR_BUTTON_LEFT:
  case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    {
      EnsureScrollbarWidget();
      
      GtkWidgetState buttonState;
      GetGtkWidgetState(aFrame, &buttonState);
      
      GtkArrowType arrowType = GtkArrowType(aWidgetType - NS_THEME_SCROLLBAR_BUTTON_UP);
      
      moz_gtk_scrollbar_button_paint(window, gScrollbarWidget->style, &gdk_rect, &gdk_clip,
                                     &buttonState, arrowType);
    }
    break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::GetWidgetBorder(nsIDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsMargin* aResult)
{
  aResult->top = aResult->bottom = aResult->left = aResult->right = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       nsSize* aResult)
{
  aResult->width = aResult->height = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType, 
                                     nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
  // Some widget types just never change state.
  if (aWidgetType == NS_THEME_TOOLBOX || aWidgetType == NS_THEME_TOOLBAR ||
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_VERTICAL || 
      aWidgetType == NS_THEME_SCROLLBAR_TRACK_HORIZONTAL || 
      aWidgetType == NS_THEME_STATUSBAR || aWidgetType == NS_THEME_STATUSBAR_PANEL ||
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
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsNativeThemeGTK::ThemeSupportsWidget(nsIPresContext* aPresContext,
                                      PRUint8 aWidgetType)
{
  switch (aWidgetType) {
  case NS_THEME_BUTTON:
  case NS_THEME_CHECKBOX:
  case NS_THEME_SCROLLBAR_BUTTON_UP:
  case NS_THEME_SCROLLBAR_BUTTON_DOWN:
  case NS_THEME_SCROLLBAR_BUTTON_LEFT:
  case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
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

void
nsNativeThemeGTK::SetupWidgetPrototype(GtkWidget* widget)
{
  if (!mProtoWindow) {
    mProtoWindow = gtk_window_new(GTK_WINDOW_POPUP);
    mProtoLayout = gtk_fixed_new();
    gtk_container_add(GTK_CONTAINER(mProtoWindow), mProtoLayout);
  }

  gtk_container_add(GTK_CONTAINER(mProtoLayout), widget);
  gtk_widget_set_rc_style(widget);
  gtk_widget_realize(widget);
}

void
nsNativeThemeGTK::EnsureButtonWidget()
{
  if (!gButtonWidget) {
    gButtonWidget = gtk_button_new_with_label("M");
    SetupWidgetPrototype(gButtonWidget);
  }
}

void
nsNativeThemeGTK::EnsureCheckBoxWidget()
{
  if (!gCheckboxWidget) {
    gCheckboxWidget = gtk_check_button_new_with_label("M");
    SetupWidgetPrototype(gCheckboxWidget);
  }
}

void
nsNativeThemeGTK::EnsureScrollbarWidget()
{
  if (!gScrollbarWidget) {
    gScrollbarWidget = gtk_vscrollbar_new(NULL);
    SetupWidgetPrototype(gScrollbarWidget);
  }
}
