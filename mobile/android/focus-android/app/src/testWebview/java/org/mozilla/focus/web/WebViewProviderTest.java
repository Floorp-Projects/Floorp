package org.mozilla.focus.web;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

import static org.junit.Assert.assertEquals;

@RunWith(RobolectricTestRunner.class)
public class WebViewProviderTest {

    @Test public void testGetUABrowserString() {
        // Typical situation with a webview UA string from Android 5:
        String focusToken = "Focus/1.0";
        final String existing = "Mozilla/5.0 (Linux; Android 5.0.2; Android SDK built for x86_64 Build/LSY66K) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/37.0.0.0 Mobile Safari/537.36";
        assertEquals("AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 " + focusToken + " Chrome/37.0.0.0 Mobile Safari/537.36",
                WebViewProvider.INSTANCE.getUABrowserString(existing, focusToken));

        // Make sure we can use any token, e.g Klar:
        focusToken = "Klar/2.0";
        assertEquals("AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 " + focusToken + " Chrome/37.0.0.0 Mobile Safari/537.36",
                WebViewProvider.INSTANCE.getUABrowserString(existing, focusToken));

        // And a non-standard UA String, which doesn't contain AppleWebKit
        focusToken = "Focus/1.0";
        final String imaginaryKit = "Mozilla/5.0 (Linux) ImaginaryKit/-10 (KHTML, like Gecko) Version/4.0 Chrome/37.0.0.0 Mobile Safari/537.36";
        assertEquals("ImaginaryKit/-10 (KHTML, like Gecko) Version/4.0 " + focusToken + " Chrome/37.0.0.0 Mobile Safari/537.36",
                WebViewProvider.INSTANCE.getUABrowserString(imaginaryKit, focusToken));

        // Another non-standard UA String, this time with no Chrome (in which case we should be appending focus)
        final String chromeless = "Mozilla/5.0 (Linux; Android 5.0.2; Android SDK built for x86_64 Build/LSY66K) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Imaginary/37.0.0.0 Mobile Safari/537.36";
        assertEquals("AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Imaginary/37.0.0.0 Mobile Safari/537.36 " + focusToken,
                WebViewProvider.INSTANCE.getUABrowserString(chromeless, focusToken));

        // No AppleWebkit, no Chrome
        final String chromelessImaginaryKit = "Mozilla/5.0 (Linux) ImaginaryKit/-10 (KHTML, like Gecko) Version/4.0 Imaginary/37.0.0.0 Mobile Safari/537.36";
        assertEquals("ImaginaryKit/-10 (KHTML, like Gecko) Version/4.0 Imaginary/37.0.0.0 Mobile Safari/537.36 " + focusToken,
                WebViewProvider.INSTANCE.getUABrowserString(chromelessImaginaryKit, focusToken));

    }
}