/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["MatchPattern", "MatchGlobs", "MatchURLFilters"];

/* globals MatchPattern, MatchGlobs */

const PERMITTED_SCHEMES = ["http", "https", "file", "ftp", "data"];
const PERMITTED_SCHEMES_REGEXP = PERMITTED_SCHEMES.join("|");

// The basic RE for matching patterns
const PATTERN_REGEXP = new RegExp(`^(${PERMITTED_SCHEMES_REGEXP}|\\*)://(\\*|\\*\\.[^*/]+|[^*/]+|)(/.*)$`);

// The schemes/protocols implied by a pattern that starts with *://
const WILDCARD_SCHEMES = ["http", "https"];

// This function converts a glob pattern (containing * and possibly ?
// as wildcards) to a regular expression.
function globToRegexp(pat, allowQuestion) {
  // Escape everything except ? and *.
  pat = pat.replace(/[.+^${}()|[\]\\]/g, "\\$&");

  if (allowQuestion) {
    pat = pat.replace(/\?/g, ".");
  } else {
    pat = pat.replace(/\?/g, "\\?");
  }
  pat = pat.replace(/\*/g, ".*");
  return new RegExp("^" + pat + "$");
}

// These patterns follow the syntax in
// https://developer.chrome.com/extensions/match_patterns
function SingleMatchPattern(pat) {
  this.pat = pat;
  if (pat == "<all_urls>") {
    this.schemes = PERMITTED_SCHEMES;
    this.hostMatch = () => true;
    this.pathMatch = () => true;
  } else if (!pat) {
    this.schemes = [];
  } else {
    let match = PATTERN_REGEXP.exec(pat);
    if (!match) {
      Cu.reportError(`Invalid match pattern: '${pat}'`);
      this.schemes = [];
      return;
    }

    if (match[1] == "*") {
      this.schemes = WILDCARD_SCHEMES;
    } else {
      this.schemes = [match[1]];
    }

    // We allow the host to be empty for file URLs.
    if (match[2] == "" && this.schemes[0] != "file") {
      Cu.reportError(`Invalid match pattern: '${pat}'`);
      this.schemes = [];
      return;
    }

    this.host = match[2];
    this.hostMatch = this.getHostMatcher(match[2]);

    let pathMatch = globToRegexp(match[3], false);
    this.pathMatch = pathMatch.test.bind(pathMatch);
  }
}

SingleMatchPattern.prototype = {
  getHostMatcher(host) {
    // This code ignores the port, as Chrome does.
    if (host == "*") {
      return () => true;
    }
    if (host.startsWith("*.")) {
      let suffix = host.substr(2);
      let dotSuffix = "." + suffix;

      return ({host}) => host === suffix || host.endsWith(dotSuffix);
    }
    return uri => uri.host === host;
  },

  matches(uri, ignorePath = false) {
    return (
      this.schemes.includes(uri.scheme) &&
      this.hostMatch(uri) &&
      (ignorePath || (
        this.pathMatch(uri.cloneIgnoringRef().path)
      ))
    );
  },
};

this.MatchPattern = function(pat) {
  this.pat = pat;
  if (!pat) {
    this.matchers = [];
  } else if (pat instanceof String || typeof(pat) == "string") {
    this.matchers = [new SingleMatchPattern(pat)];
  } else {
    this.matchers = pat.map(p => new SingleMatchPattern(p));
  }

  XPCOMUtils.defineLazyGetter(this, "explicitMatchers", () => {
    return this.matchers.filter(matcher => matcher.pat != "<all_urls>" &&
                                           matcher.host &&
                                           !matcher.host.startsWith("*"));
  });
};

MatchPattern.prototype = {
  // |uri| should be an nsIURI.
  matches(uri) {
    return this.matchers.some(matcher => matcher.matches(uri));
  },

  matchesIgnoringPath(uri, explicit = false) {
    if (explicit) {
      return this.explicitMatchers.some(matcher => matcher.matches(uri, true));
    }
    return this.matchers.some(matcher => matcher.matches(uri, true));
  },

  // Checks that this match pattern grants access to read the given
  // cookie. |cookie| should be an |nsICookie2| instance.
  matchesCookie(cookie) {
    // First check for simple matches.
    let secureURI = NetUtil.newURI(`https://${cookie.rawHost}/`);
    if (this.matchesIgnoringPath(secureURI)) {
      return true;
    }

    let plainURI = NetUtil.newURI(`http://${cookie.rawHost}/`);
    if (!cookie.isSecure && this.matchesIgnoringPath(plainURI)) {
      return true;
    }

    if (!cookie.isDomain) {
      return false;
    }

    // Things get tricker for domain cookies. The extension needs to be able
    // to read any cookies that could be read any host it has permissions
    // for. This means that our normal host matching checks won't work,
    // since the pattern "*://*.foo.example.com/" doesn't match ".example.com",
    // but it does match "bar.foo.example.com", which can read cookies
    // with the domain ".example.com".
    //
    // So, instead, we need to manually check our filters, and accept any
    // with hosts that end with our cookie's host.

    let {host, isSecure} = cookie;

    for (let matcher of this.matchers) {
      let schemes = matcher.schemes;
      if (schemes.includes("https") || (!isSecure && schemes.includes("http"))) {
        if (matcher.host.endsWith(host)) {
          return true;
        }
      }
    }

    return false;
  },

  // Test if this MatchPattern subsumes the given pattern (i.e., whether
  // this pattern matches everything the given pattern does).
  // Note, this method considers only to protocols and hosts/domains,
  // paths are ignored.
  subsumes(pattern) {
    let match = PATTERN_REGEXP.exec(pattern);
    if (!match) {
      throw new Error("Invalid match pattern");
    }

    if (match[1] == "*") {
      return WILDCARD_SCHEMES.every(scheme => this.matchesIgnoringPath({scheme, host: match[2]}));
    }

    return this.matchesIgnoringPath({scheme: match[1], host: match[2]});
  },

  serialize() {
    return this.pat;
  },
};

// Globs can match everything. Be careful, this DOES NOT filter by allowed schemes!
this.MatchGlobs = function(globs) {
  this.original = globs;
  if (globs) {
    this.regexps = Array.from(globs, (glob) => globToRegexp(glob, true));
  } else {
    this.regexps = [];
  }
};

MatchGlobs.prototype = {
  matches(str) {
    return this.regexps.some(regexp => regexp.test(str));
  },
  serialize() {
    return this.original;
  },
};

// Match WebNavigation URL Filters.
this.MatchURLFilters = function(filters) {
  if (!Array.isArray(filters)) {
    throw new TypeError("filters should be an array");
  }

  if (filters.length == 0) {
    throw new Error("filters array should not be empty");
  }

  this.filters = filters;
};

MatchURLFilters.prototype = {
  matches(url) {
    let uri = NetUtil.newURI(url);
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
    return this.filters.some(filter => this.matchURLFilter({filter, data, uri, uriURL}));
  },

  matchURLFilter({filter, data, uri, uriURL}) {
    // Test for scheme based filtering.
    if (filter.schemes) {
      // Return false if none of the schemes matches.
      if (!filter.schemes.some((scheme) => uri.schemeIs(scheme))) {
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
      return filter.ports.some((filterPort) => {
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
      if (!this.testMatchOnURLComponent({urlComponent, data, filter})) {
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
      if (!urlWithoutQueryAndRef ||
          !urlWithoutQueryAndRef.match(filter.originAndPathMatches)) {
        return false;
      }
    }

    return true;
  },

  testMatchOnURLComponent({urlComponent: key, data, filter}) {
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
  },

  serialize() {
    return this.filters;
  },
};
