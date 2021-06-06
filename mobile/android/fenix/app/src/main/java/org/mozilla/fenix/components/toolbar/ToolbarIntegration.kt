/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.content.res.Configuration
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.tabs.toolbar.TabCounterToolbarButton
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
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

@ExperimentalCoroutinesApi
abstract class ToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    toolbarMenu: ToolbarMenu,
    sessionId: String?,
    isPrivate: Boolean,
    renderStyle: ToolbarFeature.RenderStyle
) : LifecycleAwareFeature {

    val store = context.components.core.store
    private val toolbarPresenter: ToolbarPresenter = ToolbarPresenter(
        toolbar,
        store,
        sessionId,
        ToolbarFeature.UrlRenderConfiguration(
            context.components.publicSuffixList,
            ThemeManager.resolveAttribute(R.attr.primaryText, context),
            renderStyle = renderStyle
        )
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

@ExperimentalCoroutinesApi
class DefaultToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    toolbarMenu: ToolbarMenu,
    domainAutocompleteProvider: DomainAutocompleteProvider,
    historyStorage: HistoryStorage,
    lifecycleOwner: LifecycleOwner,
    sessionId: String? = null,
    isPrivate: Boolean,
    interactor: BrowserToolbarInteractor,
    engine: Engine
) : ToolbarIntegration(
    context = context,
    toolbar = toolbar,
    toolbarMenu = toolbarMenu,
    sessionId = sessionId,
    isPrivate = isPrivate,
    renderStyle = ToolbarFeature.RenderStyle.UncoloredUrl
) {

    init {
        toolbar.display.menuBuilder = toolbarMenu.menuBuilder
        toolbar.private = isPrivate

        val drawable =
            if (isPrivate) AppCompatResources.getDrawable(
                context,
                R.drawable.shield_dark
            ) else when (context.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) {
                Configuration.UI_MODE_NIGHT_UNDEFINED, // We assume light here per Android doc's recommendation
                Configuration.UI_MODE_NIGHT_NO -> {
                    AppCompatResources.getDrawable(context, R.drawable.shield_light)
                }
                Configuration.UI_MODE_NIGHT_YES -> {
                    AppCompatResources.getDrawable(context, R.drawable.shield_dark)
                }
                else -> AppCompatResources.getDrawable(context, R.drawable.shield_light)
            }

        toolbar.display.indicators =
            if (context.settings().shouldUseTrackingProtection) {
                listOf(
                    DisplayToolbar.Indicators.TRACKING_PROTECTION,
                    DisplayToolbar.Indicators.SECURITY,
                    DisplayToolbar.Indicators.EMPTY,
                    DisplayToolbar.Indicators.HIGHLIGHT
                )
            } else {
                listOf(
                    DisplayToolbar.Indicators.SECURITY,
                    DisplayToolbar.Indicators.EMPTY,
                    DisplayToolbar.Indicators.HIGHLIGHT
                )
            }
            context.settings().shouldUseTrackingProtection

        toolbar.display.icons = toolbar.display.icons.copy(
            emptyIcon = null,
            trackingProtectionTrackersBlocked = drawable!!,
            trackingProtectionNothingBlocked = AppCompatResources.getDrawable(
                context,
                R.drawable.ic_tracking_protection_enabled
            )!!,
            trackingProtectionException = AppCompatResources.getDrawable(
                context,
                R.drawable.ic_tracking_protection_disabled
            )!!
        )

        val tabCounterMenu = FenixTabCounterMenu(
            context = context,
            onItemTapped = {
                interactor.onTabCounterMenuItemTapped(it)
            },
            iconColor =
                if (isPrivate) {
                    ContextCompat.getColor(context, R.color.primary_text_private_theme)
                } else {
                    null
                }
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
            menu = tabCounterMenu
        )

        val tabCount = if (isPrivate) {
            store.state.privateTabs.size
        } else {
            store.state.normalTabs.size
        }

        tabsAction.updateCount(tabCount)

        toolbar.addBrowserAction(tabsAction)

        val engineForSpeculativeConnects = if (!isPrivate) engine else null
        ToolbarAutocompleteFeature(
            toolbar,
            engineForSpeculativeConnects
        ).apply {
            addDomainProvider(domainAutocompleteProvider)
            if (context.settings().shouldShowHistorySuggestions) {
                addHistoryStorageProvider(historyStorage)
            }
        }
    }
}
