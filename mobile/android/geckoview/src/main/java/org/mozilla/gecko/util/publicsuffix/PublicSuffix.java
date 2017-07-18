/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util.publicsuffix;

import android.content.Context;
import android.support.annotation.NonNull;
import android.support.annotation.WorkerThread;

import org.mozilla.gecko.util.StringUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Helper methods for the public suffix part of a domain.
 *
 * A "public suffix" is one under which Internet users can (or historically could) directly register
 * names. Some examples of public suffixes are .com, .co.uk and pvt.k12.ma.us.
 *
 * https://publicsuffix.org/
 *
 * Some parts of the implementation of this class are based on InternetDomainName class of the Guava
 * project: https://github.com/google/guava
 */
public class PublicSuffix {
    /**
     * Strip the public suffix from the domain. Returns the original domain if no public suffix
     * could be found.
     *
     * www.mozilla.org -> www.mozilla
     * independent.co.uk -> independent
     */
    @NonNull
    @WorkerThread // This method might need to load data from disk
    public static String stripPublicSuffix(Context context, @NonNull String domain) {
        if (domain.length() == 0) {
            return domain;
        }

        final int index = findPublicSuffixIndex(context, domain);
        if (index == -1) {
            return domain;
        }

        return domain.substring(0, index);
    }

    /**
     * Returns the index of the leftmost part of the public suffix, or -1 if not found.
     */
    @WorkerThread
    private static int findPublicSuffixIndex(Context context, String domain) {
        final List<String> parts = normalizeAndSplit(domain);
        final int partsSize = parts.size();
        final Set<String> exact = PublicSuffixPatterns.getExactSet(context);

        for (int i = 0; i < partsSize; i++) {
            String ancestorName = TextUtils.join(".", parts.subList(i, partsSize));

            if (exact.contains(ancestorName)) {
                return joinIndex(parts, i);
            }

            // Excluded domains (e.g. !nhs.uk) use the next highest
            // domain as the effective public suffix (e.g. uk).
            if (PublicSuffixPatterns.EXCLUDED.contains(ancestorName)) {
                return joinIndex(parts, i + 1);
            }

            if (matchesWildcardPublicSuffix(ancestorName)) {
                return joinIndex(parts, i);
            }
        }

        return -1;
    }

    /**
     * Normalize domain and split into domain parts (www.mozilla.org -> [www, mozilla, org]).
     */
    private static List<String> normalizeAndSplit(String domain) {
        domain = domain.replaceAll("[.\u3002\uFF0E\uFF61]", "."); // All dot-like characters to '.'
        domain = domain.toLowerCase();

        if (domain.endsWith(".")) {
            domain = domain.substring(0, domain.length() - 1); // Strip trailing '.'
        }

        List<String> parts = new ArrayList<>();
        Collections.addAll(parts, domain.split("\\."));

        return parts;
    }

    /**
     * Translate the index of the leftmost part of the public suffix to the index of the domain string.
     *
     * [www, mozilla, org] and 2 => 12 (www.mozilla)
     */
    private static int joinIndex(List<String> parts, int index) {
        int actualIndex = parts.get(0).length();

        for (int i = 1; i < index; i++) {
            actualIndex += parts.get(i).length() + 1; // Add one for the "." that is not part of the list elements
        }

        return actualIndex;
    }

    /**
     * Does the domain name match one of the "wildcard" patterns (e.g. {@code "*.ar"})?
     */
    private static boolean matchesWildcardPublicSuffix(String domain) {
        final String[] pieces = domain.split("\\.", 2);
        return pieces.length == 2 && PublicSuffixPatterns.UNDER.contains(pieces[1]);
    }
}
