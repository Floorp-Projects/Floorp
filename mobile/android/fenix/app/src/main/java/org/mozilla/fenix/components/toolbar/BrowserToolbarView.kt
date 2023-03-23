/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.graphics.Color
import android.view.HapticFeedbackConstants
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.behavior.BrowserToolbarBehavior
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.support.ktx.util.URLStringUtils
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.interactor.BrowserToolbarInteractor
import org.mozilla.fenix.customtabs.CustomTabToolbarIntegration
import org.mozilla.fenix.customtabs.CustomTabToolbarMenu
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.utils.ToolbarPopupWindow
import java.lang.ref.WeakReference
import mozilla.components.browser.toolbar.behavior.ToolbarPosition as MozacToolbarPosition

@SuppressWarnings("LargeClass")
class BrowserToolbarView(
    context: Context,
    container: ViewGroup,
    private val settings: Settings,
    private val interactor: BrowserToolbarInteractor,
    private val customTabSession: CustomTabSessionState?,
    private val lifecycleOwner: LifecycleOwner,
) {

    @LayoutRes
    private val toolbarLayout = when (settings.toolbarPosition) {
        ToolbarPosition.BOTTOM -> R.layout.component_bottom_browser_toolbar
        ToolbarPosition.TOP -> R.layout.component_browser_top_toolbar
    }

    private val layout = LayoutInflater.from(context)
        .inflate(toolbarLayout, container, true)

    @VisibleForTesting
    internal var view: BrowserToolbar = layout
        .findViewById(R.id.toolbar)

    val toolbarIntegration: ToolbarIntegration

    @VisibleForTesting
    internal val isPwaTabOrTwaTab: Boolean
        get() = customTabSession?.config?.externalAppType == ExternalAppType.PROGRESSIVE_WEB_APP ||
            customTabSession?.config?.externalAppType == ExternalAppType.TRUSTED_WEB_ACTIVITY

    init {
        val isCustomTabSession = customTabSession != null

        view.display.setOnUrlLongClickListener {
            ToolbarPopupWindow.show(
                WeakReference(view),
                customTabSession?.id,
                interactor::onBrowserToolbarPasteAndGo,
                interactor::onBrowserToolbarPaste,
            )
            true
        }

        with(context) {
            val isPinningSupported = components.useCases.webAppUseCases.isPinningSupported()

            view.apply {
                setToolbarBehavior()

                elevation = resources.getDimension(R.dimen.browser_fragment_toolbar_elevation)
                if (!isCustomTabSession) {
                    display.setUrlBackground(getDrawable(R.drawable.search_url_background))
                }

                display.onUrlClicked = {
                    interactor.onBrowserToolbarClicked()
                    false
                }

                display.progressGravity = when (settings.toolbarPosition) {
                    ToolbarPosition.BOTTOM -> DisplayToolbar.Gravity.TOP
                    ToolbarPosition.TOP -> DisplayToolbar.Gravity.BOTTOM
                }

                val primaryTextColor = ContextCompat.getColor(
                    context,
                    ThemeManager.resolveAttribute(R.attr.textPrimary, context),
                )
                val secondaryTextColor = ContextCompat.getColor(
                    context,
                    ThemeManager.resolveAttribute(R.attr.textSecondary, context),
                )
                val separatorColor = ContextCompat.getColor(
                    context,
                    ThemeManager.resolveAttribute(R.attr.borderPrimary, context),
                )

                display.urlFormatter = { url -> URLStringUtils.toDisplayUrl(url) }

                display.colors = display.colors.copy(
                    text = primaryTextColor,
                    securityIconSecure = primaryTextColor,
                    securityIconInsecure = Color.TRANSPARENT,
                    menu = primaryTextColor,
                    hint = secondaryTextColor,
                    separator = separatorColor,
                    trackingProtection = primaryTextColor,
                    highlight = ContextCompat.getColor(
                        context,
                        R.color.fx_mobile_icon_color_information,
                    ),
                )

                display.hint = context.getString(R.string.search_hint)
            }

            val menuToolbar: ToolbarMenu
            if (isCustomTabSession) {
                menuToolbar = CustomTabToolbarMenu(
                    context = this,
                    store = components.core.store,
                    sessionId = customTabSession?.id,
                    shouldReverseItems = settings.toolbarPosition == ToolbarPosition.TOP,
                    isSandboxCustomTab = false,
                    onItemTapped = {
                        it.performHapticIfNeeded(view)
                        interactor.onBrowserToolbarMenuItemTapped(it)
                    },
                )
            } else {
                menuToolbar = DefaultToolbarMenu(
                    context = this,
                    store = components.core.store,
                    hasAccountProblem = components.backgroundServices.accountManager.accountNeedsReauth(),
                    onItemTapped = {
                        it.performHapticIfNeeded(view)
                        interactor.onBrowserToolbarMenuItemTapped(it)
                    },
                    lifecycleOwner = lifecycleOwner,
                    bookmarksStorage = bookmarkStorage,
                    pinnedSiteStorage = components.core.pinnedSiteStorage,
                    isPinningSupported = isPinningSupported,
                )
                view.display.setMenuDismissAction {
                    view.invalidateActions()
                }
            }

            toolbarIntegration = if (customTabSession != null) {
                CustomTabToolbarIntegration(
                    this,
                    view,
                    menuToolbar,
                    customTabSession.id,
                    isPrivate = customTabSession.content.private,
                )
            } else {
                DefaultToolbarIntegration(
                    this,
                    view,
                    menuToolbar,
                    lifecycleOwner,
                    sessionId = null,
                    isPrivate = components.core.store.state.selectedTab?.content?.private ?: false,
                    interactor = interactor,
                )
            }
        }
    }

    fun expand() {
        // expand only for normal tabs and custom tabs not for PWA or TWA
        if (isPwaTabOrTwaTab) {
            return
        }

        (view.layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? BrowserToolbarBehavior)?.forceExpand(view)
        }
    }

    fun collapse() {
        // collapse only for normal tabs and custom tabs not for PWA or TWA. Mirror expand()
        if (isPwaTabOrTwaTab) {
            return
        }

        (view.layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            (behavior as? BrowserToolbarBehavior)?.forceCollapse(view)
        }
    }

    fun dismissMenu() {
        view.dismissMenu()
    }

    /**
     * Sets whether the toolbar will have a dynamic behavior (to be scrolled) or not.
     *
     * This will intrinsically check and disable the dynamic behavior if
     *  - this is disabled in app settings
     *  - toolbar is placed at the bottom and tab shows a PWA or TWA
     *
     *  Also if the user has not explicitly set a toolbar position and has a screen reader enabled
     *  the toolbar will be placed at the top and in a fixed position.
     *
     * @param shouldDisableScroll force disable of the dynamic behavior irrespective of the intrinsic checks.
     */
    fun setToolbarBehavior(shouldDisableScroll: Boolean = false) {
        when (settings.toolbarPosition) {
            ToolbarPosition.BOTTOM -> {
                if (settings.isDynamicToolbarEnabled && !isPwaTabOrTwaTab && !settings.shouldUseFixedTopToolbar) {
                    setDynamicToolbarBehavior(MozacToolbarPosition.BOTTOM)
                } else {
                    expandToolbarAndMakeItFixed()
                }
            }
            ToolbarPosition.TOP -> {
                if (settings.shouldUseFixedTopToolbar ||
                    !settings.isDynamicToolbarEnabled ||
                    shouldDisableScroll
                ) {
                    expandToolbarAndMakeItFixed()
                } else {
                    setDynamicToolbarBehavior(MozacToolbarPosition.TOP)
                }
            }
        }
    }

    @VisibleForTesting
    internal fun expandToolbarAndMakeItFixed() {
        expand()
        (view.layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            behavior = null
        }
    }

    @VisibleForTesting
    internal fun setDynamicToolbarBehavior(toolbarPosition: MozacToolbarPosition) {
        (view.layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
            behavior = BrowserToolbarBehavior(view.context, null, toolbarPosition)
        }
    }

    @Suppress("ComplexCondition")
    private fun ToolbarMenu.Item.performHapticIfNeeded(view: View) {
        if (this is ToolbarMenu.Item.Reload && this.bypassCache ||
            this is ToolbarMenu.Item.Back && this.viewHistory ||
            this is ToolbarMenu.Item.Forward && this.viewHistory
        ) {
            view.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
        }
    }
}
