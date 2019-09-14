/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

/* exported MatchURLFilters */

var EXPORTED_SYMBOLS = ["MatchURLFilters"];

// Match WebNavigation URL Filters.
class MatchURLFilters {
  constructor(filters) {
    if (!Array.isArray(filters)) {
      throw new TypeError("filters should be an array");
    }

    if (!filters.length) {
      throw new Error("filters array should not be empty");
    }

    this.filters = filters;
  }

  matches(url) {
    let uri = Services.io.newURI(url);
    // Set uriURL to an empty object (needed because some schemes, e.g. about doesn't support nsIURL).
    let uriURL = {};
    if (uri instanceof Ci.nsIURL) {
      uriURL = uri;
    }

    // Set host to a empty string by default (needed so that schemes without an host,
    // e.g. about, can pass an empty string for host based event filtering as expected).
    let host = "";
    try {
      host = uri.host;
    } catch (e) {
      // 'uri.host' throws an exception with some uri schemes (e.g. about).
    }

    let port;
    try {
      port = uri.port;
    } catch (e) {
      // 'uri.port' throws an exception with some uri schemes (e.g. about),
      // in which case it will be |undefined|.
    }

    let data = {
      // NOTE: This properties are named after the name of their related
      // filters (e.g. `pathContains/pathEquals/...` will be tested against the
      // `data.path` property, and the same is done for the `host`, `query` and `url`
      // components as well).
      path: uriURL.filePath,
      query: uriURL.query,
      host,
      port,
      url,
    };

    // If any of the filters matches, matches returns true.
    return this.filters.some(filter =>
      this.matchURLFilter({ filter, data, uri, uriURL })
    );
  }

  matchURLFilter({ filter, data, uri, uriURL }) {
    // Test for scheme based filtering.
    if (filter.schemes) {
      // Return false if none of the schemes matches.
      if (!filter.schemes.some(scheme => uri.schemeIs(scheme))) {
        return false;
      }
    }

    // Test for exact port matching or included in a range of ports.
    if (filter.ports) {
      let port = data.port;
      if (port === -1) {
        // NOTE: currently defaultPort for "resource" and "chrome" schemes defaults to -1,
        // for "about", "data" and "javascript" schemes defaults to undefined.
        if (["resource", "chrome"].includes(uri.scheme)) {
          port = undefined;
        } else {
          port = Services.io.getProtocolHandler(uri.scheme).defaultPort;
        }
      }

      // Return false if none of the ports (or port ranges) is verified
      return filter.ports.some(filterPort => {
        if (Array.isArray(filterPort)) {
          let [lower, upper] = filterPort;
          return port >= lower && port <= upper;
        }

        return port === filterPort;
      });
    }

    // Filters on host, url, path, query:
    // hostContains, hostEquals, hostSuffix, hostPrefix,
    // urlContains, urlEquals, ...
    for (let urlComponent of ["host", "path", "query", "url"]) {
      if (!this.testMatchOnURLComponent({ urlComponent, data, filter })) {
        return false;
      }
    }

    // urlMatches is a regular expression string and it is tested for matches
    // on the "url without the ref".
    if (filter.urlMatches) {
      let urlWithoutRef = uri.specIgnoringRef;
      if (!urlWithoutRef.match(filter.urlMatches)) {
        return false;
      }
    }

    // originAndPathMatches is a regular expression string and it is tested for matches
    // on the "url without the query and the ref".
    if (filter.originAndPathMatches) {
      let urlWithoutQueryAndRef = uri.resolve(uriURL.filePath);
      // The above 'uri.resolve(...)' will be null for some URI schemes
      // (e.g. about).
      // TODO: handle schemes which will not be able to resolve the filePath
      // (e.g. for "about:blank", 'urlWithoutQueryAndRef' should be "about:blank" instead
      // of null)
      if (
        !urlWithoutQueryAndRef ||
        !urlWithoutQueryAndRef.match(filter.originAndPathMatches)
      ) {
        return false;
      }
    }

    return true;
  }

  testMatchOnURLComponent({ urlComponent: key, data, filter }) {
    // Test for equals.
    // NOTE: an empty string should not be considered a filter to skip.
    if (filter[`${key}Equals`] != null) {
      if (data[key] !== filter[`${key}Equals`]) {
        return false;
      }
    }

    // Test for contains.
    if (filter[`${key}Contains`]) {
      let value = (key == "host" ? "." : "") + data[key];
      if (!data[key] || !value.includes(filter[`${key}Contains`])) {
        return false;
      }
    }

    // Test for prefix.
    if (filter[`${key}Prefix`]) {
      if (!data[key] || !data[key].startsWith(filter[`${key}Prefix`])) {
        return false;
      }
    }

    // Test for suffix.
    if (filter[`${key}Suffix`]) {
      if (!data[key] || !data[key].endsWith(filter[`${key}Suffix`])) {
        return false;
      }
    }

    return true;
  }

  serialize() {
    return this.filters;
  }
}
