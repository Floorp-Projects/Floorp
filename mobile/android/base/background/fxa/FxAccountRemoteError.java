/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

public interface FxAccountRemoteError {
  public static final int ATTEMPT_TO_CREATE_AN_ACCOUNT_THAT_ALREADY_EXISTS = 101;
  public static final int ATTEMPT_TO_ACCESS_AN_ACCOUNT_THAT_DOES_NOT_EXIST = 102;
  public static final int INCORRECT_PASSWORD = 103;
  public static final int ATTEMPT_TO_OPERATE_ON_AN_UNVERIFIED_ACCOUNT = 104;
  public static final int INVALID_VERIFICATION_CODE = 105;
  public static final int REQUEST_BODY_WAS_NOT_VALID_JSON = 106;
  public static final int REQUEST_BODY_CONTAINS_INVALID_PARAMETERS = 107;
  public static final int REQUEST_BODY_MISSING_REQUIRED_PARAMETERS = 108;
  public static final int INVALID_REQUEST_SIGNATURE = 109;
  public static final int INVALID_AUTHENTICATION_TOKEN = 110;
  public static final int INVALID_AUTHENTICATION_TIMESTAMP = 111;
  public static final int INVALID_AUTHENTICATION_NONCE = 115;
  public static final int CONTENT_LENGTH_HEADER_WAS_NOT_PROVIDED = 112;
  public static final int REQUEST_BODY_TOO_LARGE = 113;
  public static final int CLIENT_HAS_SENT_TOO_MANY_REQUESTS = 114;
  public static final int INVALID_NONCE_IN_REQUEST_SIGNATURE = 115;
  public static final int ENDPOINT_IS_NO_LONGER_SUPPORTED = 116;
  public static final int INCORRECT_LOGIN_METHOD_FOR_THIS_ACCOUNT = 117;
  public static final int INCORRECT_KEY_RETRIEVAL_METHOD_FOR_THIS_ACCOUNT = 118;
  public static final int INCORRECT_API_VERSION_FOR_THIS_ACCOUNT = 119;
  public static final int INCORRECT_EMAIL_CASE = 120;
  public static final int SERVICE_TEMPORARILY_UNAVAILABLE_DUE_TO_HIGH_LOAD = 201;
  public static final int UNKNOWN_ERROR = 999;
}
