/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.trackingprotection

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifySequence
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.concept.engine.content.blocking.TrackingProtectionExceptionStorage
import mozilla.components.feature.session.TrackingProtectionUseCases
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.settings.SupportUtils

class TrackingProtectionExceptionsInteractorTest {

    @MockK(relaxed = true) private lateinit var activity: HomeActivity
    private lateinit var interactor: TrackingProtectionExceptionsInteractor

    private val results: List<TrackingProtectionException> = emptyList()
    private val engine: Engine = mockk(relaxed = true)
    private val store = BrowserStore()
    private val capture = CaptureActionsMiddleware<ExceptionsFragmentState, ExceptionsFragmentAction>()
    private val exceptionsStore = ExceptionsFragmentStore(middlewares = listOf(capture))
    private val trackingProtectionUseCases = TrackingProtectionUseCases(store, engine)
    private val trackingStorage: TrackingProtectionExceptionStorage = mockk(relaxed = true)

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        interactor = DefaultTrackingProtectionExceptionsInteractor(
            activity = activity,
            exceptionsStore = exceptionsStore,
            trackingProtectionUseCases = trackingProtectionUseCases
        )

        every { engine.trackingProtectionExceptionStore } returns trackingStorage
        every { trackingStorage.fetchAll(any()) } answers {
            firstArg<(List<TrackingProtectionException>) -> Unit>()(results)
        }
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

        verifySequence {
            trackingStorage.removeAll(any())
            trackingStorage.fetchAll(any())
        }

        exceptionsStore.waitUntilIdle()

        capture.assertLastAction(ExceptionsFragmentAction.Change::class) {
            assertEquals(results, it.list)
        }
    }

    @Test
    fun onDeleteOne() {
        val exceptionsItem = mockk<TrackingProtectionException>()
        interactor.onDeleteOne(exceptionsItem)

        verifySequence {
            trackingStorage.remove(exceptionsItem)
            trackingStorage.fetchAll(any())
        }

        exceptionsStore.waitUntilIdle()

        capture.assertLastAction(ExceptionsFragmentAction.Change::class) {
            assertEquals(results, it.list)
        }
    }
}
