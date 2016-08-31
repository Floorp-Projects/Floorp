/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.home;

import android.graphics.Bitmap;

import org.mozilla.gecko.favicons.OnFaviconLoadedListener;
import org.mozilla.gecko.widget.FaviconView;

import java.lang.ref.WeakReference;

// Only holds a reference to the FaviconView itself, so if the row gets
// discarded while a task is outstanding, we'll leak less memory.
public class UpdateViewFaviconLoadedListener implements OnFaviconLoadedListener {
    private final WeakReference<FaviconView> view;
    public UpdateViewFaviconLoadedListener(FaviconView view) {
        this.view = new WeakReference<FaviconView>(view);
    }

    /**
     * Update this row's favicon.
     * <p>
     * This method is always invoked on the UI thread.
     */
    @Override
    public void onFaviconLoaded(String url, String faviconURL, Bitmap favicon) {
        FaviconView v = view.get();
        if (v == null) {
            // Guess we stuck around after the TwoLinePageRow went away.
            return;
        }

        if (favicon == null) {
            v.showDefaultFavicon(url);
            return;
        }

        v.updateImage(favicon, faviconURL);
    }
}
