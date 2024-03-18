/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.engine.system.matcher

import android.util.JsonReader
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import java.io.StringReader

@RunWith(AndroidJUnit4::class)
class SafelistTest {

    /**
     * Test setup:
     * mozilla.org: allow foo.com
     * foo.mozilla.org: additionally allow bar.com
     *
     * Test:
     * mozilla.org can only use foo.com, but foo.mozilla.org can use both foo.com and bar.com
     */
    @Test
    fun safelist() {
        val mozillaOrg = "mozilla.org"
        val fooMozillaOrg = "foo.mozilla.org"
        val fooCom = "foo.com"
        val barCom = "bar.com"

        val fooComTrie = Trie.createRootNode()
        fooComTrie.put(fooCom.reverse())

        val barComTrie = Trie.createRootNode()
        barComTrie.put(barCom.reverse())

        val safelist = Safelist()
        safelist.put(mozillaOrg.reverse(), fooComTrie)
        safelist.put(fooMozillaOrg.reverse(), barComTrie)

        assertTrue(safelist.contains("http://$mozillaOrg", "http://$fooCom"))
        assertFalse(safelist.contains("http://$mozillaOrg", "http://$barCom"))
        assertTrue(safelist.contains("http://hello.$mozillaOrg", "http://$fooCom"))
        assertFalse(safelist.contains("http://hello.$mozillaOrg", "http://$barCom"))
        assertTrue(safelist.contains("http://$mozillaOrg/somewhere", "http://$fooCom/somewhereElse/bla/bla"))
        assertFalse(safelist.contains("http://$mozillaOrg/another/page.html?u=a", "http://$barCom/hello"))

        assertTrue(safelist.contains("http://$fooMozillaOrg", "http://$fooCom"))
        assertTrue(safelist.contains("http://$fooMozillaOrg", "http://$barCom"))
        assertTrue(safelist.contains("http://hello.$fooMozillaOrg", "http://$fooCom"))
        assertTrue(safelist.contains("http://hello.$fooMozillaOrg", "http://$barCom"))
        assertTrue(safelist.contains("http://$fooMozillaOrg/somewhere", "http://$fooCom/somewhereElse/bla/bla"))
        assertTrue(safelist.contains("http://$fooMozillaOrg/another/page.html?u=a", "http://$barCom/hello"))

        // Test some invalid inputs
        assertFalse(safelist.contains("http://$barCom", "http://$barCom"))
        assertFalse(safelist.contains("http://$barCom", "http://$mozillaOrg"))

        // Check we don't safelist resources for data:
        assertFalse(safelist.contains("data:text/html;stuff", "http://$fooCom/somewhereElse/bla/bla"))
    }

    @Test
    fun safelistTrie() {
        val safelist = Trie.createRootNode()
        safelist.put("abc")

        val trie = SafelistTrie.createRootNode()
        trie.putSafelist("def", safelist)
        Assert.assertNull(trie.findNode("abc"))

        val foundSafelist = trie.findNode("def") as SafelistTrie
        Assert.assertNotNull(foundSafelist)
        Assert.assertNotNull(foundSafelist.safelist?.findNode("abc"))

        try {
            trie.putSafelist("def", safelist)
            fail("Expected IllegalStateException")
        } catch (e: IllegalStateException) { }
    }

    val SAFE_LIST_JSON = """{
      "Host1": {
        "properties": [
          "host1.com",
          "host1.de"
        ],
        "resources": [
          "host1ads.com",
          "host1ads.de"
        ]
      },
      "Host2": {
        "properties": [
          "host2.com",
          "host2.de"
        ],
        "resources": [
          "host2ads.com",
          "host2ads.de"
        ]
      }
    }"""

    @Test
    fun fromJson() {
        val safelist = Safelist.fromJson(JsonReader(StringReader(SAFE_LIST_JSON)))

        assertTrue(safelist.contains("http://host1.com", "http://host1ads.com"))
        assertTrue(safelist.contains("https://host1.com", "https://host1ads.de"))
        assertTrue(safelist.contains("javascript://host1.de", "javascript://host1ads.com"))
        assertTrue(safelist.contains("file://host1.de", "file://host1ads.de"))

        assertTrue(safelist.contains("http://host2.com", "http://host2ads.com"))
        assertTrue(safelist.contains("about://host2.com", "about://host2ads.de"))
        assertTrue(safelist.contains("http://host2.de", "http://host2ads.com"))
        assertTrue(safelist.contains("http://host2.de", "http://host2ads.de"))

        assertFalse(safelist.contains("data://host2.de", "data://host2ads.de"))
        assertFalse(safelist.contains("foo://host2.de", "foo://host2ads.de"))
    }
}
