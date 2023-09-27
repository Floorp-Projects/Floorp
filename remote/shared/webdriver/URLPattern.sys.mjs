/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
});

/**
 * Parsed pattern to use for URL matching.
 *
 * @typedef {object} ParsedURLPattern
 * @property {string|null} protocol
 *     The protocol, for instance "https".
 * @property {string|null} hostname
 *     The hostname, for instance "example.com".
 * @property {string|null} port
 *     The serialized port. Empty string for default ports of special schemes.
 * @property {string|null} path
 *     The path, starting with "/".
 * @property {string|null} search
 *     The search query string, without the leading "?"
 */

/**
 * Subset of properties extracted from a parsed URL.
 *
 * @typedef {object} ParsedURL
 * @property {string=} host
 * @property {string|Array<string>} path
 *     Either a string if the path is an opaque path, or an array of strings
 *     (path segments).
 * @property {number=} port
 * @property {string=} query
 * @property {string=} scheme
 */

/**
 * Enum of URLPattern types.
 *
 * @readonly
 * @enum {URLPatternType}
 */
const URLPatternType = {
  Pattern: "pattern",
  String: "string",
};

const supportedURLPatternTypes = Object.values(URLPatternType);

const SPECIAL_SCHEMES = ["file", "http", "https", "ws", "wss"];
const DEFAULT_PORTS = {
  file: null,
  http: 80,
  https: 443,
  ws: 80,
  wss: 443,
};

/**
 * Check if a given URL pattern is compatible with the provided URL.
 *
 * Implements https://w3c.github.io/webdriver-bidi/#match-url-pattern
 *
 * @param {ParsedURLPattern} urlPattern
 *     The URL pattern to match.
 * @param {string} url
 *     The string representation of a URL to test against the pattern.
 *
 * @returns {boolean}
 *     True if the pattern is compatible with the provided URL, false otherwise.
 */
export function matchURLPattern(urlPattern, url) {
  const parsedURL = parseURL(url);

  if (urlPattern.protocol !== null && urlPattern.protocol != parsedURL.scheme) {
    return false;
  }

  if (urlPattern.hostname !== null && urlPattern.hostname != parsedURL.host) {
    return false;
  }

  if (urlPattern.port !== null && urlPattern.port != serializePort(parsedURL)) {
    return false;
  }

  if (
    urlPattern.pathname !== null &&
    urlPattern.pathname != serializePath(parsedURL)
  ) {
    return false;
  }

  if (urlPattern.search !== null) {
    const urlQuery = parsedURL.query === null ? "" : parsedURL.query;
    if (urlPattern.search != urlQuery) {
      return false;
    }
  }

  return true;
}

/**
 * Parse a URLPattern into a parsed pattern object which can be used to match
 * URLs using `matchURLPattern`.
 *
 * Implements https://w3c.github.io/webdriver-bidi/#parse-url-pattern
 *
 * @param {URLPattern} pattern
 *     The pattern to parse.
 *
 * @returns {ParsedURLPattern}
 *     The parsed URL pattern.
 *
 * @throws {InvalidArgumentError}
 *     Raised if an argument is of an invalid type or value.
 * @throws {UnsupportedOperationError}
 *     Raised if the pattern uses a protocol not supported by Firefox.
 */
export function parseURLPattern(pattern) {
  lazy.assert.object(
    pattern,
    `Expected url pattern to be an object, got ${pattern}`
  );

  let hasProtocol = true;
  let hasHostname = true;
  let hasPort = true;
  let hasPathname = true;
  let hasSearch = true;

  let patternUrl;
  switch (pattern.type) {
    case URLPatternType.Pattern:
      patternUrl = "";
      if ("protocol" in pattern) {
        patternUrl += parseProtocol(pattern.protocol);
      } else {
        hasProtocol = false;
        patternUrl += "http";
      }

      const scheme = patternUrl.toLowerCase();
      patternUrl += ":";
      if (SPECIAL_SCHEMES.includes(scheme)) {
        patternUrl += "//";
      }

      if ("hostname" in pattern) {
        patternUrl += parseHostname(pattern.hostname, scheme);
      } else {
        if (scheme != "file") {
          patternUrl += "placeholder";
        }
        hasHostname = false;
      }

      if ("port" in pattern) {
        patternUrl += parsePort(pattern.port);
      } else {
        hasPort = false;
      }

      if ("pathname" in pattern) {
        patternUrl += parsePathname(pattern.pathname);
      } else {
        hasPathname = false;
      }

      if ("search" in pattern) {
        patternUrl += parseSearch(pattern.search);
      } else {
        hasSearch = false;
      }
      break;
    case URLPatternType.String:
      lazy.assert.string(
        pattern.pattern,
        `Expected "urlPattern" of type "string" to have a string "pattern" property, got ${pattern.pattern}`
      );
      patternUrl = unescapeUrlPattern(pattern.pattern);
      break;
    default:
      throw new lazy.error.InvalidArgumentError(
        `Expected "urlPattern" type to be one of ${supportedURLPatternTypes}, got ${pattern.type}`
      );
  }

  if (!URL.canParse(patternUrl)) {
    throw new lazy.error.InvalidArgumentError(
      `Unable to parse URL "${patternUrl}"`
    );
  }

  let parsedURL;
  try {
    parsedURL = parseURL(patternUrl);
  } catch (e) {
    throw new lazy.error.InvalidArgumentError(
      `Failed to parse URL "${patternUrl}"`
    );
  }

  if (hasProtocol && !SPECIAL_SCHEMES.includes(parsedURL.scheme)) {
    throw new lazy.error.UnsupportedOperationError(
      `URL pattern did not specify a supported protocol (one of ${SPECIAL_SCHEMES}), got ${parsedURL.scheme}`
    );
  }

  return {
    protocol: hasProtocol ? parsedURL.scheme : null,
    hostname: hasHostname ? parsedURL.host : null,
    port: hasPort ? serializePort(parsedURL) : null,
    pathname:
      hasPathname && parsedURL.path.length ? serializePath(parsedURL) : null,
    search: hasSearch ? parsedURL.query || "" : null,
  };
}

/**
 * Parse the hostname property of a URLPatternPattern.
 *
 * @param {string} hostname
 *     A hostname property.
 * @param {string} scheme
 *     The scheme for the URLPatternPattern.
 *
 * @returns {string}
 *     The parsed property.
 *
 * @throws {InvalidArgumentError}
 *     Raised if an argument is of an invalid type or value.
 */
function parseHostname(hostname, scheme) {
  if (typeof hostname != "string" || hostname == "") {
    throw new lazy.error.InvalidArgumentError(
      `Expected URLPattern "hostname" to be a non-empty string, got ${hostname}`
    );
  }

  if (scheme == "file") {
    throw new lazy.error.InvalidArgumentError(
      `URLPattern with "file" scheme cannot specify a hostname, got ${hostname}`
    );
  }

  hostname = unescapeUrlPattern(hostname);

  const forbiddenHostnameCharacters = ["/", "?", "#"];
  let insideBrackets = false;
  for (const codepoint of hostname) {
    if (
      forbiddenHostnameCharacters.includes(codepoint) ||
      (!insideBrackets && codepoint == ":")
    ) {
      throw new lazy.error.InvalidArgumentError(
        `URL pattern "hostname" contained a forbidden character, got "${hostname}"`
      );
    }

    if (codepoint == "[") {
      insideBrackets = true;
    } else if (codepoint == "]") {
      insideBrackets = false;
    }
  }

  return hostname;
}

/**
 * Parse the pathname property of a URLPatternPattern.
 *
 * @param {string} pathname
 *     A pathname property.
 *
 * @returns {string}
 *     The parsed property.
 *
 * @throws {InvalidArgumentError}
 *     Raised if an argument is of an invalid type or value.
 */
function parsePathname(pathname) {
  lazy.assert.string(
    pathname,
    `Expected URLPattern "pathname" to be a string, got ${pathname}`
  );

  pathname = unescapeUrlPattern(pathname);
  if (!pathname.startsWith("/")) {
    pathname = `/${pathname}`;
  }

  if (pathname.includes("?") || pathname.includes("#")) {
    throw new lazy.error.InvalidArgumentError(
      `URL pattern "pathname" contained a forbidden character, got "${pathname}"`
    );
  }

  return pathname;
}

/**
 * Parse the port property of a URLPatternPattern.
 *
 * @param {string} port
 *     A port property.
 *
 * @returns {string}
 *     The parsed property.
 *
 * @throws {InvalidArgumentError}
 *     Raised if an argument is of an invalid type or value.
 */
function parsePort(port) {
  if (typeof port != "string" || port == "") {
    throw new lazy.error.InvalidArgumentError(
      `Expected URLPattern "port" to be a non-empty string, got ${port}`
    );
  }

  port = unescapeUrlPattern(port);

  const isNumber = /^\d*$/.test(port);
  if (!isNumber) {
    throw new lazy.error.InvalidArgumentError(
      `URL pattern "port" is not a valid number, got "${port}"`
    );
  }

  return `:${port}`;
}

/**
 * Parse the protocol property of a URLPatternPattern.
 *
 * @param {string} protocol
 *     A protocol property.
 *
 * @returns {string}
 *     The parsed property.
 *
 * @throws {InvalidArgumentError}
 *     Raised if an argument is of an invalid type or value.
 */
function parseProtocol(protocol) {
  if (typeof protocol != "string" || protocol == "") {
    throw new lazy.error.InvalidArgumentError(
      `Expected URLPattern "protocol" to be a non-empty string, got ${protocol}`
    );
  }

  protocol = unescapeUrlPattern(protocol);
  if (!/^[a-zA-Z0-9+-.]*$/.test(protocol)) {
    throw new lazy.error.InvalidArgumentError(
      `URL pattern "protocol" contained a forbidden character, got "${protocol}"`
    );
  }

  return protocol;
}

/**
 * Parse the search property of a URLPatternPattern.
 *
 * @param {string} search
 *     A search property.
 *
 * @returns {string}
 *     The parsed property.
 *
 * @throws {InvalidArgumentError}
 *     Raised if an argument is of an invalid type or value.
 */
function parseSearch(search) {
  lazy.assert.string(
    search,
    `Expected URLPattern "search" to be a string, got ${search}`
  );

  search = unescapeUrlPattern(search);
  if (!search.startsWith("?")) {
    search = `?${search}`;
  }

  if (search.includes("#")) {
    throw new lazy.error.InvalidArgumentError(
      `Expected URLPattern "search" to never contain "#", got ${search}`
    );
  }

  return search;
}

/**
 * Parse a string URL. This tries to be close to Basic URL Parser, however since
 * this is not currently implemented in Firefox and URL parsing has many edge
 * cases, it does not try to be a faithful implementation.
 *
 * Edge cases which are not supported are mostly about non-special URLs, which
 * in practice should not be observable in automation.
 *
 * @param {string} url
 *     The string based URL to parse.
 * @returns {ParsedURL}
 *     The parsed URL.
 */
function parseURL(url) {
  const urlObj = new URL(url);
  const uri = urlObj.URI;

  return {
    scheme: uri.scheme,
    // Note: Use urlObj instead of uri for hostname:
    // nsIURI removes brackets from ipv6 hostnames (eg [::1] becomes ::1).
    host: urlObj.hostname,
    path: uri.filePath,
    // Note: Use urlObj instead of uri for port:
    // nsIURI throws on the port getter for non-special schemes.
    port: urlObj.port != "" ? Number(uri.port) : null,
    query: uri.hasQuery ? uri.query : null,
  };
}

/**
 * Serialize the path of a parsed URL.
 *
 * @see https://pr-preview.s3.amazonaws.com/w3c/webdriver-bidi/pull/429.html#parse-url-pattern
 *
 * @param {ParsedURL} url
 *     A parsed url.
 *
 * @returns {string}
 *     The serialized path
 */
function serializePath(url) {
  // Check for opaque path
  if (typeof url.path == "string") {
    return url.path;
  }

  let serialized = "";
  for (const segment of url.path) {
    serialized += `/${segment}`;
  }

  return serialized;
}

/**
 * Serialize the port of a parsed URL.
 *
 * @see https://pr-preview.s3.amazonaws.com/w3c/webdriver-bidi/pull/429.html#parse-url-pattern
 *
 * @param {ParsedURL} url
 *     A parsed url.
 *
 * @returns {string}
 *     The serialized port
 */
function serializePort(url) {
  let port = null;
  if (
    SPECIAL_SCHEMES.includes(url.scheme) &&
    DEFAULT_PORTS[url.scheme] !== null &&
    (url.port === null || url.port == DEFAULT_PORTS[url.scheme])
  ) {
    port = "";
  } else if (url.port !== null) {
    port = `${url.port}`;
  }

  return port;
}

/**
 * Unescape and check a pattern string against common forbidden characters.
 *
 * @see https://pr-preview.s3.amazonaws.com/w3c/webdriver-bidi/pull/429.html#unescape-url-pattern
 *
 * @param {string} pattern
 *     Either a full URLPatternString pattern or a property of a URLPatternPattern.
 *
 * @returns {string}
 *     The unescaped pattern
 *
 * @throws {InvalidArgumentError}
 *     Raised if an argument is of an invalid type or value.
 */
function unescapeUrlPattern(pattern) {
  const forbiddenCharacters = ["(", ")", "*", "{", "}"];
  const escapeCharacter = "\\";

  let isEscaped = false;
  let result = "";

  for (const codepoint of Array.from(pattern)) {
    if (!isEscaped) {
      if (forbiddenCharacters.includes(codepoint)) {
        throw new lazy.error.InvalidArgumentError(
          `URL pattern contained an unescaped forbidden character ${codepoint}`
        );
      }

      if (codepoint == escapeCharacter) {
        isEscaped = true;
        continue;
      }
    }

    result += codepoint;
    isEscaped = false;
  }

  return result;
}
