/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.studies

import android.widget.TextView
import androidx.appcompat.widget.SwitchCompat
import androidx.recyclerview.widget.RecyclerView
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.TestCoroutineScope
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.databinding.SettingsStudiesBinding
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class StudiesViewTest {
    @RelaxedMockK
    private lateinit var activity: HomeActivity

    @RelaxedMockK
    private lateinit var experiments: NimbusApi

    @RelaxedMockK
    private lateinit var binding: SettingsStudiesBinding

    @RelaxedMockK
    private lateinit var interactor: StudiesInteractor

    @RelaxedMockK
    private lateinit var settings: Settings

    private val testCoroutineScope = TestCoroutineScope()
    private val testDispatcher = TestCoroutineDispatcher()
    private lateinit var view: StudiesView

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        view = spyk(
            StudiesView(
                testCoroutineScope,
                testContext,
                binding,
                interactor,
                settings,
                experiments
            ) { true }
        )
    }

    @After
    fun cleanUp() {
        testCoroutineScope.cleanupTestCoroutines()
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `WHEN calling bind THEN bind all the related information`() {
        val studiesTitle = mockk<TextView>(relaxed = true)
        val studiesSwitch = mockk<SwitchCompat>(relaxed = true)
        val studiesList = mockk<RecyclerView>(relaxed = true)

        every { settings.isExperimentationEnabled } returns true
        every { view.provideStudiesTitle() } returns studiesTitle
        every { view.provideStudiesSwitch() } returns studiesSwitch
        every { view.provideStudiesList() } returns studiesList
        every { view.bindDescription() } just runs
        every { view.getSwitchTitle() } returns "Title"

        view.bind()

        verify {
            studiesTitle.text = "Title"
            studiesSwitch.isChecked = true
            view.bindDescription()
            studiesList.adapter = any()
        }
    }

    @Test
    fun `WHEN calling onRemoveButtonClicked THEN delegate to the interactor`() {
        val experiment = mockk<EnrolledExperiment>()
        val adapter = mockk<StudiesAdapter>(relaxed = true)

        every { view.adapter } returns adapter

        view.onRemoveButtonClicked(experiment)

        verify {
            interactor.removeStudy(experiment)
            adapter.removeStudy(experiment)
        }
    }
}
