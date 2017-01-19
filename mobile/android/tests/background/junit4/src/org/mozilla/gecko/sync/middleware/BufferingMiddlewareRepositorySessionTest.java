/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.middleware;

import org.junit.Before;
import org.junit.Test;
import org.mozilla.gecko.background.testhelpers.MockRecord;
import org.mozilla.gecko.sync.middleware.storage.BufferStorage;
import org.mozilla.gecko.sync.middleware.storage.MemoryBufferStorage;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.mock;

import static org.junit.Assert.*;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

public class BufferingMiddlewareRepositorySessionTest {
    private RepositorySession innerRepositorySession;
    private BufferingMiddlewareRepositorySession bufferingSession;
    private BufferingMiddlewareRepositorySession bufferingSessionMocked;
    private BufferStorage bufferStorage;
    private BufferStorage bufferStorageMocked;

    @Before
    public void setUp() throws Exception {
        BufferingMiddlewareRepository bufferingRepository;
        Repository innerRepositoy;

        innerRepositoy = mock(Repository.class);
        innerRepositorySession = mock(RepositorySession.class);
        bufferingRepository = new BufferingMiddlewareRepository(
                0L,
                new MemoryBufferStorage(),
                innerRepositoy
        );

        bufferStorage = new MemoryBufferStorage();
        bufferStorageMocked = mock(MemoryBufferStorage.class);

        bufferingSession = new BufferingMiddlewareRepositorySession(
                innerRepositorySession, bufferingRepository, 0L,
                bufferStorage);

        bufferingSessionMocked = new BufferingMiddlewareRepositorySession(
                innerRepositorySession, bufferingRepository, 0L,
                bufferStorageMocked);
    }

    @Test
    public void store() throws Exception {
        assertEquals(0, bufferStorage.all().size());

        MockRecord record = new MockRecord("guid1", null, 1, false);
        bufferingSession.store(record);
        assertEquals(1, bufferStorage.all().size());

        MockRecord record1 = new MockRecord("guid2", null, 1, false);
        bufferingSession.store(record1);
        assertEquals(2, bufferStorage.all().size());
        assertEquals(1, bufferStorage.latestModifiedTimestamp());

        // record2 must replace record.
        MockRecord record2 = new MockRecord("guid1", null, 2, false);
        bufferingSession.store(record2);
        assertEquals(2, bufferStorage.all().size());
        assertEquals(2, bufferStorage.latestModifiedTimestamp());

        // Ensure inner session doesn't see incoming records.
        verify(innerRepositorySession, never()).store(record);
        verify(innerRepositorySession, never()).store(record1);
        verify(innerRepositorySession, never()).store(record2);
    }

    @Test
    public void storeDone() throws Exception {
        // Verify that storage's flush is called.
        verify(bufferStorageMocked, times(0)).flush();
        bufferingSessionMocked.doStoreDonePrepare();
        verify(bufferStorageMocked, times(1)).flush();

        // Trivial case, no records to merge.
        bufferingSession.doStoreDone(123L);
        verify(innerRepositorySession, times(1)).storeDone(123L);
        verify(innerRepositorySession, never()).store(any(Record.class));

        // Reset call counters.
        reset(innerRepositorySession);

        // Now store some records.
        MockRecord record = new MockRecord("guid1", null, 1, false);
        bufferingSession.store(record);

        MockRecord record2 = new MockRecord("guid2", null, 13, false);
        bufferingSession.store(record2);

        MockRecord record3 = new MockRecord("guid3", null, 5, false);
        bufferingSession.store(record3);

        // NB: same guid as above.
        MockRecord record4 = new MockRecord("guid3", null, -1, false);
        bufferingSession.store(record4);

        // Done storing.
        bufferingSession.doStoreDone(123L);

        // Ensure all records where stored in the wrapped session.
        verify(innerRepositorySession, times(1)).store(record);
        verify(innerRepositorySession, times(1)).store(record2);
        verify(innerRepositorySession, times(1)).store(record4);

        // Ensure storeDone was called on the wrapped session.
        verify(innerRepositorySession, times(1)).storeDone(123L);

        // Ensure buffer wasn't cleared on the wrapped session.
        assertEquals(3, bufferStorage.all().size());
    }

    @Test
    public void storeFlush() throws Exception {
        verify(bufferStorageMocked, times(0)).flush();
        bufferingSessionMocked.storeFlush();
        verify(bufferStorageMocked, times(1)).flush();
    }

    @Test
    public void performCleanup() throws Exception {
        // Baseline.
        assertEquals(0, bufferStorage.all().size());

        // Test that we can call cleanup with an empty buffer storage.
        bufferingSession.performCleanup();
        assertEquals(0, bufferStorage.all().size());

        // Store a couple of records.
        MockRecord record = new MockRecord("guid1", null, 1, false);
        bufferingSession.store(record);

        MockRecord record2 = new MockRecord("guid2", null, 13, false);
        bufferingSession.store(record2);

        // Confirm it worked.
        assertEquals(2, bufferStorage.all().size());

        // Test that buffer storage is cleaned up.
        bufferingSession.performCleanup();
        assertEquals(0, bufferStorage.all().size());
    }

    @Test
    public void sourceFailed() throws Exception {
        // Source failes before any records have been stored.
        bufferingSession.sourceFailed(new Exception());
        assertEquals(0, bufferStorage.all().size());

        // Store some records now.
        MockRecord record = new MockRecord("guid1", null, 1, false);
        bufferingSession.store(record);

        MockRecord record2 = new MockRecord("guid2", null, 13, false);
        bufferingSession.store(record2);

        MockRecord record3 = new MockRecord("guid3", null, 5, false);
        bufferingSession.store(record3);

        // Verify that buffer is intact after source fails.
        bufferingSession.sourceFailed(new Exception());
        assertEquals(3, bufferStorage.all().size());

        // Verify that buffer is flushed after source fails.
        verify(bufferStorageMocked, times(0)).flush();
        bufferingSessionMocked.sourceFailed(new Exception());
        verify(bufferStorageMocked, times(1)).flush();
    }

    @Test
    public void abort() throws Exception {
        MockRecord record = new MockRecord("guid1", null, 1, false);
        bufferingSession.store(record);

        MockRecord record2 = new MockRecord("guid2", null, 13, false);
        bufferingSession.store(record2);

        MockRecord record3 = new MockRecord("guid3", null, 5, false);
        bufferingSession.store(record3);

        // NB: same guid as above.
        MockRecord record4 = new MockRecord("guid3", null, -1, false);
        bufferingSession.store(record4);

        bufferingSession.abort();

        // Verify number of records didn't change.
        // Abort shouldn't clear the buffer.
        assertEquals(3, bufferStorage.all().size());
    }

    @Test
    public void setStoreDelegate() throws Exception {
        RepositorySessionStoreDelegate delegate = mock(RepositorySessionStoreDelegate.class);
        bufferingSession.setStoreDelegate(delegate);
        verify(innerRepositorySession).setStoreDelegate(delegate);
    }

    @Test
    public void getHighWaterMarkTimestamp() throws Exception {
        // Trivial case, empty buffer.
        assertEquals(0, bufferingSession.getHighWaterMarkTimestamp());

        MockRecord record = new MockRecord("guid1", null, 1, false);
        bufferingSession.store(record);
        assertEquals(1, bufferingSession.getHighWaterMarkTimestamp());

        MockRecord record3 = new MockRecord("guid3", null, 5, false);
        bufferingSession.store(record3);
        assertEquals(5, bufferingSession.getHighWaterMarkTimestamp());

        // NB: same guid as above.
        MockRecord record4 = new MockRecord("guid3", null, -1, false);
        bufferingSession.store(record4);
        assertEquals(1, bufferingSession.getHighWaterMarkTimestamp());

        MockRecord record2 = new MockRecord("guid2", null, 13, false);
        bufferingSession.store(record2);
        assertEquals(13, bufferingSession.getHighWaterMarkTimestamp());
    }
}