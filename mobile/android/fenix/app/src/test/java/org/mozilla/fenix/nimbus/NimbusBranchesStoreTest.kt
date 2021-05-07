/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.nimbus

import io.mockk.mockk
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mozilla.experiments.nimbus.Branch

class NimbusBranchesStoreTest {

    private lateinit var nimbusBranchesState: NimbusBranchesState
    private lateinit var nimbusBranchesStore: NimbusBranchesStore

    @Before
    fun setup() {
        nimbusBranchesState = NimbusBranchesState(branches = emptyList())
        nimbusBranchesStore = NimbusBranchesStore(nimbusBranchesState)
    }

    @Test
    fun `GIVEN a new branch and selected branch WHEN UpdateBranches action is dispatched THEN state is updated`() = runBlocking {
        assertTrue(nimbusBranchesStore.state.isLoading)

        val branches: List<Branch> = listOf(mockk(), mockk())
        val selectedBranch = "control"

        nimbusBranchesStore.dispatch(NimbusBranchesAction.UpdateBranches(branches, selectedBranch))
            .join()

        assertEquals(branches, nimbusBranchesStore.state.branches)
        assertEquals(selectedBranch, nimbusBranchesStore.state.selectedBranch)
        assertFalse(nimbusBranchesStore.state.isLoading)
    }

    @Test
    fun `GIVEN a new selected branch WHEN UpdateSelectedBranch action is dispatched THEN selectedBranch state is updated`() = runBlocking {
        assertEquals("", nimbusBranchesStore.state.selectedBranch)

        val selectedBranch = "control"

        nimbusBranchesStore.dispatch(NimbusBranchesAction.UpdateSelectedBranch(selectedBranch)).join()

        assertEquals(selectedBranch, nimbusBranchesStore.state.selectedBranch)
    }
}
