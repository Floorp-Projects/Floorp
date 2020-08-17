/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.support.utils;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

@RunWith(AndroidJUnit4.class)
public class WebURLFinderTest {

    public String find(String string) {
        // Test with explicit unicode support. Implicit unicode support is available in Android
        // but not on host systems where testing will take place. See the comment in WebURLFinder
        // for additional information.
        return new WebURLFinder(string, true).bestWebURL();
    }

    public String find(String[] strings) {
        // Test with explicit unicode support. Implicit unicode support is available in Android
        // but not on host systems where testing will take place. See the comment in WebURLFinder
        // for additional information.
        return new WebURLFinder(Arrays.asList(strings), true).bestWebURL();
    }

    public void testNoEmail() {
        assertNull(find("test@test.com"));
    }

    @Test
    public void testSchemeFirst() {
        assertEquals("http://scheme.com", find("test.com http://scheme.com"));
        assertEquals("http://ç.çơḿ", find("www.cnn.com http://ç.çơḿ"));
    }

    @Test
    public void testFullURL() {
        assertEquals("http://scheme.com:8080/inner#anchor&arg=1", find("test.com http://scheme.com:8080/inner#anchor&arg=1"));
        assertEquals("http://s-scheme.com:8080/inner#anchor&arg=1", find("test.com http://s-scheme.com:8080/inner#anchor&arg=1"));
        assertEquals("http://t-example:8080/appversion-1.0.0/f/action.xhtml", find("test.com http://t-example:8080/appversion-1.0.0/f/action.xhtml"));
        assertEquals("http://t-example:8080/appversion-1.0.0/f/action.xhtml", find("http://t-example:8080/appversion-1.0.0/f/action.xhtml"));
        assertEquals("http://ß.de/", find("http://ß.de/ çnn.çơḿ"));
        assertEquals("htt-p://ß.de/", find("çnn.çơḿ htt-p://ß.de/"));
    }

    @Test
    public void testNoScheme() {
        assertEquals("noscheme.com", find("noscheme.com example.com"));
        assertEquals("noscheme.com", find("-noscheme.com example.com"));
        assertEquals("n-oscheme.com", find("n-oscheme.com example.com"));
        assertEquals("n-oscheme.com", find("----------n-oscheme.com "));
        assertEquals("n-oscheme.ç", find("----------n-oscheme.ç-----------------------"));
    }

    @Test
    public void testNoBadScheme() {
        assertNull(find("file:///test javascript:///test.js"));
    }

    @Test
    public void testStrings() {
        assertEquals("http://test.com", find(new String[]{"http://test.com", "noscheme.com"}));
        assertEquals("http://test.com", find(new String[]{"noschemefirst.com", "http://test.com"}));
        assertEquals("http://test.com/inner#test", find(new String[]{"noschemefirst.com", "http://test.com/inner#test", "http://second.org/fark"}));
        assertEquals("http://test.com", find(new String[]{"javascript:///test.js", "http://test.com"}));
        assertEquals("http://çnn.çơḿ", find(new String[]{"www.cnn.com http://çnn.çơḿ"}));
    }

    @Test
    public void testIsWebURL() {
        assertTrue(WebURLFinder.Companion.isWebURL("http://test.com"));

        assertFalse(WebURLFinder.Companion.isWebURL("#http#://test.com"));
        assertFalse(WebURLFinder.Companion.isWebURL("file:///sdcard/download"));
        assertFalse(WebURLFinder.Companion.isWebURL("filE:///sdcard/Download"));
        assertFalse(WebURLFinder.Companion.isWebURL("javascript:alert('Hi')"));
        assertFalse(WebURLFinder.Companion.isWebURL("JAVascript:alert('Hi')"));

    }
}