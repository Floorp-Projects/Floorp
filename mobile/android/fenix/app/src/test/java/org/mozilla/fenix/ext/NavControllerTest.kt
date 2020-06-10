/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.os.Bundle
import androidx.navigation.NavController
import androidx.navigation.NavDestination
import androidx.navigation.NavDirections
import androidx.navigation.NavOptions
import androidx.navigation.Navigator.Extras
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
    @MockK private lateinit var mockExtras: Extras
    @MockK private lateinit var mockOptions: NavOptions
    @MockK private lateinit var mockBundle: Bundle

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
    fun `Nav with id, directions, and extras args`() {
        navController.nav(currentDestId, navDirections, mockExtras)
        verify { navController.currentDestination }
        verify { navController.navigate(navDirections, mockExtras) }
    }

    @Test
    fun `Nav with id, directions, and options args`() {
        navController.nav(currentDestId, navDirections, mockOptions)
        verify { navController.currentDestination }
        verify { navController.navigate(navDirections, mockOptions) }
    }

    @Test
    fun `Nav with id, directions, options, and extras args`() {
        every { navDirections.actionId } returns 5
        every { navDirections.arguments } returns mockBundle

        navController.nav(currentDestId, navDirections, mockOptions, mockExtras)
        verify { navController.currentDestination }
        verify { navController.navigate(5, mockBundle, mockOptions, mockExtras) }
    }

    @Test
    fun `Nav with id, destId, bundle, options, and extras args`() {
        navController.nav(currentDestId, 5, mockBundle, mockOptions, mockExtras)
        verify { navController.currentDestination }
        verify { navController.navigate(5, mockBundle, mockOptions, mockExtras) }
    }

    @Test
    fun `Test error response for id exception in-block`() {
        navController.nav(7, navDirections)
        verify { navController.currentDestination }
        verify { Sentry.capture("Fragment id 4 did not match expected 7") }
        confirmVerified(navController)
    }

    @Test
    fun `Test error response for null current destination`() {
        every { navController.currentDestination } returns null
        navController.nav(7, navDirections, mockExtras)
        verify { navController.currentDestination }
        verify { Sentry.capture("Fragment id null did not match expected 7") }
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
