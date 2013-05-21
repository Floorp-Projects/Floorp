/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/Troubleshoot.jsm");

window.addEventListener("load", function onload(event) {
  window.removeEventListener("load", onload, false);
  Troubleshoot.snapshot(function (snapshot) {
    for (let prop in snapshotFormatters)
      snapshotFormatters[prop](snapshot[prop]);
  });
  populateResetBox();
}, false);

// Each property in this object corresponds to a property in Troubleshoot.jsm's
// snapshot data.  Each function is passed its property's corresponding data,
// and it's the function's job to update the page with it.
let snapshotFormatters = {

  application: function application(data) {
    $("application-box").textContent = data.name;
    $("useragent-box").textContent = data.userAgent;
    $("supportLink").href = data.supportURL;
    let version = data.version;
    if (data.vendor)
      version += " (" + data.vendor + ")";
    $("version-box").textContent = version;
  },

  extensions: function extensions(data) {
    $.append($("extensions-tbody"), data.map(function (extension) {
      return $.new("tr", [
        $.new("td", extension.name),
        $.new("td", extension.version),
        $.new("td", extension.isActive),
        $.new("td", extension.id),
      ]);
    }));
  },

  modifiedPreferences: function modifiedPreferences(data) {
    $.append($("prefs-tbody"), sortedArrayFromObject(data).map(
      function ([name, value]) {
        return $.new("tr", [
          $.new("td", name, "pref-name"),
          // Very long preference values can cause users problems when they
          // copy and paste them into some text editors.  Long values generally
          // aren't useful anyway, so truncate them to a reasonable length.
          $.new("td", String(value).substr(0, 120), "pref-value"),
        ]);
      }
    ));
  },

  graphics: function graphics(data) {
    // graphics-info-properties tbody
    if ("info" in data) {
      let trs = sortedArrayFromObject(data.info).map(function ([prop, val]) {
        return $.new("tr", [
          $.new("th", prop, "column"),
          $.new("td", String(val)),
        ]);
      });
      $.append($("graphics-info-properties"), trs);
      delete data.info;
    }

    // graphics-failures-tbody tbody
    if ("failures" in data) {
      $.append($("graphics-failures-tbody"), data.failures.map(function (val) {
        return $.new("tr", [$.new("td", val)]);
      }));
      delete data.failures;
    }

    // graphics-tbody tbody

    function localizedMsg(msgArray) {
      let nameOrMsg = msgArray.shift();
      if (msgArray.length) {
        // formatStringFromName logs an NS_ASSERTION failure otherwise that says
        // "use GetStringFromName".  Lame.
        try {
          return strings.formatStringFromName(nameOrMsg, msgArray,
                                              msgArray.length);
        }
        catch (err) {
          // Throws if nameOrMsg is not a name in the bundle.  This shouldn't
          // actually happen though, since msgArray.length > 1 => nameOrMsg is a
          // name in the bundle, not a message, and the remaining msgArray
          // elements are parameters.
          return nameOrMsg;
        }
      }
      try {
        return strings.GetStringFromName(nameOrMsg);
      }
      catch (err) {
        // Throws if nameOrMsg is not a name in the bundle.
      }
      return nameOrMsg;
    }

    let out = Object.create(data);
    let strings = stringBundle();

    out.acceleratedWindows =
      data.numAcceleratedWindows + "/" + data.numTotalWindows;
    if (data.windowLayerManagerType)
      out.acceleratedWindows += " " + data.windowLayerManagerType;
    if (data.numAcceleratedWindowsMessage)
      out.acceleratedWindows +=
        " " + localizedMsg(data.numAcceleratedWindowsMessage);
    delete data.numAcceleratedWindows;
    delete data.numTotalWindows;
    delete data.windowLayerManagerType;
    delete data.numAcceleratedWindowsMessage;

    if ("direct2DEnabledMessage" in data) {
      out.direct2DEnabled = localizedMsg(data.direct2DEnabledMessage);
      delete data.direct2DEnabledMessage;
      delete data.direct2DEnabled;
    }

    if ("directWriteEnabled" in data) {
      out.directWriteEnabled = data.directWriteEnabled;
      if ("directWriteVersion" in data)
        out.directWriteEnabled += " (" + data.directWriteVersion + ")";
      delete data.directWriteEnabled;
      delete data.directWriteVersion;
    }

    if ("webglRendererMessage" in data) {
      out.webglRenderer = localizedMsg(data.webglRendererMessage);
      delete data.webglRendererMessage;
      delete data.webglRenderer;
    }

    let localizedOut = {};
    for (let prop in out) {
      let val = out[prop];
      if (typeof(val) == "string" && !val)
        // Ignore properties that are empty strings.
        continue;
      try {
        var localizedName = strings.GetStringFromName(prop);
      }
      catch (err) {
        // This shouldn't happen, but if there's a reported graphics property
        // that isn't in the string bundle, don't let it break the page.
        localizedName = prop;
      }
      localizedOut[localizedName] = val;
    }
    let trs = sortedArrayFromObject(localizedOut).map(function ([prop, val]) {
      return $.new("tr", [
        $.new("th", prop, "column"),
        $.new("td", val),
      ]);
    });
    $.append($("graphics-tbody"), trs);
  },

  javaScript: function javaScript(data) {
    $("javascript-incremental-gc").textContent = data.incrementalGCEnabled;
  },

  accessibility: function accessibility(data) {
    $("a11y-activated").textContent = data.isActive;
    $("a11y-force-disabled").textContent = data.forceDisabled || 0;
  },

  libraryVersions: function libraryVersions(data) {
    let strings = stringBundle();
    let trs = [
      $.new("tr", [
        $.new("th", ""),
        $.new("th", strings.GetStringFromName("minLibVersions")),
        $.new("th", strings.GetStringFromName("loadedLibVersions")),
      ])
    ];
    sortedArrayFromObject(data).forEach(
      function ([name, val]) {
        trs.push($.new("tr", [
          $.new("td", name),
          $.new("td", val.minVersion),
          $.new("td", val.version),
        ]));
      }
    );
    $.append($("libversions-tbody"), trs);
  },

  userJS: function userJS(data) {
    if (!data.exists)
      return;
    let userJSFile = Services.dirsvc.get("PrefD", Ci.nsIFile);
    userJSFile.append("user.js");
    $("prefs-user-js-link").href = Services.io.newFileURI(userJSFile).spec;
    $("prefs-user-js-section").style.display = "";
    // Clear the no-copy class
    $("prefs-user-js-section").className = "";
  },
};

let $ = document.getElementById.bind(document);

$.new = function $_new(tag, textContentOrChildren, className) {
  let elt = document.createElement(tag);
  if (className)
    elt.className = className;
  if (Array.isArray(textContentOrChildren))
    this.append(elt, textContentOrChildren);
  else
    elt.textContent = String(textContentOrChildren);
  return elt;
};

$.append = function $_append(parent, children) {
  children.forEach(function (c) parent.appendChild(c));
};

function stringBundle() {
  return Services.strings.createBundle(
           "chrome://global/locale/aboutSupport.properties");
}

function sortedArrayFromObject(obj) {
  let tuples = [];
  for (let prop in obj)
    tuples.push([prop, obj[prop]]);
  tuples.sort(function ([prop1, v1], [prop2, v2]) prop1.localeCompare(prop2));
  return tuples;
}

function copyRawDataToClipboard(button) {
  if (button)
    button.disabled = true;
  try {
    Troubleshoot.snapshot(function (snapshot) {
      if (button)
        button.disabled = false;
      let str = Cc["@mozilla.org/supports-string;1"].
                createInstance(Ci.nsISupportsString);
      str.data = JSON.stringify(snapshot, undefined, 2);
      let transferable = Cc["@mozilla.org/widget/transferable;1"].
                         createInstance(Ci.nsITransferable);
      transferable.init(getLoadContext());
      transferable.addDataFlavor("text/unicode");
      transferable.setTransferData("text/unicode", str, str.data.length * 2);
      Cc["@mozilla.org/widget/clipboard;1"].
        getService(Ci.nsIClipboard).
        setData(transferable, null, Ci.nsIClipboard.kGlobalClipboard);
#ifdef ANDROID
      // Present a toast notification.
      let message = {
        type: "Toast:Show",
        message: stringBundle().GetStringFromName("rawDataCopied"),
        duration: "short"
      };
      Cc["@mozilla.org/android/bridge;1"].
        getService(Ci.nsIAndroidBridge).
        handleGeckoMessage(JSON.stringify(message));
#endif
    });
  }
  catch (err) {
    if (button)
      button.disabled = false;
    throw err;
  }
}

function getLoadContext() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsILoadContext);
}

function copyContentsToClipboard() {
  // Get the HTML and text representations for the important part of the page.
  let contentsDiv = $("contents");
  let dataHtml = contentsDiv.innerHTML;
  let dataText = createTextForElement(contentsDiv);

  // We can't use plain strings, we have to use nsSupportsString.
  let supportsStringClass = Cc["@mozilla.org/supports-string;1"];
  let ssHtml = supportsStringClass.createInstance(Ci.nsISupportsString);
  let ssText = supportsStringClass.createInstance(Ci.nsISupportsString);

  let transferable = Cc["@mozilla.org/widget/transferable;1"]
                       .createInstance(Ci.nsITransferable);
  transferable.init(getLoadContext());

  // Add the HTML flavor.
  transferable.addDataFlavor("text/html");
  ssHtml.data = dataHtml;
  transferable.setTransferData("text/html", ssHtml, dataHtml.length * 2);

  // Add the plain text flavor.
  transferable.addDataFlavor("text/unicode");
  ssText.data = dataText;
  transferable.setTransferData("text/unicode", ssText, dataText.length * 2);

  // Store the data into the clipboard.
  let clipboard = Cc["@mozilla.org/widget/clipboard;1"]
                    .getService(Ci.nsIClipboard);
  clipboard.setData(transferable, null, clipboard.kGlobalClipboard);

#ifdef ANDROID
  // Present a toast notification.
  let message = {
    type: "Toast:Show",
    message: stringBundle().GetStringFromName("textCopied"),
    duration: "short"
  };
  Cc["@mozilla.org/android/bridge;1"].
    getService(Ci.nsIAndroidBridge).
    handleGeckoMessage(JSON.stringify(message));
#endif
}

// Return the plain text representation of an element.  Do a little bit
// of pretty-printing to make it human-readable.
function createTextForElement(elem) {
  // Generate the initial text.
  let textFragmentAccumulator = [];
  generateTextForElement(elem, "", textFragmentAccumulator);
  let text = textFragmentAccumulator.join("");

  // Trim extraneous whitespace before newlines, then squash extraneous
  // blank lines.
  text = text.replace(/[ \t]+\n/g, "\n");
  text = text.replace(/\n\n\n+/g, "\n\n");

  // Actual CR/LF pairs are needed for some Windows text editors.
#ifdef XP_WIN
  text = text.replace(/\n/g, "\r\n");
#endif

  return text;
}

function generateTextForElement(elem, indent, textFragmentAccumulator) {
  if (elem.classList.contains("no-copy"))
    return;

  // Add a little extra spacing around most elements.
  if (elem.tagName != "td")
    textFragmentAccumulator.push("\n");

  // Generate the text representation for each child node.
  let node = elem.firstChild;
  while (node) {

    if (node.nodeType == Node.TEXT_NODE) {
      // Text belonging to this element uses its indentation level.
      generateTextForTextNode(node, indent, textFragmentAccumulator);
    }
    else if (node.nodeType == Node.ELEMENT_NODE) {
      // Recurse on the child element with an extra level of indentation.
      generateTextForElement(node, indent + "  ", textFragmentAccumulator);
    }

    // Advance!
    node = node.nextSibling;
  }
}

function generateTextForTextNode(node, indent, textFragmentAccumulator) {
  // If the text node is the first of a run of text nodes, then start
  // a new line and add the initial indentation.
  let prevNode = node.previousSibling;
  if (!prevNode || prevNode.nodeType == Node.TEXT_NODE)
    textFragmentAccumulator.push("\n" + indent);

  // Trim the text node's text content and add proper indentation after
  // any internal line breaks.
  let text = node.textContent.trim().replace("\n", "\n" + indent, "g");
  textFragmentAccumulator.push(text);
}

function openProfileDirectory() {
  // Get the profile directory.
  let currProfD = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let profileDir = currProfD.path;

  // Show the profile directory.
  let nsLocalFile = Components.Constructor("@mozilla.org/file/local;1",
                                           "nsILocalFile", "initWithPath");
  new nsLocalFile(profileDir).reveal();
}

/**
 * Profile reset is only supported for the default profile if the appropriate migrator exists.
 */
function populateResetBox() {
  if (resetSupported())
    $("reset-box").style.visibility = "visible";
}

/**
 * Restart the application to reset the profile.
 */
function resetProfileAndRestart() {
  let branding = Services.strings.createBundle("chrome://branding/locale/brand.properties");
  let brandShortName = branding.GetStringFromName("brandShortName");

  // Prompt the user to confirm.
  let retVals = {
    reset: false,
  };
  window.openDialog("chrome://global/content/resetProfile.xul", null,
                    "chrome,modal,centerscreen,titlebar,dialog=yes", retVals);
  if (!retVals.reset)
    return;

  // Set the reset profile environment variable.
  let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);
  env.set("MOZ_RESET_PROFILE_RESTART", "1");

  let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
  appStartup.quit(Ci.nsIAppStartup.eForceQuit | Ci.nsIAppStartup.eRestart);
}
