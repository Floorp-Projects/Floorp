/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.repositories.RepositorySession;
import org.mozilla.gecko.sync.repositories.RepositorySessionBundle;

public class DeferredRepositorySessionFinishDelegate implements
    RepositorySessionFinishDelegate {
  protected final ExecutorService executor;
  protected final RepositorySessionFinishDelegate inner;

  public DeferredRepositorySessionFinishDelegate(RepositorySessionFinishDelegate inner,
                                                 ExecutorService executor) {
    this.executor = executor;
    this.inner = inner;
  }

  @Override
  public void onFinishSucceeded(final RepositorySession session,
                                final RepositorySessionBundle bundle) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onFinishSucceeded(session, bundle);
      }
    });
  }

  @Override
  public void onFinishFailed(final Exception ex) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onFinishFailed(ex);
      }
    });
  }

  @Override
  public RepositorySessionFinishDelegate deferredFinishDelegate(ExecutorService newExecutor) {
    if (newExecutor == executor) {
      return this;
    }
    throw new IllegalArgumentException("Can't re-defer this delegate.");
  }
}