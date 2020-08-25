/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.PendingIntent
import android.graphics.Bitmap
import android.graphics.Color
import android.view.Window
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.core.graphics.drawable.toDrawable
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSession
import mozilla.components.browser.state.state.CustomTabActionButtonConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.customtabs.feature.CustomTabSessionTitleObserver
import mozilla.components.feature.customtabs.menu.sendWithUrl
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.content.share
import mozilla.components.support.ktx.android.view.setNavigationBarTheme
import mozilla.components.support.ktx.android.view.setStatusBarTheme
import mozilla.components.support.utils.ColorUtils.getReadableTextColor

/**
 * Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig.
 * @param toolbar Reference to the browser toolbar, so that the color and menu items can be set.
 * @param sessionId ID of the custom tab session. No-op if null or invalid.
 * @param menuBuilder Menu builder reference to pull menu options from.
 * @param menuItemIndex Location to insert any custom menu options into the predefined menu list.
 * @param window Reference to the window so the navigation bar color can be set.
 * @param shareListener Invoked when the share button is pressed.
 * @param closeListener Invoked when the close button is pressed.
 */
@Suppress("LargeClass")
class CustomTabsToolbarFeature(
    private val sessionManager: SessionManager,
    private val toolbar: BrowserToolbar,
    private val sessionId: String? = null,
    private val menuBuilder: BrowserMenuBuilder? = null,
    private val menuItemIndex: Int = menuBuilder?.items?.size ?: 0,
    private val window: Window? = null,
    private val shareListener: (() -> Unit)? = null,
    private val closeListener: () -> Unit
) : LifecycleAwareFeature, UserInteractionHandler {

    private val sessionObserver = CustomTabSessionTitleObserver(toolbar)
    private val context get() = toolbar.context
    internal var initialized = false
    internal var readableColor = Color.WHITE

    /**
     * Initializes the feature and registers the [CustomTabSessionTitleObserver].
     */
    override fun start() {
        if (initialized) return

        initialized = sessionManager.runWithSession(sessionId) {
            it.register(sessionObserver)
            initialize(it)
        }
    }

    /**
     * Unregisters the [CustomTabSessionTitleObserver].
     */
    override fun stop() {
        sessionManager.runWithSession(sessionId) {
            it.unregister(sessionObserver)
            true
        }
    }

    @VisibleForTesting
    internal fun initialize(session: Session): Boolean {
        val config = session.customTabConfig ?: return false

        // Don't allow clickable toolbar so a custom tab can't switch to edit mode.
        toolbar.display.onUrlClicked = { false }

        // If it's available, hold on to the readable colour for other assets.
        config.toolbarColor?.let {
            readableColor = getReadableTextColor(it)
        }

        // Change the toolbar colour
        updateToolbarColor(config.toolbarColor, config.navigationBarColor)

        // Add navigation close action
        if (config.showCloseButton) {
            addCloseButton(session, config.closeButtonIcon)
        }

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

    @VisibleForTesting
    internal fun updateToolbarColor(@ColorInt toolbarColor: Int?, @ColorInt navigationBarColor: Int?) {
        toolbarColor?.let { color ->
            toolbar.setBackgroundColor(color)

            toolbar.display.colors = toolbar.display.colors.copy(
                text = readableColor,
                title = readableColor,
                securityIconSecure = readableColor,
                securityIconInsecure = readableColor,
                trackingProtection = readableColor,
                menu = readableColor
            )

            window?.setStatusBarTheme(color)
        }
        navigationBarColor?.let { color ->
            window?.setNavigationBarTheme(color)
        }
    }

    /**
     * Display a close button at the start of the toolbar.
     * When clicked, it calls [closeListener].
     */
    @VisibleForTesting
    internal fun addCloseButton(session: Session, bitmap: Bitmap?) {
        val drawableIcon = bitmap?.toDrawable(context.resources)
            ?: getDrawable(context, R.drawable.mozac_ic_close)!!.mutate()

        drawableIcon.setTint(readableColor)

        val button = Toolbar.ActionButton(
            drawableIcon,
            context.getString(R.string.mozac_feature_customtabs_exit_button)
        ) {
            emitCloseFact()
            sessionManager.remove(session)
            closeListener.invoke()
        }
        toolbar.addNavigationAction(button)
    }

    /**
     * Display an action button from the custom tab config on the toolbar.
     * When clicked, it activates the corresponding [PendingIntent].
     */
    @VisibleForTesting
    internal fun addActionButton(session: Session, buttonConfig: CustomTabActionButtonConfig?) {
        buttonConfig?.let { config ->
            val drawableIcon = config.icon.toDrawable(context.resources)
            if (config.tint) {
                drawableIcon.setTint(readableColor)
            }

            val button = Toolbar.ActionButton(
                drawableIcon,
                config.description
            ) {
                emitActionButtonFact()
                config.pendingIntent.sendWithUrl(context, session.url)
            }

            toolbar.addBrowserAction(button)
        }
    }

    /**
     * Display a share button as a button on the toolbar.
     * When clicked, it activates [shareListener] and defaults to the [share] KTX helper.
     */
    @VisibleForTesting
    internal fun addShareButton(session: Session) {
        val drawableIcon = getDrawable(context, R.drawable.mozac_ic_share)!!
        drawableIcon.setTint(readableColor)

        val button = Toolbar.ActionButton(
            drawableIcon,
            context.getString(R.string.mozac_feature_customtabs_share_link)
        ) {
            val listener = shareListener ?: { context.share(session.url) }
            emitActionButtonFact()
            listener.invoke()
        }

        toolbar.addBrowserAction(button)
    }

    /**
     * Build the menu items displayed when the 3-dot overflow menu is opened.
     */
    @VisibleForTesting
    internal fun addMenuItems(
        session: Session,
        menuItems: List<CustomTabMenuItem>,
        index: Int
    ) {
        menuItems.map { item ->
            SimpleBrowserMenuItem(item.name) {
                item.pendingIntent.sendWithUrl(context, session.url)
            }
        }.also { items ->
            val combinedItems = menuBuilder?.let { builder ->
                val newMenuItemList = mutableListOf<BrowserMenuItem>()
                val insertIndex = index.coerceIn(0, builder.items.size)

                newMenuItemList.apply {
                    addAll(builder.items)
                    addAll(insertIndex, items)
                }
            } ?: items

            val combinedExtras = menuBuilder?.let { builder ->
                builder.extras + Pair("customTab", true)
            }

            toolbar.display.menuBuilder = BrowserMenuBuilder(combinedItems, combinedExtras.orEmpty())
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
}
