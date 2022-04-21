/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.tips

import android.content.Context
import androidx.annotation.StringRes
import mozilla.components.browser.state.state.SessionState
import org.mozilla.focus.GleanMetrics.ProTips
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.ext.settings
import org.mozilla.focus.utils.AppConstants
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.SupportUtils.getSumoURLForTopic

class Tip(
    val tipId: String,
    @StringRes
    val stringId: Int,
    val appName: String? = null,
    val shouldDisplay: () -> Boolean,
    val deepLink: (() -> Unit)? = null
) {
    companion object {

        fun createAllowlistTip(context: Context, tipId: String): Tip {
            val stringId = R.string.tip_explain_allowlist3
            val url = getSumoURLForTopic(context, SupportUtils.SumoTopic.ALLOWLIST)

            val deepLink = {
                context.components.tabsUseCases.addTab(
                    url,
                    source = SessionState.Source.Internal.HomeScreen,
                    selectTab = true,
                    private = true
                )

                ProTips.linkInTipClicked.record(ProTips.LinkInTipClickedExtra(tipId = tipId))
            }

            val shouldDisplayAllowListTip = {
                // Since the refactoring from a custom exception list to using the
                // exceotion list in Gecko, this method always returns false. To
                // determine whether we would like to show it, we'd need to query
                // Gecko asynchronously. But since the TipManager calls all those
                // methods synchronously, this is not really possible wihtout
                // refactoring TipManager. At this time it is easier to just not
                // show this tip.
                false
            }

            return Tip(tipId, stringId, null, shouldDisplayAllowListTip, deepLink)
        }

        fun createFreshLookTip(context: Context, tipId: String): Tip {
            val stringId = R.string.tip_fresh_look
            val name = context.getString(R.string.app_name)

            val sumoTopic = if (AppConstants.isKlarBuild) {
                SupportUtils.SumoTopic.WHATS_NEW_KLAR
            } else {
                SupportUtils.SumoTopic.WHATS_NEW_FOCUS
            }

            val whatsNewUrl = getSumoURLForTopic(context, sumoTopic)
            val deepLink = {
                context.components.tabsUseCases.addTab(
                    url = whatsNewUrl,
                    source = SessionState.Source.Internal.HomeScreen,
                    selectTab = true,
                    private = true
                )
                ProTips.linkInTipClicked.record(ProTips.LinkInTipClickedExtra(tipId = tipId))
            }
            val shouldDisplayFreshLookTip = {
                context.settings.getAppLaunchCount() == 0
            }

            return Tip(tipId, stringId, name, shouldDisplayFreshLookTip, deepLink)
        }

        fun createShortcutsTip(context: Context, tipId: String): Tip {
            val stringId = R.string.tip_about_shortcuts
            val name = context.getString(R.string.app_name)

            val shouldDisplayShortcutsTip = { true }

            return Tip(tipId, stringId, name, shouldDisplayShortcutsTip)
        }

        fun createTrackingProtectionTip(context: Context, tipId: String): Tip {
            val stringId = R.string.tip_disable_tracking_protection3

            val settings = context.settings

            val shouldDisplayTrackingProtection = {
                settings.shouldBlockOtherTrackers() ||
                    settings.shouldBlockAdTrackers() ||
                    settings.shouldBlockAnalyticTrackers()
            }

            return Tip(tipId, stringId, null, shouldDisplayTrackingProtection)
        }

        fun createRequestDesktopTip(context: Context, tipId: String): Tip {
            val stringId = R.string.tip_request_desktop2
            val name = context.getString(R.string.app_name)
            val requestDesktopURL =
                "https://support.mozilla.org/kb/switch-desktop-view-firefox-focus-android"

            val shouldDisplayRequestDesktop = {
                !context.settings.hasRequestedDesktop()
            }

            val deepLinkRequestDesktop = {
                context.components.tabsUseCases.addTab(
                    requestDesktopURL,
                    source = SessionState.Source.Internal.HomeScreen,
                    selectTab = true,
                    private = true
                )
                ProTips.linkInTipClicked.record(ProTips.LinkInTipClickedExtra(tipId = tipId))
            }

            return Tip(tipId, stringId, name, shouldDisplayRequestDesktop, deepLinkRequestDesktop)
        }
    }
}

// Yes, this a large class with a lot of functions. But it's very simple and still easy to read.
@Suppress("TooManyFunctions")
object TipManager {

    private val listOfTips = mutableListOf<Tip>()
    private var listInitialized = false

    private const val FRESH_LOOK_TIP = "fresh_look_tip"
    private const val SHORTCUTS_TIP = "shortcuts_tip"
    private const val ALLOW_LIST_TIP = "allow_list_tip"
    private const val ETP_TIP = "etp_tip"
    private const val REQUEST_DESKTOP_TIP = "request_desktop_tip"

    private fun populateListOfTips(context: Context) {
        addFreshLookTip(context)
        addShortcutsTip(context)
        addTrackingProtectionTip(context)
        addAllowlistTip(context)
        addRequestDesktopTip(context)
    }

    fun getAvailableTips(context: Context): List<Tip>? {
        if (!context.settings.shouldDisplayHomescreenTips()) return null

        if (!listInitialized) {
            listOfTips.clear()
            populateListOfTips(context)
            listInitialized = true
        }

        // All tips should be filtered by 'shouldDisplay' but for now we are sending the list as it is
        // listOfTips.filter { tip -> tip.shouldDisplay() }
        return listOfTips
    }

    private fun addFreshLookTip(context: Context) {
        val tip = Tip.createFreshLookTip(context, FRESH_LOOK_TIP)
        listOfTips.add(tip)
    }

    private fun addShortcutsTip(context: Context) {
        val tip = Tip.createShortcutsTip(context, SHORTCUTS_TIP)
        listOfTips.add(tip)
    }

    private fun addAllowlistTip(context: Context) {
        val tip = Tip.createAllowlistTip(context, ALLOW_LIST_TIP)
        listOfTips.add(tip)
    }

    private fun addTrackingProtectionTip(context: Context) {
        val tip = Tip.createTrackingProtectionTip(context, ETP_TIP)
        listOfTips.add(tip)
    }

    private fun addRequestDesktopTip(context: Context) {
        val tip = Tip.createRequestDesktopTip(context, REQUEST_DESKTOP_TIP)
        listOfTips.add(tip)
    }
}
