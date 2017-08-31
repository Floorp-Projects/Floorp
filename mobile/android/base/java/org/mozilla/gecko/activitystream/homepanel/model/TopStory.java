/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.model;

import android.support.annotation.Nullable;

import org.mozilla.gecko.activitystream.Utils;
import org.mozilla.gecko.activitystream.homepanel.StreamRecyclerAdapter;

public class TopStory implements WebpageRowModel {
    private @Nullable Boolean isBookmarked;
    private @Nullable Boolean isPinned;

    private final String title;
    private final String url;
    private final String imageUrl;

    public TopStory(String title, String url, String imageUrl) {
        this.title = title;
        this.url = url;
        this.imageUrl = imageUrl;
    }

    @Override
    public String getTitle() {
        return title;
    }

    @Override
    public String getUrl() {
        return url;
    }

    @Override
    public String getImageUrl() {
        return imageUrl;
    }

    @Override
    public StreamRecyclerAdapter.RowItemType getRowItemType() {
        return StreamRecyclerAdapter.RowItemType.TOP_STORIES_ITEM;
    }

    @Override
    public Boolean isBookmarked() {
        return isBookmarked;
    }

    @Override
    public Boolean isPinned() {
        return isPinned;
    }

    @Override
    public void updateBookmarked(boolean bookmarked) {
        this.isBookmarked = bookmarked;
    }

    @Override
    public void updatePinned(boolean pinned) {
        this.isPinned = pinned;
    }

    @Override
    public Utils.HighlightSource getSource() {
        return Utils.HighlightSource.POCKET;
    }

    @Override
    public long getUniqueId() {
        return getUrl().hashCode();
    }

    /**
     * Pinned and Bookmarked state are loaded in {@link org.mozilla.gecko.activitystream.homepanel.menu.ActivityStreamContextMenu#postInit()},
     * and will be fetched if the state cached in {@link TopStory} is null (See {@link WebpageModel#isPinned}
     * and {@link WebpageModel#isBookmarked}
     **/
    @Override
    public void onStateCommitted() {
        // Since Top Stories are not loaded from a cursor that automatically notifies of
        // changes to bookmark or pin state, trash this cached state once we've committed it
        // so it will be fetched every time.
        isPinned = null;
        isBookmarked = null;
    }
}
