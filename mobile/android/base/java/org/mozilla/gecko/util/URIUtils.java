/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.content.Context;
import android.os.AsyncTask;
import android.support.annotation.IntRange;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.WorkerThread;
import android.text.TextUtils;
import ch.boye.httpclientandroidlib.conn.util.InetAddressUtils;
import org.mozilla.gecko.util.publicsuffix.PublicSuffix;

import java.lang.ref.WeakReference;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.regex.Pattern;

/** Utilities for operating on URLs. */
public class URIUtils {
    private static final String LOGTAG = "GeckoURIUtils";

    private static final Pattern EMPTY_PATH = Pattern.compile("/*");

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
     * Returns true if {@link URI#getPath()} is not empty, false otherwise where empty means the given path contains
     * characters other than "/".
     *
     * This is necessary because the URI method will return "/" for "http://google.com/".
     */
    public static boolean isPathEmpty(@NonNull final URI uri) {
        final String path = uri.getPath();
        return TextUtils.isEmpty(path) || EMPTY_PATH.matcher(path).matches();

    }

    /**
     * Returns the domain for the given URI, formatted by the other available parameters.
     *
     * A public suffix is a top-level domain. For the input, "https://github.com", you can specify
     * {@code shouldIncludePublicSuffix}:
     * - true: "github.com"
     * - false: "github"
     *
     * The subdomain count is the number of subdomains you want to include; the domain will always be included. For
     * the input, "https://m.blog.github.io/", excluding the public suffix and the subdomain count:
     * - 0: "github"
     * - 1: "blog.github.com"
     * - 2: "m.blog.github.com"
     *
     * ipv4 & ipv6 urls will return the address directly.
     *
     * This implementation is influenced by Firefox iOS and can be used in place of some URI formatting functions:
     * - hostSLD [1]: exclude publicSuffix, 0 subdomains
     * - baseDomain [2]: include publicSuffix, 0 subdomains
     *
     * Expressing the method this way (instead of separate baseDomain & hostSLD methods) is more flexible if we want to
     * change the subdomain count and works well with our {@link GetFormattedDomainAsyncTask}, which can take the
     * same parameters we pass in here, rather than creating a new Task for each method.
     *
     * [1]: https://github.com/mozilla-mobile/firefox-ios/blob/deb9736c905cdf06822ecc4a20152df7b342925d/Shared/Extensions/NSURLExtensions.swift#L152
     * [2]: https://github.com/mozilla-mobile/firefox-ios/blob/deb9736c905cdf06822ecc4a20152df7b342925d/Shared/Extensions/NSURLExtensions.swift#L205
     *
     * @param context the Activity context.
     * @param uri the URI whose host we should format.
     * @param shouldIncludePublicSuffix true if the public suffix should be included, false otherwise.
     * @param subdomainCount The number of subdomains to include.
     *
     * @return the formatted domain, or the empty String if the host cannot be found.
     */
    @NonNull
    @WorkerThread // calls PublicSuffix methods.
    public static String getFormattedDomain(@NonNull final Context context, @NonNull final URI uri,
            final boolean shouldIncludePublicSuffix, @IntRange(from = 0) final int subdomainCount) {
        if (context == null) { throw new NullPointerException("Expected non-null Context argument"); }
        if (uri == null) { throw new NullPointerException("Expected non-null uri argument"); }
        if (subdomainCount < 0) { throw new IllegalArgumentException("Expected subdomainCount >= 0."); }

        final String host = uri.getHost();
        if (TextUtils.isEmpty(host)) {
            return ""; // There's no host so there's no domain to retrieve.
        }

        if (InetAddressUtils.isIPv4Address(host) ||
                isIPv6(uri) ||
                !host.contains(".")) { // If this is just a hostname and not a FQDN, use the entire hostname.
            return host;
        }

        final String domainStr = PublicSuffix.getPublicSuffix(context, host, subdomainCount + 1);
        if (TextUtils.isEmpty(domainStr)) {
            // There is no public suffix found so we assume the whole host is a domain.
            return stripSubdomains(host, subdomainCount);
        }

        if (!shouldIncludePublicSuffix) {
            // We could be slightly more efficient if we wrote a new algorithm rather than using PublicSuffix twice
            // but I don't think it's worth the time and it'd complicate the code with more independent branches.
            return PublicSuffix.stripPublicSuffix(context, domainStr);
        }
        return domainStr;
    }

    /** Strips any subdomains from the host over the given limit. */
    private static String stripSubdomains(String host, final int desiredSubdomainCount) {
        int includedSubdomainCount = 0;
        for (int i = host.length() - 1; i >= 0; --i) {
            if (host.charAt(i) == '.') {
                if (includedSubdomainCount >= desiredSubdomainCount) {
                    return host.substring(i + 1, host.length());
                }

                includedSubdomainCount += 1;
            }
        }

        // There are fewer subdomains than the total we'll accept so return them all!
        return host;
    }

    // impl via FFiOS: https://github.com/mozilla-mobile/firefox-ios/blob/deb9736c905cdf06822ecc4a20152df7b342925d/Shared/Extensions/NSURLExtensions.swift#L292
    private static boolean isIPv6(final URI uri) {
        final String host = uri.getHost();
        return !TextUtils.isEmpty(host) && host.contains(":");
    }

    /**
     * An async task that will take a URI formatted as a String and will retrieve
     * {@link #getFormattedDomain(Context, URI, boolean, int)}. To use this, extend the class and override
     * {@link #onPostExecute(Object)}, where the formatted domain, or the empty String if the host cannot determined,
     * will be returned.
     */
    public static abstract class GetFormattedDomainAsyncTask extends AsyncTask<Void, Void, String> {
        protected final WeakReference<Context> contextWeakReference;
        protected final URI uri;
        protected final boolean shouldIncludePublicSuffix;
        protected final int subdomainCount;

        public GetFormattedDomainAsyncTask(final Context context, final URI uri, final boolean shouldIncludePublicSuffix,
                final int subdomainCount) {
            this.contextWeakReference = new WeakReference<>(context);
            this.uri = uri;
            this.shouldIncludePublicSuffix = shouldIncludePublicSuffix;
            this.subdomainCount = subdomainCount;
        }

        @Override
        protected String doInBackground(final Void... params) {
            final Context context = contextWeakReference.get();
            if (context == null) {
                return "";
            }

            return URIUtils.getFormattedDomain(context, uri, shouldIncludePublicSuffix, subdomainCount);
        }
    }
}
