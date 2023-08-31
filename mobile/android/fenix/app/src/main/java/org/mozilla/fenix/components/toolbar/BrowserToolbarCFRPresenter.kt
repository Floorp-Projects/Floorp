/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.clickable
import androidx.compose.material.Text
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.testTag
import androidx.compose.ui.semantics.testTagsAsResourceId
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat.getColor
import androidx.core.view.isVisible
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.flow.transformWhile
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.compose.cfr.CFRPopup
import mozilla.components.compose.cfr.CFRPopup.PopupAlignment.INDICATOR_CENTERED_IN_ANCHOR
import mozilla.components.compose.cfr.CFRPopupProperties
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.SumoTopic.TOTAL_COOKIE_PROTECTION
import org.mozilla.fenix.shopping.DefaultShoppingExperienceFeature
import org.mozilla.fenix.shopping.ShoppingExperienceFeature
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.Settings

/**
 * Vertical padding needed to improve the visual alignment of the popup and respect the UX design.
 */
private const val CFR_TO_ANCHOR_VERTICAL_PADDING = -6

/**
 * The minimum number of opened tabs to show the Total Cookie Protection CFR.
 */
private const val CFR_MINIMUM_NUMBER_OPENED_TABS = 5

/**
 * Delegate for handling all the business logic for showing CFRs in the toolbar.
 *
 * @param context used for various Android interactions.
 * @param browserStore will be observed for tabs updates
 * @param settings used to read and write persistent user settings
 * @param toolbar will serve as anchor for the CFRs
 * @param sessionId optional custom tab id used to identify the custom tab in which to show a CFR.
 * @param shoppingExperienceFeature Used to determine if [ShoppingExperienceFeature] is enabled.
 */
class BrowserToolbarCFRPresenter(
    private val context: Context,
    private val browserStore: BrowserStore,
    private val settings: Settings,
    private val toolbar: BrowserToolbar,
    private val isPrivate: Boolean,
    private val sessionId: String? = null,
    private val shoppingExperienceFeature: ShoppingExperienceFeature = DefaultShoppingExperienceFeature(
        context.settings(),
    ),
) {
    @VisibleForTesting
    internal var scope: CoroutineScope? = null

    @VisibleForTesting
    internal var popup: CFRPopup? = null

    /**
     * Start observing [browserStore] for updates which may trigger showing a CFR.
     */
    @Suppress("MagicNumber")
    fun start() {
        when (getCFRToShow()) {
            ToolbarCFR.TCP -> {
                scope = browserStore.flowScoped { flow ->
                    flow.mapNotNull { it.findCustomTabOrSelectedTab(sessionId)?.content?.progress }
                        // The "transformWhile" below ensures that the 100% progress is only collected once.
                        .transformWhile { progress ->
                            emit(progress)
                            progress != 100
                        }.filter { it == 100 }.collect {
                            scope?.cancel()
                            showTcpCfr()
                        }
                }
            }

            ToolbarCFR.SHOPPING, ToolbarCFR.SHOPPING_OPTED_IN -> {
                scope = browserStore.flowScoped { flow ->
                    flow.mapNotNull { it.selectedTab }
                        .filter { it.isProductUrl && it.content.progress == 100 && !it.content.loading }
                        .distinctUntilChanged()
                        .collect {
                            if (toolbar.findViewById<View>(R.id.mozac_browser_toolbar_page_actions).isVisible) {
                                scope?.cancel()
                                showShoppingCFR(getCFRToShow() == ToolbarCFR.SHOPPING_OPTED_IN)
                            }
                        }
                }
            }

            ToolbarCFR.ERASE -> {
                scope = browserStore.flowScoped { flow ->
                    flow
                        .mapNotNull { it.findCustomTabOrSelectedTab(sessionId) }
                        .filter { it.content.private }
                        .map { it.content.progress }
                        // The "transformWhile" below ensures that the 100% progress is only collected once.
                        .transformWhile { progress ->
                            emit(progress)
                            progress != 100
                        }
                        .filter { it == 100 }
                        .collect {
                            scope?.cancel()
                            showEraseCfr()
                        }
                }
            }

            ToolbarCFR.NONE -> {
                // no-op
            }
        }
    }

    private fun getCFRToShow(): ToolbarCFR = when {
        settings.shouldShowEraseActionCFR && isPrivate -> {
            ToolbarCFR.ERASE
        }

        settings.shouldShowTotalCookieProtectionCFR && (
            !settings.shouldShowCookieBannerReEngagementDialog() ||
                settings.openTabsCount >= CFR_MINIMUM_NUMBER_OPENED_TABS
            ) -> ToolbarCFR.TCP

        shoppingExperienceFeature.isEnabled &&
            settings.shouldShowReviewQualityCheckCFR -> {
            val optInTime = settings.reviewQualityCheckOptInTimeInMillis
            if (optInTime != 0L && System.currentTimeMillis() - optInTime > Settings.ONE_DAY_MS) {
                ToolbarCFR.SHOPPING_OPTED_IN
            } else {
                ToolbarCFR.SHOPPING
            }
        }

        else -> ToolbarCFR.NONE
    }

    /**
     * Stop listening for [browserStore] updates.
     * CFRs already shown are not automatically dismissed.
     */
    fun stop() {
        scope?.cancel()
    }

    @VisibleForTesting
    internal fun showEraseCfr() {
        CFRPopup(
            anchor = toolbar.findViewById(
                R.id.mozac_browser_toolbar_navigation_actions,
            ),
            properties = CFRPopupProperties(
                popupAlignment = INDICATOR_CENTERED_IN_ANCHOR,
                popupBodyColors = listOf(
                    getColor(context, R.color.fx_mobile_layer_color_gradient_end),
                    getColor(context, R.color.fx_mobile_layer_color_gradient_start),
                ),
                popupVerticalOffset = CFR_TO_ANCHOR_VERTICAL_PADDING.dp,
                dismissButtonColor = getColor(context, R.color.fx_mobile_icon_color_oncolor),
                indicatorDirection = if (settings.toolbarPosition == ToolbarPosition.TOP) {
                    CFRPopup.IndicatorDirection.UP
                } else {
                    CFRPopup.IndicatorDirection.DOWN
                },
            ),
            onDismiss = {
                when (it) {
                    true -> TrackingProtection.tcpCfrExplicitDismissal.record(NoExtras())
                    false -> TrackingProtection.tcpCfrImplicitDismissal.record(NoExtras())
                }
            },
            text = {
                FirefoxTheme {
                    Text(
                        text = context.getString(R.string.erase_action_cfr_message),
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        style = FirefoxTheme.typography.body2,
                    )
                }
            },
        ).run {
            settings.shouldShowEraseActionCFR = false
            popup = this
            show()
        }
    }

    @OptIn(ExperimentalComposeUiApi::class)
    @VisibleForTesting
    @Suppress("LongMethod")
    internal fun showTcpCfr() {
        CFRPopup(
            anchor = toolbar.findViewById(
                R.id.mozac_browser_toolbar_security_indicator,
            ),
            properties = CFRPopupProperties(
                popupAlignment = INDICATOR_CENTERED_IN_ANCHOR,
                popupBodyColors = listOf(
                    getColor(context, R.color.fx_mobile_layer_color_gradient_end),
                    getColor(context, R.color.fx_mobile_layer_color_gradient_start),
                ),
                popupVerticalOffset = CFR_TO_ANCHOR_VERTICAL_PADDING.dp,
                dismissButtonColor = getColor(context, R.color.fx_mobile_icon_color_oncolor),
                indicatorDirection = if (settings.toolbarPosition == ToolbarPosition.TOP) {
                    CFRPopup.IndicatorDirection.UP
                } else {
                    CFRPopup.IndicatorDirection.DOWN
                },
            ),
            onDismiss = {
                when (it) {
                    true -> {
                        TrackingProtection.tcpCfrExplicitDismissal.record(NoExtras())
                        settings.shouldShowTotalCookieProtectionCFR = false
                    }
                    false -> TrackingProtection.tcpCfrImplicitDismissal.record(NoExtras())
                }
            },
            text = {
                FirefoxTheme {
                    Text(
                        text = context.getString(R.string.tcp_cfr_message),
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        style = FirefoxTheme.typography.body2,
                        modifier = Modifier
                            .semantics {
                                testTagsAsResourceId = true
                                testTag = "tcp_cfr.message"
                            },
                    )
                }
            },
            action = {
                FirefoxTheme {
                    Text(
                        text = context.getString(R.string.tcp_cfr_learn_more),
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        modifier = Modifier
                            .semantics {
                                testTagsAsResourceId = true
                                testTag = "tcp_cfr.action"
                            }
                            .clickable {
                                context.components.useCases.tabsUseCases.selectOrAddTab.invoke(
                                    SupportUtils.getSumoURLForTopic(
                                        context,
                                        TOTAL_COOKIE_PROTECTION,
                                    ),
                                )
                                TrackingProtection.tcpSumoLinkClicked.record(NoExtras())
                                settings.shouldShowTotalCookieProtectionCFR = false
                                popup?.dismiss()
                            },
                        style = FirefoxTheme.typography.body2.copy(
                            textDecoration = TextDecoration.Underline,
                        ),
                    )
                }
            },
        ).run {
            popup = this
            show()
            TrackingProtection.tcpCfrShown.record(NoExtras())
        }
    }

    @VisibleForTesting
    internal fun showShoppingCFR(shouldShowOptedInCFR: Boolean) {
        CFRPopup(
            anchor = toolbar.findViewById(
                R.id.mozac_browser_toolbar_page_actions,
            ),
            properties = CFRPopupProperties(
                popupWidth = 475.dp,
                popupAlignment = CFRPopup.PopupAlignment.BODY_CENTERED_IN_SCREEN,
                popupBodyColors = listOf(
                    getColor(context, R.color.fx_mobile_layer_color_gradient_start),
                    getColor(context, R.color.fx_mobile_layer_color_gradient_end),
                ),
                popupVerticalOffset = CFR_TO_ANCHOR_VERTICAL_PADDING.dp,
                dismissButtonColor = getColor(context, R.color.fx_mobile_icon_color_oncolor),
                indicatorDirection = if (settings.toolbarPosition == ToolbarPosition.TOP) {
                    CFRPopup.IndicatorDirection.UP
                } else {
                    CFRPopup.IndicatorDirection.DOWN
                },
                dismissOnBackPress = false,
                dismissOnClickOutside = false,
            ),
            onDismiss = {
                when (it) {
                    true -> {
                        settings.shouldShowReviewQualityCheckCFR = false
                    }
                    false -> {}
                }
            },
            text = {
                FirefoxTheme {
                    Text(
                        text = if (shouldShowOptedInCFR) {
                            stringResource(id = R.string.review_quality_check_second_cfr_message)
                        } else {
                            stringResource(id = R.string.review_quality_check_first_cfr_message)
                        },
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        style = FirefoxTheme.typography.body2,
                    )
                }
            },
        ).run {
            popup = this
            show()
        }
    }
}

/**
 * The CFR to be shown in the toolbar.
 */
private enum class ToolbarCFR {
    TCP, SHOPPING, SHOPPING_OPTED_IN, ERASE, NONE
}
