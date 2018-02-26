/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

public class ExpectNoStoreDelegate extends ExpectStoreCompletedDelegate {
    @Override
    public void onRecordStoreSucceeded(int count) {
        performNotify("Should not have stored records: " + count, null);
    }
}