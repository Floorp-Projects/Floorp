/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicLong;

import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.DelayedWorkTracker;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.Logger;
import org.mozilla.gecko.sync.UnexpectedJSONException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.net.SyncStorageCollectionRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.net.WBOCollectionRequestDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionGuidsSinceDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionWipeDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

import ch.boye.httpclientandroidlib.entity.ContentProducer;
import ch.boye.httpclientandroidlib.entity.EntityTemplate;

public class Server11RepositorySession extends RepositorySession {
  private static byte[] recordsStart;
  private static byte[] recordSeparator;
  private static byte[] recordsEnd;

  static {
    try {
      recordsStart    = "[\n".getBytes("UTF-8");
      recordSeparator = ",\n".getBytes("UTF-8");
      recordsEnd      = "\n]\n".getBytes("UTF-8");
    } catch (UnsupportedEncodingException e) {
      // These won't fail.
    }
  }

  public static final String LOG_TAG = "Server11Session";

  private static final int UPLOAD_BYTE_THRESHOLD = 1024 * 1024;    // 1MB.
  private static final int UPLOAD_ITEM_THRESHOLD = 50;
  private static final int PER_RECORD_OVERHEAD   = 2;              // Comma, newline.
  // {}, newlines, but we get to skip one record overhead.
  private static final int PER_BATCH_OVERHEAD    = 5 - PER_RECORD_OVERHEAD;

  /**
   * Return the X-Weave-Timestamp header from <code>response</code>, or the
   * current time if it is missing.
   * <p>
   * <b>Warning:</b> this can cause the timestamp of <code>response</code> to
   * cross domains (from server clock to local clock), which could cause records
   * to be skipped on account of clock drift. This should never happen, because
   * <i>every</i> server response should have a well-formed X-Weave-Timestamp
   * header.
   *
   * @param response
   *          The <code>SyncStorageResponse</code> to interrogate.
   * @return Normalized timestamp in milliseconds.
   */
  public static long getNormalizedTimestamp(SyncStorageResponse response) {
    long normalizedTimestamp = -1;
    try {
      normalizedTimestamp = response.normalizedWeaveTimestamp();
    } catch (NumberFormatException e) {
      Logger.warn(LOG_TAG, "Malformed X-Weave-Timestamp header received.", e);
    }
    if (-1 == normalizedTimestamp) {
      Logger.warn(LOG_TAG, "Computing stand-in timestamp from local clock. Clock drift could cause records to be skipped.");
      normalizedTimestamp = System.currentTimeMillis();
    }
    return normalizedTimestamp;
  }

  /**
   * Used to track outstanding requests, so that we can abort them as needed.
   */
  private Set<SyncStorageCollectionRequest> pending = Collections.synchronizedSet(new HashSet<SyncStorageCollectionRequest>());

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
    private DelayedWorkTracker workTracker = new DelayedWorkTracker();

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
    public String credentials() {
      return serverRepository.credentialsSource.credentials();
    }

    @Override
    public String ifUnmodifiedSince() {
      return null;
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
      Logger.debug(LOG_TAG, "Fetch done.");
      removeRequestFromPending();

      final long normalizedTimestamp = getNormalizedTimestamp(response);
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
  AtomicLong uploadTimestamp = new AtomicLong(0);

  private void bumpUploadTimestamp(long ts) {
    while (true) {
      long existing = uploadTimestamp.get();
      if (existing > ts) {
        return;
      }
      if (uploadTimestamp.compareAndSet(existing, ts)) {
        return;
      }
    }
  }

  public Server11RepositorySession(Repository repository) {
    super(repository);
    serverRepository = (Server11Repository) repository;
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

  protected Object recordsBufferMonitor = new Object();
  protected ArrayList<byte[]> recordsBuffer = new ArrayList<byte[]>();
  protected int byteCount = PER_BATCH_OVERHEAD;

  @Override
  public void store(Record record) throws NoStoreDelegateException {
    if (delegate == null) {
      throw new NoStoreDelegateException();
    }
    this.enqueue(record);
  }

  /**
   * Batch incoming records until some reasonable threshold (e.g., 50),
   * some size limit is hit (probably way less than 3MB!), or storeDone
   * is received.
   * @param record
   */
  protected void enqueue(Record record) {
    // JSONify and store the bytes, rather than the record.
    byte[] json = record.toJSONBytes();
    int delta   = json.length;
    synchronized (recordsBufferMonitor) {
      if ((delta + byteCount     > UPLOAD_BYTE_THRESHOLD) ||
          (recordsBuffer.size() >= UPLOAD_ITEM_THRESHOLD)) {

        // POST the existing contents, then enqueue.
        flush();
      }
      recordsBuffer.add(json);
      byteCount += PER_RECORD_OVERHEAD + delta;
    }
  }

  // Asynchronously upload records.
  // Must be locked!
  protected void flush() {
    if (recordsBuffer.size() > 0) {
      final ArrayList<byte[]> outgoing = recordsBuffer;
      RepositorySessionStoreDelegate uploadDelegate = this.delegate;
      storeWorkQueue.execute(new RecordUploadRunnable(uploadDelegate, outgoing, byteCount));

      recordsBuffer = new ArrayList<byte[]>();
      byteCount = PER_BATCH_OVERHEAD;
    }
  }

  @Override
  public void storeDone() {
    Logger.debug(LOG_TAG, "storeDone().");
    synchronized (recordsBufferMonitor) {
      flush();
      // Do this in a Runnable so that the timestamp is grabbed after any upload.
      final Runnable r = new Runnable() {
        @Override
        public void run() {
          synchronized (recordsBufferMonitor) {
            final long end = uploadTimestamp.get();
            Logger.debug(LOG_TAG, "Calling storeDone with " + end);
            storeDone(end);
          }
        }
      };
      storeWorkQueue.execute(r);
    }
  }

  /**
   * Make an HTTP request, and convert HTTP request delegate callbacks into
   * store callbacks within the context of this RepositorySession.
   *
   * @author rnewman
   *
   */
  protected class RecordUploadRunnable implements Runnable, SyncStorageRequestDelegate {

    public final String LOG_TAG = "RecordUploadRunnable";
    private ArrayList<byte[]> outgoing;
    private long byteCount;

    public RecordUploadRunnable(RepositorySessionStoreDelegate storeDelegate,
                                ArrayList<byte[]> outgoing,
                                long byteCount) {
      Logger.info(LOG_TAG, "Preparing record upload for " +
                  outgoing.size() + " records (" +
                  byteCount + " bytes).");
      this.outgoing  = outgoing;
      this.byteCount = byteCount;
    }

    @Override
    public String credentials() {
      return serverRepository.credentialsSource.credentials();
    }

    @Override
    public String ifUnmodifiedSince() {
      return null;
    }

    @Override
    public void handleRequestSuccess(SyncStorageResponse response) {
      Logger.info(LOG_TAG, "POST of " + outgoing.size() + " records done.");

      ExtendedJSONObject body;
      try {
        body = response.jsonObjectBody(); // jsonObjectBody() throws or returns non-null.
      } catch (Exception e) {
        Logger.error(LOG_TAG, "Got exception parsing POST success body.", e);
        // TODO
        return;
      }

      // Be defensive when logging timestamp.
      if (body.containsKey("modified")) {
        Long modified = body.getTimestamp("modified");
        if (modified != null) {
          Logger.debug(LOG_TAG, "POST request success. Modified timestamp: " + modified.longValue());
        } else {
          Logger.warn(LOG_TAG, "POST success body contains malformed 'modified': " + body.toJSONString());
        }
      } else {
        Logger.warn(LOG_TAG, "POST success body does not contain key 'modified': " + body.toJSONString());
      }

      try {
        JSONArray          success = body.getArray("success");
        ExtendedJSONObject failed  = body.getObject("failed");
        if ((success != null) &&
            (success.size() > 0)) {
          Logger.debug(LOG_TAG, "Successful records: " + success.toString());
          // TODO: how do we notify without the whole record?

          long normalizedTimestamp = getNormalizedTimestamp(response);
          Logger.debug(LOG_TAG, "Passing back upload X-Weave-Timestamp: " + normalizedTimestamp);
          bumpUploadTimestamp(normalizedTimestamp);
        }
        if ((failed != null) &&
            (failed.object.size() > 0)) {
          Logger.debug(LOG_TAG, "Failed records: " + failed.object.toString());
          // TODO: notify.
        }
      } catch (UnexpectedJSONException e) {
        Logger.error(LOG_TAG, "Got exception processing success/failed in POST success body.", e);
        // TODO
        return;
      }
      Logger.info(LOG_TAG, "POST of " + outgoing.size() + " records handled.");
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      // TODO: ensure that delegate methods don't get called more than once.
      // TODO: call session.interpretHTTPFailure.
      this.handleRequestError(new HTTPFailureException(response));
    }

    @Override
    public void handleRequestError(final Exception ex) {
      Logger.warn(LOG_TAG, "Got request error: " + ex, ex);
      delegate.onRecordStoreFailed(ex);
    }

    public class ByteArraysContentProducer implements ContentProducer {

      ArrayList<byte[]> outgoing;
      public ByteArraysContentProducer(ArrayList<byte[]> arrays) {
        outgoing = arrays;
      }

      @Override
      public void writeTo(OutputStream outstream) throws IOException {
        int count = outgoing.size();
        outstream.write(recordsStart);
        outstream.write(outgoing.get(0));
        for (int i = 1; i < count; ++i) {
          outstream.write(recordSeparator);
          outstream.write(outgoing.get(i));
        }
        outstream.write(recordsEnd);
      }
    }

    public class ByteArraysEntity extends EntityTemplate {
      private long count;
      public ByteArraysEntity(ArrayList<byte[]> arrays, long totalBytes) {
        super(new ByteArraysContentProducer(arrays));
        this.count = totalBytes;
        this.setContentType("application/json");
        // charset is set in BaseResource.
      }

      @Override
      public long getContentLength() {
        return count;
      }

      @Override
      public boolean isRepeatable() {
        return true;
      }
    }

    public ByteArraysEntity getBodyEntity() {
      ByteArraysEntity body = new ByteArraysEntity(outgoing, byteCount);
      return body;
    }

    @Override
    public void run() {
      if (outgoing == null ||
          outgoing.size() == 0) {
        Logger.debug(LOG_TAG, "No items: RecordUploadRunnable returning immediately.");
        return;
      }

      URI u = serverRepository.collectionURI();
      SyncStorageRequest request = new SyncStorageRequest(u);

      request.delegate = this;

      // We don't want the task queue to proceed until this request completes.
      // Fortunately, BaseResource is currently synchronous.
      // If that ever changes, you'll need to block here.
      ByteArraysEntity body = getBodyEntity();
      request.post(body);
    }
  }
}
