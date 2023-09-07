/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals DEFAULT_STORE, PRIVATE_STORE */

var { ExtensionError } = ExtensionUtils;

const SAME_SITE_STATUSES = [
  "no_restriction", // Index 0 = Ci.nsICookie.SAMESITE_NONE
  "lax", // Index 1 = Ci.nsICookie.SAMESITE_LAX
  "strict", // Index 2 = Ci.nsICookie.SAMESITE_STRICT
];

const isIPv4 = host => {
  let match = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/.exec(host);

  if (match) {
    return match[1] < 256 && match[2] < 256 && match[3] < 256 && match[4] < 256;
  }
  return false;
};
const isIPv6 = host => host.includes(":");
const addBracketIfIPv6 = host =>
  isIPv6(host) && !host.startsWith("[") ? `[${host}]` : host;
const dropBracketIfIPv6 = host =>
  isIPv6(host) && host.startsWith("[") && host.endsWith("]")
    ? host.slice(1, -1)
    : host;

// Converts the partitionKey format of the extension API (i.e. PartitionKey) to
// a valid format for the "partitionKey" member of OriginAttributes.
function fromExtPartitionKey(extPartitionKey) {
  if (!extPartitionKey) {
    // Unpartitioned by default.
    return "";
  }
  const { topLevelSite } = extPartitionKey;
  // TODO: Expand API to force the generation of a partitionKey that differs
  // from the default that's specified by privacy.dynamic_firstparty.use_site.
  if (topLevelSite) {
    // If topLevelSite is set and a non-empty string (a site in a URL format).
    try {
      return ChromeUtils.getPartitionKeyFromURL(topLevelSite);
    } catch (e) {
      throw new ExtensionError("Invalid value for 'partitionKey' attribute");
    }
  }
  // Unpartitioned.
  return "";
}
// Converts an internal partitionKey (format used by OriginAttributes) to the
// string value as exposed through the extension API.
function toExtPartitionKey(partitionKey) {
  if (!partitionKey) {
    // Canonical representation of an empty partitionKey is null.
    // In theory {topLevelSite: ""} also works, but alas.
    return null;
  }
  // Parse partitionKey in order to generate the desired return type (URL).
  // OriginAttributes::ParsePartitionKey cannot be used because it assumes that
  // the input matches the format of the privacy.dynamic_firstparty.use_site
  // pref, which is not necessarily the case for cookies before the pref flip.
  if (!partitionKey.startsWith("(")) {
    // A partitionKey generated with privacy.dynamic_firstparty.use_site=false.
    return { topLevelSite: `https://${partitionKey}` };
  }
  // partitionKey starts with "(" and ends with ")".
  let [scheme, domain, port] = partitionKey.slice(1, -1).split(",");
  let topLevelSite = `${scheme}://${domain}`;
  if (port) {
    topLevelSite += `:${port}`;
  }
  return { topLevelSite };
}

const convertCookie = ({ cookie, isPrivate }) => {
  let result = {
    name: cookie.name,
    value: cookie.value,
    domain: addBracketIfIPv6(cookie.host),
    hostOnly: !cookie.isDomain,
    path: cookie.path,
    secure: cookie.isSecure,
    httpOnly: cookie.isHttpOnly,
    sameSite: SAME_SITE_STATUSES[cookie.sameSite],
    session: cookie.isSession,
    firstPartyDomain: cookie.originAttributes.firstPartyDomain || "",
    partitionKey: toExtPartitionKey(cookie.originAttributes.partitionKey),
  };

  if (!cookie.isSession) {
    result.expirationDate = cookie.expiry;
  }

  if (cookie.originAttributes.userContextId) {
    result.storeId = getCookieStoreIdForContainer(
      cookie.originAttributes.userContextId
    );
  } else if (cookie.originAttributes.privateBrowsingId || isPrivate) {
    result.storeId = PRIVATE_STORE;
  } else {
    result.storeId = DEFAULT_STORE;
  }

  return result;
};

const isSubdomain = (otherDomain, baseDomain) => {
  return otherDomain == baseDomain || otherDomain.endsWith("." + baseDomain);
};

// Checks that the given extension has permission to set the given cookie for
// the given URI.
const checkSetCookiePermissions = (extension, uri, cookie) => {
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
  // See source/netwerk/cookie/CookieService.cpp, in particular
  // CheckDomain() and SetCookieInternal().

  if (uri.scheme != "http" && uri.scheme != "https") {
    return false;
  }

  if (!extension.allowedOrigins.matches(uri)) {
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
  cookie.host = dropBracketIfIPv6(cookie.host);

  if (cookie.host != uri.host) {
    // Not an exact match, so check for a valid subdomain.
    let baseDomain;
    try {
      baseDomain = Services.eTLD.getBaseDomain(uri);
    } catch (e) {
      if (
        e.result == Cr.NS_ERROR_HOST_IS_IP_ADDRESS ||
        e.result == Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS
      ) {
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
    if (
      !isSubdomain(cookie.host, baseDomain) ||
      !isSubdomain(uri.host, cookie.host)
    ) {
      return false;
    }

    // RFC2109 suggests that we may only add cookies for sub-domains 1-level
    // below us, but enforcing that would break the web, so we don't.
  }

  // If the host is an IP address, avoid adding a leading ".".
  // An IP address is not a domain name, and only supports host-only cookies.
  if (isIPv6(cookie.host) || isIPv4(cookie.host)) {
    return true;
  }

  // An explicit domain was passed, so add a leading "." to make this a
  // domain cookie.
  cookie.host = "." + cookie.host;

  // We don't do any significant checking of path permissions. RFC2109
  // suggests we only allow sites to add cookies for sub-paths, similar to
  // same origin policy enforcement, but no-one implements this.

  return true;
};

/**
 * Converts the details received from the cookies API to the OriginAttributes
 * format, using default values when needed (firstPartyDomain/partitionKey).
 *
 * If allowPattern is true, an OriginAttributesPattern may be returned instead.
 *
 * @param {object} details
 *        The details received from the extension.
 * @param {BaseContext} context
 * @param {boolean} allowPattern
 *        Whether to potentially return an OriginAttributesPattern instead of
 *        OriginAttributes. The get/set/remove cookie methods operate on exact
 *        OriginAttributes, the getAll method allows a partial pattern and may
 *        potentially match cookies with distinct origin attributes.
 * @returns {object} An object with the following properties:
 *  - originAttributes {OriginAttributes|OriginAttributesPattern}
 *  - isPattern {boolean} Whether originAttributes is a pattern.
 *  - isPrivate {boolean} Whether the cookie belongs to private browsing mode.
 *  - storeId {string} The storeId of the cookie.
 */
const oaFromDetails = (details, context, allowPattern) => {
  // Default values, may be filled in based on details.
  let originAttributes = {
    userContextId: 0,
    privateBrowsingId: 0,
    // The following two keys may be deleted if allowPattern=true
    firstPartyDomain: details.firstPartyDomain ?? "",
    partitionKey: fromExtPartitionKey(details.partitionKey),
  };

  let isPrivate = context.incognito;
  let storeId = isPrivate ? PRIVATE_STORE : DEFAULT_STORE;
  if (details.storeId) {
    storeId = details.storeId;
    if (isDefaultCookieStoreId(storeId)) {
      isPrivate = false;
    } else if (isPrivateCookieStoreId(storeId)) {
      isPrivate = true;
    } else {
      isPrivate = false;
      let userContextId = getContainerForCookieStoreId(storeId);
      if (!userContextId) {
        throw new ExtensionError(`Invalid cookie store id: "${storeId}"`);
      }
      originAttributes.userContextId = userContextId;
    }
  }

  if (isPrivate) {
    originAttributes.privateBrowsingId = 1;
    if (!context.privateBrowsingAllowed) {
      throw new ExtensionError(
        "Extension disallowed access to the private cookies storeId."
      );
    }
  }

  // If any of the originAttributes's keys are deleted, this becomes true.
  let isPattern = false;
  if (allowPattern) {
    // firstPartyDomain is unset / void / string.
    // If unset, then we default to non-FPI cookies (or if FPI is enabled,
    // an error is thrown by validateFirstPartyDomain). We are able to detect
    // whether the property is set due to "omit-key-if-missing" in cookies.json.
    // If set to a string, we keep the filter.
    // If set to void (undefined / null), we drop the FPI filter:
    if ("firstPartyDomain" in details && details.firstPartyDomain == null) {
      delete originAttributes.firstPartyDomain;
      isPattern = true;
    }

    // partitionKey is an object or null.
    // null implies the default (unpartitioned cookies).
    // An object is a filter for partitionKey; currently we require topLevelSite
    // to be set to determine the exact partitionKey. Without it, we drop the
    // dFPI filter:
    if (details.partitionKey && details.partitionKey.topLevelSite == null) {
      delete originAttributes.partitionKey;
      isPattern = true;
    }
  }
  return { originAttributes, isPattern, isPrivate, storeId };
};

/**
 * Query the cookie store for matching cookies.
 *
 * @param {object} detailsIn
 * @param {Array} props          Properties the extension is interested in matching against.
 *                               The firstPartyDomain / partitionKey / storeId
 *                               props are always accounted for.
 * @param {BaseContext} context  The context making the query.
 * @param {boolean} allowPattern Whether to allow the query to match distinct
 *                               origin attributes instead of falling back to
 *                               default values. See the oaFromDetails method.
 */
const query = function* (detailsIn, props, context, allowPattern) {
  let details = {};
  props.forEach(property => {
    if (detailsIn[property] !== null) {
      details[property] = detailsIn[property];
    }
  });

  let parsedOA;
  try {
    parsedOA = oaFromDetails(detailsIn, context, allowPattern);
  } catch (e) {
    if (e.message.startsWith("Invalid cookie store id")) {
      // For backwards-compatibility with previous versions of Firefox, fail
      // silently (by not returning any results) instead of throwing an error.
      return;
    }
    throw e;
  }
  let { originAttributes, isPattern, isPrivate, storeId } = parsedOA;

  if ("domain" in details) {
    details.domain = details.domain.toLowerCase().replace(/^\./, "");
    details.domain = dropBracketIfIPv6(details.domain);
  }

  // We can use getCookiesFromHost for faster searching.
  let cookies;
  let host;
  let url;
  if ("url" in details) {
    try {
      url = new URL(details.url);
      host = dropBracketIfIPv6(url.hostname);
    } catch (ex) {
      // This often happens for about: URLs
      return;
    }
  } else if ("domain" in details) {
    host = details.domain;
  }

  if (host && !isPattern) {
    // getCookiesFromHost is more efficient than getCookiesWithOriginAttributes
    // if the host and all origin attributes are known.
    cookies = Services.cookies.getCookiesFromHost(host, originAttributes);
  } else {
    cookies = Services.cookies.getCookiesWithOriginAttributes(
      JSON.stringify(originAttributes),
      host
    );
  }

  // Based on CookieService::GetCookieStringFromHttp
  function matches(cookie) {
    function domainMatches(host) {
      return (
        cookie.rawHost == host ||
        (cookie.isDomain && host.endsWith(cookie.host))
      );
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
      return path[cookiePath.length] === "/";
    }

    // "Restricts the retrieved cookies to those that would match the given URL."
    if (url) {
      if (!domainMatches(host)) {
        return false;
      }

      if (cookie.isSecure && url.protocol != "https:") {
        return false;
      }

      if (!pathMatches(url.pathname)) {
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

    // Check that the extension has permissions for this host.
    if (!context.extension.allowedOrigins.matchesCookie(cookie)) {
      return false;
    }

    return true;
  }

  for (const cookie of cookies) {
    if (matches(cookie)) {
      yield { cookie, isPrivate, storeId };
    }
  }
};

const validateFirstPartyDomain = details => {
  if (details.firstPartyDomain != null) {
    return;
  }
  if (Services.prefs.getBoolPref("privacy.firstparty.isolate")) {
    throw new ExtensionError(
      "First-Party Isolation is enabled, but the required 'firstPartyDomain' attribute was not set."
    );
  }
};

this.cookies = class extends ExtensionAPIPersistent {
  PERSISTENT_EVENTS = {
    onChanged({ fire }) {
      let observer = (subject, topic) => {
        let notify = (removed, cookie, cause) => {
          cookie.QueryInterface(Ci.nsICookie);

          if (this.extension.allowedOrigins.matchesCookie(cookie)) {
            fire.async({
              removed,
              cookie: convertCookie({
                cookie,
                isPrivate: topic == "private-cookie-changed",
              }),
              cause,
            });
          }
        };

        let notification = subject.QueryInterface(Ci.nsICookieNotification);
        let { cookie } = notification;

        let {
          COOKIE_DELETED,
          COOKIE_ADDED,
          COOKIE_CHANGED,
          COOKIES_BATCH_DELETED,
        } = Ci.nsICookieNotification;

        // We do our best effort here to map the incompatible states.
        switch (notification.action) {
          case COOKIE_DELETED:
            notify(true, cookie, "explicit");
            break;
          case COOKIE_ADDED:
            notify(false, cookie, "explicit");
            break;
          case COOKIE_CHANGED:
            notify(true, cookie, "overwrite");
            notify(false, cookie, "explicit");
            break;
          case COOKIES_BATCH_DELETED:
            let cookieArray = notification.batchDeletedCookies.QueryInterface(
              Ci.nsIArray
            );
            for (let i = 0; i < cookieArray.length; i++) {
              let cookie = cookieArray.queryElementAt(i, Ci.nsICookie);
              if (!cookie.isSession && cookie.expiry * 1000 <= Date.now()) {
                notify(true, cookie, "expired");
              } else {
                notify(true, cookie, "evicted");
              }
            }
            break;
        }
      };

      const { privateBrowsingAllowed } = this.extension;
      Services.obs.addObserver(observer, "cookie-changed");
      if (privateBrowsingAllowed) {
        Services.obs.addObserver(observer, "private-cookie-changed");
      }
      return {
        unregister() {
          Services.obs.removeObserver(observer, "cookie-changed");
          if (privateBrowsingAllowed) {
            Services.obs.removeObserver(observer, "private-cookie-changed");
          }
        },
        convert(_fire) {
          fire = _fire;
        },
      };
    },
  };
  getAPI(context) {
    let { extension } = context;
    let self = {
      cookies: {
        get: function (details) {
          validateFirstPartyDomain(details);

          // TODO bug 1818968: We don't sort by length of path and creation time.
          let allowed = ["url", "name"];
          for (let cookie of query(details, allowed, context)) {
            return Promise.resolve(convertCookie(cookie));
          }

          // Found no match.
          return Promise.resolve(null);
        },

        getAll: function (details) {
          if (!("firstPartyDomain" in details)) {
            // Check and throw an error if firstPartyDomain is required.
            validateFirstPartyDomain(details);
          }

          let allowed = ["url", "name", "domain", "path", "secure", "session"];
          let result = Array.from(
            query(details, allowed, context, /* allowPattern = */ true),
            convertCookie
          );

          return Promise.resolve(result);
        },

        set: function (details) {
          validateFirstPartyDomain(details);
          if (details.firstPartyDomain && details.partitionKey) {
            // FPI and dFPI are mutually exclusive, so it does not make sense
            // to accept non-empty (i.e. non-default) values for both.
            throw new ExtensionError(
              "Partitioned cookies cannot have a 'firstPartyDomain' attribute."
            );
          }

          let uri = Services.io.newURI(details.url);

          let path;
          if (details.path !== null) {
            path = details.path;
          } else {
            // This interface essentially emulates the behavior of the
            // Set-Cookie header. In the case of an omitted path, the cookie
            // service uses the directory path of the requesting URL, ignoring
            // any filename or query parameters.
            path = uri.QueryInterface(Ci.nsIURL).directory;
          }

          let name = details.name !== null ? details.name : "";
          let value = details.value !== null ? details.value : "";
          let secure = details.secure !== null ? details.secure : false;
          let httpOnly = details.httpOnly !== null ? details.httpOnly : false;
          let isSession = details.expirationDate === null;
          let expiry = isSession
            ? Number.MAX_SAFE_INTEGER
            : details.expirationDate;

          let { originAttributes } = oaFromDetails(details, context);

          let cookieAttrs = {
            host: details.domain,
            path: path,
            isSecure: secure,
          };
          if (!checkSetCookiePermissions(extension, uri, cookieAttrs)) {
            return Promise.reject({
              message: `Permission denied to set cookie ${JSON.stringify(
                details
              )}`,
            });
          }

          let sameSite = SAME_SITE_STATUSES.indexOf(details.sameSite);

          let schemeType = Ci.nsICookie.SCHEME_UNSET;
          if (uri.scheme === "https") {
            schemeType = Ci.nsICookie.SCHEME_HTTPS;
          } else if (uri.scheme === "http") {
            schemeType = Ci.nsICookie.SCHEME_HTTP;
          } else if (uri.scheme === "file") {
            schemeType = Ci.nsICookie.SCHEME_FILE;
          }

          // The permission check may have modified the domain, so use
          // the new value instead.
          Services.cookies.add(
            cookieAttrs.host,
            path,
            name,
            value,
            secure,
            httpOnly,
            isSession,
            expiry,
            originAttributes,
            sameSite,
            schemeType
          );

          return self.cookies.get(details);
        },

        remove: function (details) {
          validateFirstPartyDomain(details);

          let allowed = ["url", "name"];
          for (let { cookie, storeId } of query(details, allowed, context)) {
            Services.cookies.remove(
              cookie.host,
              cookie.name,
              cookie.path,
              cookie.originAttributes
            );

            // TODO Bug 1387957: could there be multiple per subdomain?
            return Promise.resolve({
              url: details.url,
              name: details.name,
              storeId,
              firstPartyDomain: cookie.originAttributes.firstPartyDomain,
              partitionKey: toExtPartitionKey(
                cookie.originAttributes.partitionKey
              ),
            });
          }

          return Promise.resolve(null);
        },

        getAllCookieStores: function () {
          let data = {};
          for (let tab of extension.tabManager.query()) {
            if (!(tab.cookieStoreId in data)) {
              data[tab.cookieStoreId] = [];
            }
            data[tab.cookieStoreId].push(tab.id);
          }

          let result = [];
          for (let key in data) {
            result.push({
              id: key,
              tabIds: data[key],
              incognito: key == PRIVATE_STORE,
            });
          }
          return Promise.resolve(result);
        },

        onChanged: new EventManager({
          context,
          module: "cookies",
          event: "onChanged",
          extensionApi: this,
        }).api(),
      },
    };

    return self;
  }
};
