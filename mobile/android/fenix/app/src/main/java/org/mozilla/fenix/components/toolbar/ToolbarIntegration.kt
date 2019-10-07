/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.appcompat.content.res.AppCompatResources
import com.airbnb.lottie.LottieCompositionFactory
import com.airbnb.lottie.LottieDrawable
import mozilla.components.browser.domains.autocomplete.DomainAutocompleteProvider
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.runWithSession
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.feature.toolbar.ToolbarAutocompleteFeature
import mozilla.components.feature.toolbar.ToolbarFeature
import mozilla.components.feature.toolbar.ToolbarPresenter
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.theme.ThemeManager

class ToolbarIntegration(
    context: Context,
    toolbar: BrowserToolbar,
    toolbarMenu: ToolbarMenu,
    domainAutocompleteProvider: DomainAutocompleteProvider,
    historyStorage: HistoryStorage,
    sessionManager: SessionManager,
    sessionId: String? = null,
    isPrivate: Boolean,
    interactor: BrowserToolbarViewInteractor
) : LifecycleAwareFeature {

    private var renderStyle: ToolbarFeature.RenderStyle = ToolbarFeature.RenderStyle.UncoloredUrl

    init {
        toolbar.setMenuBuilder(toolbarMenu.menuBuilder)
        toolbar.private = isPrivate

        run {
            sessionManager.runWithSession(sessionId) {
                it.isCustomTabSession()
            }.also { isCustomTab ->
                if (isCustomTab) {
                    renderStyle = ToolbarFeature.RenderStyle.RegistrableDomain
                    return@run
                }

                val task = LottieCompositionFactory
                    .fromRawRes(
                        context,
                        ThemeManager.resolveAttribute(R.attr.shieldLottieFile, context)
                    )
                task.addListener { result ->
                    val lottieDrawable = LottieDrawable()
                    lottieDrawable.composition = result
                    toolbar.displayTrackingProtectionIcon =
                        context.settings().shouldUseTrackingProtection && FeatureFlags.etpCategories
                    toolbar.displaySeparatorView =
                        context.settings().shouldUseTrackingProtection && FeatureFlags.etpCategories

                    toolbar.setTrackingProtectionIcons(
                        iconOnNoTrackersBlocked = AppCompatResources.getDrawable(
                            context,
                            R.drawable.ic_tracking_protection_enabled
                        )!!,
                        iconOnTrackersBlocked = lottieDrawable,
                        iconDisabledForSite = AppCompatResources.getDrawable(
                            context,
                            R.drawable.ic_tracking_protection_disabled
                        )!!
                    )
                }

                val tabsAction = TabCounterToolbarButton(
                    sessionManager,
                    {
                        toolbar.hideKeyboard()
                        interactor.onTabCounterClicked()
                    },
                    isPrivate
                )
                toolbar.addBrowserAction(tabsAction)
            }
        }

        ToolbarAutocompleteFeature(toolbar).apply {
            addDomainProvider(domainAutocompleteProvider)
            if (context.settings().shouldShowHistorySuggestions) {
                addHistoryStorageProvider(historyStorage)
            }
        }
    }

    private val toolbarPresenter: ToolbarPresenter = ToolbarPresenter(
        toolbar,
        context.components.core.store,
        sessionId,
        ToolbarFeature.UrlRenderConfiguration(
            PublicSuffixList(context),
            ThemeManager.resolveAttribute(R.attr.primaryText, context), renderStyle = renderStyle
        )
    )
    private var menuPresenter =
        MenuPresenter(toolbar, context.components.core.sessionManager, sessionId)

    override fun start() {
        menuPresenter.start()
        toolbarPresenter.start()
    }

    override fun stop() {
        menuPresenter.stop()
        toolbarPresenter.stop()
    }
}
