/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.feature.tabs.toolbar.TabCounterToolbarButton
import mozilla.components.feature.toolbar.ToolbarBehaviorController
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.feature.toolbar.ToolbarPresenter
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.components.toolbar.interactor.BrowserToolbarInteractor
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager

/**
 * Feature configuring the toolbar when in display mode.
 */
abstract class ToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    toolbarMenu: ToolbarMenu,
    sessionId: String?,
    isPrivate: Boolean,
    renderStyle: ToolbarFeature.RenderStyle,
) : LifecycleAwareFeature {

    val store = context.components.core.store
    private val toolbarPresenter: ToolbarPresenter = ToolbarPresenter(
        toolbar = toolbar,
        store = store,
        customTabId = sessionId,
        shouldDisplaySearchTerms = true,
        urlRenderConfiguration = ToolbarFeature.UrlRenderConfiguration(
            context.components.publicSuffixList,
            ThemeManager.resolveAttribute(R.attr.textPrimary, context),
            renderStyle = renderStyle,
        ),
    )

    private val menuPresenter =
        MenuPresenter(toolbar, context.components.core.store, sessionId)

    private val toolbarController = ToolbarBehaviorController(toolbar, store, sessionId)

    init {
        toolbar.display.menuBuilder = toolbarMenu.menuBuilder
        toolbar.private = isPrivate
    }

    override fun start() {
        menuPresenter.start()
        toolbarPresenter.start()
        toolbarController.start()
    }

    override fun stop() {
        menuPresenter.stop()
        toolbarPresenter.stop()
        toolbarController.stop()
    }

    fun invalidateMenu() {
        menuPresenter.invalidateActions()
    }
}

class DefaultToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    toolbarMenu: ToolbarMenu,
    lifecycleOwner: LifecycleOwner,
    sessionId: String? = null,
    isPrivate: Boolean,
    interactor: BrowserToolbarInteractor,
) : ToolbarIntegration(
    context = context,
    toolbar = toolbar,
    toolbarMenu = toolbarMenu,
    sessionId = sessionId,
    isPrivate = isPrivate,
    renderStyle = ToolbarFeature.RenderStyle.UncoloredUrl,
) {

    @VisibleForTesting
    internal var cfrPresenter = BrowserToolbarCFRPresenter(
        context = context,
        browserStore = context.components.core.store,
        settings = context.settings(),
        toolbar = toolbar,
        isPrivate = isPrivate,
        sessionId = sessionId,
        onShoppingCfrActionClicked = interactor::onShoppingCfrActionClicked,
        onShoppingCfrDisplayed = interactor::onShoppingCfrDisplayed,
    )

    init {
        toolbar.display.menuBuilder = toolbarMenu.menuBuilder
        toolbar.private = isPrivate

        toolbar.display.indicators = listOf(
            DisplayToolbar.Indicators.SECURITY,
            DisplayToolbar.Indicators.EMPTY,
            DisplayToolbar.Indicators.HIGHLIGHT,
        )

        val tabCounterMenu = FenixTabCounterMenu(
            context = context,
            onItemTapped = {
                interactor.onTabCounterMenuItemTapped(it)
            },
            iconColor = if (isPrivate) {
                ContextCompat.getColor(context, R.color.fx_mobile_private_text_color_primary)
            } else {
                null
            },
        ).also {
            it.updateMenu(context.settings().toolbarPosition)
        }

        val tabsAction = TabCounterToolbarButton(
            lifecycleOwner = lifecycleOwner,
            showTabs = {
                toolbar.hideKeyboard()
                interactor.onTabCounterClicked()
            },
            store = store,
            menu = tabCounterMenu,
            showMaskInPrivateMode = context.settings().feltPrivateBrowsingEnabled,
        )

        val tabCount = if (isPrivate) {
            store.state.privateTabs.size
        } else {
            store.state.normalTabs.size
        }

        tabsAction.updateCount(tabCount)

        toolbar.addBrowserAction(tabsAction)
    }

    override fun start() {
        super.start()
        cfrPresenter.start()
    }

    override fun stop() {
        cfrPresenter.stop()
        super.stop()
    }
}
