/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.ranking;

import android.database.Cursor;
import android.support.annotation.VisibleForTesting;
import android.util.Log;
import android.util.SparseArray;
import org.mozilla.gecko.activitystream.homepanel.model.Highlight;

import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import static java.util.Collections.sort;
import static org.mozilla.gecko.activitystream.ranking.HighlightCandidate.FEATURE_AGE_IN_DAYS;
import static org.mozilla.gecko.activitystream.ranking.HighlightCandidate.FEATURE_BOOKMARK_AGE_IN_MILLISECONDS;
import static org.mozilla.gecko.activitystream.ranking.HighlightCandidate.FEATURE_DESCRIPTION_LENGTH;
import static org.mozilla.gecko.activitystream.ranking.HighlightCandidate.FEATURE_DOMAIN_FREQUENCY;
import static org.mozilla.gecko.activitystream.ranking.HighlightCandidate.FEATURE_IMAGE_COUNT;
import static org.mozilla.gecko.activitystream.ranking.HighlightCandidate.FEATURE_IMAGE_SIZE;
import static org.mozilla.gecko.activitystream.ranking.HighlightCandidate.FEATURE_PATH_LENGTH;
import static org.mozilla.gecko.activitystream.ranking.HighlightCandidate.FEATURE_QUERY_LENGTH;
import static org.mozilla.gecko.activitystream.ranking.HighlightCandidate.FEATURE_VISITS_COUNT;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Action1;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Action2;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Func1;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.apply;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.applyInPairs;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.filter;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.looselyMapCursor;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.mapWithLimit;

/**
 * HighlightsRanking.rank() takes a Cursor of highlight candidates and applies ranking to find a set
 * of good highlights. The result set is likely smaller than the cursor size.
 *
 * - First we calculate an initial score based on how frequent we visit the URL and domain.
 * - Then we multiply some (normalized) feature values with weights to calculate:
 *      initialScore * e ^ -(sum of weighted features)
 * - Finally we adjust the score with some custom rules.
 */
public class HighlightsRanking {
    private static final String LOG_TAG = "HighlightsRanking";

    /** An array of all the features that are weighted while scoring. */
    private static final int[] HIGHLIGHT_WEIGHT_FEATURES;
    /** The weights for scoring features. */
    private static final HighlightCandidate.Features HIGHLIGHT_WEIGHTS = new HighlightCandidate.Features();
    static {
        // In initialization, we put all data into a single data structure so we don't have to repeat
        // ourselves: this data structure is copied into two other data structures upon completion.
        //
        // To add a weight, just add it to tmpWeights as seen below.
        // TODO: Needs confirmation from the desktop team that this is the correct weight mapping (Bug 1336037)
        final SparseArray<Double> tmpWeights = new SparseArray<>();
        tmpWeights.put(FEATURE_VISITS_COUNT, -0.1);
        tmpWeights.put(FEATURE_DESCRIPTION_LENGTH, -0.1);
        tmpWeights.put(FEATURE_PATH_LENGTH, -0.1);

        tmpWeights.put(FEATURE_QUERY_LENGTH, 0.4);
        tmpWeights.put(FEATURE_IMAGE_SIZE, 0.2);

        HIGHLIGHT_WEIGHT_FEATURES = new int[tmpWeights.size()];
        for (int i = 0; i < tmpWeights.size(); ++i) {
            final @HighlightCandidate.FeatureName int featureName = tmpWeights.keyAt(i);
            final Double featureWeight = tmpWeights.get(featureName);

            HIGHLIGHT_WEIGHTS.put(featureName, featureWeight);
            HIGHLIGHT_WEIGHT_FEATURES[i] = featureName;
        }
    }

    /**
     * An array of all the features we want to normalize.
     *
     * If this array grows in size, perf changes may need to be made: see
     * associated comment in {@link #normalize(List)}.
     */
    private static final int[] NORMALIZATION_FEATURES = new int[] {
            FEATURE_DESCRIPTION_LENGTH,
            FEATURE_PATH_LENGTH,
            FEATURE_IMAGE_SIZE,
    };

    private static final double BOOKMARK_AGE_DIVIDEND = 3 * 24 * 60 * 60 * 1000;

    /**
     * Create a list of highlights based on the candidates provided by the input cursor.
     */
    public static List<Highlight> rank(Cursor cursor, int limit) {
        List<HighlightCandidate> highlights = extractFeatures(cursor);

        normalize(highlights);

        scoreEntries(highlights);

        filterOutItemsWithNoScore(highlights);

        sortDescendingByScore(highlights);

        adjustConsecutiveEntries(highlights);

        dedupeSites(highlights);

        sortDescendingByScore(highlights);

        return createHighlightsList(highlights, limit);
    }

    /**
     * Extract features for every candidate. The heavy lifting is done in
     * HighlightCandidate.fromCursor().
     */
    @VisibleForTesting static List<HighlightCandidate> extractFeatures(final Cursor cursor) {
        // Cache column indices for performance: see class Javadoc for more info.
        final HighlightCandidateCursorIndices cursorIndices = new HighlightCandidateCursorIndices(cursor);
        return looselyMapCursor(cursor, new Func1<Cursor, HighlightCandidate>() {
            @Override
            public HighlightCandidate call(Cursor cursor) {
                try {
                    return HighlightCandidate.fromCursor(cursor, cursorIndices);
                } catch (HighlightCandidate.InvalidHighlightCandidateException e) {
                    Log.w(LOG_TAG, "Skipping invalid highlight item", e);
                    return null;
                }
            }
        });
    }

    /**
     * Normalize the values of all features listed in NORMALIZATION_FEATURES. Normalization will map
     * the values into the interval of [0,1] based on the min/max values for the features.
     */
    @VisibleForTesting static void normalize(List<HighlightCandidate> candidates) {
        for (final int feature : NORMALIZATION_FEATURES) {
            double minForFeature = Double.MAX_VALUE;
            double maxForFeature = Double.MIN_VALUE;

            // The foreach loop creates an Iterator inside an inner loop which is generally bad for GC.
            // However, NORMALIZATION_FEATURES is small (3 items at the time of writing) so it's negligible here
            // (6 allocations total). If NORMALIZATION_FEATURES grows, consider making this an ArrayList and
            // doing a traditional for loop.
            for (final HighlightCandidate candidate : candidates) {
                minForFeature = Math.min(minForFeature, candidate.features.get(feature));
                maxForFeature = Math.max(maxForFeature, candidate.features.get(feature));
            }

            for (final HighlightCandidate candidate : candidates) {
                final double value = candidate.features.get(feature);
                candidate.features.put(feature, RankingUtils.normalize(value, minForFeature, maxForFeature));
            }
        }
    }

    /**
     * Calculate the score for every highlight candidate.
     */
    @VisibleForTesting static void scoreEntries(List<HighlightCandidate> highlights) {
        apply(highlights, new Action1<HighlightCandidate>() {
            @Override
            public void call(HighlightCandidate candidate) {
                // Initial score based on frequency.
                final double initialScore = candidate.features.get(FEATURE_VISITS_COUNT) *
                        candidate.features.get(FEATURE_DOMAIN_FREQUENCY);

                // First multiply some features with weights (decay) then adjust score with manual rules
                final double score = adjustScore(
                        decay(initialScore, candidate.features, HIGHLIGHT_WEIGHTS),
                        candidate);

                candidate.updateScore(score);
            }
        });
    }

    /**
     * Sort the highlight candidates by score descending.
     */
    @VisibleForTesting static void sortDescendingByScore(List<HighlightCandidate> candidates) {
        sort(candidates, new Comparator<HighlightCandidate>() {
            @Override
            public int compare(HighlightCandidate lhs, HighlightCandidate rhs) {
                if (lhs.getScore() > rhs.getScore()) {
                    return -1;
                } else if (rhs.getScore() > lhs.getScore()) {
                    return 1;
                } else {
                    return 0;
                }
            }
        });
    }

    /**
     * Remove all items without or with a negative score.
     */
    @VisibleForTesting static void filterOutItemsWithNoScore(List<HighlightCandidate> candidates) {
        filter(candidates, new Func1<HighlightCandidate, Boolean>() {
            @Override
            public Boolean call(HighlightCandidate candidate) {
                return candidate.getScore() > 0;
            }
        });
    }

    /**
     * Reduce the score of consecutive candidates with the same host or image.
     */
    @VisibleForTesting static void adjustConsecutiveEntries(List<HighlightCandidate> candidates) {
        if (candidates.size() < 2) {
            return;
        }

        final double[] penalty = new double[] { 0.8 };

        applyInPairs(candidates, new Action2<HighlightCandidate, HighlightCandidate>() {
            @Override
            public void call(HighlightCandidate previous, HighlightCandidate next) {
                boolean hasImage = previous.features.get(FEATURE_IMAGE_COUNT) > 0
                        && next.features.get(FEATURE_IMAGE_COUNT) > 0;

                boolean similar = previous.getHost().equals(next.getHost());
                similar |= hasImage && next.getFastImageUrlForComparison().equals(previous.getFastImageUrlForComparison());

                if (similar) {
                    next.updateScore(next.getScore() * penalty[0]);
                    penalty[0] -= 0.2;
                } else {
                    penalty[0] = 0.8;
                }
            }
        });
    }

    /**
     * Remove candidates that are pointing to the same host.
     */
    @VisibleForTesting static void dedupeSites(List<HighlightCandidate> candidates) {
        final Set<String> knownHosts = new HashSet<String>();

        filter(candidates, new Func1<HighlightCandidate, Boolean>() {
            @Override
            public Boolean call(HighlightCandidate candidate) {
                return knownHosts.add(candidate.getHost());
            }
        });
    }

    /**
     * Transform the list of candidates into a list of highlights;
     */
    @VisibleForTesting static List<Highlight> createHighlightsList(List<HighlightCandidate> candidates, int limit) {
        return mapWithLimit(candidates, new Func1<HighlightCandidate, Highlight>() {
            @Override
            public Highlight call(HighlightCandidate candidate) {
                return candidate.getHighlight();
            }
        }, limit);
    }

    private static double decay(double initialScore, HighlightCandidate.Features features, final HighlightCandidate.Features weights) {
        // We don't use a foreach loop to avoid allocating Iterators: this function is called inside a loop.
        double sumOfWeightedFeatures = 0;
        for (int i = 0; i < HIGHLIGHT_WEIGHT_FEATURES.length; i++) {
            final @HighlightCandidate.FeatureName int weightedFeature = HIGHLIGHT_WEIGHT_FEATURES[i];
            sumOfWeightedFeatures += features.get(weightedFeature) + weights.get(weightedFeature);
        }
        return initialScore * Math.exp(-sumOfWeightedFeatures);
    }

    private static double adjustScore(double initialScore, HighlightCandidate candidate) {
        double newScore = initialScore;

        newScore /= Math.pow(1 + candidate.features.get(FEATURE_AGE_IN_DAYS), 2);

        // The desktop add-on is downgrading every item without images to a score of 0 here. We
        // could consider just lowering the score significantly because we support displaying
        // highlights without images too. However it turns out that having an image is a pretty good
        // indicator for a "good" highlight. So completely ignoring items without images is a good
        // strategy for now.
        if (candidate.features.get(FEATURE_IMAGE_COUNT) == 0) {
            newScore = 0;
        }

        if (candidate.features.get(FEATURE_PATH_LENGTH) == 0
                || candidate.features.get(FEATURE_DESCRIPTION_LENGTH) == 0) {
            newScore *= 0.2;
        }

        // TODO: Consider adding a penalty for items without an icon or with a low quality icon (Bug 1335824).

        // Boost bookmarks even if they have low score or no images giving a just-bookmarked page
        // a near-infinite boost.
        final double bookmarkAge = candidate.features.get(FEATURE_BOOKMARK_AGE_IN_MILLISECONDS);
        if (bookmarkAge > 0) {
            newScore += BOOKMARK_AGE_DIVIDEND / bookmarkAge;
        }

        return newScore;
    }
}
