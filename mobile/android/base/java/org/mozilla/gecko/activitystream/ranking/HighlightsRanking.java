/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.activitystream.ranking;

import android.database.Cursor;
import android.support.annotation.VisibleForTesting;
import android.util.Log;

import org.mozilla.gecko.activitystream.homepanel.model.Highlight;

import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import static java.util.Collections.sort;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Action1;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Action2;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Func1;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.Func2;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.apply;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.apply2D;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.applyInPairs;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.filter;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.looselyMapCursor;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.mapWithLimit;
import static org.mozilla.gecko.activitystream.ranking.RankingUtils.reduce;

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

    private static final Map<String, Double> HIGHLIGHT_WEIGHTS = new HashMap<>();
    static {
        // TODO: Needs confirmation from the desktop team that this is the correct weight mapping (Bug 1336037)
        HIGHLIGHT_WEIGHTS.put(HighlightCandidate.FEATURE_VISITS_COUNT, -0.1);
        HIGHLIGHT_WEIGHTS.put(HighlightCandidate.FEATURE_DESCRIPTION_LENGTH, -0.1);
        HIGHLIGHT_WEIGHTS.put(HighlightCandidate.FEATURE_PATH_LENGTH, -0.1);

        HIGHLIGHT_WEIGHTS.put(HighlightCandidate.FEATURE_QUERY_LENGTH, 0.4);
        HIGHLIGHT_WEIGHTS.put(HighlightCandidate.FEATURE_IMAGE_SIZE, 0.2);
    }

    private static final List<String> NORMALIZATION_FEATURES = Arrays.asList(
            HighlightCandidate.FEATURE_DESCRIPTION_LENGTH,
            HighlightCandidate.FEATURE_PATH_LENGTH,
            HighlightCandidate.FEATURE_IMAGE_SIZE);

    private static final List<String> ADJUSTMENT_FEATURES = Arrays.asList(
            HighlightCandidate.FEATURE_BOOKMARK_AGE_IN_MILLISECONDS,
            HighlightCandidate.FEATURE_IMAGE_COUNT,
            HighlightCandidate.FEATURE_AGE_IN_DAYS,
            HighlightCandidate.FEATURE_DOMAIN_FREQUENCY
    );

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
    @VisibleForTesting static List<HighlightCandidate> extractFeatures(Cursor cursor) {
        return looselyMapCursor(cursor, new Func1<Cursor, HighlightCandidate>() {
            @Override
            public HighlightCandidate call(Cursor cursor) {
                try {
                    return HighlightCandidate.fromCursor(cursor);
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
        final HashMap<String, double[]> minMaxValues = new HashMap<>(); // 0 = min, 1 = max

        // First update the min/max values for all features
        apply2D(candidates, NORMALIZATION_FEATURES, new Action2<HighlightCandidate, String>() {
            @Override
            public void call(HighlightCandidate candidate, String feature) {
                double[] minMaxForFeature = minMaxValues.get(feature);

                if (minMaxForFeature == null) {
                    minMaxForFeature = new double[] { Double.MAX_VALUE, Double.MIN_VALUE };
                    minMaxValues.put(feature, minMaxForFeature);
                }

                minMaxForFeature[0] = Math.min(minMaxForFeature[0], candidate.getFeatureValue(feature));
                minMaxForFeature[1] = Math.max(minMaxForFeature[1], candidate.getFeatureValue(feature));
            }
        });

        // Then normalizeFeatureValue the features with the min max values into (0, 1) range.
        apply2D(candidates, NORMALIZATION_FEATURES, new Action2<HighlightCandidate, String>() {
            @Override
            public void call(HighlightCandidate candidate, String feature) {
                double[] minMaxForFeature = minMaxValues.get(feature);
                double value = candidate.getFeatureValue(feature);

                candidate.setFeatureValue(feature,
                        RankingUtils.normalize(value, minMaxForFeature[0], minMaxForFeature[1]));
            }
        });
    }

    /**
     * Calculate the score for every highlight candidate.
     */
    @VisibleForTesting static void scoreEntries(List<HighlightCandidate> highlights) {
        apply(highlights, new Action1<HighlightCandidate>() {
            @Override
            public void call(HighlightCandidate candidate) {
                final Map<String, Double> featuresForWeighting = candidate.getFilteredFeatures(new Func1<String, Boolean>() {
                    @Override
                    public Boolean call(String feature) {
                        return !ADJUSTMENT_FEATURES.contains(feature);
                    }
                });

                // Initial score based on frequency.
                final double initialScore = candidate.getFeatureValue(HighlightCandidate.FEATURE_VISITS_COUNT)
                        * candidate.getFeatureValue(HighlightCandidate.FEATURE_DOMAIN_FREQUENCY);

                // First multiply some features with weights (decay) then adjust score with manual rules
                final double score = adjustScore(
                        decay(initialScore, featuresForWeighting, HIGHLIGHT_WEIGHTS),
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
                boolean hasImage = previous.getFeatureValue(HighlightCandidate.FEATURE_IMAGE_COUNT) > 0
                        && next.getFeatureValue(HighlightCandidate.FEATURE_IMAGE_COUNT) > 0;

                boolean similar = previous.getHost().equals(next.getHost());
                similar |= hasImage && next.getImageUrl().equals(previous.getImageUrl());

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

    private static double decay(double initialScore, Map<String, Double> features, final Map<String, Double> weights) {
        if (features.size() != weights.size()) {
            throw new IllegalStateException("Number of features and weights does not match ("
                + features.size() + " != " + weights.size());
        }

        double sumOfWeightedFeatures = reduce(features.entrySet(), new Func2<Map.Entry<String, Double>, Double, Double>() {
            @Override
            public Double call(Map.Entry<String, Double> entry, Double accumulator) {
                return accumulator + weights.get(entry.getKey()) * entry.getValue();
            }
        }, 0d);

        return initialScore * Math.exp(-sumOfWeightedFeatures);
    }

    private static double adjustScore(double initialScore, HighlightCandidate candidate) {
        double newScore = initialScore;

        newScore /= Math.pow(1 + candidate.getFeatureValue(HighlightCandidate.FEATURE_AGE_IN_DAYS), 2);

        // The desktop add-on is downgrading every item without images to a score of 0 here. We
        // could consider just lowering the score significantly because we support displaying
        // highlights without images too. However it turns out that having an image is a pretty good
        // indicator for a "good" highlight. So completely ignoring items without images is a good
        // strategy for now.
        if (candidate.getFeatureValue(HighlightCandidate.FEATURE_IMAGE_COUNT) == 0) {
            newScore = 0;
        }

        if (candidate.getFeatureValue(HighlightCandidate.FEATURE_PATH_LENGTH) == 0
                || candidate.getFeatureValue(HighlightCandidate.FEATURE_DESCRIPTION_LENGTH) == 0) {
            newScore *= 0.2;
        }

        // TODO: Consider adding a penalty for items without an icon or with a low quality icon (Bug 1335824).

        // Boost bookmarks even if they have low score or no images giving a just-bookmarked page
        // a near-infinite boost.
        double bookmarkAge = candidate.getFeatureValue(HighlightCandidate.FEATURE_BOOKMARK_AGE_IN_MILLISECONDS);
        if (bookmarkAge > 0) {
            newScore += BOOKMARK_AGE_DIVIDEND / bookmarkAge;
        }

        return newScore;
    }
}
