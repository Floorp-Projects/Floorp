/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.search

import androidx.fragment.app.Fragment
import androidx.navigation.NavBackStackEntry
import androidx.navigation.NavController
import androidx.navigation.fragment.findNavController
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.unmockkStatic
import org.junit.After
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Before
import org.junit.Test

internal class SearchDialogFragmentTest {
    private val navController: NavController = mockk()
    private val fragment = SearchDialogFragment()

    @Before
    fun setup() {
        mockkStatic("androidx.navigation.fragment.FragmentKt")
        every { any<Fragment>().findNavController() } returns navController
    }

    @After
    fun teardown() {
        unmockkStatic("androidx.navigation.fragment.FragmentKt")
    }

    @Test
    fun `GIVEN this is the only visible fragment WHEN asking for the previous destination THEN return null`() {
        every { navController.backQueue } returns ArrayDeque(listOf(getDestination(fragmentName)))

        assertNull(fragment.getPreviousDestination())
    }

    @Test
    fun `GIVEN this and FragmentB on top of this are visible WHEN asking for the previous destination THEN return null`() {
        every { navController.backQueue } returns ArrayDeque(
            listOf(
                getDestination(fragmentName),
                getDestination("FragmentB"),
            ),
        )

        assertNull(fragment.getPreviousDestination())
    }

    @Test
    fun `GIVEN FragmentA, this and FragmentB are visible WHEN asking for the previous destination THEN return FragmentA`() {
        val fragmentADestination = getDestination("FragmentA")
        every { navController.backQueue } returns ArrayDeque(
            listOf(
                fragmentADestination,
                getDestination(fragmentName),
                getDestination("FragmentB"),
            ),
        )

        assertSame(fragmentADestination, fragment.getPreviousDestination())
    }

    @Test
    fun `GIVEN FragmentA and this on top of it are visible WHEN asking for the previous destination THEN return FragmentA`() {
        val fragmentADestination = getDestination("FragmentA")
        every { navController.backQueue } returns ArrayDeque(
            listOf(
                fragmentADestination,
                getDestination(fragmentName),
            ),
        )

        assertSame(fragmentADestination, fragment.getPreviousDestination())
    }
}

private val fragmentName = SearchDialogFragment::class.java.canonicalName?.substringAfterLast('.')!!

private fun getDestination(destinationName: String): NavBackStackEntry {
    return mockk {
        every { destination } returns mockk {
            every { displayName } returns "test.id/$destinationName"
        }
    }
}
