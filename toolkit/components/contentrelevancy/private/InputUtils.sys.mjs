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
 *   An array of URLs. Note that the actual number could be less than `maxUrls`.
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
 *   An array of URLs. Note that the actual number could be less than `maxUrls`.
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

/**
 * Get the URLs as a combination of the top frecent and the most recent
 * browsing history.
 *
 * It will fetch `maxUrls` URLs from top frecent URLs and use most recent URLs
 * as a fallback if the former is insufficient. Duplicates will be removed
 * As a result, the returned URLs might be fewer than requested.
 *
 * @param {number} maxUrls
 *   The maximum number of URLs to fetch.
 * @returns {Array}
 *   An array of URLs.
 */
export async function getFrecentRecentCombinedUrls(maxUrls) {
  let urls = await getTopFrecentUrls(maxUrls);
  if (urls.length < maxUrls) {
    const n = Math.round((maxUrls - urls.length) * 1.2); // Over-fetch for deduping
    const recentUrls = await getMostRecentUrls(n);
    urls = dedupUrls(urls, recentUrls).slice(0, maxUrls);
  }

  return urls;
}

/**
 * A helper to deduplicate items from any number of grouped URLs.
 *
 * Note:
 *   - Currently, all the elements (URLs) of the input arrays are treated as keys.
 *   - It doesn't assume the uniqueness within the group, therefore, in-group
 *     duplicates will be deduped as well.
 *
 * @param {Array} groups
 *   Contains an arbitrary number of arrays of URLs.
 * @returns {Array}
 *   An array of unique URLs from the input groups.
 */
function dedupUrls(...groups) {
  const uniques = new Set(groups.flat());
  return [...uniques];
}
