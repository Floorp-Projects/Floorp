/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.uploaders;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class PayloadTest {
    private Payload payload;
    private long byteLimit = 1024;
    private long recordLimit = 5;
    private Object lock = new Object();

    @Before
    public void setUp() throws Exception {
        payload = new Payload(lock, byteLimit, recordLimit);
    }

    @Test
    public void testByteLimits() {
        assertTrue(payload.canFit(0));

        // Should just fit
        assertTrue(payload.canFit(byteLimit - BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));

        // Can't fit a record due to payload overhead.
        assertFalse(payload.canFit(byteLimit));

        assertFalse(payload.canFit(byteLimit + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));
        assertFalse(payload.canFit(byteLimit * 1000));

        long recordDelta = byteLimit / 2;
        assertFalse(payload.addAndEstimateIfFull(recordDelta, new byte[0], null));

        // Record delta shouldn't fit due to payload overhead.
        assertFalse(payload.canFit(recordDelta));
    }

    @Test
    public void testCountLimits() {
        byte[] bytes = new byte[0];

        // Our record limit is 5, let's add 4.
        assertFalse(payload.addAndEstimateIfFull(1, bytes, null));
        assertFalse(payload.addAndEstimateIfFull(1, bytes, null));
        assertFalse(payload.addAndEstimateIfFull(1, bytes, null));
        assertFalse(payload.addAndEstimateIfFull(1, bytes, null));

        // 5th record still fits in
        assertTrue(payload.canFit(1));

        // Add the 5th record
        assertTrue(payload.addAndEstimateIfFull(1, bytes, null));

        // 6th record won't fit
        assertFalse(payload.canFit(1));
    }

    @Test
    public void testAdd() {
        assertTrue(payload.getByteCount() == 2 * BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT);
        assertTrue(payload.getRecordCount() == 0);
        assertTrue(payload.isEmpty());
        assertTrue(payload.getRecordsBuffer().isEmpty());
        assertTrue(payload.getRecordGuidsBuffer().isEmpty());

        try {
            payload.addAndEstimateIfFull(1024);
            fail("Simple add is not supported");
        } catch (UnsupportedOperationException e) {
            assertTrue(true);
        }

        byte[] recordBytes1 = new byte[100];
        assertFalse(payload.addAndEstimateIfFull(1, recordBytes1, "guid1"));

        assertTrue(payload.getRecordsBuffer().size() == 1);
        assertTrue(payload.getRecordGuidsBuffer().size() == 1);
        assertTrue(payload.getRecordGuidsBuffer().contains("guid1"));
        assertTrue(payload.getRecordsBuffer().contains(recordBytes1));

        assertTrue(payload.getByteCount() == (1 + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));
        assertTrue(payload.getRecordCount() == 1);

        assertFalse(payload.isEmpty());

        assertFalse(payload.addAndEstimateIfFull(1, recordBytes1, "guid2"));
        assertFalse(payload.addAndEstimateIfFull(1, recordBytes1, "guid3"));
        assertFalse(payload.addAndEstimateIfFull(1, recordBytes1, "guid4"));

        assertTrue(payload.getByteCount() == (4 + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));
        assertTrue(payload.getRecordCount() == 4);

        assertTrue(payload.addAndEstimateIfFull(1, recordBytes1, "guid5"));

        try {
            assertTrue(payload.addAndEstimateIfFull(1, recordBytes1, "guid6"));
            fail("Payload should not let us insert records that won't fit");
        } catch (IllegalStateException e) {
            assertTrue(true);
        }
    }

    @Test
    public void testReset() {
        assertTrue(payload.getByteCount() == 2 * BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT);
        assertTrue(payload.getRecordCount() == 0);
        assertTrue(payload.getRecordsBuffer().isEmpty());
        assertTrue(payload.getRecordGuidsBuffer().isEmpty());
        assertTrue(payload.isEmpty());

        // Shouldn't throw even if already empty
        payload.reset();
        assertTrue(payload.getByteCount() == 2 * BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT);
        assertTrue(payload.getRecordCount() == 0);
        assertTrue(payload.getRecordsBuffer().isEmpty());
        assertTrue(payload.getRecordGuidsBuffer().isEmpty());
        assertTrue(payload.isEmpty());

        byte[] recordBytes1 = new byte[100];
        assertFalse(payload.addAndEstimateIfFull(1, recordBytes1, "guid1"));
        assertFalse(payload.isEmpty());
        payload.reset();

        assertTrue(payload.getByteCount() == 2 * BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT);
        assertTrue(payload.getRecordCount() == 0);
        assertTrue(payload.getRecordsBuffer().isEmpty());
        assertTrue(payload.getRecordGuidsBuffer().isEmpty());
        assertTrue(payload.isEmpty());
    }
}