/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.annotation.SuppressLint
import android.app.Activity
import android.content.ActivityNotFoundException
import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.LayoutInflater
import android.widget.Toast
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.Observer
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavDirections
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.service.glean.private.NoExtras
import mozilla.components.support.ktx.android.view.showKeyboard
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.Config
import org.mozilla.fenix.GleanMetrics.Addons
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.GleanMetrics.TrackingProtection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.AmoCollectionOverrideDialogBinding
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.navigateToNotificationsSettings
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.REQUEST_CODE_BROWSER_ROLE
import org.mozilla.fenix.ext.openSetDefaultBrowserOption
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.nimbus.MessageSurfaceId
import org.mozilla.fenix.perf.ProfilerViewModel
import org.mozilla.fenix.settings.account.AccountUiView
import org.mozilla.fenix.utils.BrowsersCache
import org.mozilla.fenix.utils.Settings
import kotlin.system.exitProcess

@Suppress("LargeClass", "TooManyFunctions")
class SettingsFragment : PreferenceFragmentCompat() {

    private val args by navArgs<SettingsFragmentArgs>()
    private lateinit var accountUiView: AccountUiView
    private val profilerViewModel: ProfilerViewModel by activityViewModels()

    private val accountObserver = object : AccountObserver {
        private fun updateAccountUi(profile: Profile? = null) {
            val context = context ?: return
            lifecycleScope.launch {
                accountUiView.updateAccountUIState(
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

    // A flag used to track if we're going through the onCreate->onStart->onResume lifecycle chain.
    // If it's set to `true`, code in `onResume` can assume that `onCreate` executed a moment prior.
    // This flag is set to `false` at the end of `onResume`.
    private var creatingFragment = true

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        accountUiView = AccountUiView(
            fragment = this,
            scope = lifecycleScope,
            accountManager = requireComponents.backgroundServices.accountManager,
            httpClient = requireComponents.core.client,
            updateFxASyncOverrideMenu = ::updateFxASyncOverrideMenu,
            updateFxAAllowDomesticChinaServerMenu = :: updateFxAAllowDomesticChinaServerMenu
        )

        // Observe account changes to keep the UI up-to-date.
        requireComponents.backgroundServices.accountManager.register(
            accountObserver,
            owner = this,
            autoPause = true
        )

        // It's important to update the account UI state in onCreate since that ensures we'll never
        // display an incorrect state in the UI. We take care to not also call it as part of onResume
        // if it was just called here (via the 'creatingFragment' flag).
        // For example, if user is signed-in, and we don't perform this call in onCreate, we'll briefly
        // display a "Sign In" preference, which will then get replaced by the correct account information
        // once this call is ran in onResume shortly after.
        accountUiView.updateAccountUIState(
            requireContext(),
            requireComponents.backgroundServices.accountManager.accountProfile()
        )

        val booleanPreferenceTelemetryAllowList = listOf(
            requireContext().getString(R.string.pref_key_show_search_suggestions),
            requireContext().getString(R.string.pref_key_remote_debugging),
            requireContext().getString(R.string.pref_key_telemetry),
            requireContext().getString(R.string.pref_key_tracking_protection),
            requireContext().getString(R.string.pref_key_search_bookmarks),
            requireContext().getString(R.string.pref_key_search_browsing_history),
            requireContext().getString(R.string.pref_key_show_clipboard_suggestions),
            requireContext().getString(R.string.pref_key_show_search_engine_shortcuts),
            requireContext().getString(R.string.pref_key_open_links_in_a_private_tab),
            requireContext().getString(R.string.pref_key_sync_logins),
            requireContext().getString(R.string.pref_key_sync_bookmarks),
            requireContext().getString(R.string.pref_key_sync_history),
            requireContext().getString(R.string.pref_key_show_voice_search),
            requireContext().getString(R.string.pref_key_show_search_suggestions_in_private)
        )

        preferenceManager.sharedPreferences
            .registerOnSharedPreferenceChangeListener(this) { sharedPreferences, key ->
                try {
                    if (key in booleanPreferenceTelemetryAllowList) {
                        val enabled = sharedPreferences.getBoolean(key, false)
                        Events.preferenceToggled.record(Events.PreferenceToggledExtra(enabled, key))
                    }
                } catch (e: ClassCastException) {
                    // The setting is not a boolean, not tracked
                }
            }

        profilerViewModel.getProfilerState().observe(
            this,
            Observer<Boolean> {
                updateProfilerUI(it)
            }
        )
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        val preferencesId = getPreferenceLayoutId()

        setPreferencesFromResource(preferencesId, rootKey)
        updateMakeDefaultBrowserPreference()
    }

    /**
     * @return The preference layout to be used depending on flags and existing experiment branches.
     * Note: Changing Settings screen before experiment is over requires changing all layouts.
     */
    private fun getPreferenceLayoutId() =
        if (isDefaultBrowserExperimentBranch()) {
            R.xml.preferences_default_browser_experiment
        } else {
            R.xml.preferences
        }

    @SuppressLint("RestrictedApi")
    override fun onResume() {
        super.onResume()

        // Use nimbus to set the title, and a trivial addition
        val nimbusValidation = FxNimbus.features.nimbusValidation.value()

        val title = nimbusValidation.settingsTitle
        val suffix = nimbusValidation.settingsPunctuation

        showToolbar("$title$suffix")

        // Account UI state is updated as part of `onCreate`. To not do it twice in a row, we only
        // update it here if we're not going through the `onCreate->onStart->onResume` lifecycle chain.
        update(shouldUpdateAccountUIState = !creatingFragment)

        requireView().findViewById<RecyclerView>(R.id.recycler_view)
            ?.hideInitialScrollBar(viewLifecycleOwner.lifecycleScope)

        if (args.preferenceToScrollTo != null) {
            scrollToPreference(args.preferenceToScrollTo)
        }
        // Consider finish of `onResume` to be the point at which we consider this fragment as 'created'.
        creatingFragment = false
    }

    override fun onDestroyView() {
        super.onDestroyView()
        accountUiView.cancel()
    }

    private fun update(shouldUpdateAccountUIState: Boolean) {
        val settings = requireContext().settings()

        val trackingProtectionPreference =
            requirePreference<Preference>(R.string.pref_key_tracking_protection_settings)
        trackingProtectionPreference.summary = if (settings.shouldUseTrackingProtection) {
            getString(R.string.tracking_protection_on)
        } else {
            getString(R.string.tracking_protection_off)
        }

        val aboutPreference = requirePreference<Preference>(R.string.pref_key_about)
        val appName = getString(R.string.app_name)
        aboutPreference.title = getString(R.string.preferences_about, appName)

        val deleteBrowsingDataPreference =
            requirePreference<Preference>(R.string.pref_key_delete_browsing_data_on_quit_preference)
        deleteBrowsingDataPreference.summary = if (settings.shouldDeleteBrowsingDataOnQuit) {
            getString(R.string.delete_browsing_data_quit_on)
        } else {
            getString(R.string.delete_browsing_data_quit_off)
        }

        val tabSettingsPreference =
            requirePreference<Preference>(R.string.pref_key_tabs)
        tabSettingsPreference.summary = context?.settings()?.getTabTimeoutString()

        val autofillPreference = requirePreference<Preference>(R.string.pref_key_credit_cards)
        autofillPreference.title = if (settings.addressFeature) {
            getString(R.string.preferences_autofill)
        } else {
            getString(R.string.preferences_credit_cards)
        }

        setupPreferences()

        if (shouldUpdateAccountUIState) {
            accountUiView.updateAccountUIState(
                requireContext(),
                requireComponents.backgroundServices.accountManager.accountProfile()
            )
        }

        updateMakeDefaultBrowserPreference()
    }

    @SuppressLint("InflateParams")
    @Suppress("ComplexMethod", "LongMethod")
    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        // Hide the scrollbar so the animation looks smoother
        val recyclerView = requireView().findViewById<RecyclerView>(R.id.recycler_view)
        recyclerView.isVerticalScrollBarEnabled = false

        val directions: NavDirections? = when (preference.key) {
            resources.getString(R.string.pref_key_sign_in) -> {
                SettingsFragmentDirections.actionSettingsFragmentToTurnOnSyncFragment()
            }
            resources.getString(R.string.pref_key_tabs) -> {
                SettingsFragmentDirections.actionSettingsFragmentToTabsSettingsFragment()
            }
            resources.getString(R.string.pref_key_home) -> {
                SettingsFragmentDirections.actionSettingsFragmentToHomeSettingsFragment()
            }
            resources.getString(R.string.pref_key_search_settings) -> {
                SettingsFragmentDirections.actionSettingsFragmentToSearchEngineFragment()
            }
            resources.getString(R.string.pref_key_tracking_protection_settings) -> {
                TrackingProtection.etpSettings.record(NoExtras())
                SettingsFragmentDirections.actionSettingsFragmentToTrackingProtectionFragment()
            }
            resources.getString(R.string.pref_key_site_permissions) -> {
                SettingsFragmentDirections.actionSettingsFragmentToSitePermissionsFragment()
            }
            resources.getString(R.string.pref_key_private_browsing) -> {
                SettingsFragmentDirections.actionSettingsFragmentToPrivateBrowsingFragment()
            }
            resources.getString(R.string.pref_key_https_only_settings) -> {
                SettingsFragmentDirections.actionSettingsFragmentToHttpsOnlyFragment()
            }
            resources.getString(R.string.pref_key_accessibility) -> {
                SettingsFragmentDirections.actionSettingsFragmentToAccessibilityFragment()
            }
            resources.getString(R.string.pref_key_language) -> {
                SettingsFragmentDirections.actionSettingsFragmentToLocaleSettingsFragment()
            }
            resources.getString(R.string.pref_key_addons) -> {
                Addons.openAddonsInSettings.record(mozilla.components.service.glean.private.NoExtras())
                SettingsFragmentDirections.actionSettingsFragmentToAddonsFragment()
            }
            resources.getString(R.string.pref_key_data_choices) -> {
                SettingsFragmentDirections.actionSettingsFragmentToDataChoicesFragment()
            }
            resources.getString(R.string.pref_key_help) -> {
                (activity as HomeActivity).openToBrowserAndLoad(
                    searchTermOrURL = SupportUtils.getSumoURLForTopic(
                        requireContext(),
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
                SettingsFragmentDirections.actionSettingsFragmentToSavedLoginsAuthFragment()
            }
            resources.getString(R.string.pref_key_credit_cards) -> {
                SettingsFragmentDirections.actionSettingsFragmentToAutofillSettingFragment()
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
            resources.getString(R.string.pref_key_notifications) -> {
                context?.navigateToNotificationsSettings()
                null
            }
            resources.getString(R.string.pref_key_customize) -> {
                SettingsFragmentDirections.actionSettingsFragmentToCustomizationFragment()
            }
            resources.getString(R.string.pref_key_privacy_link) -> {
                val intent = SupportUtils.createCustomTabIntent(
                    requireContext(),
                    SupportUtils.getMozillaPageUrl(SupportUtils.MozillaPage.PRIVATE_NOTICE)
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
            resources.getString(R.string.pref_key_debug_settings) -> {
                SettingsFragmentDirections.actionSettingsFragmentToSecretSettingsFragment()
            }
            resources.getString(R.string.pref_key_secret_debug_info) -> {
                SettingsFragmentDirections.actionSettingsFragmentToSecretInfoSettingsFragment()
            }
            resources.getString(R.string.pref_key_nimbus_experiments) -> {
                SettingsFragmentDirections.actionSettingsFragmentToNimbusExperimentsFragment()
            }
            resources.getString(R.string.pref_key_override_amo_collection) -> {
                val context = requireContext()
                val dialogView = LayoutInflater.from(context).inflate(R.layout.amo_collection_override_dialog, null)

                val binding = AmoCollectionOverrideDialogBinding.bind(dialogView)
                AlertDialog.Builder(context).apply {
                    setTitle(context.getString(R.string.preferences_customize_amo_collection))
                    setView(dialogView)
                    setNegativeButton(R.string.customize_addon_collection_cancel) { dialog: DialogInterface, _ ->
                        dialog.cancel()
                    }

                    setPositiveButton(R.string.customize_addon_collection_ok) { _, _ ->
                        context.settings().overrideAmoUser = binding.customAmoUser.text.toString()
                        context.settings().overrideAmoCollection = binding.customAmoCollection.text.toString()

                        Toast.makeText(
                            context,
                            getString(R.string.toast_customize_addon_collection_done),
                            Toast.LENGTH_LONG
                        ).show()

                        Handler(Looper.getMainLooper()).postDelayed(
                            {
                                exitProcess(0)
                            },
                            AMO_COLLECTION_OVERRIDE_EXIT_DELAY
                        )
                    }

                    binding.customAmoCollection.setText(context.settings().overrideAmoCollection)
                    binding.customAmoUser.setText(context.settings().overrideAmoUser)
                    binding.customAmoUser.requestFocus()
                    binding.customAmoUser.showKeyboard()
                    create()
                }.show()

                null
            }
            resources.getString(R.string.pref_key_start_profiler) -> {
                if (requireContext().components.core.engine.profiler?.isProfilerActive()!!) {
                    SettingsFragmentDirections.actionSettingsFragmentToStopProfilerDialog()
                } else {
                    SettingsFragmentDirections.actionSettingsFragmentToStartProfilerDialog()
                }
            }
            else -> null
        }
        directions?.let { navigateFromSettings(directions) }
        return super.onPreferenceTreeClick(preference)
    }

    private fun setupPreferences() {
        val leakKey = getPreferenceKey(R.string.pref_key_leakcanary)
        val debuggingKey = getPreferenceKey(R.string.pref_key_remote_debugging)
        val preferenceLeakCanary = findPreference<Preference>(leakKey)
        val preferenceRemoteDebugging = findPreference<Preference>(debuggingKey)
        val preferenceMakeDefaultBrowser =
            requirePreference<Preference>(R.string.pref_key_make_default_browser)
        val preferenceOpenLinksInExternalApp =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_open_links_in_external_app))
        if (!Config.channel.isReleased) {
            preferenceLeakCanary?.setOnPreferenceChangeListener { _, newValue ->
                val isEnabled = newValue == true
                context?.application?.updateLeakCanaryState(isEnabled)
                true
            }
        }

        preferenceRemoteDebugging?.isVisible = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M
        preferenceRemoteDebugging?.setOnPreferenceChangeListener<Boolean> { preference, newValue ->
            preference.context.settings().preferences.edit()
                .putBoolean(preference.key, newValue).apply()
            requireComponents.core.engine.settings.remoteDebuggingEnabled = newValue
            true
        }

        preferenceMakeDefaultBrowser.onPreferenceClickListener =
            getClickListenerForMakeDefaultBrowser()

        preferenceOpenLinksInExternalApp?.onPreferenceChangeListener = SharedPreferenceUpdater()

        val preferenceFxAOverride =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_override_fxa_server))
        val preferenceSyncOverride =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_override_sync_tokenserver))

        val syncFxAOverrideUpdater = object : StringSharedPreferenceUpdater() {
            override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                return super.onPreferenceChange(preference, newValue).also {
                    updateFxASyncOverrideMenu()
                    Toast.makeText(
                        context,
                        getString(R.string.toast_override_fxa_sync_server_done),
                        Toast.LENGTH_LONG
                    ).show()
                    Handler(Looper.getMainLooper()).postDelayed(
                        {
                            exitProcess(0)
                        },
                        FXA_SYNC_OVERRIDE_EXIT_DELAY
                    )
                }
            }
        }
        preferenceFxAOverride?.onPreferenceChangeListener = syncFxAOverrideUpdater
        preferenceSyncOverride?.onPreferenceChangeListener = syncFxAOverrideUpdater

        val preferenceStartProfiler =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_start_profiler))

        with(requireContext().settings()) {
            findPreference<Preference>(
                getPreferenceKey(R.string.pref_key_nimbus_experiments)
            )?.isVisible = showSecretDebugMenuThisSession
            findPreference<Preference>(
                getPreferenceKey(R.string.pref_key_debug_settings)
            )?.isVisible = showSecretDebugMenuThisSession
            findPreference<Preference>(
                getPreferenceKey(R.string.pref_key_secret_debug_info)
            )?.isVisible = showSecretDebugMenuThisSession
            preferenceStartProfiler?.isVisible = showSecretDebugMenuThisSession &&
                (requireContext().components.core.engine.profiler?.isProfilerActive() != null)
        }
        setupAmoCollectionOverridePreference(requireContext().settings())
        setupAllowDomesticChinaFxaServerPreference()
        setupHttpsOnlyPreferences()
    }

    /**
     * For >=Q -> Use new RoleManager API to show in-app browser switching dialog.
     * For <Q && >=N -> Navigate user to Android Default Apps Settings.
     * For <N -> Open sumo page to show user how to change default app.
     */
    private fun getClickListenerForMakeDefaultBrowser(): Preference.OnPreferenceClickListener {
        return Preference.OnPreferenceClickListener {
            activity?.openSetDefaultBrowserOption()
            true
        }
    }

    @Suppress("DEPRECATION")
    // https://github.com/mozilla-mobile/fenix/issues/19919
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        // If the user made us the default browser, update the switch
        if (resultCode == Activity.RESULT_OK && requestCode == REQUEST_CODE_BROWSER_ROLE) {
            updateMakeDefaultBrowserPreference()
        }
    }

    private fun updateMakeDefaultBrowserPreference() {
        if (!isDefaultBrowserExperimentBranch()) {
            requirePreference<DefaultBrowserPreference>(R.string.pref_key_make_default_browser).updateSwitch()
        }
    }

    private fun navigateFromSettings(directions: NavDirections) {
        view?.findNavController()?.let { navController ->
            if (navController.currentDestination?.id == R.id.settingsFragment) {
                navController.navigate(directions)
            }
        }
    }

    // Extension function for hiding the scroll bar on initial loading. We must do this so the
    // animation to the next screen doesn't animate the initial scroll bar (it ignores
    // isVerticalScrollBarEnabled being set to false).
    private fun RecyclerView.hideInitialScrollBar(scope: CoroutineScope) {
        scope.launch {
            val originalSize = scrollBarSize
            scrollBarSize = 0
            delay(SCROLL_INDICATOR_DELAY)
            scrollBarSize = originalSize
        }
    }

    private fun updateFxAAllowDomesticChinaServerMenu() {
        val settings = requireContext().settings()
        val preferenceAllowDomesticChinaServer =
            findPreference<SwitchPreference>(getPreferenceKey(R.string.pref_key_allow_domestic_china_fxa_server))
        // Only enable changes to these prefs when the user isn't connected to an account.
        val enabled =
            requireComponents.backgroundServices.accountManager.authenticatedAccount() == null
        val checked = settings.allowDomesticChinaFxaServer
        val visible = Config.channel.isMozillaOnline
        preferenceAllowDomesticChinaServer?.apply {
            isEnabled = enabled
            isChecked = checked
            isVisible = visible
        }
    }

    private fun updateFxASyncOverrideMenu() {
        val preferenceFxAOverride =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_override_fxa_server))
        val preferenceSyncOverride =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_override_sync_tokenserver))
        val settings = requireContext().settings()
        val show = settings.overrideFxAServer.isNotEmpty() ||
            settings.overrideSyncTokenServer.isNotEmpty() ||
            settings.showSecretDebugMenuThisSession
        // Only enable changes to these prefs when the user isn't connected to an account.
        val enabled =
            requireComponents.backgroundServices.accountManager.authenticatedAccount() == null
        preferenceFxAOverride?.apply {
            isVisible = show
            isEnabled = enabled
            summary = settings.overrideFxAServer.ifEmpty { null }
        }
        preferenceSyncOverride?.apply {
            isVisible = show
            isEnabled = enabled
            summary = settings.overrideSyncTokenServer.ifEmpty { null }
        }
    }

    @VisibleForTesting
    internal fun setupAmoCollectionOverridePreference(settings: Settings) {
        val preferenceAmoCollectionOverride =
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_override_amo_collection))

        val show = (
            Config.channel.isNightlyOrDebug && (
                settings.amoCollectionOverrideConfigured() || settings.showSecretDebugMenuThisSession
                )
            )
        preferenceAmoCollectionOverride?.apply {
            isVisible = show
            summary = settings.overrideAmoCollection.ifEmpty { null }
        }
    }

    private fun setupAllowDomesticChinaFxaServerPreference() {
        val allowDomesticChinaFxAServer = getPreferenceKey(R.string.pref_key_allow_domestic_china_fxa_server)
        val preferenceAllowDomesticChinaFxAServer = findPreference<SwitchPreference>(allowDomesticChinaFxAServer)
        val visible = Config.channel.isMozillaOnline

        preferenceAllowDomesticChinaFxAServer?.apply {
            isVisible = visible
        }

        if (visible) {
            preferenceAllowDomesticChinaFxAServer?.onPreferenceChangeListener =
                Preference.OnPreferenceChangeListener { preference, newValue ->
                    preference.context.settings().preferences.edit()
                        .putBoolean(preference.key, newValue as Boolean).apply()
                    updateFxAAllowDomesticChinaServerMenu()
                    Toast.makeText(
                        context,
                        getString(R.string.toast_override_fxa_sync_server_done),
                        Toast.LENGTH_LONG
                    ).show()
                    Handler(Looper.getMainLooper()).postDelayed(
                        {
                            exitProcess(0)
                        },
                        FXA_SYNC_OVERRIDE_EXIT_DELAY
                    )
                }
        }
    }

    @VisibleForTesting
    internal fun setupHttpsOnlyPreferences() {
        val httpsOnlyPreference =
            requirePreference<Preference>(R.string.pref_key_https_only_settings)
        httpsOnlyPreference.summary = context?.let {
            if (it.settings().shouldUseHttpsOnly) {
                getString(R.string.preferences_https_only_on)
            } else {
                getString(R.string.preferences_https_only_off)
            }
        }
    }

    private fun isDefaultBrowserExperimentBranch(): Boolean =
        requireContext().settings().isDefaultBrowserMessageLocation(MessageSurfaceId.SETTINGS)

    private fun isFirefoxDefaultBrowser(): Boolean {
        val browsers = BrowsersCache.all(requireContext())
        return browsers.isFirefoxDefaultBrowser
    }

    private fun updateProfilerUI(profilerStatus: Boolean) {
        if (profilerStatus) {
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_start_profiler))?.title =
                resources.getString(R.string.profiler_stop)
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_start_profiler))?.summary =
                resources.getString(R.string.profiler_running)
        } else {
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_start_profiler))?.title =
                resources.getString(R.string.preferences_start_profiler)
            findPreference<Preference>(getPreferenceKey(R.string.pref_key_start_profiler))?.summary = ""
        }
    }

    companion object {
        private const val SCROLL_INDICATOR_DELAY = 10L
        private const val FXA_SYNC_OVERRIDE_EXIT_DELAY = 2000L
        private const val AMO_COLLECTION_OVERRIDE_EXIT_DELAY = 3000L
    }
}
