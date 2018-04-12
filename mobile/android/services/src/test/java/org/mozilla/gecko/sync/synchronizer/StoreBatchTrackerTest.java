/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.synchronizer;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.synchronizer.StoreBatchTracker.Batch;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class StoreBatchTrackerTest {
    private StoreBatchTracker tracker;

    @Before
    public void setUp() throws Exception {
        tracker = new StoreBatchTracker();
    }

    private void recordCounts(int attempted, int succeeded, int failed) {
        for (int i = 0; i < attempted; ++i) {
            tracker.onRecordStoreAttempted();
        }
        tracker.onRecordStoreSucceeded(succeeded);
        for (int i = 0; i < failed; ++i) {
            tracker.onRecordStoreFailed();
        }
    }

    @Test
    public void testSingleBatch() {
        {
            recordCounts(3, 3, 0);
            tracker.onBatchFinished();
            ArrayList<Batch> batches = tracker.getStoreBatches();
            assertEquals(batches.size(), 1);
            assertEquals(batches.get(0).failed, 0);
            assertEquals(batches.get(0).sent, 3);
            tracker.reset();
        }
        {
            recordCounts(3, 2, 1);
            // Don't bother calling onBatchFinished, to ensure we finish it automatically.
            ArrayList<Batch> batches = tracker.getStoreBatches();
            assertEquals(batches.size(), 1);
            assertEquals(batches.get(0).failed, 1);
            assertEquals(batches.get(0).sent, 3);
            tracker.reset();
        }
        {
            recordCounts(3, 0, 3);
            tracker.onBatchFinished();
            ArrayList<Batch> batches = tracker.getStoreBatches();
            assertEquals(batches.size(), 1);
            assertEquals(batches.get(0).failed, 3);
            assertEquals(batches.get(0).sent, 3);
            tracker.reset();
        }
    }

    @Test
    public void testBatchFail() {
        recordCounts(3, 3, 0);
        tracker.onBatchFailed();
        ArrayList<Batch> batches = tracker.getStoreBatches();
        assertEquals(batches.size(), 1);
        // The important thing is that there's a non-zero number of failed here.
        assertEquals(batches.get(0).failed, 1);
        assertEquals(batches.get(0).sent, 3);
    }

    @Test
    public void testMultipleBatches() {
        recordCounts(3, 3, 0);
        tracker.onBatchFinished();
        recordCounts(8, 8, 0);
        tracker.onBatchFinished();
        recordCounts(5, 5, 0);
        tracker.onBatchFinished();
        // Fake an empty "commit" POST.
        tracker.onBatchFinished();
        ArrayList<Batch> batches = tracker.getStoreBatches();

        assertEquals(batches.size(), 4);

        assertEquals(batches.get(0).failed, 0);
        assertEquals(batches.get(0).sent, 3);

        assertEquals(batches.get(1).failed, 0);
        assertEquals(batches.get(1).sent, 8);

        assertEquals(batches.get(2).failed, 0);
        assertEquals(batches.get(2).sent, 5);

        assertEquals(batches.get(3).failed, 0);
        assertEquals(batches.get(3).sent, 0);
    }

    @Test
    public void testDroppedStores() {
        recordCounts(3, 1, 0);
        tracker.onBatchFinished();
        ArrayList<Batch> batches = tracker.getStoreBatches();
        assertEquals(batches.size(), 1);
        assertEquals(batches.get(0).failed, 2);
        assertEquals(batches.get(0).sent, 3);
    }

    @Test
    public void testReset() {
        assertEquals(tracker.getStoreBatches().size(), 0);
        recordCounts(2, 1, 1);
        assertEquals(tracker.getStoreBatches().size(), 1);
        tracker.reset();
        assertEquals(tracker.getStoreBatches().size(), 0);
    }
}