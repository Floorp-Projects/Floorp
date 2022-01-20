/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeWin_h
#define nsNativeThemeWin_h

#include <windows.h>

#include "gfxTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsITheme.h"
#include "nsNativeBasicTheme.h"
#include "nsNativeTheme.h"
#include "nsSize.h"
#include "nsStyleConsts.h"
#include "nsUXThemeConstants.h"
#include "nsUXThemeData.h"
#include "ScrollbarDrawingWin.h"

namespace mozilla::widget {

class nsNativeThemeWin : public nsNativeBasicTheme {
 protected:
  using ScrollbarDrawingWin = mozilla::widget::ScrollbarDrawingWin;
  virtual ~nsNativeThemeWin();

 public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  NS_DECL_ISUPPORTS_INHERITED

  // Whether we draw a non-native widget.
  //
  // We draw widgets as non-native when their color-scheme is dark, since win32
  // classic APIs don't allow us to draw dark form controls.
  //
  // We don't call into the non-native theme for sizing information
  // (GetWidgetPadding/Border and GetMinimumWidgetSize), to avoid subtle sizing
  // changes. The non-native theme can basically draw at any size, so we prefer
  // to have consistent sizing information with the native theme.
  bool IsWidgetNonNative(nsIFrame*, StyleAppearance);

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  const nsRect& aRect, const nsRect& aDirtyRect,
                                  DrawOverflow) override;

  bool CreateWebRenderCommandsForWidget(
      mozilla::wr::DisplayListBuilder&, mozilla::wr::IpcResourceUpdateQueue&,
      const mozilla::layers::StackingContextHelper&,
      mozilla::layers::RenderRootStateManager*, nsIFrame*, StyleAppearance,
      const nsRect&) override;

  [[nodiscard]] LayoutDeviceIntMargin GetWidgetBorder(
      nsDeviceContext* aContext, nsIFrame* aFrame,
      StyleAppearance aAppearance) override;

  bool GetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                        StyleAppearance aAppearance,
                        LayoutDeviceIntMargin* aResult) override;

  virtual bool GetWidgetOverflow(nsDeviceContext* aContext, nsIFrame* aFrame,
                                 StyleAppearance aAppearance,
                                 nsRect* aOverflowRect) override;

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  virtual Transparency GetWidgetTransparency(
      nsIFrame* aFrame, StyleAppearance aAppearance) override;

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, StyleAppearance aAppearance,
                                nsAtom* aAttribute, bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;

  NS_IMETHOD ThemeChanged() override;

  bool ThemeSupportsWidget(nsPresContext* aPresContext, nsIFrame* aFrame,
                           StyleAppearance aAppearance) override;

  bool WidgetIsContainer(StyleAppearance aAppearance) override;

  bool ThemeDrawsFocusForWidget(nsIFrame*, StyleAppearance) override;

  bool ThemeWantsButtonInnerFocusRing(nsIFrame*, StyleAppearance) override {
    return true;
  }

  bool ThemeNeedsComboboxDropmarker() override;

  bool WidgetAppearanceDependsOnWindowFocus(StyleAppearance) override;

  enum { eThemeGeometryTypeWindowButtons = eThemeGeometryTypeUnknown + 1 };
  ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame*,
                                               StyleAppearance) override;

  ScrollbarSizes GetScrollbarSizes(nsPresContext*, StyleScrollbarWidth,
                                   Overlay) override;

  explicit nsNativeThemeWin(
      mozilla::UniquePtr<ScrollbarDrawing>&& aScrollbarDrawingWin);

 protected:
  mozilla::Maybe<nsUXThemeClass> GetThemeClass(StyleAppearance aAppearance);
  HANDLE GetTheme(StyleAppearance aAppearance);
  nsresult GetThemePartAndState(nsIFrame* aFrame, StyleAppearance aAppearance,
                                int32_t& aPart, int32_t& aState);
  nsresult ClassicGetThemePartAndState(nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       int32_t& aPart, int32_t& aState,
                                       bool& aFocused);
  nsresult ClassicDrawWidgetBackground(gfxContext* aContext, nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       const nsRect& aRect,
                                       const nsRect& aClipRect);
  [[nodiscard]] LayoutDeviceIntMargin ClassicGetWidgetBorder(
      nsDeviceContext* aContext, nsIFrame* aFrame, StyleAppearance aAppearance);
  bool ClassicGetWidgetPadding(nsDeviceContext* aContext, nsIFrame* aFrame,
                               StyleAppearance aAppearance,
                               LayoutDeviceIntMargin* aResult);
  nsresult ClassicGetMinimumWidgetSize(nsIFrame* aFrame,
                                       StyleAppearance aAppearance,
                                       mozilla::LayoutDeviceIntSize* aResult,
                                       bool* aIsOverridable);
  bool ClassicThemeSupportsWidget(nsIFrame* aFrame,
                                  StyleAppearance aAppearance);
  void DrawCheckedRect(HDC hdc, const RECT& rc, int32_t fore, int32_t back,
                       HBRUSH defaultBack);
  bool MayDrawCustomScrollbarPart(gfxContext* aContext, nsIFrame* aFrame,
                                  StyleAppearance aAppearance,
                                  const nsRect& aRect, const nsRect& aClipRect);
  uint32_t GetWidgetNativeDrawingFlags(StyleAppearance aAppearance);
  int32_t StandardGetState(nsIFrame* aFrame, StyleAppearance aAppearance,
                           bool wantFocused);
  bool IsMenuActive(nsIFrame* aFrame, StyleAppearance aAppearance);
  RECT CalculateProgressOverlayRect(nsIFrame* aFrame, RECT* aWidgetRect,
                                    bool aIsVertical, bool aIsIndeterminate,
                                    bool aIsClassic);
  void DrawThemedProgressMeter(nsIFrame* aFrame, StyleAppearance aAppearance,
                               HANDLE aTheme, HDC aHdc, int aPart, int aState,
                               RECT* aWidgetRect, RECT* aClipRect);

  [[nodiscard]] LayoutDeviceIntMargin GetCachedWidgetBorder(
      HANDLE aTheme, nsUXThemeClass aThemeClass, StyleAppearance aAppearance,
      int32_t aPart, int32_t aState);

  nsresult GetCachedMinimumWidgetSize(nsIFrame* aFrame, HANDLE aTheme,
                                      nsUXThemeClass aThemeClass,
                                      StyleAppearance aAppearance,
                                      int32_t aPart, int32_t aState,
                                      THEMESIZE aSizeReq,
                                      mozilla::LayoutDeviceIntSize* aResult);

  SIZE GetCachedGutterSize(HANDLE theme);

 private:
  TimeStamp mProgressDeterminateTimeStamp;
  TimeStamp mProgressIndeterminateTimeStamp;

  // eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT is about 800 at the time of
  // writing this, and nsIntMargin is 16 bytes wide, which makes this cache (1/8
  // + 16) * 800 bytes, or about ~12KB. We could probably reduce this cache to
  // 3KB by caching on the aAppearance value instead, but there would be some
  // uncacheable values, since we derive some theme parts from other arguments.
  uint8_t
      mBorderCacheValid[(eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT + 7) /
                        8];
  LayoutDeviceIntMargin
      mBorderCache[eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT];

  // See the above not for mBorderCache and friends. However
  // mozilla::LayoutDeviceIntSize is half the size of nsIntMargin, making the
  // cache roughly half as large. In total the caches should come to about 18KB.
  uint8_t mMinimumWidgetSizeCacheValid
      [(eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT + 7) / 8];
  mozilla::LayoutDeviceIntSize
      mMinimumWidgetSizeCache[eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT];

  bool mGutterSizeCacheValid;
  SIZE mGutterSizeCache;
};

}  // namespace mozilla::widget

#endif
