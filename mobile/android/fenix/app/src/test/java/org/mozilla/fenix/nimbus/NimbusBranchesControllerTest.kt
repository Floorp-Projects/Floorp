/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus

import android.R
import android.app.Activity
import android.content.Context
import android.view.View
import android.view.ViewGroup
import androidx.navigation.NavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.verify
import io.mockk.verifyAll
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.experiments.nimbus.Branch
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.getRootView
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.nimbus.controller.NimbusBranchesController
import org.mozilla.fenix.utils.Settings

class NimbusBranchesControllerTest {

    private val experiments: NimbusApi = mockk(relaxed = true)
    private val experimentId = "id"

    private lateinit var controller: NimbusBranchesController
    private lateinit var navController: NavController
    private lateinit var nimbusBranchesStore: NimbusBranchesStore
    private lateinit var settings: Settings
    private lateinit var activity: Context
    private lateinit var components: Components
    private lateinit var snackbar: FenixSnackbar
    private lateinit var rootView: View

    @Before
    fun setup() {
        components = mockk(relaxed = true)
        settings = mockk(relaxed = true)
        snackbar = mockk(relaxed = true)
        navController = mockk(relaxed = true)

        rootView = mockk<ViewGroup>(relaxed = true)
        activity = mockk<Activity>(relaxed = true) {
            every { findViewById<View>(R.id.content) } returns rootView
            every { getRootView() } returns rootView
        }

        mockkObject(FenixSnackbar)
        every { FenixSnackbar.make(any(), any(), any(), any()) } returns snackbar

        every { activity.settings() } returns settings

        every { navController.currentDestination } returns mockk {
            every { id } returns org.mozilla.fenix.R.id.nimbusBranchesFragment
        }

        nimbusBranchesStore = NimbusBranchesStore(NimbusBranchesState(emptyList()))
        controller = NimbusBranchesController(
            activity,
            navController,
            nimbusBranchesStore,
            experiments,
            experimentId,
        )
    }

    @Test
    fun `WHEN branch item is clicked THEN branch is opted into and selectedBranch state is updated`() {
        every { settings.isTelemetryEnabled } returns true
        every { settings.isExperimentationEnabled } returns true

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optInWithBranch(experimentId, branch.slug)
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }

    @Test
    fun `WHEN branch item is clicked THEN branch is opted out and selectedBranch state is updated`() {
        every { settings.isTelemetryEnabled } returns true
        every { settings.isExperimentationEnabled } returns true
        every { experiments.getExperimentBranch(experimentId) } returns "slug"

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optOut(experimentId)
        }
    }

    @Test
    fun `WHEN studies and telemetry are ON and item is clicked THEN branch is opted in`() {
        every { settings.isTelemetryEnabled } returns true
        every { settings.isExperimentationEnabled } returns true

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optInWithBranch(experimentId, branch.slug)
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }

    @Test
    fun `WHEN studies and telemetry are Off THEN branch is opted in AND data is not sent`() {
        every { settings.isTelemetryEnabled } returns false
        every { settings.isExperimentationEnabled } returns false
        every { activity.getString(any()) } returns "hello"

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verifyAll {
            experiments.getExperimentBranch(experimentId)
            experiments.optInWithBranch(experimentId, branch.slug)
            snackbar.setText("hello")
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }

    @Test
    fun `WHEN studies are ON and telemetry Off THEN branch is opted in`() {
        every { settings.isExperimentationEnabled } returns true
        every { settings.isTelemetryEnabled } returns false

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optInWithBranch(experimentId, branch.slug)
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }

    @Test
    fun `WHEN studies are OFF and telemetry ON THEN branch is opted in`() {
        every { settings.isExperimentationEnabled } returns false
        every { settings.isTelemetryEnabled } returns true

        val branch = Branch(
            slug = "slug",
            ratio = 1,
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optInWithBranch(experimentId, branch.slug)
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }
}
