/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @namespace Defines useful methods to work with localized content
 */
var l10n = exports;

/**
 * Retrieve the localized content for a given DTD entity
 *
 * @memberOf l10n
 * @param {String[]} aDTDs Array of URLs for DTD files.
 * @param {String} aEntityId ID of the entity to get the localized content of.
 *
 * @returns {String} Localized content
 */
function getEntity(aDTDs, aEntityId) {
  // Add xhtml11.dtd to prevent missing entity errors with XHTML files
  aDTDs.push("resource:///res/dtd/xhtml11.dtd");

  // Build a string of external entities
  var references = "";
  for (i = 0; i < aDTDs.length; i++) {
    var id = 'dtd' + i;
    references += '<!ENTITY % ' + id + ' SYSTEM "' + aDTDs[i] + '">%' + id + ';';
  }

  var header = '<?xml version="1.0"?><!DOCTYPE elem [' + references + ']>';
  var element = '<elem id="entity">&' + aEntityId + ';</elem>';
  var content = header + element;

  var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
               createInstance(Ci.nsIDOMParser);
  var doc = parser.parseFromString(content, 'text/xml');
  var node = doc.querySelector('elem[id="entity"]');

  if (!node) {
    throw new Error("Unkown entity '" + aEntityId + "'");
  }

  return node.textContent;
}


/**
 * Retrieve the localized content for a given property
 *
 * @memberOf l10n
 * @param {String} aURL URL of the .properties file.
 * @param {String} aProperty The property to get the value of.
 *
 * @returns {String} Value of the requested property
 */
function getProperty(aURL, aProperty) {
  var sbs = Cc["@mozilla.org/intl/stringbundle;1"].
            getService(Ci.nsIStringBundleService);
  var bundle = sbs.createBundle(aURL);

  try {
    return bundle.GetStringFromName(aProperty);
  }
  catch (ex) {
    throw new Error("Unkown property '" + aProperty + "'");
  }
}


// Export of functions
l10n.getEntity = getEntity;
l10n.getProperty = getProperty;
