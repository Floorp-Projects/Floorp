/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview

import android.content.Context
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.MessageHandler
import mozilla.components.concept.engine.webextension.Port
import mozilla.components.feature.readerview.internal.ReaderViewControlsInteractor
import mozilla.components.feature.readerview.internal.ReaderViewControlsPresenter
import mozilla.components.feature.readerview.view.ReaderViewControlsView
import mozilla.components.feature.readerview.ReaderViewFeature.ColorScheme.LIGHT
import mozilla.components.feature.readerview.ReaderViewFeature.FontType.SERIF
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.webextensions.WebExtensionController
import org.json.JSONObject
import java.lang.ref.WeakReference
import kotlin.properties.Delegates.observable

typealias OnReaderViewAvailableChange = (available: Boolean) -> Unit

/**
 * Feature implementation that provides a reader view for the selected
 * session, based on a web extension.
 *
 * @property context a reference to the context.
 * @property engine a reference to the application's browser engine.
 * @property sessionManager a reference to the application's [SessionManager].
 * @param controlsView the view to use to display reader mode controls.
 * @property onReaderViewAvailableChange a callback invoked to indicate whether
 * or not reader view is available for the page loaded by the currently selected
 * session. The callback will be invoked when a page is loaded or refreshed,
 * on any navigation (back or forward), and when the selected session
 * changes.
 */
@Suppress("TooManyFunctions")
class ReaderViewFeature(
    private val context: Context,
    private val engine: Engine,
    private val sessionManager: SessionManager,
    controlsView: ReaderViewControlsView,
    private val onReaderViewAvailableChange: OnReaderViewAvailableChange = { }
) : SelectionAwareSessionObserver(sessionManager), LifecycleAwareFeature, BackHandler {

    @VisibleForTesting
    // This is an internal var to make it mutable for unit testing purposes only
    internal var extensionController = WebExtensionController(READER_VIEW_EXTENSION_ID, READER_VIEW_EXTENSION_URL)

    @VisibleForTesting
    internal val config = Config(context.getSharedPreferences(SHARED_PREF_NAME, Context.MODE_PRIVATE))

    private val controlsPresenter = ReaderViewControlsPresenter(controlsView, config)
    private val controlsInteractor = ReaderViewControlsInteractor(controlsView, config)

    enum class FontType(val value: String) { SANSSERIF("sans-serif"), SERIF("serif") }
    enum class ColorScheme { LIGHT, SEPIA, DARK }

    inner class Config(private val prefs: SharedPreferences) {

        var colorScheme by observable(ColorScheme.valueOf(prefs.getString(COLOR_SCHEME_KEY, LIGHT.name)!!)) {
            _, old, new -> if (old != new) {
                val message = JSONObject().put(ACTION_MESSAGE_KEY, ACTION_SET_COLOR_SCHEME).put(ACTION_VALUE, new.name)
                sendConfigMessage(message)
                prefs.edit().putString(COLOR_SCHEME_KEY, new.name).apply()
            }
        }

        var fontType by observable(FontType.valueOf(prefs.getString(FONT_TYPE_KEY, SERIF.name)!!)) {
            _, old, new -> if (old != new) {
                val message = JSONObject().put(ACTION_MESSAGE_KEY, ACTION_SET_FONT_TYPE).put(ACTION_VALUE, new.value)
                sendConfigMessage(message)
                prefs.edit().putString(FONT_TYPE_KEY, new.name).apply()
            }
        }

        var fontSize by observable(prefs.getInt(FONT_SIZE_KEY, FONT_SIZE_DEFAULT)) {
            _, old, new -> if (old != new) {
                val message = JSONObject().put(ACTION_MESSAGE_KEY, ACTION_CHANGE_FONT_SIZE).put(ACTION_VALUE, new - old)
                sendConfigMessage(message)
                prefs.edit().putInt(FONT_SIZE_KEY, new).apply()
            }
        }

        private fun sendConfigMessage(message: JSONObject, session: Session? = activeSession) {
            session?.let {
                extensionController.sendContentMessage(message, sessionManager.getEngineSession(it))
            }
        }
    }

    override fun start() {
        observeSelected()
        registerReaderViewContentMessageHandler()

        extensionController.install(engine)
        activeSession?.let {
            if (extensionController.portConnected(sessionManager.getEngineSession(it))) {
                updateReaderViewState(it)
            }
        }

        controlsInteractor.start()
    }

    override fun stop() {
        controlsInteractor.stop()
        super.stop()
    }

    override fun onBackPressed(): Boolean {
        activeSession?.let {
            if (it.readerMode) {
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

    override fun onSessionSelected(session: Session) {
        super.onSessionSelected(session)
        updateReaderViewState(session)
    }

    override fun onSessionAdded(session: Session) {
        registerReaderViewContentMessageHandler(session)
    }

    override fun onSessionRemoved(session: Session) {
        extensionController.disconnectPort(sessionManager.getEngineSession(session))
    }

    override fun onUrlChanged(session: Session, url: String) {
        session.readerable = false
        session.readerMode = false
        checkReaderable(session)
    }

    override fun onReaderableStateUpdated(session: Session, readerable: Boolean) {
        onReaderViewAvailableChange(readerable)
    }

    /**
     * Shows the reader view UI.
     */
    fun showReaderView(session: Session? = activeSession) {
        session?.let { it ->
            extensionController.sendContentMessage(createShowReaderMessage(config), sessionManager.getEngineSession(it))
            it.readerMode = true
        }
    }

    /**
     * Hides the reader view UI.
     */
    fun hideReaderView(session: Session? = activeSession) {
        session?.let { it ->
            it.readerMode = false
            // We will re-determine if the original page is readerable when it's loaded.
            it.readerable = false
            extensionController.sendContentMessage(createHideReaderMessage(), sessionManager.getEngineSession(it))
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
    internal fun checkReaderable(session: Session? = activeSession) {
        session?.let {
            val engineSession = sessionManager.getEngineSession(session)
            if (extensionController.portConnected(engineSession)) {
                extensionController.sendContentMessage(createCheckReaderableMessage(), engineSession)
            }
        }
    }

    @VisibleForTesting
    internal fun registerReaderViewContentMessageHandler(session: Session? = activeSession) {
        if (session == null) {
            return
        }

        val engineSession = sessionManager.getOrCreateEngineSession(session)
        val messageHandler = ReaderViewContentMessageHandler(session, WeakReference(config))
        extensionController.registerContentMessageHandler(engineSession, messageHandler)
    }

    @VisibleForTesting
    internal fun updateReaderViewState(session: Session? = activeSession) {
        if (session == null) {
            return
        }

        checkReaderable(session)
        if (session.readerMode) {
            showReaderView(session)
        }
    }

    private class ReaderViewContentMessageHandler(
        private val session: Session,
        // This needs to be a weak reference because the engine session this message handler will be
        // attached to has a longer lifespan than the feature instance i.e. a tab can remain open,
        // but we don't want to prevent the feature (and therefore its context/fragment) from
        // being garbage collected. The config has references to both the context and feature.
        private val config: WeakReference<Config>
    ) : MessageHandler {
        override fun onPortConnected(port: Port) {
            val config = config.get() ?: return

            port.postMessage(createCheckReaderableMessage())
            if (session.readerMode) {
                port.postMessage(createShowReaderMessage(config))
            }
        }

        override fun onPortMessage(message: Any, port: Port) {
            if (message is JSONObject) {
                session.readerable = message.optBoolean(READERABLE_RESPONSE_MESSAGE_KEY, false)
            }
        }
    }

    @VisibleForTesting
    companion object {
        internal const val READER_VIEW_EXTENSION_ID = "mozacReaderview"
        internal const val READER_VIEW_EXTENSION_URL = "resource://android/assets/extensions/readerview/"

        // Constants for building messages sent to the web extension:
        // Change the font type: {"action": "setFontType", "value": "sans-serif"}
        // Show reader view: {"action": "show", "value": {"fontSize": 3, "fontType": "serif", "colorScheme": "dark"}}
        internal const val ACTION_MESSAGE_KEY = "action"
        internal const val ACTION_SHOW = "show"
        internal const val ACTION_HIDE = "hide"
        internal const val ACTION_CHECK_READERABLE = "checkReaderable"
        internal const val ACTION_SET_COLOR_SCHEME = "setColorScheme"
        internal const val ACTION_CHANGE_FONT_SIZE = "changeFontSize"
        internal const val ACTION_SET_FONT_TYPE = "setFontType"
        internal const val ACTION_VALUE = "value"
        internal const val ACTION_VALUE_SHOW_FONT_SIZE = "fontSize"
        internal const val ACTION_VALUE_SHOW_FONT_TYPE = "fontType"
        internal const val ACTION_VALUE_SHOW_COLOR_SCHEME = "colorScheme"
        internal const val READERABLE_RESPONSE_MESSAGE_KEY = "readerable"

        // Constants for storing the reader mode config in shared preferences
        internal const val SHARED_PREF_NAME = "mozac_feature_reader_view"
        internal const val COLOR_SCHEME_KEY = "mozac-readerview-colorscheme"
        internal const val FONT_TYPE_KEY = "mozac-readerview-fonttype"
        internal const val FONT_SIZE_KEY = "mozac-readerview-fontsize"
        internal const val FONT_SIZE_DEFAULT = 3

        private fun createCheckReaderableMessage(): JSONObject {
            return JSONObject().put(ACTION_MESSAGE_KEY, ACTION_CHECK_READERABLE)
        }

        private fun createShowReaderMessage(config: Config): JSONObject {
            val configJson = JSONObject()
                    .put(ACTION_VALUE_SHOW_FONT_SIZE, config.fontSize)
                    .put(ACTION_VALUE_SHOW_FONT_TYPE, config.fontType.name.toLowerCase())
                    .put(ACTION_VALUE_SHOW_COLOR_SCHEME, config.colorScheme.name.toLowerCase())

            return JSONObject()
                    .put(ACTION_MESSAGE_KEY, ACTION_SHOW)
                    .put(ACTION_VALUE, configJson)
        }

        private fun createHideReaderMessage(): JSONObject {
            return JSONObject().put(ACTION_MESSAGE_KEY, ACTION_HIDE)
        }
    }
}
