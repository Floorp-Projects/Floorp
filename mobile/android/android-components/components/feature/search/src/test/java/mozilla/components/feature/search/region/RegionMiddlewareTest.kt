/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.region

import mozilla.components.browser.state.action.InitAction
import mozilla.components.browser.state.search.RegionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.service.location.LocationService
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.fakes.FakeClock
import mozilla.components.support.test.fakes.android.FakeContext
import mozilla.components.support.test.fakes.android.FakeSharedPreferences
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test

class RegionMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

    private lateinit var locationService: FakeLocationService
    private lateinit var clock: FakeClock
    private lateinit var regionManager: RegionManager

    @Before
    fun setUp() {
        clock = FakeClock()
        locationService = FakeLocationService()
        regionManager = RegionManager(
            context = FakeContext(),
            locationService = locationService,
            currentTime = clock::time,
            preferences = lazy { FakeSharedPreferences() },
        )
    }

    @Test
    fun `Updates region on init`() {
        val middleware = RegionMiddleware(FakeContext(), locationService, dispatcher)
        middleware.regionManager = regionManager

        locationService.region = LocationService.Region("FR", "France")

        val store = BrowserStore(
            middleware = listOf(middleware),
        )

        store.waitUntilIdle()
        middleware.updateJob?.joinBlocking()
        store.waitUntilIdle()

        assertNotEquals(RegionState.Default, store.state.search.region)
        assertEquals("FR", store.state.search.region!!.home)
        assertEquals("FR", store.state.search.region!!.current)
    }

    @Test
    fun `Uses default region if could never get updated`() {
        val middleware = RegionMiddleware(FakeContext(), locationService, dispatcher)
        middleware.regionManager = regionManager

        val store = BrowserStore(
            middleware = listOf(middleware),
        )

        store.dispatch(InitAction).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        assertEquals(RegionState.Default, store.state.search.region)
        assertEquals("XX", store.state.search.region!!.home)
        assertEquals("XX", store.state.search.region!!.current)
    }

    @Test
    fun `Dispatches cached home region and update later`() = runTestOnMain {
        val middleware = RegionMiddleware(FakeContext(), locationService, dispatcher)
        middleware.regionManager = regionManager

        locationService.region = LocationService.Region("FR", "France")
        regionManager.update()

        val store = BrowserStore(
            middleware = listOf(middleware),
        )

        store.dispatch(InitAction).joinBlocking()
        middleware.updateJob?.joinBlocking()
        store.waitUntilIdle()

        assertEquals("FR", store.state.search.region!!.home)
        assertEquals("FR", store.state.search.region!!.current)

        locationService.region = LocationService.Region("DE", "Germany")
        regionManager.update()

        store.dispatch(InitAction).joinBlocking()
        middleware.updateJob?.joinBlocking()
        store.waitUntilIdle()

        assertEquals("FR", store.state.search.region!!.home)
        assertEquals("DE", store.state.search.region!!.current)

        clock.advanceBy(1000L * 60L * 60L * 24L * 21L)

        store.dispatch(InitAction).joinBlocking()
        middleware.updateJob?.joinBlocking()
        store.waitUntilIdle()

        assertEquals("DE", store.state.search.region!!.home)
        assertEquals("DE", store.state.search.region!!.current)
    }
}
