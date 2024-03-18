/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.domains

import org.junit.Assert
import org.junit.Test

class DomainTest {
    @Test
    fun domainCreation() {
        val firstItem = Domain.create("https://mozilla.com")

        Assert.assertTrue(firstItem.protocol == "https://")
        Assert.assertFalse(firstItem.hasWww)
        Assert.assertTrue(firstItem.host == "mozilla.com")

        val secondItem = Domain.create("www.mozilla.com")

        Assert.assertTrue(secondItem.protocol == "http://")
        Assert.assertTrue(secondItem.hasWww)
        Assert.assertTrue(secondItem.host == "mozilla.com")
    }

    @Test
    fun domainCanCreateUrl() {
        val firstItem = Domain.create("https://mozilla.com")
        Assert.assertEquals("https://mozilla.com", firstItem.url)

        val secondItem = Domain.create("www.mozilla.com")
        Assert.assertEquals("http://www.mozilla.com", secondItem.url)
    }

    @Test(expected = IllegalStateException::class)
    fun domainCreationWithBadURLThrowsException() {
        Domain.create("http://www.")
    }
}
