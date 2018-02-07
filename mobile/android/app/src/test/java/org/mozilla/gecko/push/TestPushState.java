/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.push;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.mozilla.gecko.util.StringUtils;
import org.robolectric.RuntimeEnvironment;

import java.io.File;
import java.io.FileOutputStream;

@RunWith(RobolectricTestRunner.class)
public class TestPushState {
    @Test
    public void testRoundTrip() throws Exception {
        final PushState state = new PushState(RuntimeEnvironment.application, "test.json");
        // Fresh state should have no registrations (and no subscriptions).
        Assert.assertTrue(state.registrations.isEmpty());

        final PushRegistration registration = new PushRegistration("endpoint", true, Fetched.now("uaid"), "secret");
        final PushSubscription subscription = new PushSubscription("chid", "profileName", "webpushEndpoint", "service", null);
        registration.putSubscription("chid", subscription);
        state.putRegistration("profileName", registration);
        Assert.assertEquals(1, state.registrations.size());
        state.checkpoint();

        final PushState readState = new PushState(RuntimeEnvironment.application, "test.json");
        Assert.assertEquals(1, readState.registrations.size());
        final PushRegistration storedRegistration = readState.getRegistration("profileName");
        Assert.assertEquals(registration, storedRegistration);

        Assert.assertEquals(1, storedRegistration.subscriptions.size());
        final PushSubscription storedSubscription = storedRegistration.getSubscription("chid");
        Assert.assertEquals(subscription, storedSubscription);
    }

    @Test
    public void testMissingRegistration() throws Exception {
        final PushState state = new PushState(RuntimeEnvironment.application, "testMissingRegistration.json");
        Assert.assertNull(state.getRegistration("missingProfileName"));
    }

    @Test
    public void testMissingSubscription() throws Exception {
        final PushRegistration registration = new PushRegistration("endpoint", true, Fetched.now("uaid"), "secret");
        Assert.assertNull(registration.getSubscription("missingChid"));
    }

    @Test
    public void testCorruptedJSON() throws Exception {
        // Write some malformed JSON.
        // TODO: use mcomella's helpers!
        final File file = new File(RuntimeEnvironment.application.getApplicationInfo().dataDir, "testCorruptedJSON.json");
        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(file);
            fos.write("}".getBytes(StringUtils.UTF_8));
        } finally {
            if (fos != null) {
                fos.close();
            }
        }

        final PushState state = new PushState(RuntimeEnvironment.application, "testCorruptedJSON.json");
        Assert.assertTrue(state.getRegistrations().isEmpty());
    }
}
