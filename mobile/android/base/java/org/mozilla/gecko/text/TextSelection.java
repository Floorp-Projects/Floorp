/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.text;

import org.mozilla.gecko.ActionBarTextSelection;
import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.widget.ActionModePresenter;
import org.mozilla.geckoview.GeckoView;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

public interface TextSelection {
    void create();

    boolean dismiss();

    void destroy();

    static class Factory {
        public static TextSelection create(@NonNull final GeckoView geckoView,
                                           @Nullable final ActionModePresenter presenter) {
            if (Versions.preMarshmallow) {
                return new ActionBarTextSelection(geckoView, presenter);
            } else {
                return new FloatingToolbarTextSelection(geckoView);
            }
        }
    }
}
