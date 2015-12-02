/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.repositories.RepositorySession;

/**
 * One of these two methods is guaranteed to be called after session.begin() is
 * invoked (possibly during the invocation). The callback will be invoked prior
 * to any other RepositorySession callbacks.
 *
 * @author rnewman
 *
 */
public interface RepositorySessionBeginDelegate {
  public void onBeginFailed(Exception ex);
  public void onBeginSucceeded(RepositorySession session);
  public RepositorySessionBeginDelegate deferredBeginDelegate(ExecutorService executor);
}
