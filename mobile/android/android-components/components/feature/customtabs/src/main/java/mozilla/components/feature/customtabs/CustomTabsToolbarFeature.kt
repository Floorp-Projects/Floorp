/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.customtabs

import android.graphics.Color
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.base.feature.LifecycleAwareFeature

/**
 * Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig.
 */
class CustomTabsToolbarFeature(
    private val sessionManager: SessionManager,
    private val toolbar: BrowserToolbar,
    private val sessionId: String? = null
) : LifecycleAwareFeature {

    override fun start() {
        val session = sessionId?.let { sessionManager.findSessionById(sessionId) }
        session?.let { initialize(it) }
    }

    internal fun initialize(session: Session) {
        session.customTabConfig?.let { config ->
            // Change the toolbar colour
            updateToolbarColor(config.toolbarColor)
        }
    }

    internal fun updateToolbarColor(toolbarColor: Int?) {
        toolbarColor?.let { color ->
            toolbar.setBackgroundColor(color)
            toolbar.textColor = getReadableTextColor(color)
        }
    }

    override fun stop() {}

    companion object {
        @Suppress("MagicNumber")
        internal fun getReadableTextColor(backgroundColor: Int): Int {
            val greyValue = greyscaleFromRGB(backgroundColor)
            // 186 chosen rather than the seemingly obvious 128 because of gamma.
            return if (greyValue < 186) {
                Color.WHITE
            } else {
                Color.BLACK
            }
        }

        @Suppress("MagicNumber")
        private fun greyscaleFromRGB(color: Int): Int {
            val red = Color.red(color)
            val green = Color.green(color)
            val blue = Color.blue(color)
            // Magic weighting taken from a stackoverflow post, supposedly related to how
            // humans perceive color.
            return (0.299 * red + 0.587 * green + 0.114 * blue).toInt()
        }
    }
}
