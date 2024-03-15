/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  BytesValueType:
    "chrome://remote/content/webdriver-bidi/modules/root/network.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
  UserContextManager:
    "chrome://remote/content/shared/UserContextManager.sys.mjs",
});

const CookieFieldsMapping = {
  domain: "host",
  expiry: "expiry",
  httpOnly: "isHttpOnly",
  name: "name",
  path: "path",
  sameSite: "sameSite",
  secure: "isSecure",
  size: "size",
  value: "value",
};

const MAX_COOKIE_EXPIRY = Number.MAX_SAFE_INTEGER;

/**
 * Enum of possible partition types supported by the
 * storage.getCookies command.
 *
 * @readonly
 * @enum {PartitionType}
 */
const PartitionType = {
  Context: "context",
  StorageKey: "storageKey",
};

const PartitionKeyAttributes = ["sourceOrigin", "userContext"];

/**
 * Enum of possible SameSite types supported by the
 * storage.getCookies command.
 *
 * @readonly
 * @enum {SameSiteType}
 */
const SameSiteType = {
  [Ci.nsICookie.SAMESITE_NONE]: "none",
  [Ci.nsICookie.SAMESITE_LAX]: "lax",
  [Ci.nsICookie.SAMESITE_STRICT]: "strict",
};

class StorageModule extends Module {
  destroy() {}

  /**
   * Used as an argument for storage.getCookies command
   * to represent fields which should be used to filter the output
   * of the command.
   *
   * @typedef CookieFilter
   *
   * @property {string=} domain
   * @property {number=} expiry
   * @property {boolean=} httpOnly
   * @property {string=} name
   * @property {string=} path
   * @property {SameSiteType=} sameSite
   * @property {boolean=} secure
   * @property {number=} size
   * @property {Network.BytesValueType=} value
   */

  /**
   * Used as an argument for storage.getCookies command as one of the available variants
   * {BrowsingContextPartitionDescriptor} or {StorageKeyPartitionDescriptor}, to represent
   * fields should be used to build a partition key.
   *
   * @typedef PartitionDescriptor
   */

  /**
   * @typedef BrowsingContextPartitionDescriptor
   *
   * @property {PartitionType} [type=PartitionType.context]
   * @property {string} context
   */

  /**
   * @typedef StorageKeyPartitionDescriptor
   *
   * @property {PartitionType} [type=PartitionType.storageKey]
   * @property {string=} sourceOrigin
   * @property {string=} userContext
   */

  /**
   * @typedef PartitionKey
   *
   * @property {string=} sourceOrigin
   * @property {string=} userContext
   */

  /**
   * An object that holds the result of storage.getCookies command.
   *
   * @typedef GetCookiesResult
   *
   * @property {Array<Cookie>} cookies
   *    List of cookies.
   * @property {PartitionKey} partitionKey
   *    An object which represent the partition key which was used
   *    to retrieve the cookies.
   */

  /**
   * Remove zero or more cookies which match a set of provided parameters.
   *
   * @param {object=} options
   * @param {CookieFilter=} options.filter
   *     An object which holds field names and values, which
   *     should be used to filter the output of the command.
   * @param {PartitionDescriptor=} options.partition
   *     An object which holds the information which
   *     should be used to build a partition key.
   *
   * @returns {PartitionKey}
   *     An object with the partition key which was used to
   *     retrieve cookies which had to be removed.
   * @throws {InvalidArgumentError}
   *     If the provided arguments are not valid.
   * @throws {NoSuchFrameError}
   *     If the provided browsing context cannot be found.
   */
  async deleteCookies(options = {}) {
    let { filter = {} } = options;
    const { partition: partitionSpec = null } = options;

    this.#assertPartition(partitionSpec);
    filter = this.#assertCookieFilter(filter);

    const partitionKey = this.#expandStoragePartitionSpec(partitionSpec);
    const store = this.#getTheCookieStore(partitionKey);
    const cookies = this.#getMatchingCookies(store, filter);

    for (const cookie of cookies) {
      Services.cookies.remove(
        cookie.host,
        cookie.name,
        cookie.path,
        cookie.originAttributes
      );
    }

    return { partitionKey: this.#formatPartitionKey(partitionKey) };
  }

  /**
   * Retrieve zero or more cookies which match a set of provided parameters.
   *
   * @param {object=} options
   * @param {CookieFilter=} options.filter
   *     An object which holds field names and values, which
   *     should be used to filter the output of the command.
   * @param {PartitionDescriptor=} options.partition
   *     An object which holds the information which
   *     should be used to build a partition key.
   *
   * @returns {GetCookiesResult}
   *     An object which holds a list of retrieved cookies and
   *     the partition key which was used.
   * @throws {InvalidArgumentError}
   *     If the provided arguments are not valid.
   * @throws {NoSuchFrameError}
   *     If the provided browsing context cannot be found.
   */
  async getCookies(options = {}) {
    let { filter = {} } = options;
    const { partition: partitionSpec = null } = options;

    this.#assertPartition(partitionSpec);
    filter = this.#assertCookieFilter(filter);

    const partitionKey = this.#expandStoragePartitionSpec(partitionSpec);
    const store = this.#getTheCookieStore(partitionKey);
    const cookies = this.#getMatchingCookies(store, filter);
    const serializedCookies = [];

    for (const cookie of cookies) {
      serializedCookies.push(this.#serializeCookie(cookie));
    }

    return {
      cookies: serializedCookies,
      partitionKey: this.#formatPartitionKey(partitionKey),
    };
  }

  /**
   * An object representation of the cookie which should be set.
   *
   * @typedef PartialCookie
   *
   * @property {string} domain
   * @property {number=} expiry
   * @property {boolean=} httpOnly
   * @property {string} name
   * @property {string=} path
   * @property {SameSiteType=} sameSite
   * @property {boolean=} secure
   * @property {number=} size
   * @property {Network.BytesValueType} value
   */

  /**
   * Create a new cookie in a cookie store.
   *
   * @param {object=} options
   * @param {PartialCookie} options.cookie
   *     An object representation of the cookie which
   *     should be set.
   * @param {PartitionDescriptor=} options.partition
   *     An object which holds the information which
   *     should be used to build a partition key.
   *
   * @returns {PartitionKey}
   *     An object with the partition key which was used to
   *     add the cookie.
   * @throws {InvalidArgumentError}
   *     If the provided arguments are not valid.
   * @throws {NoSuchFrameError}
   *     If the provided browsing context cannot be found.
   * @throws {UnableToSetCookieError}
   *     If the cookie was not added.
   */
  async setCookie(options = {}) {
    const { cookie: cookieSpec, partition: partitionSpec = null } = options;
    lazy.assert.object(
      cookieSpec,
      `Expected "cookie" to be an object, got ${cookieSpec}`
    );

    const {
      domain,
      expiry = null,
      httpOnly = null,
      name,
      path = null,
      sameSite = null,
      secure = null,
      value,
    } = cookieSpec;
    this.#assertCookie({
      domain,
      expiry,
      httpOnly,
      name,
      path,
      sameSite,
      secure,
      value,
    });
    this.#assertPartition(partitionSpec);

    const partitionKey = this.#expandStoragePartitionSpec(partitionSpec);

    // The cookie store is defined by originAttributes.
    const originAttributes = this.#getOriginAttributes(partitionKey);

    const deserializedValue = this.#deserializeProtocolBytes(value);

    // The XPCOM interface requires to be specified if a cookie is session.
    const isSession = expiry === null;

    let schemeType;
    if (secure) {
      schemeType = Ci.nsICookie.SCHEME_HTTPS;
    } else {
      schemeType = Ci.nsICookie.SCHEME_HTTP;
    }

    try {
      Services.cookies.add(
        domain,
        path === null ? "/" : path,
        name,
        deserializedValue,
        secure === null ? false : secure,
        httpOnly === null ? false : httpOnly,
        isSession,
        // The XPCOM interface requires the expiry field even for session cookies.
        expiry === null ? MAX_COOKIE_EXPIRY : expiry,
        originAttributes,
        this.#getSameSitePlatformProperty(sameSite),
        schemeType
      );
    } catch (e) {
      throw new lazy.error.UnableToSetCookieError(e);
    }

    return { partitionKey: this.#formatPartitionKey(partitionKey) };
  }

  #assertCookie(cookie) {
    lazy.assert.object(
      cookie,
      `Expected "cookie" to be an object, got ${cookie}`
    );

    const { domain, expiry, httpOnly, name, path, sameSite, secure, value } =
      cookie;

    lazy.assert.string(
      domain,
      `Expected "domain" to be a string, got ${domain}`
    );

    lazy.assert.string(name, `Expected "name" to be a string, got ${name}`);

    this.#assertValue(value);

    if (expiry !== null) {
      lazy.assert.positiveInteger(
        expiry,
        `Expected "expiry" to be a positive number, got ${expiry}`
      );
    }

    if (httpOnly !== null) {
      lazy.assert.boolean(
        httpOnly,
        `Expected "httpOnly" to be a boolean, got ${httpOnly}`
      );
    }

    if (path !== null) {
      lazy.assert.string(path, `Expected "path" to be a string, got ${path}`);
    }

    this.#assertSameSite(sameSite);

    if (secure !== null) {
      lazy.assert.boolean(
        secure,
        `Expected "secure" to be a boolean, got ${secure}`
      );
    }
  }

  #assertCookieFilter(filter) {
    lazy.assert.object(
      filter,
      `Expected "filter" to be an object, got ${filter}`
    );

    const {
      domain = null,
      expiry = null,
      httpOnly = null,
      name = null,
      path = null,
      sameSite = null,
      secure = null,
      size = null,
      value = null,
    } = filter;

    if (domain !== null) {
      lazy.assert.string(
        domain,
        `Expected "filter.domain" to be a string, got ${domain}`
      );
    }

    if (expiry !== null) {
      lazy.assert.positiveInteger(
        expiry,
        `Expected "filter.expiry" to be a positive number, got ${expiry}`
      );
    }

    if (httpOnly !== null) {
      lazy.assert.boolean(
        httpOnly,
        `Expected "filter.httpOnly" to be a boolean, got ${httpOnly}`
      );
    }

    if (name !== null) {
      lazy.assert.string(
        name,
        `Expected "filter.name" to be a string, got ${name}`
      );
    }

    if (path !== null) {
      lazy.assert.string(
        path,
        `Expected "filter.path" to be a string, got ${path}`
      );
    }

    this.#assertSameSite(sameSite, "filter.sameSite");

    if (secure !== null) {
      lazy.assert.boolean(
        secure,
        `Expected "filter.secure" to be a boolean, got ${secure}`
      );
    }

    if (size !== null) {
      lazy.assert.positiveInteger(
        size,
        `Expected "filter.size" to be a positive number, got ${size}`
      );
    }

    if (value !== null) {
      this.#assertValue(value, "filter.value");
    }

    return {
      domain,
      expiry,
      httpOnly,
      name,
      path,
      sameSite,
      secure,
      size,
      value,
    };
  }

  #assertPartition(partitionSpec) {
    if (partitionSpec === null) {
      return;
    }
    lazy.assert.object(
      partitionSpec,
      `Expected "partition" to be an object, got ${partitionSpec}`
    );

    const { type } = partitionSpec;
    lazy.assert.string(
      type,
      `Expected "partition.type" to be a string, got ${type}`
    );

    switch (type) {
      case PartitionType.Context: {
        const { context } = partitionSpec;
        lazy.assert.string(
          context,
          `Expected "partition.context" to be a string, got ${context}`
        );

        break;
      }

      case PartitionType.StorageKey: {
        const { sourceOrigin = null, userContext = null } = partitionSpec;
        if (sourceOrigin !== null) {
          lazy.assert.string(
            sourceOrigin,
            `Expected "partition.sourceOrigin" to be a string, got ${sourceOrigin}`
          );
          lazy.assert.that(
            sourceOrigin => URL.canParse(sourceOrigin),
            `Expected "partition.sourceOrigin" to be a valid URL, got ${sourceOrigin}`
          )(sourceOrigin);

          const url = new URL(sourceOrigin);
          lazy.assert.that(
            url => url.pathname === "/" && url.hash === "" && url.search === "",
            `Expected "partition.sourceOrigin" to contain only origin, got ${sourceOrigin}`
          )(url);
        }
        if (userContext !== null) {
          lazy.assert.string(
            userContext,
            `Expected "partition.userContext" to be a string, got ${userContext}`
          );

          if (!lazy.UserContextManager.hasUserContextId(userContext)) {
            throw new lazy.error.NoSuchUserContextError(
              `User Context with id ${userContext} was not found`
            );
          }
        }
        break;
      }

      default: {
        throw new lazy.error.InvalidArgumentError(
          `Expected "partition.type" to be one of ${Object.values(
            PartitionType
          )}, got ${type}`
        );
      }
    }
  }

  #assertSameSite(sameSite, fieldName = "sameSite") {
    if (sameSite !== null) {
      const sameSiteTypeValue = Object.values(SameSiteType);
      lazy.assert.in(
        sameSite,
        sameSiteTypeValue,
        `Expected "${fieldName}" to be one of ${sameSiteTypeValue}, got ${sameSite}`
      );
    }
  }

  #assertValue(value, fieldName = "value") {
    lazy.assert.object(
      value,
      `Expected "${fieldName}" to be an object, got ${value}`
    );

    const { type, value: protocolBytesValue } = value;

    const bytesValueTypeValue = Object.values(lazy.BytesValueType);
    lazy.assert.in(
      type,
      bytesValueTypeValue,
      `Expected "${fieldName}.type" to be one of ${bytesValueTypeValue}, got ${type}`
    );

    lazy.assert.string(
      protocolBytesValue,
      `Expected "${fieldName}.value" to be string, got ${protocolBytesValue}`
    );
  }

  /**
   * Deserialize filter.
   *
   * @see https://w3c.github.io/webdriver-bidi/#deserialize-filter
   */
  #deserializeFilter(filter) {
    const deserializedFilter = {};
    for (const [fieldName, value] of Object.entries(filter)) {
      if (value === null) {
        continue;
      }

      const deserializedName = CookieFieldsMapping[fieldName];
      let deserializedValue;

      switch (deserializedName) {
        case "sameSite":
          deserializedValue = this.#getSameSitePlatformProperty(value);
          break;

        case "value":
          deserializedValue = this.#deserializeProtocolBytes(value);
          break;

        default:
          deserializedValue = value;
      }

      deserializedFilter[deserializedName] = deserializedValue;
    }

    return deserializedFilter;
  }

  /**
   * Deserialize the value to string, since platform API
   * returns cookie's value as a string.
   */
  #deserializeProtocolBytes(cookieValue) {
    const { type, value } = cookieValue;

    if (type === lazy.BytesValueType.String) {
      return value;
    }

    // For type === BytesValueType.Base64.
    return atob(value);
  }

  /**
   * Build a partition key.
   *
   * @see https://w3c.github.io/webdriver-bidi/#expand-a-storage-partition-spec
   */
  #expandStoragePartitionSpec(partitionSpec) {
    if (partitionSpec === null) {
      partitionSpec = {};
    }

    if (partitionSpec.type === PartitionType.Context) {
      const { context: contextId } = partitionSpec;
      const browsingContext = this.#getBrowsingContext(contextId);

      // Define browsing context’s associated storage partition as combination of user context id
      // and the origin of the document in this browsing context.
      return {
        sourceOrigin: browsingContext.currentURI.prePath,
        userContext: browsingContext.originAttributes.userContextId,
      };
    }

    const partitionKey = {};
    for (const keyName of PartitionKeyAttributes) {
      if (keyName in partitionSpec) {
        // Retrieve a platform user context id.
        if (keyName === "userContext") {
          partitionKey[keyName] = lazy.UserContextManager.getInternalIdById(
            partitionSpec.userContext
          );
        } else {
          partitionKey[keyName] = partitionSpec[keyName];
        }
      }
    }

    return partitionKey;
  }

  /**
   * Prepare the partition key in the right format for returning to a client.
   */
  #formatPartitionKey(partitionKey) {
    if ("userContext" in partitionKey) {
      // Exchange platform id for Webdriver BiDi id for the user context to return it to the client.
      partitionKey.userContext = lazy.UserContextManager.getIdByInternalId(
        partitionKey.userContext
      );
    }

    return partitionKey;
  }

  /**
   * Retrieves a browsing context based on its id.
   *
   * @param {number} contextId
   *     Id of the browsing context.
   * @returns {BrowsingContext}
   *     The browsing context.
   * @throws {NoSuchFrameError}
   *     If the browsing context cannot be found.
   */
  #getBrowsingContext(contextId) {
    const context = lazy.TabManager.getBrowsingContextById(contextId);
    if (context === null) {
      throw new lazy.error.NoSuchFrameError(
        `Browsing Context with id ${contextId} not found`
      );
    }

    return context;
  }

  /**
   * Since cookies retrieved from the platform API
   * always contain expiry even for session cookies,
   * we should check ourselves if it's a session cookie
   * and do not return expiry in case it is.
   */
  #getCookieExpiry(cookie) {
    const { expiry, isSession } = cookie;
    return isSession ? null : expiry;
  }

  #getCookieSize(cookie) {
    const { name, value } = cookie;
    return name.length + value.length;
  }

  /**
   * Filter and serialize given cookies with provided filter.
   *
   * @see https://w3c.github.io/webdriver-bidi/#get-matching-cookies
   */
  #getMatchingCookies(cookieStore, filter) {
    const cookies = [];
    const deserializedFilter = this.#deserializeFilter(filter);

    for (const storedCookie of cookieStore) {
      if (this.#matchCookie(storedCookie, deserializedFilter)) {
        cookies.push(storedCookie);
      }
    }
    return cookies;
  }

  /**
   * Prepare the data in the required for platform API format.
   */
  #getOriginAttributes(partitionKey) {
    const originAttributes = {};

    if (partitionKey.sourceOrigin) {
      originAttributes.partitionKey = ChromeUtils.getPartitionKeyFromURL(
        partitionKey.sourceOrigin
      );
    }
    if ("userContext" in partitionKey) {
      originAttributes.userContextId = partitionKey.userContext;
    }

    return originAttributes;
  }

  #getSameSitePlatformProperty(sameSite) {
    switch (sameSite) {
      case "lax": {
        return Ci.nsICookie.SAMESITE_LAX;
      }
      case "strict": {
        return Ci.nsICookie.SAMESITE_STRICT;
      }
    }

    return Ci.nsICookie.SAMESITE_NONE;
  }

  /**
   * Return a cookie store of the storage partition for a given storage partition key.
   *
   * The implementation differs here from the spec, since in gecko there is no
   * direct way to get all the cookies for a given partition key.
   *
   * @see https://w3c.github.io/webdriver-bidi/#get-the-cookie-store
   */
  #getTheCookieStore(storagePartitionKey) {
    let store = [];

    // Prepare the data in the format required for the platform API.
    const originAttributes = this.#getOriginAttributes(storagePartitionKey);

    // In case we want to get the cookies for a certain `sourceOrigin`,
    // we have to separately retrieve cookies for a hostname built from `sourceOrigin`,
    // and with `partitionKey` equal an empty string to retrieve the cookies that which were set
    // by this hostname but without `partitionKey`, e.g. with `document.cookie`.
    if (storagePartitionKey.sourceOrigin) {
      const url = new URL(storagePartitionKey.sourceOrigin);
      const hostname = url.hostname;

      const principal = Services.scriptSecurityManager.createContentPrincipal(
        Services.io.newURI(url),
        {}
      );
      const isSecureProtocol = principal.isOriginPotentiallyTrustworthy;

      // We want to keep `userContext` id here, if it's present,
      // but set the `partitionKey` to an empty string.
      const cookiesMatchingHostname =
        Services.cookies.getCookiesWithOriginAttributes(
          JSON.stringify({ ...originAttributes, partitionKey: "" }),
          hostname
        );

      for (const cookie of cookiesMatchingHostname) {
        // Ignore secure cookies for non-secure protocols.
        if (cookie.isSecure && !isSecureProtocol) {
          continue;
        }
        store.push(cookie);
      }
    }

    // Add the cookies which exactly match a built partition attributes.
    store = store.concat(
      Services.cookies.getCookiesWithOriginAttributes(
        JSON.stringify(originAttributes)
      )
    );

    return store;
  }

  /**
   * Match a provided cookie with provided filter.
   *
   * @see https://w3c.github.io/webdriver-bidi/#match-cookie
   */
  #matchCookie(storedCookie, filter) {
    for (const [fieldName, value] of Object.entries(filter)) {
      // Since we set `null` to not specified values, we have to check for `null` here
      // and not match on these values.
      if (value === null) {
        continue;
      }

      let storedCookieValue = storedCookie[fieldName];

      // The platform represantation of cookie doesn't contain a size field,
      // so we have to calculate it to match.
      if (fieldName === "size") {
        storedCookieValue = this.#getCookieSize(storedCookie);
      }

      if (storedCookieValue !== value) {
        return false;
      }
    }

    return true;
  }

  /**
   * Serialize a cookie.
   *
   * @see https://w3c.github.io/webdriver-bidi/#serialize-cookie
   */
  #serializeCookie(storedCookie) {
    const cookie = {};
    for (const [serializedName, cookieName] of Object.entries(
      CookieFieldsMapping
    )) {
      switch (serializedName) {
        case "expiry": {
          const expiry = this.#getCookieExpiry(storedCookie);
          if (expiry !== null) {
            cookie.expiry = expiry;
          }
          break;
        }

        case "sameSite":
          cookie.sameSite = SameSiteType[storedCookie.sameSite];
          break;

        case "size":
          cookie.size = this.#getCookieSize(storedCookie);
          break;

        case "value":
          // Bug 1879309. Add support for non-UTF8 cookies,
          // when a byte representation of value is available.
          // For now, use a value field, which is returned as a string.
          cookie.value = {
            type: lazy.BytesValueType.String,
            value: storedCookie.value,
          };
          break;

        default:
          cookie[serializedName] = storedCookie[cookieName];
      }
    }

    return cookie;
  }
}

export const storage = StorageModule;
