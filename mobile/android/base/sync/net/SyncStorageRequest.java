/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.security.GeneralSecurityException;
import java.util.HashMap;

import org.mozilla.gecko.sync.GlobalConstants;

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
      client.getParams().setParameter(CoreProtocolPNames.USER_AGENT, GlobalConstants.USER_AGENT);

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
