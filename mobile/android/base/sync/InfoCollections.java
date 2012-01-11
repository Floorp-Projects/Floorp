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
 * Richard Newman <rnewman@mozilla.com>
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

package org.mozilla.gecko.sync;

import java.io.IOException;
import java.net.URISyntaxException;
import java.util.HashMap;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.delegates.InfoCollectionsDelegate;
import org.mozilla.gecko.sync.net.SyncStorageRecordRequest;
import org.mozilla.gecko.sync.net.SyncStorageRequestDelegate;
import org.mozilla.gecko.sync.net.SyncStorageResponse;

import android.util.Log;

public class InfoCollections implements SyncStorageRequestDelegate {
  private static final String LOG_TAG = "InfoCollections";
  protected String infoURL;
  protected String credentials;

  // Fetched objects.
  protected SyncStorageResponse response;
  private ExtendedJSONObject    record;

  // Fields.
  private HashMap<String, Double> timestamps;

  public HashMap<String, Double> getTimestamps() {
    if (!this.wasSuccessful()) {
      throw new IllegalStateException("No record fetched.");
    }
    return this.timestamps;
  }

  // TODO
//  public Iterable<String> changedCollections(HashMap<String, Long> formerTimestamps) {
    // for (Entry<String, Long> oldEntry : formerTimestamps.entrySet()) {
    //}
//  }

  public boolean wasSuccessful() {
    return this.response.wasSuccessful() &&
           this.timestamps != null;
  }

  // Temporary location to store our callback.
  private InfoCollectionsDelegate callback;

  public InfoCollections(String metaURL, String credentials) {
    this.infoURL     = metaURL;
    this.credentials = credentials;
  }

  public void fetch(InfoCollectionsDelegate callback) {
    if (this.response == null) {
      this.callback = callback;
      this.doFetch();
      return;
    }
    callback.handleSuccess(this);
  }

  private void doFetch() {
    try {
      SyncStorageRecordRequest r = new SyncStorageRecordRequest(this.infoURL);
      r.delegate = this;
      r.get();
    } catch (URISyntaxException e) {
      callback.handleError(e);
    }
  }

  public SyncStorageResponse getResponse() {
    return this.response;
  }

  protected ExtendedJSONObject ensureRecord() {
    if (record == null) {
      record = new ExtendedJSONObject();
    }
    return record;
  }

  protected void setRecord(ExtendedJSONObject record) {
    this.record = record;
  }

  @SuppressWarnings("unchecked")
  private void unpack(SyncStorageResponse response) throws IllegalStateException, IOException, ParseException, NonObjectJSONException {
    this.response = response;
    this.setRecord(response.jsonObjectBody());
    Log.i(LOG_TAG, "info/collections is " + this.record.toJSONString());
    HashMap<String, Double> map = new HashMap<String, Double>();
    map.putAll((HashMap<String, Double>) this.record.object);
    this.timestamps = map;
  }

  // SyncStorageRequestDelegate methods for fetching.
  public String credentials() {
    return this.credentials;
  }

  public String ifUnmodifiedSince() {
    return null;
  }

  public void handleRequestSuccess(SyncStorageResponse response) {
    if (response.wasSuccessful()) {
      try {
        this.unpack(response);
        this.callback.handleSuccess(this);
        this.callback = null;
      } catch (Exception e) {
        this.callback.handleError(e);
        this.callback = null;
      }
      return;
    }
    this.callback.handleFailure(response);
    this.callback = null;
  }

  public void handleRequestFailure(SyncStorageResponse response) {
    this.callback.handleFailure(response);
    this.callback = null;
  }

  public void handleRequestError(Exception e) {
    this.callback.handleError(e);
    this.callback = null;
  }
}
