/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// You can direct about:memory to immediately load memory reports from a file
// by providing a file= query string.  For example,
//
//     about:memory?file=/home/username/reports.json.gz
//
// "file=" is not case-sensitive.  We'll URI-unescape the contents of the
// "file=" argument, and obviously the filename is case-sensitive iff you're on
// a case-sensitive filesystem.  If you specify more than one "file=" argument,
// only the first one is used.

"use strict";

// ---------------------------------------------------------------------------

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var CC = Components.Constructor;

const KIND_NONHEAP           = Ci.nsIMemoryReporter.KIND_NONHEAP;
const KIND_HEAP              = Ci.nsIMemoryReporter.KIND_HEAP;
const KIND_OTHER             = Ci.nsIMemoryReporter.KIND_OTHER;

const UNITS_BYTES            = Ci.nsIMemoryReporter.UNITS_BYTES;
const UNITS_COUNT            = Ci.nsIMemoryReporter.UNITS_COUNT;
const UNITS_COUNT_CUMULATIVE = Ci.nsIMemoryReporter.UNITS_COUNT_CUMULATIVE;
const UNITS_PERCENTAGE       = Ci.nsIMemoryReporter.UNITS_PERCENTAGE;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
 "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
 "resource://gre/modules/FileUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "nsBinaryStream",
                            () => CC("@mozilla.org/binaryinputstream;1",
                                     "nsIBinaryInputStream",
                                     "setInputStream"));
XPCOMUtils.defineLazyGetter(this, "nsFile",
                            () => CC("@mozilla.org/file/local;1",
                                     "nsIFile", "initWithPath"));
XPCOMUtils.defineLazyGetter(this, "nsGzipConverter",
                            () => CC("@mozilla.org/streamconv;1?from=gzip&to=uncompressed",
                                     "nsIStreamConverter"));

var gMgr = Cc["@mozilla.org/memory-reporter-manager;1"]
             .getService(Ci.nsIMemoryReporterManager);

const gPageName = 'about:memory';
document.title = gPageName;

const gUnnamedProcessStr = "Main Process";

var gIsDiff = false;

// ---------------------------------------------------------------------------

// Forward slashes in URLs in paths are represented with backslashes to avoid
// being mistaken for path separators.  Paths/names where this hasn't been
// undone are prefixed with "unsafe"; the rest are prefixed with "safe".
function flipBackslashes(aUnsafeStr) {
  // Save memory by only doing the replacement if it's necessary.
  return (aUnsafeStr.indexOf('\\') === -1)
         ? aUnsafeStr
         : aUnsafeStr.replace(/\\/g, '/');
}

const gAssertionFailureMsgPrefix = "aboutMemory.js assertion failed: ";

// This is used for things that should never fail, and indicate a defect in
// this file if they do.
function assert(aCond, aMsg) {
  if (!aCond) {
    reportAssertionFailure(aMsg)
    throw new Error(gAssertionFailureMsgPrefix + aMsg);
  }
}

// This is used for malformed input from memory reporters.
function assertInput(aCond, aMsg) {
  if (!aCond) {
    throw new Error("Invalid memory report(s): " + aMsg);
  }
}

function handleException(ex) {
  let str = "" + ex;
  if (str.startsWith(gAssertionFailureMsgPrefix)) {
    // Argh, assertion failure within this file!  Give up.
    throw ex;
  } else {
    // File or memory reporter problem.  Print a message.
    updateMainAndFooter(str, HIDE_FOOTER, "badInputWarning");
  }
}

function reportAssertionFailure(aMsg) {
  let debug = Cc["@mozilla.org/xpcom/debug;1"].getService(Ci.nsIDebug2);
  if (debug.isDebugBuild) {
    debug.assertion(aMsg, "false", "aboutMemory.js", 0);
  }
}

function debug(x) {
  let section = appendElement(document.body, 'div', 'section');
  appendElementWithText(section, "div", "debug", JSON.stringify(x));
}

// ---------------------------------------------------------------------------

function onUnload() {
}

// ---------------------------------------------------------------------------

// The <div> holding everything but the header and footer (if they're present).
// It's what is updated each time the page changes.
var gMain;

// The <div> holding the footer.
var gFooter;

// The "verbose" checkbox.
var gVerbose;

// The "anonymize" checkbox.
var gAnonymize;

// Values for the |aFooterAction| argument to updateTitleMainAndFooter.
var HIDE_FOOTER = 0;
var SHOW_FOOTER = 1;

function updateTitleMainAndFooter(aTitleNote, aMsg, aFooterAction, aClassName) {
  document.title = gPageName;
  if (aTitleNote) {
    document.title += " (" + aTitleNote + ")";
  }

  // Clear gMain by replacing it with an empty node.
  let tmp = gMain.cloneNode(false);
  gMain.parentNode.replaceChild(tmp, gMain);
  gMain = tmp;

  gMain.classList.remove('hidden');
  gMain.classList.remove('verbose');
  gMain.classList.remove('non-verbose');
  if (gVerbose) {
    gMain.classList.add(gVerbose.checked ? 'verbose' : 'non-verbose');
  }

  let msgElement;
  if (aMsg) {
    let className = "section"
    if (aClassName) {
      className = className + " " + aClassName;
    }
    msgElement = appendElementWithText(gMain, 'div', className, aMsg);
  }

  switch (aFooterAction) {
   case HIDE_FOOTER: gFooter.classList.add('hidden'); break;
   case SHOW_FOOTER: gFooter.classList.remove('hidden'); break;
   default: assert(false, "bad footer action in updateTitleMainAndFooter");
  }
  return msgElement;
}

function updateMainAndFooter(aMsg, aFooterAction, aClassName) {
  return updateTitleMainAndFooter("", aMsg, aFooterAction, aClassName);
}

function appendTextNode(aP, aText) {
  let e = document.createTextNode(aText);
  aP.appendChild(e);
  return e;
}

function appendElement(aP, aTagName, aClassName) {
  let e = document.createElement(aTagName);
  if (aClassName) {
    e.className = aClassName;
  }
  aP.appendChild(e);
  return e;
}

function appendElementWithText(aP, aTagName, aClassName, aText) {
  let e = appendElement(aP, aTagName, aClassName);
  // Setting textContent clobbers existing children, but there are none.  More
  // importantly, it avoids creating a JS-land object for the node, saving
  // memory.
  e.textContent = aText;
  return e;
}

// ---------------------------------------------------------------------------

const explicitTreeDescription =
"This tree covers explicit memory allocations by the application.  It includes \
\n\n\
* allocations made at the operating system level (via calls to functions such as \
VirtualAlloc, vm_allocate, and mmap), \
\n\n\
* allocations made at the heap allocation level (via functions such as malloc, \
calloc, realloc, memalign, operator new, and operator new[]) that have not been \
explicitly decommitted (i.e. evicted from memory and swap), and \
\n\n\
* where possible, the overhead of the heap allocator itself.\
\n\n\
It excludes memory that is mapped implicitly such as code and data segments, \
and thread stacks. \
\n\n\
'explicit' is not guaranteed to cover every explicit allocation, but it does cover \
most (including the entire heap), and therefore it is the single best number to \
focus on when trying to reduce memory usage.";

// ---------------------------------------------------------------------------

function appendButton(aP, aTitle, aOnClick, aText, aId) {
  let b = appendElementWithText(aP, "button", "", aText);
  b.title = aTitle;
  b.onclick = aOnClick;
  if (aId) {
    b.id = aId;
  }
  return b;
}

function appendHiddenFileInput(aP, aId, aChangeListener) {
  let input = appendElementWithText(aP, "input", "hidden", "");
  input.type = "file";
  input.id = aId;      // used in testing
  input.addEventListener("change", aChangeListener);
  return input;
}

function onLoad() {
  // Generate the header.

  let header = appendElement(document.body, "div", "ancillary");

  // A hidden file input element that can be invoked when necessary.
  let fileInput1 = appendHiddenFileInput(header, "fileInput1", function() {
    let file = this.files[0];
    let filename = file.mozFullPath;
    updateAboutMemoryFromFile(filename);
  });

  // Ditto.
  let fileInput2 =
      appendHiddenFileInput(header, "fileInput2", function(e) {
    let file = this.files[0];
    // First time around, we stash a copy of the filename and reinvoke.  Second
    // time around we do the diff and display.
    if (!this.filename1) {
      this.filename1 = file.mozFullPath;

      // e.skipClick is only true when testing -- it allows fileInput2's
      // onchange handler to be re-called without having to go via the file
      // picker.
      if (!e.skipClick) {
        this.click();
      }
    } else {
      let filename1 = this.filename1;
      delete this.filename1;
      updateAboutMemoryFromTwoFiles(filename1, file.mozFullPath);
    }
  });

  const CuDesc = "Measure current memory reports and show.";
  const LdDesc = "Load memory reports from file and show.";
  const DfDesc = "Load memory report data from two files and show the " +
                 "difference.";

  const SvDesc = "Save memory reports to file.";

  const GCDesc = "Do a global garbage collection.";
  const CCDesc = "Do a cycle collection.";
  const MMDesc = "Send three \"heap-minimize\" notifications in a " +
                 "row.  Each notification triggers a global garbage " +
                 "collection followed by a cycle collection, and causes the " +
                 "process to reduce memory usage in other ways, e.g. by " +
                 "flushing various caches.";

  const GCAndCCLogDesc = "Save garbage collection log and concise cycle " +
                         "collection log.\n" +
                         "WARNING: These logs may be large (>1GB).";
  const GCAndCCAllLogDesc = "Save garbage collection log and verbose cycle " +
                            "collection log.\n" +
                            "WARNING: These logs may be large (>1GB).";

  const DMDEnabledDesc = "Analyze memory reports coverage and save the " +
                         "output to the temp directory.\n";
  const DMDDisabledDesc = "DMD is not running. Please re-start with $DMD and " +
                          "the other relevant environment variables set " +
                          "appropriately.";

  let ops = appendElement(header, "div", "");

  let row1 = appendElement(ops, "div", "opsRow");

  let labelDiv1 =
   appendElementWithText(row1, "div", "opsRowLabel", "Show memory reports");
  let label1 = appendElementWithText(labelDiv1, "label", "");
  gVerbose = appendElement(label1, "input", "");
  gVerbose.type = "checkbox";
  gVerbose.id = "verbose";   // used for testing
  appendTextNode(label1, "verbose");

  const kEllipsis = "\u2026";

  // The "measureButton" id is used for testing.
  appendButton(row1, CuDesc, doMeasure, "Measure", "measureButton");
  appendButton(row1, LdDesc, () => fileInput1.click(), "Load" + kEllipsis);
  appendButton(row1, DfDesc, () => fileInput2.click(),
               "Load and diff" + kEllipsis);

  let row2 = appendElement(ops, "div", "opsRow");

  let labelDiv2 =
    appendElementWithText(row2, "div", "opsRowLabel", "Save memory reports");
  appendButton(row2, SvDesc, saveReportsToFile, "Measure and save" + kEllipsis);

  // XXX: this isn't a great place for this checkbox, but I can't think of
  // anywhere better.
  let label2 = appendElementWithText(labelDiv2, "label", "");
  gAnonymize = appendElement(label2, "input", "");
  gAnonymize.type = "checkbox";
  appendTextNode(label2, "anonymize");

  let row3 = appendElement(ops, "div", "opsRow");

  appendElementWithText(row3, "div", "opsRowLabel", "Free memory");
  appendButton(row3, GCDesc, doGC, "GC");
  appendButton(row3, CCDesc, doCC, "CC");
  appendButton(row3, MMDesc, doMMU, "Minimize memory usage");

  let row4 = appendElement(ops, "div", "opsRow");

  appendElementWithText(row4, "div", "opsRowLabel", "Save GC & CC logs");
  appendButton(row4, GCAndCCLogDesc,
               saveGCLogAndConciseCCLog, "Save concise", 'saveLogsConcise');
  appendButton(row4, GCAndCCAllLogDesc,
               saveGCLogAndVerboseCCLog, "Save verbose", 'saveLogsVerbose');

  // Three cases here:
  // - DMD is disabled (i.e. not built): don't show the button.
  // - DMD is enabled but is not running: show the button, but disable it.
  // - DMD is enabled and is running: show the button and enable it.
  if (gMgr.isDMDEnabled) {
    let row5 = appendElement(ops, "div", "opsRow");

    appendElementWithText(row5, "div", "opsRowLabel", "Save DMD output");
    let enableButtons = gMgr.isDMDRunning;

    let dmdButton =
      appendButton(row5, enableButtons ? DMDEnabledDesc : DMDDisabledDesc,
                   doDMD, "Save");
    dmdButton.disabled = !enableButtons;
  }

  // Generate the main div, where content ("section" divs) will go.  It's
  // hidden at first.

  gMain = appendElement(document.body, 'div', '');
  gMain.id = 'mainDiv';

  // Generate the footer.  It's hidden at first.

  gFooter = appendElement(document.body, 'div', 'ancillary hidden');

  let a = appendElementWithText(gFooter, "a", "option",
                                "Troubleshooting information");
  a.href = "about:support";

  let legendText1 = "Click on a non-leaf node in a tree to expand ('++') " +
                    "or collapse ('--') its children.";
  let legendText2 = "Hover the pointer over the name of a memory report " +
                    "to see a description of what it measures.";

  appendElementWithText(gFooter, "div", "legend", legendText1);
  appendElementWithText(gFooter, "div", "legend hiddenOnMobile", legendText2);

  // See if we're loading from a file.  (Because about:memory is a non-standard
  // URL, location.search is undefined, so we have to use location.href
  // instead.)
  let search = location.href.split('?')[1];
  if (search) {
    let searchSplit = search.split('&');
    for (let i = 0; i < searchSplit.length; i++) {
      if (searchSplit[i].toLowerCase().startsWith('file=')) {
        let filename = searchSplit[i].substring('file='.length);
        updateAboutMemoryFromFile(decodeURIComponent(filename));
        return;
      }
    }
  }
}

// ---------------------------------------------------------------------------

function doGC() {
  Services.obs.notifyObservers(null, "child-gc-request", null);
  Cu.forceGC();
  updateMainAndFooter("Garbage collection completed", HIDE_FOOTER);
}

function doCC() {
  Services.obs.notifyObservers(null, "child-cc-request", null);
  window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils)
        .cycleCollect();
  updateMainAndFooter("Cycle collection completed", HIDE_FOOTER);
}

function doMMU() {
  Services.obs.notifyObservers(null, "child-mmu-request", null);
  gMgr.minimizeMemoryUsage(
    () => updateMainAndFooter("Memory minimization completed", HIDE_FOOTER));
}

function doMeasure() {
  updateAboutMemoryFromReporters();
}

function saveGCLogAndConciseCCLog() {
  dumpGCLogAndCCLog(false);
}

function saveGCLogAndVerboseCCLog() {
  dumpGCLogAndCCLog(true);
}

function doDMD() {
  updateMainAndFooter("Saving memory reports and DMD output...", HIDE_FOOTER);
  try {
    let dumper = Cc["@mozilla.org/memory-info-dumper;1"]
                   .getService(Ci.nsIMemoryInfoDumper);

    dumper.dumpMemoryInfoToTempDir(/* identifier = */ "",
                                   gAnonymize.checked,
                                   /* minimize = */ false);
    updateMainAndFooter("Saved memory reports and DMD reports analysis " +
                        "to the temp directory",
                        HIDE_FOOTER);
  } catch (ex) {
    updateMainAndFooter(ex.toString(), HIDE_FOOTER);
  }
}

function dumpGCLogAndCCLog(aVerbose) {
  let dumper = Cc["@mozilla.org/memory-info-dumper;1"]
                .getService(Ci.nsIMemoryInfoDumper);

  let inProgress = updateMainAndFooter("Saving logs...", HIDE_FOOTER);
  let section = appendElement(gMain, 'div', "section");

  function displayInfo(gcLog, ccLog, isParent) {
    appendElementWithText(section, 'div', "",
                          "Saved GC log to " + gcLog.path);

    let ccLogType = aVerbose ? "verbose" : "concise";
    appendElementWithText(section, 'div', "",
                          "Saved " + ccLogType + " CC log to " + ccLog.path);
  }

  dumper.dumpGCAndCCLogsToFile("", aVerbose, /* dumpChildProcesses = */ true,
                               { onDump: displayInfo,
                                 onFinish() {
                                   inProgress.remove();
                                 }
                               });
}

/**
 * Top-level function that does the work of generating the page from the memory
 * reporters.
 */
function updateAboutMemoryFromReporters() {
  updateMainAndFooter("Measuring...", HIDE_FOOTER);

  try {
    let processLiveMemoryReports =
        function(aHandleReport, aDisplayReports) {
      let handleReport = function(aProcess, aUnsafePath, aKind, aUnits,
                                  aAmount, aDescription) {
        aHandleReport(aProcess, aUnsafePath, aKind, aUnits, aAmount,
                      aDescription, /* presence = */ undefined);
      }

      let displayReportsAndFooter = function() {
        updateTitleMainAndFooter("live measurement", "", SHOW_FOOTER);
        aDisplayReports();
      }

      gMgr.getReports(handleReport, null, displayReportsAndFooter, null,
                      gAnonymize.checked);
    }

    // Process the reports from the live memory reporters.
    appendAboutMemoryMain(processLiveMemoryReports,
                          gMgr.hasMozMallocUsableSize);

  } catch (ex) {
    handleException(ex);
  }
}

// Increment this if the JSON format changes.
//
var gCurrentFileFormatVersion = 1;


/**
 * Parse a string as JSON and extract the |memory_report| property if it has
 * one, which indicates the string is from a crash dump.
 *
 * @param aStr
 *        The string.
 * @return The extracted object.
 */
function parseAndUnwrapIfCrashDump(aStr) {
  let obj = JSON.parse(aStr);
  if (obj.memory_report !== undefined) {
    // It looks like a crash dump. The memory reports should be in the
    // |memory_report| property.
    obj = obj.memory_report;
  }
  return obj;
}

/**
 * Populate about:memory using the data in the given JSON object.
 *
 * @param aObj
 *        An object that (hopefully!) conforms to the JSON schema used by
 *        nsIMemoryInfoDumper.
 */
function updateAboutMemoryFromJSONObject(aObj) {
  try {
    assertInput(aObj.version === gCurrentFileFormatVersion,
                "data version number missing or doesn't match");
    assertInput(aObj.hasMozMallocUsableSize !== undefined,
                "missing 'hasMozMallocUsableSize' property");
    assertInput(aObj.reports && aObj.reports instanceof Array,
                "missing or non-array 'reports' property");

    let processMemoryReportsFromFile =
        function(aHandleReport, aDisplayReports) {
      for (let i = 0; i < aObj.reports.length; i++) {
        let r = aObj.reports[i];

        // A hack: for a brief time (late in the FF26 and early in the FF27
        // cycle) we were dumping memory report files that contained reports
        // whose path began with "redundant/".  Such reports were ignored by
        // about:memory.  These reports are no longer produced, but some older
        // builds are still floating around and producing files that contain
        // them, so we need to still handle them (i.e. ignore them).  This hack
        // can be removed once FF26 and associated products (e.g. B2G 1.2) are
        // no longer in common use.
        if (!r.path.startsWith("redundant/")) {
          aHandleReport(r.process, r.path, r.kind, r.units, r.amount,
                        r.description, r._presence);
        }
      }
      aDisplayReports();
    }
    appendAboutMemoryMain(processMemoryReportsFromFile,
                          aObj.hasMozMallocUsableSize);
  } catch (ex) {
    handleException(ex);
  }
}

/**
 * Populate about:memory using the data in the given JSON string.
 *
 * @param aStr
 *        A string containing JSON data conforming to the schema used by
 *        nsIMemoryReporterManager::dumpReports.
 */
function updateAboutMemoryFromJSONString(aStr) {
  try {
    let obj = parseAndUnwrapIfCrashDump(aStr);
    updateAboutMemoryFromJSONObject(obj);
  } catch (ex) {
    handleException(ex);
  }
}

/**
 * Loads the contents of a file into a string and passes that to a callback.
 *
 * @param aFilename
 *        The name of the file being read from.
 * @param aTitleNote
 *        A description to put in the page title upon completion.
 * @param aFn
 *        The function to call and pass the read string to upon completion.
 */
function loadMemoryReportsFromFile(aFilename, aTitleNote, aFn) {
  updateMainAndFooter("Loading...", HIDE_FOOTER);

  try {
    let reader = new FileReader();
    reader.onerror = () => { throw new Error("FileReader.onerror"); };
    reader.onabort = () => { throw new Error("FileReader.onabort"); };
    reader.onload = (aEvent) => {
      // Clear "Loading..." from above.
      updateTitleMainAndFooter(aTitleNote, "", SHOW_FOOTER);
      aFn(aEvent.target.result);
    };

    // If it doesn't have a .gz suffix, read it as a (legacy) ungzipped file.
    if (!aFilename.endsWith(".gz")) {
      reader.readAsText(File.createFromFileName(aFilename));
      return;
    }

    // Read compressed gzip file.
    let converter = new nsGzipConverter();
    converter.asyncConvertData("gzip", "uncompressed", {
      data: [],
      onStartRequest(aR, aC) {},
      onDataAvailable(aR, aC, aStream, aO, aCount) {
        let bi = new nsBinaryStream(aStream);
        this.data.push(bi.readBytes(aCount));
      },
      onStopRequest(aR, aC, aStatusCode) {
        try {
          if (!Components.isSuccessCode(aStatusCode)) {
            throw new Components.Exception("Error while reading gzip file", aStatusCode);
          }
          reader.readAsText(new Blob(this.data));
        } catch (ex) {
          handleException(ex);
        }
      }
    }, null);

    let file = new nsFile(aFilename);
    let fileChan = NetUtil.newChannel({
                     uri: Services.io.newFileURI(file),
                     loadUsingSystemPrincipal: true
                   });
    fileChan.asyncOpen2(converter);

  } catch (ex) {
    handleException(ex);
  }
}

/**
 * Like updateAboutMemoryFromReporters(), but gets its data from a file instead
 * of the memory reporters.
 *
 * @param aFilename
 *        The name of the file being read from.  The expected format of the
 *        file's contents is described in a comment in nsIMemoryInfoDumper.idl.
 */
function updateAboutMemoryFromFile(aFilename) {
  loadMemoryReportsFromFile(aFilename, /* title note */ aFilename,
                            updateAboutMemoryFromJSONString);
}

/**
 * Like updateAboutMemoryFromFile(), but gets its data from a two files and
 * diffs them.
 *
 * @param aFilename1
 *        The name of the first file being read from.
 * @param aFilename2
 *        The name of the first file being read from.
 */
function updateAboutMemoryFromTwoFiles(aFilename1, aFilename2) {
  let titleNote = "diff of " + aFilename1 + " and " + aFilename2;
  loadMemoryReportsFromFile(aFilename1, titleNote, function(aStr1) {
    loadMemoryReportsFromFile(aFilename2, titleNote, function(aStr2) {
      try {
        let obj1 = parseAndUnwrapIfCrashDump(aStr1);
        let obj2 = parseAndUnwrapIfCrashDump(aStr2);
        gIsDiff = true;
        updateAboutMemoryFromJSONObject(diffJSONObjects(obj1, obj2));
        gIsDiff = false;
      } catch (ex) {
        handleException(ex);
      }
    });
  });
}

// ---------------------------------------------------------------------------

// Something unlikely to appear in a process name.
var kProcessPathSep = "^:^:^";

// Short for "diff report".
function DReport(aKind, aUnits, aAmount, aDescription, aNMerged, aPresence) {
  this._kind = aKind;
  this._units = aUnits;
  this._amount = aAmount;
  this._description = aDescription;
  this._nMerged = aNMerged;
  if (aPresence !== undefined) {
    this._presence = aPresence;
  }
}

DReport.prototype = {
  assertCompatible(aKind, aUnits) {
    assert(this._kind == aKind, "Mismatched kinds");
    assert(this._units == aUnits, "Mismatched units");

    // We don't check that the "description" properties match.  This is because
    // on Linux we can get cases where the paths are the same but the
    // descriptions differ, like this:
    //
    //   "path": "size/other-files/icon-theme.cache/[r--p]",
    //   "description": "/usr/share/icons/gnome/icon-theme.cache (read-only, not executable, private)"
    //
    //   "path": "size/other-files/icon-theme.cache/[r--p]"
    //   "description": "/usr/share/icons/hicolor/icon-theme.cache (read-only, not executable, private)"
    //
    // In those cases, we just use the description from the first-encountered
    // one, which is what about:memory also does.
    // (Note: reports with those paths are no longer generated, but allowing
    // the descriptions to differ seems reasonable.)
  },

  merge(aJr) {
    this.assertCompatible(aJr.kind, aJr.units);
    this._amount += aJr.amount;
    this._nMerged++;
  },

  toJSON(aProcess, aPath, aAmount) {
    return {
      process:     aProcess,
      path:        aPath,
      kind:        this._kind,
      units:       this._units,
      amount:      aAmount,
      description: this._description,
      _presence:   this._presence
    };
  }
};

// Constants that indicate if a DReport was present only in one of the data
// sets, or had to be added for balance.
DReport.PRESENT_IN_FIRST_ONLY  = 1;
DReport.PRESENT_IN_SECOND_ONLY = 2;
DReport.ADDED_FOR_BALANCE = 3;

/**
 * Make a report map, which has combined path+process strings for keys, and
 * DReport objects for values.
 *
 * @param aJSONReports
 *        The |reports| field of a JSON object.
 * @return The constructed report map.
 */
function makeDReportMap(aJSONReports) {
  let dreportMap = {};
  for (let i = 0; i < aJSONReports.length; i++) {
    let jr = aJSONReports[i];

    assert(jr.process !== undefined, "Missing process");
    assert(jr.path !== undefined, "Missing path");
    assert(jr.kind !== undefined, "Missing kind");
    assert(jr.units !== undefined, "Missing units");
    assert(jr.amount !== undefined, "Missing amount");
    assert(jr.description !== undefined, "Missing description");

    // Strip out some non-deterministic stuff that prevents clean diffs.
    // Ideally the memory reports themselves would contain information about
    // which parts of the the process and path need to be stripped -- saving us
    // from hardwiring knowledge of specific reporters here -- but we have no
    // mechanism for that. (Any future redesign of how memory reporters work
    // should include such a mechanism.)

    // Strip PIDs:
    // - pid 123
    // - pid=123
    let pidRegex = /pid([ =])\d+/g;
    let pidSubst = "pid$1NNN";
    let process = jr.process.replace(pidRegex, pidSubst);
    let path = jr.path.replace(pidRegex, pidSubst);

    // Strip addresses:
    // - .../js-zone(0x12345678)/...
    // - .../zone(0x12345678)/...
    // - .../worker(<URL>, 0x12345678)/...
    path = path.replace(/zone\(0x[0-9A-Fa-f]+\)\//, "zone(0xNNN)/");
    path = path.replace(/\/worker\((.+), 0x[0-9A-Fa-f]+\)\//,
                        "/worker($1, 0xNNN)/");

    // Strip top window IDs:
    // - explicit/window-objects/top(<URL>, id=123)/...
    path = path.replace(/^(explicit\/window-objects\/top\(.*, id=)\d+\)/,
                        "$1NNN)");

    // Strip null principal UUIDs (but not other UUIDs, because they may be
    // deterministic, such as those used by add-ons).
    path = path.replace(
      /moz-nullprincipal:{........-....-....-....-............}/g,
      "moz-nullprincipal:{NNNNNNNN-NNNN-NNNN-NNNN-NNNNNNNNNNNN}");

    // Normalize omni.ja! paths.
    path = path.replace(/jar:file:\\\\\\(.+)\\omni.ja!/,
                        "jar:file:\\\\\\...\\omni.ja!");

    let processPath = process + kProcessPathSep + path;
    let rOld = dreportMap[processPath];
    if (rOld === undefined) {
      dreportMap[processPath] =
        new DReport(jr.kind, jr.units, jr.amount, jr.description, 1, undefined);
    } else {
      rOld.merge(jr);
    }
  }
  return dreportMap;
}

// Return a new dreportMap which is the diff of two dreportMaps.  Empties
// aDReportMap2 along the way.
function diffDReportMaps(aDReportMap1, aDReportMap2) {
  let result = {};

  for (let processPath in aDReportMap1) {
    let r1 = aDReportMap1[processPath];
    let r2 = aDReportMap2[processPath];
    let r2_amount, r2_nMerged;
    let presence;
    if (r2 !== undefined) {
      r1.assertCompatible(r2._kind, r2._units);
      r2_amount = r2._amount;
      r2_nMerged = r2._nMerged;
      delete aDReportMap2[processPath];
      presence = undefined;   // represents that it's present in both
    } else {
      r2_amount = 0;
      r2_nMerged = 0;
      presence = DReport.PRESENT_IN_FIRST_ONLY;
    }
    result[processPath] =
      new DReport(r1._kind, r1._units, r2_amount - r1._amount, r1._description,
                  Math.max(r1._nMerged, r2_nMerged), presence);
  }

  for (let processPath in aDReportMap2) {
    let r2 = aDReportMap2[processPath];
    result[processPath] = new DReport(r2._kind, r2._units, r2._amount,
                                      r2._description, r2._nMerged,
                                      DReport.PRESENT_IN_SECOND_ONLY);
  }

  return result;
}

function makeJSONReports(aDReportMap) {
  let reports = [];
  for (let processPath in aDReportMap) {
    let r = aDReportMap[processPath];
    if (r._amount !== 0) {
      // If _nMerged > 1, we give the full (aggregated) amount in the first
      // copy, and then use amount=0 in the remainder.  When viewed in
      // about:memory, this shows up as an entry with a "[2]"-style suffix
      // and the correct amount.
      let split = processPath.split(kProcessPathSep);
      assert(split.length >= 2);
      let process = split.shift();
      let path = split.join();
      reports.push(r.toJSON(process, path, r._amount));
      for (let i = 1; i < r._nMerged; i++) {
        reports.push(r.toJSON(process, path, 0));
      }
    }
  }

  return reports;
}

// Diff two JSON objects holding memory reports.
function diffJSONObjects(aJson1, aJson2) {
  function simpleProp(aProp) {
    assert(aJson1[aProp] !== undefined && aJson1[aProp] === aJson2[aProp],
           aProp + " properties don't match");
    return aJson1[aProp];
  }

  return {
    version: simpleProp("version"),

    hasMozMallocUsableSize: simpleProp("hasMozMallocUsableSize"),

    reports: makeJSONReports(diffDReportMaps(makeDReportMap(aJson1.reports),
                                             makeDReportMap(aJson2.reports)))
  };
}

// ---------------------------------------------------------------------------

// |PColl| is short for "process collection".
function PColl() {
  this._trees = {};
  this._degenerates = {};
  this._heapTotal = 0;
}

/**
 * Processes reports (whether from reporters or from a file) and append the
 * main part of the page.
 *
 * @param aProcessReports
 *        Function that extracts the memory reports from the reporters or from
 *        file.
 * @param aHasMozMallocUsableSize
 *        Boolean indicating if moz_malloc_usable_size works.
 */
function appendAboutMemoryMain(aProcessReports, aHasMozMallocUsableSize) {
  let pcollsByProcess = {};

  function handleReport(aProcess, aUnsafePath, aKind, aUnits, aAmount,
                        aDescription, aPresence) {
    if (aUnsafePath.startsWith("explicit/")) {
      assertInput(aKind === KIND_HEAP || aKind === KIND_NONHEAP,
                  "bad explicit kind");
      assertInput(aUnits === UNITS_BYTES, "bad explicit units");
    }

    assert(aPresence === undefined ||
           aPresence == DReport.PRESENT_IN_FIRST_ONLY ||
           aPresence == DReport.PRESENT_IN_SECOND_ONLY,
           "bad presence");

    let process = aProcess === "" ? gUnnamedProcessStr : aProcess;
    let unsafeNames = aUnsafePath.split('/');
    let unsafeName0 = unsafeNames[0];
    let isDegenerate = unsafeNames.length === 1;

    // Get the PColl table for the process, creating it if necessary.
    let pcoll = pcollsByProcess[process];
    if (!pcollsByProcess[process]) {
      pcoll = pcollsByProcess[process] = new PColl();
    }

    // Get the root node, creating it if necessary.
    let psubcoll = isDegenerate ? pcoll._degenerates : pcoll._trees;
    let t = psubcoll[unsafeName0];
    if (!t) {
      t = psubcoll[unsafeName0] =
        new TreeNode(unsafeName0, aUnits, isDegenerate);
    }

    if (!isDegenerate) {
      // Add any missing nodes in the tree implied by aUnsafePath, and fill in
      // the properties that we can with a top-down traversal.
      for (let i = 1; i < unsafeNames.length; i++) {
        let unsafeName = unsafeNames[i];
        let u = t.findKid(unsafeName);
        if (!u) {
          u = new TreeNode(unsafeName, aUnits, isDegenerate);
          if (!t._kids) {
            t._kids = [];
          }
          t._kids.push(u);
        }
        t = u;
      }

      // Update the heap total if necessary.
      if (unsafeName0 === "explicit" && aKind == KIND_HEAP) {
        pcollsByProcess[process]._heapTotal += aAmount;
      }
    }

    if (t._amount) {
      // Duplicate!  Sum the values and mark it as a dup.
      t._amount += aAmount;
      t._nMerged = t._nMerged ? t._nMerged + 1 : 2;
      assert(t._presence === aPresence, "presence mismatch");
    } else {
      // New leaf node.  Fill in extra node details from the report.
      t._amount = aAmount;
      t._description = aDescription;
      if (aPresence !== undefined) {
        t._presence = aPresence;
      }
    }
  }

  function displayReports() {
    // Sort the processes.
    let processes = Object.keys(pcollsByProcess);
    processes.sort(function(aProcessA, aProcessB) {
      assert(aProcessA != aProcessB,
             "Elements of Object.keys() should be unique, but " +
             "saw duplicate '" + aProcessA + "' elem.");

      // Always put the main process first.
      if (aProcessA == gUnnamedProcessStr) {
        return -1;
      }
      if (aProcessB == gUnnamedProcessStr) {
        return 1;
      }

      // Then sort by resident size.
      let nodeA = pcollsByProcess[aProcessA]._degenerates['resident'];
      let nodeB = pcollsByProcess[aProcessB]._degenerates['resident'];
      let residentA = nodeA ? nodeA._amount : -1;
      let residentB = nodeB ? nodeB._amount : -1;

      if (residentA > residentB) {
        return -1;
      }
      if (residentA < residentB) {
        return 1;
      }

      // Then sort by process name.
      if (aProcessA < aProcessB) {
        return -1;
      }
      if (aProcessA > aProcessB) {
        return 1;
      }

      return 0;
    });

    // Generate output for each process.
    for (let i = 0; i < processes.length; i++) {
      let process = processes[i];
      let section = appendElement(gMain, 'div', 'section');

      appendProcessAboutMemoryElements(section, i, process,
                                       pcollsByProcess[process]._trees,
                                       pcollsByProcess[process]._degenerates,
                                       pcollsByProcess[process]._heapTotal,
                                       aHasMozMallocUsableSize);
    }
  }

  aProcessReports(handleReport, displayReports);
}

// ---------------------------------------------------------------------------

// There are two kinds of TreeNode.
// - Leaf TreeNodes correspond to reports.
// - Non-leaf TreeNodes are just scaffolding nodes for the tree;  their values
//   are derived from their children.
// Some trees are "degenerate", i.e. they contain a single node, i.e. they
// correspond to a report whose path has no '/' separators.
function TreeNode(aUnsafeName, aUnits, aIsDegenerate) {
  this._units = aUnits;
  this._unsafeName = aUnsafeName;
  if (aIsDegenerate) {
    this._isDegenerate = true;
  }

  // Leaf TreeNodes have these properties added immediately after construction:
  // - _amount
  // - _description
  // - _nMerged (only defined if > 1)
  // - _presence (only defined if value is PRESENT_IN_{FIRST,SECOND}_ONLY)
  //
  // Non-leaf TreeNodes have these properties added later:
  // - _kids
  // - _amount
  // - _description
  // - _hideKids (only defined if true)
  // - _maxAbsDescendant (on-demand, only when gIsDiff is set)
}

TreeNode.prototype = {
  findKid(aUnsafeName) {
    if (this._kids) {
      for (let i = 0; i < this._kids.length; i++) {
        if (this._kids[i]._unsafeName === aUnsafeName) {
          return this._kids[i];
        }
      }
    }
    return undefined;
  },

  // When gIsDiff is false, tree operations -- sorting and determining if a
  // sub-tree is significant -- are straightforward. But when gIsDiff is true,
  // the combination of positive and negative values within a tree complicates
  // things. So for a non-leaf node, instead of just looking at _amount, we
  // instead look at the maximum absolute value of the node and all of its
  // descendants.
  maxAbsDescendant() {
    if (!this._kids) {
      // No kids? Just return the absolute value of the amount.
      return Math.abs(this._amount);
    }

    if ('_maxAbsDescendant' in this) {
      // We've computed this before? Return the saved value.
      return this._maxAbsDescendant;
    }

    // Compute the maximum absolute value of all descendants.
    let max = Math.abs(this._amount);
    for (let i = 0; i < this._kids.length; i++) {
      max = Math.max(max, this._kids[i].maxAbsDescendant());
    }
    this._maxAbsDescendant = max;
    return max;
  },

  toString() {
    switch (this._units) {
      case UNITS_BYTES: return formatBytes(this._amount);
      case UNITS_COUNT:
      case UNITS_COUNT_CUMULATIVE: return formatInt(this._amount);
      case UNITS_PERCENTAGE: return formatPercentage(this._amount);
      default:
        throw "Invalid memory report(s): bad units in TreeNode.toString";
    }
  }
};

// Sort TreeNodes first by size, then by name.  The latter is important for the
// about:memory tests, which need a predictable ordering of reporters which
// have the same amount.
TreeNode.compareAmounts = function(aA, aB) {
  let a, b;
  if (gIsDiff) {
    a = aA.maxAbsDescendant();
    b = aB.maxAbsDescendant();
  } else {
    a = aA._amount;
    b = aB._amount;
  }
  if (a > b) {
    return -1;
  }
  if (a < b) {
    return 1;
  }
  return TreeNode.compareUnsafeNames(aA, aB);
};

TreeNode.compareUnsafeNames = function(aA, aB) {
  return aA._unsafeName.localeCompare(aB._unsafeName);
};


/**
 * Fill in the remaining properties for the specified tree in a bottom-up
 * fashion.
 *
 * @param aRoot
 *        The tree root.
 */
function fillInTree(aRoot) {
  // Fill in the remaining properties bottom-up.
  function fillInNonLeafNodes(aT) {
    if (!aT._kids) {
      // Leaf node.  Has already been filled in.

    } else if (aT._kids.length === 1 && aT != aRoot) {
      // Non-root, non-leaf node with one child.  Merge the child with the node
      // to avoid redundant entries.
      let kid = aT._kids[0];
      let kidBytes = fillInNonLeafNodes(kid);
      aT._unsafeName += '/' + kid._unsafeName;
      if (kid._kids) {
        aT._kids = kid._kids;
      } else {
        delete aT._kids;
      }
      aT._amount = kidBytes;
      aT._description = kid._description;
      if (kid._nMerged !== undefined) {
        aT._nMerged = kid._nMerged
      }
      assert(!aT._hideKids && !kid._hideKids, "_hideKids set when merging");

    } else {
      // Non-leaf node with multiple children.  Derive its _amount and
      // _description entirely from its children...
      let kidsBytes = 0;
      for (let i = 0; i < aT._kids.length; i++) {
        kidsBytes += fillInNonLeafNodes(aT._kids[i]);
      }

      // ... except in one special case. When diffing two memory report sets,
      // if one set has a node with children and the other has the same node
      // but without children -- e.g. the first has "a/b/c" and "a/b/d", but
      // the second only has "a/b" -- we need to add a fake node "a/b/(fake)"
      // to the second to make the trees comparable. It's ugly, but it works.
      if (aT._amount !== undefined &&
          (aT._presence === DReport.PRESENT_IN_FIRST_ONLY ||
           aT._presence === DReport.PRESENT_IN_SECOND_ONLY)) {
        aT._amount += kidsBytes;
        let fake = new TreeNode('(fake child)', aT._units);
        fake._presence = DReport.ADDED_FOR_BALANCE;
        fake._amount = aT._amount - kidsBytes;
        aT._kids.push(fake);
        delete aT._presence;
      } else {
        assert(aT._amount === undefined,
               "_amount already set for non-leaf node")
        aT._amount = kidsBytes;
      }
      aT._description = "The sum of all entries below this one.";
    }
    return aT._amount;
  }

  // cannotMerge is set because don't want to merge into a tree's root node.
  fillInNonLeafNodes(aRoot);
}

/**
 * Compute the "heap-unclassified" value and insert it into the "explicit"
 * tree.
 *
 * @param aT
 *        The "explicit" tree.
 * @param aHeapAllocatedNode
 *        The "heap-allocated" tree node.
 * @param aHeapTotal
 *        The sum of all explicit HEAP reports for this process.
 * @return A boolean indicating if "heap-allocated" is known for the process.
 */
function addHeapUnclassifiedNode(aT, aHeapAllocatedNode, aHeapTotal) {
  if (aHeapAllocatedNode === undefined)
    return false;

  if (aT.findKid("heap-unclassified")) {
    // heap-unclassified was already calculated, there's nothing left to do.
    // This can happen when memory reports are exported from areweslimyet.com.
    return true;
  }

  assert(aHeapAllocatedNode._isDegenerate, "heap-allocated is not degenerate");
  let heapAllocatedBytes = aHeapAllocatedNode._amount;
  let heapUnclassifiedT = new TreeNode("heap-unclassified", UNITS_BYTES);
  heapUnclassifiedT._amount = heapAllocatedBytes - aHeapTotal;
  heapUnclassifiedT._description =
      "Memory not classified by a more specific report. This includes " +
      "slop bytes due to internal fragmentation in the heap allocator " +
      "(caused when the allocator rounds up request sizes).";
  aT._kids.push(heapUnclassifiedT);
  aT._amount += heapUnclassifiedT._amount;
  return true;
}

/**
 * Sort all kid nodes from largest to smallest, and insert aggregate nodes
 * where appropriate.
 *
 * @param aTotalBytes
 *        The size of the tree's root node.
 * @param aT
 *        The tree.
 */
function sortTreeAndInsertAggregateNodes(aTotalBytes, aT) {
  const kSignificanceThresholdPerc = 1;

  function isInsignificant(aT) {
    if (gVerbose.checked)
      return false;

    let perc = gIsDiff
             ? 100 * aT.maxAbsDescendant() / Math.abs(aTotalBytes)
             : 100 * aT._amount / aTotalBytes;
    return perc < kSignificanceThresholdPerc;
  }

  if (!aT._kids) {
    return;
  }

  aT._kids.sort(TreeNode.compareAmounts);

  // If the first child is insignificant, they all are, and there's no point
  // creating an aggregate node that lacks siblings.  Just set the parent's
  // _hideKids property and process all children.
  if (isInsignificant(aT._kids[0])) {
    aT._hideKids = true;
    for (let i = 0; i < aT._kids.length; i++) {
      sortTreeAndInsertAggregateNodes(aTotalBytes, aT._kids[i]);
    }
    return;
  }

  // Look at all children except the last one.
  let i;
  for (i = 0; i < aT._kids.length - 1; i++) {
    if (isInsignificant(aT._kids[i])) {
      // This child is below the significance threshold.  If there are other
      // (smaller) children remaining, move them under an aggregate node.
      let i0 = i;
      let nAgg = aT._kids.length - i0;
      // Create an aggregate node.  Inherit units from the parent;  everything
      // in the tree should have the same units anyway (we test this later).
      let aggT = new TreeNode("(" + nAgg + " tiny)", aT._units);
      aggT._kids = [];
      let aggBytes = 0;
      for ( ; i < aT._kids.length; i++) {
        aggBytes += aT._kids[i]._amount;
        aggT._kids.push(aT._kids[i]);
      }
      aggT._hideKids = true;
      aggT._amount = aggBytes;
      aggT._description =
        nAgg + " sub-trees that are below the " + kSignificanceThresholdPerc +
        "% significance threshold.";
      aT._kids.splice(i0, nAgg, aggT);
      aT._kids.sort(TreeNode.compareAmounts);

      // Process the moved children.
      for (i = 0; i < aggT._kids.length; i++) {
        sortTreeAndInsertAggregateNodes(aTotalBytes, aggT._kids[i]);
      }
      return;
    }

    sortTreeAndInsertAggregateNodes(aTotalBytes, aT._kids[i]);
  }

  // The first n-1 children were significant.  Don't consider if the last child
  // is significant;  there's no point creating an aggregate node that only has
  // one child.  Just process it.
  sortTreeAndInsertAggregateNodes(aTotalBytes, aT._kids[i]);
}

// Global variable indicating if we've seen any invalid values for this
// process;  it holds the unsafePaths of any such reports.  It is reset for
// each new process.
var gUnsafePathsWithInvalidValuesForThisProcess = [];

function appendWarningElements(aP, aHasKnownHeapAllocated,
                               aHasMozMallocUsableSize) {
  if (!aHasKnownHeapAllocated && !aHasMozMallocUsableSize) {
    appendElementWithText(aP, "p", "",
      "WARNING: the 'heap-allocated' memory reporter and the " +
      "moz_malloc_usable_size() function do not work for this platform " +
      "and/or configuration.  This means that 'heap-unclassified' is not " +
      "shown and the 'explicit' tree shows much less memory than it should.\n\n");

  } else if (!aHasKnownHeapAllocated) {
    appendElementWithText(aP, "p", "",
      "WARNING: the 'heap-allocated' memory reporter does not work for this " +
      "platform and/or configuration. This means that 'heap-unclassified' " +
      "is not shown and the 'explicit' tree shows less memory than it should.\n\n");

  } else if (!aHasMozMallocUsableSize) {
    appendElementWithText(aP, "p", "",
      "WARNING: the moz_malloc_usable_size() function does not work for " +
      "this platform and/or configuration.  This means that much of the " +
      "heap-allocated memory is not measured by individual memory reporters " +
      "and so will fall under 'heap-unclassified'.\n\n");
  }

  if (gUnsafePathsWithInvalidValuesForThisProcess.length > 0) {
    let div = appendElement(aP, "div");
    appendElementWithText(div, "p", "",
      "WARNING: the following values are negative or unreasonably large.\n");

    let ul = appendElement(div, "ul");
    for (let i = 0;
         i < gUnsafePathsWithInvalidValuesForThisProcess.length;
         i++) {
      appendTextNode(ul, " ");
      appendElementWithText(ul, "li", "",
        flipBackslashes(gUnsafePathsWithInvalidValuesForThisProcess[i]) + "\n");
    }

    appendElementWithText(div, "p", "",
      "This indicates a defect in one or more memory reporters.  The " +
      "invalid values are highlighted.\n\n");
    gUnsafePathsWithInvalidValuesForThisProcess = [];  // reset for the next process
  }
}

/**
 * Appends the about:memory elements for a single process.
 *
 * @param aP
 *        The parent DOM node.
 * @param aN
 *        The number of the process, starting at 0.
 * @param aProcess
 *        The name of the process.
 * @param aTrees
 *        The table of non-degenerate trees for this process.
 * @param aDegenerates
 *        The table of degenerate trees for this process.
 * @param aHasMozMallocUsableSize
 *        Boolean indicating if moz_malloc_usable_size works.
 * @return The generated text.
 */
function appendProcessAboutMemoryElements(aP, aN, aProcess, aTrees,
                                          aDegenerates, aHeapTotal,
                                          aHasMozMallocUsableSize) {
  const kUpwardsArrow   = "\u2191",
        kDownwardsArrow = "\u2193";

  let appendLink = function(aHere, aThere, aArrow) {
    let link = appendElementWithText(aP, "a", "upDownArrow", aArrow);
    link.href = "#" + aThere + aN;
    link.id = aHere + aN;
    link.title = "Go to the " + aThere + " of " + aProcess;
    link.style = "text-decoration: none";

    // This jumps to the anchor without the page location getting the anchor
    // name tacked onto its end, which is what happens with a vanilla link.
    link.addEventListener("click", function(event) {
      document.documentElement.scrollTop =
        document.querySelector(event.target.href).offsetTop;
      event.preventDefault();
    }, false);

    // This gives nice spacing when we copy and paste.
    appendElementWithText(aP, "span", "", "\n");
  }

  appendElementWithText(aP, "h1", "", aProcess);
  appendLink("start", "end", kDownwardsArrow);

  // We'll fill this in later.
  let warningsDiv = appendElement(aP, "div", "accuracyWarning");

  // The explicit tree.
  let hasExplicitTree;
  let hasKnownHeapAllocated;
  {
    let treeName = "explicit";
    let t = aTrees[treeName];
    if (t) {
      let pre = appendSectionHeader(aP, "Explicit Allocations");
      hasExplicitTree = true;
      fillInTree(t);
      // Using the "heap-allocated" reporter here instead of
      // nsMemoryReporterManager.heapAllocated goes against the usual pattern.
      // But the "heap-allocated" node will go in the tree like the others, so
      // we have to deal with it, and once we're dealing with it, it's easier
      // to keep doing so rather than switching to the distinguished amount.
      hasKnownHeapAllocated =
        aDegenerates &&
        addHeapUnclassifiedNode(t, aDegenerates["heap-allocated"], aHeapTotal);
      sortTreeAndInsertAggregateNodes(t._amount, t);
      t._description = explicitTreeDescription;
      appendTreeElements(pre, t, aProcess, "");
      delete aTrees[treeName];
    }
    appendTextNode(aP, "\n");  // gives nice spacing when we copy and paste
  }

  // Fill in and sort all the non-degenerate other trees.
  let otherTrees = [];
  for (let unsafeName in aTrees) {
    let t = aTrees[unsafeName];
    assert(!t._isDegenerate, "tree is degenerate");
    fillInTree(t);
    sortTreeAndInsertAggregateNodes(t._amount, t);
    otherTrees.push(t);
  }
  otherTrees.sort(TreeNode.compareUnsafeNames);

  // Get the length of the longest root value among the degenerate other trees,
  // and sort them as well.
  let otherDegenerates = [];
  let maxStringLength = 0;
  for (let unsafeName in aDegenerates) {
    let t = aDegenerates[unsafeName];
    assert(t._isDegenerate, "tree is not degenerate");
    let length = t.toString().length;
    if (length > maxStringLength) {
      maxStringLength = length;
    }
    otherDegenerates.push(t);
  }
  otherDegenerates.sort(TreeNode.compareUnsafeNames);

  // Now generate the elements, putting non-degenerate trees first.
  let pre = appendSectionHeader(aP, "Other Measurements");
  for (let i = 0; i < otherTrees.length; i++) {
    let t = otherTrees[i];
    appendTreeElements(pre, t, aProcess, "");
    appendTextNode(pre, "\n");  // blank lines after non-degenerate trees
  }
  for (let i = 0; i < otherDegenerates.length; i++) {
    let t = otherDegenerates[i];
    let padText = pad("", maxStringLength - t.toString().length, ' ');
    appendTreeElements(pre, t, aProcess, padText);
  }
  appendTextNode(aP, "\n");  // gives nice spacing when we copy and paste

  // Add any warnings about inaccuracies in the "explicit" tree due to platform
  // limitations.  These must be computed after generating all the text.  The
  // newlines give nice spacing if we copy+paste into a text buffer.
  if (hasExplicitTree) {
    appendWarningElements(warningsDiv, hasKnownHeapAllocated,
                          aHasMozMallocUsableSize);
  }

  appendElementWithText(aP, "h3", "", "End of " + aProcess);
  appendLink("end", "start", kUpwardsArrow);
}

/**
 * Determines if a number has a negative sign when converted to a string.
 * Works even for -0.
 *
 * @param aN
 *        The number.
 * @return A boolean.
 */
function hasNegativeSign(aN) {
  if (aN === 0) {                   // this succeeds for 0 and -0
    return 1 / aN === -Infinity;    // this succeeds for -0
  }
  return aN < 0;
}

/**
 * Formats an int as a human-readable string.
 *
 * @param aN
 *        The integer to format.
 * @param aExtra
 *        An extra string to tack onto the end.
 * @return A human-readable string representing the int.
 *
 * Note: building an array of chars and converting that to a string with
 * Array.join at the end is more memory efficient than using string
 * concatenation.  See bug 722972 for details.
 */
function formatInt(aN, aExtra) {
  let neg = false;
  if (hasNegativeSign(aN)) {
    neg = true;
    aN = -aN;
  }
  let s = [];
  while (true) {
    let k = aN % 1000;
    aN = Math.floor(aN / 1000);
    if (aN > 0) {
      if (k < 10) {
        s.unshift(",00", k);
      } else if (k < 100) {
        s.unshift(",0", k);
      } else {
        s.unshift(",", k);
      }
    } else {
      s.unshift(k);
      break;
    }
  }
  if (neg) {
    s.unshift("-");
  }
  if (aExtra) {
    s.push(aExtra);
  }
  return s.join("");
}

/**
 * Converts a byte count to an appropriate string representation.
 *
 * @param aBytes
 *        The byte count.
 * @return The string representation.
 */
function formatBytes(aBytes) {
  let unit = gVerbose.checked ? " B" : " MB";

  let s;
  if (gVerbose.checked) {
    s = formatInt(aBytes, unit);
  } else {
    let mbytes = (aBytes / (1024 * 1024)).toFixed(2);
    let a = String(mbytes).split(".");
    // If the argument to formatInt() is -0, it will print the negative sign.
    s = formatInt(Number(a[0])) + "." + a[1] + unit;
  }
  return s;
}

/**
 * Converts a percentage to an appropriate string representation.
 *
 * @param aPerc100x
 *        The percentage, multiplied by 100 (see nsIMemoryReporter).
 * @return The string representation
 */
function formatPercentage(aPerc100x) {
  return (aPerc100x / 100).toFixed(2) + "%";
}

/**
 * Right-justifies a string in a field of a given width, padding as necessary.
 *
 * @param aS
 *        The string.
 * @param aN
 *        The field width.
 * @param aC
 *        The char used to pad.
 * @return The string representation.
 */
function pad(aS, aN, aC) {
  let padding = "";
  let n2 = aN - aS.length;
  for (let i = 0; i < n2; i++) {
    padding += aC;
  }
  return padding + aS;
}

// There's a subset of the Unicode "light" box-drawing chars that is widely
// implemented in terminals, and this code sticks to that subset to maximize
// the chance that copying and pasting about:memory output to a terminal will
// work correctly.
const kHorizontal                   = "\u2500",
      kVertical                     = "\u2502",
      kUpAndRight                   = "\u2514",
      kUpAndRight_Right_Right       = "\u2514\u2500\u2500",
      kVerticalAndRight             = "\u251c",
      kVerticalAndRight_Right_Right = "\u251c\u2500\u2500",
      kVertical_Space_Space         = "\u2502  ";

const kNoKidsSep                    = " \u2500\u2500 ",
      kHideKidsSep                  = " ++ ",
      kShowKidsSep                  = " -- ";

function appendMrNameSpan(aP, aDescription, aUnsafeName, aIsInvalid, aNMerged,
                          aPresence) {
  let safeName = flipBackslashes(aUnsafeName);
  if (!aIsInvalid && !aNMerged && !aPresence) {
    safeName += "\n";
  }
  let nameSpan = appendElementWithText(aP, "span", "mrName", safeName);
  nameSpan.title = aDescription;

  if (aIsInvalid) {
    let noteText = " [?!]";
    if (!aNMerged) {
      noteText += "\n";
    }
    let noteSpan = appendElementWithText(aP, "span", "mrNote", noteText);
    noteSpan.title =
      "Warning: this value is invalid and indicates a bug in one or more " +
      "memory reporters. ";
  }

  if (aNMerged) {
    let noteText = " [" + aNMerged + "]";
    if (!aPresence) {
      noteText += "\n";
    }
    let noteSpan = appendElementWithText(aP, "span", "mrNote", noteText);
    noteSpan.title =
      "This value is the sum of " + aNMerged +
      " memory reports that all have the same path.";
  }

  if (aPresence) {
    let c, title;
    switch (aPresence) {
     case DReport.PRESENT_IN_FIRST_ONLY:
      c = '-';
      title = "This value was only present in the first set of memory reports.";
      break;
     case DReport.PRESENT_IN_SECOND_ONLY:
      c = '+';
      title = "This value was only present in the second set of memory reports.";
      break;
     case DReport.ADDED_FOR_BALANCE:
      c = '!';
      title = "One of the sets of memory reports lacked children for this " +
              "node's parent. This is a fake child node added to make the " +
              "two memory sets comparable.";
      break;
     default: assert(false, "bad presence");
      break;
    }
    let noteSpan = appendElementWithText(aP, "span", "mrNote",
                                         " [" + c + "]\n");
    noteSpan.title = title;
  }
}

// This is used to record the (safe) IDs of which sub-trees have been manually
// expanded (marked as true) and collapsed (marked as false).  It's used to
// replicate the collapsed/expanded state when the page is updated.  It can end
// up holding IDs of nodes that no longer exist, e.g. for compartments that
// have been closed.  This doesn't seem like a big deal, because the number is
// limited by the number of entries the user has changed from their original
// state.
var gShowSubtreesBySafeTreeId = {};

function assertClassListContains(e, className) {
  assert(e, "undefined " + className);
  assert(e.classList.contains(className), "classname isn't " + className);
}

function toggle(aEvent) {
  // This relies on each line being a span that contains at least four spans:
  // mrValue, mrPerc, mrSep, mrName, and then zero or more mrNotes.  All
  // whitespace must be within one of these spans for this function to find the
  // right nodes.  And the span containing the children of this line must
  // immediately follow.  Assertions check this.

  // |aEvent.target| will be one of the spans.  Get the outer span.
  let outerSpan = aEvent.target.parentNode;
  assertClassListContains(outerSpan, "hasKids");

  // Toggle the '++'/'--' separator.
  let isExpansion;
  let sepSpan = outerSpan.childNodes[2];
  assertClassListContains(sepSpan, "mrSep");
  if (sepSpan.textContent === kHideKidsSep) {
    isExpansion = true;
    sepSpan.textContent = kShowKidsSep;
  } else if (sepSpan.textContent === kShowKidsSep) {
    isExpansion = false;
    sepSpan.textContent = kHideKidsSep;
  } else {
    assert(false, "bad sepSpan textContent");
  }

  // Toggle visibility of the span containing this node's children.
  let subTreeSpan = outerSpan.nextSibling;
  assertClassListContains(subTreeSpan, "kids");
  subTreeSpan.classList.toggle("hidden");

  // Record/unrecord that this sub-tree was toggled.
  let safeTreeId = outerSpan.id;
  if (gShowSubtreesBySafeTreeId[safeTreeId] !== undefined) {
    delete gShowSubtreesBySafeTreeId[safeTreeId];
  } else {
    gShowSubtreesBySafeTreeId[safeTreeId] = isExpansion;
  }
}

function expandPathToThisElement(aElement) {
  if (aElement.classList.contains("kids")) {
    // Unhide the kids.
    aElement.classList.remove("hidden");
    expandPathToThisElement(aElement.previousSibling);  // hasKids

  } else if (aElement.classList.contains("hasKids")) {
    // Change the separator to '--'.
    let sepSpan = aElement.childNodes[2];
    assertClassListContains(sepSpan, "mrSep");
    sepSpan.textContent = kShowKidsSep;
    expandPathToThisElement(aElement.parentNode);       // kids or pre.entries

  } else {
    assertClassListContains(aElement, "entries");
  }
}

/**
 * Appends the elements for the tree, including its heading.
 *
 * @param aP
 *        The parent DOM node.
 * @param aRoot
 *        The tree root.
 * @param aProcess
 *        The process the tree corresponds to.
 * @param aPadText
 *        A string to pad the start of each entry.
 */
function appendTreeElements(aP, aRoot, aProcess, aPadText) {
  /**
   * Appends the elements for a particular tree, without a heading.
   *
   * @param aP
   *        The parent DOM node.
   * @param aProcess
   *        The process the tree corresponds to.
   * @param aUnsafeNames
   *        An array of the names forming the path to aT.
   * @param aRoot
   *        The root of the tree this sub-tree belongs to.
   * @param aT
   *        The tree.
   * @param aTreelineText1
   *        The first part of the treeline for this entry and this entry's
   *        children.
   * @param aTreelineText2a
   *        The second part of the treeline for this entry.
   * @param aTreelineText2b
   *        The second part of the treeline for this entry's children.
   * @param aParentStringLength
   *        The length of the formatted byte count of the top node in the tree.
   */
  function appendTreeElements2(aP, aProcess, aUnsafeNames, aRoot, aT,
                               aTreelineText1, aTreelineText2a,
                               aTreelineText2b, aParentStringLength) {
    function appendN(aS, aC, aN) {
      for (let i = 0; i < aN; i++) {
        aS += aC;
      }
      return aS;
    }

    // The tree line.  Indent more if this entry is narrower than its parent.
    let valueText = aT.toString();
    let extraTreelineLength =
      Math.max(aParentStringLength - valueText.length, 0);
    if (extraTreelineLength > 0) {
      aTreelineText2a =
        appendN(aTreelineText2a, kHorizontal, extraTreelineLength);
      aTreelineText2b =
        appendN(aTreelineText2b, " ", extraTreelineLength);
    }
    let treelineText = aTreelineText1 + aTreelineText2a;
    appendElementWithText(aP, "span", "treeline", treelineText);

    // Detect and record invalid values.  But not if gIsDiff is true, because
    // we expect negative values in that case.
    assertInput(aRoot._units === aT._units,
                "units within a tree are inconsistent");
    let tIsInvalid = false;
    if (!gIsDiff && !(0 <= aT._amount && aT._amount <= aRoot._amount)) {
      tIsInvalid = true;
      let unsafePath = aUnsafeNames.join("/");
      gUnsafePathsWithInvalidValuesForThisProcess.push(unsafePath);
      reportAssertionFailure("Invalid value (" + aT._amount + " / " +
                             aRoot._amount + ") for " +
                             flipBackslashes(unsafePath));
    }

    // For non-leaf nodes, the entire sub-tree is put within a span so it can
    // be collapsed if the node is clicked on.
    let d;
    let sep;
    let showSubtrees;
    if (aT._kids) {
      // Determine if we should show the sub-tree below this entry;  this
      // involves reinstating any previous toggling of the sub-tree.
      let unsafePath = aUnsafeNames.join("/");
      let safeTreeId = aProcess + ":" + flipBackslashes(unsafePath);
      showSubtrees = !aT._hideKids;
      if (gShowSubtreesBySafeTreeId[safeTreeId] !== undefined) {
        showSubtrees = gShowSubtreesBySafeTreeId[safeTreeId];
      }
      d = appendElement(aP, "span", "hasKids");
      d.id = safeTreeId;
      d.onclick = toggle;
      sep = showSubtrees ? kShowKidsSep : kHideKidsSep;
    } else {
      assert(!aT._hideKids, "leaf node with _hideKids set")
      sep = kNoKidsSep;
      d = aP;
    }

    // The value.
    appendElementWithText(d, "span", "mrValue" + (tIsInvalid ? " invalid" : ""),
                          valueText);

    // The percentage (omitted for single entries).
    let percText;
    if (!aT._isDegenerate) {
      // Treat 0 / 0 as 100%.
      let num = aRoot._amount === 0 ? 100 : (100 * aT._amount / aRoot._amount);
      let numText = num.toFixed(2);
      percText = numText === "100.00"
               ? " (100.0%)"
               : (0 <= num && num < 10 ? " (0" : " (") + numText + "%)";
      appendElementWithText(d, "span", "mrPerc", percText);
    }

    // The separator.
    appendElementWithText(d, "span", "mrSep", sep);

    // The entry's name.
    appendMrNameSpan(d, aT._description, aT._unsafeName,
                     tIsInvalid, aT._nMerged, aT._presence);

    // In non-verbose mode, invalid nodes can be hidden in collapsed sub-trees.
    // But it's good to always see them, so force this.
    if (!gVerbose.checked && tIsInvalid) {
      expandPathToThisElement(d);
    }

    // Recurse over children.
    if (aT._kids) {
      // The 'kids' class is just used for sanity checking in toggle().
      d = appendElement(aP, "span", showSubtrees ? "kids" : "kids hidden");

      let kidTreelineText1 = aTreelineText1 + aTreelineText2b;
      for (let i = 0; i < aT._kids.length; i++) {
        let kidTreelineText2a, kidTreelineText2b;
        if (i < aT._kids.length - 1) {
          kidTreelineText2a = kVerticalAndRight_Right_Right;
          kidTreelineText2b = kVertical_Space_Space;
        } else {
          kidTreelineText2a = kUpAndRight_Right_Right;
          kidTreelineText2b = "   ";
        }
        aUnsafeNames.push(aT._kids[i]._unsafeName);
        appendTreeElements2(d, aProcess, aUnsafeNames, aRoot, aT._kids[i],
                            kidTreelineText1, kidTreelineText2a,
                            kidTreelineText2b, valueText.length);
        aUnsafeNames.pop();
      }
    }
  }

  let rootStringLength = aRoot.toString().length;
  appendTreeElements2(aP, aProcess, [aRoot._unsafeName], aRoot, aRoot,
                      aPadText, "", "", rootStringLength);
}

// ---------------------------------------------------------------------------

function appendSectionHeader(aP, aText) {
  appendElementWithText(aP, "h2", "", aText + "\n");
  return appendElement(aP, "pre", "entries");
}

// ---------------------------------------------------------------------------

function saveReportsToFile() {
  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  fp.appendFilter("Zipped JSON files", "*.json.gz");
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  fp.filterIndex = 0;
  fp.addToRecentDocs = true;
  fp.defaultString = "memory-report.json.gz";

  let fpFinish = function(file) {
    let dumper = Cc["@mozilla.org/memory-info-dumper;1"]
                   .getService(Ci.nsIMemoryInfoDumper);
    let finishDumping = () => {
      updateMainAndFooter("Saved memory reports to " + file.path, HIDE_FOOTER);
    }
    dumper.dumpMemoryReportsToNamedFile(file.path, finishDumping, null,
                                        gAnonymize.checked);
  }

  let fpCallback = function(aResult) {
    if (aResult == Ci.nsIFilePicker.returnOK ||
        aResult == Ci.nsIFilePicker.returnReplace) {
      fpFinish(fp.file);
    }
  };

  try {
    fp.init(window, "Save Memory Reports", Ci.nsIFilePicker.modeSave);
  } catch (ex) {
    // This will fail on Android, since there is no Save as file picker there.
    // Just save to the default downloads dir if it does.
    Downloads.getSystemDownloadsDirectory().then(function(dirPath) {
      let file = FileUtils.File(dirPath);
      file.append(fp.defaultString);
      fpFinish(file);
    });

    return;
  }
  fp.open(fpCallback);
}
