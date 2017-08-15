/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.android;

import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionCreationDelegate;

import android.content.Context;

public class HistoryRepository extends ThreadedRepository implements org.mozilla.gecko.sync.repositories.HistoryRepository {

  @Override
  protected void sessionCreator(RepositorySessionCreationDelegate delegate, Context context) {
    HistoryRepositorySession session = new HistoryRepositorySession(HistoryRepository.this, context);
    delegate.onSessionCreated(session);
  }

  @Override
  protected DataAccessor getDataAccessor(Context context) {
    return new HistoryDataAccessor(context);
  }

}
