/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.push;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.gcm.GcmTokenClient;
import org.robolectric.RuntimeEnvironment;

import java.util.UUID;

import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Matchers.isNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;


@RunWith(TestRunner.class)
public class TestPushManager {
    private PushState state;
    private GcmTokenClient gcmTokenClient;
    private PushClient pushClient;
    private PushManager manager;

    @Before
    public void setUp() throws Exception {
        state = new PushState(RuntimeEnvironment.application, "test.json");
        gcmTokenClient = mock(GcmTokenClient.class);
        doReturn(new Fetched("opaque-gcm-token", System.currentTimeMillis())).when(gcmTokenClient).getToken(anyString(), anyBoolean());

        // Configure a mock PushClient.
        pushClient = mock(PushClient.class);
        doReturn(new RegisterUserAgentResponse("opaque-uaid", "opaque-secret"))
                .when(pushClient)
                .registerUserAgent(anyString());

        doReturn(new SubscribeChannelResponse("opaque-chid", "https://localhost:8085/opaque-push-endpoint"))
                .when(pushClient)
                .subscribeChannel(anyString(), anyString(), isNull(String.class));

        PushManager.PushClientFactory pushClientFactory = mock(PushManager.PushClientFactory.class);
        doReturn(pushClient).when(pushClientFactory).getPushClient(anyString(), anyBoolean());

        manager = new PushManager(state, gcmTokenClient, pushClientFactory);
    }

    private void assertOnlyConfigured(PushRegistration registration, String endpoint, boolean debug) {
        Assert.assertNotNull(registration);
        Assert.assertEquals(registration.autopushEndpoint, endpoint);
        Assert.assertEquals(registration.debug, debug);
        Assert.assertNull(registration.uaid.value);
    }

    private void assertRegistered(PushRegistration registration, String endpoint, boolean debug) {
        Assert.assertNotNull(registration);
        Assert.assertEquals(registration.autopushEndpoint, endpoint);
        Assert.assertEquals(registration.debug, debug);
        Assert.assertNotNull(registration.uaid.value);
    }

    private void assertSubscribed(PushSubscription subscription) {
        Assert.assertNotNull(subscription);
        Assert.assertNotNull(subscription.chid);
    }

    @Test
    public void testConfigure() throws Exception {
        PushRegistration registration = manager.configure("default", "http://localhost:8081", false, System.currentTimeMillis());
        assertOnlyConfigured(registration, "http://localhost:8081", false);

        registration = manager.configure("default", "http://localhost:8082", true, System.currentTimeMillis());
        assertOnlyConfigured(registration, "http://localhost:8082", true);
    }

    @Test(expected=PushManager.ProfileNeedsConfigurationException.class)
    public void testRegisterBeforeConfigure() throws Exception {
        PushRegistration registration = state.getRegistration("default");
        Assert.assertNull(registration);

        // Trying to register a User Agent fails before configuration.
        manager.registerUserAgent("default", System.currentTimeMillis());
    }

    @Test
    public void testRegister() throws Exception {
        PushRegistration registration = manager.configure("default", "http://localhost:8082", false, System.currentTimeMillis());
        assertOnlyConfigured(registration, "http://localhost:8082", false);

        // Let's register a User Agent, so that we can witness unregistration.
        registration = manager.registerUserAgent("default", System.currentTimeMillis());
        assertRegistered(registration, "http://localhost:8082", false);

        // Changing the debug flag should update but not try to unregister the User Agent.
        registration = manager.configure("default", "http://localhost:8082", true, System.currentTimeMillis());
        assertRegistered(registration, "http://localhost:8082", true);

        // Changing the configuration endpoint should update and try to unregister the User Agent.
        registration = manager.configure("default", "http://localhost:8083", true, System.currentTimeMillis());
        assertOnlyConfigured(registration, "http://localhost:8083", true);
    }

    @Test
    public void testRegisterMultipleProfiles() throws Exception {
        PushRegistration registration1 = manager.configure("default1", "http://localhost:8081", true, System.currentTimeMillis());
        PushRegistration registration2 = manager.configure("default2", "http://localhost:8082", true, System.currentTimeMillis());
        assertOnlyConfigured(registration1, "http://localhost:8081", true);
        assertOnlyConfigured(registration2, "http://localhost:8082", true);
        verify(gcmTokenClient, times(0)).getToken(anyString(), anyBoolean());

        registration1 = manager.registerUserAgent("default1", System.currentTimeMillis());
        assertRegistered(registration1, "http://localhost:8081", true);

        registration2 = manager.registerUserAgent("default2", System.currentTimeMillis());
        assertRegistered(registration2, "http://localhost:8082", true);

        // Just the debug flag should not unregister the User Agent.
        registration1 = manager.configure("default1", "http://localhost:8081", false, System.currentTimeMillis());
        assertRegistered(registration1, "http://localhost:8081", false);

        // But the configuration endpoint should unregister the correct User Agent.
        registration2 = manager.configure("default2", "http://localhost:8083", false, System.currentTimeMillis());
    }

    @Test
    public void testSubscribeChannel() throws Exception {
        manager.configure("default", "http://localhost:8080", false, System.currentTimeMillis());
        PushRegistration registration = manager.registerUserAgent("default", System.currentTimeMillis());
        assertRegistered(registration, "http://localhost:8080", false);

        // We should be able to register with non-null serviceData.
        final JSONObject webpushData = new JSONObject();
        webpushData.put("version", 5);
        PushSubscription subscription = manager.subscribeChannel("default", "webpush", webpushData, null, System.currentTimeMillis());
        assertSubscribed(subscription);

        subscription = manager.registrationForSubscription(subscription.chid).getSubscription(subscription.chid);
        Assert.assertNotNull(subscription);
        Assert.assertEquals(5, subscription.serviceData.get("version"));

        // We should be able to register with null serviceData.
        subscription = manager.subscribeChannel("default", "sync", null, null, System.currentTimeMillis());
        assertSubscribed(subscription);

        subscription = manager.registrationForSubscription(subscription.chid).getSubscription(subscription.chid);
        Assert.assertNotNull(subscription);
        Assert.assertNull(subscription.serviceData);
    }

    @Test
    public void testUnsubscribeChannel() throws Exception {
        manager.configure("default", "http://localhost:8080", false, System.currentTimeMillis());
        PushRegistration registration = manager.registerUserAgent("default", System.currentTimeMillis());
        assertRegistered(registration, "http://localhost:8080", false);

        // We should be able to register with non-null serviceData.
        final JSONObject webpushData = new JSONObject();
        webpushData.put("version", 5);
        PushSubscription subscription = manager.subscribeChannel("default", "webpush", webpushData, null, System.currentTimeMillis());
        assertSubscribed(subscription);

        // No exception is success.
        manager.unsubscribeChannel(subscription.chid);
    }

    public void testUnsubscribeUnknownChannel() throws Exception {
        manager.configure("default", "http://localhost:8080", false, System.currentTimeMillis());
        PushRegistration registration = manager.registerUserAgent("default", System.currentTimeMillis());
        assertRegistered(registration, "http://localhost:8080", false);

        doThrow(new RuntimeException())
                .when(pushClient)
                .unsubscribeChannel(anyString(), anyString(), anyString());

        // Un-subscribing from an unknown channel succeeds: we just ignore the request.
        manager.unsubscribeChannel(UUID.randomUUID().toString());
    }

    @Test
    public void testStartupBeforeConfiguration() throws Exception {
        verify(gcmTokenClient, never()).getToken(anyString(), anyBoolean());
        manager.startup(System.currentTimeMillis());
        verify(gcmTokenClient, times(1)).getToken(AppConstants.MOZ_ANDROID_GCM_SENDERIDS, false);
    }

    @Test
    public void testStartupBeforeRegistration() throws Exception {
        PushRegistration registration = manager.configure("default", "http://localhost:8080", true, System.currentTimeMillis());
        assertOnlyConfigured(registration, "http://localhost:8080", true);

        manager.startup(System.currentTimeMillis());
        verify(gcmTokenClient, times(1)).getToken(anyString(), anyBoolean());
    }

    @Test
    public void testStartupAfterRegistration() throws Exception {
        PushRegistration registration = manager.configure("default", "http://localhost:8080", true, System.currentTimeMillis());
        assertOnlyConfigured(registration, "http://localhost:8080", true);

        registration = manager.registerUserAgent("default", System.currentTimeMillis());
        assertRegistered(registration, "http://localhost:8080", true);

        manager.startup(System.currentTimeMillis());

        // Rather tautological.
        PushRegistration updatedRegistration = manager.state.getRegistration("default");
        Assert.assertEquals(registration.uaid, updatedRegistration.uaid);
    }

    @Test
    public void testStartupAfterSubscription() throws Exception {
        PushRegistration registration = manager.configure("default", "http://localhost:8080", true, System.currentTimeMillis());
        assertOnlyConfigured(registration, "http://localhost:8080", true);

        registration = manager.registerUserAgent("default", System.currentTimeMillis());
        assertRegistered(registration, "http://localhost:8080", true);

        PushSubscription subscription = manager.subscribeChannel("default", "webpush", null, null, System.currentTimeMillis());
        assertSubscribed(subscription);

        manager.startup(System.currentTimeMillis());

        // Rather tautological.
        registration = manager.registrationForSubscription(subscription.chid);
        PushSubscription updatedSubscription = registration.getSubscription(subscription.chid);
        Assert.assertEquals(subscription.chid, updatedSubscription.chid);
    }
}
