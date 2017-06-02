/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.os.Bundle;
import android.os.Parcelable;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.simple.JSONArray;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.telemetry.TelemetryLocalPing;

import java.util.ArrayList;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class TelemetrySyncPingBuilderTest {
    private TelemetrySyncPingBundleBuilderTest.MockTelemetryPingStore pingStore;
    private TelemetrySyncPingBuilder builder;

    @Before
    public void setUp() throws Exception {
        pingStore = new TelemetrySyncPingBundleBuilderTest.MockTelemetryPingStore();
        builder = new TelemetrySyncPingBuilder();
    }

    @Test
    public void testGeneralShape() throws Exception {
        TelemetryLocalPing localPing = builder
                .setDeviceID("device-id")
                .setUID("uid")
                .setTook(123L)
                .setRestarted(false)
                .build();
        ExtendedJSONObject payload = localPing.getPayload();
        assertEquals("uid", payload.getString("uid"));
        assertEquals(Long.valueOf(123L), payload.getLong("took"));
        assertEquals("device-id", payload.getString("deviceID"));
        assertEquals(Integer.valueOf(1), payload.getIntegerSafely("version"));
        assertFalse(payload.containsKey("restarted"));

        localPing = builder
                .setDeviceID("device-id")
                .setUID("uid")
                .setTook(123L)
                .setRestarted(true)
                .build();
        payload = localPing.getPayload();
        assertEquals("uid", payload.getString("uid"));
        assertEquals(Long.valueOf(123L), payload.getLong("took"));
        assertEquals("device-id", payload.getString("deviceID"));
        assertEquals(Integer.valueOf(1), payload.getIntegerSafely("version"));
        assertEquals(true, payload.getBoolean("restarted"));
    }

    @Test
    public void testDevices() throws Exception {
        ArrayList<Parcelable> devices = new ArrayList<>();

        TelemetryLocalPing localPing = builder
                .setDevices(devices)
                .build();
        ExtendedJSONObject payload = localPing.getPayload();

        // Empty list isn't part of the JSON.
        assertEquals(
                "{\"version\":1}",
                payload.toString()
        );

        Bundle device = new Bundle();
        device.putString("os", "Android");
        device.putString("version", "53.0a1");
        device.putString("id", "80daf12dsadsa4236914cff2cc6e9d0f80a965380e2cf8e976e4004ead887521b5d9");
        devices.add(device);

        // Test with only one device
        payload = builder
                .setDevices(devices)
                .build()
                .getPayload();
        JSONArray devicesJSON = payload.getArray("devices");
        assertEquals(1, devicesJSON.size());
        assertDevice((ExtendedJSONObject) devicesJSON.get(0), "Android", "53.0a1", "80daf12dsadsa4236914cff2cc6e9d0f80a965380e2cf8e976e4004ead887521b5d9");

        device = new Bundle();
        device.putString("os", "iOS");
        device.putString("version", "8.0");
        device.putString("id", "fa813452774b3cdc8f5f73290b5346df800f644b7b92a1ab94b6e2af748d261362");
        devices.add(device);

        // Test with more than one device
        payload = builder
                .setDevices(devices)
                .build()
                .getPayload();
        devicesJSON = payload.getArray("devices");
        assertEquals(2, devicesJSON.size());
        assertDevice((ExtendedJSONObject) devicesJSON.get(0), "Android", "53.0a1", "80daf12dsadsa4236914cff2cc6e9d0f80a965380e2cf8e976e4004ead887521b5d9");
        assertDevice((ExtendedJSONObject) devicesJSON.get(1), "iOS", "8.0", "fa813452774b3cdc8f5f73290b5346df800f644b7b92a1ab94b6e2af748d261362");
    }

    private void assertDevice(ExtendedJSONObject device, String os, String version, String id) throws JSONException {
        assertEquals(os, device.getString("os"));
        assertEquals(version, device.getString("version"));
        assertEquals(id, device.getString("id"));
    }
}