/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.bookmarks;

import android.os.Bundle;

import org.mozilla.gecko.home.HomeContextMenuInfo;

public interface EditBookmarkCallback {
    /**
     * A callback method to tell caller that undo action after editing a bookmark has been pressed.
     * Caller takes charge for the change(e.g. update database).
     */
    void onUndoEditBookmark(Bundle bundle);

    /**
     * A callback method to tell caller that undo action after deleting a bookmark has been pressed.
     * Caller takes charge for the change(e.g. update database).
     */
    void onUndoRemoveBookmark(HomeContextMenuInfo info, int position);
}
