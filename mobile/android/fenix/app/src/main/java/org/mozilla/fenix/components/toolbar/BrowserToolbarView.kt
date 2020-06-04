/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.PopupWindow
import androidx.annotation.LayoutRes
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.content.ContextCompat
import androidx.core.view.isVisible
import androidx.lifecycle.LifecycleOwner
import com.google.android.material.appbar.AppBarLayout
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_ENTER_ALWAYS
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_EXIT_UNTIL_COLLAPSED
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SCROLL
import com.google.android.material.appbar.AppBarLayout.LayoutParams.SCROLL_FLAG_SNAP
import com.google.android.material.snackbar.Snackbar
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.browser_toolbar_popup_window.view.*
import kotlinx.android.synthetic.main.component_browser_top_toolbar.*
import kotlinx.android.synthetic.main.component_browser_top_toolbar.view.*
import mozilla.components.browser.domains.autocomplete.ShippedDomainsProvider
import mozilla.components.browser.session.Session
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.behavior.BrowserToolbarBottomBehavior
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.support.ktx.android.util.dpToFloat
import mozilla.components.support.utils.URLStringUtils
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.customtabs.CustomTabToolbarIntegration
import org.mozilla.fenix.customtabs.CustomTabToolbarMenu
import org.mozilla.fenix.ext.bookmarkStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager

interface BrowserToolbarViewInteractor {
    fun onBrowserToolbarPaste(text: String)
    fun onBrowserToolbarPasteAndGo(text: String)
    fun onBrowserToolbarClicked()
    fun onBrowserToolbarMenuItemTapped(item: ToolbarMenu.Item)
    fun onTabCounterClicked()
    fun onBrowserMenuDismissed(lowPrioHighlightItems: List<ToolbarMenu.Item>)
    fun onScrolled(offset: Int)
    fun onReaderModePressed(enabled: Boolean)
}
@SuppressWarnings("LargeClass")
class BrowserToolbarView(
    private val container: ViewGroup,
    private val shouldUseBottomToolbar: Boolean,
    private val interactor: BrowserToolbarViewInteractor,
    private val customTabSession: Session?,
    private val lifecycleOwner: LifecycleOwner
) : LayoutContainer {

    override val containerView: View?
        get() = container

    private val settings = container.context.settings()

    @LayoutRes
    private val toolbarLayout = when {
        settings.shouldUseBottomToolbar -> R.layout.component_bottom_browser_toolbar
        else -> R.layout.component_browser_top_toolbar
    }

    private val layout = LayoutInflater.from(container.context)
        .inflate(toolbarLayout, container, true)

    val view: BrowserToolbar = layout
        .findViewById(R.id.toolbar)

    val toolbarIntegration: ToolbarIntegration

    init {
        val isCustomTabSession = customTabSession != null

        view.display.setOnUrlLongClickListener {
            val clipboard = view.context.components.clipboardHandler
            val customView = LayoutInflater.from(view.context)
                .inflate(R.layout.browser_toolbar_popup_window, null)
            val popupWindow = PopupWindow(
                customView,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                view.context.resources.getDimensionPixelSize(R.dimen.context_menu_height),
                true
            )

            val selectedSession = container.context.components.core.sessionManager.selectedSession

            popupWindow.elevation =
                view.context.resources.getDimension(R.dimen.mozac_browser_menu_elevation)

            // This is a workaround for SDK<23 to allow popup dismissal on outside or back button press
            // See: https://github.com/mozilla-mobile/fenix/issues/10027
            popupWindow.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))

            customView.paste.isVisible = !clipboard.text.isNullOrEmpty() && !isCustomTabSession
            customView.paste_and_go.isVisible =
                !clipboard.text.isNullOrEmpty() && !isCustomTabSession

            customView.copy.setOnClickListener {
                popupWindow.dismiss()
                if (isCustomTabSession) {
                    clipboard.text = customTabSession?.url
                } else {
                    clipboard.text = selectedSession?.url
                }

                FenixSnackbar.make(
                    view = view,
                    duration = Snackbar.LENGTH_SHORT,
                    isDisplayedWithBrowserToolbar = true
                )
                    .setText(view.context.getString(R.string.browser_toolbar_url_copied_to_clipboard_snackbar))
                    .show()
            }

            customView.paste.setOnClickListener {
                popupWindow.dismiss()
                interactor.onBrowserToolbarPaste(clipboard.text!!)
            }

            customView.paste_and_go.setOnClickListener {
                popupWindow.dismiss()
                interactor.onBrowserToolbarPasteAndGo(clipboard.text!!)
            }

            popupWindow.showAsDropDown(
                view,
                view.context.resources.getDimensionPixelSize(R.dimen.context_menu_x_offset),
                0,
                Gravity.START
            )

            true
        }

        with(container.context) {
            val sessionManager = components.core.sessionManager

            if (!shouldUseBottomToolbar) {
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

                display.progressGravity = if (shouldUseBottomToolbar) {
                    DisplayToolbar.Gravity.TOP
                } else {
                    DisplayToolbar.Gravity.BOTTOM
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
                    shouldReverseItems = !shouldUseBottomToolbar,
                    onItemTapped = {
                        interactor.onBrowserToolbarMenuItemTapped(it)
                    }
                )
            } else {
                menuToolbar = DefaultToolbarMenu(
                    context = this,
                    hasAccountProblem = components.backgroundServices.accountManager.accountNeedsReauth(),
                    shouldReverseItems = !shouldUseBottomToolbar,
                    onItemTapped = { interactor.onBrowserToolbarMenuItemTapped(it) },
                    lifecycleOwner = lifecycleOwner,
                    sessionManager = sessionManager,
                    store = components.core.store,
                    bookmarksStorage = bookmarkStorage
                )
                view.display.setMenuDismissAction {
                    interactor.onBrowserMenuDismissed(menuToolbar.getLowPrioHighlightItems())
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
                    components.core.sessionManager,
                    sessionId = null,
                    isPrivate = sessionManager.selectedSession?.private ?: false,
                    interactor = interactor,
                    engine = components.core.engine
                )
            }
        }
    }

    fun expand() {
        if (settings.shouldUseBottomToolbar && FeatureFlags.dynamicBottomToolbar) {
            (view.layoutParams as CoordinatorLayout.LayoutParams).apply {
                (behavior as BrowserToolbarBottomBehavior).forceExpand(view)
            }
        } else if (!settings.shouldUseBottomToolbar) {
            layout.app_bar?.setExpanded(true)
        }
    }

    /**
     * Dynamically sets scroll flags for the toolbar when the user does not have a screen reader enabled
     * Note that the bottom toolbar has a feature flag for being dynamic, so it may not get flags set.
     */
    fun setScrollFlags(shouldDisableScroll: Boolean = false) {
        if (view.context.settings().shouldUseBottomToolbar) {
            if (FeatureFlags.dynamicBottomToolbar && view.layoutParams is CoordinatorLayout.LayoutParams) {
                (view.layoutParams as CoordinatorLayout.LayoutParams).apply {
                    behavior = BrowserToolbarBottomBehavior(view.context, null)
                }
            }

            return
        }

        val params = view.layoutParams as AppBarLayout.LayoutParams

        params.scrollFlags = when (view.context.settings().shouldUseFixedTopToolbar || shouldDisableScroll) {
            true -> {
                // Force expand the toolbar so the user is not stuck with a hidden toolbar
                expand()
                0
            }
            false -> {
                SCROLL_FLAG_SCROLL or SCROLL_FLAG_ENTER_ALWAYS or SCROLL_FLAG_SNAP or SCROLL_FLAG_EXIT_UNTIL_COLLAPSED
            }
        }

        view.layoutParams = params
    }

    companion object {
        private const val TOOLBAR_ELEVATION = 16
    }
}
