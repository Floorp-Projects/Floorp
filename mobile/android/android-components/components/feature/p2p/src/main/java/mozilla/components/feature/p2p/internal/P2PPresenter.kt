/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.p2p.internal

import mozilla.components.feature.p2p.R
import mozilla.components.feature.p2p.internal.P2PInteractor.Companion.HTML_INDICATOR
import mozilla.components.feature.p2p.internal.P2PInteractor.Companion.URL_INDICATOR
import mozilla.components.feature.p2p.view.P2PBar
import mozilla.components.feature.p2p.view.P2PView
import mozilla.components.lib.nearby.NearbyConnection
import mozilla.components.lib.nearby.NearbyConnection.ConnectionState
import mozilla.components.lib.nearby.NearbyConnectionObserver
import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.ConcurrentMap

/**
 * Sends updates to [P2PView] based on updates from [NearbyConnection].
 */
class P2PPresenter(
    private val connectionProvider: () -> NearbyConnection,
    private val view: P2PView,
    private val outgoingMessages: ConcurrentMap<Long, Char> // shared with P2PInteractor
) {
    private val logger = Logger("P2PPresenter")
    private var nearbyConnection: NearbyConnection? = null

    private val observer = object : NearbyConnectionObserver {
        @Synchronized
        override fun onStateUpdated(connectionState: NearbyConnection.ConnectionState) {
            savedConnectionState = connectionState
            updateState(connectionState)
            when (connectionState) {
                is NearbyConnection.ConnectionState.Authenticating -> {
                    view.authenticate(
                        connectionState.neighborId,
                        connectionState.neighborName,
                        connectionState.token
                    )
                }
                is ConnectionState.ReadyToSend -> view.readyToSend()
                is ConnectionState.Failure -> view.failure(connectionState.message)
                is ConnectionState.Isolated -> view.clear()
            }
        }

        private fun updateState(connectionState: ConnectionState) {
            view.updateStatus(
                when (connectionState) {
                    is ConnectionState.Isolated -> R.string.mozac_feature_p2p_isolated
                    is ConnectionState.Advertising -> R.string.mozac_feature_p2p_advertising
                    is ConnectionState.Discovering -> R.string.mozac_feature_p2p_discovering
                    is ConnectionState.Initiating -> R.string.mozac_feature_p2p_initiating
                    is ConnectionState.Authenticating -> R.string.mozac_feature_p2p_authenticating
                    is ConnectionState.Connecting -> R.string.mozac_feature_p2p_connecting
                    // The user might find it clearer to be notified that the device is connected
                    // than that it is ready to send (it is also ready to receive).
                    is ConnectionState.ReadyToSend -> R.string.mozac_feature_p2p_connected
                    is ConnectionState.Sending -> R.string.mozac_feature_p2p_sending
                    is ConnectionState.Failure -> R.string.mozac_feature_p2p_failure
                }
            )
        }

        override fun onMessageDelivered(payloadId: Long) {
            outgoingMessages[payloadId]?.let {
                view.reportSendComplete(
                    if (it == URL_INDICATOR) {
                        R.string.mozac_feature_p2p_url_sent
                    } else {
                        R.string.mozac_feature_p2p_page_sent
                    }
                )
                // Is it better to remove entries for delivered messages from the map
                // or to leave them in (which is more functional-style)?
                outgoingMessages.remove(payloadId)
            } ?: run {
                logger.error("Sent message id was not recognized")
            }
        }

        override fun onMessageReceived(neighborId: String, neighborName: String?, message: String) {
            if (message.length > 1) {
                when (message[0]) {
                    HTML_INDICATOR -> view.receivePage(
                        neighborId, neighborName, message.substring(1)
                    )
                    URL_INDICATOR -> view.receiveUrl(
                        neighborId, neighborName, message.substring(1)
                    )
                    else -> reportError(view, "Cannot parse incoming message $message")
                }
            } else {
                reportError(view, "Trivial message received: '$message'")
            }
        }
    }

    internal fun start() {
        if (nearbyConnection == null || savedConnectionState == null) {
            nearbyConnection = connectionProvider()
            savedConnectionState = null
        } else {
            // If a connection already existed, update the UI to reflect its status.
            when (savedConnectionState) {
                is ConnectionState.Isolated -> view.initializeButtons(true, false)
                is ConnectionState.Advertising,
                is ConnectionState.Discovering,
                is ConnectionState.Initiating,
                is ConnectionState.Authenticating,
                is ConnectionState.Connecting -> view.initializeButtons(false, false)
                is ConnectionState.ReadyToSend -> view.initializeButtons(false, true)
                is ConnectionState.Failure -> view.initializeButtons(false, false)
            }
        }
        nearbyConnection?.register(observer, view as P2PBar)
    }

    private fun reportError(view: P2PView, msg: String) {
        Logger.error(msg)
        view.failure(msg)
    }

    internal fun stop() {
        nearbyConnection?.unregisterObservers()
    }

    companion object {
        private var savedConnectionState: ConnectionState? = null
    }
}
