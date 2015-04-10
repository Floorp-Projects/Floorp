/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlink.h"

#include "nsBidiKeyboard.h"
#include <gtk/gtk.h>

NS_IMPL_ISUPPORTS(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard()
{
    Reset();
}

NS_IMETHODIMP
nsBidiKeyboard::Reset()
{
    // NB: The default keymap can be null (e.g. in xpcshell). In that case,
    // simply assume that we don't have bidi keyboards.
    GdkKeymap *keymap = gdk_keymap_get_default();
    mHaveBidiKeyboards = keymap && gdk_keymap_have_bidi_layouts(keymap);
    return NS_OK;
}

nsBidiKeyboard::~nsBidiKeyboard()
{
}

NS_IMETHODIMP
nsBidiKeyboard::IsLangRTL(bool *aIsRTL)
{
    if (!mHaveBidiKeyboards)
        return NS_ERROR_FAILURE;

    *aIsRTL = (gdk_keymap_get_direction(gdk_keymap_get_default()) == PANGO_DIRECTION_RTL);

    return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::GetHaveBidiKeyboards(bool* aResult)
{
  // not implemented yet
  return NS_ERROR_NOT_IMPLEMENTED;
}
