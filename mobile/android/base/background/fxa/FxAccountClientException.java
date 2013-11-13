/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.background.fxa;

public class FxAccountClientException extends Exception {
  private static final long serialVersionUID = 7953459541558266597L;

  public FxAccountClientException(String detailMessage) {
    super(detailMessage);
  }

  public FxAccountClientException(Exception e) {
    super(e);
  }
}