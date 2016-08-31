/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.uploaders;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class BatchMetaTest {
    private BatchMeta batchMeta;
    private long byteLimit = 1024;
    private long recordLimit = 5;
    private Object lock = new Object();
    private Long collectionLastModified = 123L;

    @Before
    public void setUp() throws Exception {
        batchMeta = new BatchMeta(lock, byteLimit, recordLimit, collectionLastModified);
    }

    @Test
    public void testConstructor() {
        assertEquals(batchMeta.collectionLastModified, collectionLastModified);

        BatchMeta otherBatchMeta = new BatchMeta(lock, byteLimit, recordLimit, null);
        assertNull(otherBatchMeta.collectionLastModified);
    }

    @Test
    public void testGetLastModified() {
        // Defaults to collection L-M
        assertEquals(batchMeta.getLastModified(), Long.valueOf(123L));

        try {
            batchMeta.setLastModified(333L, true);
        } catch (BatchingUploader.LastModifiedChangedUnexpectedly e) {
        } catch (BatchingUploader.LastModifiedDidNotChange e) {}

        assertEquals(batchMeta.getLastModified(), Long.valueOf(333L));
    }

    @Test
    public void testSetLastModified() {
        assertEquals(batchMeta.getLastModified(), collectionLastModified);

        try {
            batchMeta.setLastModified(123L, true);
            assertEquals(batchMeta.getLastModified(), Long.valueOf(123L));
        } catch (BatchingUploader.LastModifiedChangedUnexpectedly e) {
            fail("Should not check for modifications on first L-M set");
        } catch (BatchingUploader.LastModifiedDidNotChange e) {
            fail("Should not check for modifications on first L-M set");
        }

        // Now the same, but passing in 'false' for "expecting to change".
        batchMeta.reset();
        assertEquals(batchMeta.getLastModified(), collectionLastModified);

        try {
            batchMeta.setLastModified(123L, false);
            assertEquals(batchMeta.getLastModified(), Long.valueOf(123L));
        } catch (BatchingUploader.LastModifiedChangedUnexpectedly e) {
            fail("Should not check for modifications on first L-M set");
        } catch (BatchingUploader.LastModifiedDidNotChange e) {
            fail("Should not check for modifications on first L-M set");
        }

        // Test that we can't modify L-M when we're not expecting to
        try {
            batchMeta.setLastModified(333L, false);
        } catch (BatchingUploader.LastModifiedChangedUnexpectedly e) {
            assertTrue("Must throw when L-M changes unexpectedly", true);
        } catch (BatchingUploader.LastModifiedDidNotChange e) {
            fail("Not expecting did-not-change throw");
        }
        assertEquals(batchMeta.getLastModified(), Long.valueOf(123L));

        // Test that we can modify L-M when we're expecting to
        try {
            batchMeta.setLastModified(333L, true);
        } catch (BatchingUploader.LastModifiedChangedUnexpectedly e) {
            fail("Not expecting changed-unexpectedly throw");
        } catch (BatchingUploader.LastModifiedDidNotChange e) {
            fail("Not expecting did-not-change throw");
        }
        assertEquals(batchMeta.getLastModified(), Long.valueOf(333L));

        // Test that we catch L-M modifications that expect to change but actually don't
        try {
            batchMeta.setLastModified(333L, true);
        } catch (BatchingUploader.LastModifiedChangedUnexpectedly e) {
            fail("Not expecting changed-unexpectedly throw");
        } catch (BatchingUploader.LastModifiedDidNotChange e) {
            assertTrue("Expected-to-change-but-did-not-change didn't throw", true);
        }
        assertEquals(batchMeta.getLastModified(), Long.valueOf(333));
    }

    @Test
    public void testSetToken() {
        assertNull(batchMeta.getToken());

        try {
            batchMeta.setToken("MTIzNA", false);
        } catch (BatchingUploader.TokenModifiedException e) {
            fail("Should be able to set token for the first time");
        }
        assertEquals("MTIzNA", batchMeta.getToken());

        try {
            batchMeta.setToken("XYCvNA", false);
        } catch (BatchingUploader.TokenModifiedException e) {
            assertTrue("Should not be able to modify a token", true);
        }
        assertEquals("MTIzNA", batchMeta.getToken());

        try {
            batchMeta.setToken("XYCvNA", true);
        } catch (BatchingUploader.TokenModifiedException e) {
            assertTrue("Should catch non-null tokens during onCommit sets", true);
        }
        assertEquals("MTIzNA", batchMeta.getToken());

        try {
            batchMeta.setToken(null, true);
        } catch (BatchingUploader.TokenModifiedException e) {
            fail("Should be able to set token to null during onCommit set");
        }
        assertNull(batchMeta.getToken());
    }

    @Test
    public void testRecordSucceeded() {
        assertTrue(batchMeta.getSuccessRecordGuids().isEmpty());

        batchMeta.recordSucceeded("guid1");

        assertTrue(batchMeta.getSuccessRecordGuids().size() == 1);
        assertTrue(batchMeta.getSuccessRecordGuids().contains("guid1"));

        try {
            batchMeta.recordSucceeded(null);
            fail();
        } catch (IllegalStateException e) {
            assertTrue("Should not be able to 'succeed' a null guid", true);
        }
    }

    @Test
    public void testByteLimits() {
        assertTrue(batchMeta.canFit(0));

        // Should just fit
        assertTrue(batchMeta.canFit(byteLimit - BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));

        // Can't fit a record due to payload overhead.
        assertFalse(batchMeta.canFit(byteLimit));

        assertFalse(batchMeta.canFit(byteLimit + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));
        assertFalse(batchMeta.canFit(byteLimit * 1000));

        long recordDelta = byteLimit / 2;
        assertFalse(batchMeta.addAndEstimateIfFull(recordDelta));

        // Record delta shouldn't fit due to payload overhead.
        assertFalse(batchMeta.canFit(recordDelta));
    }

    @Test
    public void testCountLimits() {
        // Our record limit is 5, let's add 4.
        assertFalse(batchMeta.addAndEstimateIfFull(1));
        assertFalse(batchMeta.addAndEstimateIfFull(1));
        assertFalse(batchMeta.addAndEstimateIfFull(1));
        assertFalse(batchMeta.addAndEstimateIfFull(1));

        // 5th record still fits in
        assertTrue(batchMeta.canFit(1));

        // Add the 5th record
        assertTrue(batchMeta.addAndEstimateIfFull(1));

        // 6th record won't fit
        assertFalse(batchMeta.canFit(1));
    }

    @Test
    public void testNeedCommit() {
        assertFalse(batchMeta.needToCommit());

        assertFalse(batchMeta.addAndEstimateIfFull(1));

        assertTrue(batchMeta.needToCommit());

        assertFalse(batchMeta.addAndEstimateIfFull(1));
        assertFalse(batchMeta.addAndEstimateIfFull(1));
        assertFalse(batchMeta.addAndEstimateIfFull(1));

        assertTrue(batchMeta.needToCommit());

        batchMeta.reset();

        assertFalse(batchMeta.needToCommit());
    }

    @Test
    public void testAdd() {
        // Ensure we account for payload overhead twice when the batch is empty.
        // Payload overhead is either RECORDS_START or RECORDS_END, and for an empty payload
        // we need both.
        assertTrue(batchMeta.getByteCount() == 2 * BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT);
        assertTrue(batchMeta.getRecordCount() == 0);

        assertFalse(batchMeta.addAndEstimateIfFull(1));

        assertTrue(batchMeta.getByteCount() == (1 + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));
        assertTrue(batchMeta.getRecordCount() == 1);

        assertFalse(batchMeta.addAndEstimateIfFull(1));
        assertFalse(batchMeta.addAndEstimateIfFull(1));
        assertFalse(batchMeta.addAndEstimateIfFull(1));

        assertTrue(batchMeta.getByteCount() == (4 + BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT));
        assertTrue(batchMeta.getRecordCount() == 4);

        assertTrue(batchMeta.addAndEstimateIfFull(1));

        try {
            assertTrue(batchMeta.addAndEstimateIfFull(1));
            fail("BatchMeta should not let us insert records that won't fit");
        } catch (IllegalStateException e) {
            assertTrue(true);
        }
    }

    @Test
    public void testReset() {
        assertTrue(batchMeta.getByteCount() == 2 * BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT);
        assertTrue(batchMeta.getRecordCount() == 0);
        assertTrue(batchMeta.getSuccessRecordGuids().isEmpty());

        // Shouldn't throw even if already empty
        batchMeta.reset();
        assertTrue(batchMeta.getByteCount() == 2 * BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT);
        assertTrue(batchMeta.getRecordCount() == 0);
        assertTrue(batchMeta.getSuccessRecordGuids().isEmpty());

        assertFalse(batchMeta.addAndEstimateIfFull(1));
        batchMeta.recordSucceeded("guid1");
        try {
            batchMeta.setToken("MTIzNA", false);
        } catch (BatchingUploader.TokenModifiedException e) {}
        try {
            batchMeta.setLastModified(333L, true);
        } catch (BatchingUploader.LastModifiedChangedUnexpectedly e) {
        } catch (BatchingUploader.LastModifiedDidNotChange e) {}
        assertEquals(Long.valueOf(333L), batchMeta.getLastModified());
        assertEquals("MTIzNA", batchMeta.getToken());
        assertTrue(batchMeta.getSuccessRecordGuids().size() == 1);

        batchMeta.reset();

        // Counts must be reset
        assertTrue(batchMeta.getByteCount() == 2 * BatchingUploader.PER_PAYLOAD_OVERHEAD_BYTE_COUNT);
        assertTrue(batchMeta.getRecordCount() == 0);
        assertTrue(batchMeta.getSuccessRecordGuids().isEmpty());

        // Collection L-M shouldn't change
        assertEquals(batchMeta.collectionLastModified, collectionLastModified);

        // Token must be reset
        assertNull(batchMeta.getToken());

        // L-M must be reverted to collection L-M
        assertEquals(batchMeta.getLastModified(), collectionLastModified);
    }
}