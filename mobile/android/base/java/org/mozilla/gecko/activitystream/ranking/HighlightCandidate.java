/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.ranking;

import android.database.Cursor;
import android.net.Uri;
import android.support.annotation.Nullable;
import android.support.annotation.StringDef;
import android.support.annotation.VisibleForTesting;

import org.mozilla.gecko.activitystream.ranking.RankingUtils.Func1;
import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;

import java.util.HashMap;
import java.util.Map;

/**
 * A highlight candidate (Highlight object + features). Ranking will determine whether this is an
 * actual highlight.
 */
/* package-private */ class HighlightCandidate {
    /* package-private */ static final String FEATURE_AGE_IN_DAYS = "ageInDays";
    /* package-private */ static final String FEATURE_IMAGE_COUNT = "imageCount";
    /* package-private */ static final String FEATURE_DOMAIN_FREQUENCY = "domainFrequency";
    /* package-private */ static final String FEATURE_VISITS_COUNT = "visitsCount";
    /* package-private */ static final String FEATURE_BOOKMARK_AGE_IN_MILLISECONDS = "bookmarkageInDays";
    /* package-private */ static final String FEATURE_DESCRIPTION_LENGTH = "descriptionLength";
    /* package-private */ static final String FEATURE_PATH_LENGTH = "pathLength";
    /* package-private */ static final String FEATURE_QUERY_LENGTH = "queryLength";
    /* package-private */ static final String FEATURE_IMAGE_SIZE = "imageSize";

    @StringDef({FEATURE_AGE_IN_DAYS, FEATURE_IMAGE_COUNT, FEATURE_DOMAIN_FREQUENCY, FEATURE_VISITS_COUNT,
            FEATURE_BOOKMARK_AGE_IN_MILLISECONDS, FEATURE_DESCRIPTION_LENGTH, FEATURE_PATH_LENGTH,
            FEATURE_QUERY_LENGTH, FEATURE_IMAGE_SIZE})
    public @interface Feature {}

    @VisibleForTesting final Map<String, Double> features;
    private Highlight highlight;
    private @Nullable String imageUrl;
    private String host;
    private double score;

    public static HighlightCandidate fromCursor(Cursor cursor) {
        final HighlightCandidate candidate = new HighlightCandidate();

        extractHighlight(candidate, cursor);
        extractFeatures(candidate, cursor);

        return candidate;
    }

    /**
     * Extract highlight object from cursor.
     */
    private static void extractHighlight(HighlightCandidate candidate, Cursor cursor) {
        candidate.highlight = Highlight.fromCursor(cursor);
    }

    /**
     * Extract and assign features that will be used for ranking.
     */
    private static void extractFeatures(HighlightCandidate candidate, Cursor cursor) {
        candidate.features.put(
                FEATURE_AGE_IN_DAYS,
                (System.currentTimeMillis() - cursor.getDouble(cursor.getColumnIndexOrThrow(BrowserContract.History.DATE_LAST_VISITED)))
                        / (1000 * 3600 * 24));

        candidate.features.put(
                FEATURE_VISITS_COUNT,
                cursor.getDouble(cursor.getColumnIndexOrThrow(BrowserContract.History.VISITS)));

        // Until we can determine those numbers we assume this domain has only been visited once
        // and the cursor returned all database entries.
        // TODO: Calculate values based using domain hash field (bug 1335817)
        final int occurrences = 1; // Number of times host shows up in history (Bug 1319485)
        final int domainCountSize = cursor.getCount(); // Number of domains visited (Bug 1319485)

        candidate.features.put(
                FEATURE_DOMAIN_FREQUENCY,
                Math.log(1 + domainCountSize / occurrences));

        candidate.imageUrl = candidate.highlight.getMetadata().getImageUrl();

        // The desktop add-on used the number of images returned form Embed.ly here. This is not the
        // same as total images on the page (think of small icons or the famous spacer.gif). So for
        // now this value will only be 1 or 0 depending on whether we found a good image. The desktop
        // team will face the same issue when switching from Embed.ly to the metadata-page-parser.
        // At this point we can try to find a fathom rule for determining a good value here.
        candidate.features.put(
                FEATURE_IMAGE_COUNT,
                candidate.highlight.getMetadata().hasImageUrl() ? 1d : 0d);

        // TODO: We do not store the size of the main image (Bug 1335819).
        // The desktop add-on calculates: Math.min(image.width * image.height, 1e5)
        candidate.features.put(
                FEATURE_IMAGE_SIZE,
                candidate.highlight.getMetadata().hasImageUrl() ? 1d : 0d
        );

        // Historical note: before Bug 1335198, this value was not really the time at which the
        // bookmark was created by the user. Especially synchronized bookmarks could have a recent
        // value but have been bookmarked a long time ago.
        // Current behaviour: synchronized clients will, over time, converge DATE_CREATED field
        // to the real creation date, or the earliest one mentioned in the clients constellation.
        // We are sourcing highlights from the recent visited history - so in order to
        // show up this bookmark need to have been visited recently too.
        final int bookmarkDateColumnIndex = cursor.getColumnIndexOrThrow(BrowserContract.Bookmarks.DATE_CREATED);
        if (cursor.isNull(bookmarkDateColumnIndex)) {
            candidate.features.put(
                    FEATURE_BOOKMARK_AGE_IN_MILLISECONDS,
                    0d);
        } else {
            candidate.features.put(
                    FEATURE_BOOKMARK_AGE_IN_MILLISECONDS,
                    Math.max(1, System.currentTimeMillis() - cursor.getDouble(bookmarkDateColumnIndex)));
        }

        candidate.features.put(
                FEATURE_DESCRIPTION_LENGTH,
                (double) candidate.highlight.getMetadata().getDescriptionLength());

        final Uri uri = Uri.parse(candidate.highlight.getUrl());

        candidate.host = uri.getHost();

        candidate.features.put(
                FEATURE_PATH_LENGTH,
                (double) uri.getPathSegments().size());

        candidate.features.put(
                FEATURE_QUERY_LENGTH,
                (double) uri.getQueryParameterNames().size());
    }

    @VisibleForTesting HighlightCandidate() {
        features = new HashMap<>();
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

    @Nullable
    /* package-private */ String getImageUrl() {
        return imageUrl;
    }

    /* package-private */ Highlight getHighlight() {
        return highlight;
    }

    /* package-private */ double getFeatureValue(@Feature String feature) {
        if (!features.containsKey(feature)) {
            throw new IllegalStateException("No value for feature " + feature);
        }

        return features.get(feature);
    }

    /* package-private */ void setFeatureValue(@Feature String feature, double value) {
        features.put(feature, value);
    }

    /* package-private */ Map<String, Double> getFilteredFeatures(Func1<String, Boolean> filter) {
        Map<String, Double> filteredFeatures = new HashMap<>();

        for (Map.Entry<String, Double> entry : features.entrySet()) {
            if (filter.call(entry.getKey())) {
                filteredFeatures.put(entry.getKey(), entry.getValue());
            }
        }

        return filteredFeatures;
    }
}
