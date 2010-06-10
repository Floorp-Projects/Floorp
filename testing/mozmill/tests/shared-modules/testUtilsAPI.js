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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henrik Skupin <hskupin@mozilla.com>
 *   Anthony Hughes <ahughes@mozilla.com>
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
 * @fileoverview
 * The UtilsAPI offers various helper functions for any other API which is
 * not already covered by another shared module.
 *
 * @version 1.0.3
 */

var MODULE_NAME = 'UtilsAPI';

/**
 * Get application specific informations
 * @see http://mxr.mozilla.org/mozilla-central/source/xpcom/system/nsIXULAppInfo.idl
 */
var appInfo = {
  _service: null,

  /**
   * Get the application info service
   * @returns XUL runtime object
   * @type nsiXULRuntime
   */
  get appInfo() {
    if (!this._appInfo) {
      this._service = Cc["@mozilla.org/xre/app-info;1"]
                        .getService(Ci.nsIXULAppInfo)
                        .QueryInterface(Ci.nsIXULRuntime);
    }
    return this._service;
  },

  /**
   * Get the build id
   * @returns Build id
   * @type string
   */
  get buildID() this.appInfo.appBuildID,

  /**
   * Get the application id
   * @returns Application id
   * @type string
   */
  get ID() this.appInfo.ID,

  /**
   * Get the application name
   * @returns Application name
   * @type string
   */
  get name() this.appInfo.name,

  /**
   * Get the operation system
   * @returns Operation system name
   * @type string
   */
  get os() this.appInfo.OS,

  /**
   * Get the product vendor
   * @returns Vendor name
   * @type string
   */
  get vendor() this.appInfo.vendor,

  /**
   * Get the application version
   * @returns Application version
   * @type string
   */
  get version() this.appInfo.version,

  /**
   * Get the build id of the Gecko platform
   * @returns Platform build id
   * @type string
   */
  get platformBuildID() this.appInfo.platformBuildID,

  /**
   * Get the version of the Gecko platform
   * @returns Platform version
   * @type string
   */
  get platformVersion() this.appInfo.platformVersion,

  /**
   * Get the currently used locale
   * @returns Current locale
   * @type string
   */
  get locale() {
    var registry = Cc["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Ci.nsIXULChromeRegistry);
    return registry.getSelectedLocale("global");
  },

  /**
   * Get the user agent string
   * @returns User agent
   * @type string
   */
  get userAgent() {
    var window = mozmill.wm.getMostRecentWindow("navigator:browser");
    if (window)
      return window.navigator.userAgent;
    return "";
  }
};

/**
 * Checks the visibility of an element.
 * XXX: Mozmill doesn't check if an element is visible and also operates on
 * elements which are invisible. (Bug 490548)
 *
 * @param {MozmillController} controller
 *        MozMillController of the window to operate on
 * @param {ElemBase} element
 *        Element to check its visibility
 * @param {boolean} visibility
 *        Expected visibility state of the element
 */
function assertElementVisible(controller, element, visibility)
{
  var style = controller.window.getComputedStyle(element.getNode(), "");
  var state = style.getPropertyValue("visibility");

  if (visibility) {
    controller.assertJS("subject.visibilityState == 'visible'",
                        {visibilityState: state});
  } else {
    controller.assertJS("subject.visibilityState != 'visible'",
                        {visibilityState: state});
  }
}

/**
 * Assert if the current URL is identical to the target URL.
 * With this function also redirects can be tested.
 *
 * @param {MozmillController} controller
 *        MozMillController of the window to operate on
 * @param {string} targetURL
 *        URL to check
 */
function assertLoadedUrlEqual(controller, targetUrl)
{
  var locationBar = new elementslib.ID(controller.window.document, "urlbar");
  var currentUrl = locationBar.getNode().value;

  // Load the target URL
  controller.open(targetUrl);
  controller.waitForPageLoad();

  // Check the same web page has been opened
  controller.assertValue(locationBar, currentUrl);
}

/**
 * Close the context menu inside the content area of the currently open tab
 *
 * @param {MozmillController} controller
 *        MozMillController of the window to operate on
 */
function closeContentAreaContextMenu(controller)
{
  var contextMenu = new elementslib.ID(controller.window.document, "contentAreaContextMenu");
  controller.keypress(contextMenu, "VK_ESCAPE", {});
}

/**
 * Run tests against a given search form
 *
 * @param {MozMillController} controller
 *        MozMillController of the window to operate on
 * @param {ElemBase} searchField
 *        The HTML input form element to test
 * @param {string} searchTerm
 *        The search term for the test
 * @param {ElemBase} submitButton
 *        (Optional) The forms submit button
 * @param {number} timeout
 *        The timeout value for the single tests
 */
function checkSearchField(controller, searchField, searchTerm, submitButton, timeout)
{
  controller.waitThenClick(searchField, timeout);
  controller.type(searchField, searchTerm);

  if (submitButton != undefined) {
    controller.waitThenClick(submitButton, timeout);
  }
}

/**
 * Create a new URI
 *
 * @param {string} spec
 *        The URI string in UTF-8 encoding.
 * @param {string} originCharset
 *        The charset of the document from which this URI string originated.
 * @param {string} baseURI
 *        If null, spec must specify an absolute URI. Otherwise, spec may be
 *        resolved relative to baseURI, depending on the protocol.
 * @return A URI object
 * @type nsIURI
 */
function createURI(spec, originCharset, baseURI)
{
  let iosvc = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);

  return iosvc.newURI(spec, originCharset, baseURI);
}

/**
 * Format a URL by replacing all placeholders
 *
 * @param {string} prefName
 *        The preference name which contains the URL
 * @return The formatted URL
 * @type string
 */
function formatUrlPref(prefName)
{
  var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                     .getService(Ci.nsIURLFormatter);

  return formatter.formatURLPref(prefName);
}

/**
 * Called to get the value of an individual property.
 *
 * @param {string} url
 *        URL of the string bundle.
 * @param {string} prefName
 *        The property to get the value of.
 *
 * @return The value of the requested property
 * @type string
 */
function getProperty(url, prefName)
{
  var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"].
                       getService(Components.interfaces.nsIStringBundleService);
  var bundle = sbs.createBundle(url);

  return bundle.GetStringFromName(prefName);
}
