/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus

import io.mockk.mockk
import io.mockk.verify
import mozilla.components.service.nimbus.NimbusApi
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.mozilla.experiments.nimbus.Branch
import org.mozilla.fenix.nimbus.controller.NimbusBranchesController

class NimbusBranchesControllerTest {

    private val experiments: NimbusApi = mockk(relaxed = true)
    private val experimentId = "id"

    private lateinit var controller: NimbusBranchesController
    private lateinit var nimbusBranchesStore: NimbusBranchesStore

    @Before
    fun setup() {
        nimbusBranchesStore = NimbusBranchesStore(NimbusBranchesState(emptyList()))
        controller = NimbusBranchesController(nimbusBranchesStore, experiments, experimentId)
    }

    @Test
    fun `WHEN branch item is clicked THEN branch is opted into and selectedBranch state is updated`() {
        val branch = Branch(
            slug = "slug",
            ratio = 1
        )

        controller.onBranchItemClicked(branch)

        nimbusBranchesStore.waitUntilIdle()

        verify {
            experiments.optInWithBranch(experimentId, branch.slug)
        }

        assertEquals(branch.slug, nimbusBranchesStore.state.selectedBranch)
    }
}
