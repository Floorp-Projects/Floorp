/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.nearby

import android.view.View
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.common.api.Status
import com.google.android.gms.nearby.connection.ConnectionInfo
import com.google.android.gms.nearby.connection.ConnectionLifecycleCallback
import com.google.android.gms.nearby.connection.ConnectionResolution
import com.google.android.gms.nearby.connection.ConnectionsClient
import com.google.android.gms.nearby.connection.DiscoveredEndpointInfo
import com.google.android.gms.nearby.connection.EndpointDiscoveryCallback
import com.google.android.gms.nearby.connection.Payload
import com.google.android.gms.nearby.connection.PayloadCallback
import com.google.android.gms.nearby.connection.PayloadTransferUpdate
import mozilla.components.lib.nearby.NearbyConnection.ConnectionState
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito
import org.mockito.Mockito.verify
import java.nio.charset.StandardCharsets
import kotlin.reflect.KClass

@RunWith(AndroidJUnit4::class)
class NearbyConnectionTest {
    private val DEVICE_NAME = "Name"
    private val NEIGHBOR_ID = "123ABC"
    private val NEIGHBOR_NAME = "Neighbor"
    private val AUTHENTICATION_TOKEN = "QZ32"
    private val OUTGOING_MESSAGE = "Hello neighbor!"
    private val INCOMING_MESSAGE = "Hello to you!"

    private lateinit var mockConnectionsClient: ConnectionsClient
    private lateinit var mockNearbyConnectionObserver: NearbyConnectionObserver
    private lateinit var mockConnectionInfo: ConnectionInfo

    private lateinit var nearbyConnection: NearbyConnection
    private lateinit var stateWatchingObserver: NearbyConnectionObserver
    private var state: ConnectionState? = null // mutated by stateWatchingObserver

    @Before
    fun setup() {
        state = null

        // Initialize mocks.
        mockConnectionsClient = mock<ConnectionsClient>()
        mockNearbyConnectionObserver = mock<NearbyConnectionObserver>()
        stateWatchingObserver = object : NearbyConnectionObserver by mockNearbyConnectionObserver {
            override fun onStateUpdated(connectionState: ConnectionState) {
                state = connectionState
            }
        }
        nearbyConnection = NearbyConnection(mockConnectionsClient, DEVICE_NAME)
        mockConnectionInfo = mock<ConnectionInfo>()
        whenever(mockConnectionInfo.endpointName).thenReturn(NEIGHBOR_NAME)
        whenever(mockConnectionInfo.authenticationToken).thenReturn(AUTHENTICATION_TOKEN)
    }

    @Test
    fun `Should make initial onStatusUpdated() call after 1-arg registration`() {
        nearbyConnection.register(stateWatchingObserver)
        assertEquals(ConnectionState.Isolated, state)
    }

    @Test
    fun `Should make initial onStatusUpdated() call after 2-arg registration`() {
        nearbyConnection.register(stateWatchingObserver, mock<View>())
        assertEquals(ConnectionState.Isolated, state)
    }

    @Test
    fun `Should make initial onStatusUpdated() call after 3-arg registration`() {
        val mockLifecycleOwner = mock<LifecycleOwner>()
        val mockLifecycle = mock<Lifecycle>()
        whenever(mockLifecycleOwner.lifecycle).thenReturn(mockLifecycle)
        whenever(mockLifecycle.currentState).thenReturn(androidx.lifecycle.Lifecycle.State.RESUMED)
        nearbyConnection.register(stateWatchingObserver, mockLifecycleOwner, true)
        assertEquals(ConnectionState.Isolated, state)
    }

    private fun buildPayloadTransferUpdate(
        payloadId: Long,
        success: Boolean
    ): PayloadTransferUpdate {
        val update = mock<PayloadTransferUpdate>()
        whenever(update.payloadId).thenReturn(payloadId)
        whenever(update.status).thenReturn(
            if (success) PayloadTransferUpdate.Status.SUCCESS else PayloadTransferUpdate.Status.FAILURE
        )
        return update
    }

    private fun assertState(expectedState: KClass<*>) {
        assertEquals(expectedState.simpleName, state?.name)
    }

    val x = Mockito.mock(ConnectionsClient::class.java)
    private fun testFromAdvertisingToAuthenticating(): ConnectionLifecycleCallback {
        // Should enter advertising state if startAdvertising() succeeds.
        whenever(mockConnectionsClient.startAdvertising(anyString(), anyString(), any(), any()))
            .thenReturn(VoidTask.SUCCESS)
        nearbyConnection.register(stateWatchingObserver)
        assertState(ConnectionState.Isolated::class)
        nearbyConnection.startAdvertising()
        assertState(ConnectionState.Advertising::class)

        // Should enter authenticating state if a connection is simulated.
        val connectionLifecycleCallback = argumentCaptor<ConnectionLifecycleCallback>()
        verify(mockConnectionsClient).startAdvertising(
            anyString(),
            anyString(),
            connectionLifecycleCallback.capture(),
            any()
        )
        connectionLifecycleCallback.value.onConnectionInitiated(NEIGHBOR_ID, mockConnectionInfo)
        assertState(ConnectionState.Authenticating::class)

        // AuthenticatingState should have correct properties.
        val authState = state as? ConnectionState.Authenticating
        assertEquals(NEIGHBOR_ID, authState?.neighborId)
        assertEquals(NEIGHBOR_NAME, authState?.neighborName)
        assertEquals(AUTHENTICATION_TOKEN, authState?.token)

        // Should update endpointIdsToNames.
        assertEquals(1, nearbyConnection.endpointIdsToNames.size)
        assertEquals(NEIGHBOR_NAME, nearbyConnection.endpointIdsToNames[NEIGHBOR_ID])

        // If all goes well, ConnectionLifeCycleCallback will be needed.
        return connectionLifecycleCallback.value
    }

    @Test
    fun `Should behave properly if startAdvertising() leads to connection`() {
        val connectionLifecycleCallback = testFromAdvertisingToAuthenticating()
        // Should enter connecting state if authentication is accepted.
        val authState = state as? ConnectionState.Authenticating
        authState?.accept()
        val payloadCallback = argumentCaptor<PayloadCallback>()
        var string = argumentCaptor<String>()
        verify(mockConnectionsClient).acceptConnection(string.capture(), payloadCallback.capture())
        assertEquals(NEIGHBOR_ID, string.value)
        assertState(ConnectionState.Connecting::class)

        // Should enter ready to send state if connection is completed.
        val mockConnectionResolution = mock<ConnectionResolution>()
        whenever(mockConnectionResolution.status).thenReturn(
            Status(CommonStatusCodes.SUCCESS)
        )
        connectionLifecycleCallback.onConnectionResult(NEIGHBOR_ID, mockConnectionResolution)
        assertState(ConnectionState.ReadyToSend::class)

        // Should enter sending state if sendMessage() called.
        whenever(mockConnectionsClient.sendPayload(anyString(), any())).thenReturn(VoidTask.SUCCESS)
        val returnedPayloadId = nearbyConnection.sendMessage(OUTGOING_MESSAGE)
        assertState(ConnectionState.Sending::class)

        // Should have right payload.
        var payload = argumentCaptor<Payload>()
        string = argumentCaptor<String>()
        verify(mockConnectionsClient).sendPayload(string.capture(), payload.capture())
        assertEquals(returnedPayloadId, payload.value.id)
        assertEquals(Payload.Type.BYTES, payload.value.type)
        assertEquals(
            OUTGOING_MESSAGE,
            payload.value.asBytes()?.let { String(it, StandardCharsets.UTF_8) })
        assertEquals(returnedPayloadId, payload.value.id)

        // Should have correct Sending properties.
        val sendingState = state as? ConnectionState.Sending
        assertEquals(NEIGHBOR_ID, sendingState?.neighborId)
        assertEquals(NEIGHBOR_NAME, sendingState?.neighborName)
        assertEquals(payload.value.id, sendingState?.payloadId)

        // Should not change state if failing PayloadTransferUpdate comes in.
        payloadCallback.value.onPayloadTransferUpdate(
            NEIGHBOR_ID,
            buildPayloadTransferUpdate(payload.value.id, success = false)
        )
        assertState(ConnectionState.Sending::class)

        // Should not change state if PayloadTransferUpdate with wrong id comes in.
        payloadCallback.value.onPayloadTransferUpdate(
            NEIGHBOR_ID,
            buildPayloadTransferUpdate(payload.value.id + 1, success = true)
        )
        assertTrue(state is ConnectionState.Sending)

        // Should change state to ReadyToSend if succeeding PayloadTransferUpdate comes in.
        payloadCallback.value.onPayloadTransferUpdate(
            NEIGHBOR_ID,
            buildPayloadTransferUpdate(payload.value.id, success = true)
        )
        assertState(ConnectionState.ReadyToSend::class)

        // Should call observer's onMessageReceived() if message received.
        val incomingPayload: Payload =
            Payload.fromBytes(INCOMING_MESSAGE.toByteArray(StandardCharsets.UTF_8))
        payloadCallback.value.onPayloadReceived(NEIGHBOR_ID, incomingPayload)
        val neighborName = argumentCaptor<String>()
        val neighborId = argumentCaptor<String>()
        val message = argumentCaptor<String>()
        verify(mockNearbyConnectionObserver).onMessageReceived(
            neighborId.capture(),
            neighborName.capture(),
            message.capture()
        )
        assertEquals(NEIGHBOR_ID, neighborId.value)
        assertEquals(NEIGHBOR_NAME, neighborName.value)
        assertEquals(INCOMING_MESSAGE, message.value)
        assertState(ConnectionState.ReadyToSend::class)

        // Should change state to isolated if disconnected.
        nearbyConnection.disconnect()
        assertState(ConnectionState.Isolated::class)
        verify(mockConnectionsClient).stopAllEndpoints()
    }

    @Test
    fun `Should behave properly if startAdvertising() leads to rejected authentication`() {
        testFromAdvertisingToAuthenticating()
        // Should enter connecting state if authentication is accepted.
        val authState = state as? ConnectionState.Authenticating
        authState?.reject()

        val neighborId = argumentCaptor<String>()
        verify(mockConnectionsClient).rejectConnection(neighborId.capture())
        assertEquals(NEIGHBOR_ID, neighborId.value)
        assertState(ConnectionState.Advertising::class)
    }

    @Test
    fun `Should enter failure state if startAdvertising() fails`() {
        whenever(mockConnectionsClient.startAdvertising(anyString(), anyString(), any(), any()))
            .thenReturn(VoidTask.FAILURE)
        nearbyConnection.register(stateWatchingObserver)
        assertState(ConnectionState.Isolated::class)
        nearbyConnection.startAdvertising()
        assertState(ConnectionState.Failure::class)
    }

    private fun testFromDiscoveryToEndpointDiscovery(): EndpointDiscoveryCallback {
        // Should enter discovering state if startDiscovery() succeeds.
        whenever(mockConnectionsClient.startDiscovery(anyString(), any(), any()))
            .thenReturn(VoidTask.SUCCESS)
        nearbyConnection.register(stateWatchingObserver)
        assertState(ConnectionState.Isolated::class)
        nearbyConnection.startDiscovering()
        assertState(ConnectionState.Discovering::class)

        // Should call startDiscovery() with right arguments.
        val endpointDiscoveryCallback = argumentCaptor<EndpointDiscoveryCallback>()
        val packageName = argumentCaptor<String>()
        verify(mockConnectionsClient).startDiscovery(
            packageName.capture(),
            endpointDiscoveryCallback.capture(),
            any()
        )
        assertEquals(NearbyConnection.PACKAGE_NAME, packageName.value)

        // Return endpointDiscoveryCallback, so client can call either of its methods.
        return endpointDiscoveryCallback.value
    }

    private fun testDiscoveryThroughRequestConnection(): ConnectionLifecycleCallback {
        val endpointDiscoveryCallback = testFromDiscoveryToEndpointDiscovery()

        // Should enter initiating state if an endpoint is found.
        val mockDiscoveredEndpointInfo = mock<DiscoveredEndpointInfo>()
        whenever(mockDiscoveredEndpointInfo.endpointName).thenReturn(NEIGHBOR_NAME)
        endpointDiscoveryCallback.onEndpointFound(NEIGHBOR_ID, mockDiscoveredEndpointInfo)
        assertState(ConnectionState.Initiating::class)

        // Initiating state should have correct properties.
        val initiatingState = state as? ConnectionState.Initiating
        assertEquals(NEIGHBOR_ID, initiatingState?.neighborId)
        assertEquals(NEIGHBOR_NAME, initiatingState?.neighborName)

        // Should call requestConnection() with right arguments.
        val name = argumentCaptor<String>()
        var neighborId = argumentCaptor<String>()
        val connectionLifecycleCallback = argumentCaptor<ConnectionLifecycleCallback>()
        verify(mockConnectionsClient).requestConnection(
            name.capture(),
            neighborId.capture(),
            connectionLifecycleCallback.capture()
        )
        assertEquals(DEVICE_NAME, name.value)
        assertEquals(NEIGHBOR_ID, neighborId.value)

        return connectionLifecycleCallback.value
    }

    @Test
    fun `Should behave properly if startDiscovery() leads to a connection`() {
        val connectionLifecycleCallback = testDiscoveryThroughRequestConnection()

        // Should enter authenticating state when connection initiated. called.
        connectionLifecycleCallback.onConnectionInitiated(NEIGHBOR_ID, mockConnectionInfo)
        assertState(ConnectionState.Authenticating::class)

        // Should return to discovering state if authentication rejected.
        // (The test of advertising verified what happens if authentication accepted.)
        val authState = state as? ConnectionState.Authenticating
        authState?.reject()
        val neighborId = argumentCaptor<String>()
        verify(mockConnectionsClient).rejectConnection(neighborId.capture())
        assertEquals(NEIGHBOR_ID, neighborId.value)
        assertState(ConnectionState.Discovering::class)
    }

    @Test
    fun `Should enter failure state if startDiscovery() fails`() {
        whenever(mockConnectionsClient.startDiscovery(anyString(), any(), any()))
            .thenReturn(VoidTask.FAILURE)
        nearbyConnection.register(stateWatchingObserver)
        assertState(ConnectionState.Isolated::class)
        nearbyConnection.startDiscovering()
        assertState(ConnectionState.Failure::class)
    }

    @Test
    fun `Should remain in discovery state if startDiscovery() leads to lost endpoint`() {
        val endpointDiscoveryCallback = testFromDiscoveryToEndpointDiscovery()

        // Should enter initiating state if an endpoint is found.
        val mockDiscoveredEndpointInfo = mock<DiscoveredEndpointInfo>()
        whenever(mockDiscoveredEndpointInfo.endpointName).thenReturn(NEIGHBOR_NAME)
        endpointDiscoveryCallback.onEndpointLost(NEIGHBOR_ID)
        assertState(ConnectionState.Discovering::class)
    }

    @Test
    fun `Should return null if sendMessage() called from wrong state`() {
        nearbyConnection.register(stateWatchingObserver)
        assertState(ConnectionState.Isolated::class)
        assertNull(nearbyConnection.sendMessage(OUTGOING_MESSAGE))
    }

    @Test
    fun `Should enter failure state if failing ConnectionResolution provided`() {
        val connectionLifecycleCallback = testFromAdvertisingToAuthenticating()
        val mockConnectionResolution = mock<ConnectionResolution>()
        whenever(mockConnectionResolution.status).thenReturn(Status(CommonStatusCodes.ERROR))
        connectionLifecycleCallback.onConnectionResult(NEIGHBOR_ID, mockConnectionResolution)
        assertState(ConnectionState.Failure::class)
    }

    @Test
    fun `Should encode and decode short message as bytes`() {
        val payload = nearbyConnection.stringToPayload(OUTGOING_MESSAGE)
        assertTrue(payload.type == Payload.Type.BYTES)
        assertEquals(OUTGOING_MESSAGE, nearbyConnection.payloadToString(payload))
    }

    @Test
    fun `Should encode and decode borderline message as bytes`() {
        // This tests the case where the message length is exactly equal to the bytes limit.
        val bytes = ByteArray(ConnectionsClient.MAX_BYTES_DATA_SIZE, { _ -> 0 })
        val message = String(bytes, NearbyConnection.PAYLOAD_ENCODING)
        val payload = nearbyConnection.stringToPayload(message)
        assertTrue(payload.type == Payload.Type.BYTES)
        assertEquals(message, nearbyConnection.payloadToString(payload))
    }

    fun `Should encode and decode long message as stream`() {
        // This tests the case where the message length is one more than the bytes limit.
        val bytes = ByteArray(ConnectionsClient.MAX_BYTES_DATA_SIZE, { _ -> 0 })
        val message = String(bytes, NearbyConnection.PAYLOAD_ENCODING)
        val payload = nearbyConnection.stringToPayload(message)
        assertEquals(Payload.Type.STREAM, payload.type)
        assertEquals(message, nearbyConnection.payloadToString(payload))
    }
}
