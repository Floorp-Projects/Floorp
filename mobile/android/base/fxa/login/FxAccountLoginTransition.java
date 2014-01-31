/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.fxa.login;


public class FxAccountLoginTransition {
  public interface Transition {
  }

  public static class LogMessage implements Transition {
    public final String detailMessage;

    public LogMessage(String detailMessage) {
      this.detailMessage = detailMessage;
    }

    @Override
    public String toString() {
      return getClass().getSimpleName() + (this.detailMessage == null ? "" : "('" + this.detailMessage + "')");
    }
  }

  public static class AccountNeedsVerification extends LogMessage {
    public AccountNeedsVerification() {
      super(null);
    }
  }

  public static class AccountVerified extends LogMessage {
    public AccountVerified() {
      super(null);
    }
  }

  public static class PasswordRequired extends LogMessage {
    public PasswordRequired() {
      super(null);
    }
  }

  public static class LocalError implements Transition {
    public final Exception e;

    public LocalError(Exception e) {
      this.e = e;
    }

    @Override
    public String toString() {
      return "Log(" + this.e + ")";
    }
  }

  public static class RemoteError implements Transition {
    public final Exception e;

    public RemoteError(Exception e) {
      this.e = e;
    }

    @Override
    public String toString() {
      return "Log(" + (this.e == null ? "null" : this.e) + ")";
    }
  }
}
