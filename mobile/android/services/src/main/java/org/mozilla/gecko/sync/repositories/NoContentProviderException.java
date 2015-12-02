/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import org.mozilla.gecko.sync.SyncException;

import android.net.Uri;

/**
 * Raised when a Content Provider cannot be retrieved.
 *
 * @author rnewman
 *
 */
public class NoContentProviderException extends SyncException {
  private static final long serialVersionUID = 1L;

  public final Uri requestedProvider;
  public NoContentProviderException(Uri requested) {
    super();
    this.requestedProvider = requested;
  }
}
