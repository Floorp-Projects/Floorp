/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.matcher

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TrieTest {

    @Test
    fun findNode() {
        val trie = Trie.createRootNode()

        assertNull(trie.findNode("hello"))
        val putNode = trie.put("hello")
        val foundNode = trie.findNode("hello")
        assertNotNull(putNode)
        assertNotNull(foundNode)
        assertEquals(putNode, foundNode)

        // Substring matching doesn't happen (except for subdomains)
        assertNull(trie.findNode("hell"))
        assertNull(trie.findNode("hellop"))

        // Ensure both old and new overlapping strings can still be found
        trie.put("hellohello")
        assertNotNull(trie.findNode("hello"))
        assertNotNull(trie.findNode("hellohello"))
        assertNull(trie.findNode("hell"))
        assertNull(trie.findNode("hellop"))

        // Domain and subdomain can be found
        trie.put("foo.com".reverse())
        assertNotNull(trie.findNode("foo.com".reverse()))
        assertNotNull(trie.findNode("bar.foo.com".reverse()))
        assertNull(trie.findNode("bar-foo.com".reverse()))
        assertNull(trie.findNode("oo.com".reverse()))
    }
}
