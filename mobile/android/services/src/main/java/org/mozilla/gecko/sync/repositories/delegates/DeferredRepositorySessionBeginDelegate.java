/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.repositories.RepositorySession;

public class DeferredRepositorySessionBeginDelegate implements RepositorySessionBeginDelegate {
  private final RepositorySessionBeginDelegate inner;
  private final ExecutorService executor;
  public DeferredRepositorySessionBeginDelegate(final RepositorySessionBeginDelegate inner, final ExecutorService executor) {
    this.inner = inner;
    this.executor = executor;
  }

  @Override
  public void onBeginSucceeded(final RepositorySession session) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onBeginSucceeded(session);
      }
    });
  }

  @Override
  public void onBeginFailed(final Exception ex) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onBeginFailed(ex);
      }
    });
  }
  
  @Override
  public RepositorySessionBeginDelegate deferredBeginDelegate(ExecutorService newExecutor) {
    if (newExecutor == executor) {
      return this;
    }
    throw new IllegalArgumentException("Can't re-defer this delegate.");
  }
}