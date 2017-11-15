/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware;

import org.mozilla.gecko.sync.SessionCreateException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.repositories.IdentityRecordFactory;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCleanDelegate;

import android.content.Context;

/**
 * Wrap an existing repository in middleware that encrypts and decrypts records
 * passing through.
 *
 * @author rnewman
 *
 */
public class Crypto5MiddlewareRepository extends Repository {
  public RecordFactory recordFactory = new IdentityRecordFactory();

  public KeyBundle keyBundle;
  private final Repository inner;

  public Crypto5MiddlewareRepository(Repository inner, KeyBundle keys) {
    super();
    this.inner = inner;
    this.keyBundle = keys;
  }

  @Override
  public RepositorySession createSession(Context context) throws SessionCreateException {
    final RepositorySession innerSession = inner.createSession(context);
    return new Crypto5MiddlewareRepositorySession(innerSession, this, recordFactory);
  }

  @Override
  public void clean(boolean success, RepositorySessionCleanDelegate delegate,
                    Context context) {
    this.inner.clean(success, delegate, context);
  }
}
