package org.mozilla.gecko.activitystream.homepanel.model;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

/**
 * Shared interface for activity stream item models.
 */
public interface Item {
    String getTitle();

    String getUrl();

    /**
     * Returns the metadata associated with this stream item.
     *
     * This operation could be slow in some implementations (see {@link Highlight#getMetadataSlow()}), hence the name.
     * imo, it is better to expose this possibility in the interface for all implementations rather than hide this fact.
     */
    @NonNull
    Metadata getMetadataSlow();

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
