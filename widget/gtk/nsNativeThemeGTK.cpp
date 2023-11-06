/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeThemeGTK.h"
#include "nsPresContext.h"
#include "nsStyleConsts.h"
#include "gtkdrawing.h"
#include "ScreenHelperGTK.h"
#include "WidgetUtilsGtk.h"

#include "gfx2DGlue.h"
#include "nsIObserverService.h"
#include "nsIFrame.h"
#include "nsIContent.h"
#include "nsViewManager.h"
#include "nsNameSpaceManager.h"
#include "nsGfxCIID.h"
#include "nsTransform2D.h"
#include "nsXULPopupManager.h"
#include "tree/nsTreeBodyFrame.h"
#include "prlink.h"
#include "nsGkAtoms.h"
#include "nsAttrValueInlines.h"

#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"

#include <gdk/gdkprivate.h>
#include <gtk/gtk.h>

#include "gfxContext.h"
#include "mozilla/dom/XULButtonElement.h"
#include "mozilla/gfx/BorrowedContext.h"
#include "mozilla/gfx/HelpersCairo.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_widget.h"
#include "nsWindow.h"
#include "nsLayoutUtils.h"
#include "Theme.h"

#ifdef MOZ_X11
#  ifdef CAIRO_HAS_XLIB_SURFACE
#    include "cairo-xlib.h"
#  endif
#endif

#include <algorithm>
#include <dlfcn.h>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::widget;

static int gLastGdkError;

// Return widget scale factor of the monitor where the window is located by the
// most part. We intentionally honor the text scale factor here in order to
// have consistent scaling with other UI elements.
static inline CSSToLayoutDeviceScale GetWidgetScaleFactor(nsIFrame* aFrame) {
  return aFrame->PresContext()->CSSToDevPixelScale();
}

nsNativeThemeGTK::nsNativeThemeGTK() : Theme(ScrollbarStyle()) {
  if (moz_gtk_init() != MOZ_GTK_SUCCESS) {
    memset(mDisabledWidgetTypes, 0xff, sizeof(mDisabledWidgetTypes));
    return;
  }

  ThemeChanged();
}

nsNativeThemeGTK::~nsNativeThemeGTK() { moz_gtk_shutdown(); }

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
  MOZ_ASSERT(type < static_cast<size_t>(StyleAppearance::Count));
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

bool nsNativeThemeGTK::GetGtkWidgetAndState(StyleAppearance aAppearance,
                                            nsIFrame* aFrame,
                                            WidgetNodeType& aGtkWidgetType,
                                            GtkWidgetState* aState,
                                            gint* aWidgetFlags) {
  if (aWidgetFlags) {
    *aWidgetFlags = 0;
  }

  ElementState elementState = GetContentState(aFrame, aAppearance);
  if (aState) {
    memset(aState, 0, sizeof(GtkWidgetState));

    // For XUL checkboxes and radio buttons, the state of the parent
    // determines our state.
    if (aWidgetFlags) {
      if (elementState.HasState(ElementState::CHECKED)) {
        *aWidgetFlags |= MOZ_GTK_WIDGET_CHECKED;
      }
      if (elementState.HasState(ElementState::INDETERMINATE)) {
        *aWidgetFlags |= MOZ_GTK_WIDGET_INCONSISTENT;
      }
    }

    aState->disabled =
        elementState.HasState(ElementState::DISABLED) || IsReadOnly(aFrame);
    aState->active = elementState.HasState(ElementState::ACTIVE);
    aState->focused = elementState.HasState(ElementState::FOCUS);
    aState->inHover = elementState.HasState(ElementState::HOVER);
    aState->isDefault = IsDefaultButton(aFrame);
    aState->canDefault = FALSE;  // XXX fix me

    if (aAppearance == StyleAppearance::Button ||
        aAppearance == StyleAppearance::Toolbarbutton ||
        aAppearance == StyleAppearance::Dualbutton ||
        aAppearance == StyleAppearance::ToolbarbuttonDropdown ||
        aAppearance == StyleAppearance::Menulist ||
        aAppearance == StyleAppearance::MenulistButton) {
      aState->active &= aState->inHover;
    } else if (aAppearance == StyleAppearance::Treetwisty ||
               aAppearance == StyleAppearance::Treetwistyopen) {
      if (nsTreeBodyFrame* treeBodyFrame = do_QueryFrame(aFrame)) {
        const mozilla::AtomArray& atoms =
            treeBodyFrame->GetPropertyArrayForCurrentDrawingItem();
        aState->selected = atoms.Contains(nsGkAtoms::selected);
        aState->inHover = atoms.Contains(nsGkAtoms::hover);
      }
    }

    if (IsFrameContentNodeInNamespace(aFrame, kNameSpaceID_XUL)) {
      // For these widget types, some element (either a child or parent)
      // actually has element focus, so we check the focused attribute
      // to see whether to draw in the focused state.
      aState->focused = elementState.HasState(ElementState::FOCUSRING);
      if (aAppearance == StyleAppearance::Radio ||
          aAppearance == StyleAppearance::Checkbox) {
        // In XUL, checkboxes and radios shouldn't have focus rings, their
        // labels do
        aState->focused = FALSE;
      }

      // A button with drop down menu open or an activated toggle button
      // should always appear depressed.
      if (aAppearance == StyleAppearance::Button ||
          aAppearance == StyleAppearance::Toolbarbutton ||
          aAppearance == StyleAppearance::Dualbutton ||
          aAppearance == StyleAppearance::ToolbarbuttonDropdown ||
          aAppearance == StyleAppearance::Menulist ||
          aAppearance == StyleAppearance::MenulistButton) {
        bool menuOpen = IsOpenButton(aFrame);
        aState->depressed = IsCheckedButton(aFrame) || menuOpen;
        // we must not highlight buttons with open drop down menus on hover.
        aState->inHover = aState->inHover && !menuOpen;
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
    case StyleAppearance::Checkbox:
      aGtkWidgetType = MOZ_GTK_CHECKBUTTON;
      break;
    case StyleAppearance::Radio:
      aGtkWidgetType = MOZ_GTK_RADIOBUTTON;
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
    case StyleAppearance::Separator:
      aGtkWidgetType = MOZ_GTK_TOOLBAR_SEPARATOR;
      break;
    case StyleAppearance::Toolbargripper:
      aGtkWidgetType = MOZ_GTK_GRIPPER;
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
    case StyleAppearance::Toolbar:
      aGtkWidgetType = MOZ_GTK_TOOLBAR;
      break;
    case StyleAppearance::Tooltip:
      aGtkWidgetType = MOZ_GTK_TOOLTIP;
      break;
    case StyleAppearance::ProgressBar:
      aGtkWidgetType = MOZ_GTK_PROGRESSBAR;
      break;
    case StyleAppearance::Progresschunk: {
      nsIFrame* stateFrame = aFrame->GetParent();
      ElementState elementState = GetContentState(stateFrame, aAppearance);

      aGtkWidgetType = elementState.HasState(ElementState::INDETERMINATE)
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
    case StyleAppearance::MozWindowTitlebar:
      aGtkWidgetType = MOZ_GTK_HEADER_BAR;
      break;
    case StyleAppearance::MozWindowDecorations:
      aGtkWidgetType = MOZ_GTK_WINDOW_DECORATION;
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
                               GtkTextDirection aDirection, double aScaleFactor,
                               bool aSnapped, const Point& aDrawOrigin,
                               const nsIntSize& aDrawSize,
                               GdkRectangle& aGDKRect,
                               nsITheme::Transparency aTransparency) {
  static auto sCairoSurfaceSetDeviceScalePtr =
      (void (*)(cairo_surface_t*, double, double))dlsym(
          RTLD_DEFAULT, "cairo_surface_set_device_scale");
  const bool useHiDPIWidgets =
      aScaleFactor != 1.0 && sCairoSurfaceSetDeviceScalePtr;

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

  Size clipSize((aDrawSize.width + aScaleFactor - 1) / aScaleFactor,
                (aDrawSize.height + aScaleFactor - 1) / aScaleFactor);

  // A direct Cairo draw target is not available, so we need to create a
  // temporary one.
#if defined(MOZ_X11) && defined(CAIRO_HAS_XLIB_SURFACE)
  if (GdkIsX11Display()) {
    // If using a Cairo xlib surface, then try to reuse it.
    BorrowedXlibDrawable borrow(aDrawTarget);
    if (Drawable drawable = borrow.GetDrawable()) {
      nsIntSize size = borrow.GetSize();
      cairo_surface_t* surf = cairo_xlib_surface_create(
          borrow.GetDisplay(), drawable, borrow.GetVisual(), size.width,
          size.height);
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

CSSIntMargin nsNativeThemeGTK::GetExtraSizeForWidget(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  CSSIntMargin extra;
  // Allow an extra one pixel above and below the thumb for certain
  // GTK2 themes (Ximian Industrial, Bluecurve, Misty, at least);
  // We modify the frame's overflow area.  See bug 297508.
  switch (aAppearance) {
    case StyleAppearance::Button: {
      if (IsDefaultButton(aFrame)) {
        // Some themes draw a default indicator outside the widget,
        // include that in overflow
        moz_gtk_button_get_default_overflow(&extra.top.value, &extra.left.value,
                                            &extra.bottom.value,
                                            &extra.right.value);
        break;
      }
      return {};
    }
    default:
      return {};
  }
  return extra;
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
                                       const nsRect& aDirtyRect,
                                       DrawOverflow aDrawOverflow) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::DrawWidgetBackground(aContext, aFrame, aAppearance, aRect,
                                       aDirtyRect, aDrawOverflow);
  }

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

  // Align to device pixels where sensible
  // to provide crisper and faster drawing.
  // Don't snap if it's a non-unit scale factor. We're going to have to take
  // slow paths then in any case.
  // We prioritize the size when snapping in order to avoid distorting widgets
  // that should be square, which can occur if edges are snapped independently.
  bool snapped = ctx->UserToDevicePixelSnapped(
      rect, gfxContext::SnapOption::PrioritizeSize);
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
  LayoutDeviceIntRect widgetRect(0, 0, NS_lround(rect.Width()),
                                 NS_lround(rect.Height()));

  // This is the rectangle that will actually be drawn, in gdk pixels
  LayoutDeviceIntRect drawingRect(
      int32_t(dirtyRect.X()), int32_t(dirtyRect.Y()),
      int32_t(dirtyRect.Width()), int32_t(dirtyRect.Height()));
  if (widgetRect.IsEmpty() ||
      !drawingRect.IntersectRect(widgetRect, drawingRect)) {
    return NS_OK;
  }

  NS_ASSERTION(!IsWidgetTypeDisabled(mDisabledWidgetTypes, aAppearance),
               "Trying to render an unsafe widget!");

  bool safeState = IsWidgetStateSafe(mSafeWidgetStates, aAppearance, &state);
  if (!safeState) {
    gLastGdkError = 0;
    gdk_error_trap_push();
  }

  Transparency transparency = GetWidgetTransparency(aFrame, aAppearance);

  // gdk rectangles are wrt the drawing rect.
  auto scaleFactor = GetWidgetScaleFactor(aFrame);
  LayoutDeviceIntRect gdkDevRect(-drawingRect.TopLeft(), widgetRect.Size());

  auto gdkCssRect = CSSIntRect::RoundIn(gdkDevRect / scaleFactor);
  GdkRectangle gdk_rect = {gdkCssRect.x, gdkCssRect.y, gdkCssRect.width,
                           gdkCssRect.height};

  // Save actual widget scale to GtkWidgetState as we don't provide
  // the frame to gtk3drawing routines.
  state.image_scale = std::ceil(scaleFactor.scale);

  // translate everything so (0,0) is the top left of the drawingRect
  gfxPoint origin = rect.TopLeft() + drawingRect.TopLeft().ToUnknownPoint();

  DrawThemeWithCairo(ctx, aContext->GetDrawTarget(), state, gtkWidgetType,
                     flags, direction, scaleFactor.scale, snapped,
                     ToPoint(origin), drawingRect.Size().ToUnknownSize(),
                     gdk_rect, transparency);

  if (!safeState) {
    // gdk_flush() call from expose event crashes Gtk+ on Wayland
    // (Gnome BZ #773307)
    if (GdkIsX11Display()) {
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
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::CreateWebRenderCommandsForWidget(
        aBuilder, aResources, aSc, aManager, aFrame, aAppearance, aRect);
  }
  return false;
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
                                        CSSIntMargin* aResult) {
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

CSSIntMargin nsNativeThemeGTK::GetCachedWidgetBorder(
    nsIFrame* aFrame, StyleAppearance aAppearance,
    GtkTextDirection aDirection) {
  CSSIntMargin result;

  WidgetNodeType gtkWidgetType;
  gint unusedFlags;
  if (GetGtkWidgetAndState(aAppearance, aFrame, gtkWidgetType, nullptr,
                           &unusedFlags)) {
    MOZ_ASSERT(0 <= gtkWidgetType && gtkWidgetType < MOZ_GTK_WIDGET_NODE_COUNT);
    uint8_t cacheIndex = gtkWidgetType / 8;
    uint8_t cacheBit = 1u << (gtkWidgetType % 8);

    if (mBorderCacheValid[cacheIndex] & cacheBit) {
      result = mBorderCache[gtkWidgetType];
    } else {
      moz_gtk_get_widget_border(gtkWidgetType, &result.left.value,
                                &result.top.value, &result.right.value,
                                &result.bottom.value, aDirection);
      if (gtkWidgetType != MOZ_GTK_DROPDOWN) {  // depends on aDirection
        mBorderCacheValid[cacheIndex] |= cacheBit;
        mBorderCache[gtkWidgetType] = result;
      }
    }
  }
  FixupForVerticalWritingMode(aFrame->GetWritingMode(), &result);
  return result;
}

LayoutDeviceIntMargin nsNativeThemeGTK::GetWidgetBorder(
    nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetBorder(aContext, aFrame, aAppearance);
  }

  CSSIntMargin result;
  GtkTextDirection direction = GetTextDirection(aFrame);
  switch (aAppearance) {
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
        return {};
      }
      moz_gtk_get_tab_border(&result.left.value, &result.top.value,
                             &result.right.value, &result.bottom.value,
                             direction, (GtkTabFlags)flags, gtkWidgetType);
    } break;
    default: {
      result = GetCachedWidgetBorder(aFrame, aAppearance, direction);
    }
  }

  return (CSSMargin(result) * GetWidgetScaleFactor(aFrame)).Rounded();
}

bool nsNativeThemeGTK::GetWidgetPadding(nsDeviceContext* aContext,
                                        nsIFrame* aFrame,
                                        StyleAppearance aAppearance,
                                        LayoutDeviceIntMargin* aResult) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetWidgetPadding(aContext, aFrame, aAppearance, aResult);
  }
  switch (aAppearance) {
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Tooltip:
    case StyleAppearance::MozWindowButtonBox:
    case StyleAppearance::MozWindowButtonClose:
    case StyleAppearance::MozWindowButtonMinimize:
    case StyleAppearance::MozWindowButtonMaximize:
    case StyleAppearance::MozWindowButtonRestore:
    case StyleAppearance::Dualbutton:
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward:
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
    default:
      break;
  }

  return false;
}

bool nsNativeThemeGTK::GetWidgetOverflow(nsDeviceContext* aContext,
                                         nsIFrame* aFrame,
                                         StyleAppearance aAppearance,
                                         nsRect* aOverflowRect) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::GetWidgetOverflow(aContext, aFrame, aAppearance,
                                    aOverflowRect);
  }
  auto overflow = GetExtraSizeForWidget(aFrame, aAppearance);
  if (overflow == CSSIntMargin()) {
    return false;
  }
  aOverflowRect->Inflate(CSSIntMargin::ToAppUnits(overflow));
  return true;
}

auto nsNativeThemeGTK::IsWidgetNonNative(nsIFrame* aFrame,
                                         StyleAppearance aAppearance)
    -> NonNative {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return NonNative::Always;
  }

  // If the current GTK theme color scheme matches our color-scheme, then we
  // can draw a native widget.
  if (LookAndFeel::ColorSchemeForFrame(aFrame) ==
      PreferenceSheet::ColorSchemeForChrome()) {
    return NonNative::No;
  }

  // As an special-case, for tooltips, we check if the tooltip color is the
  // same between the light and dark themes. If so we can get away with drawing
  // the native widget, see bug 1817396.
  if (aAppearance == StyleAppearance::Tooltip) {
    auto darkColor =
        LookAndFeel::Color(StyleSystemColor::Infotext, ColorScheme::Dark,
                           LookAndFeel::UseStandins::No);
    auto lightColor =
        LookAndFeel::Color(StyleSystemColor::Infotext, ColorScheme::Light,
                           LookAndFeel::UseStandins::No);
    if (darkColor == lightColor) {
      return NonNative::No;
    }
  }

  // If the non-native theme doesn't support the widget then oh well...
  if (!Theme::ThemeSupportsWidget(aFrame->PresContext(), aFrame, aAppearance)) {
    return NonNative::No;
  }

  return NonNative::BecauseColorMismatch;
}

bool nsNativeThemeGTK::IsWidgetAlwaysNonNative(nsIFrame* aFrame,
                                               StyleAppearance aAppearance) {
  return Theme::IsWidgetAlwaysNonNative(aFrame, aAppearance) ||
         aAppearance == StyleAppearance::MozMenulistArrowButton;
}

LayoutDeviceIntSize nsNativeThemeGTK::GetMinimumWidgetSize(
    nsPresContext* aPresContext, nsIFrame* aFrame,
    StyleAppearance aAppearance) {
  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::GetMinimumWidgetSize(aPresContext, aFrame, aAppearance);
  }

  CSSIntSize result;
  switch (aAppearance) {
    case StyleAppearance::Splitter: {
      if (IsHorizontal(aFrame)) {
        moz_gtk_splitter_get_metrics(GTK_ORIENTATION_HORIZONTAL, &result.width);
      } else {
        moz_gtk_splitter_get_metrics(GTK_ORIENTATION_VERTICAL, &result.height);
      }
    } break;
    case StyleAppearance::RangeThumb: {
      if (IsRangeHorizontal(aFrame)) {
        moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_HORIZONTAL,
                                       &result.width, &result.height);
      } else {
        moz_gtk_get_scalethumb_metrics(GTK_ORIENTATION_VERTICAL, &result.width,
                                       &result.width);
      }
    } break;
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward: {
      moz_gtk_get_tab_scroll_arrow_size(&result.width, &result.height);
    } break;
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
      const ToggleGTKMetrics* metrics = GetToggleMetrics(
          aAppearance == StyleAppearance::Radio ? MOZ_GTK_RADIOBUTTON
                                                : MOZ_GTK_CHECKBUTTON);
      result.width = metrics->minSizeWithBorder.width;
      result.height = metrics->minSizeWithBorder.height;
    } break;
    case StyleAppearance::ToolbarbuttonDropdown:
    case StyleAppearance::ButtonArrowUp:
    case StyleAppearance::ButtonArrowDown:
    case StyleAppearance::ButtonArrowNext:
    case StyleAppearance::ButtonArrowPrevious: {
      moz_gtk_get_arrow_size(MOZ_GTK_TOOLBARBUTTON_ARROW, &result.width,
                             &result.height);
    } break;
    case StyleAppearance::MozWindowButtonClose: {
      const ToolbarButtonGTKMetrics* metrics =
          GetToolbarButtonMetrics(MOZ_GTK_HEADER_BAR_BUTTON_CLOSE);
      result.width = metrics->minSizeWithBorderMargin.width;
      result.height = metrics->minSizeWithBorderMargin.height;
      break;
    }
    case StyleAppearance::MozWindowButtonMinimize: {
      const ToolbarButtonGTKMetrics* metrics =
          GetToolbarButtonMetrics(MOZ_GTK_HEADER_BAR_BUTTON_MINIMIZE);
      result.width = metrics->minSizeWithBorderMargin.width;
      result.height = metrics->minSizeWithBorderMargin.height;
      break;
    }
    case StyleAppearance::MozWindowButtonMaximize:
    case StyleAppearance::MozWindowButtonRestore: {
      const ToolbarButtonGTKMetrics* metrics =
          GetToolbarButtonMetrics(MOZ_GTK_HEADER_BAR_BUTTON_MAXIMIZE);
      result.width = metrics->minSizeWithBorderMargin.width;
      result.height = metrics->minSizeWithBorderMargin.height;
      break;
    }
    case StyleAppearance::Button:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Toolbarbutton:
    case StyleAppearance::Treeheadercell: {
      if (aAppearance == StyleAppearance::Menulist ||
          aAppearance == StyleAppearance::MenulistButton) {
        // Include the arrow size.
        moz_gtk_get_arrow_size(MOZ_GTK_DROPDOWN, &result.width, &result.height);
      }
      // else the minimum size is missing consideration of container
      // descendants; the value returned here will not be helpful, but the
      // box model may consider border and padding with child minimum sizes.

      CSSIntMargin border =
          GetCachedWidgetBorder(aFrame, aAppearance, GetTextDirection(aFrame));
      result.width += border.LeftRight();
      result.height += border.TopBottom();
    } break;
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield: {
      gint contentHeight = 0;
      gint borderPaddingHeight = 0;
      moz_gtk_get_entry_min_height(&contentHeight, &borderPaddingHeight);

      // Scale the min content height proportionately with the font-size if it's
      // smaller than the default one. This prevents <input type=text
      // style="font-size: .5em"> from keeping a ridiculously large size, for
      // example.
      const gfxFloat fieldFontSizeInCSSPixels = [] {
        gfxFontStyle fieldFontStyle;
        nsAutoString unusedFontName;
        DebugOnly<bool> result = LookAndFeel::GetFont(
            LookAndFeel::FontID::MozField, unusedFontName, fieldFontStyle);
        MOZ_ASSERT(result, "GTK look and feel supports the field font");
        // NOTE: GetFont returns font sizes in CSS pixels, and we want just
        // that.
        return fieldFontStyle.size;
      }();

      const gfxFloat fontSize = aFrame->StyleFont()->mFont.size.ToCSSPixels();
      if (fieldFontSizeInCSSPixels > fontSize) {
        contentHeight =
            std::round(contentHeight * fontSize / fieldFontSizeInCSSPixels);
      }

      gint height = contentHeight + borderPaddingHeight;
      if (aFrame->GetWritingMode().IsVertical()) {
        result.width = height;
      } else {
        result.height = height;
      }
    } break;
    case StyleAppearance::Separator: {
      moz_gtk_get_toolbar_separator_width(&result.width);
    } break;
    case StyleAppearance::Spinner:
      // hard code these sizes
      result.width = 14;
      result.height = 26;
      break;
    case StyleAppearance::Treeheadersortarrow:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
      // hard code these sizes
      result.width = 14;
      result.height = 13;
      break;
    case StyleAppearance::Treetwisty:
    case StyleAppearance::Treetwistyopen: {
      gint expander_size;
      moz_gtk_get_treeview_expander_size(&expander_size);
      result.width = result.height = expander_size;
    } break;
    default:
      break;
  }

  return LayoutDeviceIntSize::Round(CSSSize(result) *
                                    GetWidgetScaleFactor(aFrame));
}

NS_IMETHODIMP
nsNativeThemeGTK::WidgetStateChanged(nsIFrame* aFrame,
                                     StyleAppearance aAppearance,
                                     nsAtom* aAttribute, bool* aShouldRepaint,
                                     const nsAttrValue* aOldValue) {
  *aShouldRepaint = false;

  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::WidgetStateChanged(aFrame, aAppearance, aAttribute,
                                     aShouldRepaint, aOldValue);
  }

  // Some widget types just never change state.
  if (aAppearance == StyleAppearance::Toolbox ||
      aAppearance == StyleAppearance::Toolbar ||
      aAppearance == StyleAppearance::Progresschunk ||
      aAppearance == StyleAppearance::ProgressBar ||
      aAppearance == StyleAppearance::Tooltip ||
      aAppearance == StyleAppearance::MozWindowDecorations) {
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

  // XXXdwh Not sure what can really be done here.  Can at least guess for
  // specific widgets that they're highly unlikely to have certain states.
  // For example, a toolbar doesn't care about any states.
  if (!aAttribute) {
    // Hover/focus/active changed.  Always repaint.
    *aShouldRepaint = true;
    return NS_OK;
  }

  // Check the attribute to see if it's relevant.
  // disabled, checked, dlgtype, default, etc.
  *aShouldRepaint = false;
  if (aAttribute == nsGkAtoms::disabled || aAttribute == nsGkAtoms::checked ||
      aAttribute == nsGkAtoms::selected ||
      aAttribute == nsGkAtoms::visuallyselected ||
      aAttribute == nsGkAtoms::focused || aAttribute == nsGkAtoms::readonly ||
      aAttribute == nsGkAtoms::_default ||
      aAttribute == nsGkAtoms::menuactive || aAttribute == nsGkAtoms::open) {
    *aShouldRepaint = true;
    return NS_OK;
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
  if (IsWidgetTypeDisabled(mDisabledWidgetTypes, aAppearance)) {
    return false;
  }

  if (IsWidgetAlwaysNonNative(aFrame, aAppearance)) {
    return Theme::ThemeSupportsWidget(aPresContext, aFrame, aAppearance);
  }

  switch (aAppearance) {
    // Combobox dropdowns don't support native theming in vertical mode.
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
      if (aFrame && aFrame->GetWritingMode().IsVertical()) {
        return false;
      }
      [[fallthrough]];

    case StyleAppearance::Button:
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
    case StyleAppearance::Listbox:
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
    case StyleAppearance::Tab:
    // case StyleAppearance::Tabpanel:
    case StyleAppearance::Tabpanels:
    case StyleAppearance::TabScrollArrowBack:
    case StyleAppearance::TabScrollArrowForward:
    case StyleAppearance::Tooltip:
    case StyleAppearance::Spinner:
    case StyleAppearance::SpinnerUpbutton:
    case StyleAppearance::SpinnerDownbutton:
    case StyleAppearance::SpinnerTextfield:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Textarea:
    case StyleAppearance::Range:
    case StyleAppearance::RangeThumb:
    case StyleAppearance::Splitter:
    case StyleAppearance::MozWindowButtonBox:
    case StyleAppearance::MozWindowButtonClose:
    case StyleAppearance::MozWindowButtonMinimize:
    case StyleAppearance::MozWindowButtonMaximize:
    case StyleAppearance::MozWindowButtonRestore:
    case StyleAppearance::MozWindowTitlebar:
    case StyleAppearance::MozWindowTitlebarMaximized:
    case StyleAppearance::MozWindowDecorations:
      return !IsWidgetStyled(aPresContext, aFrame, aAppearance);
    default:
      break;
  }

  return false;
}

NS_IMETHODIMP_(bool)
nsNativeThemeGTK::WidgetIsContainer(StyleAppearance aAppearance) {
  // XXXdwh At some point flesh all of this out.
  if (aAppearance == StyleAppearance::Radio ||
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

bool nsNativeThemeGTK::ThemeDrawsFocusForWidget(nsIFrame* aFrame,
                                                StyleAppearance aAppearance) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::ThemeDrawsFocusForWidget(aFrame, aAppearance);
  }
  switch (aAppearance) {
    case StyleAppearance::Button:
    case StyleAppearance::Menulist:
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Textarea:
    case StyleAppearance::Textfield:
    case StyleAppearance::Treeheadercell:
    case StyleAppearance::NumberInput:
      return true;
    default:
      return false;
  }
}

bool nsNativeThemeGTK::ThemeNeedsComboboxDropmarker() { return false; }

nsITheme::Transparency nsNativeThemeGTK::GetWidgetTransparency(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (IsWidgetNonNative(aFrame, aAppearance) != NonNative::No) {
    return Theme::GetWidgetTransparency(aFrame, aAppearance);
  }

  switch (aAppearance) {
    // Tooltips use gtk_paint_flat_box() on Gtk2
    // but are shaped on Gtk3
    case StyleAppearance::Tooltip:
      return eTransparent;
    default:
      return eUnknownTransparency;
  }
}

already_AddRefed<Theme> do_CreateNativeThemeDoNotUseDirectly() {
  if (gfxPlatform::IsHeadless()) {
    return do_AddRef(new Theme(Theme::ScrollbarStyle()));
  }
  return do_AddRef(new nsNativeThemeGTK());
}
