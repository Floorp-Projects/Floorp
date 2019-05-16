/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.view.Menu;
import android.view.MenuItem;

public class MenuUtils {
    /*
     * This method looks for a menuitem and sets it's visible state, if
     * it exists.
     */
    public static void safeSetVisible(final Menu menu, final int id, final boolean visible) {
        MenuItem item = menu.findItem(id);
        if (item != null) {
            item.setVisible(visible);
        }
    }

    /*
     * This method looks for a menuitem and sets it's enabled state, if
     * it exists.
     */
    public static void safeSetEnabled(final Menu menu, final int id, final boolean enabled) {
        MenuItem item = menu.findItem(id);
        if (item != null) {
            item.setEnabled(enabled);
        }
    }
}
