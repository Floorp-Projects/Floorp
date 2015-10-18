/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["applicationName", "assert", "Copy", "getBrowserObject",
                        "getChromeWindow", "getWindows", "getWindowByTitle",
                        "getWindowByType", "getWindowId", "getMethodInWindows",
                        "getPreference", "saveDataURL", "setPreference",
                        "sleep", "startTimer", "stopTimer", "takeScreenshot",
                        "unwrapNode", "waitFor"
                       ];

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;


Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const applicationIdMap = {
  '{ec8030f7-c20a-464f-9b0e-13a3a9e97384}': 'Firefox'
}
const applicationName = applicationIdMap[Services.appinfo.ID] || Services.appinfo.name;

var assertions = {}; Cu.import('resource://mozmill/modules/assertions.js', assertions);
var broker = {}; Cu.import('resource://mozmill/driver/msgbroker.js', broker);
var errors = {}; Cu.import('resource://mozmill/modules/errors.js', errors);

var assert = new assertions.Assert();

var hwindow = Services.appShell.hiddenDOMWindow;

var uuidgen = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

function Copy (obj) {
  for (var n in obj) {
    this[n] = obj[n];
  }
}

/**
 * Returns the browser object of the specified window
 *
 * @param {Window} aWindow
 *        Window to get the browser element from.
 *
 * @returns {Object} The browser element
 */
function getBrowserObject(aWindow) {
  return aWindow.gBrowser;
}

function getChromeWindow(aWindow) {
  var chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsIDocShellTreeItem)
                  .rootTreeItem
                  .QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindow)
                  .QueryInterface(Ci.nsIDOMChromeWindow);

  return chromeWin;
}

function getWindows(type) {
  if (type == undefined) {
    type = "";
  }

  var windows = [];
  var enumerator = Services.wm.getEnumerator(type);

  while (enumerator.hasMoreElements()) {
    windows.push(enumerator.getNext());
  }

  if (type == "") {
    windows.push(hwindow);
  }

  return windows;
}

function getMethodInWindows(methodName) {
  for (var w of getWindows()) {
    if (w[methodName] != undefined) {
      return w[methodName];
    }
  }

  throw new Error("Method with name: '" + methodName + "' is not in any open window.");
}

function getWindowByTitle(title) {
  for (var w of getWindows()) {
    if (w.document.title && w.document.title == title) {
      return w;
    }
  }

  throw new Error("Window with title: '" + title + "' not found.");
}

function getWindowByType(type) {
  return Services.wm.getMostRecentWindow(type);
}

/**
 * Retrieve the outer window id for the given window.
 *
 * @param {Number} aWindow
 *        Window to retrieve the id from.
 * @returns {Boolean} The outer window id
 **/
function getWindowId(aWindow) {
  try {
    // Normally we can retrieve the id via window utils
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).
                   getInterface(Ci.nsIDOMWindowUtils).
                   outerWindowID;
  } catch (e) {
    // ... but for observer notifications we need another interface
    return aWindow.QueryInterface(Ci.nsISupportsPRUint64).data;
  }
}

var checkChrome = function () {
  var loc = window.document.location.href;
  try {
    loc = window.top.document.location.href;
  } catch (e) {
  }

  return /^chrome:\/\//.test(loc);
}

/**
 * Called to get the state of an individual preference.
 *
 * @param aPrefName     string The preference to get the state of.
 * @param aDefaultValue any    The default value if preference was not found.
 *
 * @returns any The value of the requested preference
 *
 * @see setPref
 * Code by Henrik Skupin: <hskupin@gmail.com>
 */
function getPreference(aPrefName, aDefaultValue) {
  try {
    var branch = Services.prefs;

    switch (typeof aDefaultValue) {
      case ('boolean'):
        return branch.getBoolPref(aPrefName);
      case ('string'):
        return branch.getCharPref(aPrefName);
      case ('number'):
        return branch.getIntPref(aPrefName);
      default:
        return branch.getComplexValue(aPrefName);
    }
  } catch (e) {
    return aDefaultValue;
  }
}

/**
 * Called to set the state of an individual preference.
 *
 * @param aPrefName string The preference to set the state of.
 * @param aValue    any    The value to set the preference to.
 *
 * @returns boolean Returns true if value was successfully set.
 *
 * @see getPref
 * Code by Henrik Skupin: <hskupin@gmail.com>
 */
function setPreference(aName, aValue) {
  try {
    var branch = Services.prefs;

    switch (typeof aValue) {
      case ('boolean'):
        branch.setBoolPref(aName, aValue);
        break;
      case ('string'):
        branch.setCharPref(aName, aValue);
        break;
      case ('number'):
        branch.setIntPref(aName, aValue);
        break;
      default:
        branch.setComplexValue(aName, aValue);
    }
  } catch (e) {
    return false;
  }

  return true;
}

/**
 * Sleep for the given amount of milliseconds
 *
 * @param {number} milliseconds
 *        Sleeps the given number of milliseconds
 */
function sleep(milliseconds) {
  var timeup = false;

  hwindow.setTimeout(function () { timeup = true; }, milliseconds);
  var thread = Services.tm.currentThread;

  while (!timeup) {
    thread.processNextEvent(true);
  }

  broker.pass({'function':'utils.sleep()'});
}

/**
 * Check if the callback function evaluates to true
 */
function assert(callback, message, thisObject) {
  var result = callback.call(thisObject);

  if (!result) {
    throw new Error(message || arguments.callee.name + ": Failed for '" + callback + "'");
  }

  return true;
}

/**
 * Unwraps a node which is wrapped into a XPCNativeWrapper or XrayWrapper
 *
 * @param {DOMnode} Wrapped DOM node
 * @returns {DOMNode} Unwrapped DOM node
 */
function unwrapNode(aNode) {
  var node = aNode;
  if (node) {
    // unwrap is not available on older branches (3.5 and 3.6) - Bug 533596
    if ("unwrap" in XPCNativeWrapper) {
      node = XPCNativeWrapper.unwrap(node);
    }
    else if (node.wrappedJSObject != null) {
      node = node.wrappedJSObject;
    }
  }

  return node;
}

/**
 * Waits for the callback evaluates to true
 */
function waitFor(callback, message, timeout, interval, thisObject) {
  broker.log({'function': 'utils.waitFor() - DEPRECATED',
              'message': 'utils.waitFor() is deprecated. Use assert.waitFor() instead'});
  assert.waitFor(callback, message, timeout, interval, thisObject);
}

/**
 * Calculates the x and y chrome offset for an element
 * See https://developer.mozilla.org/en/DOM/window.innerHeight
 *
 * Note this function will not work if the user has custom toolbars (via extension) at the bottom or left/right of the screen
 */
function getChromeOffset(elem) {
  var win = elem.ownerDocument.defaultView;
  // Calculate x offset
  var chromeWidth = 0;

  if (win["name"] != "sidebar") {
    chromeWidth = win.outerWidth - win.innerWidth;
  }

  // Calculate y offset
  var chromeHeight = win.outerHeight - win.innerHeight;
  // chromeHeight == 0 means elem is already in the chrome and doesn't need the addonbar offset
  if (chromeHeight > 0) {
    // window.innerHeight doesn't include the addon or find bar, so account for these if present
    var addonbar = win.document.getElementById("addon-bar");
    if (addonbar) {
      chromeHeight -= addonbar.scrollHeight;
    }

    var findbar = win.document.getElementById("FindToolbar");
    if (findbar) {
      chromeHeight -= findbar.scrollHeight;
    }
  }

  return {'x':chromeWidth, 'y':chromeHeight};
}

/**
 * Takes a screenshot of the specified DOM node
 */
function takeScreenshot(node, highlights) {
  var rect, win, width, height, left, top, needsOffset;
  // node can be either a window or an arbitrary DOM node
  try {
    // node is an arbitrary DOM node
    win = node.ownerDocument.defaultView;
    rect = node.getBoundingClientRect();
    width = rect.width;
    height = rect.height;
    top = rect.top;
    left = rect.left;
    // offset for highlights not needed as they will be relative to this node
    needsOffset = false;
  } catch (e) {
    // node is a window
    win = node;
    width = win.innerWidth;
    height = win.innerHeight;
    top = 0;
    left = 0;
    // offset needed for highlights to take 'outerHeight' of window into account
    needsOffset = true;
  }

  var canvas = win.document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
  canvas.width = width;
  canvas.height = height;

  var ctx = canvas.getContext("2d");
  // Draws the DOM contents of the window to the canvas
  ctx.drawWindow(win, left, top, width, height, "rgb(255,255,255)");

  // This section is for drawing a red rectangle around each element passed in via the highlights array
  if (highlights) {
    ctx.lineWidth = "2";
    ctx.strokeStyle = "red";
    ctx.save();

    for (var i = 0; i < highlights.length; ++i) {
      var elem = highlights[i];
      rect = elem.getBoundingClientRect();

      var offsetY = 0, offsetX = 0;
      if (needsOffset) {
        var offset = getChromeOffset(elem);
        offsetX = offset.x;
        offsetY = offset.y;
      } else {
        // Don't need to offset the window chrome, just make relative to containing node
        offsetY = -top;
        offsetX = -left;
      }

      // Draw the rectangle
      ctx.strokeRect(rect.left + offsetX, rect.top + offsetY, rect.width, rect.height);
    }
  }

  return canvas.toDataURL("image/jpeg", 0.5);
}

/**
 * Save the dataURL content to the specified file. It will be stored in either the persisted screenshot or temporary folder.
 *
 * @param {String} aDataURL
 *        The dataURL to save
 * @param {String} aFilename
 *        Target file name without extension
 *
 * @returns {Object} The hash containing the path of saved file, and the failure bit
 */
function saveDataURL(aDataURL, aFilename) {
  var frame = {}; Cu.import('resource://mozmill/modules/frame.js', frame);
  const FILE_PERMISSIONS = parseInt("0644", 8);

  var file;
  file = Cc['@mozilla.org/file/local;1']
         .createInstance(Ci.nsILocalFile);
  file.initWithPath(frame.persisted['screenshots']['path']);
  file.append(aFilename + ".jpg");
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, FILE_PERMISSIONS);

  // Create an output stream to write to file
  let foStream = Cc["@mozilla.org/network/file-output-stream;1"]
                 .createInstance(Ci.nsIFileOutputStream);
  foStream.init(file, 0x02 | 0x08 | 0x10, FILE_PERMISSIONS, foStream.DEFER_OPEN);

  let dataURI = NetUtil.newURI(aDataURL, "UTF8", null);
  if (!dataURI.schemeIs("data")) {
    throw TypeError("aDataURL parameter has to have 'data'" +
                    " scheme instead of '" + dataURI.scheme + "'");
  }

  // Write asynchronously to buffer;
  // Input and output streams are closed after write

  let ready = false;
  let failure = false;

  function sync(aStatus) {
    if (!Components.isSuccessCode(aStatus)) {
      failure = true;
    }
    ready = true;
  }

  NetUtil.asyncFetch(dataURI, function (aInputStream, aAsyncFetchResult) {
    if (!Components.isSuccessCode(aAsyncFetchResult)) {
        // An error occurred!
        sync(aAsyncFetchResult);
    } else {
      // Consume the input stream.
      NetUtil.asyncCopy(aInputStream, foStream, function (aAsyncCopyResult) {
        sync(aAsyncCopyResult);
      });
    }
  });

  assert.waitFor(function () {
    return ready;
  }, "DataURL has been saved to '" + file.path + "'");

  return {filename: file.path, failure: failure};
}

/**
 * Some very brain-dead timer functions useful for performance optimizations
 * This is only enabled in debug mode
 *
 **/
var gutility_mzmltimer = 0;
/**
 * Starts timer initializing with current EPOC time in milliseconds
 *
 * @returns none
 **/
function startTimer(){
  dump("TIMERCHECK:: starting now: " + Date.now() + "\n");
  gutility_mzmltimer = Date.now();
}

/**
 * Checks the timer and outputs current elapsed time since start of timer. It
 * will print out a message you provide with its "time check" so you can
 * correlate in the log file and figure out elapsed time of specific functions.
 *
 * @param aMsg    string The debug message to print with the timer check
 *
 * @returns none
 **/
function checkTimer(aMsg){
  var end = Date.now();
  dump("TIMERCHECK:: at " + aMsg + " is: " + (end - gutility_mzmltimer) + "\n");
}
