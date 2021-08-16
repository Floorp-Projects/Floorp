/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.trackingprotection

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TrackingProtectionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.SupportUtils

@RunWith(FenixRobolectricTestRunner::class)
class TrackingProtectionExceptionsInteractorTest {

    private lateinit var interactor: TrackingProtectionExceptionsInteractor

    private val activity: HomeActivity = mockk(relaxed = true)
    private val engine: Engine = mockk(relaxed = true)

    private val results: List<TrackingProtectionException> = emptyList()
    private val store = BrowserStore(
        BrowserState(
            tabs = listOf(
                // Using copy to add TP state because `createTab` doesn't support it right now.
                createTab("https://mozilla.org", false, "tab1")
                    .copy(trackingProtection = TrackingProtectionState(ignoredOnTrackingProtection = true)),
                createTab("https://firefox.com", false, "tab2")
                    .copy(trackingProtection = TrackingProtectionState(ignoredOnTrackingProtection = true))
            )
        )
    )
    private val capture =
        CaptureActionsMiddleware<ExceptionsFragmentState, ExceptionsFragmentAction>()
    private val exceptionsStore = ExceptionsFragmentStore(middlewares = listOf(capture))
    private val trackingProtectionUseCases = TrackingProtectionUseCases(store, engine)
    private val trackingStorage: TrackingProtectionExceptionStorage =
        object : TrackingProtectionExceptionStorage {
            override fun fetchAll(onResult: (List<TrackingProtectionException>) -> Unit) {
                fetchedAll = true
                onResult(results)
            }

            override fun removeAll(activeSessions: List<EngineSession>?) {
                removedAll = true
            }

            override fun add(session: EngineSession) = Unit
            override fun contains(session: EngineSession, onResult: (Boolean) -> Unit) = Unit
            override fun remove(session: EngineSession) = Unit
            override fun remove(exception: TrackingProtectionException) = Unit
            override fun restore() = Unit
        }
    private val exceptionsItem: (String) -> TrackingProtectionException = {
        object : TrackingProtectionException {
            override val url = it
        }
    }
    private var fetchedAll: Boolean = false
    private var removedAll: Boolean = false

    @Before
    fun setup() {
        interactor = DefaultTrackingProtectionExceptionsInteractor(
            activity = activity,
            exceptionsStore = exceptionsStore,
            trackingProtectionUseCases = trackingProtectionUseCases
        )

        every { engine.trackingProtectionExceptionStore } returns trackingStorage

        // Re-setting boolean checks in case they are not re-initialized per test run.
        fetchedAll = false
        removedAll = false
    }

    @Test
    fun onLearnMore() {
        interactor.onLearnMore()

        val supportUrl = SupportUtils.getGenericSumoURLForTopic(
            SupportUtils.SumoTopic.TRACKING_PROTECTION
        )
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = supportUrl,
                newTab = true,
                from = BrowserDirection.FromTrackingProtectionExceptions
            )
        }
    }

    @Test
    fun onDeleteAll() {
        interactor.onDeleteAll()

        assertTrue(removedAll)
        assertTrue(fetchedAll)

        exceptionsStore.waitUntilIdle()

        capture.assertLastAction(ExceptionsFragmentAction.Change::class) {
            assertEquals(results, it.list)
        }
    }

    @Test
    fun onDeleteOne() {
        interactor.onDeleteOne(exceptionsItem.invoke("https://mozilla.org"))

        assertTrue(fetchedAll)

        exceptionsStore.waitUntilIdle()
        store.waitUntilIdle()

        capture.assertLastAction(ExceptionsFragmentAction.Change::class) {
            assertEquals(results, it.list)
        }

        val tab = store.state.findTab("tab1")!!
        assertFalse(tab.trackingProtection.ignoredOnTrackingProtection)

        val tab2 = store.state.findTab("tab2")!!
        assertTrue(tab2.trackingProtection.ignoredOnTrackingProtection)
    }
}
