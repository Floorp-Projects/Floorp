/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNativeTheme.h"
#include "nsIWidget.h"
#include "mozilla/dom/Document.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "nsNumberControlFrame.h"
#include "nsPresContext.h"
#include "nsString.h"
#include "nsNameSpaceManager.h"
#include "nsStyleConsts.h"
#include "nsPIDOMWindow.h"
#include "nsProgressFrame.h"
#include "nsMeterFrame.h"
#include "nsRangeFrame.h"
#include "nsCSSRendering.h"
#include "ImageContainer.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLProgressElement.h"
#include "mozilla/PresShell.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/dom/DocumentInlines.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

nsNativeTheme::nsNativeTheme() : mAnimatedContentTimeout(UINT32_MAX) {}

NS_IMPL_ISUPPORTS(nsNativeTheme, nsITimerCallback, nsINamed)

/* static */ ElementState nsNativeTheme::GetContentState(
    nsIFrame* aFrame, StyleAppearance aAppearance) {
  if (!aFrame) {
    return ElementState();
  }

  nsIContent* frameContent = aFrame->GetContent();
  if (!frameContent || !frameContent->IsElement()) {
    return ElementState();
  }

  const bool isXULElement = frameContent->IsXULElement();
  if (isXULElement) {
    if (aAppearance == StyleAppearance::Checkbox ||
        aAppearance == StyleAppearance::Radio ||
        aAppearance == StyleAppearance::ToolbarbuttonDropdown ||
        aAppearance == StyleAppearance::Treeheadersortarrow ||
        aAppearance == StyleAppearance::ButtonArrowPrevious ||
        aAppearance == StyleAppearance::ButtonArrowNext ||
        aAppearance == StyleAppearance::ButtonArrowUp ||
        aAppearance == StyleAppearance::ButtonArrowDown) {
      aFrame = aFrame->GetParent();
      frameContent = aFrame->GetContent();
    }
    MOZ_ASSERT(frameContent && frameContent->IsElement());
  }

  ElementState flags = frameContent->AsElement()->StyleState();
  nsNumberControlFrame* numberControlFrame =
      nsNumberControlFrame::GetNumberControlFrameForSpinButton(aFrame);
  if (numberControlFrame &&
      numberControlFrame->GetContent()->AsElement()->StyleState().HasState(
          ElementState::DISABLED)) {
    flags |= ElementState::DISABLED;
  }

  if (!isXULElement) {
    return flags;
  }

  if (CheckBooleanAttr(aFrame, nsGkAtoms::disabled)) {
    flags |= ElementState::DISABLED;
  }

  switch (aAppearance) {
    case StyleAppearance::Radio: {
      if (CheckBooleanAttr(aFrame, nsGkAtoms::focused)) {
        flags |= ElementState::FOCUS;
        nsPIDOMWindowOuter* window =
            aFrame->GetContent()->OwnerDoc()->GetWindow();
        if (window && window->ShouldShowFocusRing()) {
          flags |= ElementState::FOCUSRING;
        }
      }
      if (CheckBooleanAttr(aFrame, nsGkAtoms::selected) ||
          CheckBooleanAttr(aFrame, nsGkAtoms::checked)) {
        flags |= ElementState::CHECKED;
      }
      break;
    }
    case StyleAppearance::Checkbox: {
      if (CheckBooleanAttr(aFrame, nsGkAtoms::checked)) {
        flags |= ElementState::CHECKED;
      } else if (CheckBooleanAttr(aFrame, nsGkAtoms::indeterminate)) {
        flags |= ElementState::INDETERMINATE;
      }
      break;
    }
    case StyleAppearance::MenulistButton:
    case StyleAppearance::Menulist:
    case StyleAppearance::NumberInput:
    case StyleAppearance::Textfield:
    case StyleAppearance::Searchfield:
    case StyleAppearance::Textarea: {
      if (CheckBooleanAttr(aFrame, nsGkAtoms::focused)) {
        flags |= ElementState::FOCUS | ElementState::FOCUSRING;
      }
      break;
    }
    default:
      break;
  }

  return flags;
}

/* static */
bool nsNativeTheme::CheckBooleanAttr(nsIFrame* aFrame, nsAtom* aAtom) {
  if (!aFrame) return false;

  nsIContent* content = aFrame->GetContent();
  if (!content || !content->IsElement()) return false;

  if (content->IsHTMLElement()) return content->AsElement()->HasAttr(aAtom);

  // For XML/XUL elements, an attribute must be equal to the literal
  // string "true" to be counted as true.  An empty string should _not_
  // be counted as true.
  return content->AsElement()->AttrValueIs(kNameSpaceID_None, aAtom, u"true"_ns,
                                           eCaseMatters);
}

/* static */
int32_t nsNativeTheme::CheckIntAttr(nsIFrame* aFrame, nsAtom* aAtom,
                                    int32_t defaultValue) {
  if (!aFrame) return defaultValue;

  nsIContent* content = aFrame->GetContent();
  if (!content || !content->IsElement()) return defaultValue;

  nsAutoString attr;
  content->AsElement()->GetAttr(aAtom, attr);
  nsresult err;
  int32_t value = attr.ToInteger(&err);
  if (attr.IsEmpty() || NS_FAILED(err)) return defaultValue;

  return value;
}

/* static */
double nsNativeTheme::GetProgressValue(nsIFrame* aFrame) {
  if (!aFrame || !aFrame->GetContent()->IsHTMLElement(nsGkAtoms::progress)) {
    return 0;
  }

  return static_cast<HTMLProgressElement*>(aFrame->GetContent())->Value();
}

/* static */
double nsNativeTheme::GetProgressMaxValue(nsIFrame* aFrame) {
  if (!aFrame || !aFrame->GetContent()->IsHTMLElement(nsGkAtoms::progress)) {
    return 100;
  }

  return static_cast<HTMLProgressElement*>(aFrame->GetContent())->Max();
}

bool nsNativeTheme::IsButtonTypeMenu(nsIFrame* aFrame) {
  if (!aFrame) return false;

  nsIContent* content = aFrame->GetContent();
  return content->IsXULElement(nsGkAtoms::button) &&
         content->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                           u"menu"_ns, eCaseMatters);
}

bool nsNativeTheme::IsPressedButton(nsIFrame* aFrame) {
  ElementState state = GetContentState(aFrame, StyleAppearance::Toolbarbutton);
  if (state.HasState(ElementState::DISABLED)) {
    return false;
  }

  return IsOpenButton(aFrame) ||
         state.HasAllStates(ElementState::ACTIVE | ElementState::HOVER);
}

bool nsNativeTheme::IsWidgetStyled(nsPresContext* aPresContext,
                                   nsIFrame* aFrame,
                                   StyleAppearance aAppearance) {
  // Check for specific widgets to see if HTML has overridden the style.
  if (!aFrame) {
    return false;
  }

  /**
   * Progress bar appearance should be the same for the bar and the container
   * frame. nsProgressFrame owns the logic and will tell us what we should do.
   */
  if (aAppearance == StyleAppearance::Progresschunk ||
      aAppearance == StyleAppearance::ProgressBar) {
    nsProgressFrame* progressFrame = do_QueryFrame(
        aAppearance == StyleAppearance::Progresschunk ? aFrame->GetParent()
                                                      : aFrame);
    if (progressFrame) {
      return !progressFrame->ShouldUseNativeStyle();
    }
  }

  /**
   * Meter bar appearance should be the same for the bar and the container
   * frame. nsMeterFrame owns the logic and will tell us what we should do.
   */
  if (aAppearance == StyleAppearance::Meterchunk ||
      aAppearance == StyleAppearance::Meter) {
    nsMeterFrame* meterFrame = do_QueryFrame(
        aAppearance == StyleAppearance::Meterchunk ? aFrame->GetParent()
                                                   : aFrame);
    if (meterFrame) {
      return !meterFrame->ShouldUseNativeStyle();
    }
  }

  /**
   * An nsRangeFrame and its children are treated atomically when it
   * comes to native theming (either all parts, or no parts, are themed).
   * nsRangeFrame owns the logic and will tell us what we should do.
   */
  if (aAppearance == StyleAppearance::Range ||
      aAppearance == StyleAppearance::RangeThumb) {
    nsRangeFrame* rangeFrame = do_QueryFrame(
        aAppearance == StyleAppearance::RangeThumb ? aFrame->GetParent()
                                                   : aFrame);
    if (rangeFrame) {
      return !rangeFrame->ShouldUseNativeStyle();
    }
  }

  return nsLayoutUtils::AuthorSpecifiedBorderBackgroundDisablesTheming(
             aAppearance) &&
         aFrame->GetContent()->IsHTMLElement() &&
         aFrame->Style()->HasAuthorSpecifiedBorderOrBackground();
}

/* static */
bool nsNativeTheme::IsFrameRTL(nsIFrame* aFrame) {
  if (!aFrame) {
    return false;
  }
  return aFrame->GetWritingMode().IsPhysicalRTL();
}

/* static */
bool nsNativeTheme::IsHTMLContent(nsIFrame* aFrame) {
  if (!aFrame) {
    return false;
  }
  nsIContent* content = aFrame->GetContent();
  return content && content->IsHTMLElement();
}

// treeheadercell:
nsNativeTheme::TreeSortDirection nsNativeTheme::GetTreeSortDirection(
    nsIFrame* aFrame) {
  if (!aFrame || !aFrame->GetContent()) return eTreeSortDirection_Natural;

  static Element::AttrValuesArray strings[] = {nsGkAtoms::descending,
                                               nsGkAtoms::ascending, nullptr};

  nsIContent* content = aFrame->GetContent();
  if (content->IsElement()) {
    switch (content->AsElement()->FindAttrValueIn(
        kNameSpaceID_None, nsGkAtoms::sortDirection, strings, eCaseMatters)) {
      case 0:
        return eTreeSortDirection_Descending;
      case 1:
        return eTreeSortDirection_Ascending;
    }
  }

  return eTreeSortDirection_Natural;
}

bool nsNativeTheme::IsLastTreeHeaderCell(nsIFrame* aFrame) {
  if (!aFrame) {
    return false;
  }

  // A tree column picker button is always the last header cell.
  if (aFrame->GetContent()->IsXULElement(nsGkAtoms::button)) {
    return true;
  }

  // Find the parent tree.
  nsIContent* parent = aFrame->GetContent()->GetParent();
  while (parent && !parent->IsXULElement(nsGkAtoms::tree)) {
    parent = parent->GetParent();
  }

  // If the column picker is visible, this can't be the last column.
  if (parent && !parent->AsElement()->AttrValueIs(kNameSpaceID_None,
                                                  nsGkAtoms::hidecolumnpicker,
                                                  u"true"_ns, eCaseMatters))
    return false;

  while ((aFrame = aFrame->GetNextSibling())) {
    if (aFrame->GetRect().Width() > 0) return false;
  }
  return true;
}

// tab:
bool nsNativeTheme::IsBottomTab(nsIFrame* aFrame) {
  if (!aFrame) return false;

  nsAutoString classStr;
  if (aFrame->GetContent()->IsElement()) {
    aFrame->GetContent()->AsElement()->GetAttr(nsGkAtoms::_class, classStr);
  }
  // FIXME: This looks bogus, shouldn't this be looking at GetClasses()?
  return !classStr.IsEmpty() && classStr.Find(u"tab-bottom") != kNotFound;
}

bool nsNativeTheme::IsFirstTab(nsIFrame* aFrame) {
  if (!aFrame) return false;

  for (nsIFrame* first : aFrame->GetParent()->PrincipalChildList()) {
    if (first->GetRect().Width() > 0 &&
        first->GetContent()->IsXULElement(nsGkAtoms::tab))
      return (first == aFrame);
  }
  return false;
}

bool nsNativeTheme::IsHorizontal(nsIFrame* aFrame) {
  if (!aFrame) return false;

  if (!aFrame->GetContent()->IsElement()) return true;

  return !aFrame->GetContent()->AsElement()->AttrValueIs(
      kNameSpaceID_None, nsGkAtoms::orient, nsGkAtoms::vertical, eCaseMatters);
}

bool nsNativeTheme::IsNextToSelectedTab(nsIFrame* aFrame, int32_t aOffset) {
  if (!aFrame) return false;

  if (aOffset == 0) return IsSelectedTab(aFrame);

  int32_t thisTabIndex = -1, selectedTabIndex = -1;

  nsIFrame* currentTab = aFrame->GetParent()->PrincipalChildList().FirstChild();
  for (int32_t i = 0; currentTab; currentTab = currentTab->GetNextSibling()) {
    if (currentTab->GetRect().Width() == 0) continue;
    if (aFrame == currentTab) thisTabIndex = i;
    if (IsSelectedTab(currentTab)) selectedTabIndex = i;
    ++i;
  }

  if (thisTabIndex == -1 || selectedTabIndex == -1) return false;

  return (thisTabIndex - selectedTabIndex == aOffset);
}

bool nsNativeTheme::IsVerticalProgress(nsIFrame* aFrame) {
  if (!aFrame) {
    return false;
  }
  return IsVerticalMeter(aFrame);
}

bool nsNativeTheme::IsVerticalMeter(nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame, "You have to pass a non-null aFrame");
  switch (aFrame->StyleDisplay()->mOrient) {
    case StyleOrient::Horizontal:
      return false;
    case StyleOrient::Vertical:
      return true;
    case StyleOrient::Inline:
      return aFrame->GetWritingMode().IsVertical();
    case StyleOrient::Block:
      return !aFrame->GetWritingMode().IsVertical();
  }
  MOZ_ASSERT_UNREACHABLE("unexpected -moz-orient value");
  return false;
}

// menupopup:
bool nsNativeTheme::IsSubmenu(nsIFrame* aFrame, bool* aLeftOfParent) {
  if (!aFrame) return false;

  nsIContent* parentContent = aFrame->GetContent()->GetParent();
  if (!parentContent || !parentContent->IsXULElement(nsGkAtoms::menu))
    return false;

  nsIFrame* parent = aFrame;
  while ((parent = parent->GetParent())) {
    if (parent->GetContent() == parentContent) {
      if (aLeftOfParent) {
        LayoutDeviceIntRect selfBounds, parentBounds;
        selfBounds = aFrame->GetNearestWidget()->GetScreenBounds();
        parentBounds = parent->GetNearestWidget()->GetScreenBounds();
        *aLeftOfParent = selfBounds.X() < parentBounds.X();
      }
      return true;
    }
  }

  return false;
}

bool nsNativeTheme::QueueAnimatedContentForRefresh(nsIContent* aContent,
                                                   uint32_t aMinimumFrameRate) {
  NS_ASSERTION(aContent, "Null pointer!");
  NS_ASSERTION(aMinimumFrameRate, "aMinimumFrameRate must be non-zero!");
  NS_ASSERTION(aMinimumFrameRate <= 1000,
               "aMinimumFrameRate must be less than 1000!");

  uint32_t timeout = 1000 / aMinimumFrameRate;
  timeout = std::min(mAnimatedContentTimeout, timeout);

  if (!mAnimatedContentTimer) {
    mAnimatedContentTimer = NS_NewTimer();
    NS_ENSURE_TRUE(mAnimatedContentTimer, false);
  }

  if (mAnimatedContentList.IsEmpty() || timeout != mAnimatedContentTimeout) {
    nsresult rv;
    if (!mAnimatedContentList.IsEmpty()) {
      rv = mAnimatedContentTimer->Cancel();
      NS_ENSURE_SUCCESS(rv, false);
    }

    if (XRE_IsContentProcess() && NS_IsMainThread()) {
      mAnimatedContentTimer->SetTarget(GetMainThreadSerialEventTarget());
    }
    rv = mAnimatedContentTimer->InitWithCallback(this, timeout,
                                                 nsITimer::TYPE_ONE_SHOT);
    NS_ENSURE_SUCCESS(rv, false);

    mAnimatedContentTimeout = timeout;
  }

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier.
  mAnimatedContentList.AppendElement(aContent);

  return true;
}

NS_IMETHODIMP
nsNativeTheme::Notify(nsITimer* aTimer) {
  NS_ASSERTION(aTimer == mAnimatedContentTimer, "Wrong timer!");

  // XXX Assumes that calling nsIFrame::Invalidate won't reenter
  //     QueueAnimatedContentForRefresh.

  uint32_t count = mAnimatedContentList.Length();
  for (uint32_t index = 0; index < count; index++) {
    nsIFrame* frame = mAnimatedContentList[index]->GetPrimaryFrame();
    if (frame) {
      frame->InvalidateFrame();
    }
  }

  mAnimatedContentList.Clear();
  mAnimatedContentTimeout = UINT32_MAX;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeTheme::GetName(nsACString& aName) {
  aName.AssignLiteral("nsNativeTheme");
  return NS_OK;
}

nsIFrame* nsNativeTheme::GetAdjacentSiblingFrameWithSameAppearance(
    nsIFrame* aFrame, bool aNextSibling) {
  if (!aFrame) return nullptr;

  // Find the next visible sibling.
  nsIFrame* sibling = aFrame;
  do {
    sibling =
        aNextSibling ? sibling->GetNextSibling() : sibling->GetPrevSibling();
  } while (sibling && sibling->GetRect().Width() == 0);

  // Check same appearance and adjacency.
  if (!sibling ||
      sibling->StyleDisplay()->EffectiveAppearance() !=
          aFrame->StyleDisplay()->EffectiveAppearance() ||
      (sibling->GetRect().XMost() != aFrame->GetRect().X() &&
       aFrame->GetRect().XMost() != sibling->GetRect().X()))
    return nullptr;
  return sibling;
}

bool nsNativeTheme::IsRangeHorizontal(nsIFrame* aFrame) {
  nsIFrame* rangeFrame = aFrame;
  if (!rangeFrame->IsRangeFrame()) {
    // If the thumb's frame is passed in, get its range parent:
    rangeFrame = aFrame->GetParent();
  }
  if (rangeFrame->IsRangeFrame()) {
    return static_cast<nsRangeFrame*>(rangeFrame)->IsHorizontal();
  }
  // Not actually a range frame - just use the ratio of the frame's size to
  // decide:
  return aFrame->GetSize().width >= aFrame->GetSize().height;
}

/* static */
bool nsNativeTheme::IsDarkBackgroundForScrollbar(nsIFrame* aFrame) {
  // Try to find the scrolled frame. Note that for stuff like xul <tree> there
  // might be none.
  {
    nsIFrame* frame = aFrame;
    nsIScrollableFrame* scrollFrame = nullptr;
    while (!scrollFrame && frame) {
      scrollFrame = frame->GetScrollTargetFrame();
      frame = frame->GetParent();
    }
    if (scrollFrame) {
      aFrame = scrollFrame->GetScrolledFrame();
    } else {
      // Leave aFrame untouched.
    }
  }

  return IsDarkBackground(aFrame);
}

/* static */
bool nsNativeTheme::IsDarkBackground(nsIFrame* aFrame) {
  auto color =
      nsCSSRendering::FindEffectiveBackgroundColor(
          aFrame, /* aStopAtThemed = */ false, /* aPreferBodyToCanvas = */ true)
          .mColor;
  return LookAndFeel::IsDarkColor(color);
}

/*static*/
bool nsNativeTheme::IsWidgetScrollbarPart(StyleAppearance aAppearance) {
  switch (aAppearance) {
    case StyleAppearance::ScrollbarVertical:
    case StyleAppearance::ScrollbarHorizontal:
    case StyleAppearance::ScrollbarbuttonUp:
    case StyleAppearance::ScrollbarbuttonDown:
    case StyleAppearance::ScrollbarbuttonLeft:
    case StyleAppearance::ScrollbarbuttonRight:
    case StyleAppearance::ScrollbarthumbVertical:
    case StyleAppearance::ScrollbarthumbHorizontal:
    case StyleAppearance::ScrollbartrackHorizontal:
    case StyleAppearance::ScrollbartrackVertical:
    case StyleAppearance::Scrollcorner:
      return true;
    default:
      return false;
  }
}

/*static*/
bool nsNativeTheme::IsWidgetAlwaysNonNative(nsIFrame* aFrame,
                                            StyleAppearance aAppearance) {
  return IsWidgetScrollbarPart(aAppearance) ||
         aAppearance == StyleAppearance::FocusOutline ||
         (aFrame && aFrame->StyleUI()->mMozTheme == StyleMozTheme::NonNative);
}
