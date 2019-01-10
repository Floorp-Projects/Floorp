/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.customtabs

import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.support.v4.content.ContextCompat
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSession
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.content.res.pxToDp

/**
 * Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig.
 */
class CustomTabsToolbarFeature(
    private val sessionManager: SessionManager,
    private val toolbar: BrowserToolbar,
    private val sessionId: String? = null,
    private val closeListener: () -> Unit
) : LifecycleAwareFeature {
    private val context = toolbar.context
    private var initialized = false

    override fun start() {
        if (initialized) {
            return
        }
        initialized = sessionManager.runWithSession(sessionId) { initialize(it) }
    }

    internal fun initialize(session: Session): Boolean {
        session.customTabConfig?.let { config ->
            // Don't allow clickable toolbar so a custom tab can't switch to edit mode.
            toolbar.onUrlClicked = { false }
            // Change the toolbar colour
            updateToolbarColor(config.toolbarColor)
            // Add navigation close action
            addCloseButton(config.closeButtonIcon)
            return true
        }
        return false
    }

    internal fun updateToolbarColor(toolbarColor: Int?) {
        toolbarColor?.let { color ->
            toolbar.setBackgroundColor(color)
            toolbar.textColor = getReadableTextColor(color)
        }
    }

    internal fun addCloseButton(bitmap: Bitmap?) {
        val drawableIcon = bitmap?.let { icon ->
            BitmapDrawable(context.resources, icon).also {
                if (it.isSmallerThan(MAX_CLOSE_BUTTON_SIZE_DP)) return@also
            }
        } ?: ContextCompat.getDrawable(context, R.drawable.mozac_ic_close)

        drawableIcon?.let {
            val button = Toolbar.ActionButton(
                it, context.getString(R.string.mozac_feature_customtabs_exit_button)
            ) { closeListener.invoke() }
            toolbar.addNavigationAction(button)
        }
    }

    override fun stop() {}

    /**
     * Removes the current Custom Tabs session when the back button is pressed and returns true.
     * Should be called when the back button is pressed.
     */
    fun onBackPressed(): Boolean {
        val result = sessionManager.runWithSession(sessionId) {
            remove(it)
            true
        }
        closeListener.invoke()
        return result
    }

    internal fun Drawable.isSmallerThan(maxSize: Int): Boolean = with(context.resources) {
        pxToDp(minimumWidth) <= maxSize && pxToDp(minimumHeight) <= maxSize
    }

    companion object {
        internal const val MAX_CLOSE_BUTTON_SIZE_DP = 24

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
