/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#include "nsIFrame.h"

#include "nsCoord.h"
#include "nsNativeThemeQt.h"
#include "nsPresContext.h"

#include "nsRect.h"
#include "nsSize.h"
#include "nsTransform2D.h"
#include "nsThemeConstants.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLInputElement.h"


#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxQtPlatform.h"
#include "gfxQPainterSurface.h"
#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif
#include "nsRenderingContext.h"

nsNativeThemeQt::nsNativeThemeQt()
{
    mNoBackgroundPalette.setColor(QPalette::Window, Qt::transparent);
    ThemeChanged();
}

nsNativeThemeQt::~nsNativeThemeQt()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsNativeThemeQt, nsNativeTheme, nsITheme)

static inline QRect qRectInPixels(const nsRect &aRect,
                                  const int32_t p2a)
{
    return QRect(NSAppUnitsToIntPixels(aRect.x, p2a),
                 NSAppUnitsToIntPixels(aRect.y, p2a),
                 NSAppUnitsToIntPixels(aRect.width, p2a),
                 NSAppUnitsToIntPixels(aRect.height, p2a));
}

static inline QImage::Format
_qimage_from_gfximage_format (gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxASurface::ImageFormatARGB32:
        return QImage::Format_ARGB32_Premultiplied;
    case gfxASurface::ImageFormatRGB24:
        return QImage::Format_RGB32;
    case gfxASurface::ImageFormatRGB16_565:
        return QImage::Format_RGB16;
    case gfxASurface::ImageFormatA8:
        return QImage::Format_Indexed8;
    case gfxASurface::ImageFormatA1:
#ifdef WORDS_BIGENDIAN
        return QImage::Format_Mono;
#else
        return QImage::Format_MonoLSB;
#endif
    default:
        return QImage::Format_Invalid;
    }

    return QImage::Format_Mono;
}

NS_IMETHODIMP
nsNativeThemeQt::DrawWidgetBackground(nsRenderingContext* aContext,
                                      nsIFrame* aFrame,
                                      uint8_t aWidgetType,
                                      const nsRect& aRect,
                                      const nsRect& aClipRect)
{
    gfxContext* context = aContext->ThebesContext();
    nsRefPtr<gfxASurface> surface = context->CurrentSurface();

#ifdef CAIRO_HAS_QT_SURFACE
    if (surface->GetType() == gfxASurface::SurfaceTypeQPainter) {
        gfxQPainterSurface* qSurface = (gfxQPainterSurface*) (surface.get());
        QPainter *painter = qSurface->GetQPainter();
        NS_ASSERTION(painter, "Where'd my QPainter go?");
        if (!painter)
            return NS_ERROR_FAILURE;
        return DrawWidgetBackground(painter, aContext,
                                    aFrame, aWidgetType,
                                    aRect, aClipRect);
    } else
#endif
    if (surface->GetType() == gfxASurface::SurfaceTypeImage) {
        gfxImageSurface* qSurface = (gfxImageSurface*) (surface.get());
        QImage tempQImage(qSurface->Data(),
                          qSurface->Width(),
                          qSurface->Height(),
                          qSurface->Stride(),
                          _qimage_from_gfximage_format(qSurface->Format()));
        QPainter painter(&tempQImage);
        return DrawWidgetBackground(&painter, aContext,
                                    aFrame, aWidgetType,
                                    aRect, aClipRect);
    }
#if defined(MOZ_X11) && defined(Q_WS_X11)
    else if (surface->GetType() == gfxASurface::SurfaceTypeXlib) {
        gfxXlibSurface* qSurface = (gfxXlibSurface*) (surface.get());
        QPixmap pixmap(QPixmap::fromX11Pixmap(qSurface->XDrawable()));
        QPainter painter(&pixmap);
        return DrawWidgetBackground(&painter, aContext,
                                    aFrame, aWidgetType,
                                    aRect, aClipRect);
    }
#endif

    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsNativeThemeQt::DrawWidgetBackground(QPainter *qPainter,
                                      nsRenderingContext* aContext,
                                      nsIFrame* aFrame,
                                      uint8_t aWidgetType,
                                      const nsRect& aRect,
                                      const nsRect& aClipRect)

{
    gfxContext* context = aContext->ThebesContext();
    nsRefPtr<gfxASurface> surface = context->CurrentSurface();

    context->UpdateSurfaceClip();

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

    int32_t p2a = aContext->AppUnitsPerDevPixel();

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
        nsEventStates eventState = GetContentState(aFrame, aWidgetType);

        if (!IsDisabled(aFrame, eventState))
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
nsNativeThemeQt::GetWidgetBorder(nsDeviceContext* ,
                                 nsIFrame* aFrame,
                                 uint8_t aWidgetType,
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

bool
nsNativeThemeQt::GetWidgetPadding(nsDeviceContext* ,
                                  nsIFrame*, uint8_t aWidgetType,
                                  nsIntMargin* aResult)
{
    // XXX: Where to get padding values, framewidth?
    if (aWidgetType == NS_THEME_TEXTFIELD ||
        aWidgetType == NS_THEME_TEXTFIELD_MULTILINE ||
        aWidgetType == NS_THEME_DROPDOWN) {
        aResult->SizeTo(2, 2, 2, 2);
        return true;
    }

    return false;
}

NS_IMETHODIMP
nsNativeThemeQt::GetMinimumWidgetSize(nsRenderingContext* aContext, nsIFrame* aFrame,
                                      uint8_t aWidgetType,
                                      nsIntSize* aResult, bool* aIsOverridable)
{
    (*aResult).width = (*aResult).height = 0;
    *aIsOverridable = true;

    QStyle *s = qApp->style();

    int32_t p2a = aContext->AppUnitsPerDevPixel();

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
        //*aIsOverridable = false;
        break;
    }
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).width = (*aResult).height;
        //*aIsOverridable = false;
        break;
        }
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL: {
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarSliderMin);
        //*aIsOverridable = false;
        break;
        }
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL: {
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarSliderMin);
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        //*aIsOverridable = false;
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
        //*aIsOverridable = false;
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
        //*aIsOverridable = false;
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
        //*aIsOverridable = false;
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
        //*aIsOverridable = false;
        break;
    }
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
        break;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsNativeThemeQt::WidgetStateChanged(nsIFrame* aFrame, uint8_t aWidgetType,
                                    nsIAtom* aAttribute, bool* aShouldRepaint)
{
    *aShouldRepaint = true;
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

bool
nsNativeThemeQt::ThemeSupportsWidget(nsPresContext* aPresContext,
                                     nsIFrame* aFrame,
                                     uint8_t aWidgetType)
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
    return false;
}

bool
nsNativeThemeQt::WidgetIsContainer(uint8_t aWidgetType)
{
//     if (aWidgetType == NS_THEME_DROPDOWN_BUTTON ||
//         aWidgetType == NS_THEME_RADIO ||
//         aWidgetType == NS_THEME_CHECKBOX) {
//         return false;
//     }

   return true;
}

bool
nsNativeThemeQt::ThemeDrawsFocusForWidget(uint8_t aWidgetType)
{
    if (aWidgetType == NS_THEME_DROPDOWN ||
        aWidgetType == NS_THEME_BUTTON || 
        aWidgetType == NS_THEME_TREEVIEW_HEADER_CELL) { 
        return true;
    }

    return false;
}

bool
nsNativeThemeQt::ThemeNeedsComboboxDropmarker()
{
    return true;
}

void
nsNativeThemeQt::InitButtonStyle(uint8_t aWidgetType,
                                 nsIFrame* aFrame,
                                 QRect rect,
                                 QStyleOptionButton &opt)
{
    nsEventStates eventState = GetContentState(aFrame, aWidgetType);

    opt.rect = rect;
    opt.palette = mNoBackgroundPalette;

    bool isDisabled = IsDisabled(aFrame, eventState);

    if (!isDisabled)
        opt.state |= QStyle::State_Enabled;
    if (eventState.HasState(NS_EVENT_STATE_HOVER))
        opt.state |= QStyle::State_MouseOver;
    if (eventState.HasState(NS_EVENT_STATE_FOCUS))
        opt.state |= QStyle::State_HasFocus;
    if (!isDisabled && eventState.HasState(NS_EVENT_STATE_ACTIVE))
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
        if (!eventState.HasState(NS_EVENT_STATE_ACTIVE))
            opt.state |= QStyle::State_Raised;
        break;
    }
}

void
nsNativeThemeQt::InitPlainStyle(uint8_t aWidgetType,
                                nsIFrame* aFrame,
                                QRect rect,
                                QStyleOption &opt,
                                QStyle::State extraFlags)
{
    nsEventStates eventState = GetContentState(aFrame, aWidgetType);

    opt.rect = rect;

    if (!IsDisabled(aFrame, eventState))
        opt.state |= QStyle::State_Enabled;
    if (eventState.HasState(NS_EVENT_STATE_HOVER))
        opt.state |= QStyle::State_MouseOver;
    if (eventState.HasState(NS_EVENT_STATE_FOCUS))
        opt.state |= QStyle::State_HasFocus;

    opt.state |= extraFlags;
}

void
nsNativeThemeQt::InitComboStyle(uint8_t aWidgetType,
                                nsIFrame* aFrame,
                                QRect rect,
                                QStyleOptionComboBox &opt)
{
    nsEventStates eventState = GetContentState(aFrame, aWidgetType);
    bool isDisabled = IsDisabled(aFrame, eventState);

    if (!isDisabled)
        opt.state |= QStyle::State_Enabled;
    if (eventState.HasState(NS_EVENT_STATE_HOVER))
        opt.state |= QStyle::State_MouseOver;
    if (eventState.HasState(NS_EVENT_STATE_FOCUS))
        opt.state |= QStyle::State_HasFocus;
    if (!eventState.HasState(NS_EVENT_STATE_ACTIVE))
        opt.state |= QStyle::State_Raised;
    if (!isDisabled && eventState.HasState(NS_EVENT_STATE_ACTIVE))
        // Don't allow sunken when disabled
        opt.state |= QStyle::State_Sunken;

    opt.rect = rect;
    opt.palette = mNoBackgroundPalette;
}
