/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.android.sync.test.helpers;

import junit.framework.AssertionFailedError;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.sync.repositories.delegates.RepositorySessionStoreDelegate;

import java.util.concurrent.ExecutorService;

public class ExpectSuccessRepositorySessionStoreDelegate extends
    ExpectSuccessDelegate implements RepositorySessionStoreDelegate {

  public ExpectSuccessRepositorySessionStoreDelegate(WaitHelper waitHelper) {
    super(waitHelper);
  }

  @Override
  public void onRecordStoreFailed(Exception ex, String guid) {
    log("Record store failed.", ex);
    performNotify(new AssertionFailedError("onRecordStoreFailed: record store should not have failed."));
  }

  @Override
  public void onRecordStoreSucceeded(String guid) {
    log("Record store succeeded.");
  }

  @Override
  public void onStoreCompleted(long storeEnd) {
    log("Record store completed at " + storeEnd);
  }

  @Override
  public void onStoreFailed(Exception e) {
    log("Store failed.", e);
    performNotify(new AssertionFailedError("onStoreFailed: store should not have failed."));
  }

  @Override
  public void onRecordStoreReconciled(String guid) {
    log("Store reconciled record " + guid);
  }

  @Override
  public RepositorySessionStoreDelegate deferredStoreDelegate(ExecutorService executor) {
    return this;
  }
}
