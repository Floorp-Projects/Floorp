/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeGTK.h"
#include "nsThemeConstants.h"
#include "gtkdrawing.h"
#include "ScreenHelperGTK.h"

#include "gfx2DGlue.h"
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
#include "nsAttrValueInlines.h"

#include "mozilla/EventStates.h"
#include "mozilla/Services.h"

#include <gdk/gdkprivate.h>
#include <gtk/gtk.h>

#include "gfxContext.h"
#include "gfxPlatformGtk.h"
#include "gfxGdkNativeRenderer.h"
#include "mozilla/gfx/BorrowedContext.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "mozilla/gfx/PathHelpers.h"

#ifdef MOZ_X11
#  ifdef CAIRO_HAS_XLIB_SURFACE
#    include "cairo-xlib.h"
#  endif
#  ifdef CAIRO_HAS_XLIB_XRENDER_SURFACE
#    include "cairo-xlib-xrender.h"
#  endif
#endif

#include <algorithm>
#include <dlfcn.h>

using namespace mozilla;
using namespace mozilla::gfx;

NS_IMPL_ISUPPORTS_INHERITED(nsNativeThemeGTK, nsNativeTheme, nsITheme,
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

/* static */ GtkTextDirection
nsNativeThemeGTK::GetTextDirection(nsIFrame* aFrame)
{
  // IsFrameRTL() treats vertical-rl modes as right-to-left (in addition to
  // horizontal text with direction=RTL), rather than just considering the
  // text direction.  GtkTextDirection does not have distinct values for
  // vertical writing modes, but considering the block flow direction is
  // important for resizers and scrollbar elements, at least.
  return IsFrameRTL(aFrame) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;
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

static bool ShouldScrollbarButtonBeDisabled(int32_t aCurpos, int32_t aMaxpos,
                                            uint8_t aWidgetType)
{
  return ((aCurpos == 0 && (aWidgetType == NS_THEME_SCROLLBARBUTTON_UP ||
                            aWidgetType == NS_THEME_SCROLLBARBUTTON_LEFT))
      || (aCurpos == aMaxpos && (aWidgetType == NS_THEME_SCROLLBARBUTTON_DOWN ||
                                 aWidgetType == NS_THEME_SCROLLBARBUTTON_RIGHT)));
}

bool
nsNativeThemeGTK::GetGtkWidgetAndState(uint8_t aWidgetType, nsIFrame* aFrame,
                                       WidgetNodeType& aGtkWidgetType,
                                       GtkWidgetState* aState,
                                       gint* aWidgetFlags)
{
  if (aState) {
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
    } else if (aWidgetType == NS_THEME_TOOLBARBUTTON_DROPDOWN ||
               aWidgetType == NS_THEME_TREEHEADERSORTARROW ||
               aWidgetType == NS_THEME_BUTTON_ARROW_PREVIOUS ||
               aWidgetType == NS_THEME_BUTTON_ARROW_NEXT ||
               aWidgetType == NS_THEME_BUTTON_ARROW_UP ||
               aWidgetType == NS_THEME_BUTTON_ARROW_DOWN) {
      // The state of an arrow comes from its parent.
      stateFrame = aFrame = aFrame->GetParent();
    }

    EventStates eventState = GetContentState(stateFrame, aWidgetType);

    aState->disabled = IsDisabled(aFrame, eventState) || IsReadOnly(aFrame);
    aState->active  = eventState.HasState(NS_EVENT_STATE_ACTIVE);
    aState->focused = eventState.HasState(NS_EVENT_STATE_FOCUS);
    aState->inHover = eventState.HasState(NS_EVENT_STATE_HOVER);
    aState->isDefault = IsDefaultButton(aFrame);
    aState->canDefault = FALSE; // XXX fix me
    aState->depressed = FALSE;

    if (aWidgetType == NS_THEME_FOCUS_OUTLINE) {
      aState->disabled = FALSE;
      aState->active  = FALSE;
      aState->inHover = FALSE;
      aState->isDefault = FALSE;
      aState->canDefault = FALSE;

      aState->focused = TRUE;
      aState->depressed = TRUE; // see moz_gtk_entry_paint()
    } else if (aWidgetType == NS_THEME_BUTTON ||
               aWidgetType == NS_THEME_TOOLBARBUTTON ||
               aWidgetType == NS_THEME_DUALBUTTON ||
               aWidgetType == NS_THEME_TOOLBARBUTTON_DROPDOWN ||
               aWidgetType == NS_THEME_MENULIST ||
               aWidgetType == NS_THEME_MENULIST_BUTTON) {
      aState->active &= aState->inHover;
    }

    if (IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) {
      // For these widget types, some element (either a child or parent)
      // actually has element focus, so we check the focused attribute
      // to see whether to draw in the focused state.
      if (aWidgetType == NS_THEME_NUMBER_INPUT ||
          aWidgetType == NS_THEME_TEXTFIELD ||
          aWidgetType == NS_THEME_TEXTFIELD_MULTILINE ||
          aWidgetType == NS_THEME_MENULIST_TEXTFIELD ||
          aWidgetType == NS_THEME_SPINNER_TEXTFIELD ||
          aWidgetType == NS_THEME_RADIO_CONTAINER ||
          aWidgetType == NS_THEME_RADIO_LABEL) {
        aState->focused = IsFocused(aFrame);
      } else if (aWidgetType == NS_THEME_RADIO ||
                 aWidgetType == NS_THEME_CHECKBOX) {
        // In XUL, checkboxes and radios shouldn't have focus rings, their labels do
        aState->focused = FALSE;
      }

      if (aWidgetType == NS_THEME_SCROLLBARTHUMB_VERTICAL ||
          aWidgetType == NS_THEME_SCROLLBARTHUMB_HORIZONTAL) {
        // for scrollbars we need to go up two to go from the thumb to
        // the slider to the actual scrollbar object
        nsIFrame *tmpFrame = aFrame->GetParent()->GetParent();

        aState->curpos = CheckIntAttr(tmpFrame, nsGkAtoms::curpos, 0);
        aState->maxpos = CheckIntAttr(tmpFrame, nsGkAtoms::maxpos, 100);

        if (CheckBooleanAttr(aFrame, nsGkAtoms::active)) {
          aState->active = TRUE;
          // Set hover state to emulate Gtk style of active scrollbar thumb
          aState->inHover = TRUE;
        }
      }

      if (aWidgetType == NS_THEME_SCROLLBARBUTTON_UP ||
          aWidgetType == NS_THEME_SCROLLBARBUTTON_DOWN ||
          aWidgetType == NS_THEME_SCROLLBARBUTTON_LEFT ||
          aWidgetType == NS_THEME_SCROLLBARBUTTON_RIGHT) {
        // set the state to disabled when the scrollbar is scrolled to
        // the beginning or the end, depending on the button type.
        int32_t curpos = CheckIntAttr(aFrame, nsGkAtoms::curpos, 0);
        int32_t maxpos = CheckIntAttr(aFrame, nsGkAtoms::maxpos, 100);
        if (ShouldScrollbarButtonBeDisabled(curpos, maxpos, aWidgetType)) {
          aState->disabled = true;
        }

        // In order to simulate native GTK scrollbar click behavior,
        // we set the active attribute on the element to true if it's
        // pressed with any mouse button.
        // This allows us to show that it's active without setting :active
        else if (CheckBooleanAttr(aFrame, nsGkAtoms::active))
          aState->active = true;

        if (aWidgetFlags) {
          *aWidgetFlags = GetScrollbarButtonType(aFrame);
          if (aWidgetType - NS_THEME_SCROLLBARBUTTON_UP < 2)
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
        } else {
          aState->inHover = CheckBooleanAttr(aFrame, nsGkAtoms::menuactive);
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
          aWidgetType == NS_THEME_TOOLBARBUTTON ||
          aWidgetType == NS_THEME_DUALBUTTON ||
          aWidgetType == NS_THEME_TOOLBARBUTTON_DROPDOWN ||
          aWidgetType == NS_THEME_MENULIST ||
          aWidgetType == NS_THEME_MENULIST_BUTTON) {
        bool menuOpen = IsOpenButton(aFrame);
        aState->depressed = IsCheckedButton(aFrame) || menuOpen;
        // we must not highlight buttons with open drop down menus on hover.
        aState->inHover = aState->inHover && !menuOpen;
      }

      // When the input field of the drop down button has focus, some themes
      // should draw focus for the drop down button as well.
      if (aWidgetType == NS_THEME_MENULIST_BUTTON && aWidgetFlags) {
        *aWidgetFlags = CheckBooleanAttr(aFrame, nsGkAtoms::parentfocused);
      }
    }
  }

  switch (aWidgetType) {
  case NS_THEME_BUTTON:
    if (aWidgetFlags)
      *aWidgetFlags = GTK_RELIEF_NORMAL;
    aGtkWidgetType = MOZ_GTK_BUTTON;
    break;
  case NS_THEME_TOOLBARBUTTON:
  case NS_THEME_DUALBUTTON:
    if (aWidgetFlags)
      *aWidgetFlags = GTK_RELIEF_NONE;
    aGtkWidgetType = MOZ_GTK_TOOLBAR_BUTTON;
    break;
  case NS_THEME_FOCUS_OUTLINE:
    aGtkWidgetType = MOZ_GTK_ENTRY;
    break;
  case NS_THEME_CHECKBOX:
  case NS_THEME_RADIO:
    aGtkWidgetType = (aWidgetType == NS_THEME_RADIO) ? MOZ_GTK_RADIOBUTTON : MOZ_GTK_CHECKBUTTON;
    break;
  case NS_THEME_SCROLLBARBUTTON_UP:
  case NS_THEME_SCROLLBARBUTTON_DOWN:
  case NS_THEME_SCROLLBARBUTTON_LEFT:
  case NS_THEME_SCROLLBARBUTTON_RIGHT:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_BUTTON;
    break;
  case NS_THEME_SCROLLBAR_VERTICAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_VERTICAL;
    if (GetWidgetTransparency(aFrame, aWidgetType) == eOpaque)
        *aWidgetFlags = MOZ_GTK_TRACK_OPAQUE;
    else
        *aWidgetFlags = 0;
    break;
  case NS_THEME_SCROLLBAR_HORIZONTAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_HORIZONTAL;
    if (GetWidgetTransparency(aFrame, aWidgetType) == eOpaque)
        *aWidgetFlags = MOZ_GTK_TRACK_OPAQUE;
    else
        *aWidgetFlags = 0;
    break;
  case NS_THEME_SCROLLBARTRACK_HORIZONTAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL;
    break;
  case NS_THEME_SCROLLBARTRACK_VERTICAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL;
    break;
  case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_THUMB_VERTICAL;
    break;
  case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    aGtkWidgetType = MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL;
    break;
  case NS_THEME_SPINNER:
    aGtkWidgetType = MOZ_GTK_SPINBUTTON;
    break;
  case NS_THEME_SPINNER_UPBUTTON:
    aGtkWidgetType = MOZ_GTK_SPINBUTTON_UP;
    break;
  case NS_THEME_SPINNER_DOWNBUTTON:
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
  case NS_THEME_SCALETHUMB_HORIZONTAL:
    if (aWidgetFlags)
      *aWidgetFlags = GTK_ORIENTATION_HORIZONTAL;
    aGtkWidgetType = MOZ_GTK_SCALE_THUMB_HORIZONTAL;
    break;
  case NS_THEME_SCALE_VERTICAL:
    if (aWidgetFlags)
      *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
    aGtkWidgetType = MOZ_GTK_SCALE_VERTICAL;
    break;
  case NS_THEME_SEPARATOR:
    aGtkWidgetType = MOZ_GTK_TOOLBAR_SEPARATOR;
    break;
  case NS_THEME_SCALETHUMB_VERTICAL:
    if (aWidgetFlags)
      *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
    aGtkWidgetType = MOZ_GTK_SCALE_THUMB_VERTICAL;
    break;
  case NS_THEME_TOOLBARGRIPPER:
    aGtkWidgetType = MOZ_GTK_GRIPPER;
    break;
  case NS_THEME_RESIZER:
    aGtkWidgetType = MOZ_GTK_RESIZER;
    break;
  case NS_THEME_NUMBER_INPUT:
  case NS_THEME_TEXTFIELD:
    aGtkWidgetType = MOZ_GTK_ENTRY;
    break;
  case NS_THEME_TEXTFIELD_MULTILINE:
#if (MOZ_WIDGET_GTK == 3)
    aGtkWidgetType = MOZ_GTK_TEXT_VIEW;
#else
    aGtkWidgetType = MOZ_GTK_ENTRY;
#endif
    break;
  case NS_THEME_LISTBOX:
  case NS_THEME_TREEVIEW:
    aGtkWidgetType = MOZ_GTK_TREEVIEW;
    break;
  case NS_THEME_TREEHEADERCELL:
    if (aWidgetFlags) {
      // In this case, the flag denotes whether the header is the sorted one or not
      if (GetTreeSortDirection(aFrame) == eTreeSortDirection_Natural)
        *aWidgetFlags = false;
      else
        *aWidgetFlags = true;
    }
    aGtkWidgetType = MOZ_GTK_TREE_HEADER_CELL;
    break;
  case NS_THEME_TREEHEADERSORTARROW:
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
  case NS_THEME_TREETWISTY:
    aGtkWidgetType = MOZ_GTK_TREEVIEW_EXPANDER;
    if (aWidgetFlags)
      *aWidgetFlags = GTK_EXPANDER_COLLAPSED;
    break;
  case NS_THEME_TREETWISTYOPEN:
    aGtkWidgetType = MOZ_GTK_TREEVIEW_EXPANDER;
    if (aWidgetFlags)
      *aWidgetFlags = GTK_EXPANDER_EXPANDED;
    break;
  case NS_THEME_MENULIST:
    aGtkWidgetType = MOZ_GTK_DROPDOWN;
    if (aWidgetFlags)
        *aWidgetFlags = IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XHTML);
    break;
  case NS_THEME_MENULIST_TEXT:
    return false; // nothing to do, but prevents the bg from being drawn
  case NS_THEME_MENULIST_TEXTFIELD:
    aGtkWidgetType = MOZ_GTK_DROPDOWN_ENTRY;
    break;
  case NS_THEME_MENULIST_BUTTON:
    aGtkWidgetType = MOZ_GTK_DROPDOWN_ARROW;
    break;
  case NS_THEME_TOOLBARBUTTON_DROPDOWN:
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
  case NS_THEME_STATUSBARPANEL:
  case NS_THEME_RESIZERPANEL:
    aGtkWidgetType = MOZ_GTK_FRAME;
    break;
  case NS_THEME_PROGRESSBAR:
  case NS_THEME_PROGRESSBAR_VERTICAL:
    aGtkWidgetType = MOZ_GTK_PROGRESSBAR;
    break;
  case NS_THEME_PROGRESSCHUNK:
  case NS_THEME_PROGRESSCHUNK_VERTICAL:
    {
      nsIFrame* stateFrame = aFrame->GetParent();
      EventStates eventStates = GetContentState(stateFrame, aWidgetType);

      aGtkWidgetType = IsIndeterminateProgress(stateFrame, eventStates)
                         ? IsVerticalProgress(stateFrame)
                           ? MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE
                           : MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE
                         : MOZ_GTK_PROGRESS_CHUNK;
    }
    break;
  case NS_THEME_TAB_SCROLL_ARROW_BACK:
  case NS_THEME_TAB_SCROLL_ARROW_FORWARD:
    if (aWidgetFlags)
      *aWidgetFlags = aWidgetType == NS_THEME_TAB_SCROLL_ARROW_BACK ?
                        GTK_ARROW_LEFT : GTK_ARROW_RIGHT;
    aGtkWidgetType = MOZ_GTK_TAB_SCROLLARROW;
    break;
  case NS_THEME_TABPANELS:
    aGtkWidgetType = MOZ_GTK_TABPANELS;
    break;
  case NS_THEME_TAB:
    {
      if (IsBottomTab(aFrame)) {
        aGtkWidgetType = MOZ_GTK_TAB_BOTTOM;
      } else {
        aGtkWidgetType = MOZ_GTK_TAB_TOP;
      }

      if (aWidgetFlags) {
        /* First bits will be used to store max(0,-bmargin) where bmargin
         * is the bottom margin of the tab in pixels  (resp. top margin,
         * for bottom tabs). */
        *aWidgetFlags = GetTabMarginPixels(aFrame);

        if (IsSelectedTab(aFrame))
          *aWidgetFlags |= MOZ_GTK_TAB_SELECTED;

        if (IsFirstTab(aFrame))
          *aWidgetFlags |= MOZ_GTK_TAB_FIRST;
      }
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
    {
      nsMenuFrame *menuFrame = do_QueryFrame(aFrame);
      if (menuFrame && menuFrame->IsOnMenuBar()) {
        aGtkWidgetType = MOZ_GTK_MENUBARITEM;
        break;
      }
    }
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
  case NS_THEME_GTK_INFO_BAR:
    aGtkWidgetType = MOZ_GTK_INFO_BAR;
    break;
  default:
    return false;
  }

  return true;
}

#if (MOZ_WIDGET_GTK == 2)
class ThemeRenderer : public gfxGdkNativeRenderer {
public:
  ThemeRenderer(GtkWidgetState aState, WidgetNodeType aGTKWidgetType,
                gint aFlags, GtkTextDirection aDirection,
                const GdkRectangle& aGDKRect, const GdkRectangle& aGDKClip)
    : mState(aState), mGTKWidgetType(aGTKWidgetType), mFlags(aFlags),
      mDirection(aDirection), mGDKRect(aGDKRect), mGDKClip(aGDKClip) {}
  nsresult DrawWithGDK(GdkDrawable * drawable, gint offsetX, gint offsetY,
                       GdkRectangle * clipRects, uint32_t numClipRects);
private:
  GtkWidgetState mState;
  WidgetNodeType mGTKWidgetType;
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
#else
class SystemCairoClipper : public ClipExporter {
public:
  explicit SystemCairoClipper(cairo_t* aContext) : mContext(aContext)
  {
  }

  void
  BeginClip(const Matrix& aTransform) override
  {
    cairo_matrix_t mat;
    GfxMatrixToCairoMatrix(aTransform, mat);
    cairo_set_matrix(mContext, &mat);

    cairo_new_path(mContext);
  }

  void
  MoveTo(const Point &aPoint) override
  {
    cairo_move_to(mContext, aPoint.x, aPoint.y);
    mCurrentPoint = aPoint;
  }

  void
  LineTo(const Point &aPoint) override
  {
    cairo_line_to(mContext, aPoint.x, aPoint.y);
    mCurrentPoint = aPoint;
  }

  void
  BezierTo(const Point &aCP1, const Point &aCP2, const Point &aCP3) override
  {
    cairo_curve_to(mContext, aCP1.x, aCP1.y, aCP2.x, aCP2.y, aCP3.x, aCP3.y);
    mCurrentPoint = aCP3;
  }

  void
  QuadraticBezierTo(const Point &aCP1, const Point &aCP2) override
  {
    Point CP0 = CurrentPoint();
    Point CP1 = (CP0 + aCP1 * 2.0) / 3.0;
    Point CP2 = (aCP2 + aCP1 * 2.0) / 3.0;
    Point CP3 = aCP2;
    cairo_curve_to(mContext, CP1.x, CP1.y, CP2.x, CP2.y, CP3.x, CP3.y);
    mCurrentPoint = aCP2;
  }

  void
  Arc(const Point &aOrigin, float aRadius, float aStartAngle, float aEndAngle,
      bool aAntiClockwise) override
  {
    ArcToBezier(this, aOrigin, Size(aRadius, aRadius), aStartAngle, aEndAngle,
                aAntiClockwise);
  }

  void
  Close() override
  {
    cairo_close_path(mContext);
  }

  void
  EndClip() override
  {
    cairo_clip(mContext);
  }

  Point
  CurrentPoint() const override
  {
    return mCurrentPoint;
  }

private:
  cairo_t* mContext;
  Point mCurrentPoint;
};

static void
DrawThemeWithCairo(gfxContext* aContext, DrawTarget* aDrawTarget,
                   GtkWidgetState aState, WidgetNodeType aGTKWidgetType,
                   gint aFlags, GtkTextDirection aDirection, gint aScaleFactor,
                   bool aSnapped, const Point& aDrawOrigin, const nsIntSize& aDrawSize,
                   GdkRectangle& aGDKRect, nsITheme::Transparency aTransparency)
{
  Point drawOffset;
  Matrix transform;
  if (!aSnapped) {
    // If we are not snapped, we depend on the DT for translation.
    drawOffset = aDrawOrigin;
    transform = aDrawTarget->GetTransform().PreTranslate(aDrawOrigin);
  } else {
    // Otherwise, we only need to take the device offset into account.
    drawOffset = aDrawOrigin - aContext->GetDeviceOffset();
    transform = Matrix::Translation(drawOffset);
  }

  if (aScaleFactor != 1)
    transform.PreScale(aScaleFactor, aScaleFactor);

  cairo_matrix_t mat;
  GfxMatrixToCairoMatrix(transform, mat);

  nsIntSize clipSize((aDrawSize.width + aScaleFactor - 1) / aScaleFactor,
                     (aDrawSize.height + aScaleFactor - 1) / aScaleFactor);

#ifndef MOZ_TREE_CAIRO
  // Directly use the Cairo draw target to render the widget if using system Cairo everywhere.
  BorrowedCairoContext borrowCairo(aDrawTarget);
  if (borrowCairo.mCairo) {
    cairo_set_matrix(borrowCairo.mCairo, &mat);

    cairo_new_path(borrowCairo.mCairo);
    cairo_rectangle(borrowCairo.mCairo, 0, 0, clipSize.width, clipSize.height);
    cairo_clip(borrowCairo.mCairo);

    moz_gtk_widget_paint(aGTKWidgetType, borrowCairo.mCairo, &aGDKRect, &aState, aFlags, aDirection);

    borrowCairo.Finish();
    return;
  }
#endif

  // A direct Cairo draw target is not available, so we need to create a temporary one.
#if defined(MOZ_X11) && defined(CAIRO_HAS_XLIB_SURFACE)
  // If using a Cairo xlib surface, then try to reuse it.
  BorrowedXlibDrawable borrow(aDrawTarget);
  if (borrow.GetDrawable()) {
    nsIntSize size = borrow.GetSize();
    cairo_surface_t* surf = nullptr;
    // Check if the surface is using XRender.
#ifdef CAIRO_HAS_XLIB_XRENDER_SURFACE
    if (borrow.GetXRenderFormat()) {
      surf = cairo_xlib_surface_create_with_xrender_format(
          borrow.GetDisplay(), borrow.GetDrawable(), borrow.GetScreen(),
          borrow.GetXRenderFormat(), size.width, size.height);
    } else {
#else
      if (! borrow.GetXRenderFormat()) {
#endif
        surf = cairo_xlib_surface_create(
            borrow.GetDisplay(), borrow.GetDrawable(), borrow.GetVisual(),
            size.width, size.height);
      }
      if (!NS_WARN_IF(!surf)) {
        Point offset = borrow.GetOffset();
        if (offset != Point()) {
          cairo_surface_set_device_offset(surf, offset.x, offset.y);
        }
        cairo_t* cr = cairo_create(surf);
        if (!NS_WARN_IF(!cr)) {
          RefPtr<SystemCairoClipper> clipper = new SystemCairoClipper(cr);
          aContext->ExportClip(*clipper);

          cairo_set_matrix(cr, &mat);

          cairo_new_path(cr);
          cairo_rectangle(cr, 0, 0, clipSize.width, clipSize.height);
          cairo_clip(cr);

          moz_gtk_widget_paint(aGTKWidgetType, cr, &aGDKRect, &aState, aFlags, aDirection);

          cairo_destroy(cr);
        }
        cairo_surface_destroy(surf);
      }
      borrow.Finish();
      return;
    }
#endif

  // Check if the widget requires complex masking that must be composited.
  // Try to directly write to the draw target's pixels if possible.
  uint8_t* data;
  nsIntSize size;
  int32_t stride;
  SurfaceFormat format;
  IntPoint origin;
  if (aDrawTarget->LockBits(&data, &size, &stride, &format, &origin)) {
    // Create a Cairo image surface context the device rectangle.
    cairo_surface_t* surf =
      cairo_image_surface_create_for_data(
        data, GfxFormatToCairoFormat(format), size.width, size.height, stride);
    if (!NS_WARN_IF(!surf)) {
      if (origin != IntPoint()) {
        cairo_surface_set_device_offset(surf, -origin.x, -origin.y);
      }
      cairo_t* cr = cairo_create(surf);
      if (!NS_WARN_IF(!cr)) {
        RefPtr<SystemCairoClipper> clipper = new SystemCairoClipper(cr);
        aContext->ExportClip(*clipper);

        cairo_set_matrix(cr, &mat);

        cairo_new_path(cr);
        cairo_rectangle(cr, 0, 0, clipSize.width, clipSize.height);
        cairo_clip(cr);

        moz_gtk_widget_paint(aGTKWidgetType, cr, &aGDKRect, &aState, aFlags, aDirection);

        cairo_destroy(cr);
      }
      cairo_surface_destroy(surf);
    }
    aDrawTarget->ReleaseBits(data);
  } else {
    // If the widget has any transparency, make sure to choose an alpha format.
    format = aTransparency != nsITheme::eOpaque ? SurfaceFormat::B8G8R8A8 : aDrawTarget->GetFormat();
    // Create a temporary data surface to render the widget into.
    RefPtr<DataSourceSurface> dataSurface =
      Factory::CreateDataSourceSurface(aDrawSize, format, aTransparency != nsITheme::eOpaque);
    DataSourceSurface::MappedSurface map;
    if (!NS_WARN_IF(!(dataSurface && dataSurface->Map(DataSourceSurface::MapType::WRITE, &map)))) {
      // Create a Cairo image surface wrapping the data surface.
      cairo_surface_t* surf =
        cairo_image_surface_create_for_data(map.mData, GfxFormatToCairoFormat(format),
                                            aDrawSize.width, aDrawSize.height, map.mStride);
      cairo_t* cr = nullptr;
      if (!NS_WARN_IF(!surf)) {
        cr = cairo_create(surf);
        if (!NS_WARN_IF(!cr)) {
          if (aScaleFactor != 1) {
            cairo_scale(cr, aScaleFactor, aScaleFactor);
          }

          moz_gtk_widget_paint(aGTKWidgetType, cr, &aGDKRect, &aState, aFlags, aDirection);
        }
      }

      // Unmap the surface before using it as a source
      dataSurface->Unmap();

      if (cr) {
        if (!aSnapped || aTransparency != nsITheme::eOpaque) {
          // The widget either needs to be masked or has transparency, so use the slower drawing path.
          aDrawTarget->DrawSurface(dataSurface,
                                   Rect(aSnapped ? drawOffset - aDrawTarget->GetTransform().GetTranslation() : drawOffset,
                                        Size(aDrawSize)),
                                   Rect(0, 0, aDrawSize.width, aDrawSize.height));
        } else {
          // The widget is a simple opaque rectangle, so just copy it out.
          aDrawTarget->CopySurface(dataSurface,
                                   IntRect(0, 0, aDrawSize.width, aDrawSize.height),
                                   TruncatedToInt(drawOffset));
        }

        cairo_destroy(cr);
      }

      if (surf) {
        cairo_surface_destroy(surf);
      }
    }
  }
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
  case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    aExtra->top = aExtra->bottom = 1;
    break;
  case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
    aExtra->left = aExtra->right = 1;
    break;

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
      break;
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
        break;
      }
      return false;
    }
  case NS_THEME_FOCUS_OUTLINE:
    {
      moz_gtk_get_focus_outline_size(&aExtra->left, &aExtra->top);
      aExtra->right = aExtra->left;
      aExtra->bottom = aExtra->top;
      break;
    }
  case NS_THEME_TAB :
    {
      if (!IsSelectedTab(aFrame))
        return false;

      gint gap_height = moz_gtk_get_tab_thickness(IsBottomTab(aFrame) ?
                            MOZ_GTK_TAB_BOTTOM : MOZ_GTK_TAB_TOP);
      if (!gap_height)
        return false;

      int32_t extra = gap_height - GetTabMarginPixels(aFrame);
      if (extra <= 0)
        return false;

      if (IsBottomTab(aFrame)) {
        aExtra->top = extra;
      } else {
        aExtra->bottom = extra;
      }
      return false;
    }
  default:
    return false;
  }
  gint scale = ScreenHelperGTK::GetGTKMonitorScaleFactor();
  aExtra->top *= scale;
  aExtra->right *= scale;
  aExtra->bottom *= scale;
  aExtra->left *= scale;
  return true;
}

NS_IMETHODIMP
nsNativeThemeGTK::DrawWidgetBackground(nsRenderingContext* aContext,
                                       nsIFrame* aFrame,
                                       uint8_t aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aDirtyRect)
{
  GtkWidgetState state;
  WidgetNodeType gtkWidgetType;
  GtkTextDirection direction = GetTextDirection(aFrame);
  gint flags;
  if (!GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, &state,
                            &flags))
    return NS_OK;

  gfxContext* ctx = aContext->ThebesContext();
  nsPresContext *presContext = aFrame->PresContext();

  gfxRect rect = presContext->AppUnitsToGfxUnits(aRect);
  gfxRect dirtyRect = presContext->AppUnitsToGfxUnits(aDirtyRect);
  gint scaleFactor = ScreenHelperGTK::GetGTKMonitorScaleFactor();

  // Align to device pixels where sensible
  // to provide crisper and faster drawing.
  // Don't snap if it's a non-unit scale factor. We're going to have to take
  // slow paths then in any case.
  bool snapped = ctx->UserToDevicePixelSnapped(rect);
  if (snapped) {
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

  NS_ASSERTION(!IsWidgetTypeDisabled(mDisabledWidgetTypes, aWidgetType),
               "Trying to render an unsafe widget!");

  bool safeState = IsWidgetStateSafe(mSafeWidgetStates, aWidgetType, &state);
  if (!safeState) {
    gLastGdkError = 0;
    gdk_error_trap_push ();
  }

  Transparency transparency = GetWidgetTransparency(aFrame, aWidgetType);

  // gdk rectangles are wrt the drawing rect.
  GdkRectangle gdk_rect = {-drawingRect.x/scaleFactor,
                           -drawingRect.y/scaleFactor,
                           widgetRect.width/scaleFactor,
                           widgetRect.height/scaleFactor};

  // translate everything so (0,0) is the top left of the drawingRect
  gfxPoint origin = rect.TopLeft() + drawingRect.TopLeft();

#if (MOZ_WIDGET_GTK == 2)
  gfxContextAutoSaveRestore autoSR(ctx);
  gfxMatrix matrix;
  if (!snapped) { // else rects are in device coords
    matrix = ctx->CurrentMatrix();
  }
  matrix.Translate(origin);
  matrix.Scale(scaleFactor, scaleFactor); // Draw in GDK coords
  ctx->SetMatrix(matrix);

  // The gdk_clip is just advisory here, meaning "you don't
  // need to draw outside this rect if you don't feel like it!"
  GdkRectangle gdk_clip = {0, 0, drawingRect.width, drawingRect.height};

  ThemeRenderer renderer(state, gtkWidgetType, flags, direction,
                         gdk_rect, gdk_clip);

  // Some themes (e.g. Clearlooks) just don't clip properly to any
  // clip rect we provide, so we cannot advertise support for clipping within
  // the widget bounds.
  uint32_t rendererFlags = 0;
  if (transparency == eOpaque) {
    rendererFlags |= gfxGdkNativeRenderer::DRAW_IS_OPAQUE;
  }

  // GtkStyles (used by the widget drawing backend) are created for a
  // particular colormap/visual.
  GdkColormap* colormap = moz_gtk_widget_get_colormap();

  renderer.Draw(ctx, drawingRect.Size(), rendererFlags, colormap);
#else 
  DrawThemeWithCairo(ctx, aContext->GetDrawTarget(),
                     state, gtkWidgetType, flags, direction, scaleFactor,
                     snapped, ToPoint(origin), drawingRect.Size(),
                     gdk_rect, transparency);
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

WidgetNodeType
nsNativeThemeGTK::NativeThemeToGtkTheme(uint8_t aWidgetType, nsIFrame* aFrame)
{
  WidgetNodeType gtkWidgetType;
  gint unusedFlags;

  if (!GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, nullptr,
                            &unusedFlags))
  {
    MOZ_ASSERT_UNREACHABLE("Unknown native widget to gtk widget mapping");
    return MOZ_GTK_WINDOW;
  }
  return gtkWidgetType;
}

NS_IMETHODIMP
nsNativeThemeGTK::GetWidgetBorder(nsDeviceContext* aContext, nsIFrame* aFrame,
                                  uint8_t aWidgetType, nsIntMargin* aResult)
{
  GtkTextDirection direction = GetTextDirection(aFrame);
  aResult->top = aResult->left = aResult->right = aResult->bottom = 0;
  switch (aWidgetType) {
  case NS_THEME_SCROLLBAR_HORIZONTAL:
  case NS_THEME_SCROLLBAR_VERTICAL:
    {
      GtkOrientation orientation =
        aWidgetType == NS_THEME_SCROLLBAR_HORIZONTAL ?
        GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
      const ScrollbarGTKMetrics* metrics = GetScrollbarMetrics(orientation);

      const GtkBorder& border = metrics->border.scrollbar;
      aResult->top = border.top;
      aResult->right = border.right;
      aResult->bottom = border.bottom;
      aResult->left = border.left;
    }
    break;
  case NS_THEME_SCROLLBARTRACK_HORIZONTAL:
  case NS_THEME_SCROLLBARTRACK_VERTICAL:
    {
      GtkOrientation orientation =
        aWidgetType == NS_THEME_SCROLLBARTRACK_HORIZONTAL ?
        GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
      const ScrollbarGTKMetrics* metrics = GetScrollbarMetrics(orientation);

      const GtkBorder& border = metrics->border.track;
      aResult->top = border.top;
      aResult->right = border.right;
      aResult->bottom = border.bottom;
      aResult->left = border.left;
    }
    break;
  case NS_THEME_TOOLBOX:
    // gtk has no toolbox equivalent.  So, although we map toolbox to
    // gtk's 'toolbar' for purposes of painting the widget background,
    // we don't use the toolbar border for toolbox.
    break;
  case NS_THEME_DUALBUTTON:
    // TOOLBAR_DUAL_BUTTON is an interesting case.  We want a border to draw
    // around the entire button + dropdown, and also an inner border if you're
    // over the button part.  But, we want the inner button to be right up
    // against the edge of the outer button so that the borders overlap.
    // To make this happen, we draw a button border for the outer button,
    // but don't reserve any space for it.
    break;
  case NS_THEME_TAB:
    {
      WidgetNodeType gtkWidgetType;
      gint flags;

      if (!GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, nullptr,
                                &flags))
        return NS_OK;

      moz_gtk_get_tab_border(&aResult->left, &aResult->top,
                             &aResult->right, &aResult->bottom, direction,
                             (GtkTabFlags)flags, gtkWidgetType);
    }
    break;
  case NS_THEME_MENUITEM:
  case NS_THEME_CHECKMENUITEM:
  case NS_THEME_RADIOMENUITEM:
    // For regular menuitems, we will be using GetWidgetPadding instead of
    // GetWidgetBorder to pad up the widget's internals; other menuitems
    // will need to fall through and use the default case as before.
    if (IsRegularMenuItem(aFrame))
      break;
    MOZ_FALLTHROUGH;
  default:
    {
      WidgetNodeType gtkWidgetType;
      gint unusedFlags;
      if (GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, nullptr,
                               &unusedFlags)) {
        moz_gtk_get_widget_border(gtkWidgetType, &aResult->left, &aResult->top,
                                  &aResult->right, &aResult->bottom, direction);
      }
    }
  }

  gint scale = ScreenHelperGTK::GetGTKMonitorScaleFactor();
  aResult->top *= scale;
  aResult->right *= scale;
  aResult->bottom *= scale;
  aResult->left *= scale;
  return NS_OK;
}

bool
nsNativeThemeGTK::GetWidgetPadding(nsDeviceContext* aContext,
                                   nsIFrame* aFrame, uint8_t aWidgetType,
                                   nsIntMargin* aResult)
{
  switch (aWidgetType) {
    case NS_THEME_BUTTON_FOCUS:
    case NS_THEME_TOOLBARBUTTON:
    case NS_THEME_DUALBUTTON:
    case NS_THEME_TAB_SCROLL_ARROW_BACK:
    case NS_THEME_TAB_SCROLL_ARROW_FORWARD:
    case NS_THEME_MENULIST_BUTTON:
    case NS_THEME_TOOLBARBUTTON_DROPDOWN:
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
        WidgetNodeType gtkWidgetType;
        if (GetGtkWidgetAndState(aWidgetType, aFrame, gtkWidgetType, nullptr,
                                 nullptr)) {
          moz_gtk_get_widget_border(gtkWidgetType, &aResult->left, &aResult->top,
                                    &aResult->right, &aResult->bottom,
                                    GetTextDirection(aFrame));
        }

        gint horizontal_padding;

        if (aWidgetType == NS_THEME_MENUITEM)
          moz_gtk_menuitem_get_horizontal_padding(&horizontal_padding);
        else
          moz_gtk_checkmenuitem_get_horizontal_padding(&horizontal_padding);

        aResult->left += horizontal_padding;
        aResult->right += horizontal_padding;

        gint scale = ScreenHelperGTK::GetGTKMonitorScaleFactor();
        aResult->top *= scale;
        aResult->right *= scale;
        aResult->bottom *= scale;
        aResult->left *= scale;

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
nsNativeThemeGTK::GetMinimumWidgetSize(nsPresContext* aPresContext,
                                       nsIFrame* aFrame, uint8_t aWidgetType,
                                       LayoutDeviceIntSize* aResult,
                                       bool* aIsOverridable)
{
  aResult->width = aResult->height = 0;
  *aIsOverridable = true;

  switch (aWidgetType) {
    case NS_THEME_SCROLLBARBUTTON_UP:
    case NS_THEME_SCROLLBARBUTTON_DOWN:
      {
        const ScrollbarGTKMetrics* metrics =
          GetScrollbarMetrics(GTK_ORIENTATION_VERTICAL);

        aResult->width = metrics->size.button.width;
        aResult->height = metrics->size.button.height;
        *aIsOverridable = false;
      }
      break;
    case NS_THEME_SCROLLBARBUTTON_LEFT:
    case NS_THEME_SCROLLBARBUTTON_RIGHT:
      {
        const ScrollbarGTKMetrics* metrics =
          GetScrollbarMetrics(GTK_ORIENTATION_HORIZONTAL);

        aResult->width = metrics->size.button.width;
        aResult->height = metrics->size.button.height;
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
    case NS_THEME_SCROLLBAR_HORIZONTAL:
    case NS_THEME_SCROLLBAR_VERTICAL:
    {
      /* While we enforce a minimum size for the thumb, this is ignored
       * for the some scrollbars if buttons are hidden (bug 513006) because
       * the thumb isn't a direct child of the scrollbar, unlike the buttons
       * or track. So add a minimum size to the track as well to prevent a
       * 0-width scrollbar. */
      GtkOrientation orientation =
        aWidgetType == NS_THEME_SCROLLBAR_HORIZONTAL ?
        GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
      const ScrollbarGTKMetrics* metrics = GetScrollbarMetrics(orientation);

      aResult->width = metrics->size.scrollbar.width;
      aResult->height = metrics->size.scrollbar.height;
    }
    break;
    case NS_THEME_SCROLLBARTHUMB_VERTICAL:
    case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
      {
        GtkOrientation orientation =
          aWidgetType == NS_THEME_SCROLLBARTHUMB_HORIZONTAL ?
          GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
        const ScrollbarGTKMetrics* metrics = GetScrollbarMetrics(orientation);

        aResult->width = metrics->size.thumb.width;
        aResult->height = metrics->size.thumb.height;
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
    case NS_THEME_RANGE:
      {
        gint scale_width, scale_height;

        moz_gtk_get_scale_metrics(IsRangeHorizontal(aFrame) ?
            GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL,
            &scale_width, &scale_height);
        aResult->width = scale_width;
        aResult->height = scale_height;

        *aIsOverridable = true;
      }
      break;
    case NS_THEME_SCALETHUMB_HORIZONTAL:
    case NS_THEME_SCALETHUMB_VERTICAL:
      {
        gint thumb_length, thumb_height;

        if (aWidgetType == NS_THEME_SCALETHUMB_VERTICAL) {
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
    case NS_THEME_TAB_SCROLL_ARROW_BACK:
    case NS_THEME_TAB_SCROLL_ARROW_FORWARD:
      {
        moz_gtk_get_tab_scroll_arrow_size(&aResult->width, &aResult->height);
        *aIsOverridable = false;
      }
      break;
  case NS_THEME_MENULIST_BUTTON:
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
  case NS_THEME_TOOLBARBUTTON_DROPDOWN:
  case NS_THEME_BUTTON_ARROW_UP:
  case NS_THEME_BUTTON_ARROW_DOWN:
  case NS_THEME_BUTTON_ARROW_NEXT:
  case NS_THEME_BUTTON_ARROW_PREVIOUS:
    {
      moz_gtk_get_arrow_size(MOZ_GTK_TOOLBARBUTTON_ARROW,
                             &aResult->width, &aResult->height);
      *aIsOverridable = false;
    }
    break;
  case NS_THEME_CHECKBOX_CONTAINER:
  case NS_THEME_RADIO_CONTAINER:
  case NS_THEME_CHECKBOX_LABEL:
  case NS_THEME_RADIO_LABEL:
  case NS_THEME_BUTTON:
  case NS_THEME_MENULIST:
  case NS_THEME_TOOLBARBUTTON:
  case NS_THEME_TREEHEADERCELL:
    {
      if (aWidgetType == NS_THEME_MENULIST) {
        // Include the arrow size.
        moz_gtk_get_arrow_size(MOZ_GTK_DROPDOWN,
                               &aResult->width, &aResult->height);
      }
      // else the minimum size is missing consideration of container
      // descendants; the value returned here will not be helpful, but the
      // box model may consider border and padding with child minimum sizes.

      nsIntMargin border;
      nsNativeThemeGTK::GetWidgetBorder(aFrame->PresContext()->DeviceContext(),
                                        aFrame, aWidgetType, &border);
      aResult->width += border.left + border.right;
      aResult->height += border.top + border.bottom;
    }
    break;
#if (MOZ_WIDGET_GTK == 3)
  case NS_THEME_NUMBER_INPUT:
  case NS_THEME_TEXTFIELD:
    {
      moz_gtk_get_entry_min_height(&aResult->height);
    }
    break;
#endif
  case NS_THEME_SEPARATOR:
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
  case NS_THEME_TREEHEADERSORTARROW:
  case NS_THEME_SPINNER_UPBUTTON:
  case NS_THEME_SPINNER_DOWNBUTTON:
    // hard code these sizes
    aResult->width = 14;
    aResult->height = 13;
    break;
  case NS_THEME_RESIZER:
    // same as Windows to make our lives easier
    aResult->width = aResult->height = 15;
    *aIsOverridable = false;
    break;
  case NS_THEME_TREETWISTY:
  case NS_THEME_TREETWISTYOPEN:
    {
      gint expander_size;

      moz_gtk_get_treeview_expander_size(&expander_size);
      aResult->width = aResult->height = expander_size;
      *aIsOverridable = false;
    }
    break;
  }

  *aResult = *aResult * ScreenHelperGTK::GetGTKMonitorScaleFactor();

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                     nsIAtom* aAttribute, bool* aShouldRepaint,
                                     const nsAttrValue* aOldValue)
{
  // Some widget types just never change state.
  if (aWidgetType == NS_THEME_TOOLBOX ||
      aWidgetType == NS_THEME_TOOLBAR ||
      aWidgetType == NS_THEME_STATUSBAR ||
      aWidgetType == NS_THEME_STATUSBARPANEL ||
      aWidgetType == NS_THEME_RESIZERPANEL ||
      aWidgetType == NS_THEME_PROGRESSCHUNK ||
      aWidgetType == NS_THEME_PROGRESSCHUNK_VERTICAL ||
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

  if ((aWidgetType == NS_THEME_SCROLLBARTHUMB_VERTICAL ||
       aWidgetType == NS_THEME_SCROLLBARTHUMB_HORIZONTAL) &&
       aAttribute == nsGkAtoms::active) {
    *aShouldRepaint = true;
    return NS_OK;
  }

  if ((aWidgetType == NS_THEME_SCROLLBARBUTTON_UP ||
       aWidgetType == NS_THEME_SCROLLBARBUTTON_DOWN ||
       aWidgetType == NS_THEME_SCROLLBARBUTTON_LEFT ||
       aWidgetType == NS_THEME_SCROLLBARBUTTON_RIGHT) &&
      (aAttribute == nsGkAtoms::curpos ||
       aAttribute == nsGkAtoms::maxpos)) {
    // If 'curpos' has changed and we are passed its old value, we can
    // determine whether the button's enablement actually needs to change.
    if (aAttribute == nsGkAtoms::curpos && aOldValue) {
      int32_t curpos = CheckIntAttr(aFrame, nsGkAtoms::curpos, 0);
      int32_t maxpos = CheckIntAttr(aFrame, nsGkAtoms::maxpos, 0);
      nsAutoString str;
      aOldValue->ToString(str);
      nsresult err;
      int32_t oldCurpos = str.ToInteger(&err);
      if (str.IsEmpty() || NS_FAILED(err)) {
        *aShouldRepaint = true;
      } else {
        bool disabledBefore = ShouldScrollbarButtonBeDisabled(oldCurpos, maxpos, aWidgetType);
        bool disabledNow = ShouldScrollbarButtonBeDisabled(curpos, maxpos, aWidgetType);
        *aShouldRepaint = (disabledBefore != disabledNow);
      }
    } else {
      *aShouldRepaint = true;
    }
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
        aAttribute == nsGkAtoms::visuallyselected ||
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
  // Combobox dropdowns don't support native theming in vertical mode.
  case NS_THEME_MENULIST:
  case NS_THEME_MENULIST_TEXT:
  case NS_THEME_MENULIST_TEXTFIELD:
    if (aFrame && aFrame->GetWritingMode().IsVertical()) {
      return false;
    }
    MOZ_FALLTHROUGH;

  case NS_THEME_BUTTON:
  case NS_THEME_BUTTON_FOCUS:
  case NS_THEME_RADIO:
  case NS_THEME_CHECKBOX:
  case NS_THEME_TOOLBOX: // N/A
  case NS_THEME_TOOLBAR:
  case NS_THEME_TOOLBARBUTTON:
  case NS_THEME_DUALBUTTON: // so we can override the border with 0
  case NS_THEME_TOOLBARBUTTON_DROPDOWN:
  case NS_THEME_BUTTON_ARROW_UP:
  case NS_THEME_BUTTON_ARROW_DOWN:
  case NS_THEME_BUTTON_ARROW_NEXT:
  case NS_THEME_BUTTON_ARROW_PREVIOUS:
  case NS_THEME_SEPARATOR:
  case NS_THEME_TOOLBARGRIPPER:
  case NS_THEME_STATUSBAR:
  case NS_THEME_STATUSBARPANEL:
  case NS_THEME_RESIZERPANEL:
  case NS_THEME_RESIZER:
  case NS_THEME_LISTBOX:
    // case NS_THEME_LISTITEM:
  case NS_THEME_TREEVIEW:
    // case NS_THEME_TREEITEM:
  case NS_THEME_TREETWISTY:
    // case NS_THEME_TREELINE:
    // case NS_THEME_TREEHEADER:
  case NS_THEME_TREEHEADERCELL:
  case NS_THEME_TREEHEADERSORTARROW:
  case NS_THEME_TREETWISTYOPEN:
    case NS_THEME_PROGRESSBAR:
    case NS_THEME_PROGRESSCHUNK:
    case NS_THEME_PROGRESSBAR_VERTICAL:
    case NS_THEME_PROGRESSCHUNK_VERTICAL:
    case NS_THEME_TAB:
    // case NS_THEME_TABPANEL:
    case NS_THEME_TABPANELS:
    case NS_THEME_TAB_SCROLL_ARROW_BACK:
    case NS_THEME_TAB_SCROLL_ARROW_FORWARD:
  case NS_THEME_TOOLTIP:
  case NS_THEME_SPINNER:
  case NS_THEME_SPINNER_UPBUTTON:
  case NS_THEME_SPINNER_DOWNBUTTON:
  case NS_THEME_SPINNER_TEXTFIELD:
    // case NS_THEME_SCROLLBAR:  (n/a for gtk)
    // case NS_THEME_SCROLLBAR_SMALL: (n/a for gtk)
  case NS_THEME_SCROLLBARBUTTON_UP:
  case NS_THEME_SCROLLBARBUTTON_DOWN:
  case NS_THEME_SCROLLBARBUTTON_LEFT:
  case NS_THEME_SCROLLBARBUTTON_RIGHT:
  case NS_THEME_SCROLLBAR_HORIZONTAL:
  case NS_THEME_SCROLLBAR_VERTICAL:
  case NS_THEME_SCROLLBARTRACK_HORIZONTAL:
  case NS_THEME_SCROLLBARTRACK_VERTICAL:
  case NS_THEME_SCROLLBARTHUMB_HORIZONTAL:
  case NS_THEME_SCROLLBARTHUMB_VERTICAL:
  case NS_THEME_NUMBER_INPUT:
  case NS_THEME_TEXTFIELD:
  case NS_THEME_TEXTFIELD_MULTILINE:
  case NS_THEME_RANGE:
  case NS_THEME_RANGE_THUMB:
  case NS_THEME_SCALE_HORIZONTAL:
  case NS_THEME_SCALETHUMB_HORIZONTAL:
  case NS_THEME_SCALE_VERTICAL:
  case NS_THEME_SCALETHUMB_VERTICAL:
    // case NS_THEME_SCALETHUMBSTART:
    // case NS_THEME_SCALETHUMBEND:
    // case NS_THEME_SCALETHUMBTICK:
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
#if (MOZ_WIDGET_GTK == 3)
  case NS_THEME_GTK_INFO_BAR:
#endif
    return !IsWidgetStyled(aPresContext, aFrame, aWidgetType);

  case NS_THEME_MENULIST_BUTTON:
    if (aFrame && aFrame->GetWritingMode().IsVertical()) {
      return false;
    }
    // "Native" dropdown buttons cause padding and margin problems, but only
    // in HTML so allow them in XUL.
    return (!aFrame || IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) &&
           !IsWidgetStyled(aPresContext, aFrame, aWidgetType);

  case NS_THEME_FOCUS_OUTLINE:
    return true;
  }

  return false;
}

NS_IMETHODIMP_(bool)
nsNativeThemeGTK::WidgetIsContainer(uint8_t aWidgetType)
{
  // XXXdwh At some point flesh all of this out.
  if (aWidgetType == NS_THEME_MENULIST_BUTTON ||
      aWidgetType == NS_THEME_RADIO ||
      aWidgetType == NS_THEME_RANGE_THUMB ||
      aWidgetType == NS_THEME_CHECKBOX ||
      aWidgetType == NS_THEME_TAB_SCROLL_ARROW_BACK ||
      aWidgetType == NS_THEME_TAB_SCROLL_ARROW_FORWARD ||
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
   if (aWidgetType == NS_THEME_MENULIST ||
      aWidgetType == NS_THEME_BUTTON || 
      aWidgetType == NS_THEME_TREEHEADERCELL)
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
#if (MOZ_WIDGET_GTK == 2)
  case NS_THEME_TOOLBAR:
  case NS_THEME_MENUBAR:
#endif
  case NS_THEME_MENUPOPUP:
  case NS_THEME_WINDOW:
  case NS_THEME_DIALOG:
    return eOpaque;
  case NS_THEME_SCROLLBAR_VERTICAL:
  case NS_THEME_SCROLLBAR_HORIZONTAL:
#if (MOZ_WIDGET_GTK == 3)
    // Make scrollbar tracks opaque on the window's scroll frame to prevent
    // leaf layers from overlapping. See bug 1179780.
    if (!(CheckBooleanAttr(aFrame, nsGkAtoms::root_) &&
          aFrame->PresContext()->IsRootContentDocument() &&
          IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)))
      return eTransparent;
#endif
    return eOpaque;
  // Tooltips use gtk_paint_flat_box() on Gtk2
  // but are shaped on Gtk3
  case NS_THEME_TOOLTIP:
#if (MOZ_WIDGET_GTK == 2)
    return eOpaque;
#else
    return eTransparent;
#endif
  }

  return eUnknownTransparency;
}
