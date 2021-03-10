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
import org.junit.After
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.SyncPreferenceView
import org.mozilla.fenix.settings.logins.fragment.SavedLoginsAuthFragmentDirections

class LoginsSyncPreferenceViewTest {

    @MockK private lateinit var syncLoginsPreference: Preference
    @MockK private lateinit var lifecycleOwner: LifecycleOwner
    @MockK private lateinit var accountManager: FxaAccountManager
    @MockK(relaxed = true) private lateinit var navController: NavController
    private lateinit var accountObserver: CapturingSlot<AccountObserver>
    private lateinit var clickListener: CapturingSlot<Preference.OnPreferenceClickListener>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        mockkConstructor(SyncEnginesStorage::class)

        accountObserver = slot()
        clickListener = slot()
        val context = mockk<Context> {
            every { getString(R.string.preferences_passwords_sync_logins_reconnect) } returns "Reconnect"
            every { getString(R.string.preferences_passwords_sync_logins_sign_in) } returns "Sign in to Sync"
            every { getString(R.string.preferences_passwords_sync_logins_on) } returns "On"
            every { getString(R.string.preferences_passwords_sync_logins_off) } returns "Off"
        }

        every { syncLoginsPreference.summary = any() } just Runs
        every { syncLoginsPreference.onPreferenceClickListener = capture(clickListener) } just Runs
        every { syncLoginsPreference.context } returns context
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

        verify { syncLoginsPreference.summary = "Reconnect" }
        assertTrue(clickListener.captured.onPreferenceClick(syncLoginsPreference))

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

        verify { syncLoginsPreference.summary = "Reconnect" }
    }

    @Test
    fun `needs login if account does not exist`() {
        every { accountManager.authenticatedAccount() } returns null
        every { accountManager.accountNeedsReauth() } returns false
        createView()

        verify { syncLoginsPreference.summary = "Sign in to Sync" }
        assertTrue(clickListener.captured.onPreferenceClick(syncLoginsPreference))

        verify {
            navController.navigate(
                SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToTurnOnSyncFragment()
            )
        }
    }

    @Test
    fun `show status for existing account`() {
        every { accountManager.authenticatedAccount() } returns mockk()
        every { accountManager.accountNeedsReauth() } returns false
        createView()

        verify { syncLoginsPreference.summary = "Off" }
        assertTrue(clickListener.captured.onPreferenceClick(syncLoginsPreference))

        verify {
            navController.navigate(
                SavedLoginsAuthFragmentDirections.actionGlobalAccountSettingsFragment()
            )
        }
    }

    @Test
    fun `show status for existing account with passwords`() {
        every { anyConstructed<SyncEnginesStorage>().getStatus() } returns mapOf(
            SyncEngine.Passwords to true
        )
        every { accountManager.authenticatedAccount() } returns mockk()
        every { accountManager.accountNeedsReauth() } returns false
        createView()

        verify { syncLoginsPreference.summary = "On" }
        assertTrue(clickListener.captured.onPreferenceClick(syncLoginsPreference))

        verify {
            navController.navigate(
                SavedLoginsAuthFragmentDirections.actionGlobalAccountSettingsFragment()
            )
        }
    }

    private fun createView() = SyncPreferenceView(
        syncPreference = syncLoginsPreference,
        lifecycleOwner = lifecycleOwner,
        accountManager = accountManager,
        syncEngine = SyncEngine.Passwords,
        onSignInToSyncClicked = {
            val directions =
                SavedLoginsAuthFragmentDirections.actionSavedLoginsAuthFragmentToTurnOnSyncFragment()
            navController.navigate(directions)
        },
        onSyncStatusClicked = {
            val directions =
                SavedLoginsAuthFragmentDirections.actionGlobalAccountSettingsFragment()
            navController.navigate(directions)
        },
        onReconnectClicked = {
            val directions =
                SavedLoginsAuthFragmentDirections.actionGlobalAccountProblemFragment()
            navController.navigate(directions)
        }
    )
}
