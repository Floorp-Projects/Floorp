/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons;

import android.graphics.Bitmap;

/**
 * Interface to be implemented by objects wishing to listen for favicon load completion events.
 */
public interface OnFaviconLoadedListener {
    void onFaviconLoaded(String url, Bitmap favicon);
}
