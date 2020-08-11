/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.view.HapticFeedbackConstants
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.LayoutRes
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.view.updateLayoutParams
import androidx.lifecycle.LifecycleOwner
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_ENTER_ALWAYS
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_EXIT_UNTIL_COLLAPSED
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SNAP
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.component_browser_top_toolbar.*
import kotlinx.android.synthetic.main.component_browser_top_toolbar.view.*
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.session.Session
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.behavior.BrowserToolbarBottomBehavior
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.support.ktx.android.util.dpToFloat
import mozilla.components.support.utils.URLStringUtils
import org.mozilla.fenix.R
import org.mozilla.fenix.customtabs.CustomTabToolbarIntegration
import org.mozilla.fenix.customtabs.CustomTabToolbarMenu
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.utils.ToolbarPopupWindow
import java.lang.ref.WeakReference

interface BrowserToolbarViewInteractor {
    fun onBrowserToolbarPaste(text: String)
    fun onBrowserToolbarPasteAndGo(text: String)
    fun onBrowserToolbarClicked()
    fun onBrowserToolbarMenuItemTapped(item: ToolbarMenu.Item)
    fun onTabCounterClicked()
    fun onTabCounterMenuItemTapped(item: TabCounterMenuItem)
    fun onScrolled(offset: Int)
    fun onReaderModePressed(enabled: Boolean)
}

@SuppressWarnings("LargeClass")
class BrowserToolbarView(
    private val container: ViewGroup,
    private val toolbarPosition: ToolbarPosition,
    private val interactor: BrowserToolbarViewInteractor,
    private val customTabSession: Session?,
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

    val view: BrowserToolbar = layout
        .findViewById(R.id.toolbar)

    val toolbarIntegration: ToolbarIntegration

    init {
        val isCustomTabSession = customTabSession != null

        view.display.setOnUrlLongClickListener {
            ToolbarPopupWindow.show(
                WeakReference(view),
                customTabSession,
                interactor::onBrowserToolbarPasteAndGo,
                interactor::onBrowserToolbarPaste
            )
            true
        }

        with(container.context) {
            val sessionManager = components.core.sessionManager

            if (toolbarPosition == ToolbarPosition.TOP) {
                val offsetChangedListener =
                    AppBarLayout.OnOffsetChangedListener { _: AppBarLayout?, verticalOffset: Int ->
                        interactor.onScrolled(verticalOffset)
                    }

                app_bar.addOnOffsetChangedListener(offsetChangedListener)
            }

            view.apply {
                setScrollFlags()

                elevation = TOOLBAR_ELEVATION.dpToFloat(resources.displayMetrics)

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
                    trackingProtection = primaryTextColor
                )

                display.hint = context.getString(R.string.search_hint)
            }

            val menuToolbar: ToolbarMenu
            if (isCustomTabSession) {
                menuToolbar = CustomTabToolbarMenu(
                    this,
                    sessionManager,
                    customTabSession?.id,
                    shouldReverseItems = toolbarPosition == ToolbarPosition.TOP,
                    onItemTapped = {
                        it.performHapticIfNeeded(view)
                        interactor.onBrowserToolbarMenuItemTapped(it)
                    }
                )
            } else {
                menuToolbar = DefaultToolbarMenu(
                    context = this,
                    hasAccountProblem = components.backgroundServices.accountManager.accountNeedsReauth(),
                    shouldReverseItems = toolbarPosition == ToolbarPosition.TOP,
                    onItemTapped = {
                        it.performHapticIfNeeded(view)
                        interactor.onBrowserToolbarMenuItemTapped(it)
                    },
                    lifecycleOwner = lifecycleOwner,
                    sessionManager = sessionManager,
                    store = components.core.store,
                    bookmarksStorage = bookmarkStorage
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
                    isPrivate = customTabSession.private
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
                    isPrivate = sessionManager.selectedSession?.private ?: false,
                    interactor = interactor,
                    engine = components.core.engine
                )
            }
        }
    }

    fun expand() {
        when (settings.toolbarPosition) {
            ToolbarPosition.BOTTOM -> {
                (view.layoutParams as CoordinatorLayout.LayoutParams).apply {
                    (behavior as BrowserToolbarBottomBehavior).forceExpand(view)
                }
            }
            ToolbarPosition.TOP -> {
                layout.app_bar?.setExpanded(true)
            }
        }
    }

    /**
     * Dynamically sets scroll flags for the toolbar when the user does not have a screen reader enabled
     * Note that the bottom toolbar has a feature flag for being dynamic, so it may not get flags set.
     */
    fun setScrollFlags(shouldDisableScroll: Boolean = false) {
        when (settings.toolbarPosition) {
            ToolbarPosition.BOTTOM -> {
                (view.layoutParams as? CoordinatorLayout.LayoutParams)?.apply {
                    behavior = BrowserToolbarBottomBehavior(view.context, null)
                }
            }
            ToolbarPosition.TOP -> {
                view.updateLayoutParams<AppBarLayout.LayoutParams> {
                    scrollFlags = if (settings.shouldUseFixedTopToolbar || shouldDisableScroll) {
                        // Force expand the toolbar so the user is not stuck with a hidden toolbar
                        expand()
                        0
                    } else {
                        SCROLL_FLAG_SCROLL or
                                SCROLL_FLAG_ENTER_ALWAYS or
                                SCROLL_FLAG_SNAP or
                                SCROLL_FLAG_EXIT_UNTIL_COLLAPSED
                    }
                }
            }
        }
    }

    companion object {
        private const val TOOLBAR_ELEVATION = 16
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
