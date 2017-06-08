/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.sync.telemetry;

import android.os.Bundle;

import org.json.simple.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.sync.CollectionConcurrentModificationException;
import org.mozilla.gecko.sync.ExtendedJSONObject;
import org.mozilla.gecko.sync.HTTPFailureException;
import org.mozilla.gecko.sync.SyncDeadlineReachedException;
import org.mozilla.gecko.sync.Utils;
import org.mozilla.gecko.sync.net.SyncStorageResponse;
import org.mozilla.gecko.sync.repositories.FetchFailedException;
import org.mozilla.gecko.sync.repositories.StoreFailedException;
import org.mozilla.gecko.sync.repositories.domain.ClientRecord;

import java.util.ArrayList;
import java.util.ConcurrentModificationException;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.doReturn;

import static org.junit.Assert.*;

@RunWith(TestRunner.class)
public class TelemetryCollectorTest {
    private TelemetryCollector collector;

    @Before
    public void setUp() throws Exception {
        collector = new TelemetryCollector();
    }

    @Test
    public void testSetIDs() throws Exception {
        final String deviceID = "random-device-guid";
        final String uid = "already-hashed-test-uid";

        collector.setIDs(uid, deviceID);
        collector.setStarted(5L);
        collector.setFinished(10L);
        final Bundle bundle = collector.build();

        // Setting UID is a pass-through, since we're expecting it to be hashed already.
        assertEquals(uid, bundle.get("uid"));
        // Expect device ID to be hashed with the UID.
        assertEquals(
                Utils.byte2Hex(Utils.sha256(deviceID.concat(uid).getBytes("UTF-8"))),
                bundle.get("deviceID")
        );
    }

    @Test(expected = IllegalStateException.class)
    public void testAddingDevicesBeforeSettingUID() throws Exception {
        collector.addDevice(new ClientRecord("client1"));
    }

    @Test
    public void testAddingDevices() throws Exception {
        ClientRecord client1 = new ClientRecord("client1-guid");
        client1.os = "iOS";
        client1.version = "1.33.7";

        collector.setStarted(5L);
        collector.setFinished(10L);

        collector.setIDs("hashed-uid", "deviceID");

        // Test that client device ID is hashed together with UID
        collector.addDevice(client1);

        Bundle data = collector.build();
        ArrayList<Bundle> devices = data.getParcelableArrayList("devices");
        assertEquals(1, devices.size());
        assertEquals(
                Utils.byte2Hex(Utils.sha256("client1-guid".concat("hashed-uid").getBytes("UTF-8"))),
                devices.get(0).getString("id")
        );
        assertEquals("iOS", devices.get(0).getString("os"));
        assertEquals("1.33.7", devices.get(0).getString("version"));

        // Test that we can add more than just one device
        ClientRecord client2 = new ClientRecord("client2-guid");
        client2.os = "Android";
        client2.version = "55.0a1";
        collector.addDevice(client2);

        data = collector.build();
        devices = data.getParcelableArrayList("devices");
        assertEquals(2, devices.size());

        assertEquals("iOS", devices.get(0).getString("os"));
        assertEquals("1.33.7", devices.get(0).getString("version"));
        assertEquals(
                Utils.byte2Hex(Utils.sha256("client1-guid".concat("hashed-uid").getBytes("UTF-8"))),
                devices.get(0).getString("id")
        );

        assertEquals("Android", devices.get(1).getString("os"));
        assertEquals("55.0a1", devices.get(1).getString("version"));
        assertEquals(
                Utils.byte2Hex(Utils.sha256("client2-guid".concat("hashed-uid").getBytes("UTF-8"))),
                devices.get(1).getString("id")
        );
    }

    @Test
    public void testDuration() throws Exception {
        collector.setStarted(5L);
        collector.setFinished(11L);
        Bundle data = collector.build();

        assertEquals(6L, data.getLong("took"));
    }

    @Test(expected = IllegalStateException.class)
    public void testDurationMissingStarted() throws Exception {
        collector.setFinished(10L);
        collector.build();
    }

    @Test(expected = IllegalStateException.class)
    public void testDurationMissingFinished() throws Exception {
        collector.setStarted(10L);
        collector.build();
    }

    @Test
    public void testError() throws Exception {
        collector.setStarted(1L);
        collector.setFinished(2L);

        // Test that we can build without setting an error.
        assertFalse(collector.hasError());
        Bundle data = collector.build();
        assertFalse(data.containsKey("error"));

        // Test various ways to set an error.
        // Just details.
        collector.setError("testError", "unexpectedStuff");
        data = collector.build();
        assertTrue(data.containsKey("error"));
        JSONObject errorJson = (JSONObject) data.getSerializable("error");
        assertEquals("testError", errorJson.get("name"));
        assertEquals("unexpectedStuff", errorJson.get("error"));
        assertTrue(collector.hasError());

        // Just exception.
        collector.setError("exceptionTest", new IllegalArgumentException());
        data = collector.build();
        assertTrue(data.containsKey("error"));
        errorJson = (JSONObject) data.getSerializable("error");
        assertEquals("exceptionTest", errorJson.get("name"));
        assertEquals("IllegalArgumentException", errorJson.get("error"));

        // Details and exception.
        collector.setError("anotherTest", "Error details", new ConcurrentModificationException());
        data = collector.build();
        assertTrue(data.containsKey("error"));
        errorJson = (JSONObject) data.getSerializable("error");
        assertEquals("anotherTest", errorJson.get("name"));
        assertEquals("ConcurrentModificationException:Error details", errorJson.get("error"));

        // Details and explicit null exception.
        collector.setError("noExceptionTest", "Error details", null);
        data = collector.build();
        assertTrue(data.containsKey("error"));
        errorJson = (JSONObject) data.getSerializable("error");
        assertEquals("noExceptionTest", errorJson.get("name"));
        assertEquals("Error details", errorJson.get("error"));
    }

    @Test
    public void testRestarted() throws Exception {
        collector.setStarted(5L);
        collector.setFinished(10L);

        Bundle data = collector.build();
        assertFalse(data.getBoolean("restarted"));
        assertFalse(data.containsKey("restarted"));

        collector.setRestarted();

        assertTrue(collector.build().getBoolean("restarted"));
    }

    @Test
    public void testCollectorFor() throws Exception {
        // Test that we'll get the same stage collector for the same stage name
        TelemetryStageCollector stageCollector = collector.collectorFor("test");
        TelemetryStageCollector stageCollector2 = collector.collectorFor("test");
        assertEquals(stageCollector, stageCollector2);

        // Test that we won't just keep getting the same stage collector for different stages
        TelemetryStageCollector stageCollector3 = collector.collectorFor("another");
        assertNotEquals(stageCollector, stageCollector3);
    }

    @Test
    public void testStageErrorBuilder() throws Exception {
        TelemetryCollector.StageErrorBuilder builder = new TelemetryCollector.StageErrorBuilder("test", "sampleerror");
        ExtendedJSONObject error = builder.build();

        // Test setting error name/details manually.
        assertEquals("test", error.getString("name"));
        assertEquals("sampleerror", error.getString("error"));

        SyncStorageResponse response400 = mock(SyncStorageResponse.class);
        doReturn(400).when(response400).getStatusCode();

        // Test setting an optional HTTP exception while manually setting error/details.
        builder.setLastException(new HTTPFailureException(response400));

        error = builder.build();
        assertEquals("test", error.getString("name"));
        assertEquals("sampleerror", error.getString("error"));
        assertEquals(Integer.valueOf(400), error.getIntegerSafely("code"));

        // Test deadline reached errors
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(SyncDeadlineReachedException.class));

        error = builder.build();
        assertEquals("unexpected", error.getString("name"));
        assertEquals("syncdeadline", error.getString("error"));

        // Test that internal concurrent modification exceptions are treated as HTTP 412
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(CollectionConcurrentModificationException.class));

        error = builder.build();
        assertEquals("httperror", error.getString("name"));
        assertEquals(Integer.valueOf(412), error.getIntegerSafely("code"));

        // Test a non-401 HTTP exception
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(new HTTPFailureException(response400));

        error = builder.build();
        assertEquals("httperror", error.getString("name"));
        assertEquals(Integer.valueOf(400), error.getIntegerSafely("code"));

        // Test 401 HTTP exceptions
        SyncStorageResponse response401 = mock(SyncStorageResponse.class);
        doReturn(401).when(response401).getStatusCode();

        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(new HTTPFailureException(response401));

        error = builder.build();
        assertEquals("autherror", error.getString("name"));
        assertEquals("storage", error.getString("from"));

        // Test generic exception handling
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(new NullPointerException());

        error = builder.build();
        assertEquals("unexpected", error.getString("name"));
        assertEquals("NullPointerException", error.getString("error"));

        // Test exceptions encountered during a fetch phase of a synchronizer
        // Generic network error
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(FetchFailedException.class))
                .setFetchException(new java.net.SocketException());

        error = builder.build();
        assertEquals("networkerror", error.getString("name"));
        assertEquals("fetch:SocketException", error.getString("error"));

        // Non-network error
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(FetchFailedException.class))
                .setFetchException(new IllegalStateException());

        error = builder.build();
        assertEquals("othererror", error.getString("name"));
        assertEquals("fetch:IllegalStateException", error.getString("error"));

        // Missing specific error
        // Non-network error
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(FetchFailedException.class));

        error = builder.build();
        assertEquals("othererror", error.getString("name"));
        assertEquals("fetch:unknown", error.getString("error"));

        // Test exceptions encountered during a store phase of a synchronizer
        // Generic network error
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(StoreFailedException.class))
                .setStoreException(new java.net.SocketException());

        error = builder.build();
        assertEquals("networkerror", error.getString("name"));
        assertEquals("store:SocketException", error.getString("error"));

        // Non-network error
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(StoreFailedException.class))
                .setStoreException(new IllegalStateException());

        error = builder.build();
        assertEquals("othererror", error.getString("name"));
        assertEquals("store:IllegalStateException", error.getString("error"));

        // Missing specific error
        // Non-network error
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(StoreFailedException.class));

        error = builder.build();
        assertEquals("othererror", error.getString("name"));
        assertEquals("store:unknown", error.getString("error"));

        // Test ability to blindly set both types of exceptions (store & fetch)
        // one of them being null. Just as the ServerSyncStage does on failures.
        // Store exceptions
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(StoreFailedException.class))
                .setFetchException(null)
                .setStoreException(new IllegalArgumentException());

        error = builder.build();
        assertEquals("othererror", error.getString("name"));
        assertEquals("store:IllegalArgumentException", error.getString("error"));

        // Fetch exceptions
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(FetchFailedException.class))
                .setFetchException(new java.net.SocketException())
                .setStoreException(null);

        error = builder.build();
        assertEquals("networkerror", error.getString("name"));
        assertEquals("fetch:SocketException", error.getString("error"));

        // Both types, but indicating that last one was a fetch exception
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(FetchFailedException.class))
                .setFetchException(new java.net.SocketException())
                .setStoreException(new IllegalStateException());

        error = builder.build();
        assertEquals("networkerror", error.getString("name"));
        assertEquals("fetch:SocketException", error.getString("error"));

        // Both types, but indicating that last one was a store exception
        builder = new TelemetryCollector.StageErrorBuilder();
        builder.setLastException(mock(StoreFailedException.class))
                .setFetchException(new java.net.SocketException())
                .setStoreException(new IllegalStateException());

        error = builder.build();
        assertEquals("othererror", error.getString("name"));
        assertEquals("store:IllegalStateException", error.getString("error"));
    }
}