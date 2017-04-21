/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.bookmarks;

import android.content.Context;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.Experiments;
import org.mozilla.gecko.switchboard.SwitchBoard;

public class BookmarkUtils {

    /**
     * Get the switch from {@link SwitchBoard}. It's used to enable/disable
     * full bookmark management features(full-page dialog, bookmark/folder modification, etc.)
     */
    public static boolean isEnabled(Context context) {
        return AppConstants.NIGHTLY_BUILD &&
                       SwitchBoard.isInExperiment(context, Experiments.FULL_BOOKMARK_MANAGEMENT);
    }
}
