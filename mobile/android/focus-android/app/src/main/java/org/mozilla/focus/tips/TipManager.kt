/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.tips

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import mozilla.components.browser.state.state.SessionState
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity
import org.mozilla.focus.locale.LocaleManager
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.Browsers
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.utils.SupportUtils
import org.mozilla.focus.utils.createTab
import java.util.Locale
import java.util.Random

class Tip(val id: Int, val text: String, val shouldDisplay: () -> Boolean, val deepLink: (() -> Unit)? = null) {
    companion object {
        private const val FORCE_SHOW_DISABLE_TIPS_LAUNCH_COUNT = 2
        private const val FORCE_SHOW_DISABLE_TIPS_INTERVAL = 30

        fun createAllowlistTip(context: Context): Tip {
            val id = R.string.tip_explain_allowlist
            val name = context.resources.getString(id)
            val url = SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.ALLOWLIST)

            val deepLink = {
                val session = createTab(url, source = SessionState.Source.MENU)
                context.components.sessionManager.add(session, selected = true)
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

        fun createTrackingProtectionTip(context: Context): Tip {
            val id = R.string.tip_disable_tracking_protection
            val name = context.resources.getString(id)

            val shouldDisplayTrackingProtection = {
                Settings.getInstance(context).shouldBlockOtherTrackers() ||
                        Settings.getInstance(context).shouldBlockAdTrackers() ||
                        Settings.getInstance(context).shouldBlockAnalyticTrackers()
            }

            return Tip(id, name, shouldDisplayTrackingProtection)
        }

        fun createHomescreenTip(context: Context): Tip {
            val id = R.string.tip_add_to_homescreen
            val name = context.resources.getString(id)
            val homescreenURL =
                    "https://support.mozilla.org/en-US/kb/add-web-page-shortcuts-your-home-screen"

            val deepLinkAddToHomescreen = {
                val session = createTab(homescreenURL, source = SessionState.Source.MENU)
                context.components.sessionManager.add(session, selected = true)
                TelemetryWrapper.pressTipEvent(id)
            }

            val shouldDisplayAddToHomescreen = { !Settings.getInstance(context).hasAddedToHomeScreen }

            return Tip(id, name, shouldDisplayAddToHomescreen, deepLinkAddToHomescreen)
        }

        fun createDefaultBrowserTip(context: Context): Tip {
            val appName = context.resources.getString(R.string.app_name)
            val id = R.string.tip_set_default_browser
            val name = context.resources.getString(id, appName)
            val browsers = Browsers(context, Browsers.TRADITIONAL_BROWSER_URL)

            val shouldDisplayDefaultBrowser = {
                !Browsers(context, Browsers.TRADITIONAL_BROWSER_URL).isDefaultBrowser(context)
            }

            val deepLinkDefaultBrowser = {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    SupportUtils.openDefaultAppsSettings(context)
                } else if (!browsers.hasDefaultBrowser(context)) {
                    // Open in method:
                    val intent = Intent(Intent.ACTION_VIEW, Uri.parse(SupportUtils.OPEN_WITH_DEFAULT_BROWSER_URL))
                    context.startActivity(intent)
                } else {
                    SupportUtils.openDefaultBrowserSumoPage(context)
                }
                TelemetryWrapper.pressTipEvent(id)
            }

            return Tip(id, name, shouldDisplayDefaultBrowser, deepLinkDefaultBrowser)
        }

        fun createAutocompleteURLTip(context: Context): Tip {
            val id = R.string.tip_autocomplete_url
            val name = context.resources.getString(id)
            val autocompleteURL =
                    "https://support.mozilla.org/en-US/kb/autocomplete-settings-firefox-focus-address-bar"

            val shouldDisplayAutocompleteUrl = {
                !Settings.getInstance(context).shouldAutocompleteFromCustomDomainList()
            }

            val deepLinkAutocompleteUrl = {
                val session = createTab(autocompleteURL, source = SessionState.Source.MENU)
                context.components.sessionManager.add(session, selected = true)
                TelemetryWrapper.pressTipEvent(id)
            }

            return Tip(id, name, shouldDisplayAutocompleteUrl, deepLinkAutocompleteUrl)
        }

        fun createOpenInNewTabTip(context: Context): Tip {
            val id = R.string.tip_open_in_new_tab
            val name = context.resources.getString(id)
            val newTabURL =
                    "https://support.mozilla.org/en-US/kb/open-new-tab-firefox-focus-android"

            val shouldDisplayOpenInNewTab = {
                !Settings.getInstance(context).hasOpenedInNewTab()
            }

            val deepLinkOpenInNewTab = {
                val session = createTab(newTabURL, source = SessionState.Source.MENU)
                context.components.sessionManager.add(session, selected = true)
                TelemetryWrapper.pressTipEvent(id)
            }

            return Tip(id, name, shouldDisplayOpenInNewTab, deepLinkOpenInNewTab)
        }

        fun createRequestDesktopTip(context: Context): Tip {
            val id = R.string.tip_request_desktop
            val name = context.resources.getString(id)
            val requestDesktopURL =
                    "https://support.mozilla.org/en-US/kb/switch-desktop-view-firefox-focus-android"

            val shouldDisplayRequestDesktop = {
                !Settings.getInstance(context).hasRequestedDesktop()
            }

            val deepLinkRequestDesktop = {
                val session = createTab(requestDesktopURL, source = SessionState.Source.MENU)
                context.components.sessionManager.add(session, selected = true)
                TelemetryWrapper.pressTipEvent(id)
            }

            return Tip(id, name, shouldDisplayRequestDesktop, deepLinkRequestDesktop)
        }

        fun createDisableTipsTip(context: Context): Tip {
            val id = R.string.tip_disable_tips2
            val name = context.resources.getString(id)

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

// Yes, this a large class with a lot of functions. But it's very simple and still easy to read.
@Suppress("TooManyFunctions")
object TipManager {
    private const val MAX_TIPS_TO_DISPLAY = 3

    private val listOfTips = mutableListOf<Tip>()
    private val random = Random()
    private var listInitialized = false
    private var tipsShown = 0
    private var locale: Locale? = null

    private fun populateListOfTips(context: Context) {
        addAllowlistTip(context)
        addTrackingProtectionTip(context)
        addHomescreenTip(context)
        addDefaultBrowserTip(context)
        addAutocompleteUrlTip(context)
        addOpenInNewTabTip(context)
        addRequestDesktopTip(context)
        addDisableTipsTip(context)
    }

    // Will not return a tip if tips are disabled or if MAX TIPS have already been shown.
    @Suppress("ReturnCount", "ComplexMethod") // Using early returns
    fun getNextTipIfAvailable(context: Context): Tip? {
        if (!Settings.getInstance(context).shouldDisplayHomescreenTips()) return null
        val currentLocale = LocaleManager.getInstance().getCurrentLocale(context)

        if (!listInitialized || currentLocale != locale) {
            locale = currentLocale
            populateListOfTips(context)
            listInitialized = true
        }

        // Only show three tips before going back to the "Focus" branding
        if (tipsShown == MAX_TIPS_TO_DISPLAY || listOfTips.count() <= 0) {
            return null
        }

        // Show the survey tip first
        for (tip in listOfTips) {
            if (tip.id == R.string.tip_take_survey && tip.shouldDisplay()) {
                listOfTips.remove(tip)
                return tip
            }
        }

        // Always show the disable tip if it's ready to be displayed
        for (tip in listOfTips) {
            if (tip.id == R.string.tip_disable_tips2 && tip.shouldDisplay()) {
                listOfTips.remove(tip)
                return tip
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

    private fun addAllowlistTip(context: Context) {
        val tip = Tip.createAllowlistTip(context)
        listOfTips.add(tip)
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
