/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.confirmVerified
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.mockkStatic
import io.mockk.verify
import io.sentry.Sentry
import org.junit.Before
import org.junit.Test
import org.mozilla.fenix.components.isSentryEnabled

class NavControllerTest {

    private val currentDestId = 4
    @MockK(relaxUnitFun = true) private lateinit var navController: NavController
    @MockK private lateinit var navDirections: NavDirections
    @MockK private lateinit var mockDestination: NavDestination
    @MockK private lateinit var mockOptions: NavOptions

    @Before
    fun setUp() {
        MockKAnnotations.init(this)
        mockkStatic("io.sentry.Sentry", "org.mozilla.fenix.components.AnalyticsKt")

        every { navController.currentDestination } returns mockDestination
        every { mockDestination.id } returns currentDestId
        every { isSentryEnabled() } returns true
        every { Sentry.capture(any<String>()) } just Runs
    }

    @Test
    fun `Nav with id and directions args`() {
        navController.nav(currentDestId, navDirections)
        verify { navController.currentDestination }
        verify { navController.navigate(navDirections, null) }
    }

    @Test
    fun `Nav with id, directions, and options args`() {
        navController.nav(currentDestId, navDirections, mockOptions)
        verify { navController.currentDestination }
        verify { navController.navigate(navDirections, mockOptions) }
    }

    @Test
    fun `Test error response for id exception in-block`() {
        navController.nav(7, navDirections)
        verify { navController.currentDestination }
        verify { Sentry.capture("Fragment id 4 did not match expected 7") }
        confirmVerified(navController)
    }

    @Test
    fun `Test record id exception fun`() {
        val actual = 7
        val expected = 4

        recordIdException(actual, expected)
        verify { Sentry.capture("Fragment id 7 did not match expected 4") }
    }
}
