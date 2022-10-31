/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview

import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.action.ReaderAction
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.feature.readerview.internal.ReaderViewConfig
import mozilla.components.feature.readerview.internal.ReaderViewControlsInteractor
import mozilla.components.feature.readerview.internal.ReaderViewControlsPresenter
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.kotlinx.coroutines.flow.filterChanged
import mozilla.components.support.webextensions.WebExtensionController
import org.json.JSONObject
import java.lang.ref.WeakReference
import java.util.Locale

typealias onReaderViewStatusChange = (available: Boolean, active: Boolean) -> Unit

/**
 * Feature implementation that provides a reader view for the selected
 * session, based on a web extension.
 *
 * @property context a reference to the context.
 * @property engine a reference to the application's browser engine.
 * @property store a reference to the application's [BrowserStore].
 * @param controlsView the view to use to display reader mode controls.
 * @property onReaderViewStatusChange a callback invoked to indicate whether
 * or not reader view is available and active for the page loaded by the
 * currently selected session. The callback will be invoked when a page is
 * loaded or refreshed, on any navigation (back or forward), and when the
 * selected session changes.
 */
class ReaderViewFeature(
    private val context: Context,
    private val engine: Engine,
    private val store: BrowserStore,
    controlsView: ReaderViewControlsView,
    private val onReaderViewStatusChange: onReaderViewStatusChange = { _, _ -> Unit },
) : LifecycleAwareFeature, UserInteractionHandler {

    private var scope: CoroutineScope? = null

    @VisibleForTesting
    // This is an internal var to make it mutable for unit testing purposes only
    internal var extensionController = WebExtensionController(
        READER_VIEW_EXTENSION_ID,
        READER_VIEW_EXTENSION_URL,
        READER_VIEW_CONTENT_PORT,
    )

    @VisibleForTesting
    internal val config = ReaderViewConfig(context) { message ->
        val engineSession = store.state.selectedTab?.engineState?.engineSession
        extensionController.sendContentMessage(message, engineSession, READER_VIEW_ACTIVE_CONTENT_PORT)
    }

    private val controlsPresenter = ReaderViewControlsPresenter(controlsView, config)
    private val controlsInteractor = ReaderViewControlsInteractor(controlsView, config)

    enum class FontType(val value: String) { SANSSERIF("sans-serif"), SERIF("serif") }
    enum class ColorScheme { LIGHT, SEPIA, DARK }

    override fun start() {
        ensureExtensionInstalled()

        scope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.tabs }
                .filterChanged {
                    it.readerState
                }
                .collect { tab ->
                    if (tab.readerState.connectRequired) {
                        connectReaderViewContentScript(tab)
                    }
                    if (tab.readerState.checkRequired) {
                        checkReaderState(tab)
                    }
                    if (tab.id == store.state.selectedTabId) {
                        maybeNotifyReaderStatusChange(tab.readerState.readerable, tab.readerState.active)
                    }
                }
        }

        controlsInteractor.start()
    }

    override fun stop() {
        scope?.cancel()
        controlsInteractor.stop()
    }

    override fun onBackPressed(): Boolean {
        store.state.selectedTab?.let {
            if (it.readerState.active) {
                if (controlsPresenter.areControlsVisible()) {
                    hideControls()
                } else {
                    hideReaderView()
                }
                return true
            }
        }
        return false
    }

    /**
     * Shows the reader view UI.
     */
    fun showReaderView(session: TabSessionState? = store.state.selectedTab) {
        session?.let { it ->
            if (!it.readerState.active) {
                extensionController.sendContentMessage(
                    createShowReaderMessage(config),
                    it.engineState.engineSession,
                    READER_VIEW_CONTENT_PORT,
                )
                store.dispatch(ReaderAction.UpdateReaderActiveAction(it.id, true))
            }
        }
    }

    /**
     * Hides the reader view UI.
     */
    fun hideReaderView(session: TabSessionState? = store.state.selectedTab) {
        session?.let { it ->
            if (it.readerState.active) {
                store.dispatch(ReaderAction.UpdateReaderActiveAction(it.id, false))
                store.dispatch(ReaderAction.UpdateReaderableAction(it.id, false))
                store.dispatch(ReaderAction.ClearReaderActiveUrlAction(it.id))
                if (it.content.canGoBack) {
                    it.engineState.engineSession?.goBack(false)
                } else {
                    extensionController.sendContentMessage(
                        createHideReaderMessage(),
                        it.engineState.engineSession,
                        READER_VIEW_ACTIVE_CONTENT_PORT,
                    )
                }
            }
        }
    }

    /**
     * Shows the reader view appearance controls.
     */
    fun showControls() {
        controlsPresenter.show()
    }

    /**
     * Hides the reader view appearance controls.
     */
    fun hideControls() {
        controlsPresenter.hide()
    }

    @VisibleForTesting
    internal fun checkReaderState(session: TabSessionState? = store.state.selectedTab) {
        session?.engineState?.engineSession?.let { engineSession ->
            val message = createCheckReaderStateMessage()
            if (extensionController.portConnected(engineSession, READER_VIEW_CONTENT_PORT)) {
                extensionController.sendContentMessage(message, engineSession, READER_VIEW_CONTENT_PORT)
            }
            if (extensionController.portConnected(engineSession, READER_VIEW_ACTIVE_CONTENT_PORT)) {
                extensionController.sendContentMessage(message, engineSession, READER_VIEW_ACTIVE_CONTENT_PORT)
            }
            store.dispatch(ReaderAction.UpdateReaderableCheckRequiredAction(session.id, false))
        }
    }

    @VisibleForTesting
    internal fun connectReaderViewContentScript(session: TabSessionState? = store.state.selectedTab) {
        session?.engineState?.engineSession?.let { engineSession ->
            extensionController.registerContentMessageHandler(
                engineSession,
                ActiveReaderViewContentMessageHandler(store, session.id, WeakReference(config)),
                READER_VIEW_ACTIVE_CONTENT_PORT,
            )
            extensionController.registerContentMessageHandler(
                engineSession,
                ReaderViewContentMessageHandler(store, session.id),
                READER_VIEW_CONTENT_PORT,
            )
            store.dispatch(ReaderAction.UpdateReaderConnectRequiredAction(session.id, false))
        }
    }

    private var lastNotified: Pair<Boolean, Boolean>? = null

    @VisibleForTesting
    internal fun maybeNotifyReaderStatusChange(readerable: Boolean = false, active: Boolean = false) {
        // Make sure we only notify the UI if needed (an actual change happened) to prevent
        // it from unnecessarily invalidating toolbar/menu items.
        if (lastNotified == null || lastNotified != Pair(readerable, active)) {
            onReaderViewStatusChange(readerable, active)
            lastNotified = Pair(readerable, active)
        }
    }

    private fun ensureExtensionInstalled() {
        val feature = WeakReference(this)
        extensionController.install(
            engine,
            onSuccess = {
                feature.get()?.connectReaderViewContentScript()
            },
        )
    }

    /**
     * Handles content messages from regular pages.
     */
    private open class ReaderViewContentMessageHandler(
        protected val store: BrowserStore,
        protected val sessionId: String,
    ) : MessageHandler {
        override fun onPortConnected(port: Port) {
            port.postMessage(createCheckReaderStateMessage())
        }

        override fun onPortMessage(message: Any, port: Port) {
            if (message is JSONObject) {
                val readerable = message.optBoolean(READERABLE_RESPONSE_MESSAGE_KEY, false)
                store.dispatch(ReaderAction.UpdateReaderableAction(sessionId, readerable))
            }
        }
    }

    /**
     * Handles content messages from active reader pages.
     */
    private class ActiveReaderViewContentMessageHandler(
        store: BrowserStore,
        sessionId: String,
        // This needs to be a weak reference because the engine session this message handler will be
        // attached to has a longer lifespan than the feature instance i.e. a tab can remain open,
        // but we don't want to prevent the feature (and therefore its context/fragment) from
        // being garbage collected. The config has references to both the context and feature.
        private val config: WeakReference<ReaderViewConfig>,
    ) : ReaderViewContentMessageHandler(store, sessionId) {

        override fun onPortMessage(message: Any, port: Port) {
            super.onPortMessage(message, port)

            if (message is JSONObject) {
                val baseUrl = message.getString(BASE_URL_RESPONSE_MESSAGE_KEY)
                store.dispatch(ReaderAction.UpdateReaderBaseUrlAction(sessionId, baseUrl))

                port.postMessage(createShowReaderMessage(config.get()))

                val activeUrl = message.getString(ACTIVE_URL_RESPONSE_MESSAGE_KEY)
                store.dispatch(ReaderAction.UpdateReaderActiveUrlAction(sessionId, activeUrl))
            }
        }
    }

    @VisibleForTesting
    companion object {
        private val logger = Logger("ReaderView")

        internal const val READER_VIEW_EXTENSION_ID = "readerview@mozac.org"

        // Name of the port connected to all pages for checking whether or not
        // a page is readerable (see readerview_content.js).
        internal const val READER_VIEW_CONTENT_PORT = "mozacReaderview"

        // Name of the port connected to active reader pages for updating
        // appearance configuration (see readerview.js).
        internal const val READER_VIEW_ACTIVE_CONTENT_PORT = "mozacReaderviewActive"
        internal const val READER_VIEW_EXTENSION_URL = "resource://android/assets/extensions/readerview/"

        // Constants for building messages sent to the web extension:
        // Change the font type: {"action": "setFontType", "value": "sans-serif"}
        // Show reader view: {"action": "show", "value": {"fontSize": 3, "fontType": "serif", "colorScheme": "dark"}}
        internal const val ACTION_MESSAGE_KEY = "action"
        internal const val ACTION_SHOW = "show"
        internal const val ACTION_HIDE = "hide"
        internal const val ACTION_CHECK_READER_STATE = "checkReaderState"
        internal const val ACTION_SET_COLOR_SCHEME = "setColorScheme"
        internal const val ACTION_CHANGE_FONT_SIZE = "changeFontSize"
        internal const val ACTION_SET_FONT_TYPE = "setFontType"
        internal const val ACTION_VALUE = "value"
        internal const val ACTION_VALUE_SHOW_FONT_SIZE = "fontSize"
        internal const val ACTION_VALUE_SHOW_FONT_TYPE = "fontType"
        internal const val ACTION_VALUE_SHOW_COLOR_SCHEME = "colorScheme"
        internal const val READERABLE_RESPONSE_MESSAGE_KEY = "readerable"
        internal const val BASE_URL_RESPONSE_MESSAGE_KEY = "baseUrl"
        internal const val ACTIVE_URL_RESPONSE_MESSAGE_KEY = "activeUrl"

        // Constants for storing the reader mode config in shared preferences
        internal const val SHARED_PREF_NAME = "mozac_feature_reader_view"
        internal const val COLOR_SCHEME_KEY = "mozac-readerview-colorscheme"
        internal const val FONT_TYPE_KEY = "mozac-readerview-fonttype"
        internal const val FONT_SIZE_KEY = "mozac-readerview-fontsize"
        internal const val FONT_SIZE_DEFAULT = 3

        internal fun createCheckReaderStateMessage(): JSONObject {
            return JSONObject().put(ACTION_MESSAGE_KEY, ACTION_CHECK_READER_STATE)
        }

        internal fun createShowReaderMessage(config: ReaderViewConfig?): JSONObject {
            if (config == null) {
                logger.warn("No config provided. Falling back to default values.")
            }

            val fontSize = config?.fontSize ?: FONT_SIZE_DEFAULT
            val fontType = config?.fontType ?: FontType.SERIF
            val colorScheme = config?.colorScheme ?: ColorScheme.LIGHT
            val configJson = JSONObject()
                .put(ACTION_VALUE_SHOW_FONT_SIZE, fontSize)
                .put(ACTION_VALUE_SHOW_FONT_TYPE, fontType.value.lowercase(Locale.ROOT))
                .put(ACTION_VALUE_SHOW_COLOR_SCHEME, colorScheme.name.lowercase(Locale.ROOT))

            return JSONObject()
                .put(ACTION_MESSAGE_KEY, ACTION_SHOW)
                .put(ACTION_VALUE, configJson)
        }

        internal fun createHideReaderMessage(): JSONObject {
            return JSONObject().put(ACTION_MESSAGE_KEY, ACTION_HIDE)
        }
    }
}
