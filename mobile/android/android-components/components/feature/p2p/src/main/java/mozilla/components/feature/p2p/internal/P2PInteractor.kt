/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.p2p.internal

import android.content.Context
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.p2p.P2PFeature
import mozilla.components.feature.p2p.R
import mozilla.components.feature.p2p.view.P2PView
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.lib.nearby.NearbyConnection
import mozilla.components.support.base.log.logger.Logger
import java.io.File
import java.io.FileOutputStream
import java.util.concurrent.ConcurrentMap

/**
 * Handle requests from [P2PView], including interactions with the extension.
 */
@SuppressWarnings("TooManyFunctions")
class P2PInteractor(
    private val store: BrowserStore,
    private val view: P2PView,
    private val tabsUseCases: TabsUseCases,
    private val sessionUseCases: SessionUseCases,
    private val sender: P2PFeature.P2PFeatureSender,
    private val onClose: (() -> Unit),
    private val connectionProvider: () -> NearbyConnection,
    private val outgoingMessages: ConcurrentMap<Long, Char> // shared with P2PPresenter
) : P2PView.Listener {
    private val logger = Logger("P2PController")
    private var nearbyConnection: NearbyConnection? = null

    internal fun start() {
        if (nearbyConnection == null) {
            nearbyConnection = connectionProvider()
        }
        view.listener = this
    }

    @SuppressWarnings("EmptyFunctionBlock")
    internal fun stop() {}

    private inline fun <reified T : NearbyConnection.ConnectionState> cast() =
        (nearbyConnection?.connectionState as? T)

    // P2PView.Listener implementation

    override fun onAdvertise() {
        nearbyConnection?.startAdvertising()
    }

    override fun onDiscover() {
        nearbyConnection?.startDiscovering()
    }

    override fun onAccept(token: String) {
        cast<NearbyConnection.ConnectionState.Authenticating>()?.run {
            accept()
        }
    }

    override fun onReject(token: String) {
        cast<NearbyConnection.ConnectionState.Authenticating>()?.run {
            reject()
        }
    }

    override fun onSetUrl(url: String, newTab: Boolean) {
        if (newTab) {
            tabsUseCases.addTab(url)
        } else {
            sessionUseCases.loadUrl(url)
        }
    }

    override fun onReset() {
        nearbyConnection?.disconnect()
    }

    override fun onSendPage() {
        if (cast<NearbyConnection.ConnectionState.ReadyToSend>() != null) {
            // Because packaging a page takes a long time, we have a status string indicating that
            // the page is being packaged up. Unlike the other calls to view.updateStatus(), this
            // does not correspond to a change in NearbyConnection.ConnectionState.
            view.updateStatus(R.string.mozac_feature_p2p_packaging)
            sender.requestHtml()
        }
    }

    // This is called after the extension has packaged up the page requested by onSendPage().
    internal fun onPageReadyToSend(page: String) {
        sendMessage(HTML_INDICATOR, page)
    }

    override fun onSendUrl() {
        store.state.selectedTab?.content?.url?.let {
            sendMessage(URL_INDICATOR, it)
        }
    }

    private fun sendMessage(indicator: Char, message: String) {
        if (nearbyConnection?.connectionState is NearbyConnection.ConnectionState.ReadyToSend) {
            when (val messageId = nearbyConnection?.sendMessage("$indicator$message")) {
                null -> reportError(view, "Unable to send message: sendMessage() returns null")
                else -> outgoingMessages[messageId] = indicator
            }
        }
    }

    private fun reportError(view: P2PView, msg: String) {
        Logger.error(msg)
        view.failure(msg)
    }

    override fun onLoadData(context: Context, data: String, newTab: Boolean) {
        // Store data in file to work around Mozilla bug 1598481, which makes
        // loading a page from memory extremely inefficient. Instead, we will
        // write it to a file and then load a URL referencing that file.
        val file = File.createTempFile("moz", ".html")
        FileOutputStream(file).use {
            it.write(data.toByteArray())
        }
        file.deleteOnExit()
        onSetUrl("file:${file.path}", newTab)
    }

    override fun onCloseToolbar() {
        onClose()
    }

    companion object {
        // Used for message headers and for hash table outgoingMessages
        const val URL_INDICATOR = 'U'
        const val HTML_INDICATOR = 'H'
    }
}
