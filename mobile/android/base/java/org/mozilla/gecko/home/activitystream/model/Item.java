package org.mozilla.gecko.home.activitystream.model;

import android.support.annotation.Nullable;

/**
 * Shared interface for activity stream item models.
 */
public interface Item {
    String getTitle();

    String getUrl();

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
}
