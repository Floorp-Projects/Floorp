/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;



import android.content.Context;
import android.os.SystemClock;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.repositories.InactiveSessionException;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.NoStoreDelegateException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionBeginDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.BookmarkRecord;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.telemetry.TelemetryStageCollector;
import org.mozilla.gecko.sync.validation.BookmarkValidationResults;
import org.mozilla.gecko.sync.validation.BookmarkValidator;

import java.util.ArrayList;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ExecutorService;

/**
 * This repository fetches all records that are present on the client or server, and runs
 * the BookmarkValidator on these, so that we can include that data in telemetry.
 *
 * This is obviously expensive, so it's worth noting that we don't run it frequently.
 * @see org.mozilla.gecko.sync.stage.ValidateBookmarksSyncStage for the set of requirements
 * that must be met in order to run validation.
 *
 * It's mostly concerned with the client's view of the world, and how client propagates that
 * view outwards. That is why we're wrapping a regular bookmarks session here, and capturing
 * any side-effects that its internal 'fetch' methods will have.
 *
 * We're not concerned with how client's view of the world is shaped - that is, we're
 * not capturing record reconciliation here directly. These sorts of effects will
 * (hopefully) be determined from validation results in aggregate.
 *
 * @see BookmarkValidationResults for the concrete set of problems it checks for.
 */
public class BookmarksValidationRepository extends Repository {
    private static final String LOG_TAG = "BookmarksValidationRepository";

    protected final ClientsDataDelegate clientsDataDelegate;
    private final TelemetryStageCollector parentCollector;

    public BookmarksValidationRepository(ClientsDataDelegate clientsDataDelegate, TelemetryStageCollector collector) {
        this.clientsDataDelegate = clientsDataDelegate;
        this.parentCollector = collector;
    }

    @Override
    public void createSession(RepositorySessionCreationDelegate delegate, Context context) {
        delegate.onSessionCreated(new BookmarksValidationRepositorySession(this, context));
    }

    public class BookmarksValidationRepositorySession extends RepositorySession {

        private final ConcurrentLinkedQueue<BookmarkRecord> local = new ConcurrentLinkedQueue<>();
        private final ConcurrentLinkedQueue<BookmarkRecord> remote = new ConcurrentLinkedQueue<>();
        private long startTime;
        private final BookmarksRepositorySession wrappedSession;

        public BookmarksValidationRepositorySession(Repository r, Context context) {
            super(r);
            startTime = SystemClock.elapsedRealtime();
            wrappedSession = new BookmarksRepositorySession(r, context);
        }

        @Override
        public long getLastSyncTimestamp() {
            return 0;
        }

        @Override
        public void wipe(RepositorySessionWipeDelegate delegate) {
            Logger.error(LOG_TAG, "wipe() called on bookmark validator");
            throw new UnsupportedOperationException();
        }

        @Override
        public synchronized boolean isActive() {
            return this.wrappedSession.isActive();
        }


        @Override
        public void begin(final RepositorySessionBeginDelegate delegate) throws InvalidSessionTransitionException {
            wrappedSession.begin(new RepositorySessionBeginDelegate() {

                @Override
                public void onBeginFailed(Exception ex) {
                    delegate.onBeginFailed(ex);
                }

                @Override
                public void onBeginSucceeded(RepositorySession session) {
                    delegate.onBeginSucceeded(BookmarksValidationRepositorySession.this);
                }

                @Override
                public RepositorySessionBeginDelegate deferredBeginDelegate(ExecutorService executor) {
                    final RepositorySessionBeginDelegate deferred = delegate.deferredBeginDelegate(executor);
                    return new RepositorySessionBeginDelegate() {
                        @Override
                        public void onBeginSucceeded(RepositorySession session) {
                            if (wrappedSession != session) {
                                Logger.warn(LOG_TAG, "Got onBeginSucceeded for session " + session + ", not our inner session!");
                            }
                            deferred.onBeginSucceeded(BookmarksValidationRepositorySession.this);
                        }

                        @Override
                        public void onBeginFailed(Exception ex) {
                            deferred.onBeginFailed(ex);
                        }

                        @Override
                        public RepositorySessionBeginDelegate deferredBeginDelegate(ExecutorService executor) {
                            return this;
                        }
                    };
                }
            });
        }

        @Override
        public void finish(final RepositorySessionFinishDelegate delegate) throws InactiveSessionException {
            wrappedSession.finish(new RepositorySessionFinishDelegate() {
                @Override
                public void onFinishFailed(Exception ex) {
                    delegate.onFinishFailed(ex);
                }

                @Override
                public void onFinishSucceeded(RepositorySession session, RepositorySessionBundle bundle) {
                    delegate.onFinishSucceeded(BookmarksValidationRepositorySession.this, bundle);
                }

                @Override
                public RepositorySessionFinishDelegate deferredFinishDelegate(ExecutorService executor) {
                    return this;
                }
            });
        }

        private void validateForTelemetry() {
            ArrayList<BookmarkRecord> localRecords = new ArrayList<>(local);
            ArrayList<BookmarkRecord> remoteRecords = new ArrayList<>(remote);
            BookmarkValidationResults results = BookmarkValidator.validateClientAgainstServer(localRecords, remoteRecords);
            ExtendedJSONObject o = new ExtendedJSONObject();
            o.put("took", SystemClock.elapsedRealtime() - startTime);
            o.put("checked", Math.max(localRecords.size(), remoteRecords.size()));
            o.put("problems", results.jsonSummary());
            Logger.info(LOG_TAG, "Completed validation in " + (SystemClock.elapsedRealtime() - startTime) + " ms");
            parentCollector.validation = o;
        }

        @Override
        public void fetchModified(final RepositorySessionFetchRecordsDelegate delegate) {
            wrappedSession.fetchAll(new RepositorySessionFetchRecordsDelegate() {
                @Override
                public void onFetchFailed(Exception ex) {
                    local.clear();
                    remote.clear();
                    delegate.onFetchFailed(ex);
                }

                @Override
                public void onFetchedRecord(Record record) {
                    local.add((BookmarkRecord)record);
                }

                @Override
                public void onFetchCompleted() {
                    validateForTelemetry();
                    delegate.onFetchCompleted();
                }

                @Override
                public void onBatchCompleted() {}

                @Override
                public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor) {
                    return null;
                }
            });
        }

        @Override
        public void fetch(String[] guids, RepositorySessionFetchRecordsDelegate delegate) throws InactiveSessionException {
            Logger.error(LOG_TAG, "fetch guids[] called on bookmark validator");
            throw new UnsupportedOperationException();
        }

        @Override
        public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
            this.fetchModified(delegate);
        }

        @Override
        public void store(Record record) throws NoStoreDelegateException {
            remote.add((BookmarkRecord)record);
        }

        @Override
        public void storeIncomplete() {
            super.storeIncomplete();
        }

        @Override
        public void storeDone() {
            super.storeDone();
        }
    }
}
