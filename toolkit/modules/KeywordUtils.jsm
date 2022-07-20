/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["KeywordUtils"];

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

var KeywordUtils = {
  /**
   * Replaces %s or %S in the provided url or postData with the given parameter,
   * acccording to the best charset for the given url.
   *
   * @return [url, postData]
   * @throws if nor url nor postData accept a param, but a param was provided.
   */
  async parseUrlAndPostData(url, postData, param) {
    let hasGETParam = /%s/i.test(url);
    let decodedPostData = postData ? unescape(postData) : "";
    let hasPOSTParam = /%s/i.test(decodedPostData);

    if (!hasGETParam && !hasPOSTParam) {
      if (param) {
        // If nor the url, nor postData contain parameters, but a parameter was
        // provided, return the original input.
        throw new Error(
          "A param was provided but there's nothing to bind it to"
        );
      }
      return [url, postData];
    }

    let charset = "";
    const re = /^(.*)\&mozcharset=([a-zA-Z][_\-a-zA-Z0-9]+)\s*$/;
    let matches = url.match(re);
    if (matches) {
      [, url, charset] = matches;
    } else {
      // Try to fetch a charset from History.
      try {
        // Will return an empty string if character-set is not found.
        let pageInfo = await lazy.PlacesUtils.history.fetch(url, {
          includeAnnotations: true,
        });
        if (
          pageInfo &&
          pageInfo.annotations.has(lazy.PlacesUtils.CHARSET_ANNO)
        ) {
          charset = pageInfo.annotations.get(lazy.PlacesUtils.CHARSET_ANNO);
        }
      } catch (ex) {
        // makeURI() throws if url is invalid.
        Cu.reportError(ex);
      }
    }

    // encodeURIComponent produces UTF-8, and cannot be used for other charsets.
    // escape() works in those cases, but it doesn't uri-encode +, @, and /.
    // Therefore we need to manually replace these ASCII characters by their
    // encodeURIComponent result, to match the behavior of nsEscape() with
    // url_XPAlphas.
    let encodedParam = "";
    if (charset && charset != "UTF-8") {
      try {
        let converter = Cc[
          "@mozilla.org/intl/scriptableunicodeconverter"
        ].createInstance(Ci.nsIScriptableUnicodeConverter);
        converter.charset = charset;
        encodedParam = converter.ConvertFromUnicode(param) + converter.Finish();
      } catch (ex) {
        encodedParam = param;
      }
      encodedParam = escape(encodedParam).replace(
        /[+@\/]+/g,
        encodeURIComponent
      );
    } else {
      // Default charset is UTF-8
      encodedParam = encodeURIComponent(param);
    }

    url = url.replace(/%s/g, encodedParam).replace(/%S/g, param);
    if (hasPOSTParam) {
      postData = decodedPostData
        .replace(/%s/g, encodedParam)
        .replace(/%S/g, param);
    }
    return [url, postData];
  },

  /**
   * Returns a set of parameters if a keyword is registered and the search
   * string can be bound to it.
   *
   * @param {string} keyword The typed keyword.
   * @param {string} searchString The full search string, including the keyword.
   * @returns { entry, url, postData }
   */
  async getBindableKeyword(keyword, searchString) {
    let entry = await lazy.PlacesUtils.keywords.fetch(keyword);
    if (!entry) {
      return {};
    }

    try {
      let [url, postData] = await this.parseUrlAndPostData(
        entry.url.href,
        entry.postData,
        searchString
      );
      return { entry, url, postData };
    } catch (ex) {
      return {};
    }
  },
};
