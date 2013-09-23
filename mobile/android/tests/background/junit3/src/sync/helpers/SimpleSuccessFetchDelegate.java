/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

import java.util.concurrent.ExecutorService;

import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionFetchRecordsDelegate;
import org.mozilla.gecko.sync.repositories.domain.Record;

public abstract class SimpleSuccessFetchDelegate extends DefaultDelegate implements
    RepositorySessionFetchRecordsDelegate {
  @Override
  public void onFetchFailed(Exception ex, Record record) {
    performNotify("Fetch failed", ex);
  }

  @Override
  public RepositorySessionFetchRecordsDelegate deferredFetchDelegate(ExecutorService executor) {
    return this;
  }
}
