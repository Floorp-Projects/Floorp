/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.reading;

import org.mozilla.gecko.sync.net.MozResponse;

public class ReadingListInvalidAuthenticationException extends Exception {
  private static final long serialVersionUID = 7112459541558266597L;

  public final MozResponse response;

  public ReadingListInvalidAuthenticationException(MozResponse response) {
    super();
    this.response = response;
  }
}
