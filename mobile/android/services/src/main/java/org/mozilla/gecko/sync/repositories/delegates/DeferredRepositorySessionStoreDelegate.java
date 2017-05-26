/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

public class DeferredRepositorySessionStoreDelegate implements
    RepositorySessionStoreDelegate {
  protected final RepositorySessionStoreDelegate inner;
  protected final ExecutorService                executor;

  public DeferredRepositorySessionStoreDelegate(
      RepositorySessionStoreDelegate inner, ExecutorService executor) {
    this.inner = inner;
    this.executor = executor;
  }

  @Override
  public void onRecordStoreSucceeded(final String guid) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onRecordStoreSucceeded(guid);
      }
    });
  }

  @Override
  public void onRecordStoreFailed(final Exception ex, final String guid) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onRecordStoreFailed(ex, guid);
      }
    });
  }

  @Override
  public RepositorySessionStoreDelegate deferredStoreDelegate(ExecutorService newExecutor) {
    if (newExecutor == executor) {
      return this;
    }
    throw new IllegalArgumentException("Can't re-defer this delegate.");
  }

  @Override
  public void onStoreCompleted(final long storeEnd) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onStoreCompleted(storeEnd);
      }
    });
  }

  @Override
  public void onStoreFailed(final Exception e) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onStoreFailed(e);
      }
    });
  }

  @Override
  public void onRecordStoreReconciled(final String guid) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onRecordStoreReconciled(guid);
      }
    });
  }
}
