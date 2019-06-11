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
class WhiteListTest {

    /**
     * Test setup:
     * mozilla.org: allow foo.com
     * foo.mozilla.org: additionally allow bar.com
     *
     * Test:
     * mozilla.org can only use foo.com, but foo.mozilla.org can use both foo.com and bar.com
     */
    @Test
    fun whiteList() {
        val mozillaOrg = "mozilla.org"
        val fooMozillaOrg = "foo.mozilla.org"
        val fooCom = "foo.com"
        val barCom = "bar.com"

        val fooComTrie = Trie.createRootNode()
        fooComTrie.put(fooCom.reverse())

        val barComTrie = Trie.createRootNode()
        barComTrie.put(barCom.reverse())

        val whiteList = WhiteList()
        whiteList.put(mozillaOrg.reverse(), fooComTrie)
        whiteList.put(fooMozillaOrg.reverse(), barComTrie)

        assertTrue(whiteList.contains("http://$mozillaOrg", "http://$fooCom"))
        assertFalse(whiteList.contains("http://$mozillaOrg", "http://$barCom"))
        assertTrue(whiteList.contains("http://hello.$mozillaOrg", "http://$fooCom"))
        assertFalse(whiteList.contains("http://hello.$mozillaOrg", "http://$barCom"))
        assertTrue(whiteList.contains("http://$mozillaOrg/somewhere", "http://$fooCom/somewhereElse/bla/bla"))
        assertFalse(whiteList.contains("http://$mozillaOrg/another/page.html?u=a", "http://$barCom/hello"))

        assertTrue(whiteList.contains("http://$fooMozillaOrg", "http://$fooCom"))
        assertTrue(whiteList.contains("http://$fooMozillaOrg", "http://$barCom"))
        assertTrue(whiteList.contains("http://hello.$fooMozillaOrg", "http://$fooCom"))
        assertTrue(whiteList.contains("http://hello.$fooMozillaOrg", "http://$barCom"))
        assertTrue(whiteList.contains("http://$fooMozillaOrg/somewhere", "http://$fooCom/somewhereElse/bla/bla"))
        assertTrue(whiteList.contains("http://$fooMozillaOrg/another/page.html?u=a", "http://$barCom/hello"))

        // Test some invalid inputs
        assertFalse(whiteList.contains("http://$barCom", "http://$barCom"))
        assertFalse(whiteList.contains("http://$barCom", "http://$mozillaOrg"))

        // Check we don't whitelist resources for data:
        assertFalse(whiteList.contains("data:text/html;stuff", "http://$fooCom/somewhereElse/bla/bla"))
    }

    @Test
    fun whiteListTrie() {
        val whitelist = Trie.createRootNode()
        whitelist.put("abc")

        val trie = WhiteListTrie.createRootNode()
        trie.putWhiteList("def", whitelist)
        Assert.assertNull(trie.findNode("abc"))

        val foundWhitelist = trie.findNode("def") as WhiteListTrie
        Assert.assertNotNull(foundWhitelist)
        Assert.assertNotNull(foundWhitelist.whitelist?.findNode("abc"))

        try {
            trie.putWhiteList("def", whitelist)
            fail("Expected IllegalStateException")
        } catch (e: IllegalStateException) { }
    }

    val WHITE_LIST_JSON = """{
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
        val whiteList = WhiteList.fromJson(JsonReader(StringReader(WHITE_LIST_JSON)))

        assertTrue(whiteList.contains("http://host1.com", "http://host1ads.com"))
        assertTrue(whiteList.contains("https://host1.com", "https://host1ads.de"))
        assertTrue(whiteList.contains("javascript://host1.de", "javascript://host1ads.com"))
        assertTrue(whiteList.contains("file://host1.de", "file://host1ads.de"))

        assertTrue(whiteList.contains("http://host2.com", "http://host2ads.com"))
        assertTrue(whiteList.contains("about://host2.com", "about://host2ads.de"))
        assertTrue(whiteList.contains("http://host2.de", "http://host2ads.com"))
        assertTrue(whiteList.contains("http://host2.de", "http://host2ads.de"))

        assertFalse(whiteList.contains("data://host2.de", "data://host2ads.de"))
        assertFalse(whiteList.contains("foo://host2.de", "foo://host2ads.de"))
    }
}