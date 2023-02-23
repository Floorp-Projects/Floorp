/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.compose.foundation.clickable
import androidx.compose.material.Text
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat.getColor
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.mapNotNull
import kotlinx.coroutines.flow.transformWhile
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
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
import org.mozilla.fenix.settings.SupportUtils
import org.mozilla.fenix.settings.SupportUtils.SumoTopic.TOTAL_COOKIE_PROTECTION
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.utils.Settings

/**
 * Vertical padding needed to improve the visual alignment of the popup and respect the UX design.
 */
private const val CFR_TO_ANCHOR_VERTICAL_PADDING = -6

/**
 * The minimum number of opened tabs to show the CFR.
 */
private const val CRF_MINIMUM_NUMBER_OPENED_TABS = 5

/**
 * Delegate for handling all the business logic for showing CFRs in the toolbar.
 *
 * @param context used for various Android interactions.
 * @param browserStore will be observed for tabs updates
 * @param settings used to read and write persistent user settings
 * @param toolbar will serve as anchor for the CFRs
 * @param sessionId optional custom tab id used to identify the custom tab in which to show a CFR.
 */
class BrowserToolbarCFRPresenter(
    private val context: Context,
    private val browserStore: BrowserStore,
    private val settings: Settings,
    private val toolbar: BrowserToolbar,
    private val sessionId: String? = null,
) {
    @VisibleForTesting
    internal var tcpCfrScope: CoroutineScope? = null

    @VisibleForTesting
    internal var tcpCfrPopup: CFRPopup? = null

    /**
     * Start observing [browserStore] for updates which may trigger showing a CFR.
     */
    @Suppress("MagicNumber")
    fun start() {
        if (shouldShowCFR()) {
            tcpCfrScope = browserStore.flowScoped { flow ->
                flow
                    .mapNotNull { it.findCustomTabOrSelectedTab(sessionId)?.content?.progress }
                    // The "transformWhile" below ensures that the 100% progress is only collected once.
                    .transformWhile { progress ->
                        emit(progress)
                        progress != 100
                    }
                    .filter { it == 100 }
                    .collect {
                        tcpCfrScope?.cancel()
                        showTcpCfr()
                    }
            }
        }
    }

    private fun shouldShowCFR(): Boolean {
        return settings.shouldShowTotalCookieProtectionCFR && (
            !settings.shouldShowCookieBannerReEngagementDialog() ||
                settings.openTabsCount >= CRF_MINIMUM_NUMBER_OPENED_TABS
            )
    }

    /**
     * Stop listening for [browserStore] updates.
     * CFRs already shown are not automatically dismissed.
     */
    fun stop() {
        tcpCfrScope?.cancel()
    }

    @VisibleForTesting
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
                    true -> TrackingProtection.tcpCfrExplicitDismissal.record(NoExtras())
                    false -> TrackingProtection.tcpCfrImplicitDismissal.record(NoExtras())
                }
            },
            text = {
                FirefoxTheme {
                    Text(
                        text = context.getString(R.string.tcp_cfr_message),
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        style = FirefoxTheme.typography.body2,
                    )
                }
            },
            action = {
                FirefoxTheme {
                    Text(
                        text = context.getString(R.string.tcp_cfr_learn_more),
                        color = FirefoxTheme.colors.textOnColorPrimary,
                        modifier = Modifier.clickable {
                            context.components.useCases.tabsUseCases.selectOrAddTab.invoke(
                                SupportUtils.getSumoURLForTopic(
                                    context,
                                    TOTAL_COOKIE_PROTECTION,
                                ),
                            )
                            TrackingProtection.tcpSumoLinkClicked.record(NoExtras())
                            tcpCfrPopup?.dismiss()
                        },
                        style = FirefoxTheme.typography.body2.copy(
                            textDecoration = TextDecoration.Underline,
                        ),
                    )
                }
            },
        ).run {
            settings.shouldShowTotalCookieProtectionCFR = false
            tcpCfrPopup = this
            show()
            TrackingProtection.tcpCfrShown.record(NoExtras())
        }
    }
}
