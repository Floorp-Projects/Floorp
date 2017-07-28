/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.ranking;

import android.database.Cursor;
import android.net.Uri;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.support.annotation.VisibleForTesting;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A highlight candidate (Highlight object + features). Ranking will determine whether this is an
 * actual highlight.
 */
/* package-private */ class HighlightCandidate {

    // Features we score over for Highlight results - see Features class for more details & usage.
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({FEATURE_AGE_IN_DAYS, FEATURE_BOOKMARK_AGE_IN_MILLISECONDS, FEATURE_DESCRIPTION_LENGTH,
            FEATURE_DOMAIN_FREQUENCY, FEATURE_IMAGE_COUNT, FEATURE_IMAGE_SIZE, FEATURE_PATH_LENGTH,
            FEATURE_QUERY_LENGTH, FEATURE_VISITS_COUNT})
    /* package-private */ @interface FeatureName {}

    // IF YOU ADD A FIELD, INCREMENT `FEATURE_COUNT`! For a perf boost, we use these ints to index into an array and
    // FEATURE_COUNT tracks the number of features we have and thus how big the array needs to be.
    private static final int FEATURE_COUNT = 9; // = the-greatest-feature-index + 1.
    /* package-private */ static final int FEATURE_AGE_IN_DAYS = 0;
    /* package-private */ static final int FEATURE_BOOKMARK_AGE_IN_MILLISECONDS = 1;
    /* package-private */ static final int FEATURE_DESCRIPTION_LENGTH = 2;
    /* package-private */ static final int FEATURE_DOMAIN_FREQUENCY = 3;
    /* package-private */ static final int FEATURE_IMAGE_COUNT = 4;
    /* package-private */ static final int FEATURE_IMAGE_SIZE = 5;
    /* package-private */ static final int FEATURE_PATH_LENGTH = 6;
    /* package-private */ static final int FEATURE_QUERY_LENGTH = 7;
    /* package-private */ static final int FEATURE_VISITS_COUNT = 8;

    /**
     * A data class for accessing Features values. It acts as a map from FeatureName -> value:
     * <pre>
     *   Features features = new Features();
     *   features.put(FEATURE_AGE_IN_DAYS, 30);
     *   double value = features.get(FEATURE_AGE_IN_DAYS);
     * </pre>
     *
     * This data is accessed frequently and needs to be performant. As such, the implementation is a little fragile
     * (e.g. we could increase type safety with enums and index into the backing array with Enum.ordinal(), but it
     * gets called enough that it's not worth the performance trade-off).
     */
    /* package-private */ static class Features {
        private final double[] values = new double[FEATURE_COUNT];

        Features() {}

        /* package-private */ double get(final @FeatureName int featureName) {
            return values[featureName];
        }

        /* package-private */ void put(final @FeatureName int featureName, final double value) {
            values[featureName] = value;
        }
    }

    @VisibleForTesting final Features features = new Features();
    private Highlight highlight;
    private @Nullable String imageUrl;
    private String host;
    private double score;

    public static HighlightCandidate fromCursor(final Cursor cursor, final HighlightCandidateCursorIndices cursorIndices)
            throws InvalidHighlightCandidateException {
        final HighlightCandidate candidate = new HighlightCandidate();

        extractHighlight(candidate, cursor, cursorIndices);
        extractFeatures(candidate, cursor, cursorIndices);

        return candidate;
    }

    /**
     * Extract highlight object from cursor.
     */
    private static void extractHighlight(final HighlightCandidate candidate, final Cursor cursor,
            final HighlightCandidateCursorIndices cursorIndices) {
        candidate.highlight = Highlight.fromCursor(cursor, cursorIndices);
    }

    /**
     * Extract and assign features that will be used for ranking.
     */
    private static void extractFeatures(final HighlightCandidate candidate, final Cursor cursor,
            final HighlightCandidateCursorIndices cursorIndices) throws InvalidHighlightCandidateException {
        candidate.features.put(
                FEATURE_AGE_IN_DAYS,
                (System.currentTimeMillis() - cursor.getDouble(cursorIndices.historyDateLastVisitedColumnIndex))
                        / (1000 * 3600 * 24));

        candidate.features.put(
                FEATURE_VISITS_COUNT,
                cursor.getDouble(cursorIndices.visitsColumnIndex));

        // Until we can determine those numbers we assume this domain has only been visited once
        // and the cursor returned all database entries.
        // TODO: Calculate values based using domain hash field (bug 1335817)
        final int occurrences = 1; // Number of times host shows up in history (Bug 1319485)
        final int domainCountSize = cursor.getCount(); // Number of domains visited (Bug 1319485)

        candidate.features.put(
                FEATURE_DOMAIN_FREQUENCY,
                Math.log(1 + domainCountSize / occurrences));

        candidate.imageUrl = candidate.highlight.getFastImageURLForComparison();

        // The desktop add-on used the number of images returned form Embed.ly here. This is not the
        // same as total images on the page (think of small icons or the famous spacer.gif). So for
        // now this value will only be 1 or 0 depending on whether we found a good image. The desktop
        // team will face the same issue when switching from Embed.ly to the metadata-page-parser.
        // At this point we can try to find a fathom rule for determining a good value here.
        candidate.features.put(
                FEATURE_IMAGE_COUNT,
                candidate.highlight.hasFastImageURL() ? 1d : 0d);

        // TODO: We do not store the size of the main image (Bug 1335819).
        // The desktop add-on calculates: Math.min(image.width * image.height, 1e5)
        candidate.features.put(
                FEATURE_IMAGE_SIZE,
                candidate.highlight.hasFastImageURL() ? 1d : 0d
        );

        // Historical note: before Bug 1335198, this value was not really the time at which the
        // bookmark was created by the user. Especially synchronized bookmarks could have a recent
        // value but have been bookmarked a long time ago.
        // Current behaviour: synchronized clients will, over time, converge DATE_CREATED field
        // to the real creation date, or the earliest one mentioned in the clients constellation.
        // We are sourcing highlights from the recent visited history - so in order to
        // show up this bookmark need to have been visited recently too.
        if (cursor.isNull(cursorIndices.bookmarkDateCreatedColumnIndex)) {
            candidate.features.put(
                    FEATURE_BOOKMARK_AGE_IN_MILLISECONDS,
                    0d);
        } else {
            candidate.features.put(
                    FEATURE_BOOKMARK_AGE_IN_MILLISECONDS,
                    Math.max(1, System.currentTimeMillis() - cursor.getDouble(cursorIndices.bookmarkDateCreatedColumnIndex)));
        }

        candidate.features.put(
                FEATURE_DESCRIPTION_LENGTH,
                (double) candidate.highlight.getFastDescriptionLength());

        final Uri uri = Uri.parse(candidate.highlight.getUrl());

        // We don't support opaque URIs (such as mailto:...), or URIs which do not have a valid host.
        // The latter might simply be URIs with invalid characters in them (such as underscore...).
        // See Bug 1363521 and Bug 1378967.
        if (!uri.isHierarchical() || uri.getHost() == null) {
            throw new InvalidHighlightCandidateException();
        }

        candidate.host = uri.getHost();

        candidate.features.put(
                FEATURE_PATH_LENGTH,
                (double) uri.getPathSegments().size());

        // Only hierarchical URIs support getQueryParameterNames.
        candidate.features.put(
                FEATURE_QUERY_LENGTH,
                (double) uri.getQueryParameterNames().size());
    }

    @VisibleForTesting HighlightCandidate() {
    }

    /* package-private */ double getScore() {
        return score;
    }

    /* package-private */ void updateScore(double score) {
        this.score = score;
    }

    /* package-private */ String getUrl() {
        return highlight.getUrl();
    }

    /* package-private */ String getHost() {
        return host;
    }

    /**
     * Gets an estimate of the actual image url that should only be used to compare against other return
     * values of this method. See {@link Highlight#getFastImageURLForComparison()} for more details.
     */
    @Nullable
    /* package-private */ String getFastImageUrlForComparison() {
        return imageUrl;
    }

    /* package-private */ Highlight getHighlight() {
        return highlight;
    }

    /* package-private */ static class InvalidHighlightCandidateException extends Exception {
        private static final long serialVersionUID = 949263104621445850L;
    }
}
