/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.verify
import io.mockk.verifyOrder
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.feature.tab.collections.TabCollection
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.TabCollectionStorage
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.sessionsOfType

class DefaultTabTrayControllerTest {

    private val activity: HomeActivity = mockk(relaxed = true)
    private val navController: NavController = mockk()
    private val sessionManager: SessionManager = mockk(relaxed = true)
    private val dismissTabTray: (() -> Unit) = mockk(relaxed = true)
    private val showUndoSnackbar: ((String, SessionManager.Snapshot) -> Unit) =
        mockk(relaxed = true)
    private val registerCollectionStorageObserver: (() -> Unit) = mockk(relaxed = true)
    private val tabCollectionStorage: TabCollectionStorage = mockk(relaxed = true)
    private val tabCollection: TabCollection = mockk()
    private val cachedTabCollections: List<TabCollection> = listOf(tabCollection)
    private val currentDestination: NavDestination = mockk(relaxed = true)

    private lateinit var controller: DefaultTabTrayController

    private val session = Session(
        "mozilla.org",
        true
    )

    private val nonPrivateSession = Session(
        "mozilla.org",
        false
    )

    @Before
    fun setUp() {
        mockkStatic("org.mozilla.fenix.ext.SessionManagerKt")

        every { activity.components.core.sessionManager } returns sessionManager
        every { activity.components.core.tabCollectionStorage } returns tabCollectionStorage
        every { sessionManager.sessionsOfType(private = true) } returns listOf(session).asSequence()
        every { sessionManager.sessionsOfType(private = false) } returns listOf(nonPrivateSession).asSequence()
        every { sessionManager.createSessionSnapshot(any()) } returns SessionManager.Snapshot.Item(
            session
        )
        every { sessionManager.remove(any()) } just Runs
        every { tabCollectionStorage.cachedTabCollections } returns cachedTabCollections
        every { sessionManager.selectedSession } returns nonPrivateSession
        every { navController.navigate(any<NavDirections>()) } just Runs
        every { navController.currentDestination } returns currentDestination
        every { currentDestination.id } returns R.id.browserFragment

        controller = DefaultTabTrayController(
            activity = activity,
            navController = navController,
            dismissTabTray = dismissTabTray,
            showUndoSnackbar = showUndoSnackbar,
            registerCollectionStorageObserver = registerCollectionStorageObserver
        )
    }

    @Test
    fun onNewTabTapped() {
        controller.onNewTabTapped(private = false)

        verifyOrder {
            activity.browsingModeManager.mode = BrowsingMode.fromBoolean(false)
            navController.navigate(
                TabTrayDialogFragmentDirections.actionGlobalHome(
                    focusOnAddressBar = true
                )
            )
            dismissTabTray()
        }

        controller.onNewTabTapped(private = true)

        verifyOrder {
            activity.browsingModeManager.mode = BrowsingMode.fromBoolean(true)
            navController.navigate(
                TabTrayDialogFragmentDirections.actionGlobalHome(
                    focusOnAddressBar = true
                )
            )
            dismissTabTray()
        }
    }

    @Test
    fun onTabTrayDismissed() {
        controller.onTabTrayDismissed()

        verify {
            dismissTabTray()
        }
    }

    @Test
    fun onSaveToCollectionClicked() {
        val navDirectionsSlot = slot<NavDirections>()
        every { navController.navigate(capture(navDirectionsSlot)) } just Runs

        controller.onSaveToCollectionClicked()
        verify {
            registerCollectionStorageObserver()
            navController.navigate(capture(navDirectionsSlot))
        }

        assertTrue(navDirectionsSlot.isCaptured)
        assertEquals(
            R.id.action_global_collectionCreationFragment,
            navDirectionsSlot.captured.actionId
        )
    }

    @Test
    fun onShareTabsClicked() {
        val navDirectionsSlot = slot<NavDirections>()
        every { navController.navigate(capture(navDirectionsSlot)) } just Runs

        controller.onShareTabsClicked(private = false)

        verify {
            navController.navigate(capture(navDirectionsSlot))
        }

        assertTrue(navDirectionsSlot.isCaptured)
        assertEquals(R.id.action_global_shareFragment, navDirectionsSlot.captured.actionId)
    }

    @Test
    fun onCloseAllTabsClicked() {
        controller.onCloseAllTabsClicked(private = false)
        val snackbarMessage = activity.getString(R.string.snackbar_tabs_closed)

        verify {
            sessionManager.createSessionSnapshot(nonPrivateSession)
            sessionManager.remove(nonPrivateSession)
            showUndoSnackbar(snackbarMessage, any())
        }
    }
}
