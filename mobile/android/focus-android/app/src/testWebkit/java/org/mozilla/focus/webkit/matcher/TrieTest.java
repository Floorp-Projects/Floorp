package org.mozilla.focus.webkit.matcher;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.webkit.matcher.Trie.WhiteListTrie;
import org.mozilla.focus.webkit.matcher.util.FocusString;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;

@RunWith(RobolectricTestRunner.class)
public class TrieTest {

    @Test
    public void findNode() throws Exception {
        final Trie trie = Trie.createRootNode();

        assertNull(trie.findNode(FocusString.create("hello")));

        final Trie putNode = trie.put(FocusString.create("hello"));
        final Trie foundNode = trie.findNode(FocusString.create("hello"));

        assertNotNull(putNode);
        assertNotNull(foundNode);
        assertEquals(putNode, foundNode);

        // Substring matching: only works in one direction
        assertNull(trie.findNode(FocusString.create("hell")));
        assertNotNull(trie.findNode(FocusString.create("hellohello")));

        trie.put(FocusString.create("hellohello"));

        // Ensure both old and new overlapping strings can still be found
        assertNotNull(trie.findNode(FocusString.create("hello")));
        assertNotNull(trie.findNode(FocusString.create("hellohello")));
        assertNull(trie.findNode(FocusString.create("hell")));
        assertNull(trie.findNode(FocusString.create("hella")));
    }

    @Test
    public void testWhiteListTrie() {
        final WhiteListTrie trie;

        {
            final Trie whitelist = Trie.createRootNode();

            whitelist.put(FocusString.create("abc"));

            trie = WhiteListTrie.createRootNode();
            trie.putWhiteList(FocusString.create("def"), whitelist);
        }

        assertNull(trie.findNode(FocusString.create("abc")));

        // In practice EntityList uses it's own search in order to cover all possible matching notes
        // (e.g. in case we have separate whitelists for mozilla.org and foo.mozilla.org), however
        // we don't need to test that here yet.
        final Trie foundWhitelist = trie.findNode(FocusString.create("def"));
        assertNotNull(foundWhitelist);

        assertNotNull(foundWhitelist.findNode(FocusString.create("abc")));
    }
}