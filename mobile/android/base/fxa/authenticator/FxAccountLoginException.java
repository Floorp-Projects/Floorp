/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.authenticator;

public class FxAccountLoginException extends Exception {
  public FxAccountLoginException(String string) {
    super(string);
  }

  public FxAccountLoginException(Exception e) {
    super(e);
  }

  private static final long serialVersionUID = 397685959625820798L;

  public static class FxAccountLoginBadPasswordException extends FxAccountLoginException {
    public FxAccountLoginBadPasswordException(String string) {
      super(string);
    }

    private static final long serialVersionUID = 397685959625820799L;
  }

  public static class FxAccountLoginAccountNotVerifiedException extends FxAccountLoginException {
    public FxAccountLoginAccountNotVerifiedException(String string) {
      super(string);
    }

    private static final long serialVersionUID = 397685959625820800L;
  }
}
