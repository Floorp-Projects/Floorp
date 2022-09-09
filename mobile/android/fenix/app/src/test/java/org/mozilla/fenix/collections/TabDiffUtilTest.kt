/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.collections

import io.mockk.every
import io.mockk.mockk
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TabDiffUtilTest {

    @Test
    fun `list size is returned`() {
        val diffUtil = TabDiffUtil(
            old = listOf(mockk(), mockk()),
            new = listOf(mockk()),
            oldSelected = emptySet(),
            newSelected = emptySet(),
            oldHideCheckboxes = false,
            newHideCheckboxes = false,
        )

        assertEquals(2, diffUtil.oldListSize)
        assertEquals(1, diffUtil.newListSize)
    }

    @Test
    fun `single lists are the same`() {
        val tab = mockk<Tab> {
            every { sessionId } returns "abc"
        }
        val diffUtil = TabDiffUtil(
            old = listOf(tab),
            new = listOf(tab),
            oldSelected = emptySet(),
            newSelected = emptySet(),
            oldHideCheckboxes = false,
            newHideCheckboxes = false,
        )

        assertTrue(diffUtil.areItemsTheSame(0, 0))
        assertTrue(diffUtil.areContentsTheSame(0, 0))
    }

    @Test
    fun `selection affects contents`() {
        val tab = mockk<Tab> {
            every { sessionId } returns "abc"
        }
        val diffUtil = TabDiffUtil(
            old = listOf(tab),
            new = listOf(tab),
            oldSelected = emptySet(),
            newSelected = setOf(tab),
            oldHideCheckboxes = false,
            newHideCheckboxes = false,
        )

        assertTrue(diffUtil.areItemsTheSame(0, 0))
        assertFalse(diffUtil.areContentsTheSame(0, 0))
    }

    @Test
    fun `hide checkboxes affects contents`() {
        val tab = mockk<Tab> {
            every { sessionId } returns "abc"
        }
        val diffUtil = TabDiffUtil(
            old = listOf(tab),
            new = listOf(tab),
            oldSelected = setOf(tab),
            newSelected = setOf(tab),
            oldHideCheckboxes = false,
            newHideCheckboxes = true,
        )

        assertTrue(diffUtil.areItemsTheSame(0, 0))
        assertFalse(diffUtil.areContentsTheSame(0, 0))
    }

    @Test
    fun `change payload covers no change case`() {
        val tab = mockk<Tab>()
        val payload = TabDiffUtil(
            old = listOf(tab),
            new = listOf(tab),
            oldSelected = setOf(tab),
            newSelected = setOf(tab),
            oldHideCheckboxes = false,
            newHideCheckboxes = false,
        ).getChangePayload(0, 0)

        assertEquals(
            CheckChanged(
                shouldBeChecked = true,
                shouldHideCheckBox = false,
            ),
            payload,
        )
    }

    @Test
    fun `include shouldBeChecked in change payload`() {
        val tab = mockk<Tab>()
        val payload = TabDiffUtil(
            old = listOf(tab),
            new = listOf(tab),
            oldSelected = emptySet(),
            newSelected = setOf(tab),
            oldHideCheckboxes = false,
            newHideCheckboxes = false,
        ).getChangePayload(0, 0)

        assertEquals(
            CheckChanged(
                shouldBeChecked = true,
                shouldHideCheckBox = false,
            ),
            payload,
        )
    }

    @Test
    fun `include shouldBeUnchecked in change payload`() {
        val tab = mockk<Tab>()
        val payload = TabDiffUtil(
            old = listOf(tab),
            new = listOf(tab),
            oldSelected = setOf(tab),
            newSelected = emptySet(),
            oldHideCheckboxes = false,
            newHideCheckboxes = true,
        ).getChangePayload(0, 0)

        assertEquals(
            CheckChanged(
                shouldBeChecked = false,
                shouldHideCheckBox = true,
            ),
            payload,
        )
    }
}
