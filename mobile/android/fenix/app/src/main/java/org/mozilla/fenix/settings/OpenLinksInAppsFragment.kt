/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.utils.view.addToRadioGroup

/**
 * Lets the user choose how open links in apps feature behaves.
 */
class OpenLinksInAppsFragment : PreferenceFragmentCompat() {
    private lateinit var radioAlways: RadioButtonPreference
    private lateinit var radioAskBeforeOpening: RadioButtonPreference
    private lateinit var radioNever: RadioButtonPreference

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.open_links_in_apps_preferences, rootKey)

        radioAlways = requirePreference(R.string.pref_key_open_links_in_apps_always)
        radioAskBeforeOpening = requirePreference(R.string.pref_key_open_links_in_apps_ask)
        radioNever = requirePreference(R.string.pref_key_open_links_in_apps_never)

        /* only show the Always option in normal browsing mode */
        radioAlways.isVisible = requireContext().settings().lastKnownMode == BrowsingMode.Normal
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_open_links_in_apps))

        setupPreferences()
    }

    private fun setupPreferences() {
        when (requireContext().settings().openLinksInExternalApp) {
            getString(R.string.pref_key_open_links_in_apps_always) ->
                if (requireContext().settings().lastKnownMode == BrowsingMode.Normal) {
                    radioAlways.setCheckedWithoutClickListener(true)
                    radioAskBeforeOpening.setCheckedWithoutClickListener(false)
                } else {
                    radioAlways.setCheckedWithoutClickListener(false)
                    radioAskBeforeOpening.setCheckedWithoutClickListener(true)
                }
            getString(R.string.pref_key_open_links_in_apps_ask) ->
                radioAskBeforeOpening.setCheckedWithoutClickListener(true)
            getString(R.string.pref_key_open_links_in_apps_never) ->
                radioNever.setCheckedWithoutClickListener(true)
        }

        radioAlways.onClickListener(::onClickAlwaysOption)
        radioAskBeforeOpening.onClickListener(::onClickAskOption)
        radioNever.onClickListener(::onClickNeverOption)

        setupRadioGroups()
    }

    private fun setupRadioGroups() {
        addToRadioGroup(
            radioAlways,
            radioAskBeforeOpening,
            radioNever,
        )
    }

    private fun onClickAlwaysOption() {
        requireContext().settings().openLinksInExternalApp =
            getString(R.string.pref_key_open_links_in_apps_always)
    }

    private fun onClickAskOption() {
        requireContext().settings().openLinksInExternalApp =
            getString(R.string.pref_key_open_links_in_apps_ask)
    }

    private fun onClickNeverOption() {
        requireContext().settings().openLinksInExternalApp =
            getString(R.string.pref_key_open_links_in_apps_never)
    }
}
