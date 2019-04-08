/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.customtabs

import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.support.annotation.VisibleForTesting
import android.support.v4.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSession
import mozilla.components.browser.session.tab.CustomTabActionButtonConfig
import mozilla.components.browser.session.tab.CustomTabMenuItem
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.support.base.feature.BackHandler
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.content.share

/**
 * Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig.
 */
class CustomTabsToolbarFeature(
    private val sessionManager: SessionManager,
    private val toolbar: BrowserToolbar,
    private val sessionId: String? = null,
    private val menuBuilder: BrowserMenuBuilder? = null,
    private val shareListener: (() -> Unit)? = null,
    private val closeListener: () -> Unit
) : LifecycleAwareFeature, BackHandler {
    private val context = toolbar.context
    internal var initialized = false
    internal var readableColor = Color.WHITE

    override fun start() {
        if (initialized) {
            return
        }
        initialized = sessionManager.runWithSession(sessionId) {
            it.register(sessionObserver)
            initialize(it)
        }
    }

    @VisibleForTesting
    internal fun initialize(session: Session): Boolean {
        session.customTabConfig?.let { config ->
            // Don't allow clickable toolbar so a custom tab can't switch to edit mode.
            toolbar.onUrlClicked = { false }
            // If it's available, hold on to the readable colour for other assets.
            config.toolbarColor?.let {
                readableColor = getReadableTextColor(it)
            }
            // Change the toolbar colour
            updateToolbarColor(config.toolbarColor)
            // Add navigation close action
            addCloseButton(config.closeButtonIcon)
            // Add action button
            addActionButton(config.actionButtonConfig)
            // Show share button
            if (config.showShareMenuItem) addShareButton(session)
            // Add menu items
            if (config.menuItems.isNotEmpty()) addMenuItems(config.menuItems)

            // Explicitly set the title regardless of the customTabConfig settings
            toolbar.titleTextSize = TITLE_TEXT_SIZE

            return true
        }
        return false
    }

    @VisibleForTesting
    internal fun updateToolbarColor(toolbarColor: Int?) {
        toolbarColor?.let { color ->
            toolbar.setBackgroundColor(color)
            toolbar.textColor = readableColor
            toolbar.titleColor = readableColor
            toolbar.siteSecurityColor = Pair(readableColor, readableColor)
            toolbar.menuViewColor = readableColor
        }
    }

    @VisibleForTesting
    internal fun addCloseButton(bitmap: Bitmap?) {
        val drawableIcon = bitmap?.let { BitmapDrawable(context.resources, it) } ?: ContextCompat.getDrawable(
            context,
            R.drawable.mozac_ic_close
        )

        drawableIcon?.apply {
            setTint(readableColor)
        }.also {
            val button = Toolbar.ActionButton(
                it, context.getString(R.string.mozac_feature_customtabs_exit_button)
            ) {
                emitCloseFact()
                closeListener.invoke()
            }
            toolbar.addNavigationAction(button)
        }
    }

    @VisibleForTesting
    internal fun addActionButton(buttonConfig: CustomTabActionButtonConfig?) {
        buttonConfig?.let { config ->
            val button = Toolbar.ActionButton(
                BitmapDrawable(context.resources, config.icon),
                config.description
            ) {
                emitActionButtonFact()
                config.pendingIntent.send()
            }

            toolbar.addBrowserAction(button)
        }
    }

    @VisibleForTesting
    internal fun addShareButton(session: Session) {
        val drawable = ContextCompat.getDrawable(context, R.drawable.mozac_ic_share)?.apply {
            setTint(readableColor)
        }

        val button = Toolbar.ActionButton(drawable, context.getString(R.string.mozac_feature_customtabs_share_link)) {
            val listener = shareListener ?: { context.share(session.url) }
            listener.invoke()
        }

        toolbar.addBrowserAction(button)
    }

    @VisibleForTesting
    internal fun addMenuItems(menuItems: List<CustomTabMenuItem>) {
        menuItems.map {
            SimpleBrowserMenuItem(it.name) { it.pendingIntent.send() }
        }.also { items ->
            val combinedItems = menuBuilder?.let { builder -> builder.items + items } ?: items
            val combinedExtras = menuBuilder?.let {
                    builder -> builder.extras + Pair("customTab", true)
            }
            toolbar.setMenuBuilder(BrowserMenuBuilder(combinedItems, combinedExtras ?: emptyMap()))
        }
    }

    private val sessionObserver = object : Session.Observer {
        override fun onTitleChanged(session: Session, title: String) {
            // Only shrink the urlTextSize if a title is displayed
            toolbar.textSize = URL_TEXT_SIZE
            toolbar.title = title
        }
    }

    override fun stop() {
        sessionManager.runWithSession(sessionId) {
            it.unregister(sessionObserver)
            true
        }
    }

    /**
     * When the back button is pressed if not initialized returns false,
     * when initialized removes the current Custom Tabs session and returns true.
     * Should be called when the back button is pressed.
     */
    override fun onBackPressed(): Boolean {
        return if (!initialized) {
            false
        } else {
            sessionManager.runWithSession(sessionId) {
                closeListener.invoke()
                remove(it)
                true
            }
        }
    }

    companion object {
        const val TITLE_TEXT_SIZE = 16f
        const val URL_TEXT_SIZE = 12f
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
