/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import io.mockk.mockk
import mozilla.components.service.glean.config.Configuration
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test

class ConfigurationKtTest {
    @Test
    fun `GIVEN server endpoint is null THEN return the same configuration`() {
        val configuration = Configuration(httpClient = mockk())

        assertEquals(configuration, configuration.setCustomEndpointIfAvailable(null))
    }

    @Test
    fun `GIVEN server endpoint is not empty THEN make a copy of configuration with server endpoint`() {
        val configuration = Configuration(httpClient = mockk())

        assertNotEquals(configuration, configuration.setCustomEndpointIfAvailable("test"))
    }

    @Test
    fun `GIVEN server endpoint is empty THEN return the same configuration`() {
        val configuration = Configuration(httpClient = mockk())

        assertEquals(configuration, configuration.setCustomEndpointIfAvailable(""))
    }
}
