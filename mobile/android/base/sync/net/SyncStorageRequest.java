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

package org.mozilla.gecko.sync.net;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.HashMap;

import android.util.Log;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.client.ClientProtocolException;
import ch.boye.httpclientandroidlib.client.methods.HttpRequestBase;
import ch.boye.httpclientandroidlib.impl.client.DefaultHttpClient;
import ch.boye.httpclientandroidlib.params.CoreProtocolPNames;

public class SyncStorageRequest implements Resource {

  public static HashMap<String, String> SERVER_ERROR_MESSAGES;
  static {
    HashMap<String, String> errors = new HashMap<String, String>();

    // Sync protocol errors.
    errors.put("1", "Illegal method/protocol");
    errors.put("2", "Incorrect/missing CAPTCHA");
    errors.put("3", "Invalid/missing username");
    errors.put("4", "Attempt to overwrite data that can't be overwritten (such as creating a user ID that already exists)");
    errors.put("5", "User ID does not match account in path");
    errors.put("6", "JSON parse failure");
    errors.put("7", "Missing password field");
    errors.put("8", "Invalid Weave Basic Object");
    errors.put("9", "Requested password not strong enough");
    errors.put("10", "Invalid/missing password reset code");
    errors.put("11", "Unsupported function");
    errors.put("12", "No email address on file");
    errors.put("13", "Invalid collection");
    errors.put("14", "User over quota");
    errors.put("15", "The email does not match the username");
    errors.put("16", "Client upgrade required");
    errors.put("255", "An unexpected server error occurred: pool is empty.");

    // Infrastructure-generated errors.
    errors.put("\"server issue: getVS failed\"",                         "server issue: getVS failed");
    errors.put("\"server issue: prefix not set\"",                       "server issue: prefix not set");
    errors.put("\"server issue: host header not received from client\"", "server issue: host header not received from client");
    errors.put("\"server issue: database lookup failed\"",               "server issue: database lookup failed");
    errors.put("\"server issue: database is not healthy\"",              "server issue: database is not healthy");
    errors.put("\"server issue: database not in pool\"",                 "server issue: database not in pool");
    errors.put("\"server issue: database marked as down\"",              "server issue: database marked as down");
    SERVER_ERROR_MESSAGES = errors;
  }
  public static String getServerErrorMessage(String body) {
    if (SERVER_ERROR_MESSAGES.containsKey(body)) {
      return SERVER_ERROR_MESSAGES.get(body);
    }
    return body;
  }

  /**
   * @param uri
   * @throws URISyntaxException
   */
  public SyncStorageRequest(String uri) throws URISyntaxException {
    this(new URI(uri));
  }

  /**
   * @param uri
   */
  public SyncStorageRequest(URI uri) {
    this.resource = new BaseResource(uri);
    this.resourceDelegate = this.makeResourceDelegate(this);
    this.resource.delegate = this.resourceDelegate;
  }

  /**
   * A ResourceDelegate that mediates between Resource-level notifications and the SyncStorageRequest.
   */
  public class SyncStorageResourceDelegate extends SyncResourceDelegate {
    private static final String LOG_TAG = "SyncStorageResourceDelegate";
    protected SyncStorageRequest request;

    SyncStorageResourceDelegate(SyncStorageRequest request) {
      super(request);
      this.request = request;
    }

    @Override
    public String getCredentials() {
      return this.request.delegate.credentials();
    }

    @Override
    public void handleHttpResponse(HttpResponse response) {
      Log.d(LOG_TAG, "SyncStorageResourceDelegate handling response: " + response.getStatusLine() + ".");
      SyncStorageRequestDelegate d = this.request.delegate;
      SyncStorageResponse res = new SyncStorageResponse(response);
      if (res.wasSuccessful()) {
        d.handleRequestSuccess(res);
      } else {
        Log.w(LOG_TAG, "HTTP request failed.");
        try {
          Log.w(LOG_TAG, "HTTP response body: " + res.getErrorMessage());
        } catch (Exception e) {
          Log.e(LOG_TAG, "Can't fetch HTTP response body.", e);
        }
        d.handleRequestFailure(res);
      }
    }

    @Override
    public void handleHttpProtocolException(ClientProtocolException e) {
      this.request.delegate.handleRequestError(e);
    }

    @Override
    public void handleHttpIOException(IOException e) {
      this.request.delegate.handleRequestError(e);
    }

    @Override
    public void handleTransportException(GeneralSecurityException e) {
      this.request.delegate.handleRequestError(e);
    }

    @Override
    public void addHeaders(HttpRequestBase request, DefaultHttpClient client) {
      client.getParams().setParameter(CoreProtocolPNames.USER_AGENT, USER_AGENT);

      // Clients can use their delegate interface to specify X-If-Unmodified-Since.
      String ifUnmodifiedSince = this.request.delegate.ifUnmodifiedSince();
      if (ifUnmodifiedSince != null) {
        request.setHeader("x-if-unmodified-since", ifUnmodifiedSince);
      }
      if (request.getMethod().equalsIgnoreCase("DELETE")) {
        request.addHeader("x-confirm-delete", "1");
      }
    }
  }

  public static String USER_AGENT = "Firefox AndroidSync 0.5";
  protected SyncResourceDelegate resourceDelegate;
  public SyncStorageRequestDelegate delegate;
  protected BaseResource resource;

  public SyncStorageRequest() {
    super();
  }

  // Default implementation. Override this.
  protected SyncResourceDelegate makeResourceDelegate(SyncStorageRequest request) {
    return new SyncStorageResourceDelegate(request);
  }

  public void get() {
    this.resource.get();
  }

  public void delete() {
    this.resource.delete();
  }

  public void post(HttpEntity body) {
    this.resource.post(body);
  }

  public void put(HttpEntity body) {
    this.resource.put(body);
  }
}
