/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.fragment.app.Fragment
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.navigation.Navigator.Extras
import androidx.navigation.fragment.NavHostFragment
import io.mockk.Runs
import io.mockk.confirmVerified
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class FragmentTest {

    private val navDirections: NavDirections = mockk(relaxed = true)
    private val mockDestination = spyk(NavDestination("hi"))
    private val mockExtras: Extras = mockk(relaxed = true)
    private val mockId = 4
    private val navController = spyk(NavController(testContext))
    private val mockFragment: Fragment = mockk(relaxed = true)
    private val mockOptions: NavOptions = mockk(relaxed = true)

    @Before
    fun setup() {
        mockkStatic(NavHostFragment::class)
        every { (NavHostFragment.findNavController(mockFragment)) } returns navController
        every { (NavHostFragment.findNavController(mockFragment).currentDestination) } returns mockDestination
        every { (mockDestination.id) } returns mockId
        every { (navController.currentDestination) } returns mockDestination
        every { (NavHostFragment.findNavController(mockFragment).currentDestination?.id) } answers { (mockDestination.id) }
    }

    @Test
    fun `Test nav fun with ID and directions`() {
        every { (NavHostFragment.findNavController(mockFragment).navigate(navDirections, null)) } just Runs

        mockFragment.nav(mockId, navDirections)
        verify { (NavHostFragment.findNavController(mockFragment).currentDestination) }
        verify { (NavHostFragment.findNavController(mockFragment).navigate(navDirections, null)) }
        confirmVerified(mockFragment)
    }

    @Test
    fun `Test nav fun with ID, directions, and options`() {
        every { (NavHostFragment.findNavController(mockFragment).navigate(navDirections, mockOptions)) } just Runs

        mockFragment.nav(mockId, navDirections, mockOptions)
        verify { (NavHostFragment.findNavController(mockFragment).currentDestination) }
        verify { (NavHostFragment.findNavController(mockFragment).navigate(navDirections, mockOptions)) }
        confirmVerified(mockFragment)
    }
}
