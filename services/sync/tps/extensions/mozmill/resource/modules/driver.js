/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @namespace Defines the Mozmill driver for global actions
 */
var driver = exports;

Cu.import("resource://gre/modules/Services.jsm");

// Temporarily include utils module to re-use sleep
var assertions = {}; Cu.import('resource://mozmill/modules/assertions.js', assertions);
var mozmill = {}; Cu.import("resource://mozmill/driver/mozmill.js", mozmill);
var utils = {}; Cu.import('resource://mozmill/stdlib/utils.js', utils);

/**
 * Gets the topmost browser window. If there are none at that time, optionally
 * opens one. Otherwise will raise an exception if none are found.
 *
 * @memberOf driver
 * @param {Boolean] [aOpenIfNone=true] Open a new browser window if none are found.
 * @returns {DOMWindow}
 */
function getBrowserWindow(aOpenIfNone) {
  // Set default
  if (typeof aOpenIfNone === 'undefined') {
    aOpenIfNone = true;
  }

  // If implicit open is off, turn on strict checking, and vice versa.
  let win = getTopmostWindowByType("navigator:browser", !aOpenIfNone);

  // Can just assume automatic open here. If we didn't want it and nothing found,
  // we already raised above when getTopmostWindow was called.
  if (!win)
    win = openBrowserWindow();

  return win;
}


/**
 * Retrieves the hidden window on OS X
 *
 * @memberOf driver
 * @returns {DOMWindow} The hidden window
 */
function getHiddenWindow() {
  return Services.appShell.hiddenDOMWindow;
}


/**
 * Opens a new browser window
 *
 * @memberOf driver
 * @returns {DOMWindow}
 */
function openBrowserWindow() {
  // On OS X we have to be able to create a new browser window even with no other
  // window open. Therefore we have to use the hidden window. On other platforms
  // at least one remaining browser window has to exist.
  var win = mozmill.isMac ? getHiddenWindow() :
                            getTopmostWindowByType("navigator:browser", true);
  return win.OpenBrowserWindow();
}


/**
 * Pause the test execution for the given amount of time
 *
 * @type utils.sleep
 * @memberOf driver
 */
var sleep = utils.sleep;

/**
 * Wait until the given condition via the callback returns true.
 *
 * @type utils.waitFor
 * @memberOf driver
 */
var waitFor = assertions.Assert.waitFor;

//
// INTERNAL WINDOW ENUMERATIONS
//

/**
 * Internal function to build a list of DOM windows using a given enumerator
 * and filter.
 *
 * @private
 * @memberOf driver
 * @param {nsISimpleEnumerator} aEnumerator Window enumerator to use.
 * @param {Function} [aFilterCallback] Function which is used to filter windows.
 * @param {Boolean} [aStrict=true] Throw an error if no windows found
 *
 * @returns {DOMWindow[]} The windows found, in the same order as the enumerator.
 */
function _getWindows(aEnumerator, aFilterCallback, aStrict) {
  // Set default
  if (typeof aStrict === 'undefined')
    aStrict = true;

  let windows = [];

  while (aEnumerator.hasMoreElements()) {
    let window = aEnumerator.getNext();

    if (!aFilterCallback || aFilterCallback(window)) {
      windows.push(window);
    }
  }

  // If this list is empty and we're strict, throw an error
  if (windows.length === 0 && aStrict) {
    var message = 'No windows were found';

    // We'll throw a more detailed error if a filter was used.
    if (aFilterCallback && aFilterCallback.name)
      message += ' using filter "' + aFilterCallback.name + '"';

    throw new Error(message);
  }

  return windows;
}

//
// FILTER CALLBACKS
//

/**
 * Generator of a closure to filter a window based by a method
 *
 * @memberOf driver
 * @param {String} aName Name of the method in the window object.
 * @returns {Boolean} True if the condition is met.
 */
function windowFilterByMethod(aName) {
  return function byMethod(aWindow) { return (aName in aWindow); }
}


/**
 * Generator of a closure to filter a window based by the its title
 *
 * @param {String} aTitle Title of the window.
 * @returns {Boolean} True if the condition is met.
 */
function windowFilterByTitle(aTitle) {
  return function byTitle(aWindow) { return (aWindow.document.title === aTitle); }
}


/**
 * Generator of a closure to filter a window based by the its type
 *
 * @memberOf driver
 * @param {String} aType Type of the window.
 * @returns {Boolean} True if the condition is met.
 */
function windowFilterByType(aType) {
  return function byType(aWindow) {
           var type = aWindow.document.documentElement.getAttribute("windowtype");
           return (type === aType);
         }
}

//
// WINDOW LIST RETRIEVAL FUNCTIONS
//

/**
 * Retrieves a sorted list of open windows based on their age (newest to oldest),
 * optionally matching filter criteria.
 *
 * @memberOf driver
 * @param {Function} [aFilterCallback] Function which is used to filter windows.
 * @param {Boolean} [aStrict=true] Throw an error if no windows found
 *
 * @returns {DOMWindow[]} List of windows.
 */
function getWindowsByAge(aFilterCallback, aStrict) {
  var windows = _getWindows(Services.wm.getEnumerator(""),
                            aFilterCallback, aStrict);

  // Reverse the list, since naturally comes back old->new
  return windows.reverse();
}


/**
 * Retrieves a sorted list of open windows based on their z order (topmost first),
 * optionally matching filter criteria.
 *
 * @memberOf driver
 * @param {Function} [aFilterCallback] Function which is used to filter windows.
 * @param {Boolean} [aStrict=true] Throw an error if no windows found
 *
 * @returns {DOMWindow[]} List of windows.
 */
function getWindowsByZOrder(aFilterCallback, aStrict) {
  return _getWindows(Services.wm.getZOrderDOMWindowEnumerator("", true),
                     aFilterCallback, aStrict);
}

//
// SINGLE WINDOW RETRIEVAL FUNCTIONS
//

/**
 * Retrieves the last opened window, optionally matching filter criteria.
 *
 * @memberOf driver
 * @param {Function} [aFilterCallback] Function which is used to filter windows.
 * @param {Boolean} [aStrict=true] If true, throws error if no window found.
 *
 * @returns {DOMWindow} The window, or null if none found and aStrict == false
 */
function getNewestWindow(aFilterCallback, aStrict) {
  var windows = getWindowsByAge(aFilterCallback, aStrict);
  return windows.length ? windows[0] : null;
}

/**
 * Retrieves the topmost window, optionally matching filter criteria.
 *
 * @memberOf driver
 * @param {Function} [aFilterCallback] Function which is used to filter windows.
 * @param {Boolean} [aStrict=true] If true, throws error if no window found.
 *
 * @returns {DOMWindow} The window, or null if none found and aStrict == false
 */
function getTopmostWindow(aFilterCallback, aStrict) {
  var windows = getWindowsByZOrder(aFilterCallback, aStrict);
  return windows.length ? windows[0] : null;
}


/**
 * Retrieves the topmost window given by the window type
 *
 * XXX: Bug 462222
 *      This function has to be used instead of getTopmostWindow until the
 *      underlying platform bug has been fixed.
 *
 * @memberOf driver
 * @param {String} [aWindowType=null] Window type to query for
 * @param {Boolean} [aStrict=true] Throw an error if no windows found
 *
 * @returns {DOMWindow} The window, or null if none found and aStrict == false
 */
function getTopmostWindowByType(aWindowType, aStrict) {
  if (typeof aStrict === 'undefined')
    aStrict = true;

  var win = Services.wm.getMostRecentWindow(aWindowType);

  if (win === null && aStrict) {
    var message = 'No windows of type "' + aWindowType + '" were found';
    throw new errors.UnexpectedError(message);
  }

  return win;
}


// Export of functions
driver.getBrowserWindow = getBrowserWindow;
driver.getHiddenWindow = getHiddenWindow;
driver.openBrowserWindow = openBrowserWindow;
driver.sleep = sleep;
driver.waitFor = waitFor;

driver.windowFilterByMethod = windowFilterByMethod;
driver.windowFilterByTitle = windowFilterByTitle;
driver.windowFilterByType = windowFilterByType;

driver.getWindowsByAge = getWindowsByAge;
driver.getNewestWindow = getNewestWindow;
driver.getTopmostWindowByType = getTopmostWindowByType;


// XXX Bug: 462222
//     Currently those functions cannot be used. So they shouldn't be exported.
//driver.getWindowsByZOrder = getWindowsByZOrder;
//driver.getTopmostWindow = getTopmostWindow;
