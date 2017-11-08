/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFinishDelegate;

public class DefaultFinishDelegate extends DefaultDelegate implements RepositorySessionFinishDelegate {

  @Override
  public void onFinishFailed(Exception ex) {
    performNotify("Finish failed", ex);
  }

  @Override
  public void onFinishSucceeded(RepositorySession session, RepositorySessionBundle bundle) {
    performNotify("Hit default finish delegate", null);
  }

  @Override
  public RepositorySessionFinishDelegate deferredFinishDelegate(final ExecutorService executor) {
    final RepositorySessionFinishDelegate self = this;

    Logger.info("DefaultFinishDelegate", "Deferring…");
    return new RepositorySessionFinishDelegate() {
      @Override
      public void onFinishSucceeded(final RepositorySession session,
                                    final RepositorySessionBundle bundle) {
        Logger.info("DefaultFinishDelegate", "Executing onFinishSucceeded Runnable…");
        executor.execute(new Runnable() {
          @Override
          public void run() {
            self.onFinishSucceeded(session, bundle);
          }});
      }

      @Override
      public void onFinishFailed(final Exception ex) {
        executor.execute(new Runnable() {
          @Override
          public void run() {
            self.onFinishFailed(ex);
          }});
      }

      @Override
      public RepositorySessionFinishDelegate deferredFinishDelegate(ExecutorService newExecutor) {
        if (newExecutor == executor) {
          return this;
        }
        throw new IllegalArgumentException("Can't re-defer this delegate.");
      }
    };
  }
}
