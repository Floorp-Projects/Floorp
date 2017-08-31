/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.model;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

/**
 * Shared interface for activity stream items that model a url/link item.
 */
public interface WebpageModel {
    String getTitle();

    String getUrl();

    /**
     * Returns the image URL associated with this stream item.
     *
     * Some implementations may be slow due to lazy loading: see {@link Highlight#getImageUrl()}.
     *
     * @return the image URL, or the empty String when there is none.
     */
    @NonNull
    String getImageUrl();

    /**
     * @return True if the item is bookmarked, false otherwise. Might return 'null' if the bookmark
     *         state is unknown and the database needs to be asked whether the URL is bookmarked.
     */
    @Nullable
    Boolean isBookmarked();

    /**
     * @return True if the item is pinned, false otherwise. Will return 'null' if the pinned state
     * is unknown or this item can't be pinned.
     */
    @Nullable
    Boolean isPinned();

    void updateBookmarked(boolean bookmarked);

    void updatePinned(boolean pinned);

    /**
     * Handle any model updates that need to happen when committing state, such as bookmark or
     * pin state.
     */
    void onStateCommitted();
}
