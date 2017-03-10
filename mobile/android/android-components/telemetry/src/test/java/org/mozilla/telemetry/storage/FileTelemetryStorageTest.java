/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.telemetry.storage;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.telemetry.config.TelemetryConfiguration;
import org.mozilla.telemetry.ping.TelemetryPing;
import org.mozilla.telemetry.serialize.TelemetryPingSerializer;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.UUID;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;

import static org.junit.Assert.*;
import static org.mockito.Mockito.verify;

@RunWith(RobolectricTestRunner.class)
public class FileTelemetryStorageTest {
    private static final String TEST_PING_TYPE = "test";
    private static final String TEST_UPLOAD_PATH = "/some/random/path/upload";
    private static final String TEST_SERIALIZED_PING = "Hello Test";

    @Test
    public void testStoringAndReading() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final String documentId = UUID.randomUUID().toString();

        final TelemetryPing ping = mock(TelemetryPing.class);
        doReturn(TEST_PING_TYPE).when(ping).getType();
        doReturn(documentId).when(ping).getDocumentId();
        doReturn(TEST_UPLOAD_PATH).when(ping).getUploadPath();

        final TelemetryPingSerializer serializer = mock(TelemetryPingSerializer.class);
        doReturn(TEST_SERIALIZED_PING).when(serializer).serialize(ping);

        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);
        storage.store(ping);

        TelemetryStorage.TelemetryStorageCallback callback = spy(new TelemetryStorage.TelemetryStorageCallback() {
            @Override
            public boolean onTelemetryPingLoaded(String path, String serializedPing) {
                assertEquals(TEST_UPLOAD_PATH, path);
                assertEquals(TEST_SERIALIZED_PING, serializedPing);
                return true;
            }
        });

        final boolean processed = storage.process(TEST_PING_TYPE, callback);

        assertTrue(processed);

        verify(callback).onTelemetryPingLoaded(TEST_UPLOAD_PATH, TEST_SERIALIZED_PING);
    }

    @Test
    public void testReturningFalseInCallbackReturnsFalseFromProcess() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final String documentId = UUID.randomUUID().toString();

        final TelemetryPing ping = mock(TelemetryPing.class);
        doReturn(TEST_PING_TYPE).when(ping).getType();
        doReturn(documentId).when(ping).getDocumentId();
        doReturn(TEST_UPLOAD_PATH).when(ping).getUploadPath();

        final TelemetryPingSerializer serializer = mock(TelemetryPingSerializer.class);
        doReturn(TEST_SERIALIZED_PING).when(serializer).serialize(ping);

        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);
        storage.store(ping);

        TelemetryStorage.TelemetryStorageCallback callback = spy(new TelemetryStorage.TelemetryStorageCallback() {
            @Override
            public boolean onTelemetryPingLoaded(String path, String serializedPing) {
                assertEquals(TEST_UPLOAD_PATH, path);
                assertEquals(TEST_SERIALIZED_PING, serializedPing);
                return false;
            }
        });

        final boolean processed = storage.process(TEST_PING_TYPE, callback);

        assertFalse(processed);

        verify(callback).onTelemetryPingLoaded(TEST_UPLOAD_PATH, TEST_SERIALIZED_PING);
    }

    @Test
    public void testPingIsRemovedAfterProcessing() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final String documentId = UUID.randomUUID().toString();

        final TelemetryPing ping = mock(TelemetryPing.class);
        doReturn(TEST_PING_TYPE).when(ping).getType();
        doReturn(documentId).when(ping).getDocumentId();
        doReturn(TEST_UPLOAD_PATH).when(ping).getUploadPath();

        final TelemetryPingSerializer serializer = mock(TelemetryPingSerializer.class);
        doReturn(TEST_SERIALIZED_PING).when(serializer).serialize(ping);

        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);
        storage.store(ping);

        TelemetryStorage.TelemetryStorageCallback callback = spy(new TelemetryStorage.TelemetryStorageCallback() {
            @Override
            public boolean onTelemetryPingLoaded(String path, String serializedPing) {
                assertEquals(TEST_UPLOAD_PATH, path);
                assertEquals(TEST_SERIALIZED_PING, serializedPing);
                return true;
            }
        });

        final boolean processed = storage.process(TEST_PING_TYPE, callback);

        assertTrue(processed);

        verify(callback).onTelemetryPingLoaded(TEST_UPLOAD_PATH, TEST_SERIALIZED_PING);

        TelemetryStorage.TelemetryStorageCallback callback2 = spy(new TelemetryStorage.TelemetryStorageCallback() {
            @Override
            public boolean onTelemetryPingLoaded(String path, String serializedPing) {
                return true;
            }
        });

        final boolean processed2 = storage.process(TEST_PING_TYPE, callback2);

        assertTrue(processed2);

        verify(callback2, never()).onTelemetryPingLoaded(anyString(), anyString());
    }

    @Test
    public void testPingIsNotRemovedAfterUnsuccessfulProcessing() {
        final TelemetryConfiguration configuration = new TelemetryConfiguration(RuntimeEnvironment.application);

        final String documentId = UUID.randomUUID().toString();

        final TelemetryPing ping = mock(TelemetryPing.class);
        doReturn(TEST_PING_TYPE).when(ping).getType();
        doReturn(documentId).when(ping).getDocumentId();
        doReturn(TEST_UPLOAD_PATH).when(ping).getUploadPath();

        final TelemetryPingSerializer serializer = mock(TelemetryPingSerializer.class);
        doReturn(TEST_SERIALIZED_PING).when(serializer).serialize(ping);

        final FileTelemetryStorage storage = new FileTelemetryStorage(configuration, serializer);
        storage.store(ping);

        TelemetryStorage.TelemetryStorageCallback callback = spy(new TelemetryStorage.TelemetryStorageCallback() {
            @Override
            public boolean onTelemetryPingLoaded(String path, String serializedPing) {
                assertEquals(TEST_UPLOAD_PATH, path);
                assertEquals(TEST_SERIALIZED_PING, serializedPing);
                return false;
            }
        });

        final boolean processed = storage.process(TEST_PING_TYPE, callback);

        assertFalse(processed);

        verify(callback).onTelemetryPingLoaded(TEST_UPLOAD_PATH, TEST_SERIALIZED_PING);

        TelemetryStorage.TelemetryStorageCallback callback2 = spy(new TelemetryStorage.TelemetryStorageCallback() {
            @Override
            public boolean onTelemetryPingLoaded(String path, String serializedPing) {
                return true;
            }
        });

        final boolean processed2 = storage.process(TEST_PING_TYPE, callback2);

        assertTrue(processed2);

        verify(callback2).onTelemetryPingLoaded(TEST_UPLOAD_PATH, TEST_SERIALIZED_PING);
    }
}