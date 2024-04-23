/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
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
import kotlinx.coroutines.flow.firstOrNull
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
import mozilla.components.concept.engine.EngineSession.CookieBannerHandlingStatus
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.GleanMetrics.CookieBanners
import org.mozilla.fenix.GleanMetrics.Shopping
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
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
internal var CFR_MINIMUM_NUMBER_OPENED_TABS = 5
    @VisibleForTesting
    internal set

/**
 * Delegate for handling all the business logic for showing CFRs in the toolbar.
 *
 * @param context used for various Android interactions.
 * @param browserStore will be observed for tabs updates
 * @param settings used to read and write persistent user settings
 * @param toolbar will serve as anchor for the CFRs
 * @param isPrivate Whether or not the session is private.
 * @param sessionId optional custom tab id used to identify the custom tab in which to show a CFR.
 * @param onShoppingCfrActionClicked Triggered when the user taps on the shopping CFR action.
 * @param onShoppingCfrDisplayed Triggered when CFR is displayed to the user.
 * @param shoppingExperienceFeature Used to determine if [ShoppingExperienceFeature] is enabled.
 */
@Suppress("LongParameterList")
class BrowserToolbarCFRPresenter(
    private val context: Context,
    private val browserStore: BrowserStore,
    private val settings: Settings,
    private val toolbar: BrowserToolbar,
    private val isPrivate: Boolean,
    private val sessionId: String? = null,
    private val onShoppingCfrActionClicked: () -> Unit,
    private val onShoppingCfrDisplayed: () -> Unit,
    private val shoppingExperienceFeature: ShoppingExperienceFeature = DefaultShoppingExperienceFeature(),
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
                        }.filter { popup == null && it == 100 }.collect {
                            scope?.cancel()
                            showTcpCfr()
                        }
                }
            }
            ToolbarCFR.COOKIE_BANNERS -> {
                scope = browserStore.flowScoped { flow ->
                    flow.mapNotNull { it.findCustomTabOrSelectedTab(sessionId) }
                        .ifAnyChanged { tab ->
                            arrayOf(
                                tab.cookieBanner,
                            )
                        }
                        .filter {
                            it.content.private && it.cookieBanner == CookieBannerHandlingStatus.HANDLED
                        }
                        .collect {
                            scope?.cancel()
                            settings.shouldShowCookieBannersCFR = false
                            showCookieBannersCFR()
                        }
                }
            }

            ToolbarCFR.SHOPPING, ToolbarCFR.SHOPPING_OPTED_IN -> {
                scope = browserStore.flowScoped { flow ->
                    val shouldShowCfr: Boolean? = flow.mapNotNull { it.selectedTab }
                        .filter { it.content.isProductUrl && it.content.progress == 100 && !it.content.loading }
                        .distinctUntilChanged()
                        .map { toolbar.findViewById<View>(R.id.mozac_browser_toolbar_page_actions).isVisible }
                        .filter { popup == null && it }
                        .firstOrNull()

                    if (shouldShowCfr == true) {
                        showShoppingCFR(getCFRToShow() == ToolbarCFR.SHOPPING_OPTED_IN)
                    }

                    scope?.cancel()
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
                        .filter { popup == null && it == 100 }
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

    /**
     * Decides which Shopping CFR needs to be displayed depending on
     * participation of user in Review Checker and time elapsed
     * from last CFR display.
     */
    private fun whichShoppingCFR(): ToolbarCFR {
        fun Long.isInitialized(): Boolean = this != 0L
        fun Long.afterTwelveHours(): Boolean = this.isInitialized() &&
            System.currentTimeMillis() - this > Settings.TWELVE_HOURS_MS

        val optInTime = settings.reviewQualityCheckOptInTimeInMillis
        val firstCfrShownTime = settings.reviewQualityCheckCfrDisplayTimeInMillis

        return when {
            // Try Review Checker CFR should be displayed on first product page visit
            !firstCfrShownTime.isInitialized() ->
                ToolbarCFR.SHOPPING
            // Try Review Checker CFR should be displayed again 12 hours later only for not opted in users
            !optInTime.isInitialized() && firstCfrShownTime.afterTwelveHours() ->
                ToolbarCFR.SHOPPING
            // Already Opted In CFR should be shown 12 hours after opt in
            optInTime.afterTwelveHours() ->
                ToolbarCFR.SHOPPING_OPTED_IN
            else -> {
                ToolbarCFR.NONE
            }
        }
    }

    private fun getCFRToShow(): ToolbarCFR = when {
        settings.shouldShowEraseActionCFR && isPrivate -> {
            ToolbarCFR.ERASE
        }

        settings.shouldShowTotalCookieProtectionCFR && (
            settings.openTabsCount >= CFR_MINIMUM_NUMBER_OPENED_TABS
            ) -> ToolbarCFR.TCP

        isPrivate && settings.shouldShowCookieBannersCFR && settings.shouldUseCookieBannerPrivateMode -> {
            ToolbarCFR.COOKIE_BANNERS
        }

        shoppingExperienceFeature.isEnabled &&
            settings.shouldShowReviewQualityCheckCFR -> whichShoppingCFR()

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
    @Suppress("LongMethod")
    internal fun showCookieBannersCFR() {
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
                CookieBanners.cfrDismissal.record(NoExtras())
            },
            text = {
                FirefoxTheme {
                    Column {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Icon(
                                painter = painterResource(id = R.drawable.ic_cookies_disabled),
                                contentDescription = null,
                                tint = FirefoxTheme.colors.iconPrimary,
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Text(
                                text = context.getString(
                                    R.string.cookie_banner_cfr_title,
                                    context.getString(R.string.firefox),
                                ),
                                color = FirefoxTheme.colors.textOnColorPrimary,
                                style = FirefoxTheme.typography.subtitle2,
                            )
                        }
                        Text(
                            text = context.getString(R.string.cookie_banner_cfr_message),
                            color = FirefoxTheme.colors.textOnColorPrimary,
                            style = FirefoxTheme.typography.body2,
                            modifier = Modifier.padding(top = 2.dp),
                        )
                    }
                }
            },
        ).run {
            popup = this
            show()
            CookieBanners.cfrShown.record(NoExtras())
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
                dismissOnBackPress = true,
                dismissOnClickOutside = true,
            ),
            onDismiss = {},
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
            action = {
                FirefoxTheme {
                    Text(
                        text = if (shouldShowOptedInCFR) {
                            stringResource(id = R.string.review_quality_check_second_cfr_action)
                        } else {
                            stringResource(id = R.string.review_quality_check_first_cfr_action)
                        },
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        modifier = Modifier
                            .clickable {
                                onShoppingCfrActionClicked()
                                popup?.dismiss()
                            },
                        style = FirefoxTheme.typography.body2.copy(
                            textDecoration = TextDecoration.Underline,
                        ),
                    )
                }
            },
        ).run {
            Shopping.addressBarFeatureCalloutDisplayed.record()
            popup = this
            onShoppingCfrDisplayed()
            show()
        }
    }
}

/**
 * The CFR to be shown in the toolbar.
 */
private enum class ToolbarCFR {
    TCP, SHOPPING, SHOPPING_OPTED_IN, ERASE, COOKIE_BANNERS, NONE
}
