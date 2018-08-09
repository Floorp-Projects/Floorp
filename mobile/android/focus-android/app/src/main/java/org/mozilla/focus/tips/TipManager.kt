/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.tips

import android.content.Context
import org.mozilla.focus.utils.Settings
import java.util.Random
import android.app.Activity
import android.os.Build
import org.mozilla.focus.R.string.tip_open_in_new_tab
import org.mozilla.focus.R.string.tip_set_default_browser
import org.mozilla.focus.R.string.tip_add_to_homescreen
import org.mozilla.focus.R.string.tip_disable_tracking_protection
import org.mozilla.focus.R.string.tip_autocomplete_url
import org.mozilla.focus.R.string.tip_request_desktop
import org.mozilla.focus.R.string.app_name
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity
import org.mozilla.focus.session.SessionManager
import org.mozilla.focus.session.Source
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.SupportUtils

class Tip(val id: Int, val text: String, val shouldDisplay: () -> Boolean, val deepLink: () -> Unit)

object TipManager {

    private const val NEW_TAB_URL =
        "https://support.mozilla.org/en-US/kb/open-new-tab-firefox-focus-android"
    private const val ADD_HOMESCREEN_URL =
        "https://support.mozilla.org/en-US/kb/add-web-page-shortcuts-your-home-screen"
    private const val AUTOCOMPLETE_URL =
        "https://support.mozilla.org/en-US/kb/autocomplete-settings-firefox-focus-address-bar"
    private const val REQUEST_DESKTOP_URL =
        "https://support.mozilla.org/en-US/kb/switch-desktop-view-firefox-focus-android"
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
    }

    // Will not return a tip if tips are disabled or if MAX TIPS have already been shown.
    fun getNextTipIfAvailable(context: Context): Tip? {
        if (!listInitialized) {
            populateListOfTips(context)
            listInitialized = true
        }

        // Only show three tips before going back to the "Focus" branding and if they're enabled
        if (tipsShown == MAX_TIPS_TO_DISPLAY || listOfTips.count() <= 0 ||
            !Settings.getInstance(context).shouldDisplayHomescreenTips()) {
            return null
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
        val id = tip_disable_tracking_protection
        val name = context.resources.getString(id)

        val shouldDisplayTrackingProtection = {
            Settings.getInstance(context).shouldBlockOtherTrackers() ||
                    Settings.getInstance(context).shouldBlockAdTrackers() ||
                    Settings.getInstance(context).shouldBlockAnalyticTrackers()
        }

        val deepLinkTrackingProtection = {
            val activity = context as Activity
            (activity as LocaleAwareAppCompatActivity).openPrivacySecuritySettings()
            TelemetryWrapper.pressTipEvent(id)
        }

        listOfTips.add(Tip(id, name, shouldDisplayTrackingProtection, deepLinkTrackingProtection))
    }

    private fun addHomescreenTip(context: Context) {
        val id = tip_add_to_homescreen
        val name = context.resources.getString(id)
        val shouldDisplayAddToHomescreen = {
            !Settings.getInstance(context).hasAddedToHomescreen()
        }

        val deepLinkAddToHomescreen = {
            SessionManager.getInstance().createSession(Source.MENU, ADD_HOMESCREEN_URL)
            TelemetryWrapper.pressTipEvent(id)
        }

        listOfTips.add(Tip(id, name, shouldDisplayAddToHomescreen, deepLinkAddToHomescreen))
    }

    private fun addDefaultBrowserTip(context: Context) {
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

        listOfTips.add(Tip(id,
            name,
            shouldDisplayDefaultBrowser,
            deepLinkDefaultBrowser))
    }

    private fun addAutocompleteUrlTip(context: Context) {
        val id = tip_autocomplete_url
        val name = context.resources.getString(id)

        val shouldDisplayAutocompleteUrl = {
            !Settings.getInstance(context).shouldAutocompleteFromCustomDomainList()
        }

        val deepLinkAutocompleteUrl = {
            SessionManager.getInstance().createSession(Source.MENU, AUTOCOMPLETE_URL)
            TelemetryWrapper.pressTipEvent(id)
        }

        listOfTips.add(Tip(id, name, shouldDisplayAutocompleteUrl, deepLinkAutocompleteUrl))
    }

    private fun addOpenInNewTabTip(context: Context) {
        val id = tip_open_in_new_tab
        val name = context.resources.getString(id)

        val shouldDisplayOpenInNewTab = {
            !Settings.getInstance(context).hasOpenedInNewTab()
        }

        val deepLinkOpenInNewTab = {
            SessionManager.getInstance().createSession(Source.MENU, NEW_TAB_URL)
            TelemetryWrapper.pressTipEvent(id)
        }

        listOfTips.add(Tip(id, name, shouldDisplayOpenInNewTab, deepLinkOpenInNewTab))
    }

    private fun addRequestDesktopTip(context: Context) {
        val id = tip_request_desktop
        val name = context.resources.getString(id)

        val shouldDisplayOpenInNewTab = {
            !Settings.getInstance(context).hasRequestedDesktop()
        }

        val deepLinkOpenInNewTab = {
            SessionManager.getInstance().createSession(Source.MENU, REQUEST_DESKTOP_URL)
            TelemetryWrapper.pressTipEvent(id)
        }

        listOfTips.add(Tip(id, name, shouldDisplayOpenInNewTab, deepLinkOpenInNewTab))
    }
}
