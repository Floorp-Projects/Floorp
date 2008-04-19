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
#include <QComboBox>
#include <QRect>
#include <QPainter>
#include <QStyleOption>
#include <QStyleOptionFrameV2>
#include <QStyleOptionButton>
#include <QFlags>

#include "nsCoord.h"
#include "nsNativeThemeQt.h"
#include "nsIDeviceContext.h"


#include "nsRect.h"
#include "nsSize.h"
#include "nsTransform2D.h"
#include "nsThemeConstants.h"
#include "nsILookAndFeel.h"
#include "nsIServiceManager.h"
#include "nsIEventStateManager.h"
#include <malloc.h>


#include "gfxASurface.h"
#include "gfxContext.h"
#include "gfxQPainterSurface.h"
#include "nsIRenderingContext.h"

nsNativeThemeQt::nsNativeThemeQt() : mP2A(0)
{
    combo = new QComboBox((QWidget *)0);
    combo->resize(0, 0);
    ThemeChanged();
}

nsNativeThemeQt::~nsNativeThemeQt()
{
    delete combo;
}

NS_IMPL_ISUPPORTS1(nsNativeThemeQt, nsITheme)

static QRect qRectInPixels(const nsRect &aRect,
    const nsTransform2D *aTrans, const PRInt32 p2a)
{
    int x = aRect.x;
    int y = aRect.y;
    int w = aRect.width;
    int h = aRect.height;
    aTrans->TransformCoord(&x,&y,&w,&h);
    return QRect(
        NSAppUnitsToIntPixels(x, p2a),
        NSAppUnitsToIntPixels(y, p2a),
        NSAppUnitsToIntPixels(w, p2a),
        NSAppUnitsToIntPixels(h, p2a));
}

NS_IMETHODIMP
nsNativeThemeQt::DrawWidgetBackground(nsIRenderingContext* aContext,
                                      nsIFrame* aFrame,
                                      PRUint8 aWidgetType,
                                      const nsRect& aRect,
                                      const nsRect& aClipRect)
{
    qDebug("%s : %d", __PRETTY_FUNCTION__, IsDisabled(aFrame));

    gfxContext* context = aContext->ThebesContext();
    nsRefPtr<gfxASurface> surface = context->CurrentSurface();
    gfxASurface* raw = surface;
    gfxQPainterSurface* qSurface = (gfxQPainterSurface*)raw;
    QPainter* qPainter = qSurface->GetQPainter();

//     qDebug("aWidgetType = %d", aWidgetType);
    if (!qPainter)
        return NS_OK;

    EnsuremP2A(aContext);

    QStyle* style = qApp->style();
//    const QPalette::ColorGroup cg = qApp->palette().currentColorGroup();

    nsTransform2D* curTrans;
    aContext->GetCurrentTransform(curTrans);

    QRect r = qRectInPixels(aRect, curTrans, mP2A);
    QRect cr = qRectInPixels(aClipRect, curTrans, mP2A);

//    context->UpdateGC();
    qPainter->save();
    qPainter->translate(r.x(), r.y());
    r.translate(-r.x(), -r.y());

//     qDebug("rect=%d %d %d %d\nr=%d %d %d %d",
//         aRect.x, aRect.y, aRect.width, aRect.height,
//         r.x(), r.y(), r.width(), r.height());

    QStyle::PrimitiveElement pe = QStyle::PE_CustomBase;

    QStyle::ControlElement ce = QStyle::CE_CustomBase;

    QStyle::State eventFlags = QStyle::State_None;
    /*IsDisabled(aFrame) ?
                            QStyle::State_None :
                            QStyle::State_Enabled;*/

    PRInt32 eventState = GetContentState(aFrame, aWidgetType);
//     qDebug("eventState = %d",  eventState);

    if (eventState & NS_EVENT_STATE_HOVER) {
//        qDebug("NS_EVENT_STATE_HOVER");
        eventFlags |= QStyle::State_MouseOver;
    }
    if (eventState & NS_EVENT_STATE_FOCUS) {
//        qDebug("NS_EVENT_STATE_FOCUS");
        eventFlags |= QStyle::State_HasFocus;
    }
    if (eventState & NS_EVENT_STATE_ACTIVE) {
//        qDebug("NS_EVENT_STATE_ACTIVE");
        eventFlags |= QStyle::State_DownArrow;
    }

    switch (aWidgetType) {
    case NS_THEME_RADIO:
    case NS_THEME_RADIO_SMALL: {
        qDebug("NS_THEME_RADIO");

        ce = QStyle::CE_RadioButton;

        QStyleOptionButton option;

        ButtonStyle(aFrame, r, &option, eventFlags);

        style->drawControl(ce, &option, qPainter, NULL);
        break;
    }
    case NS_THEME_CHECKBOX: {
        qDebug("NS_THEME_CHECKBOX");

        ce = QStyle::CE_CheckBox;

        QStyleOptionButton option;

        ButtonStyle(aFrame, r, &option, eventFlags);

        style->drawControl(ce, &option, qPainter, NULL);
        break;
    }
    case NS_THEME_SCROLLBAR: {
        qDebug("NS_THEME_SCROLLBAR");
        qPainter->fillRect(r, qApp->palette().brush(QPalette::Active, QPalette::Background));
        break;
    }
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL: {
        qDebug("NS_THEME_SCROLLBAR_TRACK_HORIZONTAL");
        qPainter->fillRect(r, qApp->palette().brush(QPalette::Active, QPalette::Background));
        break;
    }
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL: {
        qDebug("NS_THEME_SCROLLBAR_TRACK_VERTICAL");
        qPainter->fillRect(r, qApp->palette().brush(QPalette::Active, QPalette::Background));
        break;
    }
    case NS_THEME_SCROLLBAR_BUTTON_LEFT: {
        qDebug("NS_THEME_SCROLLBAR_BUTTON_LEFT");
        eventFlags = QStyle::State_Horizontal;
    }
    // Fall through 
    case NS_THEME_SCROLLBAR_BUTTON_UP: {
        qDebug("NS_THEME_SCROLLBAR_BUTTON_UP");
        
        ce = QStyle::CE_ScrollBarSubLine;
        
        QStyleOption option;

        PlainStyle(aFrame, r, &option, eventFlags);

        style->drawControl(ce, &option, qPainter, NULL);
        break;
    }
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT: {
        qDebug("NS_THEME_SCROLLBAR_BUTTON_RIGHT");
        eventFlags = QStyle::State_Horizontal;
    }
    // Fall through 
    case NS_THEME_SCROLLBAR_BUTTON_DOWN: {
        qDebug("NS_THEME_SCROLLBAR_BUTTON_DOWN");
        
        ce = QStyle::CE_ScrollBarAddLine;
        
        QStyleOption option;

        PlainStyle(aFrame, r, &option, eventFlags);

        style->drawControl(ce, &option, qPainter, NULL);
        break;
    }
    //case NS_THEME_SCROLLBAR_GRIPPER_HORIZONTAL:
    //case NS_THEME_SCROLLBAR_GRIPPER_VERTICAL:
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL: {
        qDebug("NS_THEME_SCROLLBAR_THUMB_HORIZONTAL");
        eventFlags = QStyle::State_Horizontal;
    }
    // Fall through
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL: {
        qDebug("NS_THEME_SCROLLBAR_THUMB_VERTICAL");
        
        ce = QStyle::CE_ScrollBarSlider;
        
        QStyleOption option;

        PlainStyle(aFrame, r, &option, eventFlags);

        style->drawControl(ce, &option, qPainter, NULL);
        break;
    }
    case NS_THEME_BUTTON_BEVEL:
        qDebug("NS_THEME_BUTTON_BEVEL");
//         ce = QStyle::CE_PushButtonBevel;
//         flags |= QStyle::State_Raised;
        break;
    case NS_THEME_BUTTON: {
        qDebug("NS_THEME_BUTTON %d", IsDefaultButton(aFrame));

        ce = QStyle::CE_PushButton;

        eventFlags |= QStyle::State_Raised;

        QStyleOptionButton option;
        
        ButtonStyle(aFrame, r, &option, eventFlags);
        
        style->drawControl(ce, &option, qPainter, NULL);
        break;
    }
    case NS_THEME_DROPDOWN:
        qDebug("NS_THEME_DROPDOWN");
//        style.drawComplexControl(QStyle::CC_ComboBox, qPainter, combo, r, cg, flags, QStyle::SC_ComboBoxFrame);
        break;
    case NS_THEME_DROPDOWN_BUTTON:
        qDebug("NS_THEME_DROPDOWN_BUTTON");
//         r.translate(frameWidth, -frameWidth);
//         r.setHeight(r.height() + 2*frameWidth);
//        style.drawComplexControl(QStyle::CC_ComboBox, qPainter, combo, r, cg, flags, QStyle::SC_ComboBoxArrow);
        break;
    case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_DROPDOWN_TEXTFIELD:
        qDebug("NS_THEME_DROPDOWN_TEXT");
        break;
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    case NS_THEME_LISTBOX: {
        qDebug("NS_THEME_TEXTFIELD");
        
        pe = QStyle::PE_PanelLineEdit;

        QStyleOptionFrameV2 option;

        FrameStyle(aFrame, r, &option, eventFlags);

        style->drawPrimitive(pe, &option, qPainter, NULL);
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
                                 nsMargin* aResult)
{
//    qDebug("%s : %d", __PRETTY_FUNCTION__, frameWidth);

    (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right = 0;

//     switch(aWidgetType) {
//     case NS_THEME_TEXTFIELD:
//     case NS_THEME_LISTBOX:
//         (*aResult).top = (*aResult).bottom = (*aResult).left = (*aResult).right =
//                          frameWidth;
//     }

    return NS_OK;
}

PRBool
nsNativeThemeQt::GetWidgetPadding(nsIDeviceContext* ,
                                  nsIFrame*, PRUint8 aWidgetType,
                                  nsMargin* aResult)
{
//    qDebug("%s", __PRETTY_FUNCTION__);

    //XXX: is this really correct?
//     if (aWidgetType == NS_THEME_BUTTON_FOCUS ||
//         aWidgetType == NS_THEME_TOOLBAR_BUTTON ||
//         aWidgetType == NS_THEME_TOOLBAR_DUAL_BUTTON) {
//         aResult->SizeTo(0, 0, 0, 0);
//         return PR_TRUE;
//     }
    return PR_FALSE;
}

NS_IMETHODIMP
nsNativeThemeQt::GetMinimumWidgetSize(nsIRenderingContext* aContext, nsIFrame* aFrame,
                                      PRUint8 aWidgetType,
                                      nsSize* aResult, PRBool* aIsOverridable)
{
//    qDebug("%s", __PRETTY_FUNCTION__);

    (*aResult).width = (*aResult).height = 0;
    *aIsOverridable = PR_TRUE;

    QStyle *s = qApp->style();

    switch (aWidgetType) {
    case NS_THEME_RADIO_SMALL:
    case NS_THEME_RADIO:
    case NS_THEME_CHECKBOX: {
        nsRect frameRect = aFrame->GetRect();

        EnsuremP2A(aContext);

        nsTransform2D* curTrans;
        aContext->GetCurrentTransform(curTrans);

        QRect qRect = qRectInPixels(frameRect, curTrans, mP2A);

        QStyleOptionButton option;

        ButtonStyle(aFrame, qRect, &option);

        QRect rect = s->subElementRect(
            aWidgetType == NS_THEME_CHECKBOX ?
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

        EnsuremP2A(aContext);

        nsTransform2D* curTrans;
        aContext->GetCurrentTransform(curTrans);

        QRect qRect = qRectInPixels(frameRect, curTrans, mP2A);

        QStyleOptionButton option;

        ButtonStyle(aFrame, qRect, &option);

        QRect rect = s->subElementRect(
            QStyle::SE_PushButtonFocusRect,
            &option,
            NULL);

        (*aResult).width = rect.width();
        (*aResult).height = rect.height();
        break;
    }
    case NS_THEME_SCROLLBAR_BUTTON_UP:
    case NS_THEME_SCROLLBAR_BUTTON_DOWN:
    case NS_THEME_SCROLLBAR_BUTTON_LEFT:
    case NS_THEME_SCROLLBAR_BUTTON_RIGHT:
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).height = (*aResult).width;
        //*aIsOverridable = PR_FALSE;
        break;
    case NS_THEME_SCROLLBAR_THUMB_VERTICAL:
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarSliderMin);
        //*aIsOverridable = PR_FALSE;
        break;
    case NS_THEME_SCROLLBAR_THUMB_HORIZONTAL:
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarSliderMin);
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        //*aIsOverridable = PR_FALSE;
        break;
    case NS_THEME_SCROLLBAR_TRACK_VERTICAL:
        (*aResult).width = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        (*aResult).height = s->pixelMetric(QStyle::PM_SliderLength);
        break;
    case NS_THEME_SCROLLBAR_TRACK_HORIZONTAL:
        (*aResult).width = s->pixelMetric(QStyle::PM_SliderLength);
        (*aResult).height = s->pixelMetric(QStyle::PM_ScrollBarExtent);
        break;
    case NS_THEME_DROPDOWN_BUTTON: {
//        QRect r = s->subControlRect(QStyle::CC_ComboBox, combo, QStyle::SC_ComboBoxArrow);
//        (*aResult).width = r.width() - 2*frameWidth;
//        (*aResult).height = r.height() - 2*frameWidth;
        break;
    }
    case NS_THEME_DROPDOWN:
    case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_DROPDOWN_TEXTFIELD:
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
//    qDebug("%s", __PRETTY_FUNCTION__);

    *aShouldRepaint = TRUE;
    return NS_OK;
}


NS_IMETHODIMP
nsNativeThemeQt::ThemeChanged()
{
//     qDebug("%s", __PRETTY_FUNCTION__);

    QStyle *s = qApp->style();
    if (s)
      frameWidth = s->pixelMetric(QStyle::PM_DefaultFrameWidth);
    return NS_OK;
}

PRBool
nsNativeThemeQt::ThemeSupportsWidget(nsPresContext* aPresContext,
                                     nsIFrame* aFrame,
                                     PRUint8 aWidgetType)
{
//     qDebug("%s", __PRETTY_FUNCTION__);
//     return FALSE;

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
    case NS_THEME_RADIO_SMALL:
    case NS_THEME_CHECKBOX:
    case NS_THEME_BUTTON_BEVEL:
    case NS_THEME_BUTTON:
    //case NS_THEME_DROPDOWN:
    //case NS_THEME_DROPDOWN_BUTTON:
    //case NS_THEME_DROPDOWN_TEXT:
    case NS_THEME_DROPDOWN_TEXTFIELD:
    case NS_THEME_TEXTFIELD:
    case NS_THEME_TEXTFIELD_MULTILINE:
    //case NS_THEME_LISTBOX:
//         qDebug("%s : return PR_TRUE", __PRETTY_FUNCTION__);
        return PR_TRUE;
    default:
        break;
    }
//     qDebug("%s : return PR_FALSE", __PRETTY_FUNCTION__);
    return PR_FALSE;
}

PRBool
nsNativeThemeQt::WidgetIsContainer(PRUint8 aWidgetType)
{
//    qDebug("%s", __PRETTY_FUNCTION__);
    // XXXdwh At some point flesh all of this out.
/*
    if (aWidgetType == NS_THEME_DROPDOWN_BUTTON ||
        aWidgetType == NS_THEME_RADIO ||
        aWidgetType == NS_THEME_CHECKBOX) {
//         qDebug("%s : return PR_FALSE", __PRETTY_FUNCTION__);
        return PR_FALSE;
    }
//     qDebug("%s : return PR_TRUE", __PRETTY_FUNCTION__);
*/
    return PR_TRUE;
}

PRBool
nsNativeThemeQt::ThemeDrawsFocusForWidget(nsPresContext* aPresContext, nsIFrame* aFrame, PRUint8 aWidgetType)
{
//    qDebug("%s", __PRETTY_FUNCTION__);
/*
    if (aWidgetType == NS_THEME_DROPDOWN ||
        aWidgetType == NS_THEME_BUTTON || 
        aWidgetType == NS_THEME_TREEVIEW_HEADER_CELL) { 
//         qDebug("%s : return PR_TRUE", __PRETTY_FUNCTION__);
        return PR_TRUE;
    }
//     qDebug("%s : return PR_FALSE", __PRETTY_FUNCTION__);
*/
    return PR_FALSE;
}

PRBool
nsNativeThemeQt::ThemeNeedsComboboxDropmarker()
{
//    qDebug("%s", __PRETTY_FUNCTION__);
    return PR_FALSE;
}

void
nsNativeThemeQt::EnsuremP2A(nsIRenderingContext* aContext)
{
    if (!mP2A) {
        nsCOMPtr<nsIDeviceContext> dctx = nsnull;
        aContext->GetDeviceContext(*getter_AddRefs(dctx));
        mP2A = dctx->AppUnitsPerDevPixel();
    }
}

void
nsNativeThemeQt::ButtonStyle(nsIFrame* aFrame,
                             QRect aRect,
                             QStyleOptionButton* aOption,
                             QStyle::State optFlags /*= QStyle::State_None*/)
{
    QStyle::State flags = IsDisabled(aFrame) ?
        QStyle::State_None :
        QStyle::State_Enabled;

    flags |= IsChecked(aFrame) ?
        QStyle::State_On :
        QStyle::State_Off;

    flags |= optFlags;

    (*aOption).direction = QApplication::layoutDirection();
    (*aOption).rect = aRect;
    (*aOption).type = QStyleOption::SO_Button;
    (*aOption).state = flags;
    (*aOption).features = QStyleOptionButton::None;
}

void
nsNativeThemeQt::FrameStyle(nsIFrame* aFrame,
                            QRect aRect,
                            QStyleOptionFrameV2* aOption,
                            QStyle::State optFlags /*= QStyle::State_None*/)
{
    QStyle::State flags = IsDisabled(aFrame) ?
        QStyle::State_None :
        QStyle::State_Enabled;
    
    flags |= optFlags;

    (*aOption).direction = QApplication::layoutDirection();
    (*aOption).rect = aRect;
    (*aOption).type = QStyleOption::SO_Frame;
    (*aOption).state = flags;
    (*aOption).lineWidth = 1;
    (*aOption).midLineWidth = 1;
    (*aOption).features = QStyleOptionFrameV2::Flat;
}

void
nsNativeThemeQt::PlainStyle(nsIFrame* aFrame,
                            QRect aRect,
                            QStyleOption* aOption,
                            QStyle::State optFlags /*= QStyle::State_None*/)
{
    QStyle::State flags = IsDisabled(aFrame) ?
        QStyle::State_None :
        QStyle::State_Enabled;
    
    flags |= optFlags;

    (*aOption).direction = QApplication::layoutDirection();
    (*aOption).rect = aRect;
    (*aOption).type = QStyleOption::SO_Default;
    (*aOption).state = flags;
}
