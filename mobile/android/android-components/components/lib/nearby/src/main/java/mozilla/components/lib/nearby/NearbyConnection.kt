/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.nearby

import android.Manifest
import android.content.Context
import android.os.Build
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.lifecycle.LifecycleOwner
import com.google.android.gms.nearby.Nearby
import com.google.android.gms.nearby.connection.AdvertisingOptions
import com.google.android.gms.nearby.connection.ConnectionInfo
import com.google.android.gms.nearby.connection.ConnectionLifecycleCallback
import com.google.android.gms.nearby.connection.ConnectionResolution
import com.google.android.gms.nearby.connection.ConnectionsClient
import com.google.android.gms.nearby.connection.DiscoveredEndpointInfo
import com.google.android.gms.nearby.connection.DiscoveryOptions
import com.google.android.gms.nearby.connection.EndpointDiscoveryCallback
import com.google.android.gms.nearby.connection.Payload
import com.google.android.gms.nearby.connection.PayloadCallback
import com.google.android.gms.nearby.connection.PayloadTransferUpdate
import com.google.android.gms.nearby.connection.PayloadTransferUpdate.Status
import com.google.android.gms.nearby.connection.Strategy
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.io.ByteArrayInputStream
import java.nio.charset.Charset
import java.util.concurrent.ConcurrentHashMap

/**
 * A class that can be run on two devices to allow them to connect. This supports sending a single
 * message at a time in each direction. It contains internal synchronization and may be accessed
 * from any thread.
 *
 * @constructor Constructs a new connection, which will call [NearbyConnectionObserver.onStateUpdated]
 *     with an argument of type [ConnectionState.Isolated]. No further action will be taken unless
 *     other methods are called by the client.
 * @param connectionsClient the underlying client
 * @param name a human-readable name for this device
 */
@Suppress("TooManyFunctions")
class NearbyConnection(
    private val connectionsClient: ConnectionsClient,
    private val name: String = Build.MODEL,
    private val delegate: ObserverRegistry<NearbyConnectionObserver> = ObserverRegistry()
) : Observable<NearbyConnectionObserver> by delegate {
    /**
     * Another constructor
     */
    constructor(context: Context, name: String = Build.MODEL) : this(
        Nearby.getConnectionsClient(
            context
        ), name
    )

    // I assume that the number of endpoints encountered during the lifetime of the application
    // will be small and do not remove them from the map.
    @VisibleForTesting
    internal val endpointIdsToNames = ConcurrentHashMap<String, String>()

    private val logger = Logger("NearbyConnection")

    /**
     * The state of the connection. Changes in state are communicated to the client through
     * [NearbyConnectionObserver.onStateUpdated].
     */
    public sealed class ConnectionState {
        /**
         * The name of the state, which is the same as the name of the concrete class
         * (e.g., "Isolated" for [Isolated]).
         */
        val name: String = javaClass.simpleName

        /**
         * There is no connection to another device and no attempt to connect.
         */
        object Isolated : ConnectionState()

        /**
         * This device is advertising its presence. If it is discovered by another device,
         * the next state will be [Authenticating].
         */
        object Advertising : ConnectionState()

        /**
         * This device is trying to discover devices that are advertising. If it discovers one,
         * the next state with be [Initiating].
         */
        object Discovering : ConnectionState()

        /**
         * This device has discovered a neighboring device and is initiating a connection.
         * If all goes well, the next state will be [Authenticating].
         */
        class Initiating(
            /**
             * The ID of the neighbor, which is not meant for human readability.
             */
            val neighborId: String,

            /**
             * The human readable name of the neighbor.
             */
            val neighborName: String
        ) : ConnectionState()

        /**
         * This device is in the process of authenticating with a neighboring device. If both
         * devices accept the token, the next state will be [Connecting].
         */
        class Authenticating(
            // Sealed classes can't be inner, so we need to pass in the connection.
            private val nearbyConnection: NearbyConnection,

            // If the token is rejected, we will revert to this prior state.
            private val fallbackState: ConnectionState,

            /**
             * The ID of the neighbor, which is not meant for human readability.
             */
            val neighborId: String,

            /**
             * The human readable name of the neighbor.
             */
            val neighborName: String,

            /**
             * A short unique token of printable characters shared by both sides of the
             * pending connection.
             */
            val token: String
        ) : ConnectionState() {

            /**
             * Accepts the connection to the neighbor.
             */
            fun accept() {
                nearbyConnection.connectionsClient.acceptConnection(
                    neighborId,
                    nearbyConnection.payloadCallback
                )
                nearbyConnection.updateState(Connecting(neighborId, neighborName))
            }

            /**
             * Rejects the connection to the neighbor.
             */
            fun reject() {
                nearbyConnection.connectionsClient.rejectConnection(neighborId)
                // This should put us back in advertising or discovering.
                nearbyConnection.updateState(fallbackState)
            }
        }

        /**
         * The connection has been successfully authenticated.
         * Unless an error occurs, the next state will be [ReadyToSend].
         *
         * @param neighborId the neighbor's ID, which is not meant for human readability
         * @param neighborName the neighbor's human-readable name
         */
        class Connecting(val neighborId: String, val neighborName: String) : ConnectionState()

        /**
         * A connection has been made to a neighbor and this device may send a message.
         * This state is followed by [Sending] or [Failure].
         *
         * @param neighborId the neighbor's ID, which is not meant for human readability
         * @param neighborName the neighbor's human-readable name
         */
        class ReadyToSend(val neighborId: String, val neighborName: String?) : ConnectionState()

        /**
         * A message is being sent from this device. This state is followed by [ReadyToSend] or
         * [Failure].
         *
         * @param neighborId the neighbor's ID, which is not meant for human readability
         * @param neighborName the neighbor's human-readable name
         * @param payloadId the ID of the message that was sent, which will appear again
         *   in [NearbyConnectionObserver.onMessageDelivered]
         */
        class Sending(val neighborId: String, val neighborName: String?, val payloadId: Long) :
            ConnectionState()

        /**
         * A failure has occurred.
         *
         * @param message an error message describing the failure
         */
        class Failure(val message: String) : ConnectionState()
    }

    // The is mutated only in updateState(), which can be called from both the main thread and in
    // callbacks so is synchronized.
    private var connectionState: ConnectionState = ConnectionState.Isolated

    // Override all 3 register() methods to notify listener of initial state.
    override fun register(observer: NearbyConnectionObserver) {
        delegate.register(observer)
        observer.onStateUpdated(connectionState)
    }

    override fun register(observer: NearbyConnectionObserver, view: View) {
        delegate.register(observer, view)
        observer.onStateUpdated(connectionState)
    }

    override fun register(
        observer: NearbyConnectionObserver,
        owner: LifecycleOwner,
        autoPause: Boolean
    ) {
        delegate.register(observer, owner, autoPause)
        observer.onStateUpdated(connectionState)
    }

    // This method is called from both the main thread and callbacks.
    @Synchronized
    private fun updateState(cs: ConnectionState) {
        connectionState = cs
        notifyObservers { onStateUpdated(cs) }
    }

    /**
     * Starts advertising this device. After calling this, the state will be updated to
     * [ConnectionState.Advertising] or [ConnectionState.Failure]. If all goes well, eventually
     * the state will be updated to [ConnectionState.Authenticating]. A client should call either
     * [startAdvertising] or [startDiscovering] to make a connection, not both. To initiate a
     * connection, one device must call [startAdvertising] and the other [startDiscovering].
     */
    fun startAdvertising() {
        connectionsClient.startAdvertising(
            name,
            PACKAGE_NAME,
            connectionLifecycleCallback,
            AdvertisingOptions.Builder().setStrategy(STRATEGY).build()
        ).addOnSuccessListener {
            updateState(ConnectionState.Advertising)
        }.addOnFailureListener {
            reportError("failed to start advertising: $it")
        }
    }

    /**
     * Starts trying to discover nearby advertising devices. After calling this, the state will
     * be updated to [ConnectionState.Discovering] or [ConnectionState.Failure]. If all goes well,
     * eventually the state will be updated to [ConnectionState.Authenticating]. A client should
     * call either [startAdvertising] or [startDiscovering] to make a connection, not both. To
     * initiate a connection, one device must call [startAdvertising] and the other
     * [startDiscovering].
     */
    fun startDiscovering() {
        connectionsClient.startDiscovery(
            PACKAGE_NAME, endpointDiscoveryCallback,
            DiscoveryOptions.Builder().setStrategy(STRATEGY).build()
        )
            .addOnSuccessListener {
                updateState(ConnectionState.Discovering)
            }.addOnFailureListener {
                reportError("failed to start discovering: $it")
            }
    }

    // Discovery
    private val endpointDiscoveryCallback = object : EndpointDiscoveryCallback() {
        override fun onEndpointFound(endpointId: String, info: DiscoveredEndpointInfo) {
            connectionsClient.requestConnection(name, endpointId, connectionLifecycleCallback)
            updateState(ConnectionState.Initiating(endpointId, info.endpointName))
        }

        override fun onEndpointLost(endpointId: String) {
            // I don't know whether setting the state to Discovering is superfluous,
            // since I don't know if it's possible for onEndpointFound() to be called
            // first, or if only one of the callback's two methods will be called.
            updateState(ConnectionState.Discovering)
        }
    }

    // Used within startAdvertising() and startDiscovering() (via endpointDiscoveryCallback)
    private val connectionLifecycleCallback = object : ConnectionLifecycleCallback() {
        override fun onConnectionInitiated(endpointId: String, connectionInfo: ConnectionInfo) {
            // This is the last time we have access to the endpoint name, so cache it.
            endpointIdsToNames[endpointId] = connectionInfo.endpointName
            updateState(
                ConnectionState.Authenticating(
                    this@NearbyConnection,
                    if (connectionState is ConnectionState.Advertising) {
                        connectionState
                    } else {
                        ConnectionState.Discovering
                    },
                    endpointId,
                    connectionInfo.endpointName,
                    connectionInfo.authenticationToken
                )
            )
        }

        override fun onConnectionResult(endpointId: String, result: ConnectionResolution) {
            if (result.status.isSuccess) {
                connectionsClient.stopDiscovery()
                connectionsClient.stopAdvertising()
                updateState(
                    ConnectionState.ReadyToSend(
                        endpointId,
                        endpointIdsToNames[endpointId]
                    )
                )
            } else {
                reportError("onConnectionResult: connection failed with status ${result.status}")
            }
        }

        override fun onDisconnected(endpointId: String) {
            updateState(ConnectionState.Isolated)
        }
    }

    @VisibleForTesting
    internal fun payloadToString(payload: Payload): String? {
        if (payload.type == Payload.Type.BYTES) {
            payload.asBytes()?.let { return String(it, PAYLOAD_ENCODING) }
        } else if (payload.type == Payload.Type.STREAM) {
            payload.asStream()?.asInputStream()
                ?.use { return String(it.readBytes(), PAYLOAD_ENCODING) }
        }
        reportError("Received payload had illegal type: ${payload.type}")
        return null
    }

    @VisibleForTesting
    internal fun stringToPayload(message: String): Payload {
        val bytes = message.toByteArray(PAYLOAD_ENCODING)
        if (bytes.size <= ConnectionsClient.MAX_BYTES_DATA_SIZE) {
            logger.error("${bytes.size} <= ${ConnectionsClient.MAX_BYTES_DATA_SIZE}")
            return Payload.fromBytes(bytes)
        } else {
            logger.error("${bytes.size} > ${ConnectionsClient.MAX_BYTES_DATA_SIZE}")
            // Logically, it might make more sense to use Payload.fromFile() since we
            // know the size of the string, than Payload.fromStream(), but we would
            // have to create a file locally to use use the former.
            return Payload.fromStream(ByteArrayInputStream(bytes))
        }
    }

    private val payloadCallback = object : PayloadCallback() {
        override fun onPayloadReceived(endpointId: String, payload: Payload) {
            payloadToString(payload)?.let {
                notifyObservers {
                    onMessageReceived(endpointId, endpointIdsToNames[endpointId], it)
                }
            } ?: run {
                reportError("Error interpreting incoming payload")
            }
        }

        override fun onPayloadTransferUpdate(endpointId: String, update: PayloadTransferUpdate) {
            // Make sure it's reporting on our outgoing message, not an incoming one.
            if (update.status == Status.SUCCESS &&
                (connectionState as? ConnectionState.Sending)?.payloadId == update.payloadId
            ) {
                notifyObservers { onMessageDelivered(update.payloadId) }
                updateState(
                    ConnectionState.ReadyToSend(
                        endpointId,
                        endpointIdsToNames[endpointId]
                    )
                )
            }
        }
    }

    /**
     * Sends a message to a connected device. If the current state is not
     * [ConnectionState.ReadyToSend], the message will not be sent.
     *
     * @param message the message to send
     * @return an id that will be later passed back through
     *   [NearbyConnectionObserver.onMessageDelivered], or null if the message could not be sent
     */
    fun sendMessage(message: String): Long? {
        val state = connectionState
        if (state is ConnectionState.ReadyToSend) {
            val payload = stringToPayload(message)
            connectionsClient.sendPayload(state.neighborId, payload)
            updateState(
                ConnectionState.Sending(
                    state.neighborId,
                    endpointIdsToNames[state.neighborId],
                    payload.id
                )
            )
            return payload.id
        }
        return null
    }

    /**
     * Breaks any connections to neighboring devices. This also stops advertising and
     * discovering. The state will be updated to [ConnectionState.Isolated]. It is
     * important to call this when the connection is no longer needed because of
     * a [leak in the GATT client](http://bit.ly/33VP1gn).
     */
    fun disconnect() {
        connectionsClient.stopAllEndpoints() // also stops advertising and discovery
        updateState(ConnectionState.Isolated)
    }

    private fun reportError(msg: String) {
        logger.error(msg)
        updateState(ConnectionState.Failure(msg))
    }

    companion object {
        @VisibleForTesting
        internal const val PACKAGE_NAME = "mozilla.components.lib.nearby"
        @VisibleForTesting
        internal val PAYLOAD_ENCODING: Charset = Charsets.UTF_8
        private val STRATEGY = Strategy.P2P_STAR
        // The maximum number of bytes to send through Payload.fromBytes();
        // otherwise, use Payload.getStream().
        private val MAX_PAYLOAD_BYTES = ConnectionsClient.MAX_BYTES_DATA_SIZE

        /**
         * The permissions needed by [NearbyConnection]. It is the client's responsibility
         * to ensure that all are granted before constructing an instance of this class.
         */
        val PERMISSIONS: Array<String> = arrayOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.BLUETOOTH,
            Manifest.permission.BLUETOOTH_ADMIN,
            Manifest.permission.ACCESS_WIFI_STATE,
            Manifest.permission.CHANGE_WIFI_STATE
        )
    }
}

/**
 * Interface definition for observing changes in a [NearbyConnection].
 */
interface NearbyConnectionObserver {
    /**
     * Called whenever the connection's state is set. In the absence of failures, the
     * new state should differ from the prior state, but that is not guaranteed.
     *
     * @param connectionState the current state
     */
    fun onStateUpdated(connectionState: NearbyConnection.ConnectionState)

    /**
     * Called when a message is received from a neighboring device.
     *
     * @param neighborId the ID of the neighboring device
     * @param neighborName the name of the neighboring device
     * @param message the message
     */
    fun onMessageReceived(neighborId: String, neighborName: String?, message: String)

    /**
     * Called when a message has been successfully delivered to a neighboring device.
     */
    fun onMessageDelivered(payloadId: Long)
}
