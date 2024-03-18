/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.studies

import android.view.View
import android.widget.TextView
import androidx.core.view.isVisible
import com.google.android.material.button.MaterialButton
import io.mockk.MockKAnnotations
import io.mockk.every
import io.mockk.impl.annotations.RelaxedMockK
import io.mockk.just
import io.mockk.mockk
import io.mockk.runs
import io.mockk.spyk
import io.mockk.verify
import junit.framework.TestCase.assertEquals
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.service.nimbus.messaging.MESSAGING_FEATURE_ID
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.experiments.nimbus.internal.EnrolledExperiment
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.settings.studies.CustomViewHolder.SectionViewHolder
import org.mozilla.fenix.settings.studies.CustomViewHolder.StudyViewHolder
import org.mozilla.fenix.settings.studies.StudiesAdapter.Section

@RunWith(FenixRobolectricTestRunner::class)
class StudiesAdapterTest {
    @RelaxedMockK
    private lateinit var delegate: StudiesAdapterDelegate

    private lateinit var adapter: StudiesAdapter
    private lateinit var studies: List<EnrolledExperiment>

    @Before
    fun setup() {
        MockKAnnotations.init(this)
        studies = emptyList()
        adapter = spyk(StudiesAdapter(delegate, studies, false))
    }

    @Test
    fun `WHEN bindSection THEN bind the section information`() {
        val holder = mockk<SectionViewHolder>()
        val section = Section(R.string.studies_active, true)
        val titleView = mockk<TextView>(relaxed = true)
        val divider = mockk<View>(relaxed = true)

        every { holder.titleView } returns titleView
        every { holder.divider } returns divider

        adapter.bindSection(holder, section)

        verify {
            titleView.setText(section.title)
            divider.isVisible = section.visibleDivider
        }
    }

    @Test
    fun `WHEN bindStudy THEN bind the study information`() {
        val holder = mockk<StudyViewHolder>()
        val study = mockk<EnrolledExperiment>()
        val titleView = spyk(TextView(testContext))
        val summaryView = mockk<TextView>(relaxed = true)
        val deleteButton = spyk(MaterialButton(testContext))

        every { study.slug } returns "slug"
        every { study.userFacingName } returns "userFacingName"
        every { study.userFacingDescription } returns "userFacingDescription"
        every { holder.titleView } returns titleView
        every { holder.summaryView } returns summaryView
        every { holder.deleteButton } returns deleteButton

        adapter = spyk(StudiesAdapter(delegate, listOf(study), false))

        every { adapter.showDeleteDialog(any(), any()) } returns mockk()

        adapter.bindStudy(holder, study)

        verify {
            titleView.text = any()
            summaryView.text = any()
        }

        deleteButton.performClick()

        verify {
            adapter.showDeleteDialog(any(), any())
        }
    }

    @Test
    fun `WHEN removeStudy THEN the study should be removed`() {
        val study = mockk<EnrolledExperiment>()

        every { study.slug } returns "slug"

        adapter = spyk(StudiesAdapter(delegate, listOf(study), false))

        every { adapter.submitList(any()) } just runs

        assertFalse(adapter.studiesMap.isEmpty())

        adapter.removeStudy(study)

        assertTrue(adapter.studiesMap.isEmpty())

        verify {
            adapter.submitList(any())
        }
    }

    @Test
    fun `WHEN calling createListWithSections THEN returns the section + experiments`() {
        val study = mockk<EnrolledExperiment>()

        every { study.slug } returns "slug"
        every { study.featureIds } returns listOf()

        adapter = spyk(StudiesAdapter(delegate, listOf(study), false))

        val list = adapter.createListWithSections(listOf(study))

        assertEquals(2, list.size)
        assertTrue(list[0] is Section)
        assertTrue(list[1] is EnrolledExperiment)
    }

    @Test
    fun `WHEN calling createListWithSections THEN returns the section + experiments, filtering messages`() {
        val study = mockk<EnrolledExperiment>()
        every { study.slug } returns "slug"
        every { study.featureIds } returns listOf("dummy")

        val message = mockk<EnrolledExperiment>()
        every { message.slug } returns "aMessage"
        every { message.featureIds } returns listOf(MESSAGING_FEATURE_ID)

        adapter = spyk(StudiesAdapter(delegate, listOf(study, message), false))

        val list = adapter.createListWithSections(listOf(study))

        assertEquals(2, list.size)
        assertTrue(list[0] is Section)
        assertTrue(list[1] is EnrolledExperiment)
    }
}
