/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import static junit.framework.Assert.assertNotNull;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.repositories.InvalidSessionTransitionException;
import org.mozilla.gecko.sync.repositories.Repository;
import org.mozilla.gecko.sync.repositories.RepositorySession;

import android.content.Context;

public class SessionTestHelper {

  protected static RepositorySession prepareRepositorySession(
      final Context context,
      final boolean begin,
      final Repository repository) {

    final WaitHelper testWaiter = WaitHelper.getTestWaiter();

    final String logTag = "prepareRepositorySession";
    class CreationDelegate extends DefaultSessionCreationDelegate {
      private RepositorySession session;
      synchronized void setSession(RepositorySession session) {
        this.session = session;
      }
      synchronized RepositorySession getSession() {
        return this.session;
      }

      @Override
      public void onSessionCreated(final RepositorySession session) {
        assertNotNull(session);
        Logger.info(logTag, "Setting session to " + session);
        setSession(session);
        if (begin) {
          Logger.info(logTag, "Calling session.begin on new session.");
          // The begin callbacks will notify.
          try {
            session.begin(new ExpectBeginDelegate());
          } catch (InvalidSessionTransitionException e) {
            testWaiter.performNotify(e);
          }
        } else {
          Logger.info(logTag, "Notifying after setting new session.");
          testWaiter.performNotify();
        }
      }
    }

    final CreationDelegate delegate = new CreationDelegate();
    try {
      Runnable runnable = new Runnable() {
        @Override
        public void run() {
          repository.createSession(delegate, context);
        }
      };
      testWaiter.performWait(runnable);
    } catch (IllegalArgumentException ex) {
      Logger.warn(logTag, "Caught IllegalArgumentException.");
    }

    Logger.info(logTag, "Retrieving new session.");
    final RepositorySession session = delegate.getSession();
    assertNotNull(session);

    return session;
  }

  public static RepositorySession createSession(final Context context, final Repository repository) {
    return prepareRepositorySession(context, false, repository);
  }

  public static RepositorySession createAndBeginSession(Context context, Repository repository) {
    return prepareRepositorySession(context, true, repository);
  }
}
