/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import org.mozilla.gecko.sync.ThreadPool;
import org.mozilla.gecko.sync.repositories.RepositorySession;

public abstract class DeferrableRepositorySessionCreationDelegate implements RepositorySessionCreationDelegate {
  @Override
  public RepositorySessionCreationDelegate deferredCreationDelegate() {
    final RepositorySessionCreationDelegate self = this;
    return new RepositorySessionCreationDelegate() {

      // TODO: rewrite to use ExecutorService.
      @Override
      public void onSessionCreated(final RepositorySession session) {
        ThreadPool.run(new Runnable() {
          @Override
          public void run() {
            self.onSessionCreated(session);
          }});
      }

      @Override
      public void onSessionCreateFailed(final Exception ex) {
        ThreadPool.run(new Runnable() {
          @Override
          public void run() {
            self.onSessionCreateFailed(ex);
          }});
      }

      @Override
      public RepositorySessionCreationDelegate deferredCreationDelegate() {
        return this;
      }
    };
  }
}
