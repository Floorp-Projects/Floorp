/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeGTK.h"
#include "nsThemeConstants.h"
#include "gtkdrawing.h"

#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsViewManager.h"
#include "nsNameSpaceManager.h"
#include "nsGfxCIID.h"
#include "nsTransform2D.h"
#include "nsMenuFrame.h"
#include "prlink.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsRenderingContext.h"
#include "nsGkAtoms.h"
#include "mozilla/Services.h"

#include <gdk/gdkprivate.h>
#include <gtk/gtk.h>

#include "gfxContext.h"
#include "gfxPlatformGtk.h"
#include "gfxGdkNativeRenderer.h"
#include <algorithm>

NS_IMPL_ISUPPORTS_INHERITED2(nsNativeThemeGTK, nsNativeTheme, nsITheme,
                                                              nsIObserver)

static int gLastGdkError;

nsNativeThemeGTK::nsNativeThemeGTK()
{
  if (moz_gtk_init() != MOZ_GTK_SUCCESS) {
    memset(mDisabledWidgetTypes, 0xff, sizeof(mDisabledWidgetTypes));
    return;
  }

  // We have to call moz_gtk_shutdown before the event loop stops running.
  nsCOMPtr<nsIObserverService> obsServ =
    mozilla::services::GetObserverService();
  obsServ->AddObserver(this, "xpcom-shutdown", false);

  memset(mDisabledWidgetTypes, 0, sizeof(mDisabledWidgetTypes));
  memset(mSafeWidgetStates, 0, sizeof(mSafeWidgetStates));
}

nsNativeThemeGTK::~nsNativeThemeGTK() {
}

NS_IMETHODIMP
nsNativeThemeGTK::Observe(nsISupports *aSubject, const char *aTopic,
                          const char16_t *aData)
{
  if (!nsCRT::strcmp(aTopic, "xpcom-shutdown")) {
    moz_gtk_shutdown();
  } else {
    NS_NOTREACHED("unexpected topic");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

void
nsNativeThemeGTK::RefreshWidgetWindow(nsIFrame* aFrame)
{
  nsIPresShell *shell = GetPresShell(aFrame);
  if (!shell)
    return;

  nsViewManager* vm = shell->GetViewManager();
  if (!vm)
    return;
 
  vm->InvalidateAllViews();
}

static bool IsFrameContentNodeInNamespace(nsIFrame *aFrame, uint32_t aNamespace)
{
  nsIContent *content = aFrame ? aFrame->GetContent() : nullptr;
  if (!content)
    return false;
  return content->IsInNamespace(aNamespace);
}

static bool IsWidgetTypeDisabled(uint8_t* aDisabledVector, uint8_t aWidgetType) {
  return (aDisabledVector[aWidgetType >> 3] & (1 << (aWidgetType & 7))) != 0;
}

static void SetWidgetTypeDisabled(uint8_t* aDisabledVector, uint8_t aWidgetType) {
  aDisabledVector[aWidgetType >> 3] |= (1 << (aWidgetType & 7));
}

static inline uint16_t
GetWidgetStateKey(uint8_t aWidgetType, GtkWidgetState *aWidgetState)
{
  return (aWidgetState->active |
          aWidgetState->focused << 1 |
          aWidgetState->inHover << 2 |
          aWidgetState->disabled << 3 |
          aWidgetState->isDefault << 4 |
          aWidgetType << 5);
}

static bool IsWidgetStateSafe(uint8_t* aSafeVector,
                                uint8_t aWidgetType,
                                GtkWidgetState *aWidgetState)
{
  uint8_t key = GetWidgetStateKey(aWidgetType, aWidgetState);
  return (aSafeVector[key >> 3] & (1 << (key & 7))) != 0;
}

static void SetWidgetStateSafe(uint8_t *aSafeVector,
                               uint8_t aWidgetType,
                               GtkWidgetState *aWidgetState)
{
  uint8_t key = GetWidgetStateKey(aWidgetType, aWidgetState);
  aSafeVector[key >> 3] |= (1 << (key & 7));
}

static GtkTextDirection GetTextDirection(nsIFrame* aFrame)
{
  if (!aFrame)
    return GTK_TEXT_DIR_NONE;

  switch (aFrame->StyleVisibility()->mDirection) {
    case NS_STYLE_DIRECTION_RTL:
      return GTK_TEXT_DIR_RTL;
    case NS_STYLE_DIRECTION_LTR:
      return GTK_TEXT_DIR_LTR;
  }

  return GTK_TEXT_DIR_NONE;
}

// Returns positive for negative margins (otherwise 0).
gint
nsNativeThemeGTK::GetTabMarginPixels(nsIFrame* aFrame)
{
  nscoord margin =
    IsBottomTab(aFrame) ? aFrame->GetUsedMargin().top
    : aFrame->GetUsedMargin().bottom;

  return std::min<gint>(MOZ_GTK_TAB_MARGIN_MASK,
                std::max(0,
                       aFrame->PresContext()->AppUnitsToDevPixels(-margin)));
}

bool
nsNativeThemeGTK::GetGtkWidgetAndState(uint8_t aWidgetType, nsIFrame* aFrame,
                                       GtkThemeWidgetType& aGtkWidgetType,
                                       GtkWidgetState* aState,
                                       gint* aWidgetFlags)
{
  if (aState) {
    if (!aFrame) {
      // reset the entire struct to zero
      memset(aState, 0, sizeof(GtkWidgetState));
    } else {

      // For XUL checkboxes and radio buttons, the state of the parent
      // determines our state.
      nsIFrame *stateFrame = aFrame;
      if (aFrame && ((aWidgetFlags && (aWidgetType == NS_THEME_CHECKBOX ||
                                       aWidgetType == NS_THEME_RADIO)) ||
                     aWidgetType == NS_THEME_CHECKBOX_LABEL ||
                     aWidgetType == NS_THEME_RADIO_LABEL)) {

        nsIAtom* atom = nullptr;
        if (IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) {
          if (aWidgetType == NS_THEME_CHECKBOX_LABEL ||
              aWidgetType == NS_THEME_RADIO_LABEL) {
            // Adjust stateFrame so GetContentState finds the correct state.
            stateFrame = aFrame = aFrame->GetParent()->GetParent();
          } else {
            // GetContentState knows to look one frame up for radio/checkbox
            // widgets, so don't adjust stateFrame here.
            aFrame = aFrame->GetParent();
          }
          if (aWidgetFlags) {
            if (!atom) {
              atom = (aWidgetType == NS_THEME_CHECKBOX ||
                      aWidgetType == NS_THEME_CHECKBOX_LABEL) ? nsGkAtoms::checked
                                                              : nsGkAtoms::selected;
            }
            *aWidgetFlags = CheckBooleanAttr(aFrame, atom);
          }
        } else {
          if (aWidgetFlags) {
            nsCOMPtr<nsIDOMHTMLInputElement> inputElt(do_QueryInterface(aFrame->GetContent()));
            *aWidgetFlags = 0;
            if (inputElt) {
              bool isHTMLChecked;
              inputElt->GetChecked(&isHTMLChecked);
              if (isHTMLChecked)
                *aWidgetFlags |= MOZ_GTK_WIDGET_CHECKED;
            }

            if (GetIndeterminate(aFrame))
              *aWidgetFlags |= MOZ_GTK_WIDGET_INCONSISTENT;
          }
        }
      } else if (aWidgetType == NS_THEME_TOOLBAR_BUTTON_DROPDOWN ||
                 aWidgetType == NS_THEME_TREEVIEW_HEADER_SORTARROW ||
                 aWidgetType == NS_THEME_BUTTON_ARROW_PREVIOUS ||
                 aWidgetType == NS_THEME_BUTTON_ARROW_NEXT ||
                 aWidgetType == NS_THEME_BUTTON_ARROW_UP ||
                 aWidgetType == NS_THEME_BUTTON_ARROW_DOWN) {
        // The state of an arrow comes from its parent.
        stateFrame = aFrame = aFrame->GetParent();
      }

      nsEventStates eventState = GetContentState(stateFrame, aWidgetType);

      aState->disabled = IsDisabled(aFrame, eventState) || IsReadOnly(aFrame);
      aState->active  = eventState.HasState(NS_EVENT_STATE_ACTIVE);
      aState->focused = eventState.HasState(NS_EVENT_STATE_FOCUS);
      aState->inHover = eventState.HasState(NS_EVENT_STATE_HOVER);
      aState->isDefault = IsDefaultButton(aFrame);
      aState->canDefault = FALSE; // XXX fix me
      aState->depressed = FALSE;

      if (IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) {
        // For these widget types, some element (either a child or parent)
        // actually has element focus, so we check the focused attribute
        // to see whether to draw in the focused state.
        if (aWidgetType == NS_THEME_NUMBER_INPUT ||
            aWidgetType == NS_THEME_TEXTFIELD ||
            aWidgetType == NS_THEME_TEXTFIELD_MULTILINE ||
            aWidgetType == NS_THEME_DROPDOWN_TEXTFIELD ||
            aWidgetType == NS_THEME_SPINNER_TEXTFIELD ||
            aWidgetType == NS_THEME_RADIO_CONTAINER ||
            aWidgetType == NS_THEME_RADIO_LABEL) {
          aState->focused = IsFocused(aFrame);
        } else if (aWidgetType == NS_THEME_RADIO ||
                   aWidgetType == NS_THEME_CHECKBOX) {
          // In XUL, checkboxes and radios shouldn't have focus rings, their labels do
          aState->focused = FALSE;
        }

        if (aWidgetType == NS_THEME_SCROLLBAR_THUMB_VERTICAL ||
            aWidgetType == NS_THEME_SCROLLBAR_THUMB_HORIZONTAL) {
          // for scrollbars we need to go up two to go from the thumb to
          // the slider to the actual scrollbar object
          nsIFrame *tmpFrame = aFrame->GetParent()->GetParent();

          aState->curpos = CheckIntAttr(tmpFrame, nsGkAtoms::curpos, 0);
          aState->maxpos = CheckIntAttr(tmpFrame, nsGkAtoms::maxpos, 100);
        }

        if (aWidgetType == NS_THEME_SCROLLBAR_BUTTON_UP ||
            aWidgetType == NS_THEME_SCROLLBAR_BUTTON_DOWN ||
            aWidgetType == NS_THEME_SCROLLBAR_BUTTON_LEFT ||
            aWidgetType == NS_THEME_SCROLLBAR_BUTTON_RIGHT) {
          // set the state to disabled when the scrollbar is scrolled to
          // the beginning or the end, depending on the button type.
          int32_t curpos = CheckIntAttr(aFrame, nsGkAtoms::curpos, 0);
          int32_t maxpos = CheckIntAttr(aFrame, nsGkAtoms::maxpos, 100);
          if ((curpos == 0 && (aWidgetType == NS_THEME_SCROLLBAR_BUTTON_UP ||
                aWidgetType == NS_THEME_SCROLLBAR_BUTTON_LEFT)) ||
              (curpos == maxpos &&
               (aWidgetType == NS_THEME_SCROLLBAR_BUTTON_DOWN ||
                aWidgetType == NS_THEME_SCROLLBAR_BUTTON_RIGHT)))
            aState->disabled = true;

          // In order to simulate native GTK scrollbar click behavior,
          // we set the active attribute on the element to true if it's
          // pressed with any mouse button.
          // This allows us to show that it's active without setting :active
          else if (CheckBooleanAttr(aFrame, nsGkAtoms::active))
            aState->active = true;

          if (aWidgetFlags) {
            *aWidgetFlags = GetScrollbarButtonType(aFrame);
            if (aWidgetType - NS_THEME_SCROLLBAR_BUTTON_UP < 2)
              *aWidgetFlags |= MOZ_GTK_STEPPER_VERTICAL;
          }
        }

        // menu item state is determined by the attribute "_moz-menuactive",
        // and not by the mouse hovering (accessibility).  as a special case,
        // menus which are children of a menu bar are only marked as prelight
        // if they are open, not on normal hover.

        if (aWidgetType == NS_THEME_MENUITEM ||
            aWidgetType == NS_THEME_CHECKMENUITEM ||
            aWidgetType == NS_THEME_RADIOMENUITEM ||
            aWidgetType == NS_THEME_MENUSEPARATOR ||
            aWidgetType == NS_THEME_MENUARROW) {
          bool isTopLevel = false;
          nsMenuFrame *menuFrame = do_QueryFrame(aFrame);
          if (menuFrame) {
            isTopLevel = menuFrame->IsOnMenuBar();
          }

          if (isTopLevel) {
            aState->inHover = menuFrame->IsOpen();
            *aWidgetFlags |= MOZ_TOPLEVEL_MENU_ITEM;
          } else {
            aState->inHover = CheckBooleanAttr(aFrame, nsGkAtoms::menuactive);
            *aWidgetFlags &= ~MOZ_TOPLEVEL_MENU_ITEM;
          }

          aState->active = FALSE;
        
          if (aWidgetType == NS_THEME_CHECKMENUITEM ||
              aWidgetType == NS_THEME_RADIOMENUITEM) {
            *aWidgetFlags = 0;
            if (aFrame && aFrame->GetContent()) {
              *aWidgetFlags = aFrame->GetContent()->
                AttrValueIs(kNameSpaceID_None, nsGkAtoms::checked,
                            nsGkAtoms::_true, eIgnoreCase);
            }
          }
        }

        // A button with drop down menu open or an activated toggle button
        // should always appear depressed.
        if (aWidgetType == NS_THEME_BUTTON ||
            aWidgetType == NS_THEME_TOOLBAR_BUTTON ||
            aWidgetType == NS_THEME_TOOLBAR_DUAL_BUTTON ||
            aWidgetType == NS_THEME_TOOLBAR_BUTTON_DROPDOWN ||
            aWidgetType == NS_THEME_DROPDOWN ||
            aWidgetType == NS_THEME_DROPDOWN_BUTTON) {
          bool menuOpen = IsOpenButton(aFrame);
          aState->depressed = IsCheckedButton(aFrame) || menuOpen;
          // we must not highlight buttons with open drop down menus on hover.
          aState->inHover = aState->inHover && !menuOpen;
        }

        // When the input field of the drop down button has focus, some themes
        // should draw focus for the drop down button as well.
        if (aWidgetType == NS_THEME_DROPDOWN_BUTTON && aWidgetFlags) {
          *aWidgetFlags = CheckBooleanAttr(aFrame, nsGkAtoms::parentfocused);
        }
      }
    }
  }

  switch (aWidgetType) {
  case NS_THEME_BUTTON:
  case NS_THEME_TOOLBAR_BUTTON:
  case NS_THEME_TOOLBAR_DUAL_BUTTON:
    if (aWidgetFlags)
      *aWidgetFlags = (aWidgetType == NS_THEME_BUTTON) ? GTK_RELIEF_NORMAL : GTK_RELIEF_NONE;
    aGtkWidgetType = MOZ_GTK_BUTTON;
    break;
  case NS_THEME_CHECKBOX:
  case NS_THEME_RADIO:
    aGtkWidgetType = (aWidgetType == NS_THEME_RADIO) ? MOZ_GTK_RADIOBUTTON : MOZ_GTK_CHECKBUTTON;
    break;
  case NS_THEME_SCROLLBAR_BUTTON_UP:
  case NS_THEME_SCROLLBAR_BUTTON_DOWN:
  case NS_THEME_SCROLLBAR_BUTTON_LEFT:
  case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
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
  case NS_THEME_SPINNER:
    aGtkWidgetType = MOZ_GTK_SPINBUTTON;
    break;
  case NS_THEME_SPINNER_UP_BUTTON:
    aGtkWidgetType = MOZ_GTK_SPINBUTTON_UP;
    break;
  case NS_THEME_SPINNER_DOWN_BUTTON:
    aGtkWidgetType = MOZ_GTK_SPINBUTTON_DOWN;
    break;
  case NS_THEME_SPINNER_TEXTFIELD:
    aGtkWidgetType = MOZ_GTK_SPINBUTTON_ENTRY;
    break;
  case NS_THEME_RANGE:
    {
      if (IsRangeHorizontal(aFrame)) {
        if (aWidgetFlags)
          *aWidgetFlags = GTK_ORIENTATION_HORIZONTAL;
        aGtkWidgetType = MOZ_GTK_SCALE_HORIZONTAL;
      } else {
        if (aWidgetFlags)
          *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
        aGtkWidgetType = MOZ_GTK_SCALE_VERTICAL;
      }
      break;
    }
  case NS_THEME_RANGE_THUMB:
    {
      if (IsRangeHorizontal(aFrame)) {
        if (aWidgetFlags)
          *aWidgetFlags = GTK_ORIENTATION_HORIZONTAL;
        aGtkWidgetType = MOZ_GTK_SCALE_THUMB_HORIZONTAL;
      } else {
        if (aWidgetFlags)
          *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
        aGtkWidgetType = MOZ_GTK_SCALE_THUMB_VERTICAL;
      }
      break;
    }
  case NS_THEME_SCALE_HORIZONTAL:
    if (aWidgetFlags)
      *aWidgetFlags = GTK_ORIENTATION_HORIZONTAL;
    aGtkWidgetType = MOZ_GTK_SCALE_HORIZONTAL;
    break;
  case NS_THEME_SCALE_THUMB_HORIZONTAL:
    if (aWidgetFlags)
      *aWidgetFlags = GTK_ORIENTATION_HORIZONTAL;
    aGtkWidgetType = MOZ_GTK_SCALE_THUMB_HORIZONTAL;
    break;
  case NS_THEME_SCALE_VERTICAL:
    if (aWidgetFlags)
      *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
    aGtkWidgetType = MOZ_GTK_SCALE_VERTICAL;
    break;
  case NS_THEME_TOOLBAR_SEPARATOR:
    aGtkWidgetType = MOZ_GTK_TOOLBAR_SEPARATOR;
    break;
  case NS_THEME_SCALE_THUMB_VERTICAL:
    if (aWidgetFlags)
      *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
    aGtkWidgetType = MOZ_GTK_SCALE_THUMB_VERTICAL;
    break;
  case NS_THEME_TOOLBAR_GRIPPER:
    aGtkWidgetType = MOZ_GTK_GRIPPER;
    break;
  case NS_THEME_RESIZER:
    aGtkWidgetType = MOZ_GTK_RESIZER;
    break;
  case NS_THEME_NUMBER_INPUT:
  case NS_THEME_TEXTFIELD:
  case NS_THEME_TEXTFIELD_MULTILINE:
    aGtkWidgetType = MOZ_GTK_ENTRY;
    break;
  case NS_THEME_LISTBOX:
  case NS_THEME_TREEVIEW:
    aGtkWidgetType = MOZ_GTK_TREEVIEW;
    break;
  case NS_THEME_TREEVIEW_HEADER_CELL:
    if (aWidgetFlags) {
      // In this case, the flag denotes whether the header is the sorted one or not
      if (GetTreeSortDirection(aFrame) == eTreeSortDirection_Natural)
        *aWidgetFlags = false;
      else
        *aWidgetFlags = true;
    }
    aGtkWidgetType = MOZ_GTK_TREE_HEADER_CELL;
    break;
  case NS_THEME_TREEVIEW_HEADER_SORTARROW:
    if (aWidgetFlags) {
      switch (GetTreeSortDirection(aFrame)) {
        case eTreeSortDirection_Ascending:
          *aWidgetFlags = GTK_ARROW_DOWN;
          break;
        case eTreeSortDirection_Descending:
          *aWidgetFlags = GTK_ARROW_UP;
          break;
        case eTreeSortDirection_Natural:
        default:
          /* This prevents the treecolums from getting smaller
           * and wider when switching sort direction off and on
           * */
          *aWidgetFlags = GTK_ARROW_NONE;
          break;
      }
    }
    aGtkWidgetType = MOZ_GTK_TREE_HEADER_SORTARROW;
    break;
  case NS_THEME_TREEVIEW_TWISTY:
    aGtkWidgetType = MOZ_GTK_TREEVIEW_EXPANDER;
    if (aWidgetFlags)
      *aWidgetFlags = GTK_EXPANDER_COLLAPSED;
    break;
  case NS_THEME_TREEVIEW_TWISTY_OPEN:
    aGtkWidgetType = MOZ_GTK_TREEVIEW_EXPANDER;
    if (aWidgetFlags)
      *aWidgetFlags = GTK_EXPANDER_EXPANDED;
    break;
  case NS_THEME_DROPDOWN:
    aGtkWidgetType = MOZ_GTK_DROPDOWN;
    if (aWidgetFlags)
        *aWidgetFlags = IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XHTML);
    break;
  case NS_THEME_DROPDOWN_TEXT:
    return false; // nothing to do, but prevents the bg from being drawn
  case NS_THEME_DROPDOWN_TEXTFIELD:
    aGtkWidgetType = MOZ_GTK_DROPDOWN_ENTRY;
    break;
  case NS_THEME_DROPDOWN_BUTTON:
    aGtkWidgetType = MOZ_GTK_DROPDOWN_ARROW;
    break;
  case NS_THEME_TOOLBAR_BUTTON_DROPDOWN:
  case NS_THEME_BUTTON_ARROW_DOWN:
  case NS_THEME_BUTTON_ARROW_UP:
  case NS_THEME_BUTTON_ARROW_NEXT:
  case NS_THEME_BUTTON_ARROW_PREVIOUS:
    aGtkWidgetType = MOZ_GTK_TOOLBARBUTTON_ARROW;
    if (aWidgetFlags) {
      *aWidgetFlags = GTK_ARROW_DOWN;

      if (aWidgetType == NS_THEME_BUTTON_ARROW_UP)
        *aWidgetFlags = GTK_ARROW_UP;
      else if (aWidgetType == NS_THEME_BUTTON_ARROW_NEXT)
        *aWidgetFlags = GTK_ARROW_RIGHT;
      else if (aWidgetType == NS_THEME_BUTTON_ARROW_PREVIOUS)
        *aWidgetFlags = GTK_ARROW_LEFT;
    }
    break;
  case NS_THEME_CHECKBOX_CONTAINER:
    aGtkWidgetType = MOZ_GTK_CHECKBUTTON_CONTAINER;
    break;
  case NS_THEME_RADIO_CONTAINER:
    aGtkWidgetType = MOZ_GTK_RADIOBUTTON_CONTAINER;
    break;
  case NS_THEME_CHECKBOX_LABEL:
    aGtkWidgetType = MOZ_GTK_CHECKBUTTON_LABEL;
    break;
  case NS_THEME_RADIO_LABEL:
    aGtkWidgetType = MOZ_GTK_RADIOBUTTON_LABEL;
    break;
  case NS_THEME_TOOLBAR:
    aGtkWidgetType = MOZ_GTK_TOOLBAR;
    break;
  case NS_THEME_TOOLTIP:
    aGtkWidgetType = MOZ_GTK_TOOLTIP;
    break;
  case NS_THEME_STATUSBAR_PANEL:
  case NS_THEME_STATUSBAR_RESIZER_PANEL:
    aGtkWidgetType = MOZ_GTK_FRAME;
    break;
  case NS_THEME_PROGRESSBAR:
  case NS_THEME_PROGRESSBAR_VERTICAL:
    aGtkWidgetType = MOZ_GTK_PROGRESSBAR;
    break;
  case NS_THEME_PROGRESSBAR_CHUNK:
  case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    {
      nsIFrame* stateFrame = aFrame->GetParent();
      nsEventStates eventStates = GetContentState(stateFrame, aWidgetType);

      aGtkWidgetType = IsIndeterminateProgress(stateFrame, eventStates)
                         ? (stateFrame->StyleDisplay()->mOrient == NS_STYLE_ORIENT_VERTICAL)
                           ? MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE
                           : MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE
                         : MOZ_GTK_PROGRESS_CHUNK;
    }
    break;
  case NS_THEME_TAB_SCROLLARROW_BACK:
  case NS_THEME_TAB_SCROLLARROW_FORWARD:
    if (aWidgetFlags)
      *aWidgetFlags = aWidgetType == NS_THEME_TAB_SCROLLARROW_BACK ?
                        GTK_ARROW_LEFT : GTK_ARROW_RIGHT;
    aGtkWidgetType = MOZ_GTK_TAB_SCROLLARROW;
    break;
  case NS_THEME_TAB_PANELS:
    aGtkWidgetType = MOZ_GTK_TABPANELS;
    break;
  case NS_THEME_TAB:
    {
      if (aWidgetFlags) {
        /* First bits will be used to store max(0,-bmargin) where bmargin
         * is the bottom margin of the tab in pixels  (resp. top margin,
         * for bottom tabs). */
        if (IsBottomTab(aFrame)) {
            *aWidgetFlags = MOZ_GTK_TAB_BOTTOM;
        } else {
            *aWidgetFlags = 0;
        }

        *aWidgetFlags |= GetTabMarginPixels(aFrame);

        if (IsSelectedTab(aFrame))
          *aWidgetFlags |= MOZ_GTK_TAB_SELECTED;

        if (IsFirstTab(aFrame))
          *aWidgetFlags |= MOZ_GTK_TAB_FIRST;
      }

      aGtkWidgetType = MOZ_GTK_TAB;
    }
    break;
  case NS_THEME_SPLITTER:
    if (IsHorizontal(aFrame))
      aGtkWidgetType = MOZ_GTK_SPLITTER_VERTICAL;
    else 
      aGtkWidgetType = MOZ_GTK_SPLITTER_HORIZONTAL;
    break;
  case NS_THEME_MENUBAR:
    aGtkWidgetType = MOZ_GTK_MENUBAR;
    break;
  case NS_THEME_MENUPOPUP:
    aGtkWidgetType = MOZ_GTK_MENUPOPUP;
    break;
  case NS_THEME_MENUITEM:
    aGtkWidgetType = MOZ_GTK_MENUITEM;
    break;
  case NS_THEME_MENUSEPARATOR:
    aGtkWidgetType = MOZ_GTK_MENUSEPARATOR;
    break;
  case NS_THEME_MENUARROW:
    aGtkWidgetType = MOZ_GTK_MENUARROW;
    break;
  case NS_THEME_CHECKMENUITEM:
    aGtkWidgetType = MOZ_GTK_CHECKMENUITEM;
    break;
  case NS_THEME_RADIOMENUITEM:
    aGtkWidgetType = MOZ_GTK_RADIOMENUITEM;
    break;
  case NS_THEME_WINDOW:
  case NS_THEME_DIALOG:
    aGtkWidgetType = MOZ_GTK_WINDOW;
    break;
  default:
    return false;
  }

  return true;
}

#if (MOZ_WIDGET_GTK == 2)
class ThemeRenderer : public gfxGdkNativeRenderer {
public:
  ThemeRenderer(GtkWidgetState aState, GtkThemeWidgetType aGTKWidgetType,
                gint aFlags, GtkTextDirection aDirection,
                const GdkRectangle& aGDKRect, const GdkRectangle& aGDKClip)
    : mState(aState), mGTKWidgetType(aGTKWidgetType), mFlags(aFlags),
      mDirection(aDirection), mGDKRect(aGDKRect), mGDKClip(aGDKClip) {}
  nsresult DrawWithGDK(GdkDrawable * drawable, gint offsetX, gint offsetY,
                       GdkRectangle * clipRects, uint32_t numClipRects);
private:
  GtkWidgetState mState;
  GtkThemeWidgetType mGTKWidgetType;
  gint mFlags;
  GtkTextDirection mDirection;
  const GdkRectangle& mGDKRect;
  const GdkRectangle& mGDKClip;
};

nsresult
ThemeRenderer::DrawWithGDK(GdkDrawable * drawable, gint offsetX, 
        gint offsetY, GdkRectangle * clipRects, uint32_t numClipRects)
{
  GdkRectangle gdk_rect = mGDKRect;
  gdk_rect.x += offsetX;
  gdk_rect.y += offsetY;

  GdkRectangle gdk_clip = mGDKClip;
  gdk_clip.x += offsetX;
  gdk_clip.y += offsetY;

  GdkRectangle surfaceRect;
  surfaceRect.x = 0;
  surfaceRect.y = 0;
  gdk_drawable_get_size(drawable, &surfaceRect.width, &surfaceRect.height);
  gdk_rectangle_intersect(&gdk_clip, &surfaceRect, &gdk_clip);
  
  NS_ASSERTION(numClipRects == 0, "We don't support clipping!!!");
  moz_gtk_widget_paint(mGTKWidgetType, drawable, &gdk_rect, &gdk_clip,
                       &mState, mFlags, mDirection);

  return NS_OK;
}
#endif

bool
nsNativeThemeGTK::GetExtraSizeForWidget(nsIFrame* aFrame, uint8_t aWidgetType,
                                        nsIntMargin* aExtra)
{
  *aExtra = nsIntMargin(0,0,0,0);
  // Allow an extra one pixel above and below the thumb for certain
  // GTK2 themes (Ximian Industrial, Bluecurve, Misty, at least);
  // We modify the frame's overflow area.  See bug 297508.
  switch (aWidgetType) {
  case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    aExtra->top = aExtra->bottom = 1;
    return true;
  case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    aExtra->left = aExtra->right = 1;
    return true;

  // Include the indicator spacing (the padding around the control).
  case NS_THEME_CHECKBOX:
  case NS_THEME_RADIO:
    {
      gint indicator_size, indicator_spacing;

      if (aWidgetType == NS_THEME_CHECKBOX) {
        moz_gtk_checkbox_get_metrics(&indicator_size, &indicator_spacing);
      } else {
        moz_gtk_radio_get_metrics(&indicator_size, &indicator_spacing);
      }

      aExtra->top = indicator_spacing;
      aExtra->right = indicator_spacing;
      aExtra->bottom = indicator_spacing;
      aExtra->left = indicator_spacing;
      return true;
    }
  case NS_THEME_BUTTON :
    {
      if (IsDefaultButton(aFrame)) {
        // Some themes draw a default indicator outside the widget,
        // include that in overflow
        gint top, left, bottom, right;
        moz_gtk_button_get_default_overflow(&top, &left, &bottom, &right);
        aExtra->top = top;
        aExtra->right = right;
        aExtra->bottom = bottom;
        aExtra->left = left;
        return true;
      }
    }
  case NS_THEME_TAB :
    {
      if (!IsSelectedTab(aFrame))
        return false;

      gint gap_height = moz_gtk_get_tab_thickness();

      int32_t extra = gap_height - GetTabMarginPixels(aFrame);
      if (extra <= 0)
        return false;

      if (IsBottomTab(aFrame)) {
        aExtra->top = extra;
      } else {
        aExtra->bottom = extra;
      }
    }
  default:
    return false;
  }
}

NS_IMETHODIMP
nsNativeThemeGTK::DrawWidgetBackground(nsRenderingContext* aContext,
                                       nsIFrame* aFrame,
                                       uint8_t aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aDirtyRect)
{
  GtkWidgetState state;
  GtkThemeWidgetType gtkWidgetType;
  GtkTextDirection direction = GetTextDirection(aFrame);
  gint flags;
  if (!GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, &state,
                            &flags))
    return NS_OK;

  gfxContext* ctx = aContext->ThebesContext();
  nsPresContext *presContext = aFrame->PresContext();

  gfxRect rect = presContext->AppUnitsToGfxUnits(aRect);
  gfxRect dirtyRect = presContext->AppUnitsToGfxUnits(aDirtyRect);

  // Align to device pixels where sensible
  // to provide crisper and faster drawing.
  // Don't snap if it's a non-unit scale factor. We're going to have to take
  // slow paths then in any case.
  bool snapXY = ctx->UserToDevicePixelSnapped(rect);
  if (snapXY) {
    // Leave rect in device coords but make dirtyRect consistent.
    dirtyRect = ctx->UserToDevice(dirtyRect);
  }

  // Translate the dirty rect so that it is wrt the widget top-left.
  dirtyRect.MoveBy(-rect.TopLeft());
  // Round out the dirty rect to gdk pixels to ensure that gtk draws
  // enough pixels for interpolation to device pixels.
  dirtyRect.RoundOut();

  // GTK themes can only draw an integer number of pixels
  // (even when not snapped).
  nsIntRect widgetRect(0, 0, NS_lround(rect.Width()), NS_lround(rect.Height()));
  nsIntRect overflowRect(widgetRect);
  nsIntMargin extraSize;
  if (GetExtraSizeForWidget(aFrame, aWidgetType, &extraSize)) {
    overflowRect.Inflate(extraSize);
  }

  // This is the rectangle that will actually be drawn, in gdk pixels
  nsIntRect drawingRect(int32_t(dirtyRect.X()),
                        int32_t(dirtyRect.Y()),
                        int32_t(dirtyRect.Width()),
                        int32_t(dirtyRect.Height()));
  if (widgetRect.IsEmpty()
      || !drawingRect.IntersectRect(overflowRect, drawingRect))
    return NS_OK;

  // gdk rectangles are wrt the drawing rect.

  GdkRectangle gdk_rect = {-drawingRect.x, -drawingRect.y,
                           widgetRect.width, widgetRect.height};

  // translate everything so (0,0) is the top left of the drawingRect
  gfxContextAutoSaveRestore autoSR(ctx);
  if (snapXY) {
    // Rects are in device coords.
    ctx->IdentityMatrix(); 
  }
  ctx->Translate(rect.TopLeft() + gfxPoint(drawingRect.x, drawingRect.y));

  NS_ASSERTION(!IsWidgetTypeDisabled(mDisabledWidgetTypes, aWidgetType),
               "Trying to render an unsafe widget!");

  bool safeState = IsWidgetStateSafe(mSafeWidgetStates, aWidgetType, &state);
  if (!safeState) {
    gLastGdkError = 0;
    gdk_error_trap_push ();
  }

#if (MOZ_WIDGET_GTK == 2)
  // The gdk_clip is just advisory here, meaning "you don't
  // need to draw outside this rect if you don't feel like it!"
  GdkRectangle gdk_clip = {0, 0, drawingRect.width, drawingRect.height};

  ThemeRenderer renderer(state, gtkWidgetType, flags, direction,
                         gdk_rect, gdk_clip);

  // Some themes (e.g. Clearlooks) just don't clip properly to any
  // clip rect we provide, so we cannot advertise support for clipping within
  // the widget bounds.
  uint32_t rendererFlags = 0;
  if (GetWidgetTransparency(aFrame, aWidgetType) == eOpaque) {
    rendererFlags |= gfxGdkNativeRenderer::DRAW_IS_OPAQUE;
  }

  // GtkStyles (used by the widget drawing backend) are created for a
  // particular colormap/visual.
  GdkColormap* colormap = moz_gtk_widget_get_colormap();

  renderer.Draw(ctx, drawingRect.Size(), rendererFlags, colormap);
#else 
  moz_gtk_widget_paint(gtkWidgetType, ctx->GetCairo(), &gdk_rect, 
                       &state, flags, direction);
#endif

  if (!safeState) {
    gdk_flush();
    gLastGdkError = gdk_error_trap_pop ();

    if (gLastGdkError) {
#ifdef DEBUG
      printf("GTK theme failed for widget type %d, error was %d, state was "
             "[active=%d,focused=%d,inHover=%d,disabled=%d]\n",
             aWidgetType, gLastGdkError, state.active, state.focused,
             state.inHover, state.disabled);
#endif
      NS_WARNING("GTK theme failed; disabling unsafe widget");
      SetWidgetTypeDisabled(mDisabledWidgetTypes, aWidgetType);
      // force refresh of the window, because the widget was not
      // successfully drawn it must be redrawn using the default look
      RefreshWidgetWindow(aFrame);
    } else {
      SetWidgetStateSafe(mSafeWidgetStates, aWidgetType, &state);
    }
  }

  // Indeterminate progress bar are animated.
  if (gtkWidgetType == MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE ||
      gtkWidgetType == MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE) {
    if (!QueueAnimatedContentForRefresh(aFrame->GetContent(), 30)) {
      NS_WARNING("unable to animate widget!");
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::GetWidgetBorder(nsDeviceContext* aContext, nsIFrame* aFrame,
                                  uint8_t aWidgetType, nsIntMargin* aResult)
{
  GtkTextDirection direction = GetTextDirection(aFrame);
  aResult->top = aResult->left = aResult->right = aResult->bottom = 0;
  switch (aWidgetType) {
  case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
  case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    {
      MozGtkScrollbarMetrics metrics;
      moz_gtk_get_scrollbar_metrics(&metrics);
      aResult->top = aResult->left = aResult->right = aResult->bottom = metrics.trough_border;
    }
    break;
  case NS_THEME_TOOLBOX:
    // gtk has no toolbox equivalent.  So, although we map toolbox to
    // gtk's 'toolbar' for purposes of painting the widget background,
    // we don't use the toolbar border for toolbox.
    break;
  case NS_THEME_TOOLBAR_DUAL_BUTTON:
    // TOOLBAR_DUAL_BUTTON is an interesting case.  We want a border to draw
    // around the entire button + dropdown, and also an inner border if you're
    // over the button part.  But, we want the inner button to be right up
    // against the edge of the outer button so that the borders overlap.
    // To make this happen, we draw a button border for the outer button,
    // but don't reserve any space for it.
    break;
  case NS_THEME_TAB:
    // Top tabs have no bottom border, bottom tabs have no top border
    moz_gtk_get_widget_border(MOZ_GTK_TAB, &aResult->left, &aResult->top,
                              &aResult->right, &aResult->bottom, direction,
                              FALSE);
    if (IsBottomTab(aFrame))
        aResult->top = 0;
    else
        aResult->bottom = 0;
    break;
  case NS_THEME_MENUITEM:
  case NS_THEME_CHECKMENUITEM:
  case NS_THEME_RADIOMENUITEM:
    // For regular menuitems, we will be using GetWidgetPadding instead of
    // GetWidgetBorder to pad up the widget's internals; other menuitems
    // will need to fall through and use the default case as before.
    if (IsRegularMenuItem(aFrame))
      break;
  default:
    {
      GtkThemeWidgetType gtkWidgetType;
      if (GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, nullptr,
                               nullptr)) {
        moz_gtk_get_widget_border(gtkWidgetType, &aResult->left, &aResult->top,
                                  &aResult->right, &aResult->bottom, direction,
                                  IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XHTML));
      }
    }
  }
  return NS_OK;
}

bool
nsNativeThemeGTK::GetWidgetPadding(nsDeviceContext* aContext,
                                   nsIFrame* aFrame, uint8_t aWidgetType,
                                   nsIntMargin* aResult)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON_FOCUS:
    case NS_THEME_TOOLBAR_BUTTON:
    case NS_THEME_TOOLBAR_DUAL_BUTTON:
    case NS_THEME_TAB_SCROLLARROW_BACK:
    case NS_THEME_TAB_SCROLLARROW_FORWARD:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_TOOLBAR_BUTTON_DROPDOWN:
    case NS_THEME_BUTTON_ARROW_UP:
    case NS_THEME_BUTTON_ARROW_DOWN:
    case NS_THEME_BUTTON_ARROW_NEXT:
    case NS_THEME_BUTTON_ARROW_PREVIOUS:
    case NS_THEME_RANGE_THUMB:
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case NS_THEME_CHECKBOX:
    case NS_THEME_RADIO:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
    case NS_THEME_MENUITEM:
    case NS_THEME_CHECKMENUITEM:
    case NS_THEME_RADIOMENUITEM:
      {
        // Menubar and menulist have their padding specified in CSS.
        if (!IsRegularMenuItem(aFrame))
          return false;

        aResult->SizeTo(0, 0, 0, 0);
        GtkThemeWidgetType gtkWidgetType;
        if (GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, nullptr,
                                 nullptr)) {
          moz_gtk_get_widget_border(gtkWidgetType, &aResult->left, &aResult->top,
                                    &aResult->right, &aResult->bottom, GetTextDirection(aFrame),
                                    IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XHTML));
        }

        gint horizontal_padding;

        if (aWidgetType == NS_THEME_MENUITEM)
          moz_gtk_menuitem_get_horizontal_padding(&horizontal_padding);
        else
          moz_gtk_checkmenuitem_get_horizontal_padding(&horizontal_padding);

        aResult->left += horizontal_padding;
        aResult->right += horizontal_padding;

        return true;
      }
  }

  return false;
}

bool
nsNativeThemeGTK::GetWidgetOverflow(nsDeviceContext* aContext,
                                    nsIFrame* aFrame, uint8_t aWidgetType,
                                    nsRect* aOverflowRect)
{
  nsIntMargin extraSize;
  if (!GetExtraSizeForWidget(aFrame, aWidgetType, &extraSize))
    return false;

  int32_t p2a = aContext->AppUnitsPerDevPixel();
  nsMargin m(NSIntPixelsToAppUnits(extraSize.top, p2a),
             NSIntPixelsToAppUnits(extraSize.right, p2a),
             NSIntPixelsToAppUnits(extraSize.bottom, p2a),
             NSIntPixelsToAppUnits(extraSize.left, p2a));

  aOverflowRect->Inflate(m);
  return true;
}

NS_IMETHODIMP
nsNativeThemeGTK::GetMinimumWidgetSize(nsRenderingContext* aContext,
                                       nsIFrame* aFrame, uint8_t aWidgetType,
                                       nsIntSize* aResult, bool* aIsOverridable)
{
  aResult->width = aResult->height = 0;
  *aIsOverridable = true;

  switch (aWidgetType) {
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
      {
        MozGtkScrollbarMetrics metrics;
        moz_gtk_get_scrollbar_metrics(&metrics);

        aResult->width = metrics.slider_width;
        aResult->height = metrics.stepper_size;
        *aIsOverridable = false;
      }
      break;
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
      {
        MozGtkScrollbarMetrics metrics;
        moz_gtk_get_scrollbar_metrics(&metrics);

        aResult->width = metrics.stepper_size;
        aResult->height = metrics.slider_width;
        *aIsOverridable = false;
      }
      break;
    case NS_THEME_SPLITTER:
    {
      gint metrics;
      if (IsHorizontal(aFrame)) {
        moz_gtk_splitter_get_metrics(GTK_ORIENTATION_HORIZONTAL, &metrics);
        aResult->width = metrics;
        aResult->height = 0;
      } else {
        moz_gtk_splitter_get_metrics(GTK_ORIENTATION_VERTICAL, &metrics);
        aResult->width = 0;
        aResult->height = metrics;
      }
      *aIsOverridable = false;
    }
    break;
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    {
      /* While we enforce a minimum size for the thumb, this is ignored
       * for the some scrollbars if buttons are hidden (bug 513006) because
       * the thumb isn't a direct child of the scrollbar, unlike the buttons
       * or track. So add a minimum size to the track as well to prevent a
       * 0-width scrollbar. */
      MozGtkScrollbarMetrics metrics;
      moz_gtk_get_scrollbar_metrics(&metrics);

      if (aWidgetType == NS_THEME_SCROLLBAR_TRACK_VERTICAL)
        aResult->width = metrics.slider_width + 2 * metrics.trough_border;
      else
        aResult->height = metrics.slider_width + 2 * metrics.trough_border;

      *aIsOverridable = false;
    }
    break;
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
      {
        MozGtkScrollbarMetrics metrics;
        moz_gtk_get_scrollbar_metrics(&metrics);

        nsRect rect = aFrame->GetParent()->GetRect();
        int32_t p2a = aFrame->PresContext()->DeviceContext()->
                        AppUnitsPerDevPixel();
        nsMargin margin;

        /* Get the available space, if that is smaller then the minimum size,
         * adjust the mininum size to fit into it.
         * Setting aIsOverridable to true has no effect for thumbs. */
        aFrame->GetMargin(margin);
        rect.Deflate(margin);
        aFrame->GetParent()->GetBorderAndPadding(margin);
        rect.Deflate(margin);

        if (aWidgetType == NS_THEME_SCROLLBAR_THUMB_VERTICAL) {
          aResult->width = metrics.slider_width;
          aResult->height = std::min(NSAppUnitsToIntPixels(rect.height, p2a),
                                   metrics.min_slider_size);
        } else {
          aResult->height = metrics.slider_width;
          aResult->width = std::min(NSAppUnitsToIntPixels(rect.width, p2a),
                                  metrics.min_slider_size);
        }

        *aIsOverridable = false;
      }
      break;
    case NS_THEME_RANGE_THUMB:
      {
        gint thumb_length, thumb_height;

        if (IsRangeHorizontal(aFrame)) {
          moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_HORIZONTAL, &thumb_length, &thumb_height);
        } else {
          moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_VERTICAL, &thumb_height, &thumb_length);
        }
        aResult->width = thumb_length;
        aResult->height = thumb_height;

        *aIsOverridable = false;
      }
      break;
    case NS_THEME_SCALE_THUMB_HORIZONTAL:
    case NS_THEME_SCALE_THUMB_VERTICAL:
      {
        gint thumb_length, thumb_height;

        if (aWidgetType == NS_THEME_SCALE_THUMB_VERTICAL) {
          moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_VERTICAL, &thumb_length, &thumb_height);
          aResult->width = thumb_height;
          aResult->height = thumb_length;
        } else {
          moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_HORIZONTAL, &thumb_length, &thumb_height);
          aResult->width = thumb_length;
          aResult->height = thumb_height;
        }

        *aIsOverridable = false;
      }
      break;
    case NS_THEME_TAB_SCROLLARROW_BACK:
    case NS_THEME_TAB_SCROLLARROW_FORWARD:
      {
        moz_gtk_get_tab_scroll_arrow_size(&aResult->width, &aResult->height);
        *aIsOverridable = false;
      }
      break;
  case NS_THEME_DROPDOWN_BUTTON:
    {
      moz_gtk_get_combo_box_entry_button_size(&aResult->width,
                                              &aResult->height);
      *aIsOverridable = false;
    }
    break;
  case NS_THEME_MENUSEPARATOR:
    {
      gint separator_height;

      moz_gtk_get_menu_separator_height(&separator_height);
      aResult->height = separator_height;
    
      *aIsOverridable = false;
    }
    break;
  case NS_THEME_CHECKBOX:
  case NS_THEME_RADIO:
    {
      gint indicator_size, indicator_spacing;

      if (aWidgetType == NS_THEME_CHECKBOX) {
        moz_gtk_checkbox_get_metrics(&indicator_size, &indicator_spacing);
      } else {
        moz_gtk_radio_get_metrics(&indicator_size, &indicator_spacing);
      }

      // Include space for the indicator and the padding around it.
      aResult->width = indicator_size;
      aResult->height = indicator_size;
    }
    break;
  case NS_THEME_TOOLBAR_BUTTON_DROPDOWN:
  case NS_THEME_BUTTON_ARROW_UP:
  case NS_THEME_BUTTON_ARROW_DOWN:
  case NS_THEME_BUTTON_ARROW_NEXT:
  case NS_THEME_BUTTON_ARROW_PREVIOUS:
    {
        moz_gtk_get_arrow_size(&aResult->width, &aResult->height);
        *aIsOverridable = false;
    }
    break;
  case NS_THEME_CHECKBOX_CONTAINER:
  case NS_THEME_RADIO_CONTAINER:
  case NS_THEME_CHECKBOX_LABEL:
  case NS_THEME_RADIO_LABEL:
  case NS_THEME_BUTTON:
  case NS_THEME_DROPDOWN:
  case NS_THEME_TOOLBAR_BUTTON:
  case NS_THEME_TREEVIEW_HEADER_CELL:
    {
      // Just include our border, and let the box code augment the size.
      nsIntMargin border;
      nsNativeThemeGTK::GetWidgetBorder(aContext->DeviceContext(),
                                        aFrame, aWidgetType, &border);
      aResult->width = border.left + border.right;
      aResult->height = border.top + border.bottom;
    }
    break;
  case NS_THEME_TOOLBAR_SEPARATOR:
    {
      gint separator_width;
    
      moz_gtk_get_toolbar_separator_width(&separator_width);
    
      aResult->width = separator_width;
    }
    break;
  case NS_THEME_SPINNER:
    // hard code these sizes
    aResult->width = 14;
    aResult->height = 26;
    break;
  case NS_THEME_TREEVIEW_HEADER_SORTARROW:
  case NS_THEME_SPINNER_UP_BUTTON:
  case NS_THEME_SPINNER_DOWN_BUTTON:
    // hard code these sizes
    aResult->width = 14;
    aResult->height = 13;
    break;
  case NS_THEME_RESIZER:
    // same as Windows to make our lives easier
    aResult->width = aResult->height = 15;
    *aIsOverridable = false;
    break;
  case NS_THEME_TREEVIEW_TWISTY:
  case NS_THEME_TREEVIEW_TWISTY_OPEN:
    {
      gint expander_size;

      moz_gtk_get_treeview_expander_size(&expander_size);
      aResult->width = aResult->height = expander_size;
      *aIsOverridable = false;
    }
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                     nsIAtom* aAttribute, bool* aShouldRepaint)
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
      aWidgetType == NS_THEME_MENUBAR ||
      aWidgetType == NS_THEME_MENUPOPUP ||
      aWidgetType == NS_THEME_TOOLTIP ||
      aWidgetType == NS_THEME_MENUSEPARATOR ||
      aWidgetType == NS_THEME_WINDOW ||
      aWidgetType == NS_THEME_DIALOG) {
    *aShouldRepaint = false;
    return NS_OK;
  }

  if ((aWidgetType == NS_THEME_SCROLLBAR_BUTTON_UP ||
       aWidgetType == NS_THEME_SCROLLBAR_BUTTON_DOWN ||
       aWidgetType == NS_THEME_SCROLLBAR_BUTTON_LEFT ||
       aWidgetType == NS_THEME_SCROLLBAR_BUTTON_RIGHT) &&
      (aAttribute == nsGkAtoms::curpos ||
       aAttribute == nsGkAtoms::maxpos)) {
    *aShouldRepaint = true;
    return NS_OK;
  }

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = true;
  }
  else {
    // Check the attribute to see if it's relevant.  
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = false;
    if (aAttribute == nsGkAtoms::disabled ||
        aAttribute == nsGkAtoms::checked ||
        aAttribute == nsGkAtoms::selected ||
        aAttribute == nsGkAtoms::focused ||
        aAttribute == nsGkAtoms::readonly ||
        aAttribute == nsGkAtoms::_default ||
        aAttribute == nsGkAtoms::menuactive ||
        aAttribute == nsGkAtoms::open ||
        aAttribute == nsGkAtoms::parentfocused)
      *aShouldRepaint = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::ThemeChanged()
{
  memset(mDisabledWidgetTypes, 0, sizeof(mDisabledWidgetTypes));
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsNativeThemeGTK::ThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      uint8_t aWidgetType)
{
  if (IsWidgetTypeDisabled(mDisabledWidgetTypes, aWidgetType))
    return false;

  switch (aWidgetType) {
  case NS_THEME_BUTTON:
  case NS_THEME_BUTTON_FOCUS:
  case NS_THEME_RADIO:
  case NS_THEME_CHECKBOX:
  case NS_THEME_TOOLBOX: // N/A
  case NS_THEME_TOOLBAR:
  case NS_THEME_TOOLBAR_BUTTON:
  case NS_THEME_TOOLBAR_DUAL_BUTTON: // so we can override the border with 0
  case NS_THEME_TOOLBAR_BUTTON_DROPDOWN:
  case NS_THEME_BUTTON_ARROW_UP:
  case NS_THEME_BUTTON_ARROW_DOWN:
  case NS_THEME_BUTTON_ARROW_NEXT:
  case NS_THEME_BUTTON_ARROW_PREVIOUS:
  case NS_THEME_TOOLBAR_SEPARATOR:
  case NS_THEME_TOOLBAR_GRIPPER:
  case NS_THEME_STATUSBAR:
  case NS_THEME_STATUSBAR_PANEL:
  case NS_THEME_STATUSBAR_RESIZER_PANEL:
  case NS_THEME_RESIZER:
  case NS_THEME_LISTBOX:
    // case NS_THEME_LISTBOX_LISTITEM:
  case NS_THEME_TREEVIEW:
    // case NS_THEME_TREEVIEW_TREEITEM:
  case NS_THEME_TREEVIEW_TWISTY:
    // case NS_THEME_TREEVIEW_LINE:
    // case NS_THEME_TREEVIEW_HEADER:
  case NS_THEME_TREEVIEW_HEADER_CELL:
  case NS_THEME_TREEVIEW_HEADER_SORTARROW:
  case NS_THEME_TREEVIEW_TWISTY_OPEN:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSBAR_CHUNK:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSBAR_CHUNK_VERTICAL:
    case NS_THEME_TAB:
    // case NS_THEME_TAB_PANEL:
    case NS_THEME_TAB_PANELS:
    case NS_THEME_TAB_SCROLLARROW_BACK:
    case NS_THEME_TAB_SCROLLARROW_FORWARD:
  case NS_THEME_TOOLTIP:
  case NS_THEME_SPINNER:
  case NS_THEME_SPINNER_UP_BUTTON:
  case NS_THEME_SPINNER_DOWN_BUTTON:
  case NS_THEME_SPINNER_TEXTFIELD:
    // case NS_THEME_SCROLLBAR:  (n/a for gtk)
    // case NS_THEME_SCROLLBAR_SMALL: (n/a for gtk)
  case NS_THEME_SCROLLBAR_BUTTON_UP:
  case NS_THEME_SCROLLBAR_BUTTON_DOWN:
  case NS_THEME_SCROLLBAR_BUTTON_LEFT:
  case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
  case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
  case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
  case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
  case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
  case NS_THEME_NUMBER_INPUT:
  case NS_THEME_TEXTFIELD:
  case NS_THEME_TEXTFIELD_MULTILINE:
  case NS_THEME_DROPDOWN_TEXTFIELD:
  case NS_THEME_RANGE:
  case NS_THEME_RANGE_THUMB:
  case NS_THEME_SCALE_HORIZONTAL:
  case NS_THEME_SCALE_THUMB_HORIZONTAL:
  case NS_THEME_SCALE_VERTICAL:
  case NS_THEME_SCALE_THUMB_VERTICAL:
    // case NS_THEME_SCALE_THUMB_START:
    // case NS_THEME_SCALE_THUMB_END:
    // case NS_THEME_SCALE_TICK:
  case NS_THEME_CHECKBOX_CONTAINER:
  case NS_THEME_RADIO_CONTAINER:
  case NS_THEME_CHECKBOX_LABEL:
  case NS_THEME_RADIO_LABEL:
  case NS_THEME_MENUBAR:
  case NS_THEME_MENUPOPUP:
  case NS_THEME_MENUITEM:
  case NS_THEME_MENUARROW:
  case NS_THEME_MENUSEPARATOR:
  case NS_THEME_CHECKMENUITEM:
  case NS_THEME_RADIOMENUITEM:
  case NS_THEME_SPLITTER:
  case NS_THEME_WINDOW:
  case NS_THEME_DIALOG:
  case NS_THEME_DROPDOWN:
  case NS_THEME_DROPDOWN_TEXT:
    return !IsWidgetStyled(aPresContext, aFrame, aWidgetType);

  case NS_THEME_DROPDOWN_BUTTON:
    // "Native" dropdown buttons cause padding and margin problems, but only
    // in HTML so allow them in XUL.
    return (!aFrame || IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) &&
           !IsWidgetStyled(aPresContext, aFrame, aWidgetType);

  }

  return false;
}

NS_IMETHODIMP_(bool)
nsNativeThemeGTK::WidgetIsContainer(uint8_t aWidgetType)
{
  // XXXdwh At some point flesh all of this out.
  if (aWidgetType == NS_THEME_DROPDOWN_BUTTON ||
      aWidgetType == NS_THEME_RADIO ||
      aWidgetType == NS_THEME_RANGE_THUMB ||
      aWidgetType == NS_THEME_CHECKBOX ||
      aWidgetType == NS_THEME_TAB_SCROLLARROW_BACK ||
      aWidgetType == NS_THEME_TAB_SCROLLARROW_FORWARD ||
      aWidgetType == NS_THEME_BUTTON_ARROW_UP ||
      aWidgetType == NS_THEME_BUTTON_ARROW_DOWN ||
      aWidgetType == NS_THEME_BUTTON_ARROW_NEXT ||
      aWidgetType == NS_THEME_BUTTON_ARROW_PREVIOUS)
    return false;
  return true;
}

bool
nsNativeThemeGTK::ThemeDrawsFocusForWidget(uint8_t aWidgetType)
{
   if (aWidgetType == NS_THEME_DROPDOWN ||
      aWidgetType == NS_THEME_BUTTON || 
      aWidgetType == NS_THEME_TREEVIEW_HEADER_CELL)
    return true;
  
  return false;
}

bool
nsNativeThemeGTK::ThemeNeedsComboboxDropmarker()
{
  return false;
}

nsITheme::Transparency
nsNativeThemeGTK::GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType)
{
  switch (aWidgetType) {
  // These widgets always draw a default background.
  case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
  case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
  case NS_THEME_TOOLBAR:
#if (MOZ_WIDGET_GTK == 2)
  case NS_THEME_MENUBAR:
#endif
  case NS_THEME_MENUPOPUP:
  case NS_THEME_WINDOW:
  case NS_THEME_DIALOG:
  // Tooltips use gtk_paint_flat_box().
  case NS_THEME_TOOLTIP:
    return eOpaque;
  }

  return eUnknownTransparency;
}
