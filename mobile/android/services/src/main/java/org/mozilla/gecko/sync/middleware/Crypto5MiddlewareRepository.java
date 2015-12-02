/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.middleware;

import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.repositories.IdentityRecordFactory;
import org.mozilla.gecko.sync.repositories.RecordFactory;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCleanDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

import android.content.Context;

/**
 * Wrap an existing repository in middleware that encrypts and decrypts records
 * passing through.
 *
 * @author rnewman
 *
 */
public class Crypto5MiddlewareRepository extends MiddlewareRepository {

  public RecordFactory recordFactory = new IdentityRecordFactory();

  public class Crypto5MiddlewareRepositorySessionCreationDelegate extends MiddlewareRepository.SessionCreationDelegate {
    private final Crypto5MiddlewareRepository repository;
    private final RepositorySessionCreationDelegate outerDelegate;

    public Crypto5MiddlewareRepositorySessionCreationDelegate(Crypto5MiddlewareRepository repository, RepositorySessionCreationDelegate outerDelegate) {
      this.repository = repository;
      this.outerDelegate = outerDelegate;
    }

    @Override
    public void onSessionCreateFailed(Exception ex) {
      this.outerDelegate.onSessionCreateFailed(ex);
    }

    @Override
    public void onSessionCreated(RepositorySession session) {
      // Do some work, then report success with the wrapping session.
      Crypto5MiddlewareRepositorySession cryptoSession;
      try {
        // Synchronous, baby.
        cryptoSession = new Crypto5MiddlewareRepositorySession(session, this.repository, recordFactory);
      } catch (Exception ex) {
        this.outerDelegate.onSessionCreateFailed(ex);
        return;
      }
      this.outerDelegate.onSessionCreated(cryptoSession);
    }
  }

  public KeyBundle keyBundle;
  private final Repository inner;

  public Crypto5MiddlewareRepository(Repository inner, KeyBundle keys) {
    super();
    this.inner = inner;
    this.keyBundle = keys;
  }
  @Override
  public void createSession(RepositorySessionCreationDelegate delegate, Context context) {
    Crypto5MiddlewareRepositorySessionCreationDelegate delegateWrapper = new Crypto5MiddlewareRepositorySessionCreationDelegate(this, delegate);
    inner.createSession(delegateWrapper, context);
  }

  @Override
  public void clean(boolean success, RepositorySessionCleanDelegate delegate,
                    Context context) {
    this.inner.clean(success, delegate, context);
  }
}
