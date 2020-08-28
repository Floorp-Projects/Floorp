/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.appcompat.content.res.AppCompatResources
import androidx.lifecycle.LifecycleOwner
import com.airbnb.lottie.LottieCompositionFactory
import com.airbnb.lottie.LottieDrawable
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.feature.toolbar.ToolbarPresenter
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager

abstract class ToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    toolbarMenu: ToolbarMenu,
    sessionId: String?,
    isPrivate: Boolean,
    renderStyle: ToolbarFeature.RenderStyle
) : LifecycleAwareFeature {

    private val toolbarPresenter: ToolbarPresenter = ToolbarPresenter(
        toolbar,
        context.components.core.store,
        sessionId,
        ToolbarFeature.UrlRenderConfiguration(
            PublicSuffixList(context),
            ThemeManager.resolveAttribute(R.attr.primaryText, context),
            renderStyle = renderStyle
        )
    )

    private val menuPresenter =
        MenuPresenter(toolbar, context.components.core.sessionManager, sessionId)

    init {
        toolbar.display.menuBuilder = toolbarMenu.menuBuilder
        toolbar.private = isPrivate
    }

    override fun start() {
        menuPresenter.start()
        toolbarPresenter.start()
    }

    override fun stop() {
        menuPresenter.stop()
        toolbarPresenter.stop()
    }

    fun invalidateMenu() {
        menuPresenter.invalidateActions()
    }
}

class DefaultToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    toolbarMenu: ToolbarMenu,
    domainAutocompleteProvider: DomainAutocompleteProvider,
    historyStorage: HistoryStorage,
    lifecycleOwner: LifecycleOwner,
    sessionId: String? = null,
    isPrivate: Boolean,
    interactor: BrowserToolbarViewInteractor,
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

        val task = LottieCompositionFactory
            .fromRawRes(
                context,
                ThemeManager.resolveAttribute(R.attr.shieldLottieFile, context)
            )
        task.addListener { result ->
            val lottieDrawable = LottieDrawable()
            lottieDrawable.composition = result

            toolbar.display.indicators =
                if (context.settings().shouldUseTrackingProtection) {
                    listOf(
                        DisplayToolbar.Indicators.TRACKING_PROTECTION,
                        DisplayToolbar.Indicators.SECURITY,
                        DisplayToolbar.Indicators.EMPTY
                    )
                } else {
                    listOf(
                        DisplayToolbar.Indicators.SECURITY,
                        DisplayToolbar.Indicators.EMPTY
                    )
                }

            toolbar.display.displayIndicatorSeparator =
                context.settings().shouldUseTrackingProtection

            toolbar.display.icons = toolbar.display.icons.copy(
                emptyIcon = null,
                trackingProtectionTrackersBlocked = lottieDrawable,
                trackingProtectionNothingBlocked = AppCompatResources.getDrawable(
                    context,
                    R.drawable.ic_tracking_protection_enabled
                )!!,
                trackingProtectionException = AppCompatResources.getDrawable(
                    context,
                    R.drawable.ic_tracking_protection_disabled
                )!!
            )
        }

        val tabsAction = TabCounterToolbarButton(
            lifecycleOwner,
            isPrivate,
            onItemTapped = {
                interactor.onTabCounterMenuItemTapped(it)
            },
            showTabs = {
                toolbar.hideKeyboard()
                interactor.onTabCounterClicked()
            }
        )
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
