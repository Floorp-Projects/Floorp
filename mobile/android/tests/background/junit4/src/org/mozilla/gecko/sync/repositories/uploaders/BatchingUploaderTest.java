/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.repositories.uploaders;

import android.support.annotation.NonNull;

import static org.junit.Assert.*;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.MockRecord;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.InfoCollections;
import org.mozilla.gecko.sync.InfoConfiguration;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.repositories.Server11Repository;
import org.mozilla.gecko.sync.repositories.Server11RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;

import java.net.URISyntaxException;
import java.util.Random;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;

@RunWith(TestRunner.class)
public class BatchingUploaderTest {
    class MockExecutorService implements Executor {
        public int totalPayloads = 0;
        public int commitPayloads = 0;

        @Override
        public void execute(@NonNull Runnable command) {
            ++totalPayloads;
            if (((RecordUploadRunnable) command).isCommit) {
                ++commitPayloads;
            }
        }
    }

    class MockStoreDelegate implements RepositorySessionStoreDelegate {
        public int storeFailed = 0;
        public int storeSucceeded = 0;
        public int storeCompleted = 0;

        @Override
        public void onRecordStoreFailed(Exception ex, String recordGuid) {
            ++storeFailed;
        }

        @Override
        public void onRecordStoreSucceeded(String guid) {
            ++storeSucceeded;
        }

        @Override
        public void onStoreCompleted(long storeEnd) {
            ++storeCompleted;
        }

        @Override
        public RepositorySessionStoreDelegate deferredStoreDelegate(ExecutorService executor) {
            return null;
        }
    }

    private Executor workQueue;
    private RepositorySessionStoreDelegate storeDelegate;

    @Before
    public void setUp() throws Exception {
        workQueue = new MockExecutorService();
        storeDelegate = new MockStoreDelegate();
    }

    @Test
    public void testProcessEvenPayloadBatch() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        MockRecord record = new MockRecord(Utils.generateGuid(), null, 0, false);
        // 1st
        uploader.process(record);
        assertEquals(0, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 2nd -> payload full
        uploader.process(record);
        assertEquals(1, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 3rd
        uploader.process(record);
        assertEquals(1, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 4th -> batch & payload full
        uploader.process(record);
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
        // 5th
        uploader.process(record);
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
        // 6th -> payload full
        uploader.process(record);
        assertEquals(3, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
        // 7th
        uploader.process(record);
        assertEquals(3, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
        // 8th -> batch & payload full
        uploader.process(record);
        assertEquals(4, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(2, ((MockExecutorService) workQueue).commitPayloads);
        // 9th
        uploader.process(record);
        assertEquals(4, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(2, ((MockExecutorService) workQueue).commitPayloads);
        // 10th -> payload full
        uploader.process(record);
        assertEquals(5, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(2, ((MockExecutorService) workQueue).commitPayloads);
        // 11th
        uploader.process(record);
        assertEquals(5, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(2, ((MockExecutorService) workQueue).commitPayloads);
        // 12th -> batch & payload full
        uploader.process(record);
        assertEquals(6, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(3, ((MockExecutorService) workQueue).commitPayloads);
        // 13th
        uploader.process(record);
        assertEquals(6, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(3, ((MockExecutorService) workQueue).commitPayloads);
    }

    @Test
    public void testProcessUnevenPayloadBatch() {
        BatchingUploader uploader = makeConstrainedUploader(2, 5);

        MockRecord record = new MockRecord(Utils.generateGuid(), null, 0, false);
        // 1st
        uploader.process(record);
        assertEquals(0, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 2nd -> payload full
        uploader.process(record);
        assertEquals(1, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 3rd
        uploader.process(record);
        assertEquals(1, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 4th -> payload full
        uploader.process(record);
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 5th -> batch full
        uploader.process(record);
        assertEquals(3, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
        // 6th -> starts new batch
        uploader.process(record);
        assertEquals(3, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
        // 7th -> payload full
        uploader.process(record);
        assertEquals(4, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
        // 8th
        uploader.process(record);
        assertEquals(4, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
        // 9th -> payload full
        uploader.process(record);
        assertEquals(5, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
        // 10th -> batch full
        uploader.process(record);
        assertEquals(6, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(2, ((MockExecutorService) workQueue).commitPayloads);
        // 11th -> starts new batch
        uploader.process(record);
        assertEquals(6, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(2, ((MockExecutorService) workQueue).commitPayloads);
    }

    @Test
    public void testNonBatchingOptimization() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        MockRecord record = new MockRecord(Utils.generateGuid(), null, 0, false);
        // 1st
        uploader.process(record);
        assertEquals(0, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 2nd
        uploader.process(record);
        assertEquals(1, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 3rd
        uploader.process(record);
        assertEquals(1, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
        // 4th
        uploader.process(record);
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);

        // 5th
        uploader.process(record);
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);

        // And now we tell uploader that batching isn't supported.
        // It shouldn't bother with batches from now on, just payloads.
        uploader.setInBatchingMode(false);

        // 6th
        uploader.process(record);
        assertEquals(3, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);

        // 7th
        uploader.process(record);
        assertEquals(3, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);

        // 8th
        uploader.process(record);
        assertEquals(4, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);

        // 9th
        uploader.process(record);
        assertEquals(4, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);

        // 10th
        uploader.process(record);
        assertEquals(5, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
    }

    @Test
    public void testPreemtiveUploadByteCounts() {
        // While processing a record, if we know for sure that another one won't fit,
        // we upload the payload.
        BatchingUploader uploader = makeConstrainedUploader(3, 6);

        // Payload byte max: 1024; batch byte max: 4096
        MockRecord record = new MockRecord(Utils.generateGuid(), null, 0, false, 400);

        uploader.process(record);
        assertEquals(0, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);

        // After 2nd record, byte count is at 800+overhead. Our payload max is 1024, so it's unlikely
        // we can fit another record at this pace. Expect payload to be uploaded.
        uploader.process(record);
        assertEquals(1, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);

        // After this record, we'll have less than 124 bytes of room left in the payload. Expect upload.
        record = new MockRecord(Utils.generateGuid(), null, 0, false, 970);
        uploader.process(record);
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);

        uploader.process(record);
        assertEquals(3, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);

        // At this point our byte count for the batch is at 3600+overhead;
        // since we have just 496 bytes left in the batch, it's unlikely we'll fit another record.
        // Expect a batch commit
        uploader.process(record);
        assertEquals(4, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
    }

    @Test
    public void testRandomPayloadSizesBatching() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        final Random random = new Random();
        for (int i = 0; i < 15000; i++) {
            uploader.process(new MockRecord(Utils.generateGuid(), null, 0, false, random.nextInt(15000)));
        }
    }

    @Test
    public void testRandomPayloadSizesNonBatching() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        final Random random = new Random();
        uploader.setInBatchingMode(false);
        for (int i = 0; i < 15000; i++) {
            uploader.process(new MockRecord(Utils.generateGuid(), null, 0, false, random.nextInt(15000)));
        }
    }

    @Test
    public void testRandomPayloadSizesNonBatchingDelayed() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        final Random random = new Random();
        // Delay telling uploader that batching isn't supported.
        // Randomize how many records we wait for.
        final int delay = random.nextInt(20);
        for (int i = 0; i < 15000; i++) {
            if (delay == i) {
                uploader.setInBatchingMode(false);
            }
            uploader.process(new MockRecord(Utils.generateGuid(), null, 0, false, random.nextInt(15000)));
        }
    }

    @Test
    public void testNoMoreRecordsAfterPayloadPost() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        // Process two records (payload limit is also two, batch is four),
        // and ensure that 'no more records' commits.
        MockRecord record = new MockRecord(Utils.generateGuid(), null, 0, false);
        uploader.process(record);
        uploader.process(record);
        uploader.setInBatchingMode(true);
        uploader.commitIfNecessaryAfterLastPayload();
        // One will be a payload post, the other one is batch commit (empty payload)
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
    }

    @Test
    public void testNoMoreRecordsAfterPayloadPostWithOneRecordLeft() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        // Process two records (payload limit is also two, batch is four),
        // and ensure that 'no more records' commits.
        MockRecord record = new MockRecord(Utils.generateGuid(), null, 0, false);
        uploader.process(record);
        uploader.process(record);
        uploader.process(record);
        uploader.commitIfNecessaryAfterLastPayload();
        // One will be a payload post, the other one is batch commit (one record payload)
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
    }

    @Test
    public void testNoMoreRecordsNoOp() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        uploader.commitIfNecessaryAfterLastPayload();
        assertEquals(0, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
    }

    @Test
    public void testNoMoreRecordsNoOpAfterCommit() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        MockRecord record = new MockRecord(Utils.generateGuid(), null, 0, false);
        uploader.process(record);
        uploader.process(record);
        uploader.process(record);
        uploader.process(record);
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);

        uploader.commitIfNecessaryAfterLastPayload();
        assertEquals(2, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
    }

    @Test
    public void testNoMoreRecordsEvenNonBatching() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        // Process two records (payload limit is also two, batch is four),
        // set non-batching mode, and ensure that 'no more records' doesn't commit.
        MockRecord record = new MockRecord(Utils.generateGuid(), null, 0, false);
        uploader.process(record);
        uploader.process(record);
        uploader.setInBatchingMode(false);
        uploader.commitIfNecessaryAfterLastPayload();
        // One will be a payload post, the other one is batch commit (one record payload)
        assertEquals(1, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(0, ((MockExecutorService) workQueue).commitPayloads);
    }

    @Test
    public void testNoMoreRecordsIncompletePayload() {
        BatchingUploader uploader = makeConstrainedUploader(2, 4);

        // We have one record (payload limit is 2), and "no-more-records" signal should commit it.
        MockRecord record = new MockRecord(Utils.generateGuid(), null, 0, false);
        uploader.process(record);

        uploader.commitIfNecessaryAfterLastPayload();
        assertEquals(1, ((MockExecutorService) workQueue).totalPayloads);
        assertEquals(1, ((MockExecutorService) workQueue).commitPayloads);
    }

    private BatchingUploader makeConstrainedUploader(long maxPostRecords, long maxTotalRecords) {
        Server11RepositorySession server11RepositorySession = new Server11RepositorySession(
                makeCountConstrainedRepository(maxPostRecords, maxTotalRecords)
        );
        server11RepositorySession.setStoreDelegate(storeDelegate);
        return new BatchingUploader(server11RepositorySession, workQueue, storeDelegate);
    }

    private Server11Repository makeCountConstrainedRepository(long maxPostRecords, long maxTotalRecords) {
        return makeConstrainedRepository(1024, 1024, maxPostRecords, 4096, maxTotalRecords);
    }

    private Server11Repository makeConstrainedRepository(long maxRequestBytes, long maxPostBytes, long maxPostRecords, long maxTotalBytes, long maxTotalRecords) {
        ExtendedJSONObject infoConfigurationJSON = new ExtendedJSONObject();
        infoConfigurationJSON.put(InfoConfiguration.MAX_TOTAL_BYTES, maxTotalBytes);
        infoConfigurationJSON.put(InfoConfiguration.MAX_TOTAL_RECORDS, maxTotalRecords);
        infoConfigurationJSON.put(InfoConfiguration.MAX_POST_RECORDS, maxPostRecords);
        infoConfigurationJSON.put(InfoConfiguration.MAX_POST_BYTES, maxPostBytes);
        infoConfigurationJSON.put(InfoConfiguration.MAX_REQUEST_BYTES, maxRequestBytes);

        InfoConfiguration infoConfiguration = new InfoConfiguration(infoConfigurationJSON);

        try {
            return new Server11Repository(
                    "dummyCollection",
                    "http://dummy.url/",
                    null,
                    new InfoCollections(),
                    infoConfiguration
            );
        } catch (URISyntaxException e) {
            // Won't throw, and this won't happen.
            return null;
        }
    }
}