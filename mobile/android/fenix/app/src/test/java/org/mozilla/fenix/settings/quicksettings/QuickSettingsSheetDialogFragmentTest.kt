/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import junit.framework.TestCase.assertNotSame
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.TrackingProtectionAction.TrackerBlockedAction
import mozilla.components.browser.state.action.TrackingProtectionAction.TrackerLoadedAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.content.blocking.TrackerLog
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.quicksettings.protections.ProtectionsView

@RunWith(FenixRobolectricTestRunner::class)
class QuickSettingsSheetDialogFragmentTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var lifecycleOwner: MockedLifecycleOwner
    private lateinit var fragment: QuickSettingsSheetDialogFragment
    private lateinit var store: BrowserStore

    @Before
    fun setup() {
        fragment = spyk(QuickSettingsSheetDialogFragment())
        lifecycleOwner = MockedLifecycleOwner(Lifecycle.State.STARTED)

        store = BrowserStore()
        every { fragment.view } returns mockk(relaxed = true)
        every { fragment.lifecycle } returns lifecycleOwner.lifecycle
        every { fragment.activity } returns mockk(relaxed = true)
    }

    @Test
    fun `WHEN a tracker is loaded THEN trackers view is updated`() {
        val tab = createTab("mozilla.org")

        every { fragment.provideTabId() } returns tab.id
        every { fragment.updateTrackers(any()) } returns Unit

        fragment.observeTrackersChange(store)

        addAndSelectTab(tab)

        verify(exactly = 1) {
            fragment.updateTrackers(tab)
        }

        store.dispatch(TrackerLoadedAction(tab.id, mockk())).joinBlocking()

        val updatedTab = store.state.findTab(tab.id)!!

        assertNotSame(updatedTab, tab)

        verify(exactly = 1) {
            fragment.updateTrackers(updatedTab)
        }
    }

    @Test
    fun `WHEN a tracker is blocked THEN trackers view is updated`() {
        val tab = createTab("mozilla.org")

        every { fragment.provideTabId() } returns tab.id
        every { fragment.updateTrackers(any()) } returns Unit

        fragment.observeTrackersChange(store)

        addAndSelectTab(tab)

        verify(exactly = 1) {
            fragment.updateTrackers(tab)
        }

        store.dispatch(TrackerBlockedAction(tab.id, mockk())).joinBlocking()

        val updatedTab = store.state.findTab(tab.id)!!

        assertNotSame(updatedTab, tab)

        verify(exactly = 1) {
            fragment.updateTrackers(updatedTab)
        }
    }

    @Test
    fun `GIVEN no trackers WHEN calling updateTrackers THEN hide the details section`() {
        val tab = createTab("mozilla.org")
        val trackingProtectionUseCases: TrackingProtectionUseCases = mockk(relaxed = true)
        val protectionsView: ProtectionsView = mockk(relaxed = true)

        val onComplete = slot<(List<TrackerLog>) -> Unit>()

        every { fragment.protectionsView } returns protectionsView

        every {
            trackingProtectionUseCases.fetchTrackingLogs.invoke(
                any(),
                capture(onComplete),
                any(),
            )
        }.answers { onComplete.captured.invoke(emptyList()) }

        every { fragment.provideTrackingProtectionUseCases() } returns trackingProtectionUseCases

        fragment.updateTrackers(tab)

        verify {
            protectionsView.updateDetailsSection(false)
        }
    }

    @Test
    fun `GIVEN trackers WHEN calling updateTrackers THEN show the details section`() {
        val tab = createTab("mozilla.org")
        val trackingProtectionUseCases: TrackingProtectionUseCases = mockk(relaxed = true)
        val protectionsView: ProtectionsView = mockk(relaxed = true)

        val onComplete = slot<(List<TrackerLog>) -> Unit>()

        every { fragment.protectionsView } returns protectionsView

        every {
            trackingProtectionUseCases.fetchTrackingLogs.invoke(
                any(),
                capture(onComplete),
                any(),
            )
        }.answers { onComplete.captured.invoke(listOf(TrackerLog(""))) }

        every { fragment.provideTrackingProtectionUseCases() } returns trackingProtectionUseCases

        fragment.updateTrackers(tab)

        verify {
            protectionsView.updateDetailsSection(true)
        }
    }

    private fun addAndSelectTab(tab: TabSessionState) {
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(TabListAction.SelectTabAction(tab.id)).joinBlocking()
    }

    internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        private val lifecycleRegistry = LifecycleRegistry(this).apply {
            currentState = initialState
        }

        override fun getLifecycle(): Lifecycle = lifecycleRegistry
    }
}
