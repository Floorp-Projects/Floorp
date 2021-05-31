package org.mozilla.fenix.settings.logins

import android.content.Context
import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import androidx.preference.Preference
import io.mockk.CapturingSlot
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkConstructor
import io.mockk.slot
import io.mockk.unmockkConstructor
import io.mockk.verify
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import mozilla.components.support.test.any
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.SyncPreference
import org.mozilla.fenix.settings.SyncPreferenceView
import org.mozilla.fenix.settings.logins.fragment.SavedLoginsAuthFragmentDirections

class SyncPreferenceViewTest {

    @MockK private lateinit var syncPreference: SyncPreference
    @MockK private lateinit var lifecycleOwner: LifecycleOwner
    @MockK private lateinit var accountManager: FxaAccountManager
    @MockK(relaxed = true) private lateinit var navController: NavController
    private lateinit var accountObserver: CapturingSlot<AccountObserver>
    private lateinit var preferenceChangeListener: CapturingSlot<Preference.OnPreferenceChangeListener>
    private lateinit var widgetVisibilitySlot: CapturingSlot<Boolean>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkConstructor(SyncEnginesStorage::class)

        accountObserver = slot()
        preferenceChangeListener = slot()
        widgetVisibilitySlot = slot()

        val context = mockk<Context> {
            every { getString(R.string.pref_key_credit_cards_sync_cards_across_devices) } returns "pref_key_credit_cards_sync_cards_across_devices"
            every { getString(R.string.preferences_credit_cards_sync_cards_across_devices) } returns "Sync cards across devices"
            every { getString(R.string.preferences_credit_cards_sync_cards) } returns "Sync cards"

            every { getString(R.string.pref_key_sync_logins) } returns "pref_key_sync_logins"
            every { getString(R.string.preferences_passwords_sync_logins) } returns "Sync logins"
            every { getString(R.string.preferences_passwords_sync_logins_across_devices) } returns "Sync logins across devices"
        }

        syncPreference = mockk {
            every { isSwitchWidgetVisible = any() } just Runs
            every { key } returns "pref_key_sync_logins"
            every { isChecked = any() } just Runs
            every { title = any() } just Runs
        }

        every { syncPreference.title = any() } just Runs
        every { syncPreference.onPreferenceChangeListener = capture(preferenceChangeListener) } just Runs
        every { syncPreference.context } returns context
        every { accountManager.register(capture(accountObserver), owner = lifecycleOwner) } just Runs
        every { anyConstructed<SyncEnginesStorage>().getStatus() } returns emptyMap()
    }

    @After
    fun teardown() {
        unmockkConstructor(SyncEnginesStorage::class)
    }

    @Test
    fun `needs reauth ui on init`() {
        every { accountManager.authenticatedAccount() } returns mockk()
        every { accountManager.accountNeedsReauth() } returns true

        createView()

        verify { syncPreference.isSwitchWidgetVisible = false }
        verify { syncPreference.title = notLoggedInTitle }
        assertFalse(preferenceChangeListener.captured.onPreferenceChange(syncPreference, any()))
        verify {
            navController.navigate(
                SavedLoginsAuthFragmentDirections.actionGlobalAccountProblemFragment()
            )
        }
    }

    @Test
    fun `needs reauth ui on init even if null account`() {
        every { accountManager.authenticatedAccount() } returns null
        every { accountManager.accountNeedsReauth() } returns true

        createView()

        verify { syncPreference.isSwitchWidgetVisible = false }
        verify { syncPreference.title = notLoggedInTitle }
        assertFalse(preferenceChangeListener.captured.onPreferenceChange(syncPreference, any()))
        verify {
            navController.navigate(
                SavedLoginsAuthFragmentDirections.actionGlobalAccountProblemFragment()
            )
        }
    }

    @Test
    fun `needs login if account does not exist`() {
        every { accountManager.authenticatedAccount() } returns null
        every { accountManager.accountNeedsReauth() } returns false

        createView()

        verify { syncPreference.isSwitchWidgetVisible = false }
        verify { syncPreference.title = notLoggedInTitle }
        assertFalse(preferenceChangeListener.captured.onPreferenceChange(syncPreference, any()))
        verify {
            navController.navigate(
                SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToTurnOnSyncFragment()
            )
        }
    }

    @Test
    fun `GIVEN LoginScreen and syncLogins true WHEN updateSyncPreferenceStatus THEN setStatus false`() {
        every { accountManager.authenticatedAccount() } returns mockk()
        every { accountManager.accountNeedsReauth() } returns false
        every { anyConstructed<SyncEnginesStorage>().getStatus() } returns mapOf(
            SyncEngine.Passwords to true
        )
        every { anyConstructed<SyncEnginesStorage>().setStatus(any(), any()) } just Runs
        every { syncPreference.setSwitchCheckedState(any()) } just Runs

        createView()

        verify { syncPreference.isSwitchWidgetVisible = true }
        verify { syncPreference.isChecked = true }
        verify { syncPreference.title = loggedInTitle }
        assertTrue(preferenceChangeListener.captured.onPreferenceChange(syncPreference, false))
        verify { anyConstructed<SyncEnginesStorage>().setStatus(any(), false) }
    }

    @Test
    fun `GIVEN LoginScreen and syncLogins false WHEN updateSyncPreferenceStatus THEN setStatus true`() {
        every { accountManager.authenticatedAccount() } returns mockk()
        every { accountManager.accountNeedsReauth() } returns false
        every { anyConstructed<SyncEnginesStorage>().getStatus() } returns mapOf(
            SyncEngine.Passwords to false
        )
        every { anyConstructed<SyncEnginesStorage>().setStatus(any(), any()) } just Runs
        every { syncPreference.setSwitchCheckedState(any()) } just Runs

        createView()

        verify { syncPreference.isSwitchWidgetVisible = true }
        verify { syncPreference.isChecked = false }
        verify { syncPreference.title = loggedInTitle }
        assertTrue(preferenceChangeListener.captured.onPreferenceChange(syncPreference, true))
        verify { anyConstructed<SyncEnginesStorage>().setStatus(any(), true) }
    }

    private fun createView() = SyncPreferenceView(
        syncPreference = syncPreference,
        lifecycleOwner = lifecycleOwner,
        accountManager = accountManager,
        syncEngine = SyncEngine.Passwords,
        loggedOffTitle = notLoggedInTitle,
        loggedInTitle = loggedInTitle,
        onSignInToSyncClicked = {
            val directions =
                SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToTurnOnSyncFragment()
            navController.navigate(directions)
        },
        onReconnectClicked = {
            val directions =
                SavedLoginsAuthFragmentDirections.actionGlobalAccountProblemFragment()
            navController.navigate(directions)
        }
    )

    companion object {
        const val notLoggedInTitle: String = "Sync logins across devices"
        const val loggedInTitle: String = "Sync logins"
    }
}
