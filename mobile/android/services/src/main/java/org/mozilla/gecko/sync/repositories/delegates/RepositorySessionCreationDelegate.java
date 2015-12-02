/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import org.mozilla.gecko.sync.repositories.RepositorySession;

// Used to provide the sessionCallback and storeCallback
// mechanism to repository instances.
public interface RepositorySessionCreationDelegate {
  public void onSessionCreateFailed(Exception ex);
  public void onSessionCreated(RepositorySession session);
  public RepositorySessionCreationDelegate deferredCreationDelegate();
}