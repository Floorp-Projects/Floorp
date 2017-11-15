/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import static junit.framework.Assert.assertNotNull;

import org.mozilla.gecko.sync.SessionCreateException;
import org.mozilla.gecko.sync.SyncException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;

import android.content.Context;

public class SessionTestHelper {
  private static RepositorySession prepareRepositorySession(
      final Context context,
      final boolean begin,
      final Repository repository) {

    final RepositorySession session;
    try {
      session = repository.createSession(context);
      assertNotNull(session);
    } catch (SessionCreateException e) {
      throw new IllegalStateException(e);
    }

    if (begin) {
      try {
        session.begin();
      } catch (SyncException e) {
        throw new IllegalStateException(e);
      }
    }

    return session;
  }

  public static RepositorySession createSession(final Context context, final Repository repository) {
    return prepareRepositorySession(context, false, repository);
  }

  public static RepositorySession createAndBeginSession(Context context, Repository repository) {
    return prepareRepositorySession(context, true, repository);
  }
}
