/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCleanDelegate;

import android.content.Context;

public class BookmarksRepository extends Repository {
  @Override
  public RepositorySession createSession(Context context) {
    return new BookmarksRepositorySession(this, context);
  }

  @Override
  public void clean(boolean success, RepositorySessionCleanDelegate delegate, Context context) {
    if (!success) {
      return;
    }

    final BookmarksDataAccessor dataAccessor = new BookmarksDataAccessor(context);

    try {
      dataAccessor.purgeDeleted();
    } catch (Exception e) {
      delegate.onCleanFailed(this, e);
      return;
    }

    delegate.onCleaned(this);
  }
}
