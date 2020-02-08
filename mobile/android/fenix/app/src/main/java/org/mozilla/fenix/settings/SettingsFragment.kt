/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDirections
import androidx.navigation.findNavController
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

@Suppress("LargeClass")
class SettingsFragment : PreferenceFragmentCompat() {

    private val accountObserver = object : AccountObserver {
        private fun updateAccountUi(profile: Profile? = null) {
            val context = context ?: return
            lifecycleScope.launch {
                updateAccountUIState(
                    context = context,
                    profile = profile
                        ?: context.components.backgroundServices.accountManager.accountProfile()
                )
            }
        }

        override fun onAuthenticated(account: OAuthAccount, authType: AuthType) = updateAccountUi()
        override fun onLoggedOut() = updateAccountUi()
        override fun onProfileUpdated(profile: Profile) = updateAccountUi(profile)
        override fun onAuthenticationProblems() = updateAccountUi()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Observe account changes to keep the UI up-to-date.
        requireComponents.backgroundServices.accountManager.register(
            accountObserver,
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

        preferenceManager.sharedPreferences
            .registerOnSharedPreferenceChangeListener(this) { sharedPreferences, key ->
                try {
                    context?.let { context ->
                        context.components.analytics.metrics.track(
                            Event.PreferenceToggled(
                                key,
                                sharedPreferences.getBoolean(key, false),
                                context
                            )
                        )
                    }
                } catch (e: IllegalArgumentException) {
                    // The event is not tracked
                } catch (e: ClassCastException) {
                    // The setting is not a boolean, not tracked
                }
            }
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.preferences, rootKey)
        updatePreferenceVisibilityForFeatureFlags()
    }

    override fun onResume() {
        super.onResume()

        showToolbar(getString(R.string.settings_title))

        update()
    }

    private fun update() {
        val trackingProtectionPreference =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_tracking_protection_settings))
        trackingProtectionPreference?.summary = context?.let {
            if (it.settings().shouldUseTrackingProtection) {
                getString(R.string.tracking_protection_on)
            } else {
                getString(R.string.tracking_protection_off)
            }
        }

        val toolbarPreference =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_toolbar))
        toolbarPreference?.summary = context?.settings()?.toolbarSettingString

        val aboutPreference = findPreference<Preference>(getPreferenceKey(R.string.pref_key_about))
        val appName = getString(R.string.app_name)
        aboutPreference?.title = getString(R.string.preferences_about, appName)

        val deleteBrowsingDataPreference =
            findPreference<Preference>(
                getPreferenceKey(
                    R.string.pref_key_delete_browsing_data_on_quit_preference
                )
            )
        deleteBrowsingDataPreference?.summary = context?.let {
            if (it.settings().shouldDeleteBrowsingDataOnQuit) {
                getString(R.string.delete_browsing_data_quit_on)
            } else {
                getString(R.string.delete_browsing_data_quit_off)
            }
        }

        setupPreferences()

        updateAccountUIState(
            context!!,
            requireComponents.backgroundServices.accountManager.accountProfile()
        )
    }

    private fun updatePreferenceVisibilityForFeatureFlags() {
        findPreference<Preference>(getPreferenceKey(R.string.pref_key_passwords))?.apply {
            isVisible = FeatureFlags.logins
        }
        findPreference<Preference>(getPreferenceKey(R.string.pref_key_language))?.apply {
            isVisible = FeatureFlags.fenixLanguagePicker
        }
    }

    @Suppress("ComplexMethod", "LongMethod")
    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        val directions: NavDirections? = when (preference.key) {
            resources.getString(R.string.pref_key_search_settings) -> {
                SettingsFragmentDirections.actionSettingsFragmentToSearchEngineFragment()
            }
            resources.getString(R.string.pref_key_tracking_protection_settings) -> {
                requireContext().metrics.track(Event.TrackingProtectionSettings)
                SettingsFragmentDirections.actionSettingsFragmentToTrackingProtectionFragment()
            }
            resources.getString(R.string.pref_key_site_permissions) -> {
                SettingsFragmentDirections.actionSettingsFragmentToSitePermissionsFragment()
            }
            resources.getString(R.string.pref_key_private_browsing) -> {
                SettingsFragmentDirections.actionSettingsFragmentToPrivateBrowsingFragment()
            }
            resources.getString(R.string.pref_key_accessibility) -> {
                SettingsFragmentDirections.actionSettingsFragmentToAccessibilityFragment()
            }
            resources.getString(R.string.pref_key_language) -> {
                SettingsFragmentDirections.actionSettingsFragmentToLocaleSettingsFragment()
            }
            resources.getString(R.string.pref_key_addons) -> {
                SettingsFragmentDirections.actionSettingsFragmentToAddonsFragment()
            }
            resources.getString(R.string.pref_key_make_default_browser) -> {
                SettingsFragmentDirections.actionSettingsFragmentToDefaultBrowserSettingsFragment()
            }
            resources.getString(R.string.pref_key_data_choices) -> {
                SettingsFragmentDirections.actionSettingsFragmentToDataChoicesFragment()
            }
            resources.getString(R.string.pref_key_help) -> {
                (activity as HomeActivity).openToBrowserAndLoad(
                    searchTermOrURL = SupportUtils.getSumoURLForTopic(
                        context!!,
                        SupportUtils.SumoTopic.HELP
                    ),
                    newTab = true,
                    from = BrowserDirection.FromSettings
                )
                null
            }
            resources.getString(R.string.pref_key_rate) -> {
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
                null
            }
            resources.getString(R.string.pref_key_passwords) -> {
                SettingsFragmentDirections.actionSettingsFragmentToLoginsFragment()
            }
            resources.getString(R.string.pref_key_about) -> {
                SettingsFragmentDirections.actionSettingsFragmentToAboutFragment()
            }
            resources.getString(R.string.pref_key_account) -> {
                SettingsFragmentDirections.actionSettingsFragmentToAccountSettingsFragment()
            }
            resources.getString(R.string.pref_key_account_auth_error) -> {
                SettingsFragmentDirections.actionSettingsFragmentToAccountProblemFragment()
            }
            resources.getString(R.string.pref_key_delete_browsing_data) -> {
                SettingsFragmentDirections.actionSettingsFragmentToDeleteBrowsingDataFragment()
            }
            resources.getString(R.string.pref_key_delete_browsing_data_on_quit_preference) -> {
                SettingsFragmentDirections.actionSettingsFragmentToDeleteBrowsingDataOnQuitFragment()
            }
            resources.getString(R.string.pref_key_customize) -> {
                SettingsFragmentDirections.actionSettingsFragmentToCustomizationFragment()
            }
            resources.getString(R.string.pref_key_privacy_link) -> {
                val intent = SupportUtils.createCustomTabIntent(
                    requireContext(),
                    SupportUtils.getPrivacyNoticeUrl()
                )
                startActivity(intent)
                null
            }
            resources.getString(R.string.pref_key_your_rights) -> {
                val context = requireContext()
                val intent = SupportUtils.createCustomTabIntent(
                    context,
                    SupportUtils.getSumoURLForTopic(context, SupportUtils.SumoTopic.YOUR_RIGHTS)
                )
                startActivity(intent)
                null
            }
            else -> null
        }
        directions?.let { navigateFromSettings(directions) }
        return super.onPreferenceTreeClick(preference)
    }

    private fun getClickListenerForSignIn(): OnPreferenceClickListener {
        return OnPreferenceClickListener {
            context!!.components.services.launchPairingSignIn(context!!, findNavController())
            true
        }
    }

    private fun setupPreferences() {
        val leakKey = getPreferenceKey(R.string.pref_key_leakcanary)
        val debuggingKey = getPreferenceKey(R.string.pref_key_remote_debugging)

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

    private fun navigateFromSettings(directions: NavDirections) {
        view?.findNavController()?.let { navController ->
            if (navController.currentDestination?.id == R.id.settingsFragment) {
                navController.navigate(directions)
            }
        }
    }

    /**
     * Updates the UI to reflect current account state.
     * Possible conditions are logged-in without problems, logged-out, and logged-in but needs to re-authenticate.
     */
    private fun updateAccountUIState(context: Context, profile: Profile?) {
        val preferenceSignIn =
            findPreference<Preference>(context.getPreferenceKey(R.string.pref_key_sign_in))
        val preferenceFirefoxAccount =
            findPreference<AccountPreference>(context.getPreferenceKey(R.string.pref_key_account))
        val preferenceFirefoxAccountAuthError =
            findPreference<AccountAuthErrorPreference>(
                context.getPreferenceKey(
                    R.string.pref_key_account_auth_error
                )
            )
        val accountPreferenceCategory =
            findPreference<PreferenceCategory>(context.getPreferenceKey(R.string.pref_key_account_category))

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
