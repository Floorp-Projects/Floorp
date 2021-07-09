/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.view.HapticFeedbackConstants
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.annotation.VisibleForTesting
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import kotlinx.android.extensions.LayoutContainer
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.ExternalAppType
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.behavior.BrowserToolbarBehavior
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.support.utils.URLStringUtils
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.interactor.BrowserToolbarInteractor
import org.mozilla.fenix.customtabs.CustomTabToolbarIntegration
import org.mozilla.fenix.customtabs.CustomTabToolbarMenu
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.ToolbarPopupWindow
import java.lang.ref.WeakReference
import mozilla.components.browser.toolbar.behavior.ToolbarPosition as MozacToolbarPosition

@ExperimentalCoroutinesApi
@SuppressWarnings("LargeClass")
class BrowserToolbarView(
    private val container: ViewGroup,
    private val toolbarPosition: ToolbarPosition,
    private val interactor: BrowserToolbarInteractor,
    private val customTabSession: CustomTabSessionState?,
    private val lifecycleOwner: LifecycleOwner
) : LayoutContainer {

    override val containerView: View?
        get() = container

    private val settings = container.context.settings()

    @LayoutRes
    private val toolbarLayout = when (settings.toolbarPosition) {
        ToolbarPosition.BOTTOM -> R.layout.component_bottom_browser_toolbar
        ToolbarPosition.TOP -> R.layout.component_browser_top_toolbar
    }

    private val layout = LayoutInflater.from(container.context)
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
                interactor::onBrowserToolbarPaste
            )
            true
        }

        with(container.context) {
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

                display.progressGravity = when (toolbarPosition) {
                    ToolbarPosition.BOTTOM -> DisplayToolbar.Gravity.TOP
                    ToolbarPosition.TOP -> DisplayToolbar.Gravity.BOTTOM
                }

                val primaryTextColor = ContextCompat.getColor(
                    container.context,
                    ThemeManager.resolveAttribute(R.attr.primaryText, container.context)
                )
                val secondaryTextColor = ContextCompat.getColor(
                    container.context,
                    ThemeManager.resolveAttribute(R.attr.secondaryText, container.context)
                )
                val separatorColor = ContextCompat.getColor(
                    container.context,
                    ThemeManager.resolveAttribute(R.attr.toolbarDivider, container.context)
                )

                display.urlFormatter = { url -> URLStringUtils.toDisplayUrl(url) }

                display.colors = display.colors.copy(
                    text = primaryTextColor,
                    securityIconSecure = primaryTextColor,
                    securityIconInsecure = primaryTextColor,
                    menu = primaryTextColor,
                    hint = secondaryTextColor,
                    separator = separatorColor,
                    trackingProtection = primaryTextColor,
                    highlight = primaryTextColor
                )

                display.hint = context.getString(R.string.search_hint)
            }

            val menuToolbar: ToolbarMenu
            if (isCustomTabSession) {
                menuToolbar = CustomTabToolbarMenu(
                    context = this,
                    store = components.core.store,
                    sessionId = customTabSession?.id,
                    shouldReverseItems = toolbarPosition == ToolbarPosition.TOP,
                    onItemTapped = {
                        it.performHapticIfNeeded(view)
                        interactor.onBrowserToolbarMenuItemTapped(it)
                    }
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
                    isPinningSupported = isPinningSupported
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
                    isPrivate = customTabSession.content.private
                )
            } else {
                DefaultToolbarIntegration(
                    this,
                    view,
                    menuToolbar,
                    ShippedDomainsProvider().also { it.initialize(this) },
                    components.core.historyStorage,
                    lifecycleOwner,
                    sessionId = null,
                    isPrivate = components.core.store.state.selectedTab?.content?.private ?: false,
                    interactor = interactor,
                    engine = components.core.engine
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
