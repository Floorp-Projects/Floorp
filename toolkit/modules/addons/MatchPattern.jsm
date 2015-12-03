/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["MatchPattern"];

/* globals MatchPattern */

const PERMITTED_SCHEMES = ["http", "https", "file", "ftp", "app"];

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
    this.scheme = PERMITTED_SCHEMES;
    this.host = "*";
    this.path = new RegExp(".*");
  } else if (!pat) {
    this.scheme = [];
  } else {
    let re = new RegExp("^(http|https|file|ftp|app|\\*)://(\\*|\\*\\.[^*/]+|[^*/]+|)(/.*)$");
    let match = re.exec(pat);
    if (!match) {
      Cu.reportError(`Invalid match pattern: '${pat}'`);
      this.scheme = [];
      return;
    }

    if (match[1] == "*") {
      this.scheme = ["http", "https"];
    } else {
      this.scheme = [match[1]];
    }
    this.host = match[2];
    this.path = globToRegexp(match[3], false);

    // We allow the host to be empty for file URLs.
    if (this.host == "" && this.scheme[0] != "file") {
      Cu.reportError(`Invalid match pattern: '${pat}'`);
      this.scheme = [];
      return;
    }
  }
}

SingleMatchPattern.prototype = {
  matches(uri, ignorePath = false) {
    if (this.scheme.indexOf(uri.scheme) == -1) {
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

  serialize() {
    return this.pat;
  },
};
