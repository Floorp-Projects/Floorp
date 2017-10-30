package org.mozilla.focus.web;

import android.content.Context;
import android.os.Build;
import android.webkit.WebSettings;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

@RunWith(RobolectricTestRunner.class)
public class WebViewProviderTest {

    @Test public void testGetUABrowserString() {
        // Typical situation with a webview UA string from Android 5:
        String focusToken = "Focus/1.0";
        final String existing = "Mozilla/5.0 (Linux; Android 5.0.2; Android SDK built for x86_64 Build/LSY66K) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/37.0.0.0 Mobile Safari/537.36";
        assertEquals("AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 " + focusToken + " Chrome/37.0.0.0 Mobile Safari/537.36",
                WebViewProvider.getUABrowserString(existing, focusToken));

        // Make sure we can use any token, e.g Klar:
        focusToken = "Klar/2.0";
        assertEquals("AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 " + focusToken + " Chrome/37.0.0.0 Mobile Safari/537.36",
                WebViewProvider.getUABrowserString(existing, focusToken));

        // And a non-standard UA String, which doesn't contain AppleWebKit
        focusToken = "Focus/1.0";
        final String imaginaryKit = "Mozilla/5.0 (Linux) ImaginaryKit/-10 (KHTML, like Gecko) Version/4.0 Chrome/37.0.0.0 Mobile Safari/537.36";
        assertEquals("ImaginaryKit/-10 (KHTML, like Gecko) Version/4.0 " + focusToken + " Chrome/37.0.0.0 Mobile Safari/537.36",
                WebViewProvider.getUABrowserString(imaginaryKit, focusToken));

        // Another non-standard UA String, this time with no Chrome (in which case we should be appending focus)
        final String chromeless = "Mozilla/5.0 (Linux; Android 5.0.2; Android SDK built for x86_64 Build/LSY66K) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Imaginary/37.0.0.0 Mobile Safari/537.36";
        assertEquals("AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Imaginary/37.0.0.0 Mobile Safari/537.36 " + focusToken,
                WebViewProvider.getUABrowserString(chromeless, focusToken));

        // No AppleWebkit, no Chrome
        final String chromelessImaginaryKit = "Mozilla/5.0 (Linux) ImaginaryKit/-10 (KHTML, like Gecko) Version/4.0 Imaginary/37.0.0.0 Mobile Safari/537.36";
        assertEquals("ImaginaryKit/-10 (KHTML, like Gecko) Version/4.0 Imaginary/37.0.0.0 Mobile Safari/537.36 " + focusToken,
                WebViewProvider.getUABrowserString(chromelessImaginaryKit, focusToken));

    }

    @Test
    public void buildUserAgentString() throws Exception {
        final Context context = RuntimeEnvironment.application;
        final String versionName = context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName;

        // It's actually possible to get a normal webview instance with real settings and user
        // agent string, which buildUserAgentString() can successfully operate on. However we can't
        // easily test that the output is expected (without simply replicating what buildUserAgentString does),
        // so instead we just use mocking to supply a fixed UA string - we then know exactly what
        // the output String should look like:
        WebSettings testSettings = mock(WebSettings.class);
        when(testSettings.getUserAgentString()).thenReturn("Mozilla/5.0 (Linux; U; Android 4.0.3; ko-kr; LG-L160L Build/IML74K) AppleWebkit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30");

        assertEquals("Mozilla/5.0 (Linux; Android " + Build.VERSION.RELEASE + ") AppleWebkit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30 fakeappname/" + versionName,
                WebViewProvider.buildUserAgentString(context, testSettings, "fakeappname"));
    }

}