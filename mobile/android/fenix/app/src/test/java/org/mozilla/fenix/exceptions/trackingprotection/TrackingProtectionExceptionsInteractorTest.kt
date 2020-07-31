/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.exceptions.trackingprotection

import io.mockk.CapturingSlot
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import io.mockk.verifySequence
import mozilla.components.concept.engine.content.blocking.TrackingProtectionException
import mozilla.components.feature.session.TrackingProtectionUseCases
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.settings.SupportUtils

class TrackingProtectionExceptionsInteractorTest {

    @MockK(relaxed = true) private lateinit var activity: HomeActivity
    @MockK(relaxed = true) private lateinit var exceptionsStore: ExceptionsFragmentStore
    @MockK(relaxed = true) private lateinit var trackingProtectionUseCases: TrackingProtectionUseCases
    private lateinit var interactor: TrackingProtectionExceptionsInteractor
    private lateinit var onResult: CapturingSlot<(List<TrackingProtectionException>) -> Unit>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        interactor = DefaultTrackingProtectionExceptionsInteractor(
            activity = activity,
            exceptionsStore = exceptionsStore,
            trackingProtectionUseCases = trackingProtectionUseCases
        )

        onResult = slot()
        every { trackingProtectionUseCases.fetchExceptions(capture(onResult)) } just Runs
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
            trackingProtectionUseCases.removeAllExceptions()
            trackingProtectionUseCases.fetchExceptions(any())
        }

        val results = mockk<List<TrackingProtectionException>>()
        onResult.captured(results)
        verify { exceptionsStore.dispatch(ExceptionsFragmentAction.Change(results)) }
    }

    @Test
    fun onDeleteOne() {
        val exceptionsItem = mockk<TrackingProtectionException>()
        interactor.onDeleteOne(exceptionsItem)
        verifySequence {
            trackingProtectionUseCases.removeException(exceptionsItem)
            trackingProtectionUseCases.fetchExceptions(any())
        }

        val results = mockk<List<TrackingProtectionException>>()
        onResult.captured(results)
        verify { exceptionsStore.dispatch(ExceptionsFragmentAction.Change(results)) }
    }
}
