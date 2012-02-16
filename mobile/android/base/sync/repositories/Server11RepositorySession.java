/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Android Sync Client.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Richard Newman <rnewman@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.gecko.sync.repositories;

import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Date;
import java.util.concurrent.atomic.AtomicLong;

import org.json.simple.JSONArray;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.DelayedWorkTracker;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.HTTPFailureException;
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

import android.util.Log;
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
   * Convert HTTP request delegate callbacks into fetch callbacks within the
   * context of this RepositorySession.
   *
   * @author rnewman
   *
   */
  public class RequestFetchDelegateAdapter extends WBOCollectionRequestDelegate {
    RepositorySessionFetchRecordsDelegate delegate;
    private DelayedWorkTracker workTracker = new DelayedWorkTracker();

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
      Log.i(LOG_TAG, "Fetch done.");

      long normalizedTimestamp = -1;
      try {
        normalizedTimestamp = response.normalizedWeaveTimestamp();
      } catch (NumberFormatException e) {
        Log.w(LOG_TAG, "Malformed X-Weave-Timestamp header received.", e);
      }
      if (-1 == normalizedTimestamp) {
        Log.w(LOG_TAG, "Computing stand-in timestamp from local clock. Clock drift could cause records to be skipped.");
        normalizedTimestamp = new Date().getTime();
      }

      Log.d(LOG_TAG, "Fetch completed. Timestamp is " + normalizedTimestamp);
      final long ts = normalizedTimestamp;

      // When we're done processing other events, finish.
      workTracker.delayWorkItem(new Runnable() {
        @Override
        public void run() {
          Log.d(LOG_TAG, "Delayed onFetchCompleted running.");
          // TODO: verify number of returned records.
          delegate.onFetchCompleted(ts);
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
      Log.i(LOG_TAG, "Got request error.", ex);
      // When we're done processing other events, finish.
      workTracker.delayWorkItem(new Runnable() {
        @Override
        public void run() {
          Log.i(LOG_TAG, "Running onFetchFailed.");
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
        Log.i(LOG_TAG, "Got exception calling onFetchedRecord with WBO.", ex);
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
                                     SyncStorageRequestDelegate delegate)
                                         throws URISyntaxException {

    URI collectionURI = serverRepository.collectionURI(full, newer, limit, sort, ids);
    SyncStorageCollectionRequest request = new SyncStorageCollectionRequest(collectionURI);
    request.delegate = delegate;
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
    synchronized (recordsBufferMonitor) {
      flush();
      storeDone(uploadTimestamp.get());
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
      Log.i(LOG_TAG, "Preparing RecordUploadRunnable for " +
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
      Log.i(LOG_TAG, "POST of " + outgoing.size() + " records done.");

      ExtendedJSONObject body;
      try {
        body = response.jsonObjectBody();
      } catch (Exception e) {
        Log.e(LOG_TAG, "Got exception parsing POST success body.", e);
        // TODO
        return;
      }
      long modified = body.getTimestamp("modified");
      Log.i(LOG_TAG, "POST request success. Modified timestamp: " + modified);

      try {
        JSONArray          success = body.getArray("success");
        ExtendedJSONObject failed  = body.getObject("failed");
        if ((success != null) &&
            (success.size() > 0)) {
          Log.d(LOG_TAG, "Successful records: " + success.toString());
          // TODO: how do we notify without the whole record?

          long ts = response.normalizedWeaveTimestamp();
          Log.d(LOG_TAG, "Passing back upload X-Weave-Timestamp: " + ts);
          bumpUploadTimestamp(ts);
        }
        if ((failed != null) &&
            (failed.object.size() > 0)) {
          Log.d(LOG_TAG, "Failed records: " + failed.object.toString());
          // TODO: notify.
        }
      } catch (UnexpectedJSONException e) {
        Log.e(LOG_TAG, "Got exception processing success/failed in POST success body.", e);
        // TODO
        return;
      }
    }

    @Override
    public void handleRequestFailure(SyncStorageResponse response) {
      // TODO: ensure that delegate methods don't get called more than once.
      // TODO: call session.interpretHTTPFailure.
      this.handleRequestError(new HTTPFailureException(response));
    }

    @Override
    public void handleRequestError(final Exception ex) {
      Log.i(LOG_TAG, "Got request error: " + ex, ex);
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
        Log.i(LOG_TAG, "No items: RecordUploadRunnable returning immediately.");
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
