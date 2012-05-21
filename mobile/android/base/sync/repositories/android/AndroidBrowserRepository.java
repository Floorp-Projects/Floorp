/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.sync.repositories.NullCursorException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCleanDelegate;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

import android.content.Context;

public abstract class AndroidBrowserRepository extends Repository {

  @Override
  public void createSession(RepositorySessionCreationDelegate delegate, Context context) {
    new CreateSessionThread(delegate, context).start();
  }

  @Override
  public void clean(boolean success, RepositorySessionCleanDelegate delegate, Context context) {
    // Only clean deleted records if success
    if (success) {
      new CleanThread(delegate, context).start();
    }
  }

  class CleanThread extends Thread {
    private RepositorySessionCleanDelegate delegate;
    private Context context;

    public CleanThread(RepositorySessionCleanDelegate delegate, Context context) {
      if (context == null) {
        throw new IllegalArgumentException("context is null");
      }
      this.delegate = delegate;
      this.context = context;
    }

    public void run() {
      try {
        getDataAccessor(context).purgeDeleted();
      } catch (NullCursorException e) {
        delegate.onCleanFailed(AndroidBrowserRepository.this, e);
        return;
      } catch (Exception e) {
        delegate.onCleanFailed(AndroidBrowserRepository.this, e);
        return;
      }
      delegate.onCleaned(AndroidBrowserRepository.this);
    }
  }

  protected abstract AndroidBrowserRepositoryDataAccessor getDataAccessor(Context context);
  protected abstract void sessionCreator(RepositorySessionCreationDelegate delegate, Context context);

  class CreateSessionThread extends Thread {
    private RepositorySessionCreationDelegate delegate;
    private Context context;

    public CreateSessionThread(RepositorySessionCreationDelegate delegate, Context context) {
      if (context == null) {
        throw new IllegalArgumentException("context is null.");
      }
      this.delegate = delegate;
      this.context = context;
    }

    public void run() {
      sessionCreator(delegate, context);
    }
  }

}
