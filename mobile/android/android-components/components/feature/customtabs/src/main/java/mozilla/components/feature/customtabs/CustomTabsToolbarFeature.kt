/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.customtabs

import android.app.PendingIntent
import android.app.UiModeManager.MODE_NIGHT_YES
import android.content.Context
import android.content.res.Configuration
import android.graphics.Bitmap
import android.util.Size
import android.view.Window
import androidx.annotation.ColorInt
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AppCompatDelegate
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
import androidx.appcompat.app.AppCompatDelegate.MODE_NIGHT_NO
import androidx.appcompat.app.AppCompatDelegate.NightMode
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK
import androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT
import androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_SYSTEM
import androidx.browser.customtabs.CustomTabsIntent.ColorScheme
import androidx.core.content.ContextCompat.getColor
import androidx.core.graphics.drawable.toDrawable
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.state.ColorSchemeParams
import mozilla.components.browser.state.state.ColorSchemes
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
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.support.ktx.android.content.share
import mozilla.components.support.ktx.android.util.dpToPx
import mozilla.components.support.ktx.android.view.setNavigationBarTheme
import mozilla.components.support.ktx.android.view.setStatusBarTheme
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import mozilla.components.support.utils.ColorUtils.getReadableTextColor
import mozilla.components.support.utils.ext.resizeMaintainingAspectRatio
import mozilla.components.ui.icons.R as iconsR

/**
 * Initializes and resets the [BrowserToolbar] for a Custom Tab based on the [CustomTabConfig].
 *
 * @property store The given [BrowserStore] to use.
 * @property toolbar Reference to the [BrowserToolbar], so that the color and menu items can be set.
 * @property sessionId ID of the custom tab session. No-op if null or invalid.
 * @property useCases The given [CustomTabsUseCases] to use.
 * @property menuBuilder [BrowserMenuBuilder] reference to pull menu options from.
 * @property menuItemIndex Location to insert any custom menu options into the predefined menu list.
 * @property window Reference to the [Window] so the navigation bar color can be set.
 * @property updateTheme Whether or not the toolbar and system bar colors should be changed.
 * @property appNightMode The [NightMode] used in the app. Defaults to [MODE_NIGHT_FOLLOW_SYSTEM].
 * @property forceActionButtonTinting When set to true the [toolbar] action button will always be tinted
 * based on the [toolbar] background, ignoring the value of [CustomTabActionButtonConfig.tint].
 * @property isNavBarEnabled Whether or not the navigation bar is enabled.
 * @property shareListener Invoked when the share button is pressed.
 * @property closeListener Invoked when the close button is pressed.
 */
@Suppress("LargeClass")
class CustomTabsToolbarFeature(
    private val store: BrowserStore,
    private val toolbar: BrowserToolbar,
    private val sessionId: String? = null,
    private val useCases: CustomTabsUseCases,
    private val menuBuilder: BrowserMenuBuilder? = null,
    private val menuItemIndex: Int = menuBuilder?.items?.size ?: 0,
    private val window: Window? = null,
    private val updateTheme: Boolean = true,
    @NightMode private val appNightMode: Int = MODE_NIGHT_FOLLOW_SYSTEM,
    private val forceActionButtonTinting: Boolean = false,
    private val isNavBarEnabled: Boolean = false,
    private val shareListener: (() -> Unit)? = null,
    private val closeListener: () -> Unit,
) : LifecycleAwareFeature, UserInteractionHandler {
    private var initialized: Boolean = false
    private val titleObserver = CustomTabSessionTitleObserver(toolbar)
    private val context get() = toolbar.context
    private var scope: CoroutineScope? = null

    /**
     * Gets the current custom tab session.
     */
    private val session: CustomTabSessionState?
        get() = sessionId?.let { store.state.findCustomTab(it) }

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
            init(tab.config)
        }
    }

    /**
     * Unregisters the [CustomTabSessionTitleObserver].
     */
    override fun stop() {
        scope?.cancel()
    }

    @VisibleForTesting
    internal fun init(
        config: CustomTabConfig,
        setAppNightMode: (Int) -> Unit = AppCompatDelegate::setDefaultNightMode,
    ) {
        // Don't allow clickable toolbar so a custom tab can't switch to edit mode.
        toolbar.display.onUrlClicked = { false }
        toolbar.display.hidePageActionSeparator()

        // Use the intent provided color scheme or fallback to the app night mode preference.
        val nightMode = config.colorScheme?.toNightMode() ?: appNightMode

        if (updateTheme) {
            setAppNightMode.invoke(nightMode)
        }

        val colorSchemeParams = config.colorSchemes?.getConfiguredColorSchemeParams(
            nightMode = nightMode,
            isDarkMode = context.isDarkMode(),
        )

        val readableColor = if (updateTheme) {
            colorSchemeParams?.toolbarColor?.let { getReadableTextColor(it) }
                ?: toolbar.display.colors.menu
        } else {
            // It's private mode, the readable color needs match the app.
            // Note: The main app is configuring the private theme, Custom Tabs is adding the
            // additional theming for the dynamic UI elements e.g. action & share buttons.
            val colorResId = context.theme.resolveAttribute(android.R.attr.textColorPrimary)
            getColor(context, colorResId)
        }

        if (updateTheme) {
            colorSchemeParams.let {
                updateTheme(
                    toolbarColor = it?.toolbarColor,
                    navigationBarColor = it?.navigationBarColor ?: it?.toolbarColor,
                    navigationBarDividerColor = it?.navigationBarDividerColor,
                    readableColor = readableColor,
                )
            }
        }

        // Add navigation close action
        if (config.showCloseButton) {
            addCloseButton(readableColor, config.closeButtonIcon)
        }

        // Add action button
        addActionButton(readableColor, config.actionButtonConfig)

        // Show share button
        if (config.showShareMenuItem) {
            addShareButton(readableColor)
        }

        // Add menu items
        if (config.menuItems.isNotEmpty() || menuBuilder?.items?.isNotEmpty() == true) {
            addMenuItems(config.menuItems, menuItemIndex)
        }

        if (isNavBarEnabled) {
            toolbar.display.hideMenuButton()
        }
    }

    @VisibleForTesting
    internal fun updateTheme(
        @ColorInt toolbarColor: Int? = null,
        @ColorInt navigationBarColor: Int? = null,
        @ColorInt navigationBarDividerColor: Int? = null,
        @ColorInt readableColor: Int,
    ) {
        toolbarColor?.let {
            toolbar.setBackgroundColor(it)

            toolbar.display.colors = toolbar.display.colors.copy(
                text = readableColor,
                title = readableColor,
                securityIconSecure = readableColor,
                securityIconInsecure = readableColor,
                trackingProtection = readableColor,
                menu = readableColor,
            )

            window?.setStatusBarTheme(it)
        }

        if (navigationBarColor != null || navigationBarDividerColor != null) {
            window?.setNavigationBarTheme(navigationBarColor, navigationBarDividerColor)
        }
    }

    /**
     * Display a close button at the start of the toolbar.
     * When clicked, it calls [closeListener].
     */
    @VisibleForTesting
    internal fun addCloseButton(@ColorInt readableColor: Int, bitmap: Bitmap?) {
        val drawableIcon = bitmap?.toDrawable(context.resources)
            ?: getDrawable(context, iconsR.drawable.mozac_ic_cross_24)!!.mutate()

        drawableIcon.setTint(readableColor)

        val button = Toolbar.ActionButton(
            drawableIcon,
            context.getString(R.string.mozac_feature_customtabs_exit_button),
        ) {
            emitCloseFact()
            session?.let {
                useCases.remove(it.id)
            }
            closeListener.invoke()
        }
        toolbar.addNavigationAction(button)
    }

    /**
     * Display an action button from the custom tab config on the toolbar.
     * When clicked, it activates the corresponding [PendingIntent].
     */
    @VisibleForTesting
    internal fun addActionButton(
        @ColorInt readableColor: Int,
        buttonConfig: CustomTabActionButtonConfig?,
    ) {
        buttonConfig?.let { config ->
            val icon = config.icon
            val scaledIconSize = icon.resizeMaintainingAspectRatio(ACTION_BUTTON_MAX_DRAWABLE_DP_SIZE)
            val drawableIcon = Bitmap.createScaledBitmap(
                icon,
                scaledIconSize.width.dpToPx(context.resources.displayMetrics),
                scaledIconSize.height.dpToPx(context.resources.displayMetrics),
                true,
            ).toDrawable(context.resources)

            if (config.tint || forceActionButtonTinting) {
                drawableIcon.setTint(readableColor)
            }

            val button = Toolbar.ActionButton(
                drawableIcon,
                config.description,
            ) {
                emitActionButtonFact()
                session?.let {
                    config.pendingIntent.sendWithUrl(context, it.content.url)
                }
            }

            toolbar.addBrowserAction(button)
        }
    }

    /**
     * Display a share button as a button on the toolbar.
     * When clicked, it activates [shareListener] and defaults to the [share] KTX helper.
     */
    @VisibleForTesting
    internal fun addShareButton(@ColorInt readableColor: Int) {
        val drawableIcon = getDrawable(context, iconsR.drawable.mozac_ic_share_android_24)!!
        drawableIcon.setTint(readableColor)

        val button = Toolbar.ActionButton(
            drawableIcon,
            context.getString(R.string.mozac_feature_customtabs_share_link),
        ) {
            val listener = shareListener ?: {
                session?.let {
                    context.share(it.content.url)
                }
            }
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
        menuItems: List<CustomTabMenuItem>,
        index: Int,
    ) {
        menuItems.map { item ->
            SimpleBrowserMenuItem(item.name) {
                session?.let {
                    item.pendingIntent.sendWithUrl(context, it.content.url)
                }
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
        private val ACTION_BUTTON_MAX_DRAWABLE_DP_SIZE = Size(48, 24)
    }
}

@VisibleForTesting
internal fun ColorSchemes.getConfiguredColorSchemeParams(
    @NightMode nightMode: Int? = null,
    isDarkMode: Boolean = false,
) = when {
    noColorSchemeParamsSet() -> null

    defaultColorSchemeParamsOnly() -> defaultColorSchemeParams

    // Try to follow specified color scheme.
    nightMode == MODE_NIGHT_FOLLOW_SYSTEM -> {
        if (isDarkMode) {
            darkColorSchemeParams?.withDefault(defaultColorSchemeParams)
                ?: defaultColorSchemeParams
        } else {
            lightColorSchemeParams?.withDefault(defaultColorSchemeParams)
                ?: defaultColorSchemeParams
        }
    }

    nightMode == MODE_NIGHT_NO -> lightColorSchemeParams?.withDefault(
        defaultColorSchemeParams,
    ) ?: defaultColorSchemeParams

    nightMode == MODE_NIGHT_YES -> darkColorSchemeParams?.withDefault(
        defaultColorSchemeParams,
    ) ?: defaultColorSchemeParams

    // No color scheme set, try to use default.
    else -> defaultColorSchemeParams
}

/**
 * Try to convert the given [ColorScheme] to [NightMode].
 */
@VisibleForTesting
@NightMode
internal fun Int.toNightMode() = when (this) {
    COLOR_SCHEME_SYSTEM -> MODE_NIGHT_FOLLOW_SYSTEM
    COLOR_SCHEME_LIGHT -> MODE_NIGHT_NO
    COLOR_SCHEME_DARK -> MODE_NIGHT_YES
    else -> null
}

private fun ColorSchemes.noColorSchemeParamsSet() =
    defaultColorSchemeParams == null && lightColorSchemeParams == null && darkColorSchemeParams == null

private fun ColorSchemes.defaultColorSchemeParamsOnly() =
    defaultColorSchemeParams != null && lightColorSchemeParams == null && darkColorSchemeParams == null

private fun Context.isDarkMode() =
    resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK == Configuration.UI_MODE_NIGHT_YES

/**
 * Try to create a [ColorSchemeParams] using the given [defaultColorSchemeParam] as a fallback if
 * there are missing properties.
 */
@VisibleForTesting
internal fun ColorSchemeParams.withDefault(defaultColorSchemeParam: ColorSchemeParams?) = ColorSchemeParams(
    toolbarColor = toolbarColor
        ?: defaultColorSchemeParam?.toolbarColor,
    secondaryToolbarColor = secondaryToolbarColor
        ?: defaultColorSchemeParam?.secondaryToolbarColor,
    navigationBarColor = navigationBarColor
        ?: defaultColorSchemeParam?.navigationBarColor,
    navigationBarDividerColor = navigationBarDividerColor
        ?: defaultColorSchemeParam?.navigationBarDividerColor,
)
