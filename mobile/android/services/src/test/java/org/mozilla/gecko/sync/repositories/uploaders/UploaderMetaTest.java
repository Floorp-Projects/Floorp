/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.uploaders;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class UploaderMetaTest {
    private UploaderMeta uploaderMeta;
    private long byteLimit = 1024;
    private long recordLimit = 5;
    private Object lock = new Object();

    @Before
    public void setUp() throws Exception {
        uploaderMeta = new UploaderMeta(lock, byteLimit, recordLimit);
    }

    @Test
    public void testByteLimits() {
        assertTrue(uploaderMeta.canFit(0));

        // Should just fit
        assertTrue(uploaderMeta.canFit(byteLimit - BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));

        // Can't fit a record due to payload overhead.
        assertFalse(uploaderMeta.canFit(byteLimit));

        assertFalse(uploaderMeta.canFit(byteLimit + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));
        assertFalse(uploaderMeta.canFit(byteLimit * 1000));

        long recordDelta = byteLimit / 2;
        assertFalse(uploaderMeta.addAndEstimateIfFull(recordDelta));

        // Record delta shouldn't fit due to payload overhead.
        assertFalse(uploaderMeta.canFit(recordDelta));
    }

    @Test
    public void testCountLimits() {
        // Our record limit is 5, let's add 4.
        assertFalse(uploaderMeta.addAndEstimateIfFull(1));
        assertFalse(uploaderMeta.addAndEstimateIfFull(1));
        assertFalse(uploaderMeta.addAndEstimateIfFull(1));
        assertFalse(uploaderMeta.addAndEstimateIfFull(1));

        // 5th record still fits in
        assertTrue(uploaderMeta.canFit(1));

        // Add the 5th record
        assertTrue(uploaderMeta.addAndEstimateIfFull(1));

        // 6th record won't fit
        assertFalse(uploaderMeta.canFit(1));
    }

    @Test
    public void testNeedCommit() {
        assertFalse(uploaderMeta.needToCommit());

        assertFalse(uploaderMeta.addAndEstimateIfFull(1));

        assertTrue(uploaderMeta.needToCommit());

        assertFalse(uploaderMeta.addAndEstimateIfFull(1));
        assertFalse(uploaderMeta.addAndEstimateIfFull(1));
        assertFalse(uploaderMeta.addAndEstimateIfFull(1));

        assertTrue(uploaderMeta.needToCommit());
    }

    @Test
    public void testAdd() {
        // Ensure we account for payload overhead twice when the batch is empty.
        // Payload overhead is either RECORDS_START or RECORDS_END, and for an empty payload
        // we need both.
        assertTrue(uploaderMeta.getByteCount() == 2 * BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT);
        assertTrue(uploaderMeta.getRecordCount() == 0);

        assertFalse(uploaderMeta.addAndEstimateIfFull(1));

        assertTrue(uploaderMeta.getByteCount() == (1 + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));
        assertTrue(uploaderMeta.getRecordCount() == 1);

        assertFalse(uploaderMeta.addAndEstimateIfFull(1));
        assertFalse(uploaderMeta.addAndEstimateIfFull(1));
        assertFalse(uploaderMeta.addAndEstimateIfFull(1));

        assertTrue(uploaderMeta.getByteCount() == (4 + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));
        assertTrue(uploaderMeta.getRecordCount() == 4);

        assertTrue(uploaderMeta.addAndEstimateIfFull(1));

        try {
            assertTrue(uploaderMeta.addAndEstimateIfFull(1));
            fail("BatchMeta should not let us insert records that won't fit");
        } catch (IllegalStateException e) {
            assertTrue(true);
        }
    }
}