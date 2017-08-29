/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.support.annotation.NonNull;
import android.view.View;
import android.view.ViewTreeObserver;

/**
 * A OnPreDrawListener implementation that will execute a callback once and then unsubscribe itself.
 */
public abstract class OneShotOnPreDrawListener implements ViewTreeObserver.OnPreDrawListener {
    private final View view;

    public OneShotOnPreDrawListener(@NonNull View view) {
        this.view = view;
    }

    protected abstract void onPreDraw(View view);

    @Override
    public final boolean onPreDraw() {
        view.getViewTreeObserver().removeOnPreDrawListener(this);

        onPreDraw(view);

        return true;
    }
}
