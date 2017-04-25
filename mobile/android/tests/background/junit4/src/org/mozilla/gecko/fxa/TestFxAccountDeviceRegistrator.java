/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.fxa;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.fxa.authenticator.AndroidFxAccount;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

@RunWith(TestRunner.class)
public class TestFxAccountDeviceRegistrator {

    @Mock
    AndroidFxAccount fxAccount;

    @Before
    public void init() {
        // Process Mockito annotations
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void shouldRegister() {
        // Assuming there is no previous push registration errors recorded:
        when(fxAccount.getDevicePushRegistrationError()).thenReturn(0L);
        when(fxAccount.getDevicePushRegistrationErrorTime()).thenReturn(0L);

        // Should return false if the device registration version is up-to-date and a device ID is stored.
        // (general case after a successful device registration)
        when(fxAccount.getDeviceRegistrationVersion()).thenReturn(FxAccountDeviceRegistrator.DEVICE_REGISTRATION_VERSION);
        when(fxAccount.getDeviceId()).thenReturn("bogusdeviceid");
        assertFalse(FxAccountDeviceRegistrator.shouldRegister(fxAccount));

        // Should return true if the device registration version is up-to-date but no device ID is stored.
        // (data mangling)
        when(fxAccount.getDeviceRegistrationVersion()).thenReturn(FxAccountDeviceRegistrator.DEVICE_REGISTRATION_VERSION);
        when(fxAccount.getDeviceId()).thenReturn(null);
        assertTrue(FxAccountDeviceRegistrator.shouldRegister(fxAccount));

        // Should return true if the device ID is stored but no device registration version can be found.
        // (data mangling)
        when(fxAccount.getDeviceRegistrationVersion()).thenReturn(0);
        when(fxAccount.getDeviceId()).thenReturn("bogusid");
        assertTrue(FxAccountDeviceRegistrator.shouldRegister(fxAccount));

        // Should return true if the device registration version is too old.
        // (code update pushed)
        when(fxAccount.getDeviceRegistrationVersion()).thenReturn(FxAccountDeviceRegistrator.DEVICE_REGISTRATION_VERSION - 1);
        assertTrue(FxAccountDeviceRegistrator.shouldRegister(fxAccount));

        // Should return true if the device registration is OK, but we didn't get a push subscription because
        // Google Play Services were unavailable at the time and the retry delay is passed.
        when(fxAccount.getDeviceRegistrationVersion()).thenReturn(FxAccountDeviceRegistrator.DEVICE_REGISTRATION_VERSION);
        when(fxAccount.getDevicePushRegistrationError()).thenReturn(FxAccountDeviceRegistrator.ERROR_GCM_DISABLED);
        when(fxAccount.getDevicePushRegistrationErrorTime()).thenReturn(System.currentTimeMillis() -
                                                                        FxAccountDeviceRegistrator.RETRY_TIME_AFTER_GCM_DISABLED_ERROR - 1);
        assertTrue(FxAccountDeviceRegistrator.shouldRegister(fxAccount));

        // Should return false if the device registration is OK, but we didn't get a push subscription because
        // Google Play Services were unavailable at the time and the retry delay has not passed.
        // We assume that RETRY_TIME_AFTER_GCM_DISABLED_ERROR is longer than the time it takes to execute this test :)
        when(fxAccount.getDevicePushRegistrationErrorTime()).thenReturn(System.currentTimeMillis());
        assertFalse(FxAccountDeviceRegistrator.shouldRegister(fxAccount));

        // Should return false if the device registration is OK, but we didn't get a push subscription because
        // an unknown error happened at the time.
        when(fxAccount.getDevicePushRegistrationError()).thenReturn(12345L);
        assertFalse(FxAccountDeviceRegistrator.shouldRegister(fxAccount));
    }

    @Test
    public void shouldRenewRegistration() {
        // Should return true if our last push registration was done a day before our expiration threshold.
        when(fxAccount.getDeviceRegistrationTimestamp()).thenReturn(System.currentTimeMillis() -
                                                                    FxAccountDeviceRegistrator.TIME_BETWEEN_CHANNEL_REGISTRATION_IN_MILLIS -
                                                                    1 * 24 * 60 * 60 * 1000L);

        // Should return false if our last push registration is recent enough.
        // We assume that TIME_BETWEEN_CHANNEL_REGISTRATION_IN_MILLIS is longer than a day + the time it takes to run this test.
        when(fxAccount.getDeviceRegistrationTimestamp()).thenReturn(System.currentTimeMillis() -
                                                                    1 * 24 * 60 * 60 * 1000L);
    }
}
