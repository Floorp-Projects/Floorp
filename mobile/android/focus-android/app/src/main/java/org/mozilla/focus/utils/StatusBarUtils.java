/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils;

import android.view.View;
import android.view.WindowInsets;

public class StatusBarUtils {
    private static int STATUS_BAR_HEIGHT = -1;

    public interface StatusBarHeightListener {
        void onStatusBarHeightFetched(int statusBarHeight);
    }

    public static void getStatusBarHeight(final View view, final StatusBarHeightListener listener) {
        if (STATUS_BAR_HEIGHT > 0) {
            listener.onStatusBarHeightFetched(STATUS_BAR_HEIGHT);
        }

        view.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                STATUS_BAR_HEIGHT = insets.getSystemWindowInsetTop();
                listener.onStatusBarHeightFetched(STATUS_BAR_HEIGHT);
                return insets;
            }
        });
    }
}
