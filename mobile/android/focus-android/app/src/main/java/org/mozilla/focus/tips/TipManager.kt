/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.tips

import android.app.Activity
import android.content.Context
import android.os.Build
import org.mozilla.focus.R.string.app_name
import org.mozilla.focus.R.string.tip_add_to_homescreen
import org.mozilla.focus.R.string.tip_autocomplete_url
import org.mozilla.focus.R.string.tip_disable_tracking_protection
import org.mozilla.focus.R.string.tip_open_in_new_tab
import org.mozilla.focus.R.string.tip_request_desktop
import org.mozilla.focus.R.string.tip_disable_tips
import org.mozilla.focus.R.string.app_name
import org.mozilla.focus.R.string.tip_set_default_browser
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity
import org.mozilla.focus.session.SessionManager
import org.mozilla.focus.session.Source
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.homeScreenTipsExperimentDescriptor
import org.mozilla.focus.utils.isInExperiment
import java.util.Random

class Tip(val id: Int, val text: String, val shouldDisplay: () -> Boolean, val deepLink: (() -> Unit)? = null) {
    companion object {
        private const val FORCE_SHOW_DISABLE_TIPS_LAUNCH_COUNT = 2
        private const val FORCE_SHOW_DISABLE_TIPS_INTERVAL = 30

        fun createTrackingProtectionTip(context: Context): Tip {
            val id = tip_disable_tracking_protection
            val name = context.resources.getString(id)

            val shouldDisplayTrackingProtection = {
                Settings.getInstance(context).shouldBlockOtherTrackers() ||
                        Settings.getInstance(context).shouldBlockAdTrackers() ||
                        Settings.getInstance(context).shouldBlockAnalyticTrackers()
            }

            return Tip(id, name, shouldDisplayTrackingProtection)
        }

        fun createHomescreenTip(context: Context): Tip {
            val id = tip_add_to_homescreen
            val name = context.resources.getString(id)

            val shouldDisplayAddToHomescreen = { !Settings.getInstance(context).hasAddedToHomeScreen }

            return Tip(id, name, shouldDisplayAddToHomescreen)
        }

        fun createDefaultBrowserTip(context: Context): Tip {
            val appName = context.resources.getString(app_name)
            val id = tip_set_default_browser
            val name = context.resources.getString(id, appName)

            val shouldDisplayDefaultBrowser = {
                !Settings.getInstance(context).isDefaultBrowser()
            }

            val deepLinkDefaultBrowser = {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    SupportUtils.openDefaultAppsSettings(context)
                } else {
                    SupportUtils.openDefaultBrowserSumoPage(context)
                }
                TelemetryWrapper.pressTipEvent(id)
            }

            return Tip(id, name, shouldDisplayDefaultBrowser, deepLinkDefaultBrowser)
        }

        fun createAutocompleteURLTip(context: Context): Tip {
            val id = tip_autocomplete_url
            val name = context.resources.getString(id)

            val shouldDisplayAutocompleteUrl = {
                !Settings.getInstance(context).shouldAutocompleteFromCustomDomainList()
            }

            return Tip(id, name, shouldDisplayAutocompleteUrl)
        }

        fun createOpenInNewTabTip(context: Context): Tip {
            val id = tip_open_in_new_tab
            val name = context.resources.getString(id)
            val newTabURL =
                    "https://support.mozilla.org/en-US/kb/open-new-tab-firefox-focus-android"

            val shouldDisplayOpenInNewTab = {
                !Settings.getInstance(context).hasOpenedInNewTab()
            }

            val deepLinkOpenInNewTab = {
                SessionManager.getInstance().createSession(Source.MENU, newTabURL)
                TelemetryWrapper.pressTipEvent(id)
            }

            return Tip(id, name, shouldDisplayOpenInNewTab, deepLinkOpenInNewTab)
        }

        fun createRequestDesktopTip(context: Context): Tip {
            val id = tip_request_desktop
            val name = context.resources.getString(id)

            val shouldDisplayOpenInNewTab = {
                !Settings.getInstance(context).hasRequestedDesktop()
            }

            return Tip(id, name, shouldDisplayOpenInNewTab)
        }

        fun createDisableTipsTip(context: Context): Tip {
            val appName = context.resources.getString(app_name)
            val id = tip_disable_tips
            val name = context.resources.getString(id, appName)

            val shouldDisplayDisableTips = {
                // Count number of app launches
                val launchCount = Settings.getInstance(context).getAppLaunchCount()
                var shouldDisplay = false

                if (launchCount != 0 && (launchCount == FORCE_SHOW_DISABLE_TIPS_LAUNCH_COUNT ||
                                launchCount % FORCE_SHOW_DISABLE_TIPS_INTERVAL == 0)) {
                    shouldDisplay = true
                }

                shouldDisplay
            }

            val deepLinkDisableTips = {
                val activity = context as Activity
                (activity as LocaleAwareAppCompatActivity).openMozillaSettings()
                TelemetryWrapper.pressTipEvent(id)
            }

            return Tip(id, name, shouldDisplayDisableTips, deepLinkDisableTips)
        }
    }
}

object TipManager {
    private const val MAX_TIPS_TO_DISPLAY = 3

    private val listOfTips = mutableListOf<Tip>()
    private val random = Random()
    private var listInitialized = false
    private var tipsShown = 0

    private fun populateListOfTips(context: Context) {
        addTrackingProtectionTip(context)
        addHomescreenTip(context)
        addDefaultBrowserTip(context)
        addAutocompleteUrlTip(context)
        addOpenInNewTabTip(context)
        addRequestDesktopTip(context)
        addDisableTipsTip(context)
    }

    // Will not return a tip if tips are disabled or if MAX TIPS have already been shown.
    @Suppress("ReturnCount") // Using early returns
    fun getNextTipIfAvailable(context: Context): Tip? {
        if (!context.isInExperiment(homeScreenTipsExperimentDescriptor)) return null

        if (!listInitialized) {
            populateListOfTips(context)
            listInitialized = true
        }

        // Only show three tips before going back to the "Focus" branding and if they're enabled
        if (tipsShown == MAX_TIPS_TO_DISPLAY || listOfTips.count() <= 0) {
            return null
        }

        // Always show the disable tip if it's ready to be displayed
        listOfTips
                .firstOrNull { it.id == tip_disable_tips }
                ?.let {
                    if (it.shouldDisplay()) {
                        listOfTips.remove(it)
                        return it
                    }
                }

        var tip = listOfTips[getRandomTipIndex()]

        // Find a random tip that the user doesn't already know about
        while (!tip.shouldDisplay()) {
            listOfTips.remove(tip)
            if (listOfTips.count() == 0) { return null }
            tip = listOfTips[getRandomTipIndex()]
        }

        listOfTips.remove(tip)
        tipsShown += 1
        TelemetryWrapper.displayTipEvent(tip.id)
        return tip
    }

    private fun getRandomTipIndex(): Int {
        return if (listOfTips.count() == 1) 0 else random.nextInt(listOfTips.count() - 1)
    }

    private fun addTrackingProtectionTip(context: Context) {
        val tip = Tip.createTrackingProtectionTip(context)
        listOfTips.add(tip)
    }

    private fun addHomescreenTip(context: Context) {
        val tip = Tip.createHomescreenTip(context)
        listOfTips.add(tip)
    }

    private fun addDefaultBrowserTip(context: Context) {
        val tip = Tip.createDefaultBrowserTip(context)
        listOfTips.add(tip)
    }

    private fun addAutocompleteUrlTip(context: Context) {
        val tip = Tip.createAutocompleteURLTip(context)
        listOfTips.add(tip)
    }

    private fun addOpenInNewTabTip(context: Context) {
        val tip = Tip.createOpenInNewTabTip(context)
        listOfTips.add(tip)
    }

    private fun addRequestDesktopTip(context: Context) {
        val tip = Tip.createRequestDesktopTip(context)
        listOfTips.add(tip)
    }

    private fun addDisableTipsTip(context: Context) {
        val tip = Tip.createDisableTipsTip(context)
        listOfTips.add(tip)
    }
}
