/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.adjust;

import android.content.Context;
import android.content.Intent;

public class StubAdjustHelper implements AdjustHelperInterface {
    public void onCreate(final Context context, final String appToken) {
        // Do nothing.
    }

    public void onReceive(final Context context, final Intent intent) {
        // Do nothing.
    }
}
