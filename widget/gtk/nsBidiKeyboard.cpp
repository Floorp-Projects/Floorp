/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlink.h"

#include "nsBidiKeyboard.h"
#include "nsIWidget.h"
#include <gtk/gtk.h>

NS_IMPL_ISUPPORTS(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard() { Reset(); }

NS_IMETHODIMP
nsBidiKeyboard::Reset() {
  // NB: The default keymap can be null (e.g. in xpcshell). In that case,
  // simply assume that we don't have bidi keyboards.
  mHaveBidiKeyboards = false;

  GdkDisplay* display = gdk_display_get_default();
  if (!display) return NS_OK;

  GdkKeymap* keymap = gdk_keymap_get_for_display(display);
  mHaveBidiKeyboards = keymap && gdk_keymap_have_bidi_layouts(keymap);
  return NS_OK;
}

nsBidiKeyboard::~nsBidiKeyboard() = default;

NS_IMETHODIMP
nsBidiKeyboard::IsLangRTL(bool* aIsRTL) {
  if (!mHaveBidiKeyboards) return NS_ERROR_FAILURE;

  *aIsRTL = (gdk_keymap_get_direction(gdk_keymap_get_default()) ==
             PANGO_DIRECTION_RTL);

  return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::GetHaveBidiKeyboards(bool* aResult) {
  // not implemented yet
  return NS_ERROR_NOT_IMPLEMENTED;
}

// static
already_AddRefed<nsIBidiKeyboard> nsIWidget::CreateBidiKeyboardInner() {
  return do_AddRef(new nsBidiKeyboard());
}
