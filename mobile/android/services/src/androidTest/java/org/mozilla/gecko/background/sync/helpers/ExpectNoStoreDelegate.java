/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.sync.helpers;

public class ExpectNoStoreDelegate extends ExpectStoreCompletedDelegate {
    @Override
    public void onRecordStoreSucceeded(String guid) {
        performNotify("Should not have stored record " + guid, null);
    }
}