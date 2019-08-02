/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package mozilla.components.feature.customtabs

import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.BitmapDrawable
import android.net.Uri
import android.view.Window
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuItem
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
import mozilla.components.support.ktx.android.view.setNavigationBarTheme
import mozilla.components.support.ktx.android.view.setStatusBarTheme
import mozilla.components.support.utils.ColorUtils.getReadableTextColor

/**
 * Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig.
 */
class CustomTabsToolbarFeature(
    private val sessionManager: SessionManager,
    private val toolbar: BrowserToolbar,
    private val sessionId: String? = null,
    private val menuBuilder: BrowserMenuBuilder? = null,
    private val menuItemIndex: Int = menuBuilder?.items?.size ?: 0,
    private val window: Window? = null,
    private val shareListener: (() -> Unit)? = null,
    private val closeListener: () -> Unit
) : LifecycleAwareFeature, BackHandler {
    private val context
        get() = toolbar.context
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
            updateToolbarColor(config.toolbarColor, config.navigationBarColor)

            // Add navigation close action
            addCloseButton(session, config.closeButtonIcon)

            // Add action button
            addActionButton(session, config.actionButtonConfig)

            // Show share button
            if (config.showShareMenuItem) {
                addShareButton(session)
            }

            // Add menu items
            if (config.menuItems.isNotEmpty() || menuBuilder?.items?.isNotEmpty() == true) {
                addMenuItems(session, config.menuItems, menuItemIndex)
            }

            return true
        }
        return false
    }

    @VisibleForTesting
    internal fun updateToolbarColor(@ColorInt toolbarColor: Int?, @ColorInt navigationBarColor: Int?) {
        toolbarColor?.let { color ->
            toolbar.setBackgroundColor(color)
            toolbar.textColor = readableColor
            toolbar.titleColor = readableColor
            toolbar.setSiteSecurityColor(readableColor to readableColor)
            toolbar.menuViewColor = readableColor

            window?.setStatusBarTheme(color)
        }
        navigationBarColor?.let { color ->
            window?.setNavigationBarTheme(color)
        }
    }

    @VisibleForTesting
    internal fun addCloseButton(session: Session, bitmap: Bitmap?) {
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
                sessionManager.remove(session)
                closeListener.invoke()
            }
            toolbar.addNavigationAction(button)
        }
    }

    @VisibleForTesting
    internal fun addActionButton(session: Session, buttonConfig: CustomTabActionButtonConfig?) {
        buttonConfig?.let { config ->
            val button = Toolbar.ActionButton(
                BitmapDrawable(context.resources, config.icon),
                config.description
            ) {
                emitActionButtonFact()
                config.pendingIntent.send(
                    context,
                    0,
                    Intent(null, Uri.parse(session.url))
                )
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
            emitActionButtonFact()
            listener.invoke()
        }

        toolbar.addBrowserAction(button)
    }

    @VisibleForTesting
    internal fun addMenuItems(
        session: Session,
        menuItems: List<CustomTabMenuItem>,
        index: Int
    ) {
        menuItems.map {
            SimpleBrowserMenuItem(it.name) {
                it.pendingIntent.send(
                    context,
                    0,
                    Intent(null, Uri.parse(session.url))
                )
            }
        }.also { items ->
            val combinedItems = menuBuilder?.let { builder ->
                val newMenuItemList = mutableListOf<BrowserMenuItem>()
                val maxIndex = builder.items.size

                val insertIndex = if (index in 0..maxIndex) {
                    index
                } else {
                    maxIndex
                }

                newMenuItemList.apply {
                    addAll(builder.items)
                    addAll(insertIndex, items)
                }
            } ?: items

            val combinedExtras = menuBuilder?.let { builder ->
                builder.extras + Pair("customTab", true)
            }

            toolbar.setMenuBuilder(BrowserMenuBuilder(combinedItems, combinedExtras ?: emptyMap()))
        }
    }

    private val sessionObserver = object : Session.Observer {
        override fun onTitleChanged(session: Session, title: String) {
            // Empty title check can be removed when the engine observer issue is fixed
            // https://github.com/mozilla-mobile/android-components/issues/2898
            if (title.isNotEmpty()) {
                // Only shrink the urlTextSize if a title is displayed
                toolbar.textSize = URL_TEXT_SIZE
                toolbar.titleTextSize = TITLE_TEXT_SIZE

                // Explicitly set the title regardless of the customTabConfig settings
                toolbar.title = title
            }
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
        const val TITLE_TEXT_SIZE = 15f
        const val URL_TEXT_SIZE = 12f
    }
}
