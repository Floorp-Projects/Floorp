/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlink.h"

#include "nsBidiKeyboard.h"
#include <gtk/gtk.h>


static PRLibrary *gtklib = nullptr;

typedef gboolean (*GdkKeymapHaveBidiLayoutsType)(GdkKeymap *keymap);
static GdkKeymapHaveBidiLayoutsType GdkKeymapHaveBidiLayouts = nullptr;


NS_IMPL_ISUPPORTS1(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard()
{
#if defined(MOZ_X11)
    if (!gtklib)
        gtklib = PR_LoadLibrary("libgtk-x11-2.0.so.0");
#else
    return;
#endif

    if (gtklib && !GdkKeymapHaveBidiLayouts)
            GdkKeymapHaveBidiLayouts = (GdkKeymapHaveBidiLayoutsType) PR_FindFunctionSymbol(gtklib, "gdk_keymap_have_bidi_layouts");

    SetHaveBidiKeyboards();
}

nsBidiKeyboard::~nsBidiKeyboard()
{
    if (gtklib) {
        PR_UnloadLibrary(gtklib);
        gtklib = nullptr;

        GdkKeymapHaveBidiLayouts = nullptr;
    }
}

NS_IMETHODIMP
nsBidiKeyboard::IsLangRTL(bool *aIsRTL)
{
    if (!mHaveBidiKeyboards)
        return NS_ERROR_FAILURE;

    *aIsRTL = (gdk_keymap_get_direction(NULL) == PANGO_DIRECTION_RTL);

    return NS_OK;
}

nsresult
nsBidiKeyboard::SetHaveBidiKeyboards()
{
    mHaveBidiKeyboards = false;

    if (!gtklib || !GdkKeymapHaveBidiLayouts)
        return NS_ERROR_FAILURE;

    mHaveBidiKeyboards = (*GdkKeymapHaveBidiLayouts)(NULL);

    return NS_OK;
}

NS_IMETHODIMP
nsBidiKeyboard::SetLangFromBidiLevel(PRUint8 aLevel)
{
    // XXX Insert platform specific code to set keyboard language
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsBidiKeyboard::GetHaveBidiKeyboards(bool* aResult)
{
  // not implemented yet
  return NS_ERROR_NOT_IMPLEMENTED;
}
