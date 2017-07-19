/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.os.AsyncTask;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.util.Log;
import ch.boye.httpclientandroidlib.util.TextUtils;
import org.mozilla.gecko.util.publicsuffix.PublicSuffix;

import java.lang.ref.WeakReference;
import java.net.URI;
import java.net.URISyntaxException;

/** Utilities for operating on URLs. */
public class URIUtils {
    private static final String LOGTAG = "GeckoURIUtils";

    private URIUtils() {}

    /** @return a {@link URI} if possible, else null. */
    @Nullable
    public static URI uriOrNull(final String uriString) {
        try {
            return new URI(uriString);
        } catch (final URISyntaxException e) {
            return null;
        }
    }

    /**
     * Returns the second level domain (SLD) of a url. It removes any subdomain/TLD.
     * e.g. https://m.foo.com/bar/baz?noo=abc#123  => foo
     *
     * This implementation is taken from Firefox for iOS:
     *   https://github.com/mozilla-mobile/firefox-ios/blob/deb9736c905cdf06822ecc4a20152df7b342925d/Shared/Extensions/NSURLExtensions.swift#L152
     *
     * @param uriString A url from which to extract the second level domain.
     * @return The second level domain of the url.
     */
    @WorkerThread // PublicSuffix methods can touch the disk.
    public static String getHostSecondLevelDomain(@NonNull final Context context, @NonNull final String uriString)
            throws URISyntaxException {
        if (context == null) { throw new NullPointerException("Expected non-null Context argument"); }
        if (uriString == null) { throw new NullPointerException("Expected non-null uri argument"); }

        final URI uri = new URI(uriString);
        final String baseDomain = getBaseDomain(context, uri);
        if (baseDomain == null) {
            final String normalizedHost = StringUtils.stripCommonSubdomains(uri.getHost());
            return !TextUtils.isEmpty(normalizedHost) ? normalizedHost : uriString;
        }

        return PublicSuffix.stripPublicSuffix(context, baseDomain);
    }

    /**
     * Returns the base domain from a given hostname. The base domain name is defined as the public domain suffix
     * with the base private domain attached to the front. For example, for the URL www.bbc.co.uk, the base domain
     * would be bbc.co.uk. The base domain includes the public suffix (co.uk) + one level down (bbc).
     *
     * IPv4 & IPv6 urls are not supported and will return null.
     *
     * This implementation is taken from Firefox for iOS:
     *   https://github.com/mozilla-mobile/firefox-ios/blob/deb9736c905cdf06822ecc4a20152df7b342925d/Shared/Extensions/NSURLExtensions.swift#L205
     *
     * @param uri The uri to find the base domain of
     * @return The base domain string for the given host name, or null if not applicable.
     */
    @Nullable
    @WorkerThread // PublicSuffix methods can touch the disk.
    public static String getBaseDomain(@NonNull final Context context, final URI uri) {
        final String host = uri.getHost();
        if (isIPv6(uri) || TextUtils.isEmpty(host)) {
            return null;
        }

        // If this is just a hostname and not a FQDN, use the entire hostname.
        if (!host.contains(".")) {
            return host;
        }

        final String publicSuffixWithDomain = PublicSuffix.getPublicSuffix(context, host, 1);
        return !TextUtils.isEmpty(publicSuffixWithDomain) ? publicSuffixWithDomain : null;
    }

    // impl via FFiOS: https://github.com/mozilla-mobile/firefox-ios/blob/deb9736c905cdf06822ecc4a20152df7b342925d/Shared/Extensions/NSURLExtensions.swift#L292
    private static boolean isIPv6(final URI uri) {
        final String host = uri.getHost();
        return !TextUtils.isEmpty(host) && host.contains(":");
    }

    /**
     * An async task that will take a URI formatted as a String and will retrieve
     * {@link #getHostSecondLevelDomain(Context, String)}. To use this, extend the class and override
     * {@link #onPostExecute(Object)}, where the secondLevelDomain will be returned.
     */
    public static abstract class GetHostSecondLevelDomainAsyncTask extends AsyncTask<Void, Void, String> {
        protected final WeakReference<Context> contextWeakReference;
        protected final String uriString;

        public GetHostSecondLevelDomainAsyncTask(final Context contextWeakReference, final String uriString) {
            this.contextWeakReference = new WeakReference<>(contextWeakReference);
            this.uriString = uriString;
        }

        @Override
        protected String doInBackground(final Void... params) {
            final Context context = contextWeakReference.get();
            if (context == null) {
                return null;
            }

            try {
                return URIUtils.getHostSecondLevelDomain(context, uriString);
            } catch (final URISyntaxException e) {
                Log.w(LOGTAG, "Unable to fetch second level domain."); // Don't log exception to avoid logging pii/urls.
                return null;
            }
        }
    }
}
