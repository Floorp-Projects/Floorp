/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.studies

import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.service.nimbus.NimbusApi
import org.junit.Before
import org.junit.Test
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity

@ExperimentalCoroutinesApi
class DefaultStudiesInteractorTest {
    @RelaxedMockK
    private lateinit var activity: HomeActivity

    @RelaxedMockK
    private lateinit var experiments: NimbusApi

    private lateinit var interactor: DefaultStudiesInteractor

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        interactor = DefaultStudiesInteractor(activity, experiments)
    }

    @Test
    fun `WHEN calling openWebsite THEN delegate to the homeActivity`() {
        val url = ""
        interactor.openWebsite(url)

        verify {
            activity.openToBrowserAndLoad(url, true, BrowserDirection.FromStudiesFragment)
        }
    }

    @Test
    fun `WHEN calling removeStudy THEN delegate to the NimbusApi`() {
        val experiment = mockk<EnrolledExperiment>(relaxed = true)

        every { experiment.slug } returns "slug"

        interactor.removeStudy(experiment)

        verify {
            experiments.optOut("slug")
        }
    }
}
