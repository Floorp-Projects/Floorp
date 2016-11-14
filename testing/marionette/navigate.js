/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.importGlobalProperties(["URL"]);

this.EXPORTED_SYMBOLS = ["navigate"];

this.navigate = {};

/**
 * Determines if we expect to get a DOM load event (DOMContentLoaded)
 * on navigating to the |future| URL.
 *
 * @param {string} current
 *     URL the browser is currently visiting.
 * @param {string=} future
 *     Destination URL, if known.
 *
 * @return {boolean}
 *     Full page load would be expected if future is followed.
 *
 * @throws TypeError
 *     If |current| is not defined, or any of |current| or |future|
 *     are invalid URLs.
 */
navigate.isLoadEventExpected = function(current, future = undefined) {
  if (typeof current == "undefined") {
    throw TypeError("Expected at least one URL");
  }

  // assume we will go somewhere exciting
  if (typeof future == "undefined") {
    return true;
  }

  let cur = new navigate.IdempotentURL(current);
  let fut = new navigate.IdempotentURL(future);

  // assume javascript:<whatever> will modify current document
  // but this is not an entirely safe assumption to make,
  // considering it could be used to set window.location
  if (fut.protocol == "javascript:") {
    return false;
  }

  // navigating to same url, but with any hash
  if (cur.origin == fut.origin &&
      cur.pathname == fut.pathname &&
      fut.hash != "") {
    return false;
  }

  return true;
};

/**
 * Sane URL implementation that normalises URL fragments (hashes) and
 * path names for "data:" URLs, and makes them idempotent.
 *
 * At the time of writing this, the web is approximately 10 000 days (or
 * ~27.39 years) old.  One should think that by this point we would have
 * solved URLs.  The following code is prudent example that we have not.
 *
 * When a URL with a fragment identifier but no explicit name for the
 * fragment is given, i.e. "#", the {@code hash} property a {@code URL}
 * object computes is an empty string.  This is incidentally the same as
 * the default value of URLs without fragments, causing a lot of confusion.
 *
 * This means that the URL "http://a/#b" produces a hash of "#b", but that
 * "http://a/#" produces "".  This implementation rectifies this behaviour
 * by returning the actual full fragment, which is "#".
 *
 * "data:" URLs that contain fragments, which if they have the same origin
 * and path name are not meant to cause a page reload on navigation,
 * confusingly adds the fragment to the {@code pathname} property.
 * This implementation remedies this behaviour by trimming it off.
 *
 * The practical result of this is that while {@code URL} objects are
 * not idempotent, the returned URL elements from this implementation
 * guarantees that |url.hash == url.hash|.
 *
 * @param {string|URL} o
 *     Object to make an URL of.
 *
 * @return {navigate.IdempotentURL}
 *     Considered by some to be a somewhat saner URL.
 *
 * @throws TypeError
 *     If |o| is not a valid type or if is a string that cannot be parsed
 *     as a URL.
 */
navigate.IdempotentURL = function(o) {
  let url = new URL(o);

  let hash = url.hash;
  if (hash == "" && url.href[url.href.length - 1] == "#") {
    hash = "#";
  }

  return {
    hash: hash,
    host: url.host,
    hostname: url.hostname,
    href: url.href,
    origin: url.origin,
    password: url.password,
    pathname: url.pathname,
    port: url.port,
    protocol: url.protocol,
    search: url.search,
    searchParams: url.searchParams,
    username: url.username,
  };
};
