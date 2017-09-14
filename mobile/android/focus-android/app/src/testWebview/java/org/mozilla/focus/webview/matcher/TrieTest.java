package org.mozilla.focus.webview.matcher;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.webview.matcher.Trie.WhiteListTrie;
import org.mozilla.focus.webview.matcher.util.FocusString;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

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

        // Substring matching: doesn't happen (except for subdomains, we test those later)
        assertNull(trie.findNode(FocusString.create("hell")));
        assertNull(trie.findNode(FocusString.create("hellop")));

        trie.put(FocusString.create("hellohello"));

        // Ensure both old and new overlapping strings can still be found
        assertNotNull(trie.findNode(FocusString.create("hello")));
        assertNotNull(trie.findNode(FocusString.create("hellohello")));

        // These still don't match:
        assertNull(trie.findNode(FocusString.create("hell")));
        assertNull(trie.findNode(FocusString.create("hellop")));

        // Domain specific / partial domain tests:
        trie.put(FocusString.create("foo.com").reverse());

        // Domain and subdomain can be found
        assertNotNull(trie.findNode(FocusString.create("foo.com").reverse()));
        assertNotNull(trie.findNode(FocusString.create("bar.foo.com").reverse()));
        // But other domains with some overlap don't match
        assertNull(trie.findNode(FocusString.create("bar-foo.com").reverse()));
        assertNull(trie.findNode(FocusString.create("oo.com").reverse()));
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
        final WhiteListTrie foundWhitelist = (WhiteListTrie) trie.findNode(FocusString.create("def"));
        assertNotNull(foundWhitelist);

        assertNotNull(foundWhitelist.whitelist.findNode(FocusString.create("abc")));
    }
}