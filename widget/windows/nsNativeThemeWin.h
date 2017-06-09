/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNativeThemeWin_h
#define nsNativeThemeWin_h

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsNativeTheme.h"
#include "gfxTypes.h"
#include <windows.h>
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

  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext, 
                             nsIFrame* aFrame,
                             uint8_t aWidgetType,
                             nsIntMargin* aResult) override;

  virtual bool GetWidgetPadding(nsDeviceContext* aContext,
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  nsIntMargin* aResult) override;

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
                                nsIAtom* aAttribute, bool* aShouldRepaint,
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

  virtual bool ShouldHideScrollbars() override;

  nsNativeThemeWin();

protected:
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
  nsresult ClassicGetWidgetBorder(nsDeviceContext* aContext, 
                                  nsIFrame* aFrame,
                                  uint8_t aWidgetType,
                                  nsIntMargin* aResult);
  bool ClassicGetWidgetPadding(nsDeviceContext* aContext,
                               nsIFrame* aFrame,
                               uint8_t aWidgetType,
                               nsIntMargin* aResult);
  nsresult ClassicGetMinimumWidgetSize(nsIFrame* aFrame, uint8_t aWidgetType,
                                       mozilla::LayoutDeviceIntSize* aResult,
                                       bool* aIsOverridable);
  bool ClassicThemeSupportsWidget(nsIFrame* aFrame, uint8_t aWidgetType);
  void DrawCheckedRect(HDC hdc, const RECT& rc, int32_t fore, int32_t back,
                       HBRUSH defaultBack);
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

private:
  TimeStamp mProgressDeterminateTimeStamp;
  TimeStamp mProgressIndeterminateTimeStamp;
};

#endif
