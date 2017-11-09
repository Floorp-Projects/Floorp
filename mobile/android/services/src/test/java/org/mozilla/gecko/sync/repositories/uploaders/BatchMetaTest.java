package org.mozilla.gecko.sync.repositories.uploaders;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class BatchMetaTest {
    private BatchMeta batchMeta;

    @Before
    public void setUp() throws Exception {
        batchMeta = new BatchMeta(null, null);
    }

    @Test
    public void testGetLastModified() {
        assertNull(batchMeta.getLastModified());

        try {
            batchMeta.setLastModified(333L, true);
        } catch (BatchingUploader.LastModifiedDidNotChange | BatchingUploader.LastModifiedChangedUnexpectedly e) {
        }

        assertEquals(Long.valueOf(333L), batchMeta.getLastModified());
    }

    @Test
    public void testSetLastModified() {
        BatchingUploaderTest.TestRunnableWithTarget<BatchMeta> tests = new BatchingUploaderTest.TestRunnableWithTarget<BatchMeta>() {
            @Override
            void tests() {
                try {
                    batchMeta.setLastModified(123L, true);
                    assertEquals(Long.valueOf(123L), batchMeta.getLastModified());
                } catch (BatchingUploader.LastModifiedDidNotChange | BatchingUploader.LastModifiedChangedUnexpectedly e) {
                    fail("Should not check for modifications on first L-M set");
                }

                try {
                    batchMeta.setLastModified(123L, false);
                    assertEquals(Long.valueOf(123L), batchMeta.getLastModified());
                } catch (BatchingUploader.LastModifiedDidNotChange | BatchingUploader.LastModifiedChangedUnexpectedly e) {
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
                assertEquals(Long.valueOf(123L), batchMeta.getLastModified());

                // Test that we can modify L-M when we're expecting to
                try {
                    batchMeta.setLastModified(333L, true);
                } catch (BatchingUploader.LastModifiedChangedUnexpectedly e) {
                    fail("Not expecting changed-unexpectedly throw");
                } catch (BatchingUploader.LastModifiedDidNotChange e) {
                    fail("Not expecting did-not-change throw");
                }
                assertEquals(Long.valueOf(333L), batchMeta.getLastModified());

                // Test that we catch L-M modifications that expect to change but actually don't
                try {
                    batchMeta.setLastModified(333L, true);
                } catch (BatchingUploader.LastModifiedChangedUnexpectedly e) {
                    fail("Not expecting changed-unexpectedly throw");
                } catch (BatchingUploader.LastModifiedDidNotChange e) {
                    assertTrue("Expected-to-change-but-did-not-change didn't throw", true);
                }
                assertEquals(Long.valueOf(333), batchMeta.getLastModified());
            }
        };

        tests
                .setTarget(batchMeta)
                .run()
                .setTarget(new BatchMeta(123L, null))
                .run();
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
        assertEquals(0, batchMeta.getSuccessRecordGuids().length);

        batchMeta.recordSucceeded("guid1");

        assertEquals(1, batchMeta.getSuccessRecordGuids().length);
        assertEquals("guid1", batchMeta.getSuccessRecordGuids()[0]);

        try {
            batchMeta.recordSucceeded(null);
            fail();
        } catch (IllegalStateException e) {
            assertTrue("Should not be able to 'succeed' a null guid", true);
        }
    }
}