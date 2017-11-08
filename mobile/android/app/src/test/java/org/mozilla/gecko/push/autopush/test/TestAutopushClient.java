/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.push.autopush.test;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.background.testhelpers.WaitHelper;
import org.mozilla.gecko.push.autopush.AutopushClient;
import org.mozilla.gecko.push.autopush.AutopushClientException;
import org.mozilla.gecko.sync.Utils;

@RunWith(TestRunner.class)
public class TestAutopushClient {
    @Test
    public void testGetSenderID() throws Exception {
        final AutopushClient client = new AutopushClient("https://updates-autopush-dev.stage.mozaws.net/v1/gcm/829133274407",
                Utils.newSynchronousExecutor());
        Assert.assertEquals("829133274407", client.getSenderIDFromServerURI());
    }

    @Test(expected=AutopushClientException.class)
    public void testGetNoSenderID() throws Exception {
        final AutopushClient client = new AutopushClient("https://updates-autopush-dev.stage.mozaws.net/v1/gcm",
                Utils.newSynchronousExecutor());
        client.getSenderIDFromServerURI();
    }
}
