/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.webkit;

import android.content.Context;
import android.content.res.Resources;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.RawRes;
import android.support.v4.util.ArrayMap;
import android.support.v4.util.Pair;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.mozilla.focus.R;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.Map;

public class ErrorPage {

    private static final HashMap<Integer, Pair<Integer, Integer>> errorDescriptionMap;

    static {
        errorDescriptionMap = new HashMap<>();

        // Chromium's mapping (internal error code, to Android WebView error code) is described at:
        // https://chromium.googlesource.com/chromium/src.git/+/master/android_webview/java/src/org/chromium/android_webview/ErrorCodeConversionHelper.java

        errorDescriptionMap.put(WebViewClient.ERROR_UNKNOWN,
                new Pair<>(R.string.error_connectionfailure_title, R.string.error_connectionfailure_message));

        // This is probably the most commonly shown error. If there's no network, we inevitably
        // show this.
        errorDescriptionMap.put(WebViewClient.ERROR_HOST_LOOKUP,
                new Pair<>(R.string.error_hostLookup_title, R.string.error_hostLookup_message));

//        WebViewClient.ERROR_UNSUPPORTED_AUTH_SCHEME
        // TODO: we don't actually handle this in firefox - does this happen in real life?

//        WebViewClient.ERROR_AUTHENTICATION
        // TODO: there's no point in implementing this until we actually support http auth (#159)

        errorDescriptionMap.put(WebViewClient.ERROR_CONNECT,
                new Pair<>(R.string.error_connect_title, R.string.error_connect_message));

        // It's unclear what this actually means - it's not well documented. Based on looking at
        // ErrorCodeConversionHelper this could happen if networking is disabled during load, in which
        // case the generic error is good enough:
        errorDescriptionMap.put(WebViewClient.ERROR_IO,
                new Pair<>(R.string.error_connectionfailure_title, R.string.error_connectionfailure_message));

        errorDescriptionMap.put(WebViewClient.ERROR_TIMEOUT,
                new Pair<>(R.string.error_timeout_title, R.string.error_timeout_message));

        errorDescriptionMap.put(WebViewClient.ERROR_REDIRECT_LOOP,
                new Pair<>(R.string.error_redirectLoop_title, R.string.error_redirectLoop_message));

        // We already try to handle external URLs if possible (i.e. we offer to open the corresponding
        // app, if available for a given scheme). If we end up here that means no app exists.
        // We could consider showing an "open google play" link here, but ultimately it's hard
        // to know whether that's the right step, especially if there are no good apps for actually
        // handling such a protocol there - moreover there doesn't seem to be a good way to search
        // google play for apps supporting a given scheme.
        errorDescriptionMap.put(WebViewClient.ERROR_UNSUPPORTED_SCHEME,
                new Pair<>(R.string.error_unsupportedprotocol_title, R.string.error_unsupportedprotocol_message));

        errorDescriptionMap.put(WebViewClient.ERROR_FAILED_SSL_HANDSHAKE,
                new Pair<>(R.string.error_sslhandshake_title, R.string.error_sslhandshake_message));

        errorDescriptionMap.put(WebViewClient.ERROR_BAD_URL,
                new Pair<>(R.string.error_malformedURI_title, R.string.error_malformedURI_message));

        // We don't support file:/// URLs, so we shouldn't have to handle errors opening files either
//        WebViewClient.ERROR_FILE;
//        WebViewClient.ERROR_FILE_NOT_FOUND;

        // Seems to be an indication of OOM, insufficient resources, or too many queued DNS queries
        errorDescriptionMap.put(WebViewClient.ERROR_TOO_MANY_REQUESTS,
                new Pair<>(R.string.error_generic_title, R.string.error_generic_message));
    }

    public static boolean supportsErrorCode(final int errorCode) {
        return (errorDescriptionMap.get(errorCode) != null);
    }

    /**
     * Load a given resource file into a String.
     *
     * @param substitutionTable A table of substitions, e.g. %shortMessage% -> "Error loading page..."
     * @return The file content, with all substitutions having being made.
     */
    private static String loadResourceFile(@NonNull final Context context,
                                           @NonNull final @RawRes int resourceID,
                                           @Nullable final Map<String, String> substitutionTable) {

        BufferedReader fileReader = null;

        try {
            final InputStream fileStream = context.getResources().openRawResource(resourceID);
            fileReader = new BufferedReader(new InputStreamReader(fileStream));

            final StringBuilder outputBuffer = new StringBuilder();

            String line;
            while ((line = fileReader.readLine()) != null) {
                if (substitutionTable != null) {
                    for (final Map.Entry<String, String> entry : substitutionTable.entrySet()) {
                        line = line.replace(entry.getKey(), entry.getValue());
                    }
                }

                outputBuffer.append(line);
            }

            return outputBuffer.toString();
        } catch (final IOException e) {
            throw new IllegalStateException("Unable to load error page data");
        } finally {
            try {
                if (fileReader != null) {
                    fileReader.close();
                }
            } catch (IOException e) {
                // There's pretty much nothing we can do here. It doesn't seem right to crash
                // just because we couldn't close a file?
            }
        }
    }

    public static void loadErrorPage(final WebView webView, final String desiredURL, final int errorCode) {
        final Pair<Integer, Integer> errorResourceIDs = errorDescriptionMap.get(errorCode);

        if (errorResourceIDs == null) {
            throw new IllegalArgumentException("Cannot load error description for unsupported errorcode=" + errorCode);
        }

        // This is quite hacky: ideally we'd just load the css file directly using a '<link rel="stylesheet"'.
        // However webkit thinks it's still loading the original page, which can be an https:// page.
        // If mixed content blocking is enabled (which is probably what we want in Focus), then webkit
        // will block file:///android_res/ links from being loaded - which blocks our css from being loaded.
        // We could hack around that by enabling mixed content when loading an error page (and reenabling it
        // once that's loaded), but doing that correctly and reliably isn't particularly simple. Loading
        // the css data and stuffing it into our html is much simpler, especially since we're already doing
        // string substitutions.
        // As an added bonus: file:/// URIs are broken if the app-ID != app package, see:
        // https://code.google.com/p/android/issues/detail?id=211768 (this breaks loading css via file:///
        // references when running debug builds, and probably klar too) - which means this wouldn't
        // be possible even if we hacked around the mixed content issues.
        final String cssString = loadResourceFile(webView.getContext(), R.raw.errorpage_style, null);

        final Map<String, String> substitutionMap = new ArrayMap<>();

        final Resources resources = webView.getContext().getResources();

        substitutionMap.put("%page-title%", resources.getString(R.string.errorpage_title));
        substitutionMap.put("%button%", resources.getString(R.string.errorpage_refresh));

        substitutionMap.put("%messageShort%", resources.getString(errorResourceIDs.first));
        substitutionMap.put("%messageLong%", resources.getString(errorResourceIDs.second, desiredURL));

        substitutionMap.put("%css%", cssString);

        final String errorPage = loadResourceFile(webView.getContext(), R.raw.errorpage, substitutionMap);

        // We could load the raw html file directly into the webview using a file:///android_res/
        // URI - however we'd then need to do some JS hacking to do our String substitutions. Moreover
        // we'd have to deal with the mixed-content issues detailed above in that case.
        webView.loadDataWithBaseURL(desiredURL, errorPage, "text/html", "UTF8", desiredURL);
    }
}
