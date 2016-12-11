package org.mozilla.focus.webkit.matcher;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.*;

/**
 * Integration test to make sure all our whitelisting methods work as expected.
 */
@RunWith(RobolectricTestRunner.class)
public class EntityListTest {


    // TODO: we might want to clean up the mess of revhost vs normal host vs inserting a whitelist
    // item vs inserting a whitelist trie. And that isWhiteListed relies on domains, the rest doesn't
    @Test
    public void testWhitelist() {
        final String mozillaOrg = "mozilla.org";
        final String fooMozillaOrg = "foo.mozilla.org";
        final String fooCom = "foo.com";
        final String barCom = "bar.com";

        final EntityList entityList = new EntityList();

        // We set up the following data and test that matches function as expected:
        // mozilla.org - allow all from foo.com
        // foo.mozilla.org - additionally allow from bar.com
        // Thus mozilla.org can only use foo.com, but foo.mozilla.org can use foo.com and bar.com

        final Trie fooComTrie = Trie.createRootNode();
        fooComTrie.put(new StringBuilder(fooCom).reverse().toString());

        final Trie barComTrie = Trie.createRootNode();
        barComTrie.put(new StringBuilder(barCom).reverse().toString());

        entityList.putWhiteList(new StringBuilder(mozillaOrg).reverse().toString(), fooComTrie);
        entityList.putWhiteList(new StringBuilder(fooMozillaOrg).reverse().toString(), barComTrie);

        assertTrue(entityList.isWhiteListed("http://" + mozillaOrg, "http://" + fooCom));
        assertFalse(entityList.isWhiteListed("http://" + mozillaOrg, "http://" + barCom));

        assertTrue(entityList.isWhiteListed("http://" + fooMozillaOrg, "http://" + fooCom));
        assertTrue(entityList.isWhiteListed("http://" + fooMozillaOrg, "http://" + barCom));

        // Test some junk inputs to make sure we haven't messed up
        assertFalse(entityList.isWhiteListed("http://" + barCom, "http://" + barCom));
        assertFalse(entityList.isWhiteListed("http://" + barCom, "http://" + mozillaOrg));

        // Test some made up subdomains to ensure they still match *.foo.mozilla.org
        assertTrue(entityList.isWhiteListed("http://" + "hello." + fooMozillaOrg, "http://" + fooCom));
        assertTrue(entityList.isWhiteListed("http://" + "hello." + fooMozillaOrg, "http://" + barCom));

        // And that these only match *.mozilla.org
        assertTrue(entityList.isWhiteListed("http://" + "hello." + mozillaOrg, "http://" + fooCom));
        assertFalse(entityList.isWhiteListed("http://" + "hello." + mozillaOrg, "http://" + barCom));

        // And random subpages don't fail:
        assertTrue(entityList.isWhiteListed("http://" + mozillaOrg + "/somewhere", "http://" + fooCom + "/somewhereElse/bla/bla"));
        assertFalse(entityList.isWhiteListed("http://" + mozillaOrg + "/another/page.html?u=a", "http://" + barCom + "/hello"));
        assertTrue(entityList.isWhiteListed("http://" + fooMozillaOrg + "/somewhere", "http://" + fooCom + "/somewhereElse/bla/bla"));
        assertTrue(entityList.isWhiteListed("http://" + fooMozillaOrg + "/another/page.html?u=a", "http://" + barCom + "/hello"));
    }

}