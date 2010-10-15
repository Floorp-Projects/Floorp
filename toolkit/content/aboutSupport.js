# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is aboutSupport.xhtml.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Curtis Bartley <cbartley@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/AddonManager.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const ELLIPSIS = Services.prefs.getComplexValue("intl.ellipsis",
                                                Ci.nsIPrefLocalizedString).data;

// We use a preferences whitelist to make sure we only show preferences that
// are useful for support and won't compromise the user's privacy.  Note that
// entries are *prefixes*: for example, "accessibility." applies to all prefs
// under the "accessibility.*" branch.
const PREFS_WHITELIST = [
  "accessibility.",
  "browser.fixup.",
  "browser.history_expire_",
  "browser.link.open_newwindow",
  "browser.mousewheel.",
  "browser.places.",
  "browser.startup.homepage",
  "browser.tabs.",
  "browser.zoom.",
  "dom.",
  "extensions.checkCompatibility",
  "extensions.lastAppVersion",
  "font.",
  "general.useragent.",
  "gfx.",
  "mozilla.widget.render-mode",
  "layers.",
  "javascript.",
  "keyword.",
  "layout.css.dpi",
  "network.",
  "places.",
  "print.",
  "privacy.",
  "security."
];

// The blacklist, unlike the whitelist, is a list of regular expressions.
const PREFS_BLACKLIST = [
  /^network[.]proxy[.]/,
  /[.]print_to_filename$/,
];

window.onload = function () {
  // Get the support URL.
  let urlFormatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"]
                       .getService(Ci.nsIURLFormatter);
  let supportUrl = urlFormatter.formatURLPref("app.support.baseURL");

  // Update the application basics section.
  document.getElementById("application-box").textContent = Services.appinfo.name;
  document.getElementById("useragent-box").textContent = navigator.userAgent;
  document.getElementById("supportLink").href = supportUrl;
  let version = Services.appinfo.version;
  try {
    version += " (" + Services.prefs.getCharPref("app.support.vendor") + ")";
  } catch (e) {
  }
  document.getElementById("version-box").textContent = version;

  // Update the other sections.
  populatePreferencesSection();
  populateExtensionsSection();
  populateGraphicsSection();
}

function populateExtensionsSection() {
  AddonManager.getAddonsByTypes(["extension"], function(extensions) {
    let trExtensions = [];
    for (let i = 0; i < extensions.length; i++) {
      let extension = extensions[i];
      let tr = createParentElement("tr", [
        createElement("td", extension.name),
        createElement("td", extension.version),
        createElement("td", extension.isActive),
        createElement("td", extension.id),
      ]);
      trExtensions.push(tr);
    }
    appendChildren(document.getElementById("extensions-tbody"), trExtensions);
  });
}

function populatePreferencesSection() {
  let modifiedPrefs = getModifiedPrefs();

  function comparePrefs(pref1, pref2) {
    if (pref1.name < pref2.name)
      return -1;
    if (pref1.name > pref2.name)
      return 1;
    return 0;
  }

  let sortedPrefs = modifiedPrefs.sort(comparePrefs);

  let trPrefs = [];
  sortedPrefs.forEach(function (pref) {
    let tdName = createElement("td", pref.name, "pref-name");
    let tdValue = createElement("td", formatPrefValue(pref.value), "pref-value");
    let tr = createParentElement("tr", [tdName, tdValue]);
    trPrefs.push(tr);
  });

  appendChildren(document.getElementById("prefs-tbody"), trPrefs);
}

function populateGraphicsSection() {
  function createHeader(name)
  {
    let elem = createElement("th", name);
    elem.className = "column";
    return elem;
  }

  let bundle = Services.strings.createBundle("chrome://global/locale/aboutSupport.properties");
  let graphics_tbody = document.getElementById("graphics-tbody");

  var gfxInfo = null;
  try {
    // nsIGfxInfo is currently only implemented on Windows
    gfxInfo = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo);
  } catch(e) {}

  if (gfxInfo) {
    let trGraphics = [];
    trGraphics.push(createParentElement("tr", [
      createHeader(bundle.GetStringFromName("adapterDescription")),
      createElement("td", gfxInfo.adapterDescription),
    ]));
    trGraphics.push(createParentElement("tr", [
      createHeader(bundle.GetStringFromName("adapterVendorID")),
      // pad with zeros. (printf would be nicer)
      createElement("td", String('0000'+gfxInfo.adapterVendorID.toString(16)).slice(-4)),
    ]));
    trGraphics.push(createParentElement("tr", [
      createHeader(bundle.GetStringFromName("adapterDeviceID")),
      // pad with zeros. (printf would be nicer)
      createElement("td", String('0000'+gfxInfo.adapterDeviceID.toString(16)).slice(-4)),
    ]));
    trGraphics.push(createParentElement("tr", [
      createHeader(bundle.GetStringFromName("adapterRAM")),
      createElement("td", gfxInfo.adapterRAM),
    ]));
    trGraphics.push(createParentElement("tr", [
      createHeader(bundle.GetStringFromName("adapterDrivers")),
      createElement("td", gfxInfo.adapterDriver),
    ]));
    trGraphics.push(createParentElement("tr", [
      createHeader(bundle.GetStringFromName("driverVersion")),
      createElement("td", gfxInfo.adapterDriverVersion),
    ]));
    trGraphics.push(createParentElement("tr", [
      createHeader(bundle.GetStringFromName("driverDate")),
      createElement("td", gfxInfo.adapterDriverDate),
    ]));

    var d2dEnabled = false;
    try {
      d2dEnabled = gfxInfo.D2DEnabled;
    } catch(e) {}
    var d2dMessage = d2dEnabled;
    if (!d2dEnabled) {
      var d2dStatus = -1; // different from any status value defined in the IDL
      try {
        d2dStatus = gfxInfo.getFeatureStatus(gfxInfo.FEATURE_DIRECT2D);
      } catch(e) {
        window.dump(e + '\n');
      }  
      if (d2dStatus == gfxInfo.FEATURE_BLOCKED_DEVICE ||
          d2dStatus == gfxInfo.FEATURE_DISCOURAGED)
      {
        d2dMessage = bundle.GetStringFromName("blockedGraphicsCard");
      }
      else if (d2dStatus == gfxInfo.FEATURE_BLOCKED_DRIVER_VERSION)
      {
        var d2dSuggestedDriverVersion = null;
        try {
          gfxInfo.getFeatureSuggestedDriverVersion(gfxInfo.FEATURE_DIRECT2D);
        } catch(e) {
          window.dump(e + '\n');
        }
        if (d2dSuggestedDriverVersion) {
          d2dMessage = bundle.GetStringFromName("tryNewerDriverVersion").replace("%1", d2dSuggestedDriverVersion);
        }
      }
    }
    trGraphics.push(createParentElement("tr", [
      createHeader(bundle.GetStringFromName("direct2DEnabled")),
      createElement("td", d2dMessage),
    ]));

    var dwEnabled = false;
    try {
      dwEnabled = gfxInfo.DWriteEnabled;
    } catch(e) {}
    trGraphics.push(createParentElement("tr", [
      createHeader(bundle.GetStringFromName("directWriteEnabled")),
      createElement("td", dwEnabled),
    ]));

    appendChildren(graphics_tbody, trGraphics);

  } // end if (gfxInfo)

  let windows = Services.ww.getWindowEnumerator();
  let acceleratedWindows = 0;
  let totalWindows = 0;
  let mgrType;
  while (windows.hasMoreElements()) {
    totalWindows++;

    let awindow = windows.getNext().QueryInterface(Ci.nsIInterfaceRequestor);
    let windowutils = awindow.getInterface(Ci.nsIDOMWindowUtils);
    if (windowutils.layerManagerType != "Basic") {
      acceleratedWindows++;
      mgrType = windowutils.layerManagerType;
    }
  }

  let msg = acceleratedWindows + "/" + totalWindows;
  if (acceleratedWindows)
    msg += " " + mgrType;

  let header = createHeader(bundle.GetStringFromName("acceleratedWindows"));

  appendChildren(graphics_tbody, [ header, createElement("td", msg) ]);
}

function getPrefValue(aName) {
  let value = "";
  let type = Services.prefs.getPrefType(aName);
  switch (type) {
    case Ci.nsIPrefBranch2.PREF_STRING:
      value = Services.prefs.getComplexValue(aName, Ci.nsISupportsString).data;
      break;
    case Ci.nsIPrefBranch2.PREF_BOOL:
      value = Services.prefs.getBoolPref(aName);
      break;
    case Ci.nsIPrefBranch2.PREF_INT:
      value = Services.prefs.getIntPref(aName);
      break;
  }

  return { name: aName, value: value };
}

function formatPrefValue(prefValue) {
  // Some pref values are really long and don't have spaces.  This can cause
  // problems when copying and pasting into some WYSIWYG editors.  In general
  // the exact contents of really long pref values aren't particularly useful,
  // so we truncate them to some reasonable length.
  let maxPrefValueLen = 120;
  let text = "" + prefValue;
  if (text.length > maxPrefValueLen)
    text = text.substring(0, maxPrefValueLen) + ELLIPSIS;
  return text;
}

function getModifiedPrefs() {
  // We use the low-level prefs API to identify prefs that have been
  // modified, rather that Application.prefs.all since the latter is
  // much, much slower.  Application.prefs.all also gets slower each
  // time it's called.  See bug 517312.
  let prefNames = getWhitelistedPrefNames();
  let prefs = [getPrefValue(prefName)
                      for each (prefName in prefNames)
                          if (Services.prefs.prefHasUserValue(prefName)
                            && !isBlacklisted(prefName))];
  return prefs;
}

function getWhitelistedPrefNames() {
  let results = [];
  PREFS_WHITELIST.forEach(function (prefStem) {
    let prefNames = Services.prefs.getChildList(prefStem);
    results = results.concat(prefNames);
  });
  return results;
}

function isBlacklisted(prefName) {
  return PREFS_BLACKLIST.some(function (re) re.test(prefName));
}

function createParentElement(tagName, childElems) {
  let elem = document.createElement(tagName);
  appendChildren(elem, childElems);
  return elem;
}

function createElement(tagName, textContent, opt_class) {
  let elem = document.createElement(tagName);
  elem.textContent = textContent;
  elem.className = opt_class || "";
  return elem;
}

function appendChildren(parentElem, childNodes) {
  for (let i = 0; i < childNodes.length; i++)
    parentElem.appendChild(childNodes[i]);
}

function copyContentsToClipboard() {
  // Get the HTML and text representations for the important part of the page.
  let contentsDiv = document.getElementById("contents");
  let dataHtml = contentsDiv.innerHTML;
  let dataText = createTextForElement(contentsDiv);

  // We can't use plain strings, we have to use nsSupportsString.
  let supportsStringClass = Cc["@mozilla.org/supports-string;1"];
  let ssHtml = supportsStringClass.createInstance(Ci.nsISupportsString);
  let ssText = supportsStringClass.createInstance(Ci.nsISupportsString);

  let transferable = Cc["@mozilla.org/widget/transferable;1"]
                       .createInstance(Ci.nsITransferable);

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
