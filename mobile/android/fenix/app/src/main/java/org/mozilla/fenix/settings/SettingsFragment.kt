/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import androidx.lifecycle.lifecycleScope
import androidx.navigation.Navigation
import androidx.navigation.fragment.findNavController
import androidx.preference.Preference
import androidx.preference.Preference.OnPreferenceClickListener
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.Config
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.R.string.pref_key_about
import org.mozilla.fenix.R.string.pref_key_accessibility
import org.mozilla.fenix.R.string.pref_key_account
import org.mozilla.fenix.R.string.pref_key_account_auth_error
import org.mozilla.fenix.R.string.pref_key_account_category
import org.mozilla.fenix.R.string.pref_key_add_private_browsing_shortcut
import org.mozilla.fenix.R.string.pref_key_data_choices
import org.mozilla.fenix.R.string.pref_key_delete_browsing_data
import org.mozilla.fenix.R.string.pref_key_delete_browsing_data_on_quit_preference
import org.mozilla.fenix.R.string.pref_key_help
import org.mozilla.fenix.R.string.pref_key_language
import org.mozilla.fenix.R.string.pref_key_leakcanary
import org.mozilla.fenix.R.string.pref_key_make_default_browser
import org.mozilla.fenix.R.string.pref_key_passwords
import org.mozilla.fenix.R.string.pref_key_privacy_link
import org.mozilla.fenix.R.string.pref_key_rate
import org.mozilla.fenix.R.string.pref_key_remote_debugging
import org.mozilla.fenix.R.string.pref_key_search_settings
import org.mozilla.fenix.R.string.pref_key_sign_in
import org.mozilla.fenix.R.string.pref_key_site_permissions
import org.mozilla.fenix.R.string.pref_key_theme
import org.mozilla.fenix.R.string.pref_key_toolbar
import org.mozilla.fenix.R.string.pref_key_tracking_protection_settings
import org.mozilla.fenix.R.string.pref_key_your_rights
import org.mozilla.fenix.components.PrivateShortcutCreateManager
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.account.AccountAuthErrorPreference
import org.mozilla.fenix.settings.account.AccountPreference
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
        requireComponents.backgroundServices.accountManager.register(
            this,
            owner = this,
            autoPause = true
        )

        // It's important to update the account UI state in onCreate, even though we also call it in onResume, since
        // that ensures we'll never display an incorrect state in the UI. For example, if user is signed-in, and we
        // don't perform this call in onCreate, we'll briefly display a "Sign In" preference, which will then get
        // replaced by the correct account information once this call is ran in onResume shortly after.
        updateAccountUIState(
            context!!,
            requireComponents.backgroundServices.accountManager.accountProfile()
        )

        preferenceManager.sharedPreferences.registerOnSharedPreferenceChangeListener(
            preferenceChangeListener
        )
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()

        showToolbar(getString(R.string.settings_title))

        update()
    }

    private fun update() {
        val trackingProtectionPreference =
            findPreference<Preference>(getPreferenceKey(pref_key_tracking_protection_settings))
        trackingProtectionPreference?.summary = context?.let {
            if (it.settings().shouldUseTrackingProtection) {
                getString(R.string.tracking_protection_on)
            } else {
                getString(R.string.tracking_protection_off)
            }
        }

        val toolbarPreference =
            findPreference<Preference>(getPreferenceKey(pref_key_toolbar))
        toolbarPreference?.summary = context?.settings()?.toolbarSettingString

        val themesPreference =
            findPreference<Preference>(getPreferenceKey(pref_key_theme))
        themesPreference?.summary = context?.settings()?.themeSettingString

        val aboutPreference = findPreference<Preference>(getPreferenceKey(pref_key_about))
        val appName = getString(R.string.app_name)
        aboutPreference?.title = getString(R.string.preferences_about, appName)

        val deleteBrowsingDataPreference =
            findPreference<Preference>(
                getPreferenceKey(
                    pref_key_delete_browsing_data_on_quit_preference
                )
            )
        deleteBrowsingDataPreference?.summary = context?.let {
            if (it.settings().shouldDeleteBrowsingDataOnQuit) {
                getString(R.string.delete_browsing_data_quit_on)
            } else {
                getString(R.string.delete_browsing_data_quit_off)
            }
        }

        findPreference<Preference>(getPreferenceKey(pref_key_add_private_browsing_shortcut))?.apply {
            isVisible =
                !PrivateShortcutCreateManager.doesPrivateBrowsingPinnedShortcutExist(context)
        }

        setupPreferences()

        updateAccountUIState(
            context!!,
            requireComponents.backgroundServices.accountManager.accountProfile()
        )

        updatePreferenceVisibilityForFeatureFlags()
    }

    private fun updatePreferenceVisibilityForFeatureFlags() {
        findPreference<Preference>(getPreferenceKey(pref_key_passwords))?.apply {
            isVisible = FeatureFlags.logins
        }
    }

    @Suppress("ComplexMethod", "LongMethod")
    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            resources.getString(pref_key_search_settings) -> {
                navigateToSearchEngineSettings()
            }
            resources.getString(pref_key_tracking_protection_settings) -> {
                requireContext().metrics.track(Event.TrackingProtectionSettings)
                navigateToTrackingProtectionSettings()
            }
            resources.getString(pref_key_site_permissions) -> {
                navigateToSitePermissions()
            }
            resources.getString(pref_key_add_private_browsing_shortcut) -> {
                requireContext().metrics.track(Event.PrivateBrowsingCreateShortcut)
                PrivateShortcutCreateManager.createPrivateShortcut(requireContext())
            }
            resources.getString(pref_key_accessibility) -> {
                navigateToAccessibility()
            }
            resources.getString(pref_key_language) -> {
                // TODO #220
                ItsNotBrokenSnack(context!!).showSnackbar(issueNumber = "220")
            }
            resources.getString(pref_key_make_default_browser) -> {
                navigateToDefaultBrowserSettingsFragment()
            }
            resources.getString(pref_key_data_choices) -> {
                navigateToDataChoices()
            }
            resources.getString(pref_key_help) -> {
                (activity as HomeActivity).openToBrowserAndLoad(
                    searchTermOrURL = SupportUtils.getSumoURLForTopic(
                        context!!,
                        SupportUtils.SumoTopic.HELP
                    ),
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
            resources.getString(pref_key_passwords) -> {
                navigateToLoginsSettingsFragment()
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
            resources.getString(pref_key_delete_browsing_data_on_quit_preference) -> {
                navigateToDeleteBrowsingDataOnQuit()
            }
            resources.getString(pref_key_theme) -> {
                navigateToThemeSettings()
            }
            resources.getString(pref_key_toolbar) -> {
                navigateToToolbarSettings()
            }
            resources.getString(pref_key_privacy_link) -> {
                requireContext().let { context ->
                    val intent = SupportUtils.createCustomTabIntent(
                        context,
                        SupportUtils.getPrivacyNoticeUrl()
                    )
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
        preferenceManager.sharedPreferences.unregisterOnSharedPreferenceChangeListener(
            preferenceChangeListener
        )
    }

    private fun getClickListenerForSignIn(): OnPreferenceClickListener {
        return OnPreferenceClickListener {
            context!!.components.services.launchPairingSignIn(context!!, findNavController())
            true
        }
    }

    private fun setupPreferences() {
        val leakKey = getPreferenceKey(pref_key_leakcanary)
        val debuggingKey = getPreferenceKey(pref_key_remote_debugging)

        val preferenceLeakCanary = findPreference<Preference>(leakKey)
        val preferenceRemoteDebugging = findPreference<Preference>(debuggingKey)

        if (!Config.channel.isReleased) {
            preferenceLeakCanary?.setOnPreferenceChangeListener { _, newValue ->
                val isEnabled = newValue == true
                context?.application?.updateLeakCanaryState(isEnabled)
                true
            }
        }

        preferenceRemoteDebugging?.setOnPreferenceChangeListener { preference, newValue ->
            preference.context.settings().preferences.edit()
                .putBoolean(preference.key, newValue as Boolean).apply()
            requireComponents.core.engine.settings.remoteDebuggingEnabled = newValue
            true
        }
    }

    private fun navigateToLoginsSettingsFragment() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToLoginsFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToSearchEngineSettings() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToSearchEngineFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToTrackingProtectionSettings() {
        val directions =
            SettingsFragmentDirections.actionSettingsFragmentToTrackingProtectionFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToThemeSettings() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToThemeFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToToolbarSettings() {
        val directions = SettingsFragmentDirections.actionSettingsFragmentToToolbarSettingsFragment()
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

    private fun navigateToDefaultBrowserSettingsFragment() {
        val directions =
            SettingsFragmentDirections.actionSettingsFragmentToDefaultBrowserSettingsFragment()
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
        val directions =
            SettingsFragmentDirections.actionSettingsFragmentToDeleteBrowsingDataFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    private fun navigateToDeleteBrowsingDataOnQuit() {
        val directions =
            SettingsFragmentDirections.actionSettingsFragmentToDeleteBrowsingDataOnQuitFragment()
        Navigation.findNavController(view!!).navigate(directions)
    }

    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
        lifecycleScope.launch {
            context?.let {
                updateAccountUIState(
                    it,
                    it.components.backgroundServices.accountManager.accountProfile()
                )
            }
        }
    }

    override fun onLoggedOut() {
        lifecycleScope.launch {
            context?.let {
                updateAccountUIState(
                    it,
                    it.components.backgroundServices.accountManager.accountProfile()
                )
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
                updateAccountUIState(
                    it,
                    it.components.backgroundServices.accountManager.accountProfile()
                )
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
            findPreference<AccountAuthErrorPreference>(
                context.getPreferenceKey(
                    pref_key_account_auth_error
                )
            )
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
