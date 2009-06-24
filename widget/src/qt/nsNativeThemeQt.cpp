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
 * David Hyatt (hyatt@netscape.com).
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   Tim Hill (tim@prismelite.com)
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIFrame.h"

#include <QApplication>
#include <QStyle>
#include <QPalette>
#include <QRect>
#include <QPainter>
#include <QStyleOption>
#include <QStyleOptionFrameV2>
#include <QStyleOptionButton>
#include <QFlags>
#include <QStyleOptionComboBox>

#include "nsCoord.h"
#include "nsNativeThemeQt.h"
#include "nsIDeviceContext.h"
#include "nsPresContext.h"

#include "nsRect.h"
#include "nsSize.h"
#include "nsTransform2D.h"
#include "nsThemeConstants.h"
#include "nsILookAndFeel.h"
#include "nsIServiceManager.h"
#include "nsIEventStateManager.h"
#include "nsIDOMHTMLInputElement.h"
#include <malloc.h>


#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxQPainterSurface.h"
#include "nsIRenderingContext.h"

nsNativeThemeQt::nsNativeThemeQt()
{
    mNoBackgroundPalette.setColor(QPalette::Window, Qt::transparent);
    ThemeChanged();
}

nsNativeThemeQt::~nsNativeThemeQt()
{
}

NS_IMPL_ISUPPORTS1(nsNativeThemeQt, nsITheme)

static inline QRect qRectInPixels(const nsRect &aRect,
                                  const PRInt32 p2a)
{
    return QRect(NSAppUnitsToIntPixels(aRect.x, p2a),
                 NSAppUnitsToIntPixels(aRect.y, p2a),
                 NSAppUnitsToIntPixels(aRect.width, p2a),
                 NSAppUnitsToIntPixels(aRect.height, p2a));
}

NS_IMETHODIMP
nsNativeThemeQt::DrawWidgetBackground(nsIRenderingContext* aContext,
                                      nsIFrame* aFrame,
                                      PRUint8 aWidgetType,
                                      const nsRect& aRect,
                                      const nsRect& aClipRect)
{
    gfxContext* context = aContext->ThebesContext();
    nsRefPtr<gfxASurface> surface = context->CurrentSurface();

    context->UpdateSurfaceClip();

    if (surface->GetType() != gfxASurface::SurfaceTypeQPainter)
        return NS_ERROR_NOT_IMPLEMENTED;

    gfxQPainterSurface* qSurface = (gfxQPainterSurface*) (surface.get());
    QPainter* qPainter = qSurface->GetQPainter();

    NS_ASSERTION(qPainter, "Where'd my QPainter go?");

    if (qPainter == nsnull)
        return NS_OK;

    QStyle* style = qApp->style();

    qPainter->save();

    gfxPoint offs = surface->GetDeviceOffset();
    qPainter->translate(offs.x, offs.y);

    gfxMatrix ctm = context->CurrentMatrix();
    if (!ctm.HasNonTranslation()) {
        ctm.x0 = NSToCoordRound(ctm.x0);
        ctm.y0 = NSToCoordRound(ctm.y0);
    }

    QMatrix qctm(ctm.xx, ctm.yx, ctm.xy, ctm.yy, ctm.x0, ctm.y0);
    qPainter->setWorldMatrix(qctm, true);

    PRInt32 p2a = GetAppUnitsPerDevPixel(aContext);

    QRect r = qRectInPixels(aRect, p2a);
    QRect cr = qRectInPixels(aClipRect, p2a);

    QStyle::State extraFlags = QStyle::State_None;

    switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX: {
        QStyleOptionButton opt;
        InitButtonStyle (aWidgetType, aFrame, r, opt);

        if (aWidgetType == NS_THEME_CHECKBOX)
        {
            style->drawPrimitive (QStyle::PE_IndicatorCheckBox, &opt, qPainter);
        } else {
            style->drawPrimitive (QStyle::PE_IndicatorRadioButton, &opt, qPainter);
        }
        break;
    }
    case NS_THEME_BUTTON:
    case NS_THEME_BUTTON_BEVEL: {
        QStyleOptionButton opt;
        InitButtonStyle (aWidgetType, aFrame, r, opt);

        if (aWidgetType == NS_THEME_BUTTON) {
            style->drawPrimitive(QStyle::PE_PanelButtonCommand, &opt, qPainter);
            if (IsDefaultButton(aFrame))
                style->drawPrimitive(QStyle::PE_FrameDefaultButton, &opt, qPainter);
        } else {
            style->drawPrimitive(QStyle::PE_PanelButtonBevel, &opt, qPainter);
            style->drawPrimitive(QStyle::PE_FrameButtonBevel, &opt, qPainter);
        }
        break;
    }
    case NS_THEME_SCROLLBAR: {
        qPainter->fillRect(r, qApp->palette().brush(QPalette::Normal, QPalette::Window));
        break;
    }
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL: {
        qPainter->fillRect(r, qApp->palette().brush(QPalette::Active, QPalette::Window));
        break;
    }
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL: {
        qPainter->fillRect(r, qApp->palette().brush(QPalette::Active, QPalette::Window));
        break;
    }
    case NS_THEME_SCROLLBAR_BUTTON_LEFT: {
        QStyleOptionSlider opt;
        InitPlainStyle(aWidgetType, aFrame, r, (QStyleOption&)opt, QStyle::State_Horizontal);
        opt.orientation = Qt::Horizontal;
        style->drawControl(QStyle::CE_ScrollBarSubLine, &opt, qPainter, NULL);
        break;
        }
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
        QStyleOptionSlider opt;
        InitPlainStyle(aWidgetType, aFrame, r, (QStyleOption&)opt, QStyle::State_Horizontal);
        opt.orientation = Qt::Horizontal;
        style->drawControl(QStyle::CE_ScrollBarAddLine, &opt, qPainter, NULL);
        break;
        }
    case NS_THEME_SCROLLBAR_BUTTON_UP: {
        QStyleOptionSlider opt;
        InitPlainStyle(aWidgetType, aFrame, r, (QStyleOption&)opt);
        opt.orientation = Qt::Vertical;
        style->drawControl(QStyle::CE_ScrollBarSubLine, &opt, qPainter, NULL);
        break;
    }
    case NS_THEME_SCROLLBAR_BUTTON_DOWN: {
        QStyleOptionSlider opt;
        InitPlainStyle(aWidgetType, aFrame, r, (QStyleOption&)opt);
        opt.orientation = Qt::Vertical;
        style->drawControl(QStyle::CE_ScrollBarAddLine, &opt, qPainter, NULL);
        break;
    }
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL: {
        extraFlags |= QStyle::State_Horizontal;
        QStyleOptionSlider option;
        InitPlainStyle(aWidgetType, aFrame, r, (QStyleOption&)option, extraFlags);
        option.orientation = Qt::Horizontal;
        style->drawControl(QStyle::CE_ScrollBarSlider, &option, qPainter, NULL);
        break;
        }
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL: {
        QStyleOptionSlider option;
        InitPlainStyle(aWidgetType, aFrame, r, (QStyleOption&)option, extraFlags);
        option.orientation = Qt::Vertical;
        style->drawControl(QStyle::CE_ScrollBarSlider, &option, qPainter, NULL);
        break;
    }
    case NS_THEME_DROPDOWN: {
        QStyleOptionComboBox comboOpt;
        
        InitComboStyle(aWidgetType, aFrame, r, comboOpt);

        style->drawComplexControl(QStyle::CC_ComboBox, &comboOpt, qPainter);
        break;
    }
    case NS_THEME_DROPDOWN_BUTTON: {
        QStyleOptionComboBox option;

        InitComboStyle(aWidgetType, aFrame, r, option);

        style->drawPrimitive(QStyle::PE_FrameDefaultButton, &option, qPainter);
        style->drawPrimitive(QStyle::PE_IndicatorSpinDown, &option, qPainter);
        break;
    }
    case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_LISTBOX: {
        QStyleOptionFrameV2 frameOpt;
        
        if (!IsDisabled(aFrame))
            frameOpt.state |= QStyle::State_Enabled;

        frameOpt.rect = r;
        frameOpt.features = QStyleOptionFrameV2::Flat;

        if (aWidgetType == NS_THEME_TEXTFIELD || aWidgetType == NS_THEME_TEXTFIELD_MULTILINE) {
            QRect contentRect = style->subElementRect(QStyle::SE_LineEditContents, &frameOpt);
            contentRect.adjust(mFrameWidth, mFrameWidth, -mFrameWidth, -mFrameWidth);
            qPainter->fillRect(contentRect, QBrush(Qt::white));
        }
        
        frameOpt.palette = mNoBackgroundPalette;
        style->drawPrimitive(QStyle::PE_FrameLineEdit, &frameOpt, qPainter, NULL);
        break;
    }
    case NS_THEME_MENUPOPUP: {
        QStyleOptionMenuItem option;
        InitPlainStyle(aWidgetType, aFrame, r, (QStyleOption&)option, extraFlags);
        style->drawPrimitive(QStyle::PE_FrameMenu, &option, qPainter, NULL);
        break;
    }
    default:
        break;
    }

    qPainter->restore();
    return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeQt::GetWidgetBorder(nsIDeviceContext* aContext,
                                 nsIFrame* aFrame,
                                 PRUint8 aWidgetType,
                                 nsIntMargin* aResult)
{
    (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 0;

    QStyle* style = qApp->style();
    switch(aWidgetType) {
//     case NS_THEME_TEXTFIELD:
//     case NS_THEME_LISTBOX:
    case NS_THEME_MENUPOPUP: {
       (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = style->pixelMetric(QStyle::PM_MenuPanelWidth);
       break;
    }
    default:
        break;
    }

    return NS_OK;
}

PRBool
nsNativeThemeQt::GetWidgetPadding(nsIDeviceContext* ,
                                  nsIFrame*, PRUint8 aWidgetType,
                                  nsIntMargin* aResult)
{
    // XXX: Where to get padding values, framewidth?
    if (aWidgetType == NS_THEME_TEXTFIELD ||
        aWidgetType == NS_THEME_TEXTFIELD_MULTILINE ||
        aWidgetType == NS_THEME_DROPDOWN) {
        aResult->SizeTo(2, 2, 2, 2);
        return PR_TRUE;
    }

    return PR_FALSE;
}

NS_IMETHODIMP
nsNativeThemeQt::GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                      PRUint8 aWidgetType,
                                      nsIntSize* aResult, PRBool* aIsOverridable)
{
    (*aResult).width = (*aResult).height = 0;
    *aIsOverridable = PR_TRUE;

    QStyle *s = qApp->style();

    PRInt32 p2a = GetAppUnitsPerDevPixel(aContext);

    switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX: {
        nsRect frameRect = aFrame->GetRect();

        QRect qRect = qRectInPixels(frameRect, p2a);

        QStyleOptionButton option;

        InitButtonStyle(aWidgetType, aFrame, qRect, option);

        QRect rect = s->subElementRect(
            (aWidgetType == NS_THEME_CHECKBOX) ?
                QStyle::SE_CheckBoxIndicator :
                QStyle::SE_RadioButtonIndicator,
            &option,
            NULL);

        (*aResult).width = rect.width();
        (*aResult).height = rect.height();
        break;
    }
    case NS_THEME_BUTTON: {
        nsRect frameRect = aFrame->GetRect();

        QRect qRect = qRectInPixels(frameRect, p2a);

        QStyleOptionButton option;

        InitButtonStyle(aWidgetType, aFrame, qRect, option);

        QRect rect = s->subElementRect(
            QStyle::SE_PushButtonFocusRect,
            &option,
            NULL);

        (*aResult).width = rect.width();
        (*aResult).height = rect.height();
        break;
    }
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN: {
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).height = (*aResult).width;
        //*aIsOverridable = PR_FALSE;
        break;
    }
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).width = (*aResult).height;
        //*aIsOverridable = PR_FALSE;
        break;
        }
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL: {
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarSliderMin);
        //*aIsOverridable = PR_FALSE;
        break;
        }
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL: {
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarSliderMin);
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        //*aIsOverridable = PR_FALSE;
        break;
        }
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL: {
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).height = s->pixelMetric(QStyle::PM_SliderLength);
        break;
        }
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL: {
        (*aResult).width = s->pixelMetric(QStyle::PM_SliderLength);
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        break;
        }
    case NS_THEME_DROPDOWN_BUTTON: {
        QStyleOptionComboBox comboOpt;
        
        nsRect frameRect = aFrame->GetRect();
        QRect qRect = qRectInPixels(frameRect, p2a);
        comboOpt.rect = qRect;

        InitComboStyle(aWidgetType, aFrame, qRect, comboOpt);

        QRect subRect = s->subControlRect(QStyle::CC_ComboBox, &comboOpt, QStyle::SC_ComboBoxArrow, NULL);
  
        (*aResult).width = subRect.width();
        (*aResult).height = subRect.height();
        //*aIsOverridable = PR_FALSE;
        break;
    }
    case NS_THEME_DROPDOWN: {
        QStyleOptionComboBox comboOpt;

        nsRect frameRect = aFrame->GetRect();
        QRect qRect = qRectInPixels(frameRect, p2a);
        comboOpt.rect = qRect;

        InitComboStyle(aWidgetType, aFrame, qRect, comboOpt);

        QRect subRect = s->subControlRect(QStyle::CC_ComboBox, 
                                          &comboOpt, 
                                          QStyle::SC_ComboBoxFrame, 
                                          NULL);

        (*aResult).width = subRect.width();
        (*aResult).height = subRect.height();
        //*aIsOverridable = PR_FALSE;
        break;
    }
    case NS_THEME_DROPDOWN_TEXT: {
        QStyleOptionComboBox comboOpt;
        
        nsRect frameRect = aFrame->GetRect();

        QRect qRect = qRectInPixels(frameRect, p2a);
        
        comboOpt.rect = qRect;

        QRect subRect = s->subControlRect(QStyle::CC_ComboBox, &comboOpt, QStyle::SC_ComboBoxEditField, NULL);
       
        (*aResult).width = subRect.width();
        (*aResult).height = subRect.height();
        //*aIsOverridable = PR_FALSE;
        break;
    }
    case NS_THEME_DROPDOWN_TEXTFIELD: {
        QStyleOptionComboBox comboOpt;
        
        nsRect frameRect = aFrame->GetRect();

        QRect qRect = qRectInPixels(frameRect, p2a);

        comboOpt.rect = qRect;

        QRect subRect = s->subControlRect(QStyle::CC_ComboBox, &comboOpt, QStyle::SC_ComboBoxArrow, NULL);
        QRect subRect2 = s->subControlRect(QStyle::CC_ComboBox, &comboOpt, QStyle::SC_ComboBoxFrame, NULL);

        (*aResult).width = subRect.width() + subRect2.width();
        (*aResult).height = std::max(subRect.height(), subRect2.height());
        //*aIsOverridable = PR_FALSE;
        break;
    }
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
        break;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeQt::WidgetStateChanged(nsIFrame* aFrame, PRUint8 aWidgetType,
                                    nsIAtom* aAttribute, PRBool* aShouldRepaint)
{
    *aShouldRepaint = TRUE;
    return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeQt::ThemeChanged()
{
    QStyle *s = qApp->style();
    if (s)
      mFrameWidth = s->pixelMetric(QStyle::PM_DefaultFrameWidth);
    return NS_OK;
}

PRBool
nsNativeThemeQt::ThemeSupportsWidget(nsPresContext* aPresContext,
                                     nsIFrame* aFrame,
                                     PRUint8 aWidgetType)
{
    switch (aWidgetType) {
    case NS_THEME_SCROLLBAR:
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
    //case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    //case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
    case NS_THEME_BUTTON_BEVEL:
    case NS_THEME_BUTTON:
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_BUTTON:
    case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_LISTBOX:
    case NS_THEME_MENUPOPUP:
        return !IsWidgetStyled(aPresContext, aFrame, aWidgetType);
    default:
        break;
    }
    return PR_FALSE;
}

PRBool
nsNativeThemeQt::WidgetIsContainer(PRUint8 aWidgetType)
{
//     if (aWidgetType == NS_THEME_DROPDOWN_BUTTON ||
//         aWidgetType == NS_THEME_RADIO ||
//         aWidgetType == NS_THEME_CHECKBOX) {
//         return PR_FALSE;
//     }

   return PR_TRUE;
}

PRBool
nsNativeThemeQt::ThemeDrawsFocusForWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType)
{
    if (aWidgetType == NS_THEME_DROPDOWN ||
        aWidgetType == NS_THEME_BUTTON || 
        aWidgetType == NS_THEME_TREEVIEW_HEADER_CELL) { 
        return PR_TRUE;
    }

    return PR_FALSE;
}

PRBool
nsNativeThemeQt::ThemeNeedsComboboxDropmarker()
{
    return PR_TRUE;
}

nsTransparencyMode
nsNativeThemeQt::GetWidgetTransparency(PRUint8 aWidgetType)
{
  return eTransparencyOpaque;
}

void
nsNativeThemeQt::InitButtonStyle(PRUint8 aWidgetType,
                                 nsIFrame* aFrame,
                                 QRect rect,
                                 QStyleOptionButton &opt)
{
    PRInt32 eventState = GetContentState(aFrame, aWidgetType);

    opt.rect = rect;
    opt.palette = mNoBackgroundPalette;

    PRBool disabled = IsDisabled(aFrame);

    if (!disabled)
        opt.state |= QStyle::State_Enabled;
    if (eventState & NS_EVENT_STATE_HOVER)
        opt.state |= QStyle::State_MouseOver;
    if (eventState & NS_EVENT_STATE_FOCUS)
        opt.state |= QStyle::State_HasFocus;
    if (!disabled && eventState & NS_EVENT_STATE_ACTIVE)
        // Don't allow sunken when disabled
        opt.state |= QStyle::State_Sunken;

    switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX:
        if (IsChecked(aFrame))
            opt.state |= QStyle::State_On;
        else
            opt.state |= QStyle::State_Off;

        break;
    default:
        if (!(eventState & NS_EVENT_STATE_ACTIVE))
            opt.state |= QStyle::State_Raised;
        break;
    }
}

void
nsNativeThemeQt::InitPlainStyle(PRUint8 aWidgetType,
                                nsIFrame* aFrame,
                                QRect rect,
                                QStyleOption &opt,
                                QStyle::State extraFlags)
{
    PRInt32 eventState = GetContentState(aFrame, aWidgetType);

    opt.rect = rect;

    if (!IsDisabled(aFrame))
        opt.state |= QStyle::State_Enabled;
    if (eventState & NS_EVENT_STATE_HOVER)
        opt.state |= QStyle::State_MouseOver;
    if (eventState & NS_EVENT_STATE_FOCUS)
        opt.state |= QStyle::State_HasFocus;

    opt.state |= extraFlags;
}

void
nsNativeThemeQt::InitComboStyle(PRUint8 aWidgetType,
                                nsIFrame* aFrame,
                                QRect rect,
                                QStyleOptionComboBox &opt)
{
    PRInt32 eventState = GetContentState(aFrame, aWidgetType);

    PRBool disabled = IsDisabled(aFrame);

    if (!disabled)
        opt.state |= QStyle::State_Enabled;
    if (eventState & NS_EVENT_STATE_HOVER)
        opt.state |= QStyle::State_MouseOver;
    if (eventState & NS_EVENT_STATE_FOCUS)
        opt.state |= QStyle::State_HasFocus;
    if (!(eventState & NS_EVENT_STATE_ACTIVE))
        opt.state |= QStyle::State_Raised;
    if (!disabled && eventState & NS_EVENT_STATE_ACTIVE)
        // Don't allow sunken when disabled
        opt.state |= QStyle::State_Sunken;

    opt.rect = rect;
    opt.palette = mNoBackgroundPalette;
}
