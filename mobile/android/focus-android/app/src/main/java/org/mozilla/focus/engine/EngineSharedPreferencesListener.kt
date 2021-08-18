/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.engine

import android.content.Context
import android.content.SharedPreferences
import org.mozilla.focus.R
import org.mozilla.focus.ext.components
import org.mozilla.focus.utils.Settings

/**
 * SharedPreference listener that will update the engine whenever the user changes settings.
 */
class EngineSharedPreferencesListener(
    private val context: Context
) : SharedPreferences.OnSharedPreferenceChangeListener {
    override fun onSharedPreferenceChanged(preferences: SharedPreferences, key: String) {
        when (key) {
            context.getString(R.string.pref_key_privacy_block_social),
            context.getString(R.string.pref_key_privacy_block_ads),
            context.getString(R.string.pref_key_privacy_block_analytics),
            context.getString(R.string.pref_key_privacy_block_other2),
            context.getString(R.string.pref_key_performance_enable_cookies) ->
                updateTrackingProtectionPolicy()

            context.getString(R.string.pref_key_safe_browsing) ->
                updateSafeBrowsingPolicy()

            context.getString(R.string.pref_key_performance_block_javascript) ->
                updateJavaScriptSetting()

            context.getString(R.string.pref_key_remote_debugging) ->
                updateRemoteDebugging()

            context.getString(R.string.pref_key_performance_block_webfonts) ->
                updateWebFontsBlocking()
        }
    }

    internal fun updateTrackingProtectionPolicy() {
        val policy = Settings.getInstance(context).createTrackingProtectionPolicy()
        val components = context.components

        components.engineDefaultSettings.trackingProtectionPolicy = policy
        components.settingsUseCases.updateTrackingProtection(policy)
    }

    private fun updateSafeBrowsingPolicy() {
        Settings.getInstance(context).setupSafeBrowsing(context.components.engine)
    }

    private fun updateJavaScriptSetting() {
        val settings = Settings.getInstance(context)
        val components = context.components

        components.engineDefaultSettings.javascriptEnabled = !settings.shouldBlockJavaScript()
        components.engine.settings.javascriptEnabled = !settings.shouldBlockJavaScript()
    }

    private fun updateRemoteDebugging() {
        context.components.engine.settings.remoteDebuggingEnabled =
            Settings.getInstance(context).shouldEnableRemoteDebugging()
    }

    private fun updateWebFontsBlocking() {
        val settings = Settings.getInstance(context)
        val components = context.components

        components.engineDefaultSettings.webFontsEnabled = !settings.shouldBlockWebFonts()
        components.engine.settings.webFontsEnabled = !settings.shouldBlockWebFonts()
    }
}
