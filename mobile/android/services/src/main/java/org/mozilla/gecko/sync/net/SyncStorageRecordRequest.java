/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;

import org.json.simple.JSONArray;
import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ThreadPool;

/**
 * Resource class that implements expected headers and processing for Sync.
 * Accepts a simplified delegate.
 *
 * Includes:
 * * Basic Auth headers (via Resource)
 * * Error responses:
 *   * 401
 *   * 503
 * * Headers:
 *   * Retry-After
 *   * X-Weave-Backoff
 *   * X-Backoff
 *   * X-Weave-Records?
 *   * ...
 * * Timeouts
 * * Network errors
 * * application/newlines
 * * JSON parsing
 * * Content-Type and Content-Length validation.
 */
public class SyncStorageRecordRequest extends SyncStorageRequest {

  public class SyncStorageRecordResourceDelegate extends SyncStorageResourceDelegate {
    SyncStorageRecordResourceDelegate(SyncStorageRequest request) {
      super(request);
    }
  }

  public SyncStorageRecordRequest(URI uri) {
    super(uri);
  }

  public SyncStorageRecordRequest(String url) throws URISyntaxException {
    this(new URI(url));
  }

  @Override
  protected BaseResourceDelegate makeResourceDelegate(SyncStorageRequest request) {
    return new SyncStorageRecordResourceDelegate(request);
  }

  @SuppressWarnings("unchecked")
  public void post(JSONObject body) {
    // Let's do this the trivial way for now.
    // Note that POSTs should be an array, so we wrap here.
    final JSONArray toPOST = new JSONArray();
    toPOST.add(body);
    try {
      this.resource.post(toPOST);
    } catch (UnsupportedEncodingException e) {
      this.delegate.handleRequestError(e);
    }
  }

  public void post(JSONArray body) {
    // Let's do this the trivial way for now.
    try {
      this.resource.post(body);
    } catch (UnsupportedEncodingException e) {
      this.delegate.handleRequestError(e);
    }
  }

  public void put(JSONObject body) {
    // Let's do this the trivial way for now.
    try {
      this.resource.put(body);
    } catch (UnsupportedEncodingException e) {
      this.delegate.handleRequestError(e);
    }
  }

  public void post(CryptoRecord record) {
    this.post(record.toJSONObject());
  }

  public void put(CryptoRecord record) {
    this.put(record.toJSONObject());
  }

  public void deferGet() {
    final SyncStorageRecordRequest self = this;
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        self.get();
      }});
  }

  public void deferPut(final JSONObject body) {
    final SyncStorageRecordRequest self = this;
    ThreadPool.run(new Runnable() {
      @Override
      public void run() {
        self.put(body);
      }});
  }
}
