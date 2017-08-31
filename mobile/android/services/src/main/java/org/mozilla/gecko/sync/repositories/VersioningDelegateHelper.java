/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;

import org.mozilla.gecko.db.BrowserContract;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;

/**
 * RepositorySessions using this helper class are operating on locally versioned records, and are
 * performing "versioned syncing", as opposed to "timestamp-based syncing". Both are approaches to
 * record change tracking, and we prefer version-based tracking for its robustness.
 *
 * Timestamp-oriented problem overview:
 * - it is possible for sync to entirely miss local modifications to records, under certain conditions
 * - consider modifications made to records after we've fetched them from a local database, and before
 *   we finished uploading them to the server. Those modifications will be omitted during the current
 *   sync, as well as during consequent syncs. They are invisible to other clients, until the current
 *   client modifies that record again, triggering its synchronization.
 * - a set of changed records is determined purely on their modification timestamps, and so drifting
 *   and often incorrect local clocks make the matters even worse.
 *
 * The record versioning solution:
 * - the core idea is track state of each record via two versions: local and synced
 * - localVersion is incremented whenever any modifications are made to the record from Fennec proper;
 *   that is, outside of Sync
 * - whenever sync reconciles a record and detects that it also had a local modification, localVersion
 *   is incremented as well so that the new "merged" state is uploaded and local modifications aren't
 *   missed
 * - syncVersion is moved forward once we're sure we have uploaded a certain localVersion of the record
 * - before new/changed/merged records are uploaded, and right after they're read from the database,
 *   we make a note of local versions for each retrieved GUID
 * - once upload is finished successfully - that is, once this new state of the world as far as this
 *   client is concerned has been persisted to the server - we update syncVersion values for each
 *   processed GUID to the value we noted before the upload
 * - this is a "all or nothing" operation - either we move sync versions forward for all processed
 *   records, or we don't move them at all
 * - records are expected to be uploaded in atomic batches during normal operation
 * - if batching mode is disabled and we encounter a partial upload, syncVersion won't move forward
 *   and records will be eventually re-uploaded
 * - consider syncing of record X:
 *   1. Before a sync, its local version is at 7, and its sync version is at 6.
 *   2. Along with other records, X is queued for an upload to the server, and we make note of localVersion=7.
 *   3. During the download, we fetch a remotely modified version of record X. We reconcile, persist the
 *   merged version, and increment localVersion to 8.
 *   4. Upload phase begins, and we fetch record X from the database at version 8.
 *   4. During the upload, a local modification is performed, further bumping X's localVersion to 9.
 *   5. This modification is not uploaded, since copy of the record at version 8 is already in memory.
 *   6. After an upload, we move syncVersion to 8.
 *   7. Sync is finished, and X is now at localVersion=9, and syncVersion=8, indicating that there are
 *   still changes to be uploaded.
 *
 * For context, see Bug 1364644, and Bug 1258127 for the Desktop counterpart of this work ("Sync Tracker").
 *
 * @author grisha
*/
public class VersioningDelegateHelper {
    private final Context context;

    // Written on the "main" sync thread, and read from a thread running on session's storeWorkQueue.
    // We're not concerned with concurrent access, but with memory visibility across threads.
    // ConcurrentHashMap uses fine grained locking, so should have little overhead here.
    private final ConcurrentHashMap<String, Integer> localVersionsOfGuids = new ConcurrentHashMap<>();
    private final Uri repositoryContentUri;

    public VersioningDelegateHelper(Context context, Uri repositoryContentUri) {
        this.context = context;
        this.repositoryContentUri = repositoryContentUri;
    }

    public RepositorySessionFetchRecordsDelegate getFetchDelegate(RepositorySessionFetchRecordsDelegate delegate) {
        return new VersionedFetchDelegate(delegate);
    }

    public RepositorySessionStoreDelegate getStoreDelegate(RepositorySessionStoreDelegate delegate) {
        return new VersionedStoreDelegate(delegate);
    }

    public void persistSyncVersions() {
        // At this point we can be sure that all records in our guidsMap have been successfully
        // uploaded. Move syncVersions forward for each GUID en masse.
        final Bundle data = new Bundle();
        final int versionMapSizeBeforeUpdate;

        synchronized (this.localVersionsOfGuids) {
            data.putSerializable(BrowserContract.METHOD_PARAM_DATA, this.localVersionsOfGuids);
            versionMapSizeBeforeUpdate = this.localVersionsOfGuids.size();
        }

        final Bundle result = context.getContentResolver().call(
                repositoryContentUri,
                BrowserContract.METHOD_UPDATE_SYNC_VERSIONS,
                repositoryContentUri.toString(),
                data
        );

        if (result == null) {
            throw new IllegalStateException("Expected to receive a result after decrementing change counters");
        }

        final int changed = (int) result.getSerializable(BrowserContract.METHOD_RESULT);
        final int versionMapSizeAfterUpdate = this.localVersionsOfGuids.size();

        // None of these should happen, but something's clearly amiss. These strong assertions are
        // here to help figure out root cause for Bug 1392078.
        if (versionMapSizeBeforeUpdate != versionMapSizeAfterUpdate) {
            throw new IllegalStateException("Version/guid map changed during syncVersion update");
        }

        if (changed < versionMapSizeBeforeUpdate) {
            throw new IllegalStateException("Updated fewer sync versions than expected");
        }

        if (changed > versionMapSizeBeforeUpdate) {
            throw new IllegalStateException("Updated more sync versions than expected");
        }
    }

    private class VersionedFetchDelegate implements RepositorySessionFetchRecordsDelegate {
        private final RepositorySessionFetchRecordsDelegate inner;

        VersionedFetchDelegate(RepositorySessionFetchRecordsDelegate delegate) {
            this.inner = delegate;
        }

        public void onFetchFailed(Exception ex) {
            this.inner.onFetchFailed(ex);
        }

        @Override
        public void onFetchedRecord(Record record) {
            if (record.localVersion == null) {
                throw new IllegalStateException("Encountered an unversioned record during versioned sync.");
            }
            localVersionsOfGuids.put(record.guid, record.localVersion);
            this.inner.onFetchedRecord(record);
        }

        @Override
        public void onFetchCompleted() {
            this.inner.onFetchCompleted();
        }

        @Override
        public void onBatchCompleted() {
            this.inner.onBatchCompleted();
        }

        @Override
        public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor) {
            return this.inner.deferredFetchDelegate(executor);
        }
    }

    private class VersionedStoreDelegate implements RepositorySessionStoreDelegate {
        private final RepositorySessionStoreDelegate inner;

        VersionedStoreDelegate(RepositorySessionStoreDelegate delegate) {
            this.inner = delegate;
        }

        @Override
        public void onRecordStoreFailed(Exception ex, String recordGuid) {
            inner.onRecordStoreFailed(ex, recordGuid);
        }

        @Override
        public void onRecordStoreReconciled(String guid, String oldGuid, Integer newVersion) {
            if (newVersion == null) {
                throw new IllegalArgumentException("Received null localVersion after reconciling a versioned record.");
            }
            // As incoming records are processed, we might be chain-duping them. In which case,
            // we'd record a reconciled record and its guid in this map, and then another will come in
            // and will dupe to the just-reconciled record. We'll do the replacement, and record the
            // winning guid in this map. At that point, our map has two guids, one of which doesn't
            // exist in the database anymore.
            // That is why we remove old GUIDs from the map whenever we perform a replacement.
            // See Bug 1392716.
            localVersionsOfGuids.remove(oldGuid);
            localVersionsOfGuids.put(guid, newVersion);
            inner.onRecordStoreReconciled(guid, oldGuid, newVersion);
        }

        @Override
        public void onRecordStoreSucceeded(String guid) {
            inner.onRecordStoreSucceeded(guid);
        }

        @Override
        public void onStoreCompleted() {
            inner.onStoreCompleted();
        }

        @Override
        public void onStoreFailed(Exception e) {
            inner.onStoreFailed(e);
        }

        @Override
        public RepositorySessionStoreDelegate deferredStoreDelegate(ExecutorService executor) {
            return inner.deferredStoreDelegate(executor);
        }
    }
}