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
 *  Richard Newman <rnewman@mozilla.com>
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

package org.mozilla.gecko.sync.net;

import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;

import org.json.simple.JSONObject;
import org.mozilla.gecko.sync.CryptoRecord;
import org.mozilla.gecko.sync.ThreadPool;

import ch.boye.httpclientandroidlib.entity.StringEntity;

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
  protected SyncResourceDelegate makeResourceDelegate(SyncStorageRequest request) {
    return new SyncStorageRecordResourceDelegate(request);
  }

  /**
   * Helper for turning a JSON object into a payload.
   * @throws UnsupportedEncodingException
   */
  protected StringEntity jsonEntity(JSONObject body) throws UnsupportedEncodingException {
    StringEntity e = new StringEntity(body.toJSONString(), "UTF-8");
    e.setContentType("application/json");
    return e;
  }

  public void post(JSONObject body) {
    // Let's do this the trivial way for now.
    try {
      this.resource.post(jsonEntity(body));
    } catch (UnsupportedEncodingException e) {
      this.delegate.handleRequestError(e);
    }
  }

  public void put(JSONObject body) {
    // Let's do this the trivial way for now.
    try {
      this.resource.put(jsonEntity(body));
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
