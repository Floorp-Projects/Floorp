/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsQtKeyUtils_h__
#define __nsQtKeyUtils_h__

#include "nsEvent.h"

int      QtKeyCodeToDOMKeyCode     (int aKeysym);
int      DOMKeyCodeToQtKeyCode     (int aKeysym);

mozilla::widget::KeyNameIndex QtKeyCodeToDOMKeyNameIndex(int aKeysym);

#endif /* __nsQtKeyUtils_h__ */
