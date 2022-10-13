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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.CustomTabActionButtonConfig
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.CustomTabMenuItem
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.toolbar.Toolbar
import mozilla.components.feature.customtabs.feature.CustomTabSessionTitleObserver
import mozilla.components.feature.customtabs.menu.sendWithUrl
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.base.feature.UserInteractionHandler
import mozilla.components.support.ktx.android.content.share
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.ktx.android.view.setNavigationBarTheme
import mozilla.components.support.ktx.android.view.setStatusBarTheme
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import mozilla.components.support.utils.ColorUtils.getReadableTextColor

/**
 * Initializes and resets the Toolbar for a Custom Tab based on the CustomTabConfig.
 *
 * @param toolbar Reference to the browser toolbar, so that the color and menu items can be set.
 * @param sessionId ID of the custom tab session. No-op if null or invalid.
 * @param menuBuilder Menu builder reference to pull menu options from.
 * @param menuItemIndex Location to insert any custom menu options into the predefined menu list.
 * @param window Reference to the window so the navigation bar color can be set.
 * @param shareListener Invoked when the share button is pressed.
 * @param closeListener Invoked when the close button is pressed.
 * @param updateToolbarBackground Whether or not the toolbar background should be changed based on
 * [CustomTabConfig.toolbarColor].
 * @param forceActionButtonTinting When set to true the toolbar action button will always be tinted
 * based on the toolbar background, ignoring the value of [CustomTabActionButtonConfig.tint].
 */
@Suppress("LargeClass", "LongParameterList")
class CustomTabsToolbarFeature(
    private val store: BrowserStore,
    private val toolbar: BrowserToolbar,
    private val sessionId: String? = null,
    private val useCases: CustomTabsUseCases,
    private val menuBuilder: BrowserMenuBuilder? = null,
    private val menuItemIndex: Int = menuBuilder?.items?.size ?: 0,
    private val window: Window? = null,
    private val updateToolbarBackground: Boolean = true,
    private val forceActionButtonTinting: Boolean = false,
    private val shareListener: (() -> Unit)? = null,
    private val closeListener: () -> Unit,
) : LifecycleAwareFeature, UserInteractionHandler {
    private var initialized: Boolean = false
    private val titleObserver = CustomTabSessionTitleObserver(toolbar)
    private val context get() = toolbar.context
    internal var readableColor = Color.WHITE
    private var scope: CoroutineScope? = null

    /**
     * Initializes the feature and registers the [CustomTabSessionTitleObserver].
     */
    override fun start() {
        val tabId = sessionId ?: return
        val tab = store.state.findCustomTab(tabId) ?: return

        scope = store.flowScoped { flow ->
            flow
                .mapNotNull { state -> state.findCustomTab(tabId) }
                .ifAnyChanged { tab -> arrayOf(tab.content.title, tab.content.url) }
                .collect { tab -> titleObserver.onTab(tab) }
        }

        if (!initialized) {
            initialized = true
            init(tab)
        }
    }

    /**
     * Unregisters the [CustomTabSessionTitleObserver].
     */
    override fun stop() {
        scope?.cancel()
    }

    @VisibleForTesting
    internal fun init(tab: CustomTabSessionState) {
        val config = tab.config

        // Don't allow clickable toolbar so a custom tab can't switch to edit mode.
        toolbar.display.onUrlClicked = { false }

        // If it's available, hold on to the readable colour for other assets.
        if (updateToolbarBackground && config.toolbarColor != null) {
            readableColor = getReadableTextColor(config.toolbarColor!!)
        }

        // Change the toolbar colour
        updateToolbarColor(config.toolbarColor, config.navigationBarColor)

        // Add navigation close action
        if (config.showCloseButton) {
            addCloseButton(tab, config.closeButtonIcon)
        }

        // Add action button
        addActionButton(tab, config.actionButtonConfig)

        // Show share button
        if (config.showShareMenuItem) {
            addShareButton(tab)
        }

        // Add menu items
        if (config.menuItems.isNotEmpty() || menuBuilder?.items?.isNotEmpty() == true) {
            addMenuItems(tab, config.menuItems, menuItemIndex)
        }
    }

    @VisibleForTesting
    internal fun updateToolbarColor(@ColorInt toolbarColor: Int?, @ColorInt navigationBarColor: Int?) {
        if (updateToolbarBackground && toolbarColor != null) {
            toolbar.setBackgroundColor(toolbarColor)

            toolbar.display.colors = toolbar.display.colors.copy(
                text = readableColor,
                title = readableColor,
                securityIconSecure = readableColor,
                securityIconInsecure = readableColor,
                trackingProtection = readableColor,
                menu = readableColor,
            )

            window?.setStatusBarTheme(toolbarColor)
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
    internal fun addCloseButton(tab: CustomTabSessionState, bitmap: Bitmap?) {
        val drawableIcon = bitmap?.toDrawable(context.resources)
            ?: getDrawable(context, R.drawable.mozac_ic_close)!!.mutate()

        drawableIcon.setTint(readableColor)

        val button = Toolbar.ActionButton(
            drawableIcon,
            context.getString(R.string.mozac_feature_customtabs_exit_button),
        ) {
            emitCloseFact()
            useCases.remove(tab.id)
            closeListener.invoke()
        }
        toolbar.addNavigationAction(button)
    }

    /**
     * Display an action button from the custom tab config on the toolbar.
     * When clicked, it activates the corresponding [PendingIntent].
     */
    @VisibleForTesting
    internal fun addActionButton(tab: CustomTabSessionState, buttonConfig: CustomTabActionButtonConfig?) {
        buttonConfig?.let { config ->
            val drawableIcon = Bitmap.createScaledBitmap(
                config.icon,
                ACTION_BUTTON_DRAWABLE_WIDTH_DP.dpToPx(context.resources.displayMetrics),
                ACTION_BUTTON_DRAWABLE_HEIGHT_DP.dpToPx(context.resources.displayMetrics),
                true,
            )
                .toDrawable(context.resources)
            if (config.tint || forceActionButtonTinting) {
                drawableIcon.setTint(readableColor)
            }

            val button = Toolbar.ActionButton(
                drawableIcon,
                config.description,
            ) {
                emitActionButtonFact()
                config.pendingIntent.sendWithUrl(context, tab.content.url)
            }

            toolbar.addBrowserAction(button)
        }
    }

    /**
     * Display a share button as a button on the toolbar.
     * When clicked, it activates [shareListener] and defaults to the [share] KTX helper.
     */
    @VisibleForTesting
    internal fun addShareButton(tab: CustomTabSessionState) {
        val drawableIcon = getDrawable(context, R.drawable.mozac_ic_share)!!
        drawableIcon.setTint(readableColor)

        val button = Toolbar.ActionButton(
            drawableIcon,
            context.getString(R.string.mozac_feature_customtabs_share_link),
        ) {
            val listener = shareListener ?: { context.share(tab.content.url) }
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
        tab: CustomTabSessionState,
        menuItems: List<CustomTabMenuItem>,
        index: Int,
    ) {
        menuItems.map { item ->
            SimpleBrowserMenuItem(item.name) {
                item.pendingIntent.sendWithUrl(context, tab.content.url)
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
            if (sessionId != null && useCases.remove(sessionId)) {
                closeListener.invoke()
                true
            } else {
                false
            }
        }
    }

    companion object {
        private const val ACTION_BUTTON_DRAWABLE_WIDTH_DP = 24
        private const val ACTION_BUTTON_DRAWABLE_HEIGHT_DP = 24
    }
}
