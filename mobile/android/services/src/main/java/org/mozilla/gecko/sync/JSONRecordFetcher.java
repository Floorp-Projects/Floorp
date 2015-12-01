/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.delegates.JSONRecordFetchDelegate;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

/**
 * An object which fetches a chunk of JSON from a URI, using certain credentials,
 * and informs its delegate of the result.
 */
public class JSONRecordFetcher {
  private static final long DEFAULT_AWAIT_TIMEOUT_MSEC = 2 * 60 * 1000;   // Two minutes.
  private static final String LOG_TAG = "JSONRecordFetcher";

  protected final AuthHeaderProvider authHeaderProvider;
  protected final String uri;
  protected JSONRecordFetchDelegate delegate;

  public JSONRecordFetcher(final String uri, final AuthHeaderProvider authHeaderProvider) {
    if (uri == null) {
      throw new IllegalArgumentException("uri must not be null");
    }
    this.uri = uri;
    this.authHeaderProvider = authHeaderProvider;
  }

  protected String getURI() {
    return this.uri;
  }

  private class JSONFetchHandler implements SyncStorageRequestDelegate {

    // SyncStorageRequestDelegate methods for fetching.
    @Override
    public AuthHeaderProvider getAuthHeaderProvider() {
      return authHeaderProvider;
    }

    @Override
    public String ifUnmodifiedSince() {
      return null;
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
      if (response.wasSuccessful()) {
        try {
          delegate.handleSuccess(response.jsonObjectBody());
        } catch (Exception e) {
          handleRequestError(e);
        }
        return;
      }
      handleRequestFailure(response);
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      delegate.handleFailure(response);
    }

    @Override
    public void handleRequestError(Exception ex) {
      delegate.handleError(ex);
    }
  }

  public void fetch(final JSONRecordFetchDelegate delegate) {
    this.delegate = delegate;
    try {
      final SyncStorageRecordRequest r = new SyncStorageRecordRequest(this.getURI());
      r.delegate = new JSONFetchHandler();
      r.get();
    } catch (Exception e) {
      delegate.handleError(e);
    }
  }

  private class LatchedJSONRecordFetchDelegate implements JSONRecordFetchDelegate {
    public ExtendedJSONObject body = null;
    public Exception exception = null;
    private final CountDownLatch latch;

    public LatchedJSONRecordFetchDelegate(CountDownLatch latch) {
      this.latch = latch;
    }

    @Override
    public void handleFailure(SyncStorageResponse response) {
      this.exception = new HTTPFailureException(response);
      latch.countDown();
    }

    @Override
    public void handleError(Exception e) {
      this.exception = e;
      latch.countDown();
    }

    @Override
    public void handleSuccess(ExtendedJSONObject body) {
      this.body = body;
      latch.countDown();
    }
  }

  /**
   * Fetch the info record, blocking until it returns.
   * @return the info record.
   */
  public ExtendedJSONObject fetchBlocking() throws HTTPFailureException, Exception {
    CountDownLatch latch = new CountDownLatch(1);
    LatchedJSONRecordFetchDelegate delegate = new LatchedJSONRecordFetchDelegate(latch);
    this.delegate = delegate;
    this.fetch(delegate);

    // Sanity wait: the resource itself will time out and throw after two
    // minutes, so we just want to avoid coding errors causing us to block
    // endlessly.
    if (!latch.await(DEFAULT_AWAIT_TIMEOUT_MSEC, TimeUnit.MILLISECONDS)) {
      Logger.warn(LOG_TAG, "Interrupted fetching info record.");
      throw new InterruptedException("info fetch timed out.");
    }

    if (delegate.body != null) {
      return delegate.body;
    }

    if (delegate.exception != null) {
      throw delegate.exception;
    }

    throw new Exception("Unknown error.");
  }
}