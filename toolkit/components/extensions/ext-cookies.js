const { interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  runSafe,
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
    storeId: DEFAULT_STORE
  }

  if (!cookie.isSession) {
    result.expirationDate = cookie.expiry;
  }

  return result;
}

function* query(allDetails, allowed) {
  let details = {};
  allowed.map(property => {
    if (property in allDetails) {
      details[property] = allDetails[property];
    }
  });

  // We can use getCookiesFromHost for faster searching.
  let enumerator;
  if ("url" in details) {
    try {
      let uri = Services.io.newURI(details.url, null, null);
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
      // Calculate cookie path length, excluding trailing '/'.
      let length = cookie.path.length;
      if (cookie.path.endsWith("/")) {
        length -= 1;
      }

      // If the path is shorter than the cookie path, don't send it back.
      if (!path.startsWith(cookie.path.substring(0, length))) {
        return false;
      }

      let pathDelimiter = ["/", "?", "#", ";"];
      if (path.length > length && !pathDelimiter.includes(path.charAt(length))) {
        return false;
      }

      return true;
    }

    // "Restricts the retrieved cookies to those that would match the given URL."
    if ("url" in details) {
      var uri = Services.io.newURI(details.url, null, null);

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
    if ("domain" in details) {
      if (cookie.rawHost != details.domain &&
         !cookie.rawHost.endsWith("." + details.domain)) {
        return false;
      }
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

    return true;
  }

  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);
    if (matches(cookie)) {
      yield cookie;
    }
  }
}

extensions.registerPrivilegedAPI("cookies", (extension, context) => {
  let self = {
    cookies: {
      get: function(details, callback) {
        if (!details || !details.url || !details.name) {
          throw new Error("Mising required property");
        }

        // FIXME: We don't sort by length of path and creation time.
        for (let cookie of query(details, ["url", "name", "storeId"])) {
          runSafe(context, callback, convert(cookie));
          return;
        }

        // Found no match.
        runSafe(context, callback, null);
      },

      getAll: function(details, callback) {
        if (!details) {
          details = {};
        }

        let allowed = ["url", "name", "domain", "path", "secure", "session", "storeId"];
        let result = [];
        for (let cookie of query(details, allowed)) {
          result.push(convert(cookie));
        }

        runSafe(context, callback, result);
      },

      set: function(details, callback) {
        if (!details || !details.url) {
          throw new Error("Mising required property");
        }

        let uri = Services.io.newURI(details.url, null, null);

        let domain;
        if ("domain" in details) {
          domain = "." + details.domain;
        } else {
          domain = uri.host; // "If omitted, the cookie becomes a host-only cookie."
        }

        let path;
        if ("path" in details) {
          path = details.path;
        } else {
          // Chrome seems to trim the path after the last slash.
          // /x/abc/ddd == /x/abc
          // /xxxx?abc == /
          // We always have at least one slash.
          let index = uri.path.slice(1).lastIndexOf("/");
          if (index == -1) {
            path = "/";
          } else {
            path = uri.path.slice(0, index + 1); // This removes the last slash.
          }
        }

        let name = "name" in details ? details.name : "";
        let value = "value" in details ? details.value : "";
        let secure = "secure" in details ? details.secure : false;
        let httpOnly = "httpOnly" in details ? details.httpOnly : false;
        let isSession = !("expirationDate" in details);
        let expiry = isSession ? 0 : details.expirationDate;
        // Ingore storeID.

        Services.cookies.add(domain, path, name, value, secure, httpOnly, isSession, expiry);
        if (callback) {
          self.cookies.get(details, callback);
        }
      },

      remove: function(details, callback) {
        if (!details || !details.name || !details.url) {
          throw new Error("Mising required property");
        }

        for (let cookie of query(details, ["url", "name", "storeId"])) {
          Services.cookies.remove(cookie.host, cookie.name, cookie.path, false);
          if (callback) {
            runSafe(context, callback, {
              url: details.url,
              name: details.name,
              storeId: DEFAULT_STORE
            });
          }
          // Todo: could there be multiple per subdomain?
          return;
        }

        if (callback) {
          runSafe(context, callback, null);
        }
      },

      getAllCookieStores: function(callback) {
        // Todo: list all the tabIds for non-private tabs
        runSafe(context, callback, [{id: DEFAULT_STORE, tabIds: []}]);
      },

      onChanged: new EventManager(context, "cookies.onChanged", fire => {
        let observer = (subject, topic, data) => {
          let notify = (removed, cookie, cause) => {
            fire({removed, cookie: convert(cookie.QueryInterface(Ci.nsICookie2)), cause});
          }

          // We do our best effort here to map the incompatible states.
          switch (data) {
            case "deleted":
              notify(true, subject, "explicit");
              break;
            case "added":
              notify(false, subject, "explicit");
              break;
            case "changed":
              notify(false, subject, "overwrite");
              break;
            case "batch-deleted":
              for (let i = 0; i < subject.length; subject++) {
                let cookie = subject.queryElementAt(i, Ci.nsICookie2);
                if (!cookie.isSession && cookie.expiry < Date.now()) {
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
