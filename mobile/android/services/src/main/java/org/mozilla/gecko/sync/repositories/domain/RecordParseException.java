/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.domain;


public class RecordParseException extends Exception {
  private static final long serialVersionUID = -5145494854722254491L;

  public RecordParseException(String detailMessage) {
    super(detailMessage);
  }
}
