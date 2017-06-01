/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.os.Bundle;
import android.os.Parcelable;

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
        assertEquals(
                "{\"uid\":\"uid\",\"took\":123,\"deviceID\":\"device-id\",\"version\":1}",
                payload.toString()
        );
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
        assertEquals(
                "{\"devices\":[{\"os\":\"Android\",\"id\":\"80daf12dsadsa4236914cff2cc6e9d0f80a965380e2cf8e976e4004ead887521b5d9\",\"version\":\"53.0a1\"}],\"version\":1}",
                payload.toString()
        );

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
        assertEquals(
                "{\"devices\":[{\"os\":\"Android\",\"id\":\"80daf12dsadsa4236914cff2cc6e9d0f80a965380e2cf8e976e4004ead887521b5d9\",\"version\":\"53.0a1\"},{\"os\":\"iOS\",\"id\":\"fa813452774b3cdc8f5f73290b5346df800f644b7b92a1ab94b6e2af748d261362\",\"version\":\"8.0\"}],\"version\":1}",
                payload.toString()
        );
    }
}