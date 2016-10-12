/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.repositories.delegates;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.repositories.domain.Record;

public class DeferredRepositorySessionFetchRecordsDelegate implements RepositorySessionFetchRecordsDelegate {
  private final RepositorySessionFetchRecordsDelegate inner;
  private final ExecutorService executor;
  public DeferredRepositorySessionFetchRecordsDelegate(final RepositorySessionFetchRecordsDelegate inner, final ExecutorService executor) {
    this.inner = inner;
    this.executor = executor;
  }

  @Override
  public void onFetchedRecord(final Record record) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
         inner.onFetchedRecord(record);
      }
    });
  }

  @Override
  public void onFetchFailed(final Exception ex) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onFetchFailed(ex);
      }
    });
  }

  @Override
  public void onFetchCompleted(final long fetchEnd) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onFetchCompleted(fetchEnd);
      }
    });
  }

  @Override
  public void onBatchCompleted() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        inner.onBatchCompleted();
      }
    });
  }

  @Override
  public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService newExecutor) {
    if (newExecutor == executor) {
      return this;
    }
    throw new IllegalArgumentException("Can't re-defer this delegate.");
  }
}
