/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

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

  if (!extension.whiteListedHosts.matches(uri)) {
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
 * Query the cookie store for matching cookies.
 * @param {Object} detailsIn
 * @param {Array} props          Properties the extension is interested in matching against.
 * @param {BaseContext} context  The context making the query.
 */
const query = function*(detailsIn, props, context) {
  let details = {};
  props.forEach(property => {
    if (detailsIn[property] !== null) {
      details[property] = detailsIn[property];
    }
  });

  if ("domain" in details) {
    details.domain = details.domain.toLowerCase().replace(/^\./, "");
    details.domain = dropBracketIfIPv6(details.domain);
  }

  let userContextId = 0;
  let isPrivate = context.incognito;
  if (details.storeId) {
    if (!isValidCookieStoreId(details.storeId)) {
      return;
    }

    if (isDefaultCookieStoreId(details.storeId)) {
      isPrivate = false;
    } else if (isPrivateCookieStoreId(details.storeId)) {
      isPrivate = true;
    } else if (isContainerCookieStoreId(details.storeId)) {
      isPrivate = false;
      userContextId = getContainerForCookieStoreId(details.storeId);
      if (!userContextId) {
        return;
      }
    }
  }

  let storeId = DEFAULT_STORE;
  if (isPrivate) {
    storeId = PRIVATE_STORE;
  } else if ("storeId" in details) {
    storeId = details.storeId;
  }
  if (storeId == PRIVATE_STORE && !context.privateBrowsingAllowed) {
    throw new ExtensionError(
      "Extension disallowed access to the private cookies storeId."
    );
  }

  // We can use getCookiesFromHost for faster searching.
  let cookies;
  let host;
  let url;
  let originAttributes = {
    userContextId,
    privateBrowsingId: isPrivate ? 1 : 0,
  };
  if ("firstPartyDomain" in details) {
    originAttributes.firstPartyDomain = details.firstPartyDomain;
  }
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

  if (host && "firstPartyDomain" in originAttributes) {
    // getCookiesFromHost is more efficient than getCookiesWithOriginAttributes
    // if the host and all origin attributes are known.
    cookies = Services.cookies.getCookiesFromHost(host, originAttributes);
  } else {
    cookies = Services.cookies.getCookiesWithOriginAttributes(
      JSON.stringify(originAttributes),
      host
    );
  }

  // Based on CookieService::GetCookieStringInternal
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
    if (!context.extension.whiteListedHosts.matchesCookie(cookie)) {
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

const normalizeFirstPartyDomain = details => {
  if (details.firstPartyDomain != null) {
    return;
  }
  if (Services.prefs.getBoolPref("privacy.firstparty.isolate")) {
    throw new ExtensionError(
      "First-Party Isolation is enabled, but the required 'firstPartyDomain' attribute was not set."
    );
  }

  // When FPI is disabled, the "firstPartyDomain" attribute is optional
  // and defaults to the empty string.
  details.firstPartyDomain = "";
};

this.cookies = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;
    let self = {
      cookies: {
        get: function(details) {
          normalizeFirstPartyDomain(details);

          // FIXME: We don't sort by length of path and creation time.
          let allowed = ["url", "name", "storeId", "firstPartyDomain"];
          for (let cookie of query(details, allowed, context)) {
            return Promise.resolve(convertCookie(cookie));
          }

          // Found no match.
          return Promise.resolve(null);
        },

        getAll: function(details) {
          if (!("firstPartyDomain" in details)) {
            normalizeFirstPartyDomain(details);
          }

          let allowed = [
            "url",
            "name",
            "domain",
            "path",
            "secure",
            "session",
            "storeId",
          ];

          // firstPartyDomain may be set to null or undefined to not filter by FPD.
          if (details.firstPartyDomain != null) {
            allowed.push("firstPartyDomain");
          }

          let result = Array.from(
            query(details, allowed, context),
            convertCookie
          );

          return Promise.resolve(result);
        },

        set: function(details) {
          normalizeFirstPartyDomain(details);

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
          let isPrivate = context.incognito;
          let userContextId = 0;
          if (isDefaultCookieStoreId(details.storeId)) {
            isPrivate = false;
          } else if (isPrivateCookieStoreId(details.storeId)) {
            if (!context.privateBrowsingAllowed) {
              return Promise.reject({
                message:
                  "Extension disallowed access to the private cookies storeId.",
              });
            }
            isPrivate = true;
          } else if (isContainerCookieStoreId(details.storeId)) {
            let containerId = getContainerForCookieStoreId(details.storeId);
            if (containerId === null) {
              return Promise.reject({
                message: `Illegal storeId: ${details.storeId}`,
              });
            }
            isPrivate = false;
            userContextId = containerId;
          } else if (details.storeId !== null) {
            return Promise.reject({ message: "Unknown storeId" });
          }

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

          let originAttributes = {
            userContextId,
            privateBrowsingId: isPrivate ? 1 : 0,
            firstPartyDomain: details.firstPartyDomain,
          };

          let sameSite = SAME_SITE_STATUSES.indexOf(details.sameSite);

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
            sameSite
          );

          return self.cookies.get(details);
        },

        remove: function(details) {
          normalizeFirstPartyDomain(details);

          let allowed = ["url", "name", "storeId", "firstPartyDomain"];
          for (let { cookie, storeId } of query(details, allowed, context)) {
            if (
              isPrivateCookieStoreId(details.storeId) &&
              !context.privateBrowsingAllowed
            ) {
              return Promise.reject({ message: "Unknown storeId" });
            }
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
              firstPartyDomain: details.firstPartyDomain,
            });
          }

          return Promise.resolve(null);
        },

        getAllCookieStores: function() {
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
          name: "cookies.onChanged",
          register: fire => {
            let observer = (subject, topic, data) => {
              let notify = (removed, cookie, cause) => {
                cookie.QueryInterface(Ci.nsICookie);

                if (extension.whiteListedHosts.matchesCookie(cookie)) {
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
                    let cookie = subject.queryElementAt(i, Ci.nsICookie);
                    if (
                      !cookie.isSession &&
                      cookie.expiry * 1000 <= Date.now()
                    ) {
                      notify(true, cookie, "expired");
                    } else {
                      notify(true, cookie, "evicted");
                    }
                  }
                  break;
              }
            };

            Services.obs.addObserver(observer, "cookie-changed");
            if (context.privateBrowsingAllowed) {
              Services.obs.addObserver(observer, "private-cookie-changed");
            }
            return () => {
              Services.obs.removeObserver(observer, "cookie-changed");
              if (context.privateBrowsingAllowed) {
                Services.obs.removeObserver(observer, "private-cookie-changed");
              }
            };
          },
        }).api(),
      },
    };

    return self;
  }
};
