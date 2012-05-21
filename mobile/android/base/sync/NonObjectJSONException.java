/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

public class NonObjectJSONException extends UnexpectedJSONException {
  private static final long serialVersionUID = 2214238763035650087L;

  public NonObjectJSONException(Object object) {
    super(object);
  }
}
