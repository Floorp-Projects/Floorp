/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

public class NonArrayJSONException extends UnexpectedJSONException {
  private static final long serialVersionUID = 5582918057432365749L;

  public NonArrayJSONException(String detailMessage) {
    super(detailMessage);
  }

  public NonArrayJSONException(Throwable throwable) {
    super(throwable);
  }
}
