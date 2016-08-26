/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.DelayedWorkTracker;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncResponse;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.net.WBOCollectionRequestDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;
import org.mozilla.gecko.sync.repositories.uploaders.BatchingUploader;

public class Server11RepositorySession extends RepositorySession {
  public static final String LOG_TAG = "Server11Session";

  /**
   * Used to track outstanding requests, so that we can abort them as needed.
   */
  private final Set<SyncStorageCollectionRequest> pending = Collections.synchronizedSet(new HashSet<SyncStorageCollectionRequest>());

  @Override
  public void abort() {
    super.abort();
    for (SyncStorageCollectionRequest request : pending) {
      request.abort();
    }
    pending.clear();
  }

  /**
   * Convert HTTP request delegate callbacks into fetch callbacks within the
   * context of this RepositorySession.
   *
   * @author rnewman
   *
   */
  public class RequestFetchDelegateAdapter extends WBOCollectionRequestDelegate {
    RepositorySessionFetchRecordsDelegate delegate;
    private final DelayedWorkTracker workTracker = new DelayedWorkTracker();

    // So that we can clean up.
    private SyncStorageCollectionRequest request;

    public void setRequest(SyncStorageCollectionRequest request) {
      this.request = request;
    }
    private void removeRequestFromPending() {
      if (this.request == null) {
        return;
      }
      pending.remove(this.request);
      this.request = null;
    }

    public RequestFetchDelegateAdapter(RepositorySessionFetchRecordsDelegate delegate) {
      this.delegate = delegate;
    }

    @Override
    public AuthHeaderProvider getAuthHeaderProvider() {
      return serverRepository.getAuthHeaderProvider();
    }

    @Override
    public String ifUnmodifiedSince() {
      return null;
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
      Logger.debug(LOG_TAG, "Fetch done.");
      removeRequestFromPending();

      // This will change overall and will use X_LAST_MODIFIED in Bug 730142.
      final long normalizedTimestamp = response.normalizedTimestampForHeader(SyncResponse.X_WEAVE_TIMESTAMP);
      Logger.debug(LOG_TAG, "Fetch completed. Timestamp is " + normalizedTimestamp);

      // When we're done processing other events, finish.
      workTracker.delayWorkItem(new Runnable() {
        @Override
        public void run() {
          Logger.debug(LOG_TAG, "Delayed onFetchCompleted running.");
          // TODO: verify number of returned records.
          delegate.onFetchCompleted(normalizedTimestamp);
        }
      });
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      // TODO: ensure that delegate methods don't get called more than once.
      this.handleRequestError(new HTTPFailureException(response));
    }

    @Override
    public void handleRequestError(final Exception ex) {
      removeRequestFromPending();
      Logger.warn(LOG_TAG, "Got request error.", ex);
      // When we're done processing other events, finish.
      workTracker.delayWorkItem(new Runnable() {
        @Override
        public void run() {
          Logger.debug(LOG_TAG, "Running onFetchFailed.");
          delegate.onFetchFailed(ex, null);
        }
      });
    }

    @Override
    public void handleWBO(CryptoRecord record) {
      workTracker.incrementOutstanding();
      try {
        delegate.onFetchedRecord(record);
      } catch (Exception ex) {
        Logger.warn(LOG_TAG, "Got exception calling onFetchedRecord with WBO.", ex);
        // TODO: handle this better.
        throw new RuntimeException(ex);
      } finally {
        workTracker.decrementOutstanding();
      }
    }

    // TODO: this implies that we've screwed up our inheritance chain somehow.
    @Override
    public KeyBundle keyBundle() {
      return null;
    }
  }

  Server11Repository serverRepository;
  private BatchingUploader uploader;

  public Server11RepositorySession(Repository repository) {
    super(repository);
    serverRepository = (Server11Repository) repository;
  }

  public Server11Repository getServerRepository() {
    return serverRepository;
  }

  @Override
  public void setStoreDelegate(RepositorySessionStoreDelegate delegate) {
    this.delegate = delegate;

    // Now that we have the delegate, we can initialize our uploader.
    this.uploader = new BatchingUploader(this, storeWorkQueue, delegate);
  }

  private String flattenIDs(String[] guids) {
    // Consider using Utils.toDelimitedString if and when the signature changes
    // to Collection<String> guids.
    if (guids.length == 0) {
      return "";
    }
    if (guids.length == 1) {
      return guids[0];
    }
    StringBuilder b = new StringBuilder();
    for (String guid : guids) {
      b.append(guid);
      b.append(",");
    }
    return b.substring(0, b.length() - 1);
  }

  @Override
  public void guidsSince(long timestamp,
                         RepositorySessionGuidsSinceDelegate delegate) {
    // TODO Auto-generated method stub

  }

  protected void fetchWithParameters(long newer,
                                     long limit,
                                     boolean full,
                                     String sort,
                                     String ids,
                                     RequestFetchDelegateAdapter delegate)
                                         throws URISyntaxException {

    URI collectionURI = serverRepository.collectionURI(full, newer, limit, sort, ids);
    SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(collectionURI);
    request.delegate = delegate;

    // So it can clean up.
    delegate.setRequest(request);
    pending.add(request);
    request.get();
  }

  public void fetchSince(long timestamp, long limit, String sort, RepositorySessionFetchRecordsDelegate delegate) {
    try {
      this.fetchWithParameters(timestamp, limit, true, sort, null, new RequestFetchDelegateAdapter(delegate));
    } catch (URISyntaxException e) {
      delegate.onFetchFailed(e, null);
    }
  }

  @Override
  public void fetchSince(long timestamp,
                         RepositorySessionFetchRecordsDelegate delegate) {
    try {
      long limit = serverRepository.getDefaultFetchLimit();
      String sort = serverRepository.getDefaultSort();
      this.fetchWithParameters(timestamp, limit, true, sort, null, new RequestFetchDelegateAdapter(delegate));
    } catch (URISyntaxException e) {
      delegate.onFetchFailed(e, null);
    }
  }

  @Override
  public void fetchAll(RepositorySessionFetchRecordsDelegate delegate) {
    this.fetchSince(-1, delegate);
  }

  @Override
  public void fetch(String[] guids,
                    RepositorySessionFetchRecordsDelegate delegate) {
    // TODO: watch out for URL length limits!
    try {
      String ids = flattenIDs(guids);
      this.fetchWithParameters(-1, -1, true, "index", ids, new RequestFetchDelegateAdapter(delegate));
    } catch (URISyntaxException e) {
      delegate.onFetchFailed(e, null);
    }
  }

  @Override
  public void wipe(RepositorySessionWipeDelegate delegate) {
    if (!isActive()) {
      delegate.onWipeFailed(new InactiveSessionException(null));
      return;
    }
    // TODO: implement wipe.
  }

  @Override
  public void store(Record record) throws NoStoreDelegateException {
    if (delegate == null) {
      throw new NoStoreDelegateException();
    }

    // If delegate was set, this shouldn't happen.
    if (uploader == null) {
      throw new IllegalStateException("Uploader haven't been initialized");
    }

    uploader.process(record);
  }

  @Override
  public void storeDone() {
    Logger.debug(LOG_TAG, "storeDone().");

    // If delegate was set, this shouldn't happen.
    if (uploader == null) {
      throw new IllegalStateException("Uploader haven't been initialized");
    }

    uploader.noMoreRecordsToUpload();
  }

  @Override
  public boolean dataAvailable() {
    return serverRepository.updateNeeded(getLastSyncTimestamp());
  }
}
