/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TabPartitionTest {

    @Test
    fun `GIVEN a null tab partition THEN tab partition is empty`() {
        val tabPartition: TabPartition? = null

        assertTrue(tabPartition.isEmpty())
        assertFalse(tabPartition.isNotEmpty())
    }

    @Test
    fun `GIVEN a tab partition with no tab group THEN tab partition is empty`() {
        val tabPartition = TabPartition("test")

        assertTrue(tabPartition.isEmpty())
        assertFalse(tabPartition.isNotEmpty())
    }

    @Test
    fun `GIVEN a tab partition with empty tab groups THEN tab partition is empty`() {
        val tabPartition = TabPartition("test", listOf(TabGroup(), TabGroup()))

        assertTrue(tabPartition.isEmpty())
        assertFalse(tabPartition.isNotEmpty())
    }

    @Test
    fun `GIVEN a tab partition with non-empty tab group THEN tab partition is not empty`() {
        val tabPartition = TabPartition("test", listOf(TabGroup("test", "test", listOf("tab1"))))

        assertTrue(tabPartition.isNotEmpty())
        assertFalse(tabPartition.isEmpty())
    }

    @Test
    fun `GIVEN a tab partition with non-empty tab group THEN get group by name will return the group`() {
        val tabPartition = TabPartition("test", listOf(TabGroup("test id", "abc", listOf("tab1", "tab2"))))

        assertTrue(tabPartition.getGroupByName("abc") != null)
        assertEquals(listOf("tab1", "tab2"), tabPartition.getGroupByName("abc")?.tabIds)
        assertTrue(tabPartition.isNotEmpty())
        assertFalse(tabPartition.isEmpty())
    }

    @Test
    fun `GIVEN a tab partition with non-empty tab group THEN get group by ID will return the group`() {
        val tabPartition = TabPartition("test", listOf(TabGroup("test id", "abc", listOf("tab1", "tab2"))))

        assertTrue(tabPartition.getGroupById("test id") != null)
        assertEquals(listOf("tab1", "tab2"), tabPartition.getGroupById("test id")?.tabIds)
        assertTrue(tabPartition.isNotEmpty())
        assertFalse(tabPartition.isEmpty())
    }
}
