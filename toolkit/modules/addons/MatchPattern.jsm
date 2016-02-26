/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");

this.EXPORTED_SYMBOLS = ["MatchPattern"];

/* globals MatchPattern */

const PERMITTED_SCHEMES = ["http", "https", "file", "ftp", "app", "data"];
const PERMITTED_SCHEMES_REGEXP = PERMITTED_SCHEMES.join("|");

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
  if (pat == "<all_urls>") {
    this.schemes = PERMITTED_SCHEMES;
    this.host = "*";
    this.path = new RegExp(".*");
  } else if (!pat) {
    this.schemes = [];
  } else {
    let re = new RegExp(`^(${PERMITTED_SCHEMES_REGEXP}|\\*)://(\\*|\\*\\.[^*/]+|[^*/]+|)(/.*)$`);
    let match = re.exec(pat);
    if (!match) {
      Cu.reportError(`Invalid match pattern: '${pat}'`);
      this.schemes = [];
      return;
    }

    if (match[1] == "*") {
      this.schemes = ["http", "https"];
    } else {
      this.schemes = [match[1]];
    }
    this.host = match[2];
    this.path = globToRegexp(match[3], false);

    // We allow the host to be empty for file URLs.
    if (this.host == "" && this.schemes[0] != "file") {
      Cu.reportError(`Invalid match pattern: '${pat}'`);
      this.schemes = [];
      return;
    }
  }
}

SingleMatchPattern.prototype = {
  matches(uri, ignorePath = false) {
    if (!this.schemes.includes(uri.scheme)) {
      return false;
    }

    // This code ignores the port, as Chrome does.
    if (this.host == "*") {
      // Don't check anything.
    } else if (this.host[0] == "*") {
      // It must be *.foo. We also need to allow foo by itself.
      let suffix = this.host.substr(2);
      if (uri.host != suffix && !uri.host.endsWith("." + suffix)) {
        return false;
      }
    } else if (this.host != uri.host) {
      return false;
    }

    if (!ignorePath && !this.path.test(uri.path)) {
      return false;
    }

    return true;
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
};

MatchPattern.prototype = {
  // |uri| should be an nsIURI.
  matches(uri) {
    for (let matcher of this.matchers) {
      if (matcher.matches(uri)) {
        return true;
      }
    }
    return false;
  },

  matchesIgnoringPath(uri) {
    for (let matcher of this.matchers) {
      if (matcher.matches(uri, true)) {
        return true;
      }
    }
    return false;
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

  serialize() {
    return this.pat;
  },
};
