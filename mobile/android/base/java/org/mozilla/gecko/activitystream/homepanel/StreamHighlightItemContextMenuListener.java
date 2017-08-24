/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel;

import android.support.annotation.NonNull;
import org.mozilla.gecko.activitystream.homepanel.stream.HighlightItem;

/**
 * Provides a method to open the context menu for a highlight item.
 *
 * I tried declaring this inside StreamRecyclerAdapter but I got cyclical inheritance warnings
 * (I don't understand why) so it's here instead.
 */
public interface StreamHighlightItemContextMenuListener {
    void openContextMenu(HighlightItem highlightItem, int actualPosition, @NonNull final String interactionExtra);
}
