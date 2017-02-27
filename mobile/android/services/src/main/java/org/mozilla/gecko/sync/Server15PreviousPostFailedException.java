/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync;

/**
 * A previous POST failed, so we won't send any more records this session.
 */
public class Server15PreviousPostFailedException extends SyncException {
  private static final long serialVersionUID = -3582490631414624310L;
}
