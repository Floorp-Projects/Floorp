/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * An API which allows Marionette to handle localized content.
 *
 * The localization (https://mzl.la/2eUMjyF) of UI elements in Gecko
 * based applications is done via entities and properties. For static
 * values entities are used, which are located in .dtd files. Whereby for
 * dynamically updated content the values come from .property files. Both
 * types of elements can be identifed via a unique id, and the translated
 * content retrieved.
 */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["DOMParser"]);
XPCOMUtils.defineLazyGetter(this, "domParser", () => new DOMParser());

const {NoSuchElementError} =
    ChromeUtils.import("chrome://marionette/content/error.js", {});

this.EXPORTED_SYMBOLS = ["l10n"];

/** @namespace */
this.l10n = {};

/**
 * Retrieve the localized string for the specified entity id.
 *
 * Example:
 *     localizeEntity(["chrome://branding/locale/brand.dtd"], "brandShortName")
 *
 * @param {Array.<string>} urls
 *     Array of .dtd URLs.
 * @param {string} id
 *     The ID of the entity to retrieve the localized string for.
 *
 * @return {string}
 *     The localized string for the requested entity.
 */
l10n.localizeEntity = function(urls, id) {
  // Build a string which contains all possible entity locations
  let locations = [];
  urls.forEach((url, index) => {
    locations.push(`<!ENTITY % dtd_${index} SYSTEM "${url}">%dtd_${index};`);
  });

  // Use the DOM parser to resolve the entity and extract its real value
  let header = `<?xml version="1.0"?><!DOCTYPE elem [${locations.join("")}]>`;
  let elem = `<elem id="elementID">&${id};</elem>`;
  let doc = domParser.parseFromString(header + elem, "text/xml");
  let element = doc.querySelector("elem[id='elementID']");

  if (element === null) {
    throw new NoSuchElementError(`Entity with id='${id}' hasn't been found`);
  }

  return element.textContent;
};

/**
 * Retrieve the localized string for the specified property id.
 *
 * Example:
 *
 *     localizeProperty(
 *         ["chrome://global/locale/findbar.properties"], "FastFind");
 *
 * @param {Array.<string>} urls
 *     Array of .properties URLs.
 * @param {string} id
 *     The ID of the property to retrieve the localized string for.
 *
 * @return {string}
 *     The localized string for the requested property.
 */
l10n.localizeProperty = function(urls, id) {
  let property = null;

  for (let url of urls) {
    let bundle = Services.strings.createBundle(url);
    try {
      property = bundle.GetStringFromName(id);
      break;
    } catch (e) {}
  }

  if (property === null) {
    throw new NoSuchElementError(
        `Property with ID '${id}' hasn't been found`);
  }

  return property;
};
