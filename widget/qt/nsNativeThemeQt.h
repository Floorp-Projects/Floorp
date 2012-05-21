/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QStyle>
#include <QPalette>

#include "nsITheme.h"
#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsNativeTheme.h"

class QComboBox;
class QStyleOptionButton;
class QStyleOptionComboBox;
class QRect;
class nsIFrame;
class nsDeviceContext;

class nsNativeThemeQt : private nsNativeTheme,
                        public nsITheme
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  // The nsITheme interface.
  NS_IMETHOD DrawWidgetBackground(nsRenderingContext* aContext,
                                  nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  const nsRect& aRect,
                                  const nsRect& aClipRect);

  NS_IMETHOD GetWidgetBorder(nsDeviceContext* aContext,
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType,
                             nsIntMargin* aResult);

  NS_IMETHOD GetMinimumWidgetSize(nsRenderingContext* aContext, nsIFrame* aFrame,
                                  PRUint8 aWidgetType,
                                  nsIntSize* aResult,
                                  bool* aIsOverridable);

  NS_IMETHOD WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType,
                                nsIAtom* aAttribute, bool* aShouldRepaint);

  NS_IMETHOD ThemeChanged();

  bool ThemeSupportsWidget(nsPresContext* aPresContext,
                             nsIFrame* aFrame,
                             PRUint8 aWidgetType);

  bool WidgetIsContainer(PRUint8 aWidgetType);

  virtual NS_HIDDEN_(bool) GetWidgetPadding(nsDeviceContext* aContext,
                                              nsIFrame* aFrame,
                                              PRUint8 aWidgetType,
                                              nsIntMargin* aResult);

  NS_IMETHOD_(bool) ThemeDrawsFocusForWidget(nsPresContext* aPresContext,
                                               nsIFrame* aFrame, PRUint8 aWidgetType);

  bool ThemeNeedsComboboxDropmarker();

  nsNativeThemeQt();
  virtual ~nsNativeThemeQt();

private:

  inline nsresult DrawWidgetBackground(QPainter *qPainter,
                                       nsRenderingContext* aContext,
                                       nsIFrame* aFrame,
                                       PRUint8 aWidgetType,
                                       const nsRect& aRect,
                                       const nsRect& aClipRect);

  void InitButtonStyle(PRUint8 widgetType,
                       nsIFrame* aFrame,
                       QRect rect,
                       QStyleOptionButton &opt);

  void InitPlainStyle(PRUint8 aWidgetType,
                      nsIFrame* aFrame,
                      QRect rect,
                      QStyleOption &opt,
                      QStyle::State extraFlags = QStyle::State_None);

  void InitComboStyle(PRUint8 aWidgetType,
                      nsIFrame* aFrame,
                      QRect rect,
                      QStyleOptionComboBox &opt);

private:

  PRInt32 mFrameWidth;

  QPalette mNoBackgroundPalette;
};

