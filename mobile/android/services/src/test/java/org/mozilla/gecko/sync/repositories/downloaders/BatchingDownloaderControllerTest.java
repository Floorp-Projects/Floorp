/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.downloaders;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.repositories.Server15RepositorySession;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class BatchingDownloaderControllerTest {
    private BatchingDownloaderTest.MockSever15Repository serverRepository;
    private Server15RepositorySession repositorySession;
    private BatchingDownloaderTest.MockSessionFetchRecordsDelegate sessionFetchRecordsDelegate;
    private BatchingDownloaderTest.MockDownloader mockDownloader;
    private BatchingDownloaderTest.CountingShadowRepositoryState repositoryStateProvider;

    @Before
    public void setUp() throws Exception {
        sessionFetchRecordsDelegate = new BatchingDownloaderTest.MockSessionFetchRecordsDelegate();

        serverRepository = new BatchingDownloaderTest.MockSever15Repository(
                "dummyCollection", "http://dummy.url/", null,
                new InfoCollections(), new InfoConfiguration());
        repositorySession = new Server15RepositorySession(serverRepository);
        repositoryStateProvider = new BatchingDownloaderTest.CountingShadowRepositoryState();
        mockDownloader = new BatchingDownloaderTest.MockDownloader(repositorySession, true, true, repositoryStateProvider);
    }

    @Test
    public void resumeFetchSinceIfPossible() throws Exception {
        assertTrue(BatchingDownloaderController.setInitialResumeContextAndCommit(repositoryStateProvider, "offset1", 2L, "oldest"));

        // Test that we'll resume from offset if context is correct.
        BatchingDownloaderController.resumeFetchSinceIfPossible(
                mockDownloader, repositoryStateProvider, sessionFetchRecordsDelegate, 3L, 25L, "oldest");
        assertEquals("offset1", mockDownloader.offset);
        // Ensure we'll use context-provided since value.
        assertEquals(2L, mockDownloader.newer);
        assertEquals("oldest", mockDownloader.sort);

        assertTrue(BatchingDownloaderController.updateResumeContextAndCommit(repositoryStateProvider, "offset2"));

        // Test that we won't resume on context mismatch. Ensure that we use new context.
        BatchingDownloaderController.resumeFetchSinceIfPossible(
                mockDownloader, repositoryStateProvider, sessionFetchRecordsDelegate, 1L, 25L, "newest");
        assertEquals(null, mockDownloader.offset);
        assertEquals(1L, mockDownloader.newer);
        assertEquals("newest", mockDownloader.sort);

        assertTrue(BatchingDownloaderController.updateResumeContextAndCommit(repositoryStateProvider, "offset3"));

        // Test that we may fetch with a different limit and still resume since our context is valid.
        BatchingDownloaderController.resumeFetchSinceIfPossible(
                mockDownloader, repositoryStateProvider, sessionFetchRecordsDelegate, 3L, 50L, "oldest");
        assertEquals("offset3", mockDownloader.offset);
        assertEquals("oldest", mockDownloader.sort);
        assertEquals(2L, mockDownloader.newer);
    }

    @Test
    public void testInitialSetAndUpdateOfContext() throws Exception {
        assertFalse(BatchingDownloaderController.isResumeContextSet(repositoryStateProvider));

        // Test that we can't update context which wasn't set yet.
        try {
            assertFalse(BatchingDownloaderController.updateResumeContextAndCommit(repositoryStateProvider, "offset2"));
            fail();
        } catch (IllegalStateException e) {}

        // Test that we can set context and check that it's set.
        assertTrue(BatchingDownloaderController.setInitialResumeContextAndCommit(repositoryStateProvider, "offset1", 1L, "newest"));
        assertTrue(BatchingDownloaderController.isResumeContextSet(repositoryStateProvider));

        // Test that we can't set context after it was already set.
        try {
            assertFalse(BatchingDownloaderController.setInitialResumeContextAndCommit(repositoryStateProvider, "offset1", 1L, "newest"));
            fail();
        } catch (IllegalStateException e) {}

        // Test that we can update context after it was set.
        assertTrue(BatchingDownloaderController.updateResumeContextAndCommit(repositoryStateProvider, "offset2"));
    }
}