/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeGTK.h"
#include "HeadlessThemeGTK.h"
#include "nsStyleConsts.h"
#include "gtkdrawing.h"
#include "ScreenHelperGTK.h"

#include "gfx2DGlue.h"
#include "nsIObserverService.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsViewManager.h"
#include "nsNameSpaceManager.h"
#include "nsGfxCIID.h"
#include "nsTransform2D.h"
#include "nsMenuFrame.h"
#include "tree/nsTreeBodyFrame.h"
#include "prlink.h"
#include "nsGkAtoms.h"
#include "nsAttrValueInlines.h"

#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/ClearOnShutdown.h"
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
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsWindow.h"
#include "nsNativeBasicTheme.h"

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
using namespace mozilla::widget;
using mozilla::dom::HTMLInputElement;

NS_IMPL_ISUPPORTS_INHERITED(nsNativeThemeGTK, nsNativeTheme, nsITheme,
                            nsIObserver)

static int gLastGdkError;

// Return scale factor of the monitor where the window is located
// by the most part or layout.css.devPixelsPerPx pref if set to > 0.
static inline gint GetMonitorScaleFactor(nsIFrame* aFrame) {
  // When the layout.css.devPixelsPerPx is set the scale can be < 1,
  // the real monitor scale cannot go under 1.
  double scale = StaticPrefs::layout_css_devPixelsPerPx();
  if (scale <= 0) {
    nsIWidget* rootWidget = aFrame->PresContext()->GetRootWidget();
    if (rootWidget) {
      // We need to use GetDefaultScale() despite it returns monitor scale
      // factor multiplied by font scale factor because it is the only scale
      // updated in nsPuppetWidget.
      // Since we don't want to apply font scale factor for UI elements
      // (because GTK does not do so) we need to remove that from returned
      // value. The computed monitor scale factor needs to be rounded before
      // casting to integer to avoid rounding errors which would lead to
      // returning 0.
      int monitorScale = int(round(rootWidget->GetDefaultScale().scale /
                                   gfxPlatformGtk::GetFontScaleFactor()));
      // Monitor scale can be negative if it has not been initialized in the
      // puppet widget yet. We also make sure that we return positive value.
      if (monitorScale < 1) {
        return 1;
      }
      return monitorScale;
    }
  }
  // Use monitor scaling factor where devPixelsPerPx is set
  return ScreenHelperGTK::GetGTKMonitorScaleFactor();
}

nsNativeThemeGTK::nsNativeThemeGTK() {
  if (moz_gtk_init() != MOZ_GTK_SUCCESS) {
    memset(mDisabledWidgetTypes, 0xff, sizeof(mDisabledWidgetTypes));
    return;
  }

  // We have to call moz_gtk_shutdown before the event loop stops running.
  nsCOMPtr<nsIObserverService> obsServ =
      mozilla::services::GetObserverService();
  obsServ->AddObserver(this, "xpcom-shutdown", false);

  ThemeChanged();
}

nsNativeThemeGTK::~nsNativeThemeGTK() = default;

NS_IMETHODIMP
nsNativeThemeGTK::Observe(nsISupports* aSubject, const char* aTopic,
                          const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, "xpcom-shutdown")) {
    moz_gtk_shutdown();
  } else {
    MOZ_ASSERT_UNREACHABLE("unexpected topic");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

void nsNativeThemeGTK::RefreshWidgetWindow(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame);
  MOZ_ASSERT(aFrame->PresShell());

  nsViewManager* vm = aFrame->PresShell()->GetViewManager();
  if (!vm) {
    return;
  }
  vm->InvalidateAllViews();
}

static bool IsFrameContentNodeInNamespace(nsIFrame* aFrame,
                                          uint32_t aNamespace) {
  nsIContent* content = aFrame ? aFrame->GetContent() : nullptr;
  if (!content) return false;
  return content->IsInNamespace(aNamespace);
}

static bool IsWidgetTypeDisabled(const uint8_t* aDisabledVector,
                                 StyleAppearance aAppearance) {
  auto type = static_cast<size_t>(aAppearance);
  MOZ_ASSERT(type < static_cast<size_t>(mozilla::StyleAppearance::Count));
  return (aDisabledVector[type >> 3] & (1 << (type & 7))) != 0;
}

static void SetWidgetTypeDisabled(uint8_t* aDisabledVector,
                                  StyleAppearance aAppearance) {
  auto type = static_cast<size_t>(aAppearance);
  MOZ_ASSERT(type < static_cast<size_t>(mozilla::StyleAppearance::Count));
  aDisabledVector[type >> 3] |= (1 << (type & 7));
}

static inline uint16_t GetWidgetStateKey(StyleAppearance aAppearance,
                                         GtkWidgetState* aWidgetState) {
  return (aWidgetState->active | aWidgetState->focused << 1 |
          aWidgetState->inHover << 2 | aWidgetState->disabled << 3 |
          aWidgetState->isDefault << 4 |
          static_cast<uint16_t>(aAppearance) << 5);
}

static bool IsWidgetStateSafe(uint8_t* aSafeVector, StyleAppearance aAppearance,
                              GtkWidgetState* aWidgetState) {
  MOZ_ASSERT(static_cast<size_t>(aAppearance) <
             static_cast<size_t>(mozilla::StyleAppearance::Count));
  uint16_t key = GetWidgetStateKey(aAppearance, aWidgetState);
  return (aSafeVector[key >> 3] & (1 << (key & 7))) != 0;
}

static void SetWidgetStateSafe(uint8_t* aSafeVector,
                               StyleAppearance aAppearance,
                               GtkWidgetState* aWidgetState) {
  MOZ_ASSERT(static_cast<size_t>(aAppearance) <
             static_cast<size_t>(mozilla::StyleAppearance::Count));
  uint16_t key = GetWidgetStateKey(aAppearance, aWidgetState);
  aSafeVector[key >> 3] |= (1 << (key & 7));
}

/* static */
GtkTextDirection nsNativeThemeGTK::GetTextDirection(nsIFrame* aFrame) {
  // IsFrameRTL() treats vertical-rl modes as right-to-left (in addition to
  // horizontal text with direction=RTL), rather than just considering the
  // text direction.  GtkTextDirection does not have distinct values for
  // vertical writing modes, but considering the block flow direction is
  // important for resizers and scrollbar elements, at least.
  return IsFrameRTL(aFrame) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;
}

// Returns positive for negative margins (otherwise 0).
gint nsNativeThemeGTK::GetTabMarginPixels(nsIFrame* aFrame) {
  nscoord margin = IsBottomTab(aFrame) ? aFrame->GetUsedMargin().top
                                       : aFrame->GetUsedMargin().bottom;

  return std::min<gint>(
      MOZ_GTK_TAB_MARGIN_MASK,
      std::max(0, aFrame->PresContext()->AppUnitsToDevPixels(-margin)));
}

static bool ShouldScrollbarButtonBeDisabled(int32_t aCurpos, int32_t aMaxpos,
                                            StyleAppearance aAppearance) {
  return (
      (aCurpos == 0 && (aAppearance == StyleAppearance::ScrollbarbuttonUp ||
                        aAppearance == StyleAppearance::ScrollbarbuttonLeft)) ||
      (aCurpos == aMaxpos &&
       (aAppearance == StyleAppearance::ScrollbarbuttonDown ||
        aAppearance == StyleAppearance::ScrollbarbuttonRight)));
}

bool nsNativeThemeGTK::GetGtkWidgetAndState(StyleAppearance aAppearance,
                                            nsIFrame* aFrame,
                                            WidgetNodeType& aGtkWidgetType,
                                            GtkWidgetState* aState,
                                            gint* aWidgetFlags) {
  if (aState) {
    memset(aState, 0, sizeof(GtkWidgetState));

    // For XUL checkboxes and radio buttons, the state of the parent
    // determines our state.
    nsIFrame* stateFrame = aFrame;
    if (aFrame && ((aWidgetFlags && (aAppearance == StyleAppearance::Checkbox ||
                                     aAppearance == StyleAppearance::Radio)) ||
                   aAppearance == StyleAppearance::CheckboxLabel ||
                   aAppearance == StyleAppearance::RadioLabel)) {
      nsAtom* atom = nullptr;
      if (IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) {
        if (aAppearance == StyleAppearance::CheckboxLabel ||
            aAppearance == StyleAppearance::RadioLabel) {
          // Adjust stateFrame so GetContentState finds the correct state.
          stateFrame = aFrame = aFrame->GetParent()->GetParent();
        } else {
          // GetContentState knows to look one frame up for radio/checkbox
          // widgets, so don't adjust stateFrame here.
          aFrame = aFrame->GetParent();
        }
        if (aWidgetFlags) {
          if (!atom) {
            atom = (aAppearance == StyleAppearance::Checkbox ||
                    aAppearance == StyleAppearance::CheckboxLabel)
                       ? nsGkAtoms::checked
                       : nsGkAtoms::selected;
          }
          *aWidgetFlags = CheckBooleanAttr(aFrame, atom);
        }
      } else {
        if (aWidgetFlags) {
          *aWidgetFlags = 0;
          HTMLInputElement* inputElt =
              HTMLInputElement::FromNode(aFrame->GetContent());
          if (inputElt && inputElt->Checked())
            *aWidgetFlags |= MOZ_GTK_WIDGET_CHECKED;

          if (GetIndeterminate(aFrame))
            *aWidgetFlags |= MOZ_GTK_WIDGET_INCONSISTENT;
        }
      }
    } else if (aAppearance == StyleAppearance::ToolbarbuttonDropdown ||
               aAppearance == StyleAppearance::Treeheadersortarrow ||
               aAppearance == StyleAppearance::ButtonArrowPrevious ||
               aAppearance == StyleAppearance::ButtonArrowNext ||
               aAppearance == StyleAppearance::ButtonArrowUp ||
               aAppearance == StyleAppearance::ButtonArrowDown) {
      // The state of an arrow comes from its parent.
      stateFrame = aFrame = aFrame->GetParent();
    }

    EventStates eventState = GetContentState(stateFrame, aAppearance);

    aState->disabled = IsDisabled(aFrame, eventState) || IsReadOnly(aFrame);
    aState->active = eventState.HasState(NS_EVENT_STATE_ACTIVE);
    aState->focused = eventState.HasState(NS_EVENT_STATE_FOCUS);
    aState->inHover = eventState.HasState(NS_EVENT_STATE_HOVER);
    aState->isDefault = IsDefaultButton(aFrame);
    aState->canDefault = FALSE;  // XXX fix me

    if (aAppearance == StyleAppearance::FocusOutline) {
      aState->disabled = FALSE;
      aState->active = FALSE;
      aState->inHover = FALSE;
      aState->isDefault = FALSE;
      aState->canDefault = FALSE;

      aState->focused = TRUE;
      aState->depressed = TRUE;  // see moz_gtk_entry_paint()
    } else if (aAppearance == StyleAppearance::Button ||
               aAppearance == StyleAppearance::Toolbarbutton ||
               aAppearance == StyleAppearance::Dualbutton ||
               aAppearance == StyleAppearance::ToolbarbuttonDropdown ||
               aAppearance == StyleAppearance::Menulist ||
               aAppearance == StyleAppearance::MenulistButton ||
               aAppearance == StyleAppearance::MozMenulistArrowButton) {
      aState->active &= aState->inHover;
    } else if (aAppearance == StyleAppearance::Treetwisty ||
               aAppearance == StyleAppearance::Treetwistyopen) {
      nsTreeBodyFrame* treeBodyFrame = do_QueryFrame(aFrame);
      if (treeBodyFrame) {
        const mozilla::AtomArray& atoms =
            treeBodyFrame->GetPropertyArrayForCurrentDrawingItem();
        aState->selected = atoms.Contains((nsStaticAtom*)nsGkAtoms::selected);
        aState->inHover = atoms.Contains((nsStaticAtom*)nsGkAtoms::hover);
      }
    }

    if (IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) {
      // For these widget types, some element (either a child or parent)
      // actually has element focus, so we check the focused attribute
      // to see whether to draw in the focused state.
      if (aAppearance == StyleAppearance::NumberInput ||
          aAppearance == StyleAppearance::Textfield ||
          aAppearance == StyleAppearance::Textarea ||
          aAppearance == StyleAppearance::MenulistTextfield ||
          aAppearance == StyleAppearance::SpinnerTextfield ||
          aAppearance == StyleAppearance::RadioContainer ||
          aAppearance == StyleAppearance::RadioLabel) {
        aState->focused = IsFocused(aFrame);
      } else if (aAppearance == StyleAppearance::Radio ||
                 aAppearance == StyleAppearance::Checkbox) {
        // In XUL, checkboxes and radios shouldn't have focus rings, their
        // labels do
        aState->focused = FALSE;
      }

      if (aAppearance == StyleAppearance::ScrollbarthumbVertical ||
          aAppearance == StyleAppearance::ScrollbarthumbHorizontal) {
        // for scrollbars we need to go up two to go from the thumb to
        // the slider to the actual scrollbar object
        nsIFrame* tmpFrame = aFrame->GetParent()->GetParent();

        aState->curpos = CheckIntAttr(tmpFrame, nsGkAtoms::curpos, 0);
        aState->maxpos = CheckIntAttr(tmpFrame, nsGkAtoms::maxpos, 100);

        if (CheckBooleanAttr(aFrame, nsGkAtoms::active)) {
          aState->active = TRUE;
          // Set hover state to emulate Gtk style of active scrollbar thumb
          aState->inHover = TRUE;
        }
      }

      if (aAppearance == StyleAppearance::ScrollbarbuttonUp ||
          aAppearance == StyleAppearance::ScrollbarbuttonDown ||
          aAppearance == StyleAppearance::ScrollbarbuttonLeft ||
          aAppearance == StyleAppearance::ScrollbarbuttonRight) {
        // set the state to disabled when the scrollbar is scrolled to
        // the beginning or the end, depending on the button type.
        int32_t curpos = CheckIntAttr(aFrame, nsGkAtoms::curpos, 0);
        int32_t maxpos = CheckIntAttr(aFrame, nsGkAtoms::maxpos, 100);
        if (ShouldScrollbarButtonBeDisabled(curpos, maxpos, aAppearance)) {
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
          if (static_cast<uint8_t>(aAppearance) -
                  static_cast<uint8_t>(StyleAppearance::ScrollbarbuttonUp) <
              2)
            *aWidgetFlags |= MOZ_GTK_STEPPER_VERTICAL;
        }
      }

      // menu item state is determined by the attribute "_moz-menuactive",
      // and not by the mouse hovering (accessibility).  as a special case,
      // menus which are children of a menu bar are only marked as prelight
      // if they are open, not on normal hover.

      if (aAppearance == StyleAppearance::Menuitem ||
          aAppearance == StyleAppearance::Checkmenuitem ||
          aAppearance == StyleAppearance::Radiomenuitem ||
          aAppearance == StyleAppearance::Menuseparator ||
          aAppearance == StyleAppearance::Menuarrow) {
        bool isTopLevel = false;
        nsMenuFrame* menuFrame = do_QueryFrame(aFrame);
        if (menuFrame) {
          isTopLevel = menuFrame->IsOnMenuBar();
        }

        if (isTopLevel) {
          aState->inHover = menuFrame->IsOpen();
        } else {
          aState->inHover = CheckBooleanAttr(aFrame, nsGkAtoms::menuactive);
        }

        aState->active = FALSE;

        if (aAppearance == StyleAppearance::Checkmenuitem ||
            aAppearance == StyleAppearance::Radiomenuitem) {
          *aWidgetFlags = 0;
          if (aFrame && aFrame->GetContent() &&
              aFrame->GetContent()->IsElement()) {
            *aWidgetFlags = aFrame->GetContent()->AsElement()->AttrValueIs(
                kNameSpaceID_None, nsGkAtoms::checked, nsGkAtoms::_true,
                eIgnoreCase);
          }
        }
      }

      // A button with drop down menu open or an activated toggle button
      // should always appear depressed.
      if (aAppearance == StyleAppearance::Button ||
          aAppearance == StyleAppearance::Toolbarbutton ||
          aAppearance == StyleAppearance::Dualbutton ||
          aAppearance == StyleAppearance::ToolbarbuttonDropdown ||
          aAppearance == StyleAppearance::Menulist ||
          aAppearance == StyleAppearance::MenulistButton ||
          aAppearance == StyleAppearance::MozMenulistArrowButton) {
        bool menuOpen = IsOpenButton(aFrame);
        aState->depressed = IsCheckedButton(aFrame) || menuOpen;
        // we must not highlight buttons with open drop down menus on hover.
        aState->inHover = aState->inHover && !menuOpen;
      }

      // When the input field of the drop down button has focus, some themes
      // should draw focus for the drop down button as well.
      if ((aAppearance == StyleAppearance::MenulistButton ||
           aAppearance == StyleAppearance::MozMenulistArrowButton) &&
          aWidgetFlags) {
        *aWidgetFlags = CheckBooleanAttr(aFrame, nsGkAtoms::parentfocused);
      }
    }

    if (aAppearance == StyleAppearance::MozWindowTitlebar ||
        aAppearance == StyleAppearance::MozWindowTitlebarMaximized ||
        aAppearance == StyleAppearance::MozWindowButtonClose ||
        aAppearance == StyleAppearance::MozWindowButtonMinimize ||
        aAppearance == StyleAppearance::MozWindowButtonMaximize ||
        aAppearance == StyleAppearance::MozWindowButtonRestore) {
      aState->backdrop = !nsWindow::GetTopLevelWindowActiveState(aFrame);
    }

    if (aAppearance == StyleAppearance::ScrollbarbuttonUp ||
        aAppearance == StyleAppearance::ScrollbarbuttonDown ||
        aAppearance == StyleAppearance::ScrollbarbuttonLeft ||
        aAppearance == StyleAppearance::ScrollbarbuttonRight ||
        aAppearance == StyleAppearance::ScrollbarVertical ||
        aAppearance == StyleAppearance::ScrollbarHorizontal ||
        aAppearance == StyleAppearance::ScrollbartrackHorizontal ||
        aAppearance == StyleAppearance::ScrollbartrackVertical ||
        aAppearance == StyleAppearance::ScrollbarthumbVertical ||
        aAppearance == StyleAppearance::ScrollbarthumbHorizontal) {
      EventStates docState =
          aFrame->GetContent()->OwnerDoc()->GetDocumentState();
      aState->backdrop = docState.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE);
    }
  }

  switch (aAppearance) {
    case StyleAppearance::Button:
      if (aWidgetFlags) *aWidgetFlags = GTK_RELIEF_NORMAL;
      aGtkWidgetType = MOZ_GTK_BUTTON;
      break;
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Dualbutton:
      if (aWidgetFlags) *aWidgetFlags = GTK_RELIEF_NONE;
      aGtkWidgetType = MOZ_GTK_TOOLBAR_BUTTON;
      break;
    case StyleAppearance::FocusOutline:
      aGtkWidgetType = MOZ_GTK_ENTRY;
      break;
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
      aGtkWidgetType = (aAppearance == StyleAppearance::Radio)
                           ? MOZ_GTK_RADIOBUTTON
                           : MOZ_GTK_CHECKBUTTON;
      break;
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
      aGtkWidgetType = MOZ_GTK_SCROLLBAR_BUTTON;
      break;
    case StyleAppearance::ScrollbarVertical:
      aGtkWidgetType = MOZ_GTK_SCROLLBAR_VERTICAL;
      if (GetWidgetTransparency(aFrame, aAppearance) == eOpaque)
        *aWidgetFlags = MOZ_GTK_TRACK_OPAQUE;
      else
        *aWidgetFlags = 0;
      break;
    case StyleAppearance::ScrollbarHorizontal:
      aGtkWidgetType = MOZ_GTK_SCROLLBAR_HORIZONTAL;
      if (GetWidgetTransparency(aFrame, aAppearance) == eOpaque)
        *aWidgetFlags = MOZ_GTK_TRACK_OPAQUE;
      else
        *aWidgetFlags = 0;
      break;
    case StyleAppearance::ScrollbartrackHorizontal:
      aGtkWidgetType = MOZ_GTK_SCROLLBAR_TROUGH_HORIZONTAL;
      break;
    case StyleAppearance::ScrollbartrackVertical:
      aGtkWidgetType = MOZ_GTK_SCROLLBAR_TROUGH_VERTICAL;
      break;
    case StyleAppearance::ScrollbarthumbVertical:
      aGtkWidgetType = MOZ_GTK_SCROLLBAR_THUMB_VERTICAL;
      break;
    case StyleAppearance::ScrollbarthumbHorizontal:
      aGtkWidgetType = MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL;
      break;
    case StyleAppearance::InnerSpinButton:
      aGtkWidgetType = MOZ_GTK_INNER_SPIN_BUTTON;
      break;
    case StyleAppearance::Spinner:
      aGtkWidgetType = MOZ_GTK_SPINBUTTON;
      break;
    case StyleAppearance::SpinnerUpbutton:
      aGtkWidgetType = MOZ_GTK_SPINBUTTON_UP;
      break;
    case StyleAppearance::SpinnerDownbutton:
      aGtkWidgetType = MOZ_GTK_SPINBUTTON_DOWN;
      break;
    case StyleAppearance::SpinnerTextfield:
      aGtkWidgetType = MOZ_GTK_SPINBUTTON_ENTRY;
      break;
    case StyleAppearance::Range: {
      if (IsRangeHorizontal(aFrame)) {
        if (aWidgetFlags) *aWidgetFlags = GTK_ORIENTATION_HORIZONTAL;
        aGtkWidgetType = MOZ_GTK_SCALE_HORIZONTAL;
      } else {
        if (aWidgetFlags) *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
        aGtkWidgetType = MOZ_GTK_SCALE_VERTICAL;
      }
      break;
    }
    case StyleAppearance::RangeThumb: {
      if (IsRangeHorizontal(aFrame)) {
        if (aWidgetFlags) *aWidgetFlags = GTK_ORIENTATION_HORIZONTAL;
        aGtkWidgetType = MOZ_GTK_SCALE_THUMB_HORIZONTAL;
      } else {
        if (aWidgetFlags) *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
        aGtkWidgetType = MOZ_GTK_SCALE_THUMB_VERTICAL;
      }
      break;
    }
    case StyleAppearance::ScaleHorizontal:
      if (aWidgetFlags) *aWidgetFlags = GTK_ORIENTATION_HORIZONTAL;
      aGtkWidgetType = MOZ_GTK_SCALE_HORIZONTAL;
      break;
    case StyleAppearance::ScalethumbHorizontal:
      if (aWidgetFlags) *aWidgetFlags = GTK_ORIENTATION_HORIZONTAL;
      aGtkWidgetType = MOZ_GTK_SCALE_THUMB_HORIZONTAL;
      break;
    case StyleAppearance::ScaleVertical:
      if (aWidgetFlags) *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
      aGtkWidgetType = MOZ_GTK_SCALE_VERTICAL;
      break;
    case StyleAppearance::Separator:
      aGtkWidgetType = MOZ_GTK_TOOLBAR_SEPARATOR;
      break;
    case StyleAppearance::ScalethumbVertical:
      if (aWidgetFlags) *aWidgetFlags = GTK_ORIENTATION_VERTICAL;
      aGtkWidgetType = MOZ_GTK_SCALE_THUMB_VERTICAL;
      break;
    case StyleAppearance::Toolbargripper:
      aGtkWidgetType = MOZ_GTK_GRIPPER;
      break;
    case StyleAppearance::Resizer:
      aGtkWidgetType = MOZ_GTK_RESIZER;
      break;
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
      aGtkWidgetType = MOZ_GTK_ENTRY;
      break;
    case StyleAppearance::Textarea:
      aGtkWidgetType = MOZ_GTK_TEXT_VIEW;
      break;
    case StyleAppearance::Listbox:
    case StyleAppearance::Treeview:
      aGtkWidgetType = MOZ_GTK_TREEVIEW;
      break;
    case StyleAppearance::Treeheadercell:
      if (aWidgetFlags) {
        // In this case, the flag denotes whether the header is the sorted one
        // or not
        if (GetTreeSortDirection(aFrame) == eTreeSortDirection_Natural)
          *aWidgetFlags = false;
        else
          *aWidgetFlags = true;
      }
      aGtkWidgetType = MOZ_GTK_TREE_HEADER_CELL;
      break;
    case StyleAppearance::Treeheadersortarrow:
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
    case StyleAppearance::Treetwisty:
      aGtkWidgetType = MOZ_GTK_TREEVIEW_EXPANDER;
      if (aWidgetFlags) *aWidgetFlags = GTK_EXPANDER_COLLAPSED;
      break;
    case StyleAppearance::Treetwistyopen:
      aGtkWidgetType = MOZ_GTK_TREEVIEW_EXPANDER;
      if (aWidgetFlags) *aWidgetFlags = GTK_EXPANDER_EXPANDED;
      break;
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist:
      aGtkWidgetType = MOZ_GTK_DROPDOWN;
      if (aWidgetFlags)
        *aWidgetFlags =
            IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XHTML);
      break;
    case StyleAppearance::MenulistText:
      return false;  // nothing to do, but prevents the bg from being drawn
    case StyleAppearance::MenulistTextfield:
      aGtkWidgetType = MOZ_GTK_DROPDOWN_ENTRY;
      break;
    case StyleAppearance::MozMenulistArrowButton:
      aGtkWidgetType = MOZ_GTK_DROPDOWN_ARROW;
      break;
    case StyleAppearance::ToolbarbuttonDropdown:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowNext:
    case StyleAppearance::ButtonArrowPrevious:
      aGtkWidgetType = MOZ_GTK_TOOLBARBUTTON_ARROW;
      if (aWidgetFlags) {
        *aWidgetFlags = GTK_ARROW_DOWN;

        if (aAppearance == StyleAppearance::ButtonArrowUp)
          *aWidgetFlags = GTK_ARROW_UP;
        else if (aAppearance == StyleAppearance::ButtonArrowNext)
          *aWidgetFlags = GTK_ARROW_RIGHT;
        else if (aAppearance == StyleAppearance::ButtonArrowPrevious)
          *aWidgetFlags = GTK_ARROW_LEFT;
      }
      break;
    case StyleAppearance::CheckboxContainer:
      aGtkWidgetType = MOZ_GTK_CHECKBUTTON_CONTAINER;
      break;
    case StyleAppearance::RadioContainer:
      aGtkWidgetType = MOZ_GTK_RADIOBUTTON_CONTAINER;
      break;
    case StyleAppearance::CheckboxLabel:
      aGtkWidgetType = MOZ_GTK_CHECKBUTTON_LABEL;
      break;
    case StyleAppearance::RadioLabel:
      aGtkWidgetType = MOZ_GTK_RADIOBUTTON_LABEL;
      break;
    case StyleAppearance::Toolbar:
      aGtkWidgetType = MOZ_GTK_TOOLBAR;
      break;
    case StyleAppearance::Tooltip:
      aGtkWidgetType = MOZ_GTK_TOOLTIP;
      break;
    case StyleAppearance::Statusbarpanel:
    case StyleAppearance::Resizerpanel:
      aGtkWidgetType = MOZ_GTK_FRAME;
      break;
    case StyleAppearance::ProgressBar:
    case StyleAppearance::ProgressbarVertical:
      aGtkWidgetType = MOZ_GTK_PROGRESSBAR;
      break;
    case StyleAppearance::Progresschunk: {
      nsIFrame* stateFrame = aFrame->GetParent();
      EventStates eventStates = GetContentState(stateFrame, aAppearance);

      aGtkWidgetType = IsIndeterminateProgress(stateFrame, eventStates)
                           ? IsVerticalProgress(stateFrame)
                                 ? MOZ_GTK_PROGRESS_CHUNK_VERTICAL_INDETERMINATE
                                 : MOZ_GTK_PROGRESS_CHUNK_INDETERMINATE
                           : MOZ_GTK_PROGRESS_CHUNK;
    } break;
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward:
      if (aWidgetFlags)
        *aWidgetFlags = aAppearance == StyleAppearance::TabScrollArrowBack
                            ? GTK_ARROW_LEFT
                            : GTK_ARROW_RIGHT;
      aGtkWidgetType = MOZ_GTK_TAB_SCROLLARROW;
      break;
    case StyleAppearance::Tabpanels:
      aGtkWidgetType = MOZ_GTK_TABPANELS;
      break;
    case StyleAppearance::Tab: {
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

        if (IsSelectedTab(aFrame)) *aWidgetFlags |= MOZ_GTK_TAB_SELECTED;

        if (IsFirstTab(aFrame)) *aWidgetFlags |= MOZ_GTK_TAB_FIRST;
      }
    } break;
    case StyleAppearance::Splitter:
      if (IsHorizontal(aFrame))
        aGtkWidgetType = MOZ_GTK_SPLITTER_VERTICAL;
      else
        aGtkWidgetType = MOZ_GTK_SPLITTER_HORIZONTAL;
      break;
    case StyleAppearance::Menubar:
      aGtkWidgetType = MOZ_GTK_MENUBAR;
      break;
    case StyleAppearance::Menupopup:
      aGtkWidgetType = MOZ_GTK_MENUPOPUP;
      break;
    case StyleAppearance::Menuitem: {
      nsMenuFrame* menuFrame = do_QueryFrame(aFrame);
      if (menuFrame && menuFrame->IsOnMenuBar()) {
        aGtkWidgetType = MOZ_GTK_MENUBARITEM;
        break;
      }
    }
      aGtkWidgetType = MOZ_GTK_MENUITEM;
      break;
    case StyleAppearance::Menuseparator:
      aGtkWidgetType = MOZ_GTK_MENUSEPARATOR;
      break;
    case StyleAppearance::Menuarrow:
      aGtkWidgetType = MOZ_GTK_MENUARROW;
      break;
    case StyleAppearance::Checkmenuitem:
      aGtkWidgetType = MOZ_GTK_CHECKMENUITEM;
      break;
    case StyleAppearance::Radiomenuitem:
      aGtkWidgetType = MOZ_GTK_RADIOMENUITEM;
      break;
    case StyleAppearance::Window:
    case StyleAppearance::Dialog:
      aGtkWidgetType = MOZ_GTK_WINDOW;
      break;
    case StyleAppearance::MozGtkInfoBar:
      aGtkWidgetType = MOZ_GTK_INFO_BAR;
      break;
    case StyleAppearance::MozWindowTitlebar:
      aGtkWidgetType = MOZ_GTK_HEADER_BAR;
      break;
    case StyleAppearance::MozWindowTitlebarMaximized:
      aGtkWidgetType = MOZ_GTK_HEADER_BAR_MAXIMIZED;
      break;
    case StyleAppearance::MozWindowButtonBox:
      aGtkWidgetType = MOZ_GTK_HEADER_BAR_BUTTON_BOX;
      break;
    case StyleAppearance::MozWindowButtonClose:
      aGtkWidgetType = MOZ_GTK_HEADER_BAR_BUTTON_CLOSE;
      break;
    case StyleAppearance::MozWindowButtonMinimize:
      aGtkWidgetType = MOZ_GTK_HEADER_BAR_BUTTON_MINIMIZE;
      break;
    case StyleAppearance::MozWindowButtonMaximize:
      aGtkWidgetType = MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE;
      break;
    case StyleAppearance::MozWindowButtonRestore:
      aGtkWidgetType = MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE_RESTORE;
      break;
    default:
      return false;
  }

  return true;
}

class SystemCairoClipper : public ClipExporter {
 public:
  explicit SystemCairoClipper(cairo_t* aContext, gint aScaleFactor = 1)
      : mContext(aContext), mScaleFactor(aScaleFactor) {}

  void BeginClip(const Matrix& aTransform) override {
    cairo_matrix_t mat;
    GfxMatrixToCairoMatrix(aTransform, mat);
    // We also need to remove the scale factor effect from the matrix
    mat.y0 = mat.y0 / mScaleFactor;
    mat.x0 = mat.x0 / mScaleFactor;
    cairo_set_matrix(mContext, &mat);

    cairo_new_path(mContext);
  }

  void MoveTo(const Point& aPoint) override {
    cairo_move_to(mContext, aPoint.x / mScaleFactor, aPoint.y / mScaleFactor);
    mBeginPoint = aPoint;
    mCurrentPoint = aPoint;
  }

  void LineTo(const Point& aPoint) override {
    cairo_line_to(mContext, aPoint.x / mScaleFactor, aPoint.y / mScaleFactor);
    mCurrentPoint = aPoint;
  }

  void BezierTo(const Point& aCP1, const Point& aCP2,
                const Point& aCP3) override {
    cairo_curve_to(mContext, aCP1.x / mScaleFactor, aCP1.y / mScaleFactor,
                   aCP2.x / mScaleFactor, aCP2.y / mScaleFactor,
                   aCP3.x / mScaleFactor, aCP3.y / mScaleFactor);
    mCurrentPoint = aCP3;
  }

  void QuadraticBezierTo(const Point& aCP1, const Point& aCP2) override {
    Point CP0 = CurrentPoint();
    Point CP1 = (CP0 + aCP1 * 2.0) / 3.0;
    Point CP2 = (aCP2 + aCP1 * 2.0) / 3.0;
    Point CP3 = aCP2;
    cairo_curve_to(mContext, CP1.x / mScaleFactor, CP1.y / mScaleFactor,
                   CP2.x / mScaleFactor, CP2.y / mScaleFactor,
                   CP3.x / mScaleFactor, CP3.y / mScaleFactor);
    mCurrentPoint = aCP2;
  }

  void Arc(const Point& aOrigin, float aRadius, float aStartAngle,
           float aEndAngle, bool aAntiClockwise) override {
    ArcToBezier(this, aOrigin, Size(aRadius, aRadius), aStartAngle, aEndAngle,
                aAntiClockwise);
  }

  void Close() override {
    cairo_close_path(mContext);
    mCurrentPoint = mBeginPoint;
  }

  void EndClip() override { cairo_clip(mContext); }

 private:
  cairo_t* mContext;
  gint mScaleFactor;
};

static void DrawThemeWithCairo(gfxContext* aContext, DrawTarget* aDrawTarget,
                               GtkWidgetState aState,
                               WidgetNodeType aGTKWidgetType, gint aFlags,
                               GtkTextDirection aDirection, gint aScaleFactor,
                               bool aSnapped, const Point& aDrawOrigin,
                               const nsIntSize& aDrawSize,
                               GdkRectangle& aGDKRect,
                               nsITheme::Transparency aTransparency) {
  bool isX11Display = gfxPlatformGtk::GetPlatform()->IsX11Display();
  static auto sCairoSurfaceSetDeviceScalePtr =
      (void (*)(cairo_surface_t*, double, double))dlsym(
          RTLD_DEFAULT, "cairo_surface_set_device_scale");
  bool useHiDPIWidgets =
      (aScaleFactor != 1) && (sCairoSurfaceSetDeviceScalePtr != nullptr);

  Point drawOffsetScaled;
  Point drawOffsetOriginal;
  Matrix transform;
  if (!aSnapped) {
    // If we are not snapped, we depend on the DT for translation.
    drawOffsetOriginal = aDrawOrigin;
    drawOffsetScaled = useHiDPIWidgets ? drawOffsetOriginal / aScaleFactor
                                       : drawOffsetOriginal;
    transform = aDrawTarget->GetTransform().PreTranslate(drawOffsetScaled);
  } else {
    // Otherwise, we only need to take the device offset into account.
    drawOffsetOriginal = aDrawOrigin - aContext->GetDeviceOffset();
    drawOffsetScaled = useHiDPIWidgets ? drawOffsetOriginal / aScaleFactor
                                       : drawOffsetOriginal;
    transform = Matrix::Translation(drawOffsetScaled);
  }

  if (!useHiDPIWidgets && aScaleFactor != 1) {
    transform.PreScale(aScaleFactor, aScaleFactor);
  }

  cairo_matrix_t mat;
  GfxMatrixToCairoMatrix(transform, mat);

  nsIntSize clipSize((aDrawSize.width + aScaleFactor - 1) / aScaleFactor,
                     (aDrawSize.height + aScaleFactor - 1) / aScaleFactor);

  // A direct Cairo draw target is not available, so we need to create a
  // temporary one.
#if defined(MOZ_X11) && defined(CAIRO_HAS_XLIB_SURFACE)
  if (isX11Display) {
    // If using a Cairo xlib surface, then try to reuse it.
    BorrowedXlibDrawable borrow(aDrawTarget);
    if (borrow.GetDrawable()) {
      nsIntSize size = borrow.GetSize();
      cairo_surface_t* surf = nullptr;
      // Check if the surface is using XRender.
#  ifdef CAIRO_HAS_XLIB_XRENDER_SURFACE
      if (borrow.GetXRenderFormat()) {
        surf = cairo_xlib_surface_create_with_xrender_format(
            borrow.GetDisplay(), borrow.GetDrawable(), borrow.GetScreen(),
            borrow.GetXRenderFormat(), size.width, size.height);
      } else {
#  else
      if (!borrow.GetXRenderFormat()) {
#  endif
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

          moz_gtk_widget_paint(aGTKWidgetType, cr, &aGDKRect, &aState, aFlags,
                               aDirection);

          cairo_destroy(cr);
        }
        cairo_surface_destroy(surf);
      }
      borrow.Finish();
      return;
    }
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
    cairo_surface_t* surf = cairo_image_surface_create_for_data(
        data, GfxFormatToCairoFormat(format), size.width, size.height, stride);
    if (!NS_WARN_IF(!surf)) {
      if (useHiDPIWidgets) {
        sCairoSurfaceSetDeviceScalePtr(surf, aScaleFactor, aScaleFactor);
      }
      if (origin != IntPoint()) {
        cairo_surface_set_device_offset(surf, -origin.x, -origin.y);
      }
      cairo_t* cr = cairo_create(surf);
      if (!NS_WARN_IF(!cr)) {
        RefPtr<SystemCairoClipper> clipper =
            new SystemCairoClipper(cr, useHiDPIWidgets ? aScaleFactor : 1);
        aContext->ExportClip(*clipper);

        cairo_set_matrix(cr, &mat);

        cairo_new_path(cr);
        cairo_rectangle(cr, 0, 0, clipSize.width, clipSize.height);
        cairo_clip(cr);

        moz_gtk_widget_paint(aGTKWidgetType, cr, &aGDKRect, &aState, aFlags,
                             aDirection);

        cairo_destroy(cr);
      }
      cairo_surface_destroy(surf);
    }
    aDrawTarget->ReleaseBits(data);
  } else {
    // If the widget has any transparency, make sure to choose an alpha format.
    format = aTransparency != nsITheme::eOpaque ? SurfaceFormat::B8G8R8A8
                                                : aDrawTarget->GetFormat();
    // Create a temporary data surface to render the widget into.
    RefPtr<DataSourceSurface> dataSurface = Factory::CreateDataSourceSurface(
        aDrawSize, format, aTransparency != nsITheme::eOpaque);
    DataSourceSurface::MappedSurface map;
    if (!NS_WARN_IF(
            !(dataSurface &&
              dataSurface->Map(DataSourceSurface::MapType::WRITE, &map)))) {
      // Create a Cairo image surface wrapping the data surface.
      cairo_surface_t* surf = cairo_image_surface_create_for_data(
          map.mData, GfxFormatToCairoFormat(format), aDrawSize.width,
          aDrawSize.height, map.mStride);
      cairo_t* cr = nullptr;
      if (!NS_WARN_IF(!surf)) {
        cr = cairo_create(surf);
        if (!NS_WARN_IF(!cr)) {
          if (aScaleFactor != 1) {
            if (useHiDPIWidgets) {
              sCairoSurfaceSetDeviceScalePtr(surf, aScaleFactor, aScaleFactor);
            } else {
              cairo_scale(cr, aScaleFactor, aScaleFactor);
            }
          }

          moz_gtk_widget_paint(aGTKWidgetType, cr, &aGDKRect, &aState, aFlags,
                               aDirection);
        }
      }

      // Unmap the surface before using it as a source
      dataSurface->Unmap();

      if (cr) {
        // The widget either needs to be masked or has transparency, so use the
        // slower drawing path.
        aDrawTarget->DrawSurface(
            dataSurface,
            Rect(aSnapped ? drawOffsetOriginal -
                                aDrawTarget->GetTransform().GetTranslation()
                          : drawOffsetOriginal,
                 Size(aDrawSize)),
            Rect(0, 0, aDrawSize.width, aDrawSize.height));
        cairo_destroy(cr);
      }

      if (surf) {
        cairo_surface_destroy(surf);
      }
    }
  }
}

bool nsNativeThemeGTK::GetExtraSizeForWidget(nsIFrame* aFrame,
                                             StyleAppearance aAppearance,
                                             nsIntMargin* aExtra) {
  *aExtra = nsIntMargin(0, 0, 0, 0);
  // Allow an extra one pixel above and below the thumb for certain
  // GTK2 themes (Ximian Industrial, Bluecurve, Misty, at least);
  // We modify the frame's overflow area.  See bug 297508.
  switch (aAppearance) {
    case StyleAppearance::ScrollbarthumbVertical:
      aExtra->top = aExtra->bottom = 1;
      break;
    case StyleAppearance::ScrollbarthumbHorizontal:
      aExtra->left = aExtra->right = 1;
      break;

    case StyleAppearance::Button: {
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
    case StyleAppearance::FocusOutline: {
      moz_gtk_get_focus_outline_size(&aExtra->left, &aExtra->top);
      aExtra->right = aExtra->left;
      aExtra->bottom = aExtra->top;
      break;
    }
    case StyleAppearance::Tab: {
      if (!IsSelectedTab(aFrame)) return false;

      gint gap_height = moz_gtk_get_tab_thickness(
          IsBottomTab(aFrame) ? MOZ_GTK_TAB_BOTTOM : MOZ_GTK_TAB_TOP);
      if (!gap_height) return false;

      int32_t extra = gap_height - GetTabMarginPixels(aFrame);
      if (extra <= 0) return false;

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
  gint scale = GetMonitorScaleFactor(aFrame);
  aExtra->top *= scale;
  aExtra->right *= scale;
  aExtra->bottom *= scale;
  aExtra->left *= scale;
  return true;
}

bool nsNativeThemeGTK::IsWidgetVisible(StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::MozWindowButtonBox:
      return false;
    default:
      break;
  }
  return true;
}

NS_IMETHODIMP
nsNativeThemeGTK::DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       const nsRect& aRect,
                                       const nsRect& aDirtyRect) {
  GtkWidgetState state;
  WidgetNodeType gtkWidgetType;
  GtkTextDirection direction = GetTextDirection(aFrame);
  gint flags;

  if (!IsWidgetVisible(aAppearance) ||
      !GetGtkWidgetAndState(aAppearance, aFrame, gtkWidgetType, &state,
                            &flags)) {
    return NS_OK;
  }

  gfxContext* ctx = aContext;
  nsPresContext* presContext = aFrame->PresContext();

  gfxRect rect = presContext->AppUnitsToGfxUnits(aRect);
  gfxRect dirtyRect = presContext->AppUnitsToGfxUnits(aDirtyRect);
  gint scaleFactor = GetMonitorScaleFactor(aFrame);

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
  if (GetExtraSizeForWidget(aFrame, aAppearance, &extraSize)) {
    overflowRect.Inflate(extraSize);
  }

  // This is the rectangle that will actually be drawn, in gdk pixels
  nsIntRect drawingRect(int32_t(dirtyRect.X()), int32_t(dirtyRect.Y()),
                        int32_t(dirtyRect.Width()),
                        int32_t(dirtyRect.Height()));
  if (widgetRect.IsEmpty() ||
      !drawingRect.IntersectRect(overflowRect, drawingRect))
    return NS_OK;

  NS_ASSERTION(!IsWidgetTypeDisabled(mDisabledWidgetTypes, aAppearance),
               "Trying to render an unsafe widget!");

  bool safeState = IsWidgetStateSafe(mSafeWidgetStates, aAppearance, &state);
  if (!safeState) {
    gLastGdkError = 0;
    gdk_error_trap_push();
  }

  Transparency transparency = GetWidgetTransparency(aFrame, aAppearance);

  // gdk rectangles are wrt the drawing rect.
  GdkRectangle gdk_rect = {
      -drawingRect.x / scaleFactor, -drawingRect.y / scaleFactor,
      widgetRect.width / scaleFactor, widgetRect.height / scaleFactor};

  // Save actual widget scale to GtkWidgetState as we don't provide
  // nsFrame to gtk3drawing routines.
  state.scale = scaleFactor;

  // translate everything so (0,0) is the top left of the drawingRect
  gfxPoint origin = rect.TopLeft() + drawingRect.TopLeft();

  DrawThemeWithCairo(ctx, aContext->GetDrawTarget(), state, gtkWidgetType,
                     flags, direction, scaleFactor, snapped, ToPoint(origin),
                     drawingRect.Size(), gdk_rect, transparency);

  if (!safeState) {
    // gdk_flush() call from expose event crashes Gtk+ on Wayland
    // (Gnome BZ #773307)
    if (gfxPlatformGtk::GetPlatform()->IsX11Display()) {
      gdk_flush();
    }
    gLastGdkError = gdk_error_trap_pop();

    if (gLastGdkError) {
#ifdef DEBUG
      printf(
          "GTK theme failed for widget type %d, error was %d, state was "
          "[active=%d,focused=%d,inHover=%d,disabled=%d]\n",
          static_cast<int>(aAppearance), gLastGdkError, state.active,
          state.focused, state.inHover, state.disabled);
#endif
      NS_WARNING("GTK theme failed; disabling unsafe widget");
      SetWidgetTypeDisabled(mDisabledWidgetTypes, aAppearance);
      // force refresh of the window, because the widget was not
      // successfully drawn it must be redrawn using the default look
      RefreshWidgetWindow(aFrame);
    } else {
      SetWidgetStateSafe(mSafeWidgetStates, aAppearance, &state);
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

bool nsNativeThemeGTK::CreateWebRenderCommandsForWidget(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const mozilla::layers::StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager, nsIFrame* aFrame,
    StyleAppearance aAppearance, const nsRect& aRect) {
  nsPresContext* presContext = aFrame->PresContext();
  wr::LayoutRect bounds = wr::ToLayoutRect(LayoutDeviceRect::FromAppUnits(
      aRect, presContext->AppUnitsPerDevPixel()));

  switch (aAppearance) {
    case StyleAppearance::Window:
    case StyleAppearance::Dialog:
      aBuilder.PushRect(
          bounds, bounds, true,
          wr::ToColorF(ToDeviceColor(LookAndFeel::GetColor(
              LookAndFeel::ColorID::WindowBackground, NS_RGBA(0, 0, 0, 0)))));
      return true;

    default:
      return false;
  }
}

WidgetNodeType nsNativeThemeGTK::NativeThemeToGtkTheme(
    StyleAppearance aAppearance, nsIFrame* aFrame) {
  WidgetNodeType gtkWidgetType;
  gint unusedFlags;

  if (!GetGtkWidgetAndState(aAppearance, aFrame, gtkWidgetType, nullptr,
                            &unusedFlags)) {
    MOZ_ASSERT_UNREACHABLE("Unknown native widget to gtk widget mapping");
    return MOZ_GTK_WINDOW;
  }
  return gtkWidgetType;
}

static void FixupForVerticalWritingMode(WritingMode aWritingMode,
                                        LayoutDeviceIntMargin* aResult) {
  if (aWritingMode.IsVertical()) {
    bool rtl = aWritingMode.IsBidiRTL();
    LogicalMargin logical(aWritingMode, aResult->top,
                          rtl ? aResult->left : aResult->right, aResult->bottom,
                          rtl ? aResult->right : aResult->left);
    nsMargin physical = logical.GetPhysicalMargin(aWritingMode);
    aResult->top = physical.top;
    aResult->right = physical.right;
    aResult->bottom = physical.bottom;
    aResult->left = physical.left;
  }
}

void nsNativeThemeGTK::GetCachedWidgetBorder(nsIFrame* aFrame,
                                             StyleAppearance aAppearance,
                                             GtkTextDirection aDirection,
                                             LayoutDeviceIntMargin* aResult) {
  aResult->SizeTo(0, 0, 0, 0);

  WidgetNodeType gtkWidgetType;
  gint unusedFlags;
  if (GetGtkWidgetAndState(aAppearance, aFrame, gtkWidgetType, nullptr,
                           &unusedFlags)) {
    MOZ_ASSERT(0 <= gtkWidgetType && gtkWidgetType < MOZ_GTK_WIDGET_NODE_COUNT);
    uint8_t cacheIndex = gtkWidgetType / 8;
    uint8_t cacheBit = 1u << (gtkWidgetType % 8);

    if (mBorderCacheValid[cacheIndex] & cacheBit) {
      *aResult = mBorderCache[gtkWidgetType];
    } else {
      moz_gtk_get_widget_border(gtkWidgetType, &aResult->left, &aResult->top,
                                &aResult->right, &aResult->bottom, aDirection);
      if (gtkWidgetType != MOZ_GTK_DROPDOWN) {  // depends on aDirection
        mBorderCacheValid[cacheIndex] |= cacheBit;
        mBorderCache[gtkWidgetType] = *aResult;
      }
    }
  }
  FixupForVerticalWritingMode(aFrame->GetWritingMode(), aResult);
}

LayoutDeviceIntMargin nsNativeThemeGTK::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  LayoutDeviceIntMargin result;
  GtkTextDirection direction = GetTextDirection(aFrame);
  switch (aAppearance) {
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical: {
      GtkOrientation orientation =
          aAppearance == StyleAppearance::ScrollbarHorizontal
              ? GTK_ORIENTATION_HORIZONTAL
              : GTK_ORIENTATION_VERTICAL;
      const ScrollbarGTKMetrics* metrics =
          GetActiveScrollbarMetrics(orientation);

      const GtkBorder& border = metrics->border.scrollbar;
      result.top = border.top;
      result.right = border.right;
      result.bottom = border.bottom;
      result.left = border.left;
    } break;
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical: {
      GtkOrientation orientation =
          aAppearance == StyleAppearance::ScrollbartrackHorizontal
              ? GTK_ORIENTATION_HORIZONTAL
              : GTK_ORIENTATION_VERTICAL;
      const ScrollbarGTKMetrics* metrics =
          GetActiveScrollbarMetrics(orientation);

      const GtkBorder& border = metrics->border.track;
      result.top = border.top;
      result.right = border.right;
      result.bottom = border.bottom;
      result.left = border.left;
    } break;
    case StyleAppearance::Toolbox:
      // gtk has no toolbox equivalent.  So, although we map toolbox to
      // gtk's 'toolbar' for purposes of painting the widget background,
      // we don't use the toolbar border for toolbox.
      break;
    case StyleAppearance::Dualbutton:
      // TOOLBAR_DUAL_BUTTON is an interesting case.  We want a border to draw
      // around the entire button + dropdown, and also an inner border if you're
      // over the button part.  But, we want the inner button to be right up
      // against the edge of the outer button so that the borders overlap.
      // To make this happen, we draw a button border for the outer button,
      // but don't reserve any space for it.
      break;
    case StyleAppearance::Tab: {
      WidgetNodeType gtkWidgetType;
      gint flags;

      if (!GetGtkWidgetAndState(aAppearance, aFrame, gtkWidgetType, nullptr,
                                &flags)) {
        return result;
      }
      moz_gtk_get_tab_border(&result.left, &result.top, &result.right,
                             &result.bottom, direction, (GtkTabFlags)flags,
                             gtkWidgetType);
    } break;
    case StyleAppearance::Menuitem:
    case StyleAppearance::Checkmenuitem:
    case StyleAppearance::Radiomenuitem:
      // For regular menuitems, we will be using GetWidgetPadding instead of
      // GetWidgetBorder to pad up the widget's internals; other menuitems
      // will need to fall through and use the default case as before.
      if (IsRegularMenuItem(aFrame)) break;
      [[fallthrough]];
    default: {
      GetCachedWidgetBorder(aFrame, aAppearance, direction, &result);
    }
  }

  gint scale = GetMonitorScaleFactor(aFrame);
  result.top *= scale;
  result.right *= scale;
  result.bottom *= scale;
  result.left *= scale;
  return result;
}

bool nsNativeThemeGTK::GetWidgetPadding(nsDeviceContext* aContext,
                                        nsIFrame* aFrame,
                                        StyleAppearance aAppearance,
                                        LayoutDeviceIntMargin* aResult) {
  switch (aAppearance) {
    case StyleAppearance::ButtonFocus:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::MozWindowButtonBox:
    case StyleAppearance::MozWindowButtonClose:
    case StyleAppearance::MozWindowButtonMinimize:
    case StyleAppearance::MozWindowButtonMaximize:
    case StyleAppearance::MozWindowButtonRestore:
    case StyleAppearance::Dualbutton:
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward:
    case StyleAppearance::MozMenulistArrowButton:
    case StyleAppearance::ToolbarbuttonDropdown:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::ButtonArrowNext:
    case StyleAppearance::ButtonArrowPrevious:
    case StyleAppearance::RangeThumb:
    // Radios and checkboxes return a fixed size in GetMinimumWidgetSize
    // and have a meaningful baseline, so they can't have
    // author-specified padding.
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio:
      aResult->SizeTo(0, 0, 0, 0);
      return true;
    case StyleAppearance::Menuitem:
    case StyleAppearance::Checkmenuitem:
    case StyleAppearance::Radiomenuitem: {
      // Menubar and menulist have their padding specified in CSS.
      if (!IsRegularMenuItem(aFrame)) return false;

      GetCachedWidgetBorder(aFrame, aAppearance, GetTextDirection(aFrame),
                            aResult);

      gint horizontal_padding;
      if (aAppearance == StyleAppearance::Menuitem)
        moz_gtk_menuitem_get_horizontal_padding(&horizontal_padding);
      else
        moz_gtk_checkmenuitem_get_horizontal_padding(&horizontal_padding);

      aResult->left += horizontal_padding;
      aResult->right += horizontal_padding;

      gint scale = GetMonitorScaleFactor(aFrame);
      aResult->top *= scale;
      aResult->right *= scale;
      aResult->bottom *= scale;
      aResult->left *= scale;

      return true;
    }
    default:
      break;
  }

  return false;
}

bool nsNativeThemeGTK::GetWidgetOverflow(nsDeviceContext* aContext,
                                         nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         nsRect* aOverflowRect) {
  nsIntMargin extraSize;
  if (!GetExtraSizeForWidget(aFrame, aAppearance, &extraSize)) return false;

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
                                       nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       LayoutDeviceIntSize* aResult,
                                       bool* aIsOverridable) {
  aResult->width = aResult->height = 0;
  *aIsOverridable = true;

  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown: {
      const ScrollbarGTKMetrics* metrics =
          GetActiveScrollbarMetrics(GTK_ORIENTATION_VERTICAL);

      aResult->width = metrics->size.button.width;
      aResult->height = metrics->size.button.height;
      *aIsOverridable = false;
    } break;
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight: {
      const ScrollbarGTKMetrics* metrics =
          GetActiveScrollbarMetrics(GTK_ORIENTATION_HORIZONTAL);

      aResult->width = metrics->size.button.width;
      aResult->height = metrics->size.button.height;
      *aIsOverridable = false;
    } break;
    case StyleAppearance::Splitter: {
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
    } break;
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical: {
      /* While we enforce a minimum size for the thumb, this is ignored
       * for the some scrollbars if buttons are hidden (bug 513006) because
       * the thumb isn't a direct child of the scrollbar, unlike the buttons
       * or track. So add a minimum size to the track as well to prevent a
       * 0-width scrollbar. */
      GtkOrientation orientation =
          aAppearance == StyleAppearance::ScrollbarHorizontal
              ? GTK_ORIENTATION_HORIZONTAL
              : GTK_ORIENTATION_VERTICAL;
      const ScrollbarGTKMetrics* metrics =
          GetActiveScrollbarMetrics(orientation);

      aResult->width = metrics->size.scrollbar.width;
      aResult->height = metrics->size.scrollbar.height;
    } break;
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarthumbHorizontal: {
      GtkOrientation orientation =
          aAppearance == StyleAppearance::ScrollbarthumbHorizontal
              ? GTK_ORIENTATION_HORIZONTAL
              : GTK_ORIENTATION_VERTICAL;
      const ScrollbarGTKMetrics* metrics =
          GetActiveScrollbarMetrics(orientation);

      aResult->width = metrics->size.thumb.width;
      aResult->height = metrics->size.thumb.height;
      *aIsOverridable = false;
    } break;
    case StyleAppearance::RangeThumb: {
      gint thumb_length, thumb_height;

      if (IsRangeHorizontal(aFrame)) {
        moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_HORIZONTAL,
                                       &thumb_length, &thumb_height);
      } else {
        moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_VERTICAL, &thumb_height,
                                       &thumb_length);
      }
      aResult->width = thumb_length;
      aResult->height = thumb_height;

      *aIsOverridable = false;
    } break;
    case StyleAppearance::ScalethumbHorizontal:
    case StyleAppearance::ScalethumbVertical: {
      gint thumb_length, thumb_height;

      if (aAppearance == StyleAppearance::ScalethumbVertical) {
        moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_VERTICAL, &thumb_length,
                                       &thumb_height);
        aResult->width = thumb_height;
        aResult->height = thumb_length;
      } else {
        moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_HORIZONTAL,
                                       &thumb_length, &thumb_height);
        aResult->width = thumb_length;
        aResult->height = thumb_height;
      }

      *aIsOverridable = false;
    } break;
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward: {
      moz_gtk_get_tab_scroll_arrow_size(&aResult->width, &aResult->height);
      *aIsOverridable = false;
    } break;
    case StyleAppearance::MozMenulistArrowButton: {
      moz_gtk_get_combo_box_entry_button_size(&aResult->width,
                                              &aResult->height);
      *aIsOverridable = false;
    } break;
    case StyleAppearance::Menuseparator: {
      gint separator_height;

      moz_gtk_get_menu_separator_height(&separator_height);
      aResult->height = separator_height;

      *aIsOverridable = false;
    } break;
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
      const ToggleGTKMetrics* metrics = GetToggleMetrics(
          aAppearance == StyleAppearance::Radio ? MOZ_GTK_RADIOBUTTON
                                                : MOZ_GTK_CHECKBUTTON);
      aResult->width = metrics->minSizeWithBorder.width;
      aResult->height = metrics->minSizeWithBorder.height;
    } break;
    case StyleAppearance::ToolbarbuttonDropdown:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::ButtonArrowNext:
    case StyleAppearance::ButtonArrowPrevious: {
      moz_gtk_get_arrow_size(MOZ_GTK_TOOLBARBUTTON_ARROW, &aResult->width,
                             &aResult->height);
      *aIsOverridable = false;
    } break;
    case StyleAppearance::MozWindowButtonClose: {
      const ToolbarButtonGTKMetrics* metrics =
          GetToolbarButtonMetrics(MOZ_GTK_HEADER_BAR_BUTTON_CLOSE);
      aResult->width = metrics->minSizeWithBorderMargin.width;
      aResult->height = metrics->minSizeWithBorderMargin.height;
      break;
    }
    case StyleAppearance::MozWindowButtonMinimize: {
      const ToolbarButtonGTKMetrics* metrics =
          GetToolbarButtonMetrics(MOZ_GTK_HEADER_BAR_BUTTON_MINIMIZE);
      aResult->width = metrics->minSizeWithBorderMargin.width;
      aResult->height = metrics->minSizeWithBorderMargin.height;
      break;
    }
    case StyleAppearance::MozWindowButtonMaximize:
    case StyleAppearance::MozWindowButtonRestore: {
      const ToolbarButtonGTKMetrics* metrics =
          GetToolbarButtonMetrics(MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE);
      aResult->width = metrics->minSizeWithBorderMargin.width;
      aResult->height = metrics->minSizeWithBorderMargin.height;
      break;
    }
    case StyleAppearance::CheckboxContainer:
    case StyleAppearance::RadioContainer:
    case StyleAppearance::CheckboxLabel:
    case StyleAppearance::RadioLabel:
    case StyleAppearance::Button:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Treeheadercell: {
      if (aAppearance == StyleAppearance::Menulist ||
          aAppearance == StyleAppearance::MenulistButton) {
        // Include the arrow size.
        moz_gtk_get_arrow_size(MOZ_GTK_DROPDOWN, &aResult->width,
                               &aResult->height);
      }
      // else the minimum size is missing consideration of container
      // descendants; the value returned here will not be helpful, but the
      // box model may consider border and padding with child minimum sizes.

      LayoutDeviceIntMargin border;
      GetCachedWidgetBorder(aFrame, aAppearance, GetTextDirection(aFrame),
                            &border);
      aResult->width += border.left + border.right;
      aResult->height += border.top + border.bottom;
    } break;
    case StyleAppearance::MenulistTextfield:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield: {
      moz_gtk_get_entry_min_height(aFrame->GetWritingMode().IsVertical()
                                       ? &aResult->width
                                       : &aResult->height);
    } break;
    case StyleAppearance::Separator: {
      gint separator_width;

      moz_gtk_get_toolbar_separator_width(&separator_width);

      aResult->width = separator_width;
    } break;
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::Spinner:
      // hard code these sizes
      aResult->width = 14;
      aResult->height = 26;
      break;
    case StyleAppearance::Treeheadersortarrow:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      // hard code these sizes
      aResult->width = 14;
      aResult->height = 13;
      break;
    case StyleAppearance::Resizer:
      // same as Windows to make our lives easier
      aResult->width = aResult->height = 15;
      *aIsOverridable = false;
      break;
    case StyleAppearance::Treetwisty:
    case StyleAppearance::Treetwistyopen: {
      gint expander_size;

      moz_gtk_get_treeview_expander_size(&expander_size);
      aResult->width = aResult->height = expander_size;
      *aIsOverridable = false;
    } break;
    default:
      break;
  }

  *aResult = *aResult * GetMonitorScaleFactor(aFrame);

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::WidgetStateChanged(nsIFrame* aFrame,
                                     StyleAppearance aAppearance,
                                     nsAtom* aAttribute, bool* aShouldRepaint,
                                     const nsAttrValue* aOldValue) {
  // Some widget types just never change state.
  if (aAppearance == StyleAppearance::Toolbox ||
      aAppearance == StyleAppearance::Toolbar ||
      aAppearance == StyleAppearance::Statusbar ||
      aAppearance == StyleAppearance::Statusbarpanel ||
      aAppearance == StyleAppearance::Resizerpanel ||
      aAppearance == StyleAppearance::Progresschunk ||
      aAppearance == StyleAppearance::ProgressBar ||
      aAppearance == StyleAppearance::ProgressbarVertical ||
      aAppearance == StyleAppearance::Menubar ||
      aAppearance == StyleAppearance::Menupopup ||
      aAppearance == StyleAppearance::Tooltip ||
      aAppearance == StyleAppearance::Menuseparator ||
      aAppearance == StyleAppearance::Window ||
      aAppearance == StyleAppearance::Dialog) {
    *aShouldRepaint = false;
    return NS_OK;
  }

  if (aAppearance == StyleAppearance::MozWindowTitlebar ||
      aAppearance == StyleAppearance::MozWindowTitlebarMaximized ||
      aAppearance == StyleAppearance::MozWindowButtonClose ||
      aAppearance == StyleAppearance::MozWindowButtonMinimize ||
      aAppearance == StyleAppearance::MozWindowButtonMaximize ||
      aAppearance == StyleAppearance::MozWindowButtonRestore) {
    *aShouldRepaint = true;
    return NS_OK;
  }

  if ((aAppearance == StyleAppearance::ScrollbarthumbVertical ||
       aAppearance == StyleAppearance::ScrollbarthumbHorizontal) &&
      aAttribute == nsGkAtoms::active) {
    *aShouldRepaint = true;
    return NS_OK;
  }

  if ((aAppearance == StyleAppearance::ScrollbarbuttonUp ||
       aAppearance == StyleAppearance::ScrollbarbuttonDown ||
       aAppearance == StyleAppearance::ScrollbarbuttonLeft ||
       aAppearance == StyleAppearance::ScrollbarbuttonRight) &&
      (aAttribute == nsGkAtoms::curpos || aAttribute == nsGkAtoms::maxpos)) {
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
        bool disabledBefore =
            ShouldScrollbarButtonBeDisabled(oldCurpos, maxpos, aAppearance);
        bool disabledNow =
            ShouldScrollbarButtonBeDisabled(curpos, maxpos, aAppearance);
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
  } else {
    // Check the attribute to see if it's relevant.
    // disabled, checked, dlgtype, default, etc.
    *aShouldRepaint = false;
    if (aAttribute == nsGkAtoms::disabled || aAttribute == nsGkAtoms::checked ||
        aAttribute == nsGkAtoms::selected ||
        aAttribute == nsGkAtoms::visuallyselected ||
        aAttribute == nsGkAtoms::focused || aAttribute == nsGkAtoms::readonly ||
        aAttribute == nsGkAtoms::_default ||
        aAttribute == nsGkAtoms::menuactive || aAttribute == nsGkAtoms::open ||
        aAttribute == nsGkAtoms::parentfocused)
      *aShouldRepaint = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeGTK::ThemeChanged() {
  memset(mDisabledWidgetTypes, 0, sizeof(mDisabledWidgetTypes));
  memset(mSafeWidgetStates, 0, sizeof(mSafeWidgetStates));
  memset(mBorderCacheValid, 0, sizeof(mBorderCacheValid));
  return NS_OK;
}

NS_IMETHODIMP_(bool)
nsNativeThemeGTK::ThemeSupportsWidget(nsPresContext* aPresContext,
                                      nsIFrame* aFrame,
                                      StyleAppearance aAppearance) {
  if (IsWidgetTypeDisabled(mDisabledWidgetTypes, aAppearance)) return false;

  if (IsWidgetScrollbarPart(aAppearance)) {
    ComputedStyle* cs = nsLayoutUtils::StyleForScrollbar(aFrame);
    if (cs->StyleUI()->HasCustomScrollbars() ||
        // We cannot handle thin scrollbar on GTK+ widget directly as well.
        cs->StyleUIReset()->mScrollbarWidth == StyleScrollbarWidth::Thin) {
      return false;
    }
  }

  switch (aAppearance) {
    // Combobox dropdowns don't support native theming in vertical mode.
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::MenulistText:
      if (aFrame && aFrame->GetWritingMode().IsVertical()) {
        return false;
      }
      [[fallthrough]];

    case StyleAppearance::Button:
    case StyleAppearance::ButtonFocus:
    case StyleAppearance::Radio:
    case StyleAppearance::Checkbox:
    case StyleAppearance::Toolbox:  // N/A
    case StyleAppearance::Toolbar:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Dualbutton:  // so we can override the border with 0
    case StyleAppearance::ToolbarbuttonDropdown:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::ButtonArrowNext:
    case StyleAppearance::ButtonArrowPrevious:
    case StyleAppearance::Separator:
    case StyleAppearance::Toolbargripper:
    case StyleAppearance::Statusbar:
    case StyleAppearance::Statusbarpanel:
    case StyleAppearance::Resizerpanel:
    case StyleAppearance::Resizer:
    case StyleAppearance::Listbox:
      // case StyleAppearance::Listitem:
    case StyleAppearance::Treeview:
      // case StyleAppearance::Treeitem:
    case StyleAppearance::Treetwisty:
      // case StyleAppearance::Treeline:
      // case StyleAppearance::Treeheader:
    case StyleAppearance::Treeheadercell:
    case StyleAppearance::Treeheadersortarrow:
    case StyleAppearance::Treetwistyopen:
    case StyleAppearance::ProgressBar:
    case StyleAppearance::Progresschunk:
    case StyleAppearance::ProgressbarVertical:
    case StyleAppearance::Tab:
    // case StyleAppearance::Tabpanel:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward:
    case StyleAppearance::Tooltip:
    case StyleAppearance::InnerSpinButton:
    case StyleAppearance::Spinner:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
    case StyleAppearance::SpinnerTextfield:
      // case StyleAppearance::Scrollbar:  (n/a for gtk)
      // case StyleAppearance::ScrollbarSmall: (n/a for gtk)
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::MenulistTextfield:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Range:
    case StyleAppearance::RangeThumb:
    case StyleAppearance::ScaleHorizontal:
    case StyleAppearance::ScalethumbHorizontal:
    case StyleAppearance::ScaleVertical:
    case StyleAppearance::ScalethumbVertical:
      // case StyleAppearance::Scalethumbstart:
      // case StyleAppearance::Scalethumbend:
      // case StyleAppearance::Scalethumbtick:
    case StyleAppearance::CheckboxContainer:
    case StyleAppearance::RadioContainer:
    case StyleAppearance::CheckboxLabel:
    case StyleAppearance::RadioLabel:
    case StyleAppearance::Menubar:
    case StyleAppearance::Menupopup:
    case StyleAppearance::Menuitem:
    case StyleAppearance::Menuarrow:
    case StyleAppearance::Menuseparator:
    case StyleAppearance::Checkmenuitem:
    case StyleAppearance::Radiomenuitem:
    case StyleAppearance::Splitter:
    case StyleAppearance::Window:
    case StyleAppearance::Dialog:
    case StyleAppearance::MozGtkInfoBar:
      return !IsWidgetStyled(aPresContext, aFrame, aAppearance);

    case StyleAppearance::MozWindowButtonBox:
    case StyleAppearance::MozWindowButtonClose:
    case StyleAppearance::MozWindowButtonMinimize:
    case StyleAppearance::MozWindowButtonMaximize:
    case StyleAppearance::MozWindowButtonRestore:
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::MozWindowTitlebarMaximized:
      // GtkHeaderBar is available on GTK 3.10+, which is used for styling
      // title bars and title buttons.
      return gtk_check_version(3, 10, 0) == nullptr &&
             !IsWidgetStyled(aPresContext, aFrame, aAppearance);

    case StyleAppearance::MozMenulistArrowButton:
      if (aFrame && aFrame->GetWritingMode().IsVertical()) {
        return false;
      }
      // "Native" dropdown buttons cause padding and margin problems, but only
      // in HTML so allow them in XUL.
      return (!aFrame ||
              IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) &&
             !IsWidgetStyled(aPresContext, aFrame, aAppearance);

    case StyleAppearance::FocusOutline:
      return true;
    default:
      break;
  }

  return false;
}

NS_IMETHODIMP_(bool)
nsNativeThemeGTK::WidgetIsContainer(StyleAppearance aAppearance) {
  // XXXdwh At some point flesh all of this out.
  if (aAppearance == StyleAppearance::MozMenulistArrowButton ||
      aAppearance == StyleAppearance::Radio ||
      aAppearance == StyleAppearance::RangeThumb ||
      aAppearance == StyleAppearance::Checkbox ||
      aAppearance == StyleAppearance::TabScrollArrowBack ||
      aAppearance == StyleAppearance::TabScrollArrowForward ||
      aAppearance == StyleAppearance::ButtonArrowUp ||
      aAppearance == StyleAppearance::ButtonArrowDown ||
      aAppearance == StyleAppearance::ButtonArrowNext ||
      aAppearance == StyleAppearance::ButtonArrowPrevious)
    return false;
  return true;
}

bool nsNativeThemeGTK::ThemeDrawsFocusForWidget(StyleAppearance aAppearance) {
  if (aAppearance == StyleAppearance::Menulist ||
      aAppearance == StyleAppearance::MenulistButton ||
      aAppearance == StyleAppearance::Button ||
      aAppearance == StyleAppearance::Treeheadercell)
    return true;

  return false;
}

bool nsNativeThemeGTK::ThemeNeedsComboboxDropmarker() { return false; }

nsITheme::Transparency nsNativeThemeGTK::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  switch (aAppearance) {
    // These widgets always draw a default background.
    case StyleAppearance::Menupopup:
    case StyleAppearance::Window:
    case StyleAppearance::Dialog:
      return eOpaque;
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarHorizontal:
      // Make scrollbar tracks opaque on the window's scroll frame to prevent
      // leaf layers from overlapping. See bug 1179780.
      if (!(CheckBooleanAttr(aFrame, nsGkAtoms::root_) &&
            aFrame->PresContext()->IsRootContentDocument() &&
            IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL))) {
        return eTransparent;
      }
      return eOpaque;
    // Tooltips use gtk_paint_flat_box() on Gtk2
    // but are shaped on Gtk3
    case StyleAppearance::Tooltip:
      return eTransparent;
    default:
      return eUnknownTransparency;
  }
}

bool nsNativeThemeGTK::WidgetAppearanceDependsOnWindowFocus(
    StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarthumbHorizontal:
      return true;
    default:
      return false;
  }
}

already_AddRefed<nsITheme> do_GetNativeThemeDoNotUseDirectly() {
  static nsCOMPtr<nsITheme> inst;

  if (!inst) {
    if (gfxPlatform::IsHeadless()) {
      inst = new HeadlessThemeGTK();
    } else {
      inst = new nsNativeThemeGTK();
    }
    ClearOnShutdown(&inst);
  }

  return do_AddRef(inst);
}
