/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware;

import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

public abstract class MiddlewareRepository extends Repository {

  public abstract class SessionCreationDelegate implements
      RepositorySessionCreationDelegate {

    // We call through to our inner repository, so we don't need our own
    // deferral scheme.
    @Override
    public RepositorySessionCreationDelegate deferredCreationDelegate() {
      return this;
    }
  }
}
