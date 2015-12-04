/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.favicons;

/**
 * Class to represent a Favicon declared on a webpage which we may or may not have downloaded.
 * Tab objects maintain a list of these and attempt to load them in descending order of quality
 * until one works. This enables us to fallback if something fails trying to decode a Favicon.
 */
public class RemoteFavicon implements Comparable<RemoteFavicon> {
    public static final int FAVICON_SIZE_ANY = -1;

    // URL of the Favicon referred to by this object.
    public String faviconUrl;

    // The size of the Favicon, as given in the <link> tag, if any. Zero if unspecified. -1 if "any".
    public int declaredSize;

    // Mime type of the Favicon, as given in the link tag.
    public String mimeType;

    public RemoteFavicon(String faviconURL, int givenSize, String mime) {
        faviconUrl = faviconURL;
        declaredSize = givenSize;
        mimeType = mime;
    }

    /**
     * Determine equality of two RemoteFavicons.
     * Two RemoteFavicons are considered equal if they have the same URL, size, and mime type.
     * @param o The object to compare to this one.
     * @return true if o is of type RemoteFavicon and is considered equal to this one, false otherwise.
     */
    @Override
    public boolean equals(Object o) {
        if (!(o instanceof RemoteFavicon)) {
            return false;
        }
        RemoteFavicon oCast = (RemoteFavicon) o;

        return oCast.faviconUrl.equals(faviconUrl) &&
               oCast.declaredSize == declaredSize &&
               oCast.mimeType.equals(mimeType);
    }

    @Override
    public int hashCode() {
        return super.hashCode();
    }

    /**
     * Establish ordering on Favicons. Lower value in the partial ordering is a "better" Favicon.
     * @param obj Object to compare against.
     * @return -1 if this Favicon is "better" than the other, zero if they are equivalent in quality,
     *         based on the information here, 1 if the other appears "better" than this one.
     */
    @Override
    public int compareTo(RemoteFavicon obj) {
        // Size "any" trumps everything else.
        if (declaredSize == FAVICON_SIZE_ANY) {
            if (obj.declaredSize == FAVICON_SIZE_ANY) {
                return 0;
            }

            return -1;
        }

        if (obj.declaredSize == FAVICON_SIZE_ANY) {
            return 1;
        }

        // Unspecified sizes are "worse".
        if (declaredSize > obj.declaredSize) {
            return -1;
        }

        if (declaredSize == obj.declaredSize) {
            // If there's no other way to choose, we prefer container types. They *might* contain
            // an image larger than the size given in the <link> tag.
            if (Favicons.isContainerType(mimeType)) {
                return -1;
            }
            return 0;
        }
        return 1;
    }
}
