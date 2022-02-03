/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import android.graphics.Color
import androidx.annotation.VisibleForTesting
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.mapNotNull
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.browser.toolbar.display.DisplayToolbar.Indicators
import mozilla.components.feature.customtabs.CustomTabsToolbarFeature
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.tabs.CustomTabsUseCases
import mozilla.components.feature.tabs.toolbar.TabCounterToolbarButton
import mozilla.components.feature.toolbar.ToolbarBehaviorController
import mozilla.components.feature.toolbar.ToolbarPresenter
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import mozilla.components.support.ktx.android.view.hideKeyboard
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.focus.GleanMetrics.TabCount
import org.mozilla.focus.GleanMetrics.TrackingProtection
import org.mozilla.focus.R
import org.mozilla.focus.ext.isCustomTab
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.menu.browser.CustomTabMenu
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.HardwareUtils

@Suppress("LongParameterList")
class BrowserToolbarIntegration(
    private val store: BrowserStore,
    private val toolbar: BrowserToolbar,
    private val fragment: BrowserFragment,
    controller: BrowserMenuController,
    sessionUseCases: SessionUseCases,
    customTabsUseCases: CustomTabsUseCases,
    private val onUrlLongClicked: () -> Boolean,
    private val eraseActionListener: () -> Unit,
    private val tabCounterListener: () -> Unit,
    private val onTrackingProtectionShown: () -> Unit,
    private val customTabId: String? = null,
    inTesting: Boolean = false
) : LifecycleAwareFeature {
    private val presenter = ToolbarPresenter(
        toolbar,
        store,
        customTabId
    )

    @VisibleForTesting
    internal var securityIndicatorScope: CoroutineScope? = null
    private var tabsCounterScope: CoroutineScope? = null
    private var customTabsFeature: CustomTabsToolbarFeature? = null
    private var navigationButtonsIntegration: NavigationButtonsIntegration? = null
    private val eraseAction = BrowserToolbar.Button(
        imageDrawable = AppCompatResources.getDrawable(
            toolbar.context,
            R.drawable.mozac_ic_delete
        )!!,
        contentDescription = toolbar.context.getString(R.string.content_description_erase),
        iconTintColorResource = R.color.primaryText,
        listener = {
            val openedTabs = store.state.tabs.size
            TabCount.eraseButtonTapped.record(TabCount.EraseButtonTappedExtra(openedTabs))

            TelemetryWrapper.eraseEvent()

            eraseActionListener.invoke()
        }
    )
    private val tabsAction = TabCounterToolbarButton(
        lifecycleOwner = fragment,
        showTabs = {
            toolbar.hideKeyboard()
            tabCounterListener.invoke()
        },
        store = store
    )

    @VisibleForTesting
    internal var toolbarController = ToolbarBehaviorController(toolbar, store, customTabId)

    init {
        val context = toolbar.context

        toolbar.display.apply {
            colors = colors.copy(
                hint = ContextCompat.getColor(toolbar.context, R.color.urlBarHintText),
                securityIconInsecure = Color.TRANSPARENT,
                text = ContextCompat.getColor(toolbar.context, R.color.primaryText),
                menu = ContextCompat.getColor(toolbar.context, R.color.primaryText)
            )

            addTrackingProtectionIndicator()

            displayIndicatorSeparator = false

            setOnSiteSecurityClickedListener {
                TrackingProtection.toolbarShieldClicked.add()
                fragment.showTrackingProtectionPanel()
            }

            onUrlClicked = {
                fragment.edit()
                false // Do not switch to edit mode
            }

            setOnUrlLongClickListener { onUrlLongClicked() }

            icons = icons.copy(
                trackingProtectionTrackersBlocked = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_shield
                )!!,
                trackingProtectionNothingBlocked = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_shield
                )!!,
                trackingProtectionException = AppCompatResources.getDrawable(
                    context,
                    R.drawable.mozac_ic_shield_disabled
                )!!
            )
        }

        toolbar.display.setOnTrackingProtectionClickedListener {
            TrackingProtection.toolbarShieldClicked.add()
            fragment.showTrackingProtectionPanel()
        }

        if (customTabId != null) {
            val menu = CustomTabMenu(
                context = fragment.requireContext(),
                store = store,
                currentTabId = customTabId,
                onItemTapped = { controller.handleMenuInteraction(it) }
            )
            customTabsFeature = CustomTabsToolbarFeature(
                store,
                toolbar,
                sessionId = customTabId,
                useCases = customTabsUseCases,
                menuBuilder = menu.menuBuilder,
                menuItemIndex = menu.menuBuilder.items.size - 1,
                closeListener = { fragment.closeCustomTab() }
            )
        }

        if (HardwareUtils.isTablet(context)) {
            navigationButtonsIntegration = NavigationButtonsIntegration(
                context,
                store,
                toolbar,
                sessionUseCases,
                customTabId
            )
        }
        if (store.state.findCustomTabOrSelectedTab(customTabId)?.isCustomTab() == false) {
            toolbar.addNavigationAction(eraseAction)
            if (!inTesting) {
                setUrlBackground()
            }
        }
    }

    // Use the same background for display/edit modes.
    private fun setUrlBackground() {
        val urlBackground = ResourcesCompat.getDrawable(
            fragment.resources,
            R.drawable.toolbar_url_background,
            fragment.context?.theme
        )
        toolbar.display.setUrlBackground(urlBackground)
    }

    private fun setBrowserActionButtons() {
        tabsCounterScope = store.flowScoped { flow ->
            flow.ifChanged { state -> state.tabs.size > 1 }
                .collect { state ->
                    if (state.tabs.size > 1) {
                        toolbar.addBrowserAction(tabsAction)
                    } else {
                        toolbar.removeBrowserAction(tabsAction)
                    }
                }
        }
    }

    override fun start() {
        presenter.start()
        toolbarController.start()
        customTabsFeature?.start()
        navigationButtonsIntegration?.start()
        observerSecurityIndicatorChanges()
        if (store.state.findCustomTabOrSelectedTab(customTabId)?.isCustomTab() == false) {
            setBrowserActionButtons()
        }
    }

    @VisibleForTesting
    internal fun observerSecurityIndicatorChanges() {
        securityIndicatorScope = store.flowScoped { flow ->
            flow.mapNotNull { state -> state.findCustomTabOrSelectedTab(customTabId) }
                .ifChanged { tab -> tab.content.securityInfo }
                .collect {
                    val secure = it.content.securityInfo.secure
                    val url = it.content.url
                    if (secure && Indicators.SECURITY in toolbar.display.indicators) {
                        addTrackingProtectionIndicator()
                        onTrackingProtectionShown()
                    } else if (!secure && Indicators.SECURITY !in toolbar.display.indicators &&
                        !url.trim().startsWith("about:")
                    ) {
                        addSecurityIndicator()
                    }
                }
        }
    }

    override fun stop() {
        presenter.stop()

        toolbarController.stop()
        customTabsFeature?.stop()
        navigationButtonsIntegration?.stop()
        stopObserverSecurityIndicatorChanges()
        toolbar.removeBrowserAction(tabsAction)
        tabsCounterScope?.cancel()
    }

    @VisibleForTesting
    internal fun stopObserverSecurityIndicatorChanges() {
        securityIndicatorScope?.cancel()
    }

    @VisibleForTesting
    internal fun addSecurityIndicator() {
        toolbar.display.indicators = listOf(Indicators.SECURITY)
    }

    @VisibleForTesting
    internal fun addTrackingProtectionIndicator() {
        toolbar.display.indicators = listOf(Indicators.TRACKING_PROTECTION)
    }
}
