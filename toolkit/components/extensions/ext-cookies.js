"use strict";

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

var {
  EventManager,
} = ExtensionUtils;

// Cookies from private tabs currently can't be enumerated.
var DEFAULT_STORE = "firefox-default";

function convert(cookie) {
  let result = {
    name: cookie.name,
    value: cookie.value,
    domain: cookie.host,
    hostOnly: !cookie.isDomain,
    path: cookie.path,
    secure: cookie.isSecure,
    httpOnly: cookie.isHttpOnly,
    session: cookie.isSession,
    storeId: DEFAULT_STORE,
  };

  if (!cookie.isSession) {
    result.expirationDate = cookie.expiry;
  }

  return result;
}

function isSubdomain(otherDomain, baseDomain) {
  return otherDomain == baseDomain || otherDomain.endsWith("." + baseDomain);
}

// Checks that the given extension has permission to set the given cookie for
// the given URI.
function checkSetCookiePermissions(extension, uri, cookie) {
  // Permission checks:
  //
  //  - If the extension does not have permissions for the specified
  //    URL, it cannot set cookies for it.
  //
  //  - If the specified URL could not set the given cookie, neither can
  //    the extension.
  //
  // Ideally, we would just have the cookie service make the latter
  // determination, but that turns out to be quite complicated. At the
  // moment, it requires constructing a cookie string and creating a
  // dummy channel, both of which can be problematic. It also triggers
  // a whole set of additional permission and preference checks, which
  // may or may not be desirable.
  //
  // So instead, we do a similar set of checks here. Exactly what
  // cookies a given URL should be able to set is not well-documented,
  // and is not standardized in any standard that anyone actually
  // follows. So instead, we follow the rules used by the cookie
  // service.
  //
  // See source/netwerk/cookie/nsCookieService.cpp, in particular
  // CheckDomain() and SetCookieInternal().

  if (uri.scheme != "http" && uri.scheme != "https") {
    return false;
  }

  if (!extension.whiteListedHosts.matchesIgnoringPath(uri)) {
    return false;
  }

  if (!cookie.host) {
    // If no explicit host is specified, this becomes a host-only cookie.
    cookie.host = uri.host;
    return true;
  }

  // A leading "." is not expected, but is tolerated if it's not the only
  // character in the host. If there is one, start by stripping it off. We'll
  // add a new one on success.
  if (cookie.host.length > 1) {
    cookie.host = cookie.host.replace(/^\./, "");
  }
  cookie.host = cookie.host.toLowerCase();

  if (cookie.host != uri.host) {
    // Not an exact match, so check for a valid subdomain.
    let baseDomain;
    try {
      baseDomain = Services.eTLD.getBaseDomain(uri);
    } catch (e) {
      if (e.result == Cr.NS_ERROR_HOST_IS_IP_ADDRESS ||
          e.result == Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        // The cookie service uses these to determine whether the domain
        // requires an exact match. We already know we don't have an exact
        // match, so return false. In all other cases, re-raise the error.
        return false;
      }
      throw e;
    }

    // The cookie domain must be a subdomain of the base domain. This prevents
    // us from setting cookies for domains like ".co.uk".
    // The domain of the requesting URL must likewise be a subdomain of the
    // cookie domain. This prevents us from setting cookies for entirely
    // unrelated domains.
    if (!isSubdomain(cookie.host, baseDomain) ||
        !isSubdomain(uri.host, cookie.host)) {
      return false;
    }

    // RFC2109 suggests that we may only add cookies for sub-domains 1-level
    // below us, but enforcing that would break the web, so we don't.
  }

  // An explicit domain was passed, so add a leading "." to make this a
  // domain cookie.
  cookie.host = "." + cookie.host;

  // We don't do any significant checking of path permissions. RFC2109
  // suggests we only allow sites to add cookies for sub-paths, similar to
  // same origin policy enforcement, but no-one implements this.

  return true;
}

function* query(detailsIn, props, extension) {
  // Different callers want to filter on different properties. |props|
  // tells us which ones they're interested in.
  let details = {};
  props.forEach(property => {
    if (detailsIn[property] !== null) {
      details[property] = detailsIn[property];
    }
  });

  if ("domain" in details) {
    details.domain = details.domain.toLowerCase().replace(/^\./, "");
  }

  // We can use getCookiesFromHost for faster searching.
  let enumerator;
  let uri;
  if ("url" in details) {
    try {
      uri = NetUtil.newURI(details.url).QueryInterface(Ci.nsIURL);
      enumerator = Services.cookies.getCookiesFromHost(uri.host);
    } catch (ex) {
      // This often happens for about: URLs
      return;
    }
  } else if ("domain" in details) {
    enumerator = Services.cookies.getCookiesFromHost(details.domain);
  } else {
    enumerator = Services.cookies.enumerator;
  }

  // Based on nsCookieService::GetCookieStringInternal
  function matches(cookie) {
    function domainMatches(host) {
      return cookie.rawHost == host || (cookie.isDomain && host.endsWith(cookie.host));
    }

    function pathMatches(path) {
      let cookiePath = cookie.path.replace(/\/$/, "");

      if (!path.startsWith(cookiePath)) {
        return false;
      }

      // path == cookiePath, but without the redundant string compare.
      if (path.length == cookiePath.length) {
        return true;
      }

      // URL path is a substring of the cookie path, so it matches if, and
      // only if, the next character is a path delimiter.
      let pathDelimiters = ["/", "?", "#", ";"];
      return pathDelimiters.includes(path[cookiePath.length]);
    }

    // "Restricts the retrieved cookies to those that would match the given URL."
    if (uri) {
      if (!domainMatches(uri.host)) {
        return false;
      }

      if (cookie.isSecure && uri.scheme != "https") {
        return false;
      }

      if (!pathMatches(uri.path)) {
        return false;
      }
    }

    if ("name" in details && details.name != cookie.name) {
      return false;
    }

    // "Restricts the retrieved cookies to those whose domains match or are subdomains of this one."
    if ("domain" in details && !isSubdomain(cookie.rawHost, details.domain)) {
      return false;
    }

    // "Restricts the retrieved cookies to those whose path exactly matches this string.""
    if ("path" in details && details.path != cookie.path) {
      return false;
    }

    if ("secure" in details && details.secure != cookie.isSecure) {
      return false;
    }

    if ("session" in details && details.session != cookie.isSession) {
      return false;
    }

    if ("storeId" in details && details.storeId != DEFAULT_STORE) {
      return false;
    }

    // Check that the extension has permissions for this host.
    if (!extension.whiteListedHosts.matchesCookie(cookie)) {
      return false;
    }

    return true;
  }

  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
    if (matches(cookie)) {
      yield cookie;
    }
  }
}

extensions.registerSchemaAPI("cookies", "cookies", (extension, context) => {
  let self = {
    cookies: {
      get: function(details) {
        // FIXME: We don't sort by length of path and creation time.
        for (let cookie of query(details, ["url", "name", "storeId"], extension)) {
          return Promise.resolve(convert(cookie));
        }

        // Found no match.
        return Promise.resolve(null);
      },

      getAll: function(details) {
        let allowed = ["url", "name", "domain", "path", "secure", "session", "storeId"];
        let result = Array.from(query(details, allowed, extension), convert);

        return Promise.resolve(result);
      },

      set: function(details) {
        let uri = NetUtil.newURI(details.url).QueryInterface(Ci.nsIURL);

        let path;
        if (details.path !== null) {
          path = details.path;
        } else {
          // This interface essentially emulates the behavior of the
          // Set-Cookie header. In the case of an omitted path, the cookie
          // service uses the directory path of the requesting URL, ignoring
          // any filename or query parameters.
          path = uri.directory;
        }

        let name = details.name !== null ? details.name : "";
        let value = details.value !== null ? details.value : "";
        let secure = details.secure !== null ? details.secure : false;
        let httpOnly = details.httpOnly !== null ? details.httpOnly : false;
        let isSession = details.expirationDate === null;
        let expiry = isSession ? Number.MAX_SAFE_INTEGER : details.expirationDate;
        // Ignore storeID.

        let cookieAttrs = {host: details.domain, path: path, isSecure: secure};
        if (!checkSetCookiePermissions(extension, uri, cookieAttrs)) {
          return Promise.reject({message: `Permission denied to set cookie ${JSON.stringify(details)}`});
        }

        // The permission check may have modified the domain, so use
        // the new value instead.
        Services.cookies.add(cookieAttrs.host, path, name, value,
                             secure, httpOnly, isSession, expiry);

        return self.cookies.get(details);
      },

      remove: function(details) {
        for (let cookie of query(details, ["url", "name", "storeId"], extension)) {
          Services.cookies.remove(cookie.host, cookie.name, cookie.path, false, cookie.originAttributes);
          // Todo: could there be multiple per subdomain?
          return Promise.resolve({
            url: details.url,
            name: details.name,
            storeId: DEFAULT_STORE,
          });
        }

        return Promise.resolve(null);
      },

      getAllCookieStores: function() {
        // Todo: list all the tabIds for non-private tabs
        return Promise.resolve([{id: DEFAULT_STORE, tabIds: []}]);
      },

      onChanged: new EventManager(context, "cookies.onChanged", fire => {
        let observer = (subject, topic, data) => {
          let notify = (removed, cookie, cause) => {
            cookie.QueryInterface(Ci.nsICookie2);

            if (extension.whiteListedHosts.matchesCookie(cookie)) {
              fire({removed, cookie: convert(cookie), cause});
            }
          };

          // We do our best effort here to map the incompatible states.
          switch (data) {
            case "deleted":
              notify(true, subject, "explicit");
              break;
            case "added":
              notify(false, subject, "explicit");
              break;
            case "changed":
              notify(true, subject, "overwrite");
              notify(false, subject, "explicit");
              break;
            case "batch-deleted":
              subject.QueryInterface(Ci.nsIArray);
              for (let i = 0; i < subject.length; i++) {
                let cookie = subject.queryElementAt(i, Ci.nsICookie2);
                if (!cookie.isSession && cookie.expiry * 1000 <= Date.now()) {
                  notify(true, cookie, "expired");
                } else {
                  notify(true, cookie, "evicted");
                }
              }
              break;
          }
        };

        Services.obs.addObserver(observer, "cookie-changed", false);
        return () => Services.obs.removeObserver(observer, "cookie-changed");
      }).api(),
    },
  };

  return self;
});
