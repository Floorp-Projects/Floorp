package com.keepsafe.switchboard;

import android.content.Context;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.UUID;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class TestSwitchboard {

    @Test
    public void testDeviceUuidFactory() {
        final Context c = RuntimeEnvironment.application;
        final DeviceUuidFactory df = new DeviceUuidFactory(c);
        final UUID uuid = df.getDeviceUuid();
        assertNotNull("UUID is not null", uuid);
        assertEquals("DeviceUuidFactory always returns the same UUID", df.getDeviceUuid(), uuid);
    }
}
