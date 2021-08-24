/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.tips

import android.content.Context
import mozilla.components.browser.state.state.SessionState
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.SupportUtils

class Tip(val id: Int, val text: String, val shouldDisplay: () -> Boolean, val deepLink: (() -> Unit)? = null) {
    companion object {

        fun createAllowlistTip(context: Context): Tip {
            val id = R.string.tip_explain_allowlist3
            val name = context.resources.getString(id)
            val url = SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.ALLOWLIST)

            val deepLink = {
                context.components.tabsUseCases.addTab(
                    url,
                    source = SessionState.Source.Internal.Menu,
                    selectTab = true,
                    private = true
                )

                TelemetryWrapper.pressTipEvent(id)
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

            return Tip(id, name, shouldDisplayAllowListTip, deepLink)
        }

        fun createFreshLookTip(context: Context): Tip {
            val id = R.string.tip_fresh_look
            val name = context.resources.getString(id, context.getString(R.string.app_name))

            val shouldDisplayFreshLookTip = {
                Settings.getInstance(context).getAppLaunchCount() == 0
            }

            return Tip(id, name, shouldDisplayFreshLookTip)
        }

        fun createShortcutsTip(context: Context): Tip {
            val id = R.string.tip_about_shortcuts
            val name = context.resources.getString(id, context.getString(R.string.app_name))

            val shouldDisplayShortcutsTip = { true }

            return Tip(id, name, shouldDisplayShortcutsTip)
        }

        fun createTrackingProtectionTip(context: Context): Tip {
            val id = R.string.tip_disable_tracking_protection3
            val name = context.resources.getString(id)

            val shouldDisplayTrackingProtection = {
                Settings.getInstance(context).shouldBlockOtherTrackers() ||
                    Settings.getInstance(context).shouldBlockAdTrackers() ||
                    Settings.getInstance(context).shouldBlockAnalyticTrackers()
            }

            return Tip(id, name, shouldDisplayTrackingProtection)
        }

        fun createRequestDesktopTip(context: Context): Tip {
            val id = R.string.tip_request_desktop2
            val name = String.format(context.resources.getString(id), context.getString(R.string.app_name))
            val requestDesktopURL =
                "https://support.mozilla.org/kb/switch-desktop-view-firefox-focus-android"

            val shouldDisplayRequestDesktop = {
                !Settings.getInstance(context).hasRequestedDesktop()
            }

            val deepLinkRequestDesktop = {
                context.components.tabsUseCases.addTab(
                    requestDesktopURL,
                    source = SessionState.Source.Internal.Menu,
                    selectTab = true,
                    private = true
                )
                TelemetryWrapper.pressTipEvent(id)
            }

            return Tip(id, name, shouldDisplayRequestDesktop, deepLinkRequestDesktop)
        }
    }
}

// Yes, this a large class with a lot of functions. But it's very simple and still easy to read.
@Suppress("TooManyFunctions")
object TipManager {

    private val listOfTips = mutableListOf<Tip>()
    private var listInitialized = false

    private fun populateListOfTips(context: Context) {
        addFreshLookTip(context)
        addShortcutsTip(context)
        addAllowlistTip(context)
        addTrackingProtectionTip(context)
        addRequestDesktopTip(context)
    }

    fun getAvailableTips(context: Context): List<Tip>? {
        if (!Settings.getInstance(context).shouldDisplayHomescreenTips()) return null

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
        val tip = Tip.createFreshLookTip(context)
        listOfTips.add(tip)
    }

    private fun addShortcutsTip(context: Context) {
        val tip = Tip.createShortcutsTip(context)
        listOfTips.add(tip)
    }

    private fun addAllowlistTip(context: Context) {
        val tip = Tip.createAllowlistTip(context)
        listOfTips.add(tip)
    }

    private fun addTrackingProtectionTip(context: Context) {
        val tip = Tip.createTrackingProtectionTip(context)
        listOfTips.add(tip)
    }

    private fun addRequestDesktopTip(context: Context) {
        val tip = Tip.createRequestDesktopTip(context)
        listOfTips.add(tip)
    }
}
