package org.mozilla.focus.webview.matcher;

import android.net.Uri;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.webview.matcher.util.FocusString;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

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
        fooComTrie.put(FocusString.create(fooCom).reverse());

        final Trie barComTrie = Trie.createRootNode();
        barComTrie.put(FocusString.create(barCom).reverse());

        entityList.putWhiteList(FocusString.create(mozillaOrg).reverse(), fooComTrie);
        entityList.putWhiteList(FocusString.create(fooMozillaOrg).reverse(), barComTrie);

        assertTrue(entityList.isWhiteListed(Uri.parse("http://" + mozillaOrg), Uri.parse("http://" + fooCom)));
        assertFalse(entityList.isWhiteListed(Uri.parse("http://" + mozillaOrg), Uri.parse("http://" + barCom)));

        assertTrue(entityList.isWhiteListed(Uri.parse("http://" + fooMozillaOrg), Uri.parse("http://" + fooCom)));
        assertTrue(entityList.isWhiteListed(Uri.parse("http://" + fooMozillaOrg), Uri.parse("http://" + barCom)));

        // Test some junk inputs to make sure we haven't messed up
        assertFalse(entityList.isWhiteListed(Uri.parse("http://" + barCom), Uri.parse("http://" + barCom)));
        assertFalse(entityList.isWhiteListed(Uri.parse("http://" + barCom), Uri.parse("http://" + mozillaOrg)));

        // Test some made up subdomains to ensure they still match *.foo.mozilla.org
        assertTrue(entityList.isWhiteListed(Uri.parse("http://" + "hello." + fooMozillaOrg), Uri.parse("http://" + fooCom)));
        assertTrue(entityList.isWhiteListed(Uri.parse("http://" + "hello." + fooMozillaOrg), Uri.parse("http://" + barCom)));

        // And that these only match *.mozilla.org
        assertTrue(entityList.isWhiteListed(Uri.parse("http://" + "hello." + mozillaOrg), Uri.parse("http://" + fooCom)));
        assertFalse(entityList.isWhiteListed(Uri.parse("http://" + "hello." + mozillaOrg), Uri.parse("http://" + barCom)));

        // And random subpages don't fail:
        assertTrue(entityList.isWhiteListed(Uri.parse("http://" + mozillaOrg + "/somewhere"), Uri.parse("http://" + fooCom + "/somewhereElse/bla/bla")));
        assertFalse(entityList.isWhiteListed(Uri.parse("http://" + mozillaOrg + "/another/page.html?u=a"), Uri.parse("http://" + barCom + "/hello")));
        assertTrue(entityList.isWhiteListed(Uri.parse("http://" + fooMozillaOrg + "/somewhere"), Uri.parse("http://" + fooCom + "/somewhereElse/bla/bla")));
        assertTrue(entityList.isWhiteListed(Uri.parse("http://" + fooMozillaOrg + "/another/page.html?u=a"), Uri.parse("http://" + barCom + "/hello")));

        // Check we don't whitelist resources from data: pages
        assertFalse(entityList.isWhiteListed(Uri.parse("data:text/html;stuff"), Uri.parse("http://" + fooCom + "/somewhereElse/bla/bla")));
    }

}