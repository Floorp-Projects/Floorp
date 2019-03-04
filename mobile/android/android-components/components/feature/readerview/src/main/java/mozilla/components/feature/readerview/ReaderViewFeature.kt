/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.readerview

import android.content.Context
import android.content.SharedPreferences
import android.support.annotation.VisibleForTesting
import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.webextension.WebExtension
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.log.logger.Logger
import kotlin.properties.Delegates

typealias OnReaderViewAvailableChange = (available: Boolean) -> Unit

/**
 * Feature implementation that provides a reader view for the selected
 * session. This feature is implemented as a web extension and
 * needs to be installed prior to use see [ReaderViewFeature.install].
 *
 * @property context a reference to the context.
 * @property engine a reference to the application's browser engine.
 * @property sessionManager a reference to the application's [SessionManager].
 * @property onReaderViewAvailableChange a callback invoked to indicate whether
 * or not reader view is available for the page loaded by the currently selected
 * (active) session.
 */
class ReaderViewFeature(
    private val context: Context,
    private val engine: Engine,
    private val sessionManager: SessionManager,
    private val onReaderViewAvailableChange: OnReaderViewAvailableChange = { }
) : SelectionAwareSessionObserver(sessionManager), LifecycleAwareFeature, BackHandler {

    private val config = Config(context.getSharedPreferences("mozac_feature_reader_view", Context.MODE_PRIVATE))

    // TODO this object will be manipulated by the ReaderViewAppearanceFragment:
    // https://github.com/mozilla-mobile/android-components/issues/2623
    class Config(prefs: SharedPreferences) {
        enum class FontType { SANS_SERIF, SERIF }
        enum class ColorScheme { LIGHT, SEPIA, DARK }

        var colorScheme by Delegates.observable(ColorScheme.valueOf(prefs.getString(COLOR_SCHEME_KEY, "LIGHT")!!)) {
            _, old, new -> saveAndSendMessage(old, new, COLOR_SCHEME_KEY)
        }

        @Suppress("MagicNumber")
        var fontSize by Delegates.observable(prefs.getInt(FONT_SIZE_KEY, 3)) {
            _, old, new -> saveAndSendMessage(old, new, FONT_SIZE_KEY)
        }

        var fontType by Delegates.observable(FontType.valueOf(prefs.getString(FONT_TYPE_KEY, "SANS_SERIF")!!)) {
            _, old, new -> saveAndSendMessage(old, new, FONT_TYPE_KEY)
        }

        @Suppress("UNUSED_PARAMETER")
        private fun saveAndSendMessage(old: Any, new: Any, key: String) {
            if (old != new) {
                // TODO save shared preference
                // TODO send message to reader view web extension
            }
        }

        companion object {
            const val COLOR_SCHEME_KEY = "mozac-readerview-colorscheme"
            const val FONT_TYPE_KEY = "mozac-readerview-fonttype"
            const val FONT_SIZE_KEY = "mozac-readerview-fontsize"
        }
    }

    override fun start() {
        if (!ReaderViewFeature.installed) {
            ReaderViewFeature.install(engine)
        }
        observeSelected()
    }

    override fun onBackPressed(): Boolean {
        // TODO send message to exit reader view (-> see ReaderView.hide())
        return true
    }

    override fun onSessionAdded(session: Session) {
        // TODO make sure we start listening to reader view web extension
        // messages for this session: Here we will just hook things up
        // using the readerViewWebExt WebExtension object which will
        // provide the functionality for bi-directional messaging
        // (bridge to GeckoView's setSessionMessageDelegate).
    }

    override fun onSessionRemoved(session: Session) {
        // TODO stop listening to reader view web extension messages of this session
    }

    fun showReaderView() {
        // TODO send message to show reader view (-> see ReaderView.show())
    }

    fun hideReaderView() {
        // TODO send message to hide reader view (-> see ReaderView.hide())
    }

    fun showAppearanceControls() {
        // TODO show appearance fragment
    }

    fun hideAppearanceControls() {
        // TODO hide appearance fragment
    }

    // TODO observe messages delivered from the web extension to the selected session
    // e.g. to know whether or not reader view is available for the current page.
    // The session will be updated by the messaging delegate integration logic in our
    // WebExtension object. So here we can just observe the message on the session.
    // override fun onWebExtensionMessage() { }

    companion object {
        @VisibleForTesting
        internal val readerViewWebExt =
                WebExtension("mozac-readerview", "resource://android/assets/extensions/readerview/")

        @Volatile
        internal var installed = false

        /**
         * Installs the readerview web extension in the provided engine.
         *
         * @param engine a reference to the application's browser engine.
         */
        fun install(engine: Engine) {
            engine.installWebExtension(readerViewWebExt,
                onSuccess = {
                    Logger.debug("Installed extension: ${it.id}")
                },
                onError = { ext, throwable ->
                    installed = false
                    Logger.error("Failed to install extension: ${ext.id}", throwable)
                }
            )
            // TODO move install state to our WebExtension object.
            installed = true
        }
    }
}
