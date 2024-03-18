/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.autofill

import android.app.KeyguardManager
import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.provider.Settings
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.annotation.VisibleForTesting
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import androidx.preference.Preference
import androidx.preference.SwitchPreference
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import mozilla.components.lib.state.ext.consumeFrom
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.sync.autofill.AutofillCreditCardsAddressesStorage
import mozilla.components.ui.widgets.withCenterAlignedButtons
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.components.StoreProvider
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.runIfFragmentIsAttached
import org.mozilla.fenix.ext.secure
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.SharedPreferenceUpdater
import org.mozilla.fenix.settings.SyncPreferenceView
import org.mozilla.fenix.settings.biometric.BiometricPromptPreferenceFragment
import org.mozilla.fenix.settings.requirePreference

/**
 * Autofill settings fragment displays a list of settings related to autofilling, adding and
 * syncing credit cards and addresses.
 */
@SuppressWarnings("TooManyFunctions")
class AutofillSettingFragment : BiometricPromptPreferenceFragment() {

    private lateinit var store: AutofillFragmentStore

    private var isAutofillStateLoaded: Boolean = false

    /**
     * List of preferences to be enabled or disabled during authentication.
     */
    private val creditCardPreferences: List<Int> = listOf(
        R.string.pref_key_credit_cards_save_and_autofill_cards,
        R.string.pref_key_credit_cards_sync_cards_across_devices,
        R.string.pref_key_credit_cards_manage_cards,
    )

    override fun unlockMessage() = getString(R.string.credit_cards_biometric_prompt_message)

    override fun navigateOnSuccess() {
        runIfFragmentIsAttached {
            viewLifecycleOwner.lifecycleScope.launch(Dispatchers.Main) {
                // Workaround for likely biometric library bug
                // https://github.com/mozilla-mobile/fenix/issues/8438
                delay(SHORT_DELAY_MS)
                navigateToCreditCardManagementFragment()
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        store = StoreProvider.get(this) {
            AutofillFragmentStore(AutofillFragmentState())
        }
        loadAutofillState()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(
            if (requireComponents.settings.addressFeature) {
                R.xml.autofill_preferences
            } else {
                R.xml.credit_cards_preferences
            },
            rootKey,
        )

        updateSaveAndAutofillCardsSwitch()

        if (requireComponents.settings.addressFeature) {
            updateSaveAndAutofillAddressesSwitch()
        }
    }

    /**
     * Updates save and autofill cards preference switch state depending on the saved user preference.
     */
    internal fun updateSaveAndAutofillCardsSwitch() {
        requirePreference<SwitchPreference>(R.string.pref_key_credit_cards_save_and_autofill_cards).apply {
            isChecked = context.settings().shouldAutofillCreditCardDetails
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }
    }

    /**
     * Updates save and autofill addresses preference switch state depending on the saved user preference.
     */
    internal fun updateSaveAndAutofillAddressesSwitch() {
        requirePreference<SwitchPreference>(R.string.pref_key_addresses_save_and_autofill_addresses).apply {
            isChecked = context.settings().shouldAutofillAddressDetails
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        loadAutofillState()
        return super.onCreateView(inflater, container, savedInstanceState)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        requirePreference<SwitchPreference>(R.string.pref_key_credit_cards_save_and_autofill_cards).summary =
            getString(R.string.preferences_credit_cards_save_and_autofill_cards_summary_2, getString(R.string.app_name))

        consumeFrom(store) { state ->
            if (requireComponents.settings.addressFeature) {
                updateAddressPreference(state.addresses.isNotEmpty(), findNavController())
            }
            updateCardManagementPreference(state.creditCards.isNotEmpty(), findNavController())
        }

        setBiometricPrompt(view, creditCardPreferences)
    }

    override fun onPause() {
        super.onPause()
        isAutofillStateLoaded = false
    }

    override fun onResume() {
        super.onResume()

        if (requireComponents.settings.addressFeature) {
            showToolbar(getString(R.string.preferences_autofill))
        } else {
            showToolbar(getString(R.string.preferences_credit_cards_2))
        }

        SyncPreferenceView(
            syncPreference = requirePreference(R.string.pref_key_credit_cards_sync_cards_across_devices),
            lifecycleOwner = viewLifecycleOwner,
            accountManager = requireComponents.backgroundServices.accountManager,
            syncEngine = SyncEngine.CreditCards,
            loggedOffTitle = requireContext()
                .getString(R.string.preferences_credit_cards_sync_cards_across_devices),
            loggedInTitle = requireContext()
                .getString(R.string.preferences_credit_cards_sync_cards),
            onSyncSignInClicked = {
                findNavController().navigate(
                    NavGraphDirections.actionGlobalTurnOnSync(entrypoint = FenixFxAEntryPoint.AutofillSetting),
                )
            },
            onReconnectClicked = {
                findNavController().navigate(
                    AutofillSettingFragmentDirections.actionGlobalAccountProblemFragment(
                        entrypoint = FenixFxAEntryPoint.AutofillSetting,
                    ),
                )
            },
        )

        togglePrefsEnabled(creditCardPreferences, true)
    }

    /**
     * Updates preferences visibility depending on addresses being already saved or not.
     */
    @VisibleForTesting
    internal fun updateAddressPreference(
        hasAddresses: Boolean,
        navController: NavController,
    ) {
        val manageAddressesPreference =
            requirePreference<Preference>(R.string.pref_key_addresses_manage_addresses)

        if (hasAddresses) {
            manageAddressesPreference.icon = null
            manageAddressesPreference.title =
                getString(R.string.preferences_addresses_manage_addresses)
        } else {
            manageAddressesPreference.setIcon(R.drawable.ic_new)
            manageAddressesPreference.title =
                getString(R.string.preferences_addresses_add_address)
        }

        manageAddressesPreference.setOnPreferenceClickListener {
            navController.navigate(
                if (hasAddresses) {
                    AutofillSettingFragmentDirections
                        .actionAutofillSettingFragmentToAddressManagementFragment()
                } else {
                    AutofillSettingFragmentDirections
                        .actionAutofillSettingFragmentToAddressEditorFragment()
                },
            )

            super.onPreferenceTreeClick(it)
        }
    }

    /**
     * Updates preferences visibility depending on credit cards being already saved or not.
     */
    @VisibleForTesting
    internal fun updateCardManagementPreference(
        hasCreditCards: Boolean,
        navController: NavController,
    ) {
        val manageSavedCardsPreference =
            requirePreference<Preference>(R.string.pref_key_credit_cards_manage_cards)

        if (hasCreditCards) {
            manageSavedCardsPreference.icon = null
            manageSavedCardsPreference.title =
                getString(R.string.preferences_credit_cards_manage_saved_cards_2)
        } else {
            manageSavedCardsPreference.setIcon(R.drawable.ic_new)
            manageSavedCardsPreference.title =
                getString(R.string.preferences_credit_cards_add_credit_card_2)
        }

        manageSavedCardsPreference.setOnPreferenceClickListener {
            if (hasCreditCards) {
                verifyCredentialsOrShowSetupWarning(requireContext(), creditCardPreferences)
            } else {
                navController.navigate(
                    AutofillSettingFragmentDirections
                        .actionAutofillSettingFragmentToCreditCardEditorFragment(),
                )
            }
            super.onPreferenceTreeClick(it)
        }
    }

    /**
     * Fetches all the addresses and credit cards from [AutofillCreditCardsAddressesStorage] and
     * updates the [AutofillFragmentState].
     */
    private fun loadAutofillState() {
        if (isAutofillStateLoaded) {
            return
        }

        lifecycleScope.launch(Dispatchers.IO) {
            val addresses = requireComponents.core.autofillStorage.getAllAddresses()
            val creditCards = requireComponents.core.autofillStorage.getAllCreditCards()

            lifecycleScope.launch(Dispatchers.Main) {
                store.dispatch(AutofillAction.UpdateAddresses(addresses))
                store.dispatch(AutofillAction.UpdateCreditCards(creditCards))
            }
        }

        isAutofillStateLoaded = true
    }

    /**
     * Shows a dialog warning to set up a pin/password when the device is not secured. This is
     * only used when BiometricPrompt is unavailable on the device.
     */
    override fun showPinDialogWarning(context: Context) {
        AlertDialog.Builder(context).apply {
            setTitle(getString(R.string.credit_cards_warning_dialog_title_2))
            setMessage(getString(R.string.credit_cards_warning_dialog_message_3))

            setNegativeButton(getString(R.string.credit_cards_warning_dialog_later)) { _: DialogInterface, _ ->
                navigateToCreditCardManagementFragment()
            }

            setPositiveButton(getString(R.string.credit_cards_warning_dialog_set_up_now)) { it: DialogInterface, _ ->
                it.dismiss()
                val intent = Intent(Settings.ACTION_SECURITY_SETTINGS)
                startActivity(intent)
            }

            create().withCenterAlignedButtons()
        }.show().secure(activity)
        context.settings().incrementSecureWarningCount()
    }

    /**
     * Shows a prompt to verify the device's pin/password and start activity based on the result.
     * This is only used when BiometricPrompt is unavailable on the device.
     *
     * @param manager The device [KeyguardManager]
     */
    @Suppress("Deprecation")
    override fun showPinVerification(manager: KeyguardManager) {
        val intent = manager.createConfirmDeviceCredentialIntent(
            getString(R.string.credit_cards_biometric_prompt_message_pin),
            getString(R.string.credit_cards_biometric_prompt_message),
        )

        startForResult.launch(intent)
    }

    private fun navigateToCreditCardManagementFragment() {
        val directions =
            AutofillSettingFragmentDirections
                .actionAutofillSettingFragmentToCreditCardsManagementFragment()
        findNavController().navigate(directions)
    }

    companion object {
        const val SHORT_DELAY_MS = 100L
    }
}
