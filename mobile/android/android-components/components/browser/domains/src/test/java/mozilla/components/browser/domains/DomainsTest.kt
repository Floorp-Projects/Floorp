/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class DomainsTest {

    @Test
    fun loadDomains() {
        val domains = Domains.load(testContext, setOf("us"))
        Assert.assertFalse(domains.isEmpty())
        Assert.assertTrue(domains.contains("reddit.com"))
    }

    @Test
    fun loadDomainsWithDefaultCountries() {
        val domains = Domains.load(testContext)
        Assert.assertFalse(domains.isEmpty())
        // From the global list
        Assert.assertTrue(domains.contains("mozilla.org"))
    }
}
