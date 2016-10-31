/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.graphics.Rect;
import android.view.TouchDelegate;
import android.view.View;

import org.mozilla.gecko.R;

public class TouchTargetUtil {
    /**
     * Ensures that a given targetView has a large enough touch area to ensure it can be selected.
     * A TouchDelegate will be added to the enclosingView as necessary.
     *
     * @param targetView
     * @param enclosingView
     */
    public static void ensureTargetHitArea(final View targetView, final View enclosingView) {
        enclosingView.post(new Runnable() {
            @Override
            public void run() {
                Rect delegateArea = new Rect();
                targetView.getHitRect(delegateArea);

                final int targetHitArea = enclosingView.getContext().getResources().getDimensionPixelSize(R.dimen.touch_target_size);

                final int widthDelta = (targetHitArea - delegateArea.width()) / 2;
                delegateArea.right += widthDelta;
                delegateArea.left -= widthDelta;

                final int heightDelta = (targetHitArea - delegateArea.height()) / 2;
                delegateArea.bottom += heightDelta;
                delegateArea.top -= heightDelta;

                if (heightDelta <= 0 && widthDelta <= 0) {
                    return;
                }

                TouchDelegate touchDelegate = new TouchDelegate(delegateArea, targetView);
                enclosingView.setTouchDelegate(touchDelegate);
            }
        });
    }
}
