/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; -*- */
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
 * The Original Code is The Original Code is Mozilla Automated Testing Code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Dave Liebreich <davel@mozilla.com>
 *  Clint Talbert <ctalbert@mozilla.com>
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
const Cc = Components.classes;
const Ci = Components.interfaces;
 var searchService = Cc["@mozilla.org/browser/search-service;1"]
                    .getService(Ci.nsIBrowserSearchService);
var engines = searchService.getVisibleEngines({ });
var engineIndex = 0;

var pb = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch2);
var prefs=pb.getChildList('', {});
prefs.sort();

var ww = Cc["@mozilla.org/embedcomp/window-watcher;1"]
        .getService(Ci.nsIWindowWatcher);
var w = ww.openWindow(null,"chrome://browser/content/browser.xul",null,null,null);

var gConvStream;

function createOutputFile() {
  var dirServices = Cc["@mozilla.org/file/directory_service;1"]
                    .createInstance(Ci.nsIProperties);
  var file = dirServices.get("CurWorkD", Components.interfaces.nsIFile);
  file.append("test-output.xml");
  if (file.exists()) {
    file.remove(false);
  }

  var outFile = Cc["@mozilla.org/network/file-output-stream;1"]
             .createInstance(Ci.nsIFileOutputStream);
  const MODE_WRONLY = 0x02;
  const MODE_CREATE = 0x08;
  const MODE_TRUNCATE = 0x20;
  const MODE_APPEND = 0x10;
  outFile.init(file, MODE_WRONLY | MODE_CREATE | MODE_APPEND | MODE_TRUNCATE,
                0600, 0);
  // Need to create the converterStream
  gConvStream = Cc["@mozilla.org/intl/converter-output-stream;1"]
                .createInstance(Ci.nsIConverterOutputStream);
  gConvStream.init(outFile, "UTF-8", 0, 0x0000);

  // Seed the file by writing the XML header
  output("<?xml version=\"1.0\"?>\n<testrun>");
}

function output(s) {
  s = escape(s);
  gConvStream.writeString(s);
}

function escape(s) {
  // Only escapes ampersands.
  var str = s.replace(/&/g, "&amp;");
  return str;
}
function closeOutputFile() {
  output("\n</testrun>\n");
  gConvStream.close();
}

function listEngines() {
  // search field
  var submission = null;
  var engineIndex = 0;

  function outputEngine(idx, eng, submission) {
    var url = null;
    if (submission && submission.uri)
       url = submission.uri.spec;
    if (url)
      output("<l>" + idx + "|" + eng.name + "|" + url + "</l>\n");
    else
      output("<l>" + idx + "|" + eng.name + "|" + "none" + "</l>\n");
  }

  // List out the default engine information such as icon, description, and
  // hidden status
  output("\n<section id=\"searchengine\">\n");
  for each (var engine in engines) {
    engineIndex++;
    // Output the engine details
    if (engine.iconURI)
      output("<l>" + engineIndex + "|" + engine.name + "|" + engine.description +
             "|" + engine.hidden + "|" + engine.alias + "|" + engine.iconURI.spec + "</l>\n");
    else
      output("<l>" + engineIndex + "|" + engine.name + "|" + engine.description +
             "|" + engine.hidden + "|" + engine.alias + "|none</l>\n");

    // Output the normal Search URL for when the user types a query into the
    submission = engine.getSubmission("foo", null);
    outputEngine(engineIndex, engine, submission);

    // List the search Form URL that is used when the field is left blank and the
    // user hits enter.
    if (engine.searchForm)
      output("<l>" + engineIndex + "|" + engine.name + "|" + engine.searchForm + "</l>\n");
    else
      output("<l>" + engineIndex + "|" + engine.name + "|" + "none</l>\n");

    // List the search suggest URL.
    submission = engine.getSubmission("foo", "application/x-suggestions+json");
    outputEngine(engineIndex, engine, submission);
  }
  output("</section>");
}

function listPrefs() {
  output("\n<section id=\"preferences\">\n");
  var reStrings = ['(app\.distributor)',
                   '(app\.partner)',
                   '(app\.update\.channel)',
                   '(homepage)',
                   '(startup)',
                   '(browser\.contentHandlers)',
                   '(gecko\.handlerService\.schemes)',
                   '(browser\.places\.smartBookmarksVersion)'];
  var grab = new RegExp(reStrings.join('|'));
  for (var i=0; i < prefs.length; ++i) {
    var pref = prefs[i], pval = [pref, null];
    var ptype = pb.getPrefType(pref);
    try {
      switch (ptype) {
      case pb.PREF_BOOL:
        pval[1] = String(pb.getBoolPref(pref));
        break;
      case pb.PREF_INT:
        if (pref in prefMirror) {
          pval[1] = String(prefMirror[pref]);
        }
        else {
          pval[1] = String(pb.getIntPref(pref));
        }
        break;
      case pb.PREF_STRING:
        pval[1] = pb.getComplexValue(pref, Ci.nsISupportsString).data;
        if (pval[1].match(/.+\.properties$/)) {
          var data = pb.getComplexValue(pref, Ci.nsIPrefLocalizedString).data;
          pval[1] = data;
        }
        break;
      }
    }
    catch (e) {
      Components.utils.reportError("Preference " +  pref + " triggered " + e);
    }
    if (String(pval[0]).match(grab)) {
      output("<l>" + String(pval[0]) + "|" + String(pval[1]) + "</l>\n");
    }
  }
  output("</section>");
}

function handleEULA() {
  var dirUtils = Cc["@mozilla.org/file/directory_service;1"]
                 .createInstance(Ci.nsIProperties);
  var eulaFile = dirUtils.get("CurWorkD", Components.interfaces.nsIFile);
  eulaFile.append("EULA.txt");
  if (eulaFile.exists()) {
    // Then we have a EULA file, write that information into the XML output
    output("\n<section id=\"eula\">\n");
    var istream = Cc["@mozilla.org/network/file-input-stream;1"]
                 .createInstance(Ci.nsIFileInputStream);
    istream.init(eulaFile, 0x01, 0444, 0);
    istream.QueryInterface(Ci.nsILineInputStream);

    // read lines into array
    var line = {}, hasmore;
    var isFirstLine = true;
    do {
      hasmore = istream.readLine(line);
      output("<l>" + line.value + "</l>\n");
     } while(hasmore);

    istream.close();
    output("</section>");
  }
}

function listBookmarks() {
  // Exports bookmarks to a testbookmarks.html file
  // If we're not less than or equal to a 1.8 build, then we're using places
  var dirUtils = Cc["@mozilla.org/file/directory_service;1"]
                 .createInstance(Ci.nsIProperties);
  var file = dirUtils.get("CurWorkD", Components.interfaces.nsIFile);
  file.append("test-bookmarks.html");
  if (file.exists()) {
    file.remove(false);
  }

  // TODO: Checking the gecko engine level to determine whether or not
  // to do places style calls.
  // If version <= 1.8 we will use old bookmarks
  // If version >= 1.9 we will use places calls
  var appInfo = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULAppInfo);
  var appInfoAry = appInfo.platformVersion.split(".");
  if ( (appInfoAry[0] == "1") && (parseInt(appInfoAry[1], 10) <= 8) ) {
    var i = 0;
    var bmsvc = Cc["@mozilla.org/browser/bookmarks-service;1"]
                .getService(Ci.nsIBookmarksService);
    var RDF = Cc["@mozilla.org/rdf/rdf-service;1"]
               .getService(Ci.nsIRDFService);
    var selection = RDF.GetResource("NC:BookmarksRoot");
    var fname = file.path;
    var args = [{property: "http://home.netscape.com/NC-rdf#URL", literal:fname}];
    initServices();
    initBMService();
    if (BMSVC) {
      // Declared in bookmarks.js
      BMSVC.readBookmarks();
    }
    BookmarksCommand.doBookmarksCommand(selection,
                    "http://home.netscape.com/NC-rdf#command?cmd=export", args);
  } else {
    file.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0644);
    var placesExporter = Cc["@mozilla.org/browser/places/import-export-service;1"]
                         .getService(Ci.nsIPlacesImportExportService);
    placesExporter.exportHTMLToFile(file);
  }
}

function listExtensions() {
  // Gets a list of extensions from the extension manager
  output("\n<section id=\"extensions\">\n");
  var extmgr = Cc["@mozilla.org/extensions/manager;1"]
               .getService(Ci.nsIExtensionManager);
  var exts = extmgr.getItemList(Ci.nsIUpdateItem.TYPE_ANY, { });
  for (var i=0; i < exts.length; ++i) {
      var item = exts[i];
      // Don't output DOM inspector and Ffx default theme information
      if (item.id != "inspector@mozilla.org" &&
          item.id != "{972ce4c6-7e08-4474-a285-3208198ce6fd}")
        output("<l>" + i + "|" + item.name + "|" + item.id + "|" + item.version + "|"
              + item.iconURL + "|" + item.xpiURL + "|" + item.type + "</l>\n");
  }
  output("</section>");
}

function listUpdates() {
  var prompter = Cc["@mozilla.org/updates/update-prompt;1"]
                 .createInstance(Ci.nsIUpdatePrompt);
  prompter.checkForUpdates();
}

createOutputFile();
listEngines();
listPrefs();
listExtensions();
handleEULA();
listBookmarks();
listUpdates();
closeOutputFile();
setTimeout(goQuitApplication, 1500);
