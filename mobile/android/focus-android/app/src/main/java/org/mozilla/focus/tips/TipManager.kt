/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.tips

import android.content.Context
import org.mozilla.focus.R.string.tip_open_in_new_tab
import org.mozilla.focus.R.string.tip_set_default_browser
import org.mozilla.focus.R.string.tip_add_to_homescreen
import org.mozilla.focus.R.string.tip_disable_tracking_protection
import org.mozilla.focus.R.string.tip_autocomplete_url
import org.mozilla.focus.R.string.app_name
import org.mozilla.focus.utils.Settings
import org.mozilla.focus.widget.DefaultBrowserPreference
import java.util.Random
import android.app.Activity
import org.mozilla.focus.locale.LocaleAwareAppCompatActivity
import org.mozilla.focus.session.SessionManager
import org.mozilla.focus.session.Source

class Tip(val text: String, val shouldDisplay: () -> Boolean, val deepLink: () -> Unit)

object TipManager {

    private const val NEW_TAB_URL =
        "https://support.mozilla.org/en-US/kb/open-new-tab-firefox-focus-android"
    private const val ADD_HOMESCREEN_URL =
        "https://support.mozilla.org/en-US/kb/add-web-page-shortcuts-your-home-screen"
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
    }

    fun getNextTip(context: Context): Tip? {
        if (!listInitialized) {
            populateListOfTips(context)
            listInitialized = true
        }

        // Only show three tips before going back to the "Focus" branding
        if (tipsShown == MAX_TIPS_TO_DISPLAY || listOfTips.count() <= 0) {
            return null
        }

        var tip = listOfTips[getRandomTipIndex()]

        // Find a random tip that the user doesn't already know about
        while (!tip.shouldDisplay()) {
            tip = listOfTips[getRandomTipIndex()]
            if (!tip.shouldDisplay()) { listOfTips.remove(tip) }
            if (listOfTips.count() == 0) { return null }
        }

        listOfTips.remove(tip)
        tipsShown += 1
        return tip
    }

    private fun getRandomTipIndex(): Int {
        return if (listOfTips.count() == 1) 0 else random.nextInt(listOfTips.count() - 1)
    }

    private fun addTrackingProtectionTip(context: Context) {
        val shouldDisplayTrackingProtection = {
            Settings.getInstance(context).shouldBlockOtherTrackers() ||
                    Settings.getInstance(context).shouldBlockAdTrackers() ||
                    Settings.getInstance(context).shouldBlockAnalyticTrackers()
        }

        val deepLinkTrackingProtection = {
            val activity = context as Activity
            (activity as LocaleAwareAppCompatActivity).openPrivacySecuritySettings()
        }

        listOfTips.add(Tip(context.resources.getString(tip_disable_tracking_protection),
            shouldDisplayTrackingProtection, deepLinkTrackingProtection))
    }

    private fun addHomescreenTip(context: Context) {
        val shouldDisplayAddToHomescreen = {
            !Settings.getInstance(context).hasAddedToHomescreen()
        }

        val deepLinkAddToHomescreen = {
            SessionManager.getInstance().createSession(Source.MENU, ADD_HOMESCREEN_URL)
        }

        listOfTips.add(Tip(context.resources.getString(tip_add_to_homescreen),
            shouldDisplayAddToHomescreen,
            deepLinkAddToHomescreen))
    }

    private fun addDefaultBrowserTip(context: Context) {
        val appName = context.resources.getString(app_name)

        val shouldDisplayDefaultBrowser = {
            !Settings.getInstance(context).isDefaultBrowser()
        }

        val deepLinkDefaultBrowser = {
            DefaultBrowserPreference(context, null).onClick()
        }

        listOfTips.add(Tip(context.resources.getString(tip_set_default_browser, appName),
            shouldDisplayDefaultBrowser,
            deepLinkDefaultBrowser))
    }

    private fun addAutocompleteUrlTip(context: Context) {
        val shouldDisplayAutocompleteUrl = {
            !Settings.getInstance(context).shouldAutocompleteFromCustomDomainList()
        }

        val deepLinkAutocompleteUrl = {
            val activity = context as Activity
            (activity as LocaleAwareAppCompatActivity).openAutocompleteUrlSettings()
        }

        listOfTips.add(Tip(context.resources.getString(tip_autocomplete_url),
            shouldDisplayAutocompleteUrl,
            deepLinkAutocompleteUrl))
    }

    private fun addOpenInNewTabTip(context: Context) {
        val shouldDisplayOpenInNewTab = {
            !Settings.getInstance(context).hasOpenedInNewTab()
        }

        val deepLinkOpenInNewTab = {
            SessionManager.getInstance().createSession(Source.MENU, NEW_TAB_URL)
        }

        listOfTips.add(Tip(context.resources.getString(tip_open_in_new_tab),
            shouldDisplayOpenInNewTab,
            deepLinkOpenInNewTab))
    }
}
