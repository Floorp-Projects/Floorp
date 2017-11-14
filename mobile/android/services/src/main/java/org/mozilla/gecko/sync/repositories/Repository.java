/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories;

import org.mozilla.gecko.sync.SessionCreateException;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCleanDelegate;

import android.content.Context;

public abstract class Repository {
  public abstract RepositorySession createSession(Context context) throws SessionCreateException;

  public void clean(boolean success, RepositorySessionCleanDelegate delegate, Context context) {
    delegate.onCleaned(this);
  }
}