/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is MozMill Test code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <mail@hskupin.info> (Original Author)
 *   Adrian Kalla <akalla@aviary.pl>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
