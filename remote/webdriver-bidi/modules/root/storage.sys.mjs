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
   * @property {string=} userContext (not supported)
   */

  /**
   * @typedef PartitionKey
   *
   * @property {string=} sourceOrigin
   * @property {string=} userContext (not supported)
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
   * @throws {UnsupportedOperationError}
   *     Raised when the command is called with `userContext` as
   *     in `partition` argument.
   */
  async getCookies(options = {}) {
    let { filter = {} } = options;
    const { partition: partitionSpec = null } = options;

    this.#assertPartition(partitionSpec);
    filter = this.#assertGetCookieFilter(filter);

    const partitionKey = this.#expandStoragePartitionSpec(partitionSpec);
    const store = this.#getTheCookieStore(partitionKey);
    const cookies = this.#getMatchingCookies(store, filter);

    // Bug 1875255. Exchange platform id for Webdriver BiDi id for the user context to return it to the client.
    // For now we use platform user context id for returning cookies for a specific browsing context in the platform API,
    // but we can not return it directly to the client, so for now we just remove it from the response.
    delete partitionKey.userContext;

    return { cookies, partitionKey };
  }

  #assertGetCookieFilter(filter) {
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

    if (sameSite !== null) {
      lazy.assert.string(
        sameSite,
        `Expected "filter.sameSite" to be a string, got ${sameSite}`
      );
      const sameSiteTypeValue = Object.values(SameSiteType);
      lazy.assert.that(
        sameSite => sameSiteTypeValue.includes(sameSite),
        `Expected "filter.sameSite" to be one of ${sameSiteTypeValue}, got ${sameSite}`
      )(sameSite);
    }

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
      lazy.assert.object(
        value,
        `Expected "filter.value" to be an object, got ${value}`
      );

      const { type, value: protocolBytesValue } = value;

      lazy.assert.string(
        type,
        `Expected "filter.value.type" to be string, got ${type}`
      );
      const bytesValueTypeValue = Object.values(lazy.BytesValueType);
      lazy.assert.that(
        type => bytesValueTypeValue.includes(type),
        `Expected "filter.value.type" to be one of ${bytesValueTypeValue}, got ${type}`
      )(type);

      lazy.assert.string(
        protocolBytesValue,
        `Expected "filter.value.value" to be string, got ${protocolBytesValue}`
      );
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

          // TODO: Bug 1875255. Implement support for "userContext" field.
          throw new lazy.error.UnsupportedOperationError(
            `"userContext" as a field on "partition" argument is not supported yet for "storage.getCookies" command`
          );
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

  /**
   * Deserialize the value to string, since platform API
   * returns cookie's value as a string.
   */
  #deserializeCookieValue(cookieValue) {
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
        partitionKey[keyName] = partitionSpec[keyName];
      }
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

    for (const storedCookie of cookieStore) {
      const serializedCookie = this.#serializeCookie(storedCookie);
      if (this.#matchCookie(serializedCookie, filter)) {
        cookies.push(serializedCookie);
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
    // we have to additionally specify `hostname`. When `sourceOrigin` is not present
    // `hostname` will stay equal undefined.
    let hostname;

    // In case we want to get the cookies for a certain `sourceOrigin`,
    // we have to separately retrieve cookies for a hostname built from `sourceOrigin`,
    // and with `partitionKey` equal an empty string to retrieve the cookies that which were set
    // by this hostname but without `partitionKey`, e.g. with `document.cookie`
    if (storagePartitionKey.sourceOrigin) {
      const url = new URL(storagePartitionKey.sourceOrigin);
      hostname = url.hostname;

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
        JSON.stringify(originAttributes),
        hostname
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
    for (const [fieldName] of Object.entries(CookieFieldsMapping)) {
      let value = filter[fieldName];
      if (value !== null) {
        let storedCookieValue = storedCookie[fieldName];

        if (fieldName === "value") {
          value = this.#deserializeCookieValue(value);
          storedCookieValue = this.#deserializeCookieValue(storedCookieValue);
        }

        if (storedCookieValue !== value) {
          return false;
        }
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
