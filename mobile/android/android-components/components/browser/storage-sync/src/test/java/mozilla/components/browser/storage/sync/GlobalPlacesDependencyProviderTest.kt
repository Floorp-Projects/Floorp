/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.storage.sync

import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

class GlobalPlacesDependencyProviderTest {

    @Before
    @After
    fun cleanUp() {
        GlobalPlacesDependencyProvider.placesStorage = null
    }

    @Test(expected = IllegalArgumentException::class)
    fun `requirePlacesStorage called without calling initialize, exception returned`() {
        GlobalPlacesDependencyProvider.requirePlacesStorage()
    }

    @Test
    fun `requirePlacesStorage called after calling initialize, placesStorage returned`() {
        val placesStorage = mock<PlacesStorage>()
        GlobalPlacesDependencyProvider.initialize(placesStorage)
        assertEquals(placesStorage, GlobalPlacesDependencyProvider.requirePlacesStorage())
    }
}
