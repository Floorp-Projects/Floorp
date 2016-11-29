/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.net;

import java.io.IOException;
import java.util.HashMap;

import org.mozilla.gecko.background.common.log.Logger;

import ch.boye.httpclientandroidlib.HttpResponse;

public class SyncStorageResponse extends SyncResponse {
  private static final String LOG_TAG = "SyncStorageResponse";

  // Responses that are actionable get constant status codes.
  public static final String RESPONSE_CLIENT_UPGRADE_REQUIRED = "16";

  public static final HashMap<String, String> SERVER_ERROR_MESSAGES;
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
    errors.put(RESPONSE_CLIENT_UPGRADE_REQUIRED, "Client upgrade required");
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
    Logger.debug(LOG_TAG, "Looking up message for body \"" + body + "\"");
    if (SERVER_ERROR_MESSAGES.containsKey(body)) {
      return SERVER_ERROR_MESSAGES.get(body);
    }
    return body;
  }


  public SyncStorageResponse(HttpResponse res) {
    super(res);
  }

  public String getErrorMessage() throws IllegalStateException, IOException {
    return SyncStorageResponse.getServerErrorMessage(this.body().trim());
  }

  /**
   * This header gives the last-modified time of the target resource as seen during processing of
   * the request, and will be included in all success responses (200, 201, 204).
   * When given in response to a write request, this will be equal to the server’s current time and
   * to the new last-modified time of any BSOs created or changed by the request.
   */
  public String getLastModified() {
    if (!response.containsHeader(X_LAST_MODIFIED)) {
      return null;
    }
    return response.getFirstHeader(X_LAST_MODIFIED).getValue();
  }

  // TODO: Content-Type and Content-Length validation.

}
