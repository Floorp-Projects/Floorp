/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Behnam Esfahbod <behnam@zwnj.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "prlink.h"

#include "nsBidiKeyboard.h"
#include <gtk/gtk.h>


static PRLibrary *gtklib = nsnull;

typedef gboolean (*GdkKeymapHaveBidiLayoutsType)(GdkKeymap *keymap);
static GdkKeymapHaveBidiLayoutsType GdkKeymapHaveBidiLayouts = nsnull;


NS_IMPL_ISUPPORTS1(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard()
{
#if defined(MOZ_X11)
    if (!gtklib)
        gtklib = PR_LoadLibrary("libgtk-x11-2.0.so.0");
#elif defined(MOZ_DFB)
    if (!gtklib)
        gtklib = PR_LoadLibrary("libgtk-directfb-2.0.so.0");
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
        gtklib = nsnull;

        GdkKeymapHaveBidiLayouts = nsnull;
    }
}

NS_IMETHODIMP
nsBidiKeyboard::IsLangRTL(PRBool *aIsRTL)
{
    if (!mHaveBidiKeyboards)
        return NS_ERROR_FAILURE;

    *aIsRTL = (gdk_keymap_get_direction(NULL) == PANGO_DIRECTION_RTL);

    return NS_OK;
}

nsresult
nsBidiKeyboard::SetHaveBidiKeyboards()
{
    mHaveBidiKeyboards = PR_FALSE;

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

