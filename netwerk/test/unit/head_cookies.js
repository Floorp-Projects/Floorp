/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* import-globals-from head_cache.js */

"use strict";

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

XPCOMUtils.defineLazyServiceGetter(
  Services,
  "cookies",
  "@mozilla.org/cookieService;1",
  "nsICookieService"
);
XPCOMUtils.defineLazyServiceGetter(
  Services,
  "cookiemgr",
  "@mozilla.org/cookiemanager;1",
  "nsICookieManager"
);

XPCOMUtils.defineLazyServiceGetter(
  Services,
  "etld",
  "@mozilla.org/network/effective-tld-service;1",
  "nsIEffectiveTLDService"
);

function do_check_throws(f, result, stack) {
  if (!stack) {
    stack = Components.stack.caller;
  }

  try {
    f();
  } catch (exc) {
    if (exc.result == result) {
      return;
    }
    do_throw("expected result " + result + ", caught " + exc, stack);
  }
  do_throw("expected result " + result + ", none thrown", stack);
}

// Helper to step a generator function and catch a StopIteration exception.
function do_run_generator(generator) {
  try {
    generator.next();
  } catch (e) {
    do_throw("caught exception " + e, Components.stack.caller);
  }
}

// Helper to finish a generator function test.
function do_finish_generator_test(generator) {
  executeSoon(function() {
    generator.return();
    do_test_finished();
  });
}

function _observer(generator, topic) {
  Services.obs.addObserver(this, topic);

  this.generator = generator;
  this.topic = topic;
}

_observer.prototype = {
  observe(subject, topic, data) {
    Assert.equal(this.topic, topic);

    Services.obs.removeObserver(this, this.topic);

    // Continue executing the generator function.
    if (this.generator) {
      do_run_generator(this.generator);
    }

    this.generator = null;
    this.topic = null;
  },
};

// Close the cookie database. If a generator is supplied, it will be invoked
// once the close is complete.
function do_close_profile(generator) {
  // Register an observer for db close.
  let obs = new _observer(generator, "cookie-db-closed");

  // Close the db.
  let service = Services.cookies.QueryInterface(Ci.nsIObserver);
  service.observe(null, "profile-before-change", "shutdown-persist");
}

// Load the cookie database. If a generator is supplied, it will be invoked
// once the load is complete.
function do_load_profile(generator) {
  // Register an observer for read completion.
  let obs = new _observer(generator, "cookie-db-read");

  // Load the profile.
  let service = Services.cookies.QueryInterface(Ci.nsIObserver);
  service.observe(null, "profile-do-change", "");
}

// Set a single session cookie using http and test the cookie count
// against 'expected'
function do_set_single_http_cookie(uri, channel, expected) {
  Services.cookies.setCookieStringFromHttp(uri, null, "foo=bar", channel);
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri.host), expected);
}

// Set four cookies; with & without channel, http and non-http; and test
// the cookie count against 'expected' after each set.
function do_set_cookies(uri, channel, session, expected) {
  let suffix = session ? "" : "; max-age=1000";

  // without channel
  Services.cookies.setCookieString(uri, "oh=hai" + suffix, null);
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri.host), expected[0]);
  // with channel
  Services.cookies.setCookieString(uri, "can=has" + suffix, channel);
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri.host), expected[1]);
  // without channel, from http
  Services.cookies.setCookieStringFromHttp(
    uri,
    null,
    "cheez=burger" + suffix,
    null
  );
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri.host), expected[2]);
  // with channel, from http
  Services.cookies.setCookieStringFromHttp(
    uri,
    null,
    "hot=dog" + suffix,
    channel
  );
  Assert.equal(Services.cookiemgr.countCookiesFromHost(uri.host), expected[3]);
}

function do_count_cookies() {
  return Services.cookiemgr.cookies.length;
}

// Helper object to store cookie data.
function Cookie(
  name,
  value,
  host,
  path,
  expiry,
  lastAccessed,
  creationTime,
  isSession,
  isSecure,
  isHttpOnly,
  inBrowserElement = false,
  originAttributes = {},
  sameSite = Ci.nsICookie.SAMESITE_NONE,
  rawSameSite = Ci.nsICookie.SAMESITE_NONE
) {
  this.name = name;
  this.value = value;
  this.host = host;
  this.path = path;
  this.expiry = expiry;
  this.lastAccessed = lastAccessed;
  this.creationTime = creationTime;
  this.isSession = isSession;
  this.isSecure = isSecure;
  this.isHttpOnly = isHttpOnly;
  this.inBrowserElement = inBrowserElement;
  this.originAttributes = originAttributes;
  this.sameSite = sameSite;
  this.rawSameSite = rawSameSite;

  let strippedHost = host.charAt(0) == "." ? host.slice(1) : host;

  try {
    this.baseDomain = Services.etld.getBaseDomainFromHost(strippedHost);
  } catch (e) {
    if (
      e.result == Cr.NS_ERROR_HOST_IS_IP_ADDRESS ||
      e.result == Cr.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS
    ) {
      this.baseDomain = strippedHost;
    }
  }
}

// Object representing a database connection and associated statements. The
// implementation varies depending on schema version.
function CookieDatabaseConnection(file, schema) {
  // Manually generate a cookies.sqlite file with appropriate rows, columns,
  // and schema version. If it already exists, just set up our statements.
  let exists = file.exists();

  this.db = Services.storage.openDatabase(file);
  this.schema = schema;
  if (!exists) {
    this.db.schemaVersion = schema;
  }

  switch (schema) {
    case 1: {
      if (!exists) {
        this.db.executeSimpleSQL(
          "CREATE TABLE moz_cookies (       \
             id INTEGER PRIMARY KEY,        \
             name TEXT,                     \
             value TEXT,                    \
             host TEXT,                     \
             path TEXT,                     \
             expiry INTEGER,                \
             isSecure INTEGER,              \
             isHttpOnly INTEGER)"
        );
      }

      this.stmtInsert = this.db.createStatement(
        "INSERT INTO moz_cookies (        \
           id,                            \
           name,                          \
           value,                         \
           host,                          \
           path,                          \
           expiry,                        \
           isSecure,                      \
           isHttpOnly)                    \
           VALUES (                       \
           :id,                           \
           :name,                         \
           :value,                        \
           :host,                         \
           :path,                         \
           :expiry,                       \
           :isSecure,                     \
           :isHttpOnly)"
      );

      this.stmtDelete = this.db.createStatement(
        "DELETE FROM moz_cookies WHERE id = :id"
      );

      break;
    }

    case 2: {
      if (!exists) {
        this.db.executeSimpleSQL(
          "CREATE TABLE moz_cookies (       \
             id INTEGER PRIMARY KEY,        \
             name TEXT,                     \
             value TEXT,                    \
             host TEXT,                     \
             path TEXT,                     \
             expiry INTEGER,                \
             lastAccessed INTEGER,          \
             isSecure INTEGER,              \
             isHttpOnly INTEGER)"
        );
      }

      this.stmtInsert = this.db.createStatement(
        "INSERT OR REPLACE INTO moz_cookies ( \
           id,                            \
           name,                          \
           value,                         \
           host,                          \
           path,                          \
           expiry,                        \
           lastAccessed,                  \
           isSecure,                      \
           isHttpOnly)                    \
           VALUES (                       \
           :id,                           \
           :name,                         \
           :value,                        \
           :host,                         \
           :path,                         \
           :expiry,                       \
           :lastAccessed,                 \
           :isSecure,                     \
           :isHttpOnly)"
      );

      this.stmtDelete = this.db.createStatement(
        "DELETE FROM moz_cookies WHERE id = :id"
      );

      this.stmtUpdate = this.db.createStatement(
        "UPDATE moz_cookies SET lastAccessed = :lastAccessed WHERE id = :id"
      );

      break;
    }

    case 3: {
      if (!exists) {
        this.db.executeSimpleSQL(
          "CREATE TABLE moz_cookies (       \
            id INTEGER PRIMARY KEY,         \
            baseDomain TEXT,                \
            name TEXT,                      \
            value TEXT,                     \
            host TEXT,                      \
            path TEXT,                      \
            expiry INTEGER,                 \
            lastAccessed INTEGER,           \
            isSecure INTEGER,               \
            isHttpOnly INTEGER)"
        );

        this.db.executeSimpleSQL(
          "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain)"
        );
      }

      this.stmtInsert = this.db.createStatement(
        "INSERT INTO moz_cookies (        \
           id,                            \
           baseDomain,                    \
           name,                          \
           value,                         \
           host,                          \
           path,                          \
           expiry,                        \
           lastAccessed,                  \
           isSecure,                      \
           isHttpOnly)                    \
           VALUES (                       \
           :id,                           \
           :baseDomain,                   \
           :name,                         \
           :value,                        \
           :host,                         \
           :path,                         \
           :expiry,                       \
           :lastAccessed,                 \
           :isSecure,                     \
           :isHttpOnly)"
      );

      this.stmtDelete = this.db.createStatement(
        "DELETE FROM moz_cookies WHERE id = :id"
      );

      this.stmtUpdate = this.db.createStatement(
        "UPDATE moz_cookies SET lastAccessed = :lastAccessed WHERE id = :id"
      );

      break;
    }

    case 4: {
      if (!exists) {
        this.db.executeSimpleSQL(
          "CREATE TABLE moz_cookies (       \
            id INTEGER PRIMARY KEY,         \
            baseDomain TEXT,                \
            name TEXT,                      \
            value TEXT,                     \
            host TEXT,                      \
            path TEXT,                      \
            expiry INTEGER,                 \
            lastAccessed INTEGER,           \
            creationTime INTEGER,           \
            isSecure INTEGER,               \
            isHttpOnly INTEGER              \
            CONSTRAINT moz_uniqueid UNIQUE (name, host, path))"
        );

        this.db.executeSimpleSQL(
          "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain)"
        );

        this.db.executeSimpleSQL("PRAGMA journal_mode = WAL");
      }

      this.stmtInsert = this.db.createStatement(
        "INSERT INTO moz_cookies (        \
           baseDomain,                    \
           name,                          \
           value,                         \
           host,                          \
           path,                          \
           expiry,                        \
           lastAccessed,                  \
           creationTime,                  \
           isSecure,                      \
           isHttpOnly)                    \
           VALUES (                       \
           :baseDomain,                   \
           :name,                         \
           :value,                        \
           :host,                         \
           :path,                         \
           :expiry,                       \
           :lastAccessed,                 \
           :creationTime,                 \
           :isSecure,                     \
           :isHttpOnly)"
      );

      this.stmtDelete = this.db.createStatement(
        "DELETE FROM moz_cookies          \
           WHERE name = :name AND host = :host AND path = :path"
      );

      this.stmtUpdate = this.db.createStatement(
        "UPDATE moz_cookies SET lastAccessed = :lastAccessed \
           WHERE name = :name AND host = :host AND path = :path"
      );

      break;
    }

    case 10: {
      if (!exists) {
        this.db.executeSimpleSQL(
          "CREATE TABLE moz_cookies (                  \
            id INTEGER PRIMARY KEY,                    \
            baseDomain TEXT,                           \
            originAttributes TEXT NOT NULL DEFAULT '', \
            name TEXT,                                 \
            value TEXT,                                \
            host TEXT,                                 \
            path TEXT,                                 \
            expiry INTEGER,                            \
            lastAccessed INTEGER,                      \
            creationTime INTEGER,                      \
            isSecure INTEGER,                          \
            isHttpOnly INTEGER,                        \
            inBrowserElement INTEGER DEFAULT 0,        \
            sameSite INTEGER DEFAULT 0,                \
            rawSameSite INTEGER DEFAULT 0,             \
            CONSTRAINT moz_uniqueid UNIQUE (name, host, path, originAttributes))"
        );

        this.db.executeSimpleSQL(
          "CREATE INDEX moz_basedomain ON moz_cookies (baseDomain)"
        );

        this.db.executeSimpleSQL("PRAGMA journal_mode = WAL");
        this.db.executeSimpleSQL("PRAGMA wal_autocheckpoint = 16");
      }

      this.stmtInsert = this.db.createStatement(
        "INSERT INTO moz_cookies (        \
           name,                          \
           value,                         \
           host,                          \
           baseDomain,                    \
           path,                          \
           expiry,                        \
           lastAccessed,                  \
           creationTime,                  \
           isSecure,                      \
           isHttpOnly,                    \
           inBrowserElement,              \
           originAttributes,              \
           sameSite,                      \
           rawSameSite                    \
         ) VALUES (                       \
           :name,                         \
           :value,                        \
           :host,                         \
           :baseDomain,                   \
           :path,                         \
           :expiry,                       \
           :lastAccessed,                 \
           :creationTime,                 \
           :isSecure,                     \
           :isHttpOnly,                   \
           :inBrowserElement,             \
           :originAttributes,             \
           :sameSite,                     \
           :rawSameSite)"
      );

      this.stmtDelete = this.db.createStatement(
        "DELETE FROM moz_cookies          \
           WHERE name = :name AND host = :host AND path = :path AND \
                 originAttributes = :originAttributes"
      );

      this.stmtUpdate = this.db.createStatement(
        "UPDATE moz_cookies SET lastAccessed = :lastAccessed \
           WHERE name = :name AND host = :host AND path = :path AND \
                 originAttributes = :originAttributes"
      );

      break;
    }

    case 11: {
      if (!exists) {
        this.db.executeSimpleSQL(
          "CREATE TABLE moz_cookies (                  \
            id INTEGER PRIMARY KEY,                    \
            originAttributes TEXT NOT NULL DEFAULT '', \
            name TEXT,                                 \
            value TEXT,                                \
            host TEXT,                                 \
            path TEXT,                                 \
            expiry INTEGER,                            \
            lastAccessed INTEGER,                      \
            creationTime INTEGER,                      \
            isSecure INTEGER,                          \
            isHttpOnly INTEGER,                        \
            inBrowserElement INTEGER DEFAULT 0,        \
            sameSite INTEGER DEFAULT 0,                \
            rawSameSite INTEGER DEFAULT 0,             \
            CONSTRAINT moz_uniqueid UNIQUE (name, host, path, originAttributes))"
        );

        this.db.executeSimpleSQL("PRAGMA journal_mode = WAL");
        this.db.executeSimpleSQL("PRAGMA wal_autocheckpoint = 16");
      }

      this.stmtInsert = this.db.createStatement(
        "INSERT INTO moz_cookies (        \
           name,                          \
           value,                         \
           host,                          \
           path,                          \
           expiry,                        \
           lastAccessed,                  \
           creationTime,                  \
           isSecure,                      \
           isHttpOnly,                    \
           inBrowserElement,              \
           originAttributes,              \
           sameSite,                      \
           rawSameSite                    \
         ) VALUES (                       \
           :name,                         \
           :value,                        \
           :host,                         \
           :path,                         \
           :expiry,                       \
           :lastAccessed,                 \
           :creationTime,                 \
           :isSecure,                     \
           :isHttpOnly,                   \
           :inBrowserElement,             \
           :originAttributes,             \
           :sameSite,                     \
           :rawSameSite)"
      );

      this.stmtDelete = this.db.createStatement(
        "DELETE FROM moz_cookies          \
           WHERE name = :name AND host = :host AND path = :path AND \
                 originAttributes = :originAttributes"
      );

      this.stmtUpdate = this.db.createStatement(
        "UPDATE moz_cookies SET lastAccessed = :lastAccessed \
           WHERE name = :name AND host = :host AND path = :path AND \
                 originAttributes = :originAttributes"
      );

      break;
    }

    default:
      do_throw("unrecognized schemaVersion!");
  }
}

CookieDatabaseConnection.prototype = {
  insertCookie(cookie) {
    if (!(cookie instanceof Cookie)) {
      do_throw("not a cookie");
    }

    switch (this.schema) {
      case 1:
        this.stmtInsert.bindByName("id", cookie.creationTime);
        this.stmtInsert.bindByName("name", cookie.name);
        this.stmtInsert.bindByName("value", cookie.value);
        this.stmtInsert.bindByName("host", cookie.host);
        this.stmtInsert.bindByName("path", cookie.path);
        this.stmtInsert.bindByName("expiry", cookie.expiry);
        this.stmtInsert.bindByName("isSecure", cookie.isSecure);
        this.stmtInsert.bindByName("isHttpOnly", cookie.isHttpOnly);
        break;

      case 2:
        this.stmtInsert.bindByName("id", cookie.creationTime);
        this.stmtInsert.bindByName("name", cookie.name);
        this.stmtInsert.bindByName("value", cookie.value);
        this.stmtInsert.bindByName("host", cookie.host);
        this.stmtInsert.bindByName("path", cookie.path);
        this.stmtInsert.bindByName("expiry", cookie.expiry);
        this.stmtInsert.bindByName("lastAccessed", cookie.lastAccessed);
        this.stmtInsert.bindByName("isSecure", cookie.isSecure);
        this.stmtInsert.bindByName("isHttpOnly", cookie.isHttpOnly);
        break;

      case 3:
        this.stmtInsert.bindByName("id", cookie.creationTime);
        this.stmtInsert.bindByName("baseDomain", cookie.baseDomain);
        this.stmtInsert.bindByName("name", cookie.name);
        this.stmtInsert.bindByName("value", cookie.value);
        this.stmtInsert.bindByName("host", cookie.host);
        this.stmtInsert.bindByName("path", cookie.path);
        this.stmtInsert.bindByName("expiry", cookie.expiry);
        this.stmtInsert.bindByName("lastAccessed", cookie.lastAccessed);
        this.stmtInsert.bindByName("isSecure", cookie.isSecure);
        this.stmtInsert.bindByName("isHttpOnly", cookie.isHttpOnly);
        break;

      case 4:
        this.stmtInsert.bindByName("baseDomain", cookie.baseDomain);
        this.stmtInsert.bindByName("name", cookie.name);
        this.stmtInsert.bindByName("value", cookie.value);
        this.stmtInsert.bindByName("host", cookie.host);
        this.stmtInsert.bindByName("path", cookie.path);
        this.stmtInsert.bindByName("expiry", cookie.expiry);
        this.stmtInsert.bindByName("lastAccessed", cookie.lastAccessed);
        this.stmtInsert.bindByName("creationTime", cookie.creationTime);
        this.stmtInsert.bindByName("isSecure", cookie.isSecure);
        this.stmtInsert.bindByName("isHttpOnly", cookie.isHttpOnly);
        break;

      case 10:
        this.stmtInsert.bindByName("name", cookie.name);
        this.stmtInsert.bindByName("value", cookie.value);
        this.stmtInsert.bindByName("host", cookie.host);
        this.stmtInsert.bindByName("baseDomain", cookie.baseDomain);
        this.stmtInsert.bindByName("path", cookie.path);
        this.stmtInsert.bindByName("expiry", cookie.expiry);
        this.stmtInsert.bindByName("lastAccessed", cookie.lastAccessed);
        this.stmtInsert.bindByName("creationTime", cookie.creationTime);
        this.stmtInsert.bindByName("isSecure", cookie.isSecure);
        this.stmtInsert.bindByName("isHttpOnly", cookie.isHttpOnly);
        this.stmtInsert.bindByName("inBrowserElement", cookie.inBrowserElement);
        this.stmtInsert.bindByName(
          "originAttributes",
          ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
        );
        this.stmtInsert.bindByName("sameSite", cookie.sameSite);
        this.stmtInsert.bindByName("rawSameSite", cookie.rawSameSite);
        break;

      case 11:
        this.stmtInsert.bindByName("name", cookie.name);
        this.stmtInsert.bindByName("value", cookie.value);
        this.stmtInsert.bindByName("host", cookie.host);
        this.stmtInsert.bindByName("path", cookie.path);
        this.stmtInsert.bindByName("expiry", cookie.expiry);
        this.stmtInsert.bindByName("lastAccessed", cookie.lastAccessed);
        this.stmtInsert.bindByName("creationTime", cookie.creationTime);
        this.stmtInsert.bindByName("isSecure", cookie.isSecure);
        this.stmtInsert.bindByName("isHttpOnly", cookie.isHttpOnly);
        this.stmtInsert.bindByName("inBrowserElement", cookie.inBrowserElement);
        this.stmtInsert.bindByName(
          "originAttributes",
          ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
        );
        this.stmtInsert.bindByName("sameSite", cookie.sameSite);
        this.stmtInsert.bindByName("rawSameSite", cookie.rawSameSite);
        break;

      default:
        do_throw("unrecognized schemaVersion!");
    }

    do_execute_stmt(this.stmtInsert);
  },

  deleteCookie(cookie) {
    if (!(cookie instanceof Cookie)) {
      do_throw("not a cookie");
    }

    switch (this.db.schemaVersion) {
      case 1:
      case 2:
      case 3:
        this.stmtDelete.bindByName("id", cookie.creationTime);
        break;

      case 4:
        this.stmtDelete.bindByName("name", cookie.name);
        this.stmtDelete.bindByName("host", cookie.host);
        this.stmtDelete.bindByName("path", cookie.path);
        break;

      case 10:
      case 11:
        this.stmtDelete.bindByName("name", cookie.name);
        this.stmtDelete.bindByName("host", cookie.host);
        this.stmtDelete.bindByName("path", cookie.path);
        this.stmtDelete.bindByName(
          "originAttributes",
          ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
        );
        break;

      default:
        do_throw("unrecognized schemaVersion!");
    }

    do_execute_stmt(this.stmtDelete);
  },

  updateCookie(cookie) {
    if (!(cookie instanceof Cookie)) {
      do_throw("not a cookie");
    }

    switch (this.db.schemaVersion) {
      case 1:
        do_throw("can't update a schema 1 cookie!");
        break;
      case 2:
      case 3:
        this.stmtUpdate.bindByName("id", cookie.creationTime);
        this.stmtUpdate.bindByName("lastAccessed", cookie.lastAccessed);
        break;

      case 4:
        this.stmtDelete.bindByName("name", cookie.name);
        this.stmtDelete.bindByName("host", cookie.host);
        this.stmtDelete.bindByName("path", cookie.path);
        this.stmtUpdate.bindByName("name", cookie.name);
        this.stmtUpdate.bindByName("host", cookie.host);
        this.stmtUpdate.bindByName("path", cookie.path);
        this.stmtUpdate.bindByName("lastAccessed", cookie.lastAccessed);
        break;

      case 10:
      case 11:
        this.stmtDelete.bindByName("name", cookie.name);
        this.stmtDelete.bindByName("host", cookie.host);
        this.stmtDelete.bindByName("path", cookie.path);
        this.stmtDelete.bindByName(
          "originAttributes",
          ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
        );
        this.stmtUpdate.bindByName("name", cookie.name);
        this.stmtUpdate.bindByName("host", cookie.host);
        this.stmtUpdate.bindByName("path", cookie.path);
        this.stmtUpdate.bindByName(
          "originAttributes",
          ChromeUtils.originAttributesToSuffix(cookie.originAttributes)
        );
        this.stmtUpdate.bindByName("lastAccessed", cookie.lastAccessed);
        break;

      default:
        do_throw("unrecognized schemaVersion!");
    }

    do_execute_stmt(this.stmtUpdate);
  },

  close() {
    this.stmtInsert.finalize();
    this.stmtDelete.finalize();
    if (this.stmtUpdate) {
      this.stmtUpdate.finalize();
    }
    this.db.close();

    this.stmtInsert = null;
    this.stmtDelete = null;
    this.stmtUpdate = null;
    this.db = null;
  },
};

function do_get_cookie_file(profile) {
  let file = profile.clone();
  file.append("cookies.sqlite");
  return file;
}

// Count the cookies from 'host' in a database. If 'host' is null, count all
// cookies.
function do_count_cookies_in_db(connection, host) {
  let select = null;
  if (host) {
    select = connection.createStatement(
      "SELECT COUNT(1) FROM moz_cookies WHERE host = :host"
    );
    select.bindByName("host", host);
  } else {
    select = connection.createStatement("SELECT COUNT(1) FROM moz_cookies");
  }

  select.executeStep();
  let result = select.getInt32(0);
  select.reset();
  select.finalize();
  return result;
}

// Execute 'stmt', ensuring that we reset it if it throws.
function do_execute_stmt(stmt) {
  try {
    stmt.executeStep();
    stmt.reset();
  } catch (e) {
    stmt.reset();
    throw e;
  }
}
