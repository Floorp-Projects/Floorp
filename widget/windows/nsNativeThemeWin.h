/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeWin_h
#define nsNativeThemeWin_h

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsAtom.h"
#include "nsNativeTheme.h"
#include "nsThemeConstants.h"
#include "nsUXThemeConstants.h"
#include "nsUXThemeData.h"
#include "gfxTypes.h"
#include <windows.h>
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "nsSize.h"

class nsNativeThemeWin : private nsNativeTheme,
                         public nsITheme {
  virtual ~nsNativeThemeWin();

public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(gfxContext* aContext,
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aDirtyRect) override;

  nscolor GetWidgetAutoColor(mozilla::ComputedStyle* aStyle,
                             uint8_t aWidgetType) override;

  MOZ_MUST_USE LayoutDeviceIntMargin GetWidgetBorder(nsDeviceContext* aContext,
                                                     nsIFrame* aFrame,
                                                     uint8_t aWidgetType) override;

  bool GetWidgetPadding(nsDeviceContext* aContext,
                        nsIFrame* aFrame,
                        uint8_t aWidgetType,
                        LayoutDeviceIntMargin* aResult) override;

  virtual bool GetWidgetOverflow(nsDeviceContext* aContext,
                                   nsIFrame* aFrame,
                                   uint8_t aWidgetType,
                                   nsRect* aOverflowRect) override;

  NS_IMETHOD GetMinimumWidgetSize(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  mozilla::LayoutDeviceIntSize* aResult,
                                  bool* aIsOverridable) override;

  virtual Transparency GetWidgetTransparency(nsIFrame* aFrame, uint8_t aWidgetType) override;

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType, 
                                nsAtom* aAttribute, bool* aShouldRepaint,
                                const nsAttrValue* aOldValue) override;

  NS_IMETHOD ThemeChanged() override;

  bool ThemeSupportsWidget(nsPresContext* aPresContext, 
                             nsIFrame* aFrame,
                             uint8_t aWidgetType) override;

  bool WidgetIsContainer(uint8_t aWidgetType) override;

  bool ThemeDrawsFocusForWidget(uint8_t aWidgetType) override;

  bool ThemeNeedsComboboxDropmarker() override;

  virtual bool WidgetAppearanceDependsOnWindowFocus(uint8_t aWidgetType) override;

  enum {
    eThemeGeometryTypeWindowButtons = eThemeGeometryTypeUnknown + 1
  };
  virtual ThemeGeometryType ThemeGeometryTypeForWidget(nsIFrame* aFrame,
                                                       uint8_t aWidgetType) override;

  nsNativeThemeWin();

protected:
  mozilla::Maybe<nsUXThemeClass> GetThemeClass(uint8_t aWidgetType);
  HANDLE GetTheme(uint8_t aWidgetType);
  nsresult GetThemePartAndState(nsIFrame* aFrame, uint8_t aWidgetType,
                                int32_t& aPart, int32_t& aState);
  nsresult ClassicGetThemePartAndState(nsIFrame* aFrame, uint8_t aWidgetType,
                                       int32_t& aPart, int32_t& aState, bool& aFocused);
  nsresult ClassicDrawWidgetBackground(gfxContext* aContext,
                                       nsIFrame* aFrame,
                                       uint8_t aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aClipRect);
  MOZ_MUST_USE LayoutDeviceIntMargin ClassicGetWidgetBorder(nsDeviceContext* aContext,
                                                            nsIFrame* aFrame,
                                                            uint8_t aWidgetType);
  bool ClassicGetWidgetPadding(nsDeviceContext* aContext,
                               nsIFrame* aFrame,
                               uint8_t aWidgetType,
                               LayoutDeviceIntMargin* aResult);
  nsresult ClassicGetMinimumWidgetSize(nsIFrame* aFrame, uint8_t aWidgetType,
                                       mozilla::LayoutDeviceIntSize* aResult,
                                       bool* aIsOverridable);
  bool ClassicThemeSupportsWidget(nsIFrame* aFrame, uint8_t aWidgetType);
  void DrawCheckedRect(HDC hdc, const RECT& rc, int32_t fore, int32_t back,
                       HBRUSH defaultBack);
  nsresult DrawCustomScrollbarPart(gfxContext* aContext,
                                   nsIFrame* aFrame,
                                   mozilla::ComputedStyle* aStyle,
                                   uint8_t aWidgetType,
                                   const nsRect& aRect,
                                   const nsRect& aClipRect);
  uint32_t GetWidgetNativeDrawingFlags(uint8_t aWidgetType);
  int32_t StandardGetState(nsIFrame* aFrame, uint8_t aWidgetType, bool wantFocused);
  bool IsMenuActive(nsIFrame* aFrame, uint8_t aWidgetType);
  RECT CalculateProgressOverlayRect(nsIFrame* aFrame, RECT* aWidgetRect,
                                    bool aIsVertical, bool aIsIndeterminate,
                                    bool aIsClassic);
  void DrawThemedProgressMeter(nsIFrame* aFrame, int aWidgetType,
                               HANDLE aTheme, HDC aHdc,
                               int aPart, int aState,
                               RECT* aWidgetRect, RECT* aClipRect);

  MOZ_MUST_USE LayoutDeviceIntMargin GetCachedWidgetBorder(HANDLE aTheme,
                                                           nsUXThemeClass aThemeClass,
                                                           uint8_t aWidgetType,
                                                           int32_t aPart,
                                                           int32_t aState);

  nsresult GetCachedMinimumWidgetSize(nsIFrame* aFrame, HANDLE aTheme, nsUXThemeClass aThemeClass,
                                      uint8_t aWidgetType, int32_t aPart, int32_t aState,
                                      THEMESIZE aSizeReq, mozilla::LayoutDeviceIntSize* aResult);

  SIZE GetCachedGutterSize(HANDLE theme);

private:
  TimeStamp mProgressDeterminateTimeStamp;
  TimeStamp mProgressIndeterminateTimeStamp;

  // eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT is about 800 at the time of writing
  // this, and nsIntMargin is 16 bytes wide, which makes this cache (1/8 + 16) * 800
  // bytes, or about ~12KB. We could probably reduce this cache to 3KB by caching on
  // the aWidgetType value instead, but there would be some uncacheable values, since
  // we derive some theme parts from other arguments.
  uint8_t mBorderCacheValid[(eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT + 7) / 8];
  LayoutDeviceIntMargin mBorderCache[eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT];

  // See the above not for mBorderCache and friends. However mozilla::LayoutDeviceIntSize
  // is half the size of nsIntMargin, making the cache roughly half as large. In total
  // the caches should come to about 18KB.
  uint8_t mMinimumWidgetSizeCacheValid[(eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT + 7) / 8];
  mozilla::LayoutDeviceIntSize mMinimumWidgetSizeCache[eUXNumClasses * THEME_PART_DISTINCT_VALUE_COUNT];

  bool mGutterSizeCacheValid;
  SIZE mGutterSizeCache;
};

#endif
