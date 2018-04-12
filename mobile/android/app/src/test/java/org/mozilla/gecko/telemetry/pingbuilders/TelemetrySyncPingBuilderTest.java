/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.telemetry.pingbuilders;

import android.os.Bundle;
import android.os.Parcelable;

import org.json.JSONException;

import org.json.simple.JSONArray;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.synchronizer.StoreBatchTracker;
import org.mozilla.gecko.sync.telemetry.TelemetryStageCollector;
import org.mozilla.gecko.telemetry.TelemetryLocalPing;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.HashMap;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
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
                .setTook(123L)
                .setRestarted(false)
                .build();
        ExtendedJSONObject payload = localPing.getPayload();
        assertEquals(Long.valueOf(123L), payload.getLong("took"));
        assertFalse(payload.containsKey("restarted"));

        localPing = builder
                .setTook(123L)
                .setRestarted(true)
                .build();
        payload = localPing.getPayload();
        assertEquals(Long.valueOf(123L), payload.getLong("took"));
        assertTrue(payload.getLong("when") != null);
        assertEquals(true, payload.getBoolean("restarted"));

    }

    @Test
    public void testStage() throws Exception {
        HashMap<String, TelemetryStageCollector> stages = new HashMap<>();
        TelemetryStageCollector stage = new TelemetryStageCollector(null);
        stage.validation = new ExtendedJSONObject();
        stage.validation.put("took", 1L);
        stage.validation.put("checked", 0L);
        stage.validation.put("problems", new JSONArray());

        stage.error = new ExtendedJSONObject();
        stage.error.put("name", "unexpectederror");
        stage.error.put("error", "test");
        stage.started = 100;
        stage.finished = 105;
        stage.inbound = 5;
        stage.inboundStored = 3;
        stage.inboundFailed = 1;
        stage.reconciled = 1;

        stage.outbound = new ArrayList<>();
        stage.outbound.add(new StoreBatchTracker.Batch(1, 1));
        stage.outbound.add(new StoreBatchTracker.Batch(9, 0));
        stage.outbound.add(new StoreBatchTracker.Batch(0, 0));

        stages.put("testing", stage);

        TelemetryStageCollector stage2 = new TelemetryStageCollector(null);
        // If it's actually completely empty, it will get omitted.
        stage2.inbound = 1;
        stage2.inboundStored = 1;
        stages.put("testing2", stage2);

        TelemetryLocalPing localPing = builder
                .setStages(stages)
                .build();
        ExtendedJSONObject payload = localPing.getPayload();

        assertEquals(2, payload.getArray("engines").size());
        ExtendedJSONObject engine = (ExtendedJSONObject) payload.getArray("engines").get(0);

        assertEquals("testing", engine.getString("name"));
        assertEquals(Long.valueOf(5L), engine.getLong("took"));

        ExtendedJSONObject inbound = engine.getObject("incoming");
        assertEquals(Integer.valueOf(stage.inbound), inbound.getIntegerSafely("applied"));
        assertEquals(Integer.valueOf(stage.inboundStored), inbound.getIntegerSafely("succeeded"));
        assertEquals(Integer.valueOf(stage.inboundFailed), inbound.getIntegerSafely("failed"));
        assertEquals(Integer.valueOf(stage.reconciled), inbound.getIntegerSafely("reconciled"));

        ExtendedJSONObject error = engine.getObject("failureReason");
        assertEquals("unexpectederror", error.getString("name"));
        assertEquals("test", error.getString("error"));

        ExtendedJSONObject validation = engine.getObject("validation");
        assertEquals(stage.validation.getLong("took"), validation.getLong("took"));
        assertEquals(stage.validation.getLong("checked"), validation.getLong("checked"));
        assertEquals(0, stage.validation.getArray("problems").size());

        JSONArray outgoing = engine.getArray("outgoing");
        assertEquals(outgoing.size(), 3);

        ExtendedJSONObject firstBatch = (ExtendedJSONObject) outgoing.get(0);
        assertEquals(firstBatch.getLong("sent", -1), 1);
        assertEquals(firstBatch.getLong("failed", -1), 1);

        ExtendedJSONObject secondBatch = (ExtendedJSONObject) outgoing.get(1);
        assertEquals(secondBatch.getLong("sent", -1), 9);
        assertFalse(secondBatch.containsKey("failed"));

        // Ensure we include "all zero" batches, since we can actually send those on android.
        ExtendedJSONObject lastBatch = (ExtendedJSONObject) outgoing.get(2);
        assertFalse(lastBatch.containsKey("sent"));
        assertFalse(lastBatch.containsKey("failed"));

        ExtendedJSONObject emptyEngine = (ExtendedJSONObject) payload.getArray("engines").get(1);
        assertFalse(emptyEngine.containsKey("outgoing"));
    }

    @Test
    public void testDevices() throws Exception {
        ArrayList<Parcelable> devices = new ArrayList<>();

        TelemetryLocalPing localPing = builder
                .setDevices(devices)
                .build();
        ExtendedJSONObject payload = localPing.getPayload();

        // Empty list isn't part of the JSON.
        assertTrue(payload.containsKey("when"));
        assertEquals(1, payload.keySet().size());

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