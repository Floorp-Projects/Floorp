/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
});

/**
 * Get the URLs with the top frecency scores.
 *
 * Note:
 *   - Blocked URLs are excluded
 *   - Allow multiple URLs from the same domain (www vs non-www urls)
 *
 * @param {number} maxUrls
 *   The maximum number of URLs to fetch.
 * @param {number} frecencyThreshold
 *   The minimal frecency score of the URL. Will use the default set by
 *   the upstream module if unspecified. For reference, "100" means one
 *   visit in the past 3 days. see more details at:
 *   `/browser/components/urlbar/docs/ranking.rst`
 * @returns {Array}
 *   An array of URLs. Note that the actual number could be less than this.
 */
export async function getTopFrecentUrls(
  maxUrls,
  frecencyThreshold /* for test */
) {
  const options = {
    ignoreBlocked: true,
    onePerDomain: false,
    includeFavicon: false,
    topsiteFrecency: frecencyThreshold,
    numItems: maxUrls,
  };
  const records = await lazy.NewTabUtils.activityStreamLinks.getTopSites(
    options
  );

  return records.map(site => site.url);
}

/**
 * Get the URLs of the most recent browsing history.
 *
 * Note:
 *   - Blocked URLs are excluded
 *   - Recent bookmarks are excluded
 *   - Recent "Save-to-Pocket" URLs are excluded
 *   - It would only return URLs if the page meta data is present. We can relax
 *     this in the future.
 *   - Multiple URLs may be returned for the same domain
 *
 * @param {number} maxUrls
 *   The maximum number of URLs to fetch.
 * @returns {Array}
 *   An array of URLs. Note that the actual number could be less than this.
 */
export async function getMostRecentUrls(maxUrls) {
  const options = {
    ignoreBlocked: true,
    excludeBookmarks: true,
    excludeHistory: false,
    excludePocket: true,
    withFavicons: false,
    numItems: maxUrls,
  };
  const records = await lazy.NewTabUtils.activityStreamLinks.getHighlights(
    options
  );

  return records.map(site => site.url);
}
