/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Troubleshoot.jsm");
Cu.import("resource://gre/modules/ResetProfile.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

window.addEventListener("load", function onload(event) {
  window.removeEventListener("load", onload, false);
  Troubleshoot.snapshot(function (snapshot) {
    for (let prop in snapshotFormatters)
      snapshotFormatters[prop](snapshot[prop]);
  });
  populateResetBox();
  setupEventListeners();
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

#ifdef MOZ_CRASHREPORTER
  crashes: function crashes(data) {
    let strings = stringBundle();
    let daysRange = Troubleshoot.kMaxCrashAge / (24 * 60 * 60 * 1000);
    $("crashes-title").textContent =
      PluralForm.get(daysRange, strings.GetStringFromName("crashesTitle"))
                .replace("#1", daysRange);
    let reportURL;
    try {
      reportURL = Services.prefs.getCharPref("breakpad.reportURL");
      // Ignore any non http/https urls
      if (!/^https?:/i.test(reportURL))
        reportURL = null;
    }
    catch (e) { }
    if (!reportURL) {
      $("crashes-noConfig").style.display = "block";
      $("crashes-noConfig").classList.remove("no-copy");
      return;
    }
    else {
      $("crashes-allReports").style.display = "block";
      $("crashes-allReports").classList.remove("no-copy");
    }

    if (data.pending > 0) {
      $("crashes-allReportsWithPending").textContent =
        PluralForm.get(data.pending, strings.GetStringFromName("pendingReports"))
                  .replace("#1", data.pending);
    }

    let dateNow = new Date();
    $.append($("crashes-tbody"), data.submitted.map(function (crash) {
      let date = new Date(crash.date);
      let timePassed = dateNow - date;
      let formattedDate;
      if (timePassed >= 24 * 60 * 60 * 1000)
      {
        let daysPassed = Math.round(timePassed / (24 * 60 * 60 * 1000));
        let daysPassedString = strings.GetStringFromName("crashesTimeDays");
        formattedDate = PluralForm.get(daysPassed, daysPassedString)
                                  .replace("#1", daysPassed);
      }
      else if (timePassed >= 60 * 60 * 1000)
      {
        let hoursPassed = Math.round(timePassed / (60 * 60 * 1000));
        let hoursPassedString = strings.GetStringFromName("crashesTimeHours");
        formattedDate = PluralForm.get(hoursPassed, hoursPassedString)
                                  .replace("#1", hoursPassed);
      }
      else
      {
        let minutesPassed = Math.max(Math.round(timePassed / (60 * 1000)), 1);
        let minutesPassedString = strings.GetStringFromName("crashesTimeMinutes");
        formattedDate = PluralForm.get(minutesPassed, minutesPassedString)
                                  .replace("#1", minutesPassed);
      }
      return $.new("tr", [
        $.new("td", [
          $.new("a", crash.id, null, {href : reportURL + crash.id})
        ]),
        $.new("td", formattedDate)
      ]);
    }));
  },
#endif

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

  experiments: function experiments(data) {
    $.append($("experiments-tbody"), data.map(function (experiment) {
      return $.new("tr", [
        $.new("td", experiment.name),
        $.new("td", experiment.id),
        $.new("td", experiment.description),
        $.new("td", experiment.active),
        $.new("td", experiment.endDate),
        $.new("td", [
          $.new("a", experiment.detailURL, null, {href : experiment.detailURL,})
        ]),
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
    if (data.windowLayerManagerRemote)
      out.acceleratedWindows += " (OMTC)";
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

$.new = function $_new(tag, textContentOrChildren, className, attributes) {
  let elt = document.createElement(tag);
  if (className)
    elt.className = className;
  if (attributes) {
    for (let attrName in attributes)
      elt.setAttribute(attrName, attributes[attrName]);
  }
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
      Services.androidBridge.handleGeckoMessage(message);
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
  Services.androidBridge.handleGeckoMessage(message);
#endif
}

// Return the plain text representation of an element.  Do a little bit
// of pretty-printing to make it human-readable.
function createTextForElement(elem) {
  let serializer = new Serializer();
  let text = serializer.serialize(elem);

  // Actual CR/LF pairs are needed for some Windows text editors.
#ifdef XP_WIN
  text = text.replace(/\n/g, "\r\n");
#endif

  return text;
}

function Serializer() {
}

Serializer.prototype = {

  serialize: function (rootElem) {
    this._lines = [];
    this._startNewLine();
    this._serializeElement(rootElem);
    this._startNewLine();
    return this._lines.join("\n").trim() + "\n";
  },

  // The current line is always the line that writing will start at next.  When
  // an element is serialized, the current line is updated to be the line at
  // which the next element should be written.
  get _currentLine() {
    return this._lines.length ? this._lines[this._lines.length - 1] : null;
  },

  set _currentLine(val) {
    return this._lines[this._lines.length - 1] = val;
  },

  _serializeElement: function (elem) {
    if (this._ignoreElement(elem))
      return;

    // table
    if (elem.localName == "table") {
      this._serializeTable(elem);
      return;
    }

    // all other elements

    let hasText = false;
    for (let child of elem.childNodes) {
      if (child.nodeType == Node.TEXT_NODE) {
        let text = this._nodeText(child);
        this._appendText(text);
        hasText = hasText || !!text.trim();
      }
      else if (child.nodeType == Node.ELEMENT_NODE)
        this._serializeElement(child);
    }

    // For headings, draw a "line" underneath them so they stand out.
    if (/^h[0-9]+$/.test(elem.localName)) {
      let headerText = (this._currentLine || "").trim();
      if (headerText) {
        this._startNewLine();
        this._appendText("-".repeat(headerText.length));
      }
    }

    // Add a blank line underneath block elements but only if they contain text.
    if (hasText) {
      let display = window.getComputedStyle(elem).getPropertyValue("display");
      if (display == "block") {
        this._startNewLine();
        this._startNewLine();
      }
    }
  },

  _startNewLine: function (lines) {
    let currLine = this._currentLine;
    if (currLine) {
      // The current line is not empty.  Trim it.
      this._currentLine = currLine.trim();
      if (!this._currentLine)
        // The current line became empty.  Discard it.
        this._lines.pop();
    }
    this._lines.push("");
  },

  _appendText: function (text, lines) {
    this._currentLine += text;
  },

  _serializeTable: function (table) {
    // Collect the table's column headings if in fact there are any.  First
    // check thead.  If there's no thead, check the first tr.
    let colHeadings = {};
    let tableHeadingElem = table.querySelector("thead");
    if (!tableHeadingElem)
      tableHeadingElem = table.querySelector("tr");
    if (tableHeadingElem) {
      let tableHeadingCols = tableHeadingElem.querySelectorAll("th,td");
      // If there's a contiguous run of th's in the children starting from the
      // rightmost child, then consider them to be column headings.
      for (let i = tableHeadingCols.length - 1; i >= 0; i--) {
        if (tableHeadingCols[i].localName != "th")
          break;
        colHeadings[i] = this._nodeText(tableHeadingCols[i]).trim();
      }
    }
    let hasColHeadings = Object.keys(colHeadings).length > 0;
    if (!hasColHeadings)
      tableHeadingElem = null;

    let trs = table.querySelectorAll("table > tr, tbody > tr");
    let startRow =
      tableHeadingElem && tableHeadingElem.localName == "tr" ? 1 : 0;

    if (startRow >= trs.length)
      // The table's empty.
      return;

    if (hasColHeadings && !this._ignoreElement(tableHeadingElem)) {
      // Use column headings.  Print each tr as a multi-line chunk like:
      //   Heading 1: Column 1 value
      //   Heading 2: Column 2 value
      for (let i = startRow; i < trs.length; i++) {
        if (this._ignoreElement(trs[i]))
          continue;
        let children = trs[i].querySelectorAll("td");
        for (let j = 0; j < children.length; j++) {
          let text = "";
          if (colHeadings[j])
            text += colHeadings[j] + ": ";
          text += this._nodeText(children[j]).trim();
          this._appendText(text);
          this._startNewLine();
        }
        this._startNewLine();
      }
      return;
    }

    // Don't use column headings.  Assume the table has only two columns and
    // print each tr in a single line like:
    //   Column 1 value: Column 2 value
    for (let i = startRow; i < trs.length; i++) {
      if (this._ignoreElement(trs[i]))
        continue;
      let children = trs[i].querySelectorAll("th,td");
      let rowHeading = this._nodeText(children[0]).trim();
      this._appendText(rowHeading + ": " + this._nodeText(children[1]).trim());
      this._startNewLine();
    }
    this._startNewLine();
  },

  _ignoreElement: function (elem) {
    return elem.classList.contains("no-copy");
  },

  _nodeText: function (node) {
    return node.textContent.replace(/\s+/g, " ");
  },
};

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
  if (ResetProfile.resetSupported())
    $("reset-box").style.visibility = "visible";
}

/**
 * Set up event listeners for buttons.
 */
function setupEventListeners(){
  $("show-update-history-button").addEventListener("click", function (event) {
    var prompter = Cc["@mozilla.org/updates/update-prompt;1"].createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateHistory(window);
  });
  $("reset-box-button").addEventListener("click", function (event){
    ResetProfile.openConfirmationDialog(window);
  });
  $("copy-raw-data-to-clipboard").addEventListener("click", function (event){
    copyRawDataToClipboard(this);
  });
  $("copy-to-clipboard").addEventListener("click", function (event){
    copyContentsToClipboard();
  });
  $("profile-dir-button").addEventListener("click", function (event){
    openProfileDirectory();
  });
}
