/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.os.Bundle
import android.provider.Settings
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.navigation.Navigation
import androidx.navigation.fragment.findNavController
import androidx.preference.Preference
import androidx.preference.Preference.OnPreferenceClickListener
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.Config
import org.mozilla.fenix.FenixApplication
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.R.string.pref_key_about
import org.mozilla.fenix.R.string.pref_key_accessibility
import org.mozilla.fenix.R.string.pref_key_account
import org.mozilla.fenix.R.string.pref_key_account_auth_error
import org.mozilla.fenix.R.string.pref_key_account_category
import org.mozilla.fenix.R.string.pref_key_data_choices
import org.mozilla.fenix.R.string.pref_key_delete_browsing_data
import org.mozilla.fenix.R.string.pref_key_help
import org.mozilla.fenix.R.string.pref_key_language
import org.mozilla.fenix.R.string.pref_key_leakcanary
import org.mozilla.fenix.R.string.pref_key_make_default_browser
import org.mozilla.fenix.R.string.pref_key_privacy_link
import org.mozilla.fenix.R.string.pref_key_rate
import org.mozilla.fenix.R.string.pref_key_remote_debugging
import org.mozilla.fenix.R.string.pref_key_search_engine_settings
import org.mozilla.fenix.R.string.pref_key_sign_in
import org.mozilla.fenix.R.string.pref_key_site_permissions
import org.mozilla.fenix.R.string.pref_key_theme
import org.mozilla.fenix.R.string.pref_key_tracking_protection_settings
import org.mozilla.fenix.R.string.pref_key_your_rights
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.utils.ItsNotBrokenSnack

@SuppressWarnings("TooManyFunctions", "LargeClass")
class SettingsFragment : PreferenceFragmentCompat(), AccountObserver {
    private val preferenceChangeListener =
        SharedPreferences.OnSharedPreferenceChangeListener { sharedPreferences, key ->
            try {
                context?.let {
                    it.components.analytics.metrics.track(
                        Event.PreferenceToggled
                            (key, sharedPreferences.getBoolean(key, false), it)
                    )
                }
            } catch (e: IllegalArgumentException) {
                // The event is not tracked
            } catch (e: ClassCastException) {
                // The setting is not a boolean, not tracked
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Observe account changes to keep the UI up-to-date.
        requireComponents.backgroundServices.accountManager.register(this, owner = this, autoPause = true)

        // It's important to update the account UI state in onCreate, even though we also call it in onResume, since
        // that ensures we'll never display an incorrect state in the UI. For example, if user is signed-in, and we
        // don't perform this call in onCreate, we'll briefly display a "Sign In" preference, which will then get
        // replaced by the correct account information once this call is ran in onResume shortly after.
        updateAccountUIState(context!!, requireComponents.backgroundServices.accountManager.accountProfile())

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(preferenceChangeListener)

        if (SDK_INT <= Build.VERSION_CODES.M) {
            findPreference<DefaultBrowserPreference>(getPreferenceKey(R.string.pref_key_make_default_browser))?.apply {
                isVisible = false
            }
        }
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()

        (activity as AppCompatActivity).title = getString(R.string.settings_title)
        (activity as AppCompatActivity).supportActionBar?.show()
        val defaultBrowserPreference =
            findPreference<DefaultBrowserPreference>(getPreferenceKey(R.string.pref_key_make_default_browser))
        defaultBrowserPreference?.updateSwitch()

        val searchEnginePreference =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_search_engine_settings))
        searchEnginePreference?.summary = context?.let {
            requireComponents.search.searchEngineManager.getDefaultSearchEngine(it).name
        }

        val trackingProtectionPreference =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_tracking_protection_settings))
        trackingProtectionPreference?.summary = context?.let {
            if (org.mozilla.fenix.utils.Settings.getInstance(it).shouldUseTrackingProtection) {
                getString(R.string.tracking_protection_on)
            } else {
                getString(R.string.tracking_protection_off)
            }
        }

        val themesPreference =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_theme))
        themesPreference?.summary = context?.let {
            org.mozilla.fenix.utils.Settings.getInstance(it).themeSettingString
        }

        val aboutPreference = findPreference<Preference>(getPreferenceKey(R.string.pref_key_about))
        val appName = getString(R.string.app_name)
        aboutPreference?.title = getString(R.string.preferences_about, appName)

        setupPreferences()

        updateAccountUIState(context!!, requireComponents.backgroundServices.accountManager.accountProfile())
    }

    @Suppress("ComplexMethod")
    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            resources.getString(pref_key_search_engine_settings) -> {
                navigateToSearchEngineSettings()
            }
            resources.getString(pref_key_tracking_protection_settings) -> {
                navigateToTrackingProtectionSettings()
            }
            resources.getString(pref_key_site_permissions) -> {
                navigateToSitePermissions()
            }
            resources.getString(pref_key_accessibility) -> {
                navigateToAccessibility()
            }
            resources.getString(pref_key_language) -> {
                // TODO #220
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "220")
            }
            resources.getString(pref_key_data_choices) -> {
                navigateToDataChoices()
            }
            resources.getString(pref_key_help) -> {
                (activity as HomeActivity).openToBrowserAndLoad(
                    searchTermOrURL = SupportUtils.getSumoURLForTopic(context!!, SupportUtils.SumoTopic.HELP),
                    newTab = true,
                    from = BrowserDirection.FromSettings
                )
            }
            resources.getString(pref_key_rate) -> {
                try {
                    startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(SupportUtils.RATE_APP_URL)))
                } catch (e: ActivityNotFoundException) {
                    // Device without the play store installed.
                    // Opening the play store website.
                    (activity as HomeActivity).openToBrowserAndLoad(
                        searchTermOrURL = SupportUtils.FENIX_PLAY_STORE_URL,
                        newTab = true,
                        from = BrowserDirection.FromSettings
                    )
                }
            }
            resources.getString(pref_key_about) -> {
                navigateToAbout()
            }
            resources.getString(pref_key_account) -> {
                navigateToAccountSettings()
            }
            resources.getString(pref_key_account_auth_error) -> {
                navigateToAccountProblem()
            }
            resources.getString(pref_key_delete_browsing_data) -> {
                navigateToDeleteBrowsingData()
            }
            resources.getString(pref_key_theme) -> {
                navigateToThemeSettings()
            }
            resources.getString(pref_key_privacy_link) -> {
                requireContext().let { context ->
                    val intent = SupportUtils.createCustomTabIntent(context, SupportUtils.PRIVACY_NOTICE_URL)
                    startActivity(intent)
                }
            }
            resources.getString(pref_key_your_rights) -> {
                requireContext().let { context ->
                    val intent = SupportUtils.createCustomTabIntent(
                        context,
                        SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.YOUR_RIGHTS)
                    )
                    startActivity(intent)
                }
            }
        }
        return super.onPreferenceTreeClick(preference)
    }

    override fun onDestroy() {
        super.onDestroy()
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(preferenceChangeListener)
    }

    private fun getClickListenerForSignIn(): OnPreferenceClickListener {
        return OnPreferenceClickListener {
            context!!.components.services.launchPairingSignIn(context!!, findNavController())
            true
        }
    }

    private fun setupPreferences() {
        val makeDefaultBrowserKey = getPreferenceKey(pref_key_make_default_browser)
        val leakKey = getPreferenceKey(pref_key_leakcanary)
        val debuggingKey = getPreferenceKey(pref_key_remote_debugging)

        val preferenceMakeDefaultBrowser = findPreference<Preference>(makeDefaultBrowserKey)
        val preferenceLeakCanary = findPreference<Preference>(leakKey)
        val preferenceRemoteDebugging = findPreference<Preference>(debuggingKey)

        preferenceMakeDefaultBrowser?.onPreferenceClickListener =
            getClickListenerForMakeDefaultBrowser()

        if (!Config.channel.isReleased) {
            preferenceLeakCanary?.setOnPreferenceChangeListener { _, newValue ->
                (context?.applicationContext as FenixApplication).toggleLeakCanary(newValue as Boolean)
                true
            }
        }

        preferenceRemoteDebugging?.setOnPreferenceChangeListener { preference, newValue ->
            org.mozilla.fenix.utils.Settings.getInstance(preference.context).preferences.edit()
                .putBoolean(preference.key, newValue as Boolean).apply()
            requireComponents.core.engine.settings.remoteDebuggingEnabled = newValue
            true
        }
    }

    private val defaultClickListener = OnPreferenceClickListener { preference ->
        Toast.makeText(context, "${preference.title} Clicked", Toast.LENGTH_SHORT).show()
        true
    }

    private fun getClickListenerForMakeDefaultBrowser(): OnPreferenceClickListener {
        return if (SDK_INT >= Build.VERSION_CODES.N) {
            OnPreferenceClickListener {
                val intent = Intent(
                    Settings.ACTION_MANAGE_DEFAULT_APPS_SETTINGS
                )
                startActivity(intent)
                true
            }
        } else {
            defaultClickListener
        }
    }

    private fun navigateToSearchEngineSettings() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToSearchEngineFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToTrackingProtectionSettings() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToTrackingProtectionFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToThemeSettings() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToThemeFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToSitePermissions() {
        val directions =
            SettingsFragmentDirections.actionSettingsFragmentToSitePermissionsFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToAccessibility() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToAccessibilityFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToDataChoices() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToDataChoicesFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToAbout() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToAboutFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToAccountProblem() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToAccountProblemFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToAccountSettings() {
        val directions =
            SettingsFragmentDirections.actionSettingsFragmentToAccountSettingsFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToDeleteBrowsingData() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToDeleteBrowsingDataFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
        lifecycleScope.launch {
            context?.let {
                updateAccountUIState(it, it.components.backgroundServices.accountManager.accountProfile())
            }
        }
    }

    override fun onLoggedOut() {
        lifecycleScope.launch {
            context?.let {
                updateAccountUIState(it, it.components.backgroundServices.accountManager.accountProfile())
            }
        }
    }

    override fun onProfileUpdated(profile: Profile) {
        lifecycleScope.launch {
            context?.let {
                updateAccountUIState(it, profile)
            }
        }
    }

    override fun onAuthenticationProblems() {
        lifecycleScope.launch {
            context?.let {
                updateAccountUIState(it, it.components.backgroundServices.accountManager.accountProfile())
            }
        }
    }

    /**
     * Updates the UI to reflect current account state.
     * Possible conditions are logged-in without problems, logged-out, and logged-in but needs to re-authenticate.
     */
    private fun updateAccountUIState(context: Context, profile: Profile?) {
        val preferenceSignIn =
            findPreference<Preference>(context.getPreferenceKey(pref_key_sign_in))
        val preferenceFirefoxAccount =
            findPreference<AccountPreference>(context.getPreferenceKey(pref_key_account))
        val preferenceFirefoxAccountAuthError =
            findPreference<AccountAuthErrorPreference>(context.getPreferenceKey(pref_key_account_auth_error))
        val accountPreferenceCategory =
            findPreference<PreferenceCategory>(context.getPreferenceKey(pref_key_account_category))

        val accountManager = requireComponents.backgroundServices.accountManager
        val account = accountManager.authenticatedAccount()

        // Signed-in, no problems.
        if (account != null && !accountManager.accountNeedsReauth()) {
            preferenceSignIn?.isVisible = false
            preferenceSignIn?.onPreferenceClickListener = null
            preferenceFirefoxAccountAuthError?.isVisible = false
            preferenceFirefoxAccount?.isVisible = true
            accountPreferenceCategory?.isVisible = true

            preferenceFirefoxAccount?.displayName = profile?.displayName
            preferenceFirefoxAccount?.email = profile?.email

            // Signed-in, need to re-authenticate.
        } else if (account != null && accountManager.accountNeedsReauth()) {
            preferenceFirefoxAccount?.isVisible = false
            preferenceFirefoxAccountAuthError?.isVisible = true
            accountPreferenceCategory?.isVisible = true

            preferenceSignIn?.isVisible = false
            preferenceSignIn?.onPreferenceClickListener = null

            preferenceFirefoxAccountAuthError?.email = profile?.email

            // Signed-out.
        } else {
            preferenceSignIn?.isVisible = true
            preferenceSignIn?.onPreferenceClickListener = getClickListenerForSignIn()
            preferenceFirefoxAccount?.isVisible = false
            preferenceFirefoxAccountAuthError?.isVisible = false
            accountPreferenceCategory?.isVisible = false
        }
    }
}
