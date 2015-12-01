/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.Queue;
import java.util.concurrent.Executor;

import org.mozilla.gecko.background.ReadingListConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.reading.ReadingListResponse.ResponseFactory;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BaseResource;
import org.mozilla.gecko.sync.net.BaseResourceDelegate;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.net.MozResponse;
import org.mozilla.gecko.sync.net.Resource;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;

/**
 * This client exposes an API for the reading list service, documented at
 * https://github.com/mozilla-services/readinglist/
 */
public class ReadingListClient {
  static final String LOG_TAG = ReadingListClient.class.getSimpleName();
  private final AuthHeaderProvider auth;

  private final URI articlesURI;              // .../articles
  private final URI articlesBaseURI;          // .../articles/

  /**
   * Use a {@link BasicAuthHeaderProvider} for testing, and an FxA OAuth provider for the real service.
   */
  public ReadingListClient(final URI serviceURI, final AuthHeaderProvider auth) {
    this.articlesURI = serviceURI.resolve("articles");
    this.articlesBaseURI = serviceURI.resolve("articles/");
    this.auth = auth;
  }

  private BaseResource getRelativeArticleResource(final String rel) {
    return new BaseResource(this.articlesBaseURI.resolve(rel));
  }

  private static final class DelegatingUploadResourceDelegate extends UploadResourceDelegate<ReadingListRecordResponse> {
    private final ClientReadingListRecord         up;
    private final ReadingListRecordUploadDelegate uploadDelegate;

    DelegatingUploadResourceDelegate(Resource resource,
                                     AuthHeaderProvider auth,
                                     ResponseFactory<ReadingListRecordResponse> factory,
                                     ClientReadingListRecord up,
                                     ReadingListRecordUploadDelegate uploadDelegate) {
      super(resource, auth, factory);
      this.up = up;
      this.uploadDelegate = uploadDelegate;
    }

    @Override
    void onFailure(MozResponse response) {
      Logger.warn(LOG_TAG, "Upload got failure response " + response.httpResponse().getStatusLine());
      response.logResponseBody(LOG_TAG);
      if (response.getStatusCode() == 400) {
        // Error response.
        uploadDelegate.onBadRequest(up, response);
      } else {
        uploadDelegate.onFailure(up, response);
      }
    }

    @Override
    void onFailure(Exception ex) {
      Logger.warn(LOG_TAG, "Upload failed.", ex);
      uploadDelegate.onFailure(up, ex);
    }

    @Override
    void onSuccess(ReadingListRecordResponse response) {
      Logger.debug(LOG_TAG, "Upload: onSuccess: " + response.httpResponse().getStatusLine());
      final ServerReadingListRecord down;
      try {
        down = response.getRecord();
        Logger.debug(LOG_TAG, "Upload succeeded. Got GUID " + down.getGUID());
      } catch (Exception e) {
        uploadDelegate.onFailure(up, e);
        return;
      }

      uploadDelegate.onSuccess(up, response, down);
    }

    @Override
    void onSeeOther(ReadingListRecordResponse response) {
      uploadDelegate.onConflict(up, response);
    }
  }

  private static abstract class ReadingListResourceDelegate<T extends ReadingListResponse> extends BaseResourceDelegate {
    private final ReadingListResponse.ResponseFactory<T> factory;
    private final AuthHeaderProvider auth;

    public ReadingListResourceDelegate(Resource resource, AuthHeaderProvider auth, ReadingListResponse.ResponseFactory<T> factory) {
      super(resource);
      this.auth = auth;
      this.factory = factory;
    }

    abstract void onSuccess(T response);
    abstract void onNonSuccess(T response);
    abstract void onFailure(MozResponse response);
    abstract void onFailure(Exception ex);

    @Override
    public void handleHttpResponse(HttpResponse response) {
      final T resp = factory.getResponse(response);
      if (resp.wasSuccessful()) {
        onSuccess(resp);
      } else {
        onNonSuccess(resp);
      }
    }

    @Override
    public void handleTransportException(GeneralSecurityException e) {
      onFailure(e);
    }

    @Override
    public void handleHttpProtocolException(ClientProtocolException e) {
      onFailure(e);
    }

    @Override
    public void handleHttpIOException(IOException e) {
      onFailure(e);
    }

    @Override
    public String getUserAgent() {
      return ReadingListConstants.USER_AGENT;
    }

    @Override
    public AuthHeaderProvider getAuthHeaderProvider() {
      return auth;
    }

    @Override
    public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
    }
  }

  /**
   * An intermediate delegate class that handles all of the shared storage behavior,
   * such as handling If-Modified-Since.
   */
  private static abstract class StorageResourceDelegate<T extends ReadingListResponse> extends ReadingListResourceDelegate<T> {
    private final long ifModifiedSince;

    public StorageResourceDelegate(Resource resource,
                                   AuthHeaderProvider auth,
                                   ReadingListResponse.ResponseFactory<T> factory,
                                   long ifModifiedSince) {
      super(resource, auth, factory);
      this.ifModifiedSince = ifModifiedSince;
    }

    @Override
    public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
      if (ifModifiedSince != -1L) {
        // TODO: format?
        request.addHeader("If-Modified-Since", "" + ifModifiedSince);
      }
      super.addHeaders(request, client);
    }
  }

  /**
   * Wraps the @{link ReadingListRecordDelegate} interface to yield a {@link StorageResourceDelegate}.
   */
  private static abstract class RecordResourceDelegate<T extends ReadingListResponse> extends StorageResourceDelegate<T> {
    protected final ReadingListRecordDelegate recordDelegate;

    public RecordResourceDelegate(Resource resource,
                                  AuthHeaderProvider auth,
                                  ReadingListRecordDelegate recordDelegate,
                                  ReadingListResponse.ResponseFactory<T> factory,
                                  long ifModifiedSince) {
      super(resource, auth, factory, ifModifiedSince);
      this.recordDelegate = recordDelegate;
    }

    abstract void onNotFound(ReadingListResponse resp);

    @Override
    void onNonSuccess(T resp) {
      Logger.debug(LOG_TAG, "Got non-success record response " + resp.getStatusCode());
      resp.logResponseBody(LOG_TAG);

      switch (resp.getStatusCode()) {
      case 304:
        onNotModified(resp);
        break;
      case 404:
        onNotFound(resp);
        break;
      default:
        onFailure(resp);
      }
    }

    @Override
    void onFailure(MozResponse response) {
      recordDelegate.onFailure(response);
    }

    @Override
    void onFailure(Exception ex) {
      recordDelegate.onFailure(ex);
    }

    void onNotModified(T resp) {
      recordDelegate.onComplete(resp);
    }
  }

  private static final class SingleRecordResourceDelegate extends RecordResourceDelegate<ReadingListRecordResponse> {
    private final String guid;

    SingleRecordResourceDelegate(Resource resource,
                                 AuthHeaderProvider auth,
                                 ReadingListRecordDelegate recordDelegate,
                                 ResponseFactory<ReadingListRecordResponse> factory,
                                 long ifModifiedSince, String guid) {
      super(resource, auth, recordDelegate, factory, ifModifiedSince);
      this.guid = guid;
    }

    @Override
    void onSuccess(ReadingListRecordResponse response) {
      final ServerReadingListRecord record;
      try {
        record = response.getRecord();
      } catch (Exception e) {
        recordDelegate.onFailure(e);
        return;
      }

      recordDelegate.onRecordReceived(record);
      recordDelegate.onComplete(response);
    }

    @Override
    void onNotFound(ReadingListResponse resp) {
      recordDelegate.onRecordMissingOrDeleted(guid, resp);
    }
  }

  private static final class MultipleRecordResourceDelegate extends RecordResourceDelegate<ReadingListStorageResponse> {
    MultipleRecordResourceDelegate(Resource resource,
                                   AuthHeaderProvider auth,
                                   ReadingListRecordDelegate recordDelegate,
                                   ResponseFactory<ReadingListStorageResponse> factory,
                                   long ifModifiedSince) {
      super(resource, auth, recordDelegate, factory, ifModifiedSince);
    }

    @Override
    void onSuccess(ReadingListStorageResponse response) {
      try {
        final Iterable<ServerReadingListRecord> records = response.getRecords();
        for (ServerReadingListRecord readingListRecord : records) {
          recordDelegate.onRecordReceived(readingListRecord);
        }
      } catch (Exception e) {
        recordDelegate.onFailure(e);
        return;
      }

      recordDelegate.onComplete(response);
    }

    @Override
    void onNotFound(ReadingListResponse resp) {
      // Should not occur against articlesURI root.
      recordDelegate.onFailure(resp);
    }
  }

  private static abstract class UploadResourceDelegate<T extends ReadingListResponse> extends StorageResourceDelegate<T> {
    public UploadResourceDelegate(Resource resource,
                                  AuthHeaderProvider auth,
                                  ReadingListResponse.ResponseFactory<T> factory,
                                  long ifModifiedSince) {
      super(resource, auth, factory, ifModifiedSince);
    }

    public UploadResourceDelegate(Resource resource,
                                  AuthHeaderProvider auth,
                                  ReadingListResponse.ResponseFactory<T> factory) {
      this(resource, auth, factory, -1L);
    }

    @Override
    void onNonSuccess(T resp) {
      if (resp.getStatusCode() == 303) {
        onSeeOther(resp);
        return;
      }
      onFailure(resp);
    }

    abstract void onSeeOther(T resp);
  }


  /**
   * Recursively calls `patch` with items from the queue, delivering callbacks
   * to the provided delegate. Calls `onBatchDone` when the queue is exhausted.
   *
   * Uses the provided executor to flatten the recursive call stack.
   */
  private abstract class BatchingUploadDelegate implements ReadingListRecordUploadDelegate {
    private final Queue<ClientReadingListRecord>  queue;
    private final ReadingListRecordUploadDelegate batchUploadDelegate;
    private final Executor                        executor;

    BatchingUploadDelegate(Queue<ClientReadingListRecord> queue,
                           ReadingListRecordUploadDelegate batchUploadDelegate,
                           Executor executor) {
      this.queue = queue;
      this.batchUploadDelegate = batchUploadDelegate;
      this.executor = executor;
    }

    abstract void again(ClientReadingListRecord record);

    void next() {
      final ClientReadingListRecord record = queue.poll();
      executor.execute(new Runnable() {
        @Override
        public void run() {
          if (record == null) {
            batchUploadDelegate.onBatchDone();
            return;
          }

          again(record);
        }
      });
    }

    @Override
    public void onSuccess(ClientReadingListRecord up,
                          ReadingListRecordResponse response,
                          ServerReadingListRecord down) {
      batchUploadDelegate.onSuccess(up, response, down);
      next();
    }

    @Override
    public void onInvalidUpload(ClientReadingListRecord up,
                                ReadingListResponse response) {
      batchUploadDelegate.onInvalidUpload(up, response);
      next();
    }

    @Override
    public void onFailure(ClientReadingListRecord up, MozResponse response) {
      batchUploadDelegate.onFailure(up, response);
      next();
    }

    @Override
    public void onFailure(ClientReadingListRecord up, Exception ex) {
      batchUploadDelegate.onFailure(up, ex);
      next();
    }

    @Override
    public void onConflict(ClientReadingListRecord up,
                           ReadingListResponse response) {
      batchUploadDelegate.onConflict(up, response);
      next();
    }

    @Override
    public void onBadRequest(ClientReadingListRecord up, MozResponse response) {
      batchUploadDelegate.onBadRequest(up, response);
      next();
    }

    @Override
    public void onBatchDone() {
      // This should never occur, but if it does, pass through.
      batchUploadDelegate.onBatchDone();
    }
  }

  private class PostBatchingUploadDelegate extends BatchingUploadDelegate {
    PostBatchingUploadDelegate(Queue<ClientReadingListRecord> queue,
                               ReadingListRecordUploadDelegate batchUploadDelegate,
                               Executor executor) {
      super(queue, batchUploadDelegate, executor);
    }

    @Override
    void again(ClientReadingListRecord record) {
      add(record, PostBatchingUploadDelegate.this);
    }
  }

  private class PatchBatchingUploadDelegate extends BatchingUploadDelegate {
    PatchBatchingUploadDelegate(Queue<ClientReadingListRecord> queue,
                                ReadingListRecordUploadDelegate batchUploadDelegate,
                                Executor executor) {
      super(queue, batchUploadDelegate, executor);
    }

    @Override
    void again(ClientReadingListRecord record) {
      patch(record, PatchBatchingUploadDelegate.this);
    }
  }

  private class DeleteBatchingDelegate implements ReadingListDeleteDelegate {
    private final Queue<String> queue;
    private final ReadingListDeleteDelegate batchDeleteDelegate;
    private final Executor executor;

    DeleteBatchingDelegate(Queue<String> guids,
                           ReadingListDeleteDelegate batchDeleteDelegate,
                           Executor executor) {
      this.queue = guids;
      this.batchDeleteDelegate = batchDeleteDelegate;
      this.executor = executor;
    }

    void next() {
      final String guid = queue.poll();
      executor.execute(new Runnable() {
        @Override
        public void run() {
          if (guid == null) {
            batchDeleteDelegate.onBatchDone();
            return;
          }

          again(guid);
        }
      });
    }

    void again(String guid) {
      delete(guid, DeleteBatchingDelegate.this, -1L);
    }

    @Override
    public void onSuccess(ReadingListRecordResponse response,
                          ReadingListRecord record) {
      batchDeleteDelegate.onSuccess(response, record);
      next();
    }

    @Override
    public void onPreconditionFailed(String guid, MozResponse response) {
      batchDeleteDelegate.onPreconditionFailed(guid, response);
      next();
    }

    @Override
    public void onRecordMissingOrDeleted(String guid, MozResponse response) {
      batchDeleteDelegate.onRecordMissingOrDeleted(guid, response);
      next();
    }

    @Override
    public void onFailure(Exception e) {
      batchDeleteDelegate.onFailure(e);
      next();
    }

    @Override
    public void onFailure(MozResponse response) {
      batchDeleteDelegate.onFailure(response);
      next();
    }

    @Override
    public void onBatchDone() {
      // This should never occur, but if it does, pass through.
      batchDeleteDelegate.onBatchDone();
    }
  }

  // Deliberately declare `delegate` non-final so we can't capture it below. We prefer
  // to use `recordDelegate` explicitly.
  public void getOne(final String guid, ReadingListRecordDelegate delegate, final long ifModifiedSince) {
    final BaseResource r = getRelativeArticleResource(guid);
    r.delegate = new SingleRecordResourceDelegate(r, auth, delegate, ReadingListRecordResponse.FACTORY, ifModifiedSince, guid);
    if (ReadingListConstants.DEBUG) {
      Logger.info(LOG_TAG, "Getting record " + guid);
    }
    r.get();
  }

  // Deliberately declare `delegate` non-final so we can't capture it below. We prefer
  // to use `recordDelegate` explicitly.
  public void getAll(final FetchSpec spec, ReadingListRecordDelegate delegate, final long ifModifiedSince) throws URISyntaxException {
    final BaseResource r = new BaseResource(spec.getURI(this.articlesURI));
    r.delegate = new MultipleRecordResourceDelegate(r, auth, delegate, ReadingListStorageResponse.FACTORY, ifModifiedSince);
    if (ReadingListConstants.DEBUG) {
      Logger.info(LOG_TAG, "Getting all records from " + r.getURIString());
    }
    r.get();
  }

  /**
   * Mutates the provided queue.
   */
  public void patch(final Queue<ClientReadingListRecord> queue, final Executor executor, final ReadingListRecordUploadDelegate batchUploadDelegate) {
    if (queue.isEmpty()) {
      batchUploadDelegate.onBatchDone();
      return;
    }

    final ReadingListRecordUploadDelegate uploadDelegate = new PatchBatchingUploadDelegate(queue, batchUploadDelegate, executor);

    patch(queue.poll(), uploadDelegate);
  }

  public void patch(final ClientReadingListRecord up, final ReadingListRecordUploadDelegate uploadDelegate) {
    final String guid = up.getGUID();
    if (guid == null) {
      uploadDelegate.onFailure(up, new IllegalArgumentException("Supplied record must have a GUID."));
      return;
    }

    final BaseResource r = getRelativeArticleResource(guid);
    r.delegate = new DelegatingUploadResourceDelegate(r, auth, ReadingListRecordResponse.FACTORY, up,
                                                      uploadDelegate);

    final ExtendedJSONObject body = up.toJSON();
    if (ReadingListConstants.DEBUG) {
      Logger.info(LOG_TAG, "Patching record " + guid + ": " + body.toJSONString());
    }
    r.patch(body);
  }

  /**
   * Mutates the provided queue.
   */
  public void add(final Queue<ClientReadingListRecord> queue, final Executor executor, final ReadingListRecordUploadDelegate batchUploadDelegate) {
    if (queue.isEmpty()) {
      batchUploadDelegate.onBatchDone();
      return;
    }

    final ReadingListRecordUploadDelegate uploadDelegate = new PostBatchingUploadDelegate(queue, batchUploadDelegate, executor);

    add(queue.poll(), uploadDelegate);
  }

  public void add(final ClientReadingListRecord up, final ReadingListRecordUploadDelegate uploadDelegate) {
    final BaseResource r = new BaseResource(this.articlesURI);
    r.delegate = new DelegatingUploadResourceDelegate(r, auth, ReadingListRecordResponse.FACTORY, up,
                                                      uploadDelegate);

    final ExtendedJSONObject body = up.toJSON();
    if (ReadingListConstants.DEBUG) {
      Logger.info(LOG_TAG, "Uploading new record: " + body.toJSONString());
    }
    r.post(body);
  }

  public void delete(final Queue<String> guids, final Executor executor, final ReadingListDeleteDelegate batchDeleteDelegate) {
    if (guids.isEmpty()) {
      batchDeleteDelegate.onBatchDone();
      return;
    }

    final ReadingListDeleteDelegate deleteDelegate = new DeleteBatchingDelegate(guids, batchDeleteDelegate, executor);

    delete(guids.poll(), deleteDelegate, -1L);
  }

  public void delete(final String guid, final ReadingListDeleteDelegate delegate, final long ifUnmodifiedSince) {
    final BaseResource r = getRelativeArticleResource(guid);

    // If If-Unmodified-Since is provided, and the record has been modified,
    // we'll receive a 412 Precondition Failed.
    // If the record is missing or already deleted, a 404 will be returned.
    // Otherwise, the response will be the deleted record.
    r.delegate = new ReadingListResourceDelegate<ReadingListRecordResponse>(r, auth, ReadingListRecordResponse.FACTORY) {
      @Override
      public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
        if (ifUnmodifiedSince != -1) {
          request.addHeader("If-Unmodified-Since", "" + ifUnmodifiedSince);
        }
        super.addHeaders(request, client);
      }

      @Override
      void onFailure(MozResponse response) {
        switch (response.getStatusCode()) {
        case 412:
          delegate.onPreconditionFailed(guid, response);
          return;
        }
        delegate.onFailure(response);
      }

      @Override
      void onSuccess(ReadingListRecordResponse response) {
        final ReadingListRecord record;
        try {
          record = response.getRecord();
        } catch (Exception e) {
          delegate.onFailure(e);
          return;
        }

        delegate.onSuccess(response, record);
      }

      @Override
      void onFailure(Exception ex) {
        delegate.onFailure(ex);
      }

      @Override
      void onNonSuccess(ReadingListRecordResponse response) {
        if (response.getStatusCode() == 404) {
          // Already deleted!
          delegate.onRecordMissingOrDeleted(guid, response);
        }
      }
    };

    if (ReadingListConstants.DEBUG) {
      Logger.debug(LOG_TAG, "Deleting " + r.getURIString());
    }
    r.delete();
  }

  // TODO: modified times etc.
  public void wipe(final ReadingListWipeDelegate delegate) {
    Logger.info(LOG_TAG, "Wiping server.");
    final BaseResource r = new BaseResource(this.articlesURI);

    r.delegate = new ReadingListResourceDelegate<ReadingListStorageResponse>(r, auth, ReadingListStorageResponse.FACTORY) {

      @Override
      void onSuccess(ReadingListStorageResponse response) {
        Logger.info(LOG_TAG, "Wipe succeded.");
        delegate.onSuccess(response);
      }

      @Override
      void onNonSuccess(ReadingListStorageResponse response) {
        Logger.warn(LOG_TAG, "Wipe failed: " + response.getStatusCode());
        onFailure(response);
      }

      @Override
      void onFailure(MozResponse response) {
        Logger.warn(LOG_TAG, "Wipe failed: " + response.getStatusCode());
        delegate.onFailure(response);
      }

      @Override
      void onFailure(Exception ex) {
        Logger.warn(LOG_TAG, "Wipe failed.", ex);
        delegate.onFailure(ex);
      }
    };

    r.delete();
  }
}
