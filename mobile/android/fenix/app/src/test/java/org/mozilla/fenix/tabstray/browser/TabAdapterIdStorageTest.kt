/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.concept.tabstray.Tab
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import java.util.UUID

@RunWith(FenixRobolectricTestRunner::class)
class TabAdapterIdStorageTest {

    @Test
    fun `the same ID is returned when queried multiple times`() {
        val storage = TabAdapterIdStorage()
        val tab = createTab()

        val id1 = storage.getStableId(tab)
        val id2 = storage.getStableId(tab)

        assertEquals(id1, id2)
    }

    @Test
    fun `the same ID is returned when the cache is at max`() {
        val storage = TabAdapterIdStorage(2)
        val tab1 = createTab()
        val tab2 = createTab()

        val id1 = storage.getStableId(tab1)
        val id2 = storage.getStableId(tab2)
        val id1Again = storage.getStableId(tab1)

        assertEquals(id1, id1Again)
        assertNotEquals(id1, id2)
    }

    @Test
    fun `the same ID is NOT returned if the cache is over max`() {
        val storage = TabAdapterIdStorage(2)
        val tab1 = createTab()
        val tab2 = createTab()
        val tab3 = createTab()

        val id1 = storage.getStableId(tab1)
        val id2 = storage.getStableId(tab2)
        val id3 = storage.getStableId(tab3)
        val id1Again = storage.getStableId(tab1)

        assertNotEquals(id1, id1Again)
        assertNotEquals(id1, id2)
        assertNotEquals(id1, id3)
    }

    @Test
    fun `the same ID is returned if the cache is resized when full`() {
        val storage = TabAdapterIdStorage(2)
        val tab1 = createTab()
        val tab2 = createTab()
        val tab3 = createTab()

        val id1 = storage.getStableId(tab1)
        val id2 = storage.getStableId(tab2)

        storage.resizeCacheIfNeeded(3)

        val id3 = storage.getStableId(tab3)
        val id1Again = storage.getStableId(tab1)

        assertEquals(id1, id1Again)
        assertNotEquals(id1, id2)
        assertNotEquals(id1, id3)
        assertNotEquals(id2, id3)
    }
}

fun createTab() = Tab(
    UUID.randomUUID().toString(),
    "https://mozilla.org"
)
