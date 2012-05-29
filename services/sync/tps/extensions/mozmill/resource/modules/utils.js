/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["openFile", "saveFile", "saveAsFile", "genBoiler",
                        "getFile", "Copy", "getChromeWindow", "getWindows", "runEditor",
                        "runFile", "getWindowByTitle", "getWindowByType", "tempfile",
                        "getMethodInWindows", "getPreference", "setPreference",
                        "sleep", "assert", "unwrapNode", "TimeoutError", "waitFor",
                        "takeScreenshot",
                       ];

var hwindow = Components.classes["@mozilla.org/appshell/appShellService;1"]
              .getService(Components.interfaces.nsIAppShellService)
              .hiddenDOMWindow;

var uuidgen = Components.classes["@mozilla.org/uuid-generator;1"]
    .getService(Components.interfaces.nsIUUIDGenerator);

function Copy (obj) {
  for (var n in obj) {
    this[n] = obj[n];
  }
}

function getChromeWindow(aWindow) {
  var chromeWin = aWindow
           .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
           .getInterface(Components.interfaces.nsIWebNavigation)
           .QueryInterface(Components.interfaces.nsIDocShellTreeItem)
           .rootTreeItem
           .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
           .getInterface(Components.interfaces.nsIDOMWindow)
           .QueryInterface(Components.interfaces.nsIDOMChromeWindow);
  return chromeWin;
}

function getWindows(type) {
  if (type == undefined) {
      type = "";
  }
  var windows = []
  var enumerator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator)
                     .getEnumerator(type);
  while(enumerator.hasMoreElements()) {
    windows.push(enumerator.getNext());
  }
  if (type == "") {
    windows.push(hwindow);
  }
  return windows;
}

function getMethodInWindows (methodName) {
  for each(w in getWindows()) {
    if (w[methodName] != undefined) {
      return w[methodName];
    }
  }
  throw new Error("Method with name: '" + methodName + "' is not in any open window.");
}

function getWindowByTitle(title) {
  for each(w in getWindows()) {
    if (w.document.title && w.document.title == title) {
      return w;
    }
  }
}

function getWindowByType(type) {
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
           .getService(Components.interfaces.nsIWindowMediator);
  return wm.getMostRecentWindow(type);
}

function tempfile(appention) {
  if (appention == undefined) {
    var appention = "mozmill.utils.tempfile"
  }
	var tempfile = Components.classes["@mozilla.org/file/directory_service;1"].getService(Components.interfaces.nsIProperties).get("TmpD", Components.interfaces.nsIFile);
	tempfile.append(uuidgen.generateUUID().toString().replace('-', '').replace('{', '').replace('}',''))
	tempfile.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0777);
	tempfile.append(appention);
	tempfile.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0666);
	// do whatever you need to the created file
	return tempfile.clone()
}

var checkChrome = function() {
   var loc = window.document.location.href;
   try {
       loc = window.top.document.location.href;
   } catch (e) {}

   if (/^chrome:\/\//.test(loc)) { return true; }
   else { return false; }
}


 var runFile = function(w){
   //define the interface
   var nsIFilePicker = Components.interfaces.nsIFilePicker;
   var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
   //define the file picker window
   fp.init(w, "Select a File", nsIFilePicker.modeOpen);
   fp.appendFilter("JavaScript Files","*.js");
   //show the window
   var res = fp.show();
   //if we got a file
   if (res == nsIFilePicker.returnOK){
     var thefile = fp.file;
     //create the paramObj with a files array attrib
     var paramObj = {};
     paramObj.files = [];
     paramObj.files.push(thefile.path);
   }
 };

 var saveFile = function(w, content, filename){
   //define the file interface
   var file = Components.classes["@mozilla.org/file/local;1"]
                        .createInstance(Components.interfaces.nsILocalFile);
   //point it at the file we want to get at
   file.initWithPath(filename);

   // file is nsIFile, data is a string
   var foStream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                            .createInstance(Components.interfaces.nsIFileOutputStream);

   // use 0x02 | 0x10 to open file for appending.
   foStream.init(file, 0x02 | 0x08 | 0x20, 0666, 0);
   // write, create, truncate
   // In a c file operation, we have no need to set file mode with or operation,
   // directly using "r" or "w" usually.

   foStream.write(content, content.length);
   foStream.close();
 };

  var saveAsFile = function(w, content){
     //define the interface
     var nsIFilePicker = Components.interfaces.nsIFilePicker;
     var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
     //define the file picker window
     fp.init(w, "Select a File", nsIFilePicker.modeSave);
     fp.appendFilter("JavaScript Files","*.js");
     //show the window
     var res = fp.show();
     //if we got a file
     if ((res == nsIFilePicker.returnOK) || (res == nsIFilePicker.returnReplace)){
       var thefile = fp.file;

       //forcing the user to save as a .js file
       if (thefile.path.indexOf(".js") == -1){
         //define the file interface
         var file = Components.classes["@mozilla.org/file/local;1"]
                              .createInstance(Components.interfaces.nsILocalFile);
         //point it at the file we want to get at
         file.initWithPath(thefile.path+".js");
         var thefile = file;
       }

       // file is nsIFile, data is a string
       var foStream = Components.classes["@mozilla.org/network/file-output-stream;1"]
                               .createInstance(Components.interfaces.nsIFileOutputStream);

       // use 0x02 | 0x10 to open file for appending.
       foStream.init(thefile, 0x02 | 0x08 | 0x20, 0666, 0);
       // write, create, truncate
       // In a c file operation, we have no need to set file mode with or operation,
       // directly using "r" or "w" usually.
       foStream.write(content, content.length);
       foStream.close();
       return thefile.path;
     }
  };

 var openFile = function(w){
    //define the interface
    var nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    //define the file picker window
    fp.init(w, "Select a File", nsIFilePicker.modeOpen);
    fp.appendFilter("JavaScript Files","*.js");
    //show the window
    var res = fp.show();
    //if we got a file
    if (res == nsIFilePicker.returnOK){
      var thefile = fp.file;
      //create the paramObj with a files array attrib
      var data = getFile(thefile.path);

      return {path:thefile.path, data:data};
    }
  };

 var getFile = function(path){
   //define the file interface
   var file = Components.classes["@mozilla.org/file/local;1"]
                        .createInstance(Components.interfaces.nsILocalFile);
   //point it at the file we want to get at
   file.initWithPath(path);
   // define file stream interfaces
   var data = "";
   var fstream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                           .createInstance(Components.interfaces.nsIFileInputStream);
   var sstream = Components.classes["@mozilla.org/scriptableinputstream;1"]
                           .createInstance(Components.interfaces.nsIScriptableInputStream);
   fstream.init(file, -1, 0, 0);
   sstream.init(fstream);

   //pull the contents of the file out
   var str = sstream.read(4096);
   while (str.length > 0) {
     data += str;
     str = sstream.read(4096);
   }

   sstream.close();
   fstream.close();

   //data = data.replace(/\r|\n|\r\n/g, "");
   return data;
 };

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
    var branch = Components.classes["@mozilla.org/preferences-service;1"].
                 getService(Components.interfaces.nsIPrefBranch);
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
  } catch(e) {
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
    var branch = Components.classes["@mozilla.org/preferences-service;1"].
                 getService(Components.interfaces.nsIPrefBranch);
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
  } catch(e) {
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
  // We basically just call this once after the specified number of milliseconds
  var timeup = false;
  function wait() { timeup = true; }
  hwindow.setTimeout(wait, milliseconds);

  var thread = Components.classes["@mozilla.org/thread-manager;1"].
               getService().currentThread;
  while(!timeup) {
    thread.processNextEvent(true);
  }
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
 * TimeoutError
 *
 * Error object used for timeouts
 */
function TimeoutError(message, fileName, lineNumber) {
  var err = new Error();
  if (err.stack) {
    this.stack = err.stack;
  }
  this.message = message === undefined ? err.message : message;
  this.fileName = fileName === undefined ? err.fileName : fileName;
  this.lineNumber = lineNumber === undefined ? err.lineNumber : lineNumber;
};
TimeoutError.prototype = new Error();
TimeoutError.prototype.constructor = TimeoutError;
TimeoutError.prototype.name = 'TimeoutError';

/**
 * Waits for the callback evaluates to true
 */
function waitFor(callback, message, timeout, interval, thisObject) {
  timeout = timeout || 5000;
  interval = interval || 100;

  var self = {counter: 0, result: callback.call(thisObject)};

  function wait() {
    self.counter += interval;
    self.result = callback.call(thisObject);
  }

  var timeoutInterval = hwindow.setInterval(wait, interval);
  var thread = Components.classes["@mozilla.org/thread-manager;1"].
               getService().currentThread;

  while((self.result != true) && (self.counter < timeout))  {
    thread.processNextEvent(true);
  }

  hwindow.clearInterval(timeoutInterval);

  if (self.counter >= timeout) {
    message = message || arguments.callee.name + ": Timeout exceeded for '" + callback + "'";
    throw new TimeoutError(message);
  }

  return true;
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
function takeScreenshot(node, name, highlights) {
  var rect, win, width, height, left, top, needsOffset;
  // node can be either a window or an arbitrary DOM node
  try {
    win = node.ownerDocument.defaultView;   // node is an arbitrary DOM node
    rect = node.getBoundingClientRect();
    width = rect.width;
    height = rect.height;
    top = rect.top;
    left = rect.left;
    // offset for highlights not needed as they will be relative to this node
    needsOffset = false;
  } catch (e) {
    win = node;                             // node is a window
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
  } // end highlights

  // if there is a name save the file, else return dataURL
  if (name) {
    return saveCanvas(canvas, name);
  }
  return canvas.toDataURL("image/png","");
}

/**
 * Takes a canvas as input and saves it to the file tempdir/name.png
 * Returns the filepath of the saved file
 */
function saveCanvas(canvas, name) {
  var file = Components.classes["@mozilla.org/file/directory_service;1"]
                                .getService(Components.interfaces.nsIProperties)
                                .get("TmpD", Components.interfaces.nsIFile);
  file.append("mozmill_screens");
  file.append(name + ".png");
  file.createUnique(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0666);

  // create a data url from the canvas and then create URIs of the source and targets
  var io = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
  var source = io.newURI(canvas.toDataURL("image/png", ""), "UTF8", null);
  var target = io.newFileURI(file)

  // prepare to save the canvas data
  var persist = Components.classes["@mozilla.org/embedding/browser/nsWebBrowserPersist;1"]
                                   .createInstance(Components.interfaces.nsIWebBrowserPersist);

  persist.persistFlags = Components.interfaces.nsIWebBrowserPersist.PERSIST_FLAGS_REPLACE_EXISTING_FILES;
  persist.persistFlags |= Components.interfaces.nsIWebBrowserPersist.PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

  // save the canvas data to the file
  persist.saveURI(source, null, null, null, null, file);

  return file.path;
}
