/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import kotlinx.coroutines.experimental.runBlocking
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
class DomainsTest {

    @Test
    fun testLoadDomains() = runBlocking {
        val domains = Domains.load(RuntimeEnvironment.application, setOf("us"))
        Assert.assertFalse(domains.isEmpty())
        Assert.assertTrue(domains.contains("google.com"))
    }
}