/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.support.utils;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

@RunWith(AndroidJUnit4.class)
public class WebURLFinderTest {

    public String find(String string) {
        return new WebURLFinder(string).bestWebURL();
    }

    public String find(String[] strings) {
        return new WebURLFinder(Arrays.asList(strings)).bestWebURL();
    }

    public void testNoEmail() {
        assertNull(find("test@test.com"));
    }

    @Test
    public void testSchemeFirst() {
        assertEquals("http://scheme.com", find("test.com http://scheme.com"));
    }

    @Test
    public void testFullURL() {
        assertEquals("http://scheme.com:8080/inner#anchor&arg=1", find("test.com http://scheme.com:8080/inner#anchor&arg=1"));
        assertEquals("http://s-scheme.com:8080/inner#anchor&arg=1", find("test.com http://s-scheme.com:8080/inner#anchor&arg=1"));
        assertEquals("http://t-example:8080/appversion-1.0.0/f/action.xhtml", find("test.com http://t-example:8080/appversion-1.0.0/f/action.xhtml"));
        assertEquals("http://t-example:8080/appversion-1.0.0/f/action.xhtml", find("http://t-example:8080/appversion-1.0.0/f/action.xhtml"));
    }

    @Test
    public void testNoScheme() {
        assertEquals("noscheme.com", find("noscheme.com"));
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
    }
}