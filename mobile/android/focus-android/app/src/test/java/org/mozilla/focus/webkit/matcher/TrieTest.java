package org.mozilla.focus.webkit.matcher;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.webkit.matcher.Trie.WhiteListTrie;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class TrieTest {

    @Test
    public void findNode() throws Exception {
        final Trie trie = Trie.createRootNode();

        assertNull(trie.findNode("hello"));

        final Trie putNode = trie.put("hello");
        final Trie foundNode = trie.findNode("hello");

        assertNotNull(putNode);
        assertNotNull(foundNode);
        assertEquals(putNode, foundNode);

        // Substring matching: only works in one direction
        assertNull(trie.findNode("hell"));
        assertNotNull(trie.findNode("hellohello"));

        trie.put("hellohello");

        // Ensure both old and new overlapping strings can still be found
        assertNotNull(trie.findNode("hello"));
        assertNotNull(trie.findNode("hellohello"));
        assertNull(trie.findNode("hell"));
        assertNull(trie.findNode("hella"));
    }

    @Test
    public void testWhiteListTrie() {
        final WhiteListTrie trie;

        {
            final Trie whitelist = Trie.createRootNode();

            whitelist.put("abc");

            trie = WhiteListTrie.createRootNode();
            trie.putWhiteList("def", whitelist);
        }

        assertNull(trie.findNode("abc"));

        // In practice EntityList uses it's own search in order to cover all possible matching notes
        // (e.g. in case we have separate whitelists for mozilla.org and foo.mozilla.org), however
        // we don't need to test that here yet.
        final Trie foundWhitelist = trie.findNode("def");
        assertNotNull(foundWhitelist);

        assertNotNull(foundWhitelist.findNode("abc"));
    }
}