package org.mozilla.focus.webview;

import android.net.Uri;
import android.os.StrictMode;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.focus.BuildConfig;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.Map;

import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

// IMPORTANT NOTE - IF RUNNING TESTS USING ANDROID STUDIO:
// Read the following for a guide on how to get AS to correctly load resources, if you don't
// do this you will get Resource Loading exceptions:
// http://robolectric.org/getting-started/#note-for-linux-and-mac-users

@RunWith(RobolectricTestRunner.class)
@Config(constants = BuildConfig.class, packageName = "org.mozilla.focus")
public class TrackingProtectionWebViewClientTest {

    private TrackingProtectionWebViewClient trackingProtectionWebViewClient;
    private WebView webView;

    @Before
    public void setup() {
        trackingProtectionWebViewClient = new TrackingProtectionWebViewClient(RuntimeEnvironment.application);

        webView = mock(WebView.class);
        when(webView.getContext()).thenReturn(RuntimeEnvironment.application);
    }

    @After
    public void cleanup() {
        // Reset strict mode: for every test, Robolectric will create FocusApplication again.
        // FocusApplication expects strict mode to be disabled (since it loads some preferences from disk),
        // before enabling it itself. If we run multiple tests, strict mode will stay enabled
        // and FocusApplication crashes during initialisation for the second test.
        // This applies across multiple Test classes, e.g. DisconnectTest can cause
        // TrackingProtectionWebViewCLientTest to fail, unless it clears StrictMode first.
        // (FocusApplicaiton is initialised before @Before methods are run.)
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder().build());
    }

    @Test
    public void shouldInterceptRequest() throws Exception {
        trackingProtectionWebViewClient.notifyCurrentURL("http://www.mozilla.org");

        // Just some generic sanity checks that a definitely not blocked domain can be loaded, and
        // definitely blocked domains can't be
        {
            final WebResourceRequest request = createRequest("http://mozilla.org/about", false);
            final WebResourceResponse response = trackingProtectionWebViewClient.shouldInterceptRequest(webView, request);
            assertResourceAllowed(response);
        }

        {
            final WebResourceRequest request = createRequest("http://trackersimulator.org/foobar", false);
            final WebResourceResponse response = trackingProtectionWebViewClient.shouldInterceptRequest(webView, request);
            assertResourceBlocked(response);
        }
    }

    @Test
    public void testMainFrameAllowed() throws Exception {
        trackingProtectionWebViewClient.notifyCurrentURL("http://mozilla.org");

        // Blocked sites can still be loaded if opened as the main frame
        {
            final WebResourceRequest request = createRequest("http://trackersimulator.org/foobar", true);
            final WebResourceResponse response = trackingProtectionWebViewClient.shouldInterceptRequest(webView, request);
            assertResourceAllowed(response);
        }

        // And when we're loading that site, it can load first-party resources
        trackingProtectionWebViewClient.notifyCurrentURL("http://trackersimulator.org");
        {
            final WebResourceRequest request = createRequest("http://trackersimulator.org/other.js", false);
            final WebResourceResponse response = trackingProtectionWebViewClient.shouldInterceptRequest(webView, request);
            assertResourceAllowed(response);
        }

        // But other sites still can't load it:
        trackingProtectionWebViewClient.notifyCurrentURL("http://mozilla.org");
        {
            final WebResourceRequest request = createRequest("http://trackersimulator.org/foobar", false);
            final WebResourceResponse response = trackingProtectionWebViewClient.shouldInterceptRequest(webView, request);
            assertResourceBlocked(response);
        }
    }

    @Test
    public void testFaviconBlocked() throws Exception {
        trackingProtectionWebViewClient.notifyCurrentURL("http://www.mozilla.org");

        {
            // Webkit tries to load favicon.ico, even though it isn't used:
            final WebResourceRequest request = createRequest("http://mozilla.org/favicon.ico", false);
            final WebResourceResponse response = trackingProtectionWebViewClient.shouldInterceptRequest(webView, request);
            assertResourceBlocked(response);
        }

        {
            // But we can't block other images since they might be used by actual pages
            final WebResourceRequest request = createRequest("http://mozilla.org/favicon.png", false);
            final WebResourceResponse response = trackingProtectionWebViewClient.shouldInterceptRequest(webView, request);
            assertResourceAllowed(response);
        }
    }

    private void assertResourceAllowed(final WebResourceResponse response) {
        // shouldInterceptRequest returns null to indicate that WebView should just load the resource
        assertNull(response);
    }

    private void assertResourceBlocked(final WebResourceResponse response) {
        // shouldInterceptRequest a valid response in other cases, e.g. with null data to indicate
        // a blocked resource.
        assertNull(response.getData());
    }

    private static WebResourceRequest createRequest(final String url, final boolean isForMainFrame) {
        return new WebResourceRequest() {
            @Override
            public Uri getUrl() {
                return Uri.parse(url);
            }

            @Override
            public boolean isForMainFrame() {
                return isForMainFrame;
            }

            @Override
            public boolean isRedirect() {
                return false;
            }

            @Override
            public boolean hasGesture() {
                return false;
            }

            @Override
            public String getMethod() {
                return null;
            }

            @Override
            public Map<String, String> getRequestHeaders() {
                return null;
            }
        };
    }
}