/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa.oauth;

public interface FxAccountOAuthRemoteError {
  public static final int ATTEMPT_TO_CREATE_AN_ACCOUNT_THAT_ALREADY_EXISTS = 101;
  public static final int UNKNOWN_CLIENT_ID = 101;
  public static final int INCORRECT_CLIENT_SECRET = 102;
  public static final int REDIRECT_URI_DOES_NOT_MATCH_REGISTERED_VALUE = 103;
  public static final int INVALID_FXA_ASSERTION = 104;
  public static final int UNKNOWN_CODE = 105;
  public static final int INCORRECT_CODE = 106;
  public static final int EXPIRED_CODE = 107;
  public static final int INVALID_TOKEN = 108;
  public static final int INVALID_REQUEST_PARAMETER = 109;
  public static final int UNKNOWN_ERROR = 999;
}
