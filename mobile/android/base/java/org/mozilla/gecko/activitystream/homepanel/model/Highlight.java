/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.homepanel.model;

import android.database.Cursor;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import android.text.TextUtils;
import org.mozilla.gecko.activitystream.Utils;
import org.mozilla.gecko.activitystream.homepanel.StreamRecyclerAdapter;
import org.mozilla.gecko.activitystream.ranking.HighlightCandidateCursorIndices;
import org.mozilla.gecko.activitystream.ranking.HighlightsRanking;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Highlight implements WebpageRowModel {

    /**
     * A pattern matching a json object containing the key "image_url" and extracting the value. afaik, these urls
     * are not encoded so it's entirely possible that the url will contain a quote and we will not extract the whole
     * url. However, given these are coming from websites providing favicon-like images, it's not likely a quote will
     * appear and since these urls are only being used to compare against one another (as imageURLs in Highlight items),
     * a partial URL may actually have the same behavior: good enough for me!
     */
    private static final Pattern FAST_IMAGE_URL_PATTERN = Pattern.compile("\"image_url\":\"([^\"]+)\"");

    // A pattern matching a json object containing the key "description_length" and extracting the value: this
    // regex should perfectly match values in json without whitespace.
    private static final Pattern FAST_DESCRIPTION_LENGTH_PATTERN = Pattern.compile("\"description_length\":([0-9]+)");

    private final String title;
    private final String url;
    private final Utils.HighlightSource source;

    private long historyId;

    private @Nullable Metadata metadata; // lazily-loaded.
    private @Nullable final String metadataJSON;
    private @Nullable String fastImageURL;
    private int fastDescriptionLength;

    private @Nullable Boolean isPinned;
    private @Nullable Boolean isBookmarked;

    public static Highlight fromCursor(final Cursor cursor, final HighlightCandidateCursorIndices cursorIndices) {
        return new Highlight(cursor, cursorIndices);
    }

    private Highlight(final Cursor cursor, final HighlightCandidateCursorIndices cursorIndices) {
        title = cursor.getString(cursorIndices.titleColumnIndex);
        url = cursor.getString(cursorIndices.urlColumnIndex);
        source = Utils.highlightSource(cursor, cursorIndices);

        historyId = cursor.getLong(cursorIndices.historyIDColumnIndex);

        metadataJSON = cursor.getString(cursorIndices.metadataColumnIndex);
        fastImageURL = initFastImageURL(metadataJSON);
        fastDescriptionLength = initFastDescriptionLength(metadataJSON);

        updateState();
    }

    /** Gets a fast image URL. Full docs for this method at {@link #getFastImageURLForComparison()} & {@link #FAST_IMAGE_URL_PATTERN}. */
    @VisibleForTesting static @Nullable String initFastImageURL(final String metadataJSON) {
        return extractFirstGroupFromMetadataJSON(metadataJSON, FAST_IMAGE_URL_PATTERN);
    }

    /** Gets a fast description length. Full docs for this method at {@link #getFastDescriptionLength()} & {@link #FAST_DESCRIPTION_LENGTH_PATTERN}. */
    @VisibleForTesting static int initFastDescriptionLength(final String metadataJSON) {
        final String extractedStr = extractFirstGroupFromMetadataJSON(metadataJSON, FAST_DESCRIPTION_LENGTH_PATTERN);
        try {
            return !TextUtils.isEmpty(extractedStr) ? Integer.parseInt(extractedStr) : 0;
        } catch (final NumberFormatException e) { /* intentionally blank */ }
        return 0;
    }

    private static @Nullable String extractFirstGroupFromMetadataJSON(final String metadataJSON, final Pattern pattern) {
        if (metadataJSON == null) {
            return null;
        }

        final Matcher matcher = pattern.matcher(metadataJSON);
        return matcher.find() ? matcher.group(1) : null;
    }

    @Override
    public StreamRecyclerAdapter.RowItemType getRowItemType() {
        return StreamRecyclerAdapter.RowItemType.HIGHLIGHT_ITEM;
    }

    private void updateState() {
        // We can only be certain of bookmark state if an item is a bookmark item.
        // Otherwise, due to the underlying highlights query, we have to look up states when
        // menus are displayed.
        switch (source) {
            case BOOKMARKED:
                isBookmarked = true;
                isPinned = null;
                break;
            case VISITED:
                isBookmarked = null;
                isPinned = null;
                break;
            default:
                throw new IllegalArgumentException("Unknown source: " + source);
        }
    }

    public String getTitle() {
        return title;
    }

    public String getUrl() {
        return url;
    }

    /**
     * Retrieves the metadata associated with this highlight, lazily loaded.
     *
     * AVOID USING THIS FOR A LARGE NUMBER OF ITEMS, particularly in {@link HighlightsRanking#extractFeatures(Cursor)},
     * where we added lazy loading to improve performance.
     *
     * The JSONObject constructor inside Metadata takes a non-trivial amount of time to run so
     * we lazily load it. At the time of writing, in {@link HighlightsRanking#extractFeatures(Cursor)}, we get
     * 500 highlights before curating down to the ~5 shown items. For the non-displayed items, we use
     * the getFast* methods and, for the shown items, lazy-load the metadata since only then is it necessary.
     * These methods include:
     * - {@link #getFastDescriptionLength()}
     * - {@link #getFastImageURLForComparison()}
     * - {@link #hasFastImageURL()}
     */
    @NonNull
    public Metadata getMetadataSlow() {
        if (metadata == null) {
            metadata = new Metadata(metadataJSON);
        }
        return metadata;
    }

    /**
     * Returns the image URL associated with this Highlight.
     *
     * This implementation may be slow: see {@link #getMetadataSlow()}.
     *
     * @return the image URL, or the empty String if there is none.
     */
    @NonNull
    @Override
    public String getImageUrl() {
        final Metadata metadata = getMetadataSlow();
        final String imageUrl = metadata.getImageUrl();
        return imageUrl != null ? imageUrl : "";
    }

    /**
     * Returns the image url in the highlight's metadata. This value does not provide valid image url but is
     * consistent across invocations and can be used to compare against other Highlight's fast image urls.
     * See {@link #getMetadataSlow()} for a description of why we use this method.
     *
     * To get a valid image url (at a performance penalty), use {@link #getMetadataSlow()}
     * {@link #getMetadataSlow()} & {@link Metadata#getImageUrl()}.
     *
     * Note that this explanation is dependent on the implementation of {@link #initFastImageURL(String)}.
     *
     * @return the image url, or null if one could not be found.
     */
    public @Nullable String getFastImageURLForComparison() {
        return fastImageURL;
    }

    /**
     * Returns true if {@link #getFastImageURLForComparison()} has found an image url, false otherwise.
     * See that method for caveats.
     */
    public boolean hasFastImageURL() {
        return fastImageURL != null;
    }

    /**
     * Returns the description length in the highlight's metadata. This value is expected to correct in all cases.
     * See {@link #getMetadataSlow()} for why we use this method.
     *
     * This is a faster version of {@link #getMetadataSlow()} & {@link Metadata#getDescriptionLength()} because
     * retrieving the metadata in this way does a full json parse, which is slower.
     *
     * Note: this explanation is dependent on the implementation of {@link #initFastDescriptionLength(String)}.
     *
     * @return the given description length, or 0 if no description length was given
     */
    public int getFastDescriptionLength() {
        return fastDescriptionLength;
    }

    public Boolean isBookmarked() {
        return isBookmarked;
    }

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
        return source;
    }

    @Override
    public long getUniqueId() {
        return historyId;
    }

    // The Highlights cursor automatically notifies of data changes, so nothing needs to be done here.
    @Override
    public void onStateCommitted() {}}
