/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.icons.loader;

import android.content.Context;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mozilla.gecko.background.testhelpers.TestRunner;
import org.mozilla.gecko.icons.IconDescriptor;
import org.mozilla.gecko.icons.IconRequest;
import org.mozilla.gecko.icons.IconResponse;
import org.mozilla.gecko.icons.Icons;
import org.mozilla.gecko.icons.storage.FailureCache;
import org.robolectric.RuntimeEnvironment;

import java.net.HttpURLConnection;

import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

@RunWith(TestRunner.class)
public class TestIconDownloader {
    /**
     * Scenario: A request with a non HTTP URL (data:image/*) is executed.
     *
     * Verify that:
     *  * No download is performed.
     */
    @Test
    public void testDownloaderDoesNothingForNonHttpUrls() throws Exception {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createGenericIcon(
                        "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAEklEQVR4AWP4z8AAxCDiP8N/AB3wBPxcBee7AAAAAElFTkSuQmCC"))
                .build();

        final IconDownloader downloader = spy(new IconDownloader());
        IconResponse response = downloader.load(request);

        Assert.assertNull(response);

        verify(downloader, never()).downloadAndDecodeImage(any(Context.class), anyString());
        verify(downloader, never()).connectTo(anyString());
    }

    /**
     * Scenario: Request contains an URL and server returns 301 with location header (always the same URL).
     *
     * Verify that:
     *  * Download code stops and does not loop forever.
     */
    @Test
    public void testRedirectsAreFollowedButNotInCircles() throws Exception {
        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createFavicon(
                        "https://www.mozilla.org/media/img/favicon.52506929be4c.ico",
                        32,
                        "image/x-icon"))
                .build();

        HttpURLConnection mockedConnection = mock(HttpURLConnection.class);
        doReturn(301).when(mockedConnection).getResponseCode();
        doReturn("http://example.org/favicon.ico").when(mockedConnection).getHeaderField("Location");

        final IconDownloader downloader = spy(new IconDownloader());
        doReturn(mockedConnection).when(downloader).connectTo(anyString());
        IconResponse response = downloader.load(request);

        Assert.assertNull(response);

        verify(downloader).connectTo("https://www.mozilla.org/media/img/favicon.52506929be4c.ico");
        verify(downloader).connectTo("http://example.org/favicon.ico");
    }

    /**
     * Scenario: Request contains an URL and server returns HTTP 404.
     *
     * Verify that:
     *  * URL is added to failure cache.
     */
    @Test
    public void testUrlIsAddedToFailureCacheIfServerReturnsClientError() throws Exception {
        final String faviconUrl = "https://www.mozilla.org/404.ico";

        final IconRequest request = Icons.with(RuntimeEnvironment.application)
                .pageUrl("http://www.mozilla.org")
                .icon(IconDescriptor.createFavicon(faviconUrl, 32, "image/x-icon"))
                .build();

        HttpURLConnection mockedConnection = mock(HttpURLConnection.class);
        doReturn(404).when(mockedConnection).getResponseCode();

        Assert.assertFalse(FailureCache.get().isKnownFailure(faviconUrl));

        final IconDownloader downloader = spy(new IconDownloader());
        doReturn(mockedConnection).when(downloader).connectTo(anyString());
        IconResponse response = downloader.load(request);

        Assert.assertNull(response);

        Assert.assertTrue(FailureCache.get().isKnownFailure(faviconUrl));
    }
}
