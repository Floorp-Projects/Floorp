/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.tokenserver;

import java.util.List;

import org.mozilla.gecko.sync.ExtendedJSONObject;

public class TokenServerException extends Exception {
  private static final long serialVersionUID = 7185692034925819696L;

  public final List<ExtendedJSONObject> errors;

  public TokenServerException(List<ExtendedJSONObject> errors) {
    super();
    this.errors = errors;
  }

  public TokenServerException(List<ExtendedJSONObject> errors, String string) {
    super(string);
    this.errors = errors;
  }

  public TokenServerException(List<ExtendedJSONObject> errors, Throwable e) {
    super(e);
    this.errors = errors;
  }

  public static class TokenServerConditionsRequiredException extends TokenServerException {
    private static final long serialVersionUID = 7578072663150608399L;

    public final ExtendedJSONObject conditionUrls;

    public TokenServerConditionsRequiredException(ExtendedJSONObject urls) {
      super(null);
      this.conditionUrls = urls;
    }
  }

  public static class TokenServerInvalidCredentialsException extends TokenServerException {
    private static final long serialVersionUID = 7578072663150608398L;

    public TokenServerInvalidCredentialsException(List<ExtendedJSONObject> errors) {
      super(errors);
    }

    public TokenServerInvalidCredentialsException(List<ExtendedJSONObject> errors, String message) {
      super(errors, message);
    }
  }

  public static class TokenServerUnknownServiceException extends TokenServerException {
    private static final long serialVersionUID = 7578072663150608397L;

    public TokenServerUnknownServiceException(List<ExtendedJSONObject> errors) {
      super(errors);
    }

    public TokenServerUnknownServiceException(List<ExtendedJSONObject> errors, String message) {
      super(errors, message);
    }
  }

  public static class TokenServerMalformedRequestException extends TokenServerException {
    private static final long serialVersionUID = 7578072663150608396L;

    public TokenServerMalformedRequestException(List<ExtendedJSONObject> errors) {
      super(errors);
    }

    public TokenServerMalformedRequestException(List<ExtendedJSONObject> errors, String message) {
      super(errors, message);
    }
  }

  public static class TokenServerMalformedResponseException extends TokenServerException {
    private static final long serialVersionUID = 7578072663150608395L;

    public TokenServerMalformedResponseException(List<ExtendedJSONObject> errors, String message) {
      super(errors, message);
    }

    public TokenServerMalformedResponseException(List<ExtendedJSONObject> errors, Throwable e) {
      super(errors, e);
    }
  }
}
