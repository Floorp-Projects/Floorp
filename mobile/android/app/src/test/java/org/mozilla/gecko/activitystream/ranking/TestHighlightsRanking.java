/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.activitystream.ranking;

import android.net.Uri;
import junit.framework.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

@RunWith(TestRunner.class)
public class TestHighlightsRanking {
    @Test
    public void testNormalization() {
        final HighlightCandidate candidate1 = createCandidateWithNormalizationFeatures(30d, 0d, 100d);
        final HighlightCandidate candidate2 = createCandidateWithNormalizationFeatures(50d, 10d, 0d);
        final HighlightCandidate candidate3 = createCandidateWithNormalizationFeatures(15d, 75d, 10000d);
        final HighlightCandidate candidate4 = createCandidateWithNormalizationFeatures(75d, 100d, 250d);
        final HighlightCandidate candidate5 = createCandidateWithNormalizationFeatures(115d, 20d, 2000d);

        List<HighlightCandidate> candidates = Arrays.asList(candidate1, candidate2, candidate3, candidate4, candidate5);

        HighlightsRanking.normalize(candidates);

        Assert.assertEquals(0.15, candidate1.features.get(HighlightCandidate.FEATURE_DESCRIPTION_LENGTH), 1e-6);
        Assert.assertEquals(0.35, candidate2.features.get(HighlightCandidate.FEATURE_DESCRIPTION_LENGTH), 1e-6);
        Assert.assertEquals(0, candidate3.features.get(HighlightCandidate.FEATURE_DESCRIPTION_LENGTH), 1e-6);
        Assert.assertEquals(0.6, candidate4.features.get(HighlightCandidate.FEATURE_DESCRIPTION_LENGTH), 1e-6);
        Assert.assertEquals(1.0, candidate5.features.get(HighlightCandidate.FEATURE_DESCRIPTION_LENGTH), 1e-6);

        Assert.assertEquals(0, candidate1.features.get(HighlightCandidate.FEATURE_PATH_LENGTH), 1e-6);
        Assert.assertEquals(0.1, candidate2.features.get(HighlightCandidate.FEATURE_PATH_LENGTH), 1e-6);
        Assert.assertEquals(0.75, candidate3.features.get(HighlightCandidate.FEATURE_PATH_LENGTH), 1e-6);
        Assert.assertEquals(1, candidate4.features.get(HighlightCandidate.FEATURE_PATH_LENGTH), 1e-6);
        Assert.assertEquals(0.2, candidate5.features.get(HighlightCandidate.FEATURE_PATH_LENGTH), 1e-6);

        Assert.assertEquals(0.01, candidate1.features.get(HighlightCandidate.FEATURE_IMAGE_SIZE), 1e-6);
        Assert.assertEquals(0, candidate2.features.get(HighlightCandidate.FEATURE_IMAGE_SIZE), 1e-6);
        Assert.assertEquals(1.0, candidate3.features.get(HighlightCandidate.FEATURE_IMAGE_SIZE), 1e-6);
        Assert.assertEquals(0.025, candidate4.features.get(HighlightCandidate.FEATURE_IMAGE_SIZE), 1e-6);
        Assert.assertEquals(0.2, candidate5.features.get(HighlightCandidate.FEATURE_IMAGE_SIZE), 1e-6);
    }

    @Test
    public void testSortingByScore() {
        List<HighlightCandidate> candidates = Arrays.asList(
                createCandidateWithScore(1000),
                createCandidateWithScore(-10),
                createCandidateWithScore(0),
                createCandidateWithScore(25),
                createCandidateWithScore(0.75),
                createCandidateWithScore(0.1),
                createCandidateWithScore(2500));

        HighlightsRanking.sortDescendingByScore(candidates);

        Assert.assertEquals(2500, candidates.get(0).getScore(), 1e-6);
        Assert.assertEquals(1000, candidates.get(1).getScore(), 1e-6);
        Assert.assertEquals(25, candidates.get(2).getScore(), 1e-6);
        Assert.assertEquals(0.75, candidates.get(3).getScore(), 1e-6);
        Assert.assertEquals(0.1, candidates.get(4).getScore(), 1e-6);
        Assert.assertEquals(0, candidates.get(5).getScore(), 1e-6);
        Assert.assertEquals(-10, candidates.get(6).getScore(), 1e-6);
    }

    private static HighlightCandidate createCandidateWithNormalizationFeatures(double descriptionLength,
            double pathLength, double imageSize) {

        HighlightCandidate candidate = new HighlightCandidate();
        candidate.features.put(HighlightCandidate.FEATURE_DESCRIPTION_LENGTH, descriptionLength);
        candidate.features.put(HighlightCandidate.FEATURE_PATH_LENGTH, pathLength);
        candidate.features.put(HighlightCandidate.FEATURE_IMAGE_SIZE, imageSize);
        return candidate;
    }

    private static HighlightCandidate createCandidateWithScore(double score) {
        HighlightCandidate candidate = new HighlightCandidate();
        candidate.updateScore(score);
        return candidate;
    }

    @Test
    public void testDedupeSitesRemoveWWWDupeSiteWithNoPath() {
        final HighlightCandidate maintainedCandidate = mockCandidate("http://feedly.com");
        final List<HighlightCandidate> candidates = new ArrayList<>(Arrays.asList(
                mockCandidate("http://www.feedly.com"),
                maintainedCandidate
        ));

        HighlightsRanking.dedupeSites(candidates);

        Assert.assertEquals("Expected www. site to be removed", 1, candidates.size());
        Assert.assertEquals("Expected remaining candidate to be non-www candidate.", maintainedCandidate, candidates.get(0));
    }

    @Test
    public void testDedupeSitesRemoveWWWDupeSiteWithSamePath() {
        final HighlightCandidate maintainedCandidate = mockCandidate("http://feedly.com/feed/whatever");
        final List<HighlightCandidate> candidates = new ArrayList<>(Arrays.asList(
                mockCandidate("http://www.feedly.com/feed/whatever"),
                maintainedCandidate
        ));

        HighlightsRanking.dedupeSites(candidates);

        Assert.assertEquals("Expected www. site to be removed", 1, candidates.size());
        Assert.assertEquals("Expected remaining candidate to be non-www candidate.", maintainedCandidate, candidates.get(0));
    }

    @Test
    public void testDedupeSitesKeepWWWDupeSiteWithDifferentPath() {
        final List<HighlightCandidate> candidates = new ArrayList<>(Arrays.asList(
                mockCandidate("http://www.feedly.com/home"),
                mockCandidate("http://feedly.com/feed")
        ));

        HighlightsRanking.dedupeSites(candidates);

        Assert.assertEquals("Expected no candidates to be removed", 2, candidates.size());
    }

    private HighlightCandidate mockCandidate(final String urlStr) {
        final String host = Uri.parse(urlStr).getHost();

        final HighlightCandidate mock = mock(HighlightCandidate.class);
        when(mock.getUrl()).thenReturn(urlStr);
        when(mock.getHost()).thenReturn(host);
        return mock;
    }
}
