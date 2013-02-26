/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <Qt>
#include <QApplication>

#include "nsBidiKeyboard.h"

NS_IMPL_ISUPPORTS1(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard() : nsIBidiKeyboard()
{
}

nsBidiKeyboard::~nsBidiKeyboard()
{
}

NS_IMETHODIMP nsBidiKeyboard::IsLangRTL(bool *aIsRTL)
{
    *aIsRTL = false;

#if (QT_VERSION < QT_VERSION_CHECK(5,0,0))
    Qt::LayoutDirection layoutDir = QApplication::keyboardInputDirection();
#else
    QInputMethod* input = qApp->inputMethod();
    Qt::LayoutDirection layoutDir = input ? input->inputDirection() : Qt::LeftToRight;
#endif

    if (layoutDir == Qt::RightToLeft) {
        *aIsRTL = true;
    }
    
    return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::SetLangFromBidiLevel(uint8_t aLevel)
{
    return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::GetHaveBidiKeyboards(bool* aResult)
{
  // not implemented yet
  return NS_ERROR_NOT_IMPLEMENTED;
}
