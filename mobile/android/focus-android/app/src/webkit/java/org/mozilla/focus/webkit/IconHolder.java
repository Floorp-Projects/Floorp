/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.webkit;

import android.graphics.Bitmap;

/**
 * Helper class to keep a reference to the current page's icon.
 */
/* package */ class IconHolder {
    private Bitmap currentIcon;

    /* package */ void updateIcon(final Bitmap bitmap) {
        if (currentIcon == null) {
            // We do not have an icon for the current website yet. So let's just use this one.
            currentIcon = bitmap;
            return;
        }

        if (bitmap.getWidth() * bitmap.getHeight() > currentIcon.getWidth() * currentIcon.getHeight()) {
            // The new icon is bigger, let's use the new one.
            currentIcon = bitmap;
        }
    }

    /* package */ void clearIcon() {
        currentIcon = null;
    }

    /* package */ Bitmap getIcon() {
        return currentIcon;
    }
}
