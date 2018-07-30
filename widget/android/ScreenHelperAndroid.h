/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScreenHelperAndroid_h___
#define ScreenHelperAndroid_h___

#include "mozilla/widget/ScreenManager.h"

class ScreenHelperAndroid final : public mozilla::widget::ScreenManager::Helper
{
public:
    ScreenHelperAndroid() {
        Refresh();
    }

    ~ScreenHelperAndroid() {
    }

    void Refresh();
};

#endif /* ScreenHelperAndroid_h___ */
