/* -*- Mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil; -*- */
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
 * The Original Code is about:memory
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Nicholas Nethercote <nnethercote@mozilla.com>
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

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

// Must use .href here instead of .search because "about:memory" is a
// non-standard URL.
var gVerbose = (location.href.split(/[\?,]/).indexOf("verbose") !== -1);

var gAddedObserver = false;

const MR_MAPPED = Ci.nsIMemoryReporter.MR_MAPPED;
const MR_HEAP   = Ci.nsIMemoryReporter.MR_HEAP;
const MR_OTHER  = Ci.nsIMemoryReporter.MR_OTHER;

const kUnknown = -1;    // used for _memoryUsed if a memory reporter failed

function onLoad()
{
  var os = Cc["@mozilla.org/observer-service;1"].
      getService(Ci.nsIObserverService);
  os.notifyObservers(null, "child-memory-reporter-request", null);

  os.addObserver(ChildMemoryListener, "child-memory-reporter-update", false);
  gAddedObserver = true;

  update();
}

function onUnload()
{
  // We need to check if the observer has been added before removing; in some
  // circumstances (eg. reloading the page quickly) it might not have because
  // onLoad might not fire.
  if (gAddedObserver) {
    var os = Cc["@mozilla.org/observer-service;1"].
        getService(Ci.nsIObserverService);
    os.removeObserver(ChildMemoryListener, "child-memory-reporter-update");
  }
}

function ChildMemoryListener(aSubject, aTopic, aData)
{
  update();
}

function $(n)
{
  return document.getElementById(n);
}

function doGlobalGC()
{
  Cu.forceGC();
  update();
}

function doGlobalGCandCC()
{
  window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils)
        .garbageCollect();
  update();
}

// For maximum effect, this returns to the event loop between each
// notification.  See bug 610166 comment 12 for an explanation.
// Ideally a single notification would be enough.
function sendHeapMinNotifications()
{
  function runSoon(f)
  {
    var tm = Cc["@mozilla.org/thread-manager;1"]
              .getService(Ci.nsIThreadManager);

    tm.mainThread.dispatch({ run: f }, Ci.nsIThread.DISPATCH_NORMAL);
  }

  function sendHeapMinNotificationsInner()
  {
    var os = Cc["@mozilla.org/observer-service;1"]
             .getService(Ci.nsIObserverService);
    os.notifyObservers(null, "memory-pressure", "heap-minimize");

    if (++j < 3)
      runSoon(sendHeapMinNotificationsInner);
    else
      runSoon(update);
  }

  var j = 0;
  sendHeapMinNotificationsInner();
}

/**
 * Top-level function that does the work of generating the page.
 */
function update()
{
  // First, clear the page contents.  Necessary because update() might be
  // called more than once due to ChildMemoryListener.
  var content = $("content");
  content.parentNode.replaceChild(content.cloneNode(false), content);
  content = $("content");

  var mgr = Cc["@mozilla.org/memory-reporter-manager;1"].
      getService(Ci.nsIMemoryReporterManager);

  // Process each memory reporter:
  // - Make a copy of it into a sub-table indexed by its process.  Each copy
  //   looks like this:
  //
  //     interface Tmr {
  //       _tpath: string;
  //       _kind:        number;
  //       _description: string;
  //       _memoryUsed:  number;
  //     }
  //
  // - The .path property is renamed ._tpath ("truncated path") in the copy
  //   because the process name and ':' (if present) are removed.
  // - Note that copying mr.memoryUsed (which calls a C++ function under the
  //   IDL covers) to tmr._memoryUsed for every reporter now means that the
  //   results as consistent as possible -- measurements are made all at
  //   once before most of the memory required to generate this page is
  //   allocated.
  var tmrTable = {};
  var e = mgr.enumerateReporters();
  while (e.hasMoreElements()) {
    var mr = e.getNext().QueryInterface(Ci.nsIMemoryReporter);
    var process;
    var tmr = {};
    var i = mr.path.indexOf(':');
    if (i === -1) {
      process = "Main";
      tmr._tpath = mr.path;
    } else {
      process = mr.path.slice(0, i);
      tmr._tpath = mr.path.slice(i + 1);
    }
    tmr._kind        = mr.kind;
    tmr._description = mr.description;
    tmr._memoryUsed  = mr.memoryUsed;

    if (!tmrTable[process]) {
      tmrTable[process] = {};
    }
    var tmrs = tmrTable[process];
    if (tmrs[tmr._tpath]) {
      // Already an entry;  must be a duplicated reporter.  This can
      // happen legitimately.  Sum the sizes.
      tmrs[tmr._tpath]._memoryUsed += tmr._memoryUsed;
    } else {
      tmrs[tmr._tpath] = tmr;
    }
  }

  // Generate output for one process at a time.  Always start with the
  // Main process.
  var text = genProcessText("Main", tmrTable["Main"]);
  for (var process in tmrTable) {
    if (process !== "Main") {
      text += genProcessText(process, tmrTable[process]);
    }
  }

  // Memory-related actions.
  const GCDesc = "Do a global garbage collection.";
  // XXX: once bug 625302 is fixed, should change this button to just do a CC.
  const CCDesc = "Do a global garbage collection followed by a cycle " +
                 "collection. (It currently is not possible to do a cycle " +
                 "collection on its own, see bug 625302.)";
  const MPDesc = "Send three \"heap-minimize\" notifications in a " +
                 "row.  Each notification triggers a global garbage " +
                 "collection followed by a cycle collection, and causes the " +
                 "process to reduce memory usage in other ways, e.g. by " +
                 "flushing various caches.";

  text += "<div>" +
    "<button title='" + GCDesc + "' onclick='doGlobalGC()'>GC</button>" +
    "<button title='" + CCDesc + "' onclick='doGlobalGCandCC()'>GC + CC</button>" +
    "<button title='" + MPDesc + "' onclick='sendHeapMinNotifications()'>" + "Minimize memory usage</button>" +
    "</div>";

  // Generate verbosity option link at the bottom.
  text += "<div>";
  text += gVerbose
        ? "<span class='option'><a href='about:memory'>Less verbose</a></span>"
        : "<span class='option'><a href='about:memory?verbose'>More verbose</a></span>";
  text += "</div>";

  text += "<div>" +
          "<span class='legend'>Hover the pointer over the name of a memory " +
          "reporter to see a detailed description of what it measures.</span>"
          "</div>";


  var div = document.createElement("div");
  div.innerHTML = text;
  content.appendChild(div);
}

function cmpTmrs(a, b)
{
  return b._memoryUsed - a._memoryUsed
};

/**
 * Generates the text for a single process.
 *
 * @param aProcess
 *        The name of the process
 * @param aTmrs
 *        Table of Tmrs for this process
 * @return The generated text
 */
function genProcessText(aProcess, aTmrs)
{
  /**
   * From a list of memory reporters, builds a tree that mirrors the tree
   * structure that will be shown as output.
   *
   * @return The built tree.  The tree nodes have this structure:
   *         interface Node {
   *           _name: string;
   *           _kind:        number;
   *           _description: string;
   *           _memoryUsed:  number;    (non-negative or 'kUnknown')
   *           _kids:        [Node];
   *           _hasReporter: boolean;   (only defined if 'true')
   *           _hasProblem:  boolean;   (only defined if 'true')
   *         }
   */
  function buildTree()
  {
    const treeName = "explicit";
    const omitThresholdPerc = 0.5; /* percent */

    function findKid(aName, aKids)
    {
      for (var i = 0; i < aKids.length; i++) {
        if (aKids[i]._name === aName) {
          return aKids[i];
        }
      }
      return undefined;
    }

    // We want to process all reporters that begin with 'treeName'.
    // First we build the tree but only filling in '_name', '_kind', '_kids'
    // and maybe '._hasReporter'.  This is done top-down from the reporters.
    var t = {
      _name: "falseRoot",
      _kind: MR_OTHER,
      _kids: []
    };
    for (var tpath in aTmrs) {
      var tmr = aTmrs[tpath];
      if (tmr._tpath.slice(0, treeName.length) === treeName) {
        var names = tmr._tpath.split('/');
        var u = t;
        for (var i = 0; i < names.length; i++) {
          var name = names[i];
          var uMatch = findKid(name, u._kids);
          if (uMatch) {
            u = uMatch;
          } else {
            var v = {
              _name: name,
              _kind: MR_OTHER,
              _kids: []
            };
            u._kids.push(v);
            u = v;
          }
        }
        u._kind = tmr._kind;
        u._hasReporter = true;
      }
    }
    // Using falseRoot makes the above code simpler.  Now discard it, leaving
    // treeName at the root.
    t = t._kids[0];

    // Next, fill in '_description' and '_memoryUsed', and maybe '_hasProblem'
    // for each node.  This is done bottom-up because for most non-leaf nodes
    // '_memoryUsed' and '_description' are determined from the child nodes.
    function fillInTree(aT, aPretpath)
    {
      var tpath = aPretpath ? aPretpath + '/' + aT._name : aT._name;
      if (aT._kids.length === 0) {
        // Leaf node.  Must have a reporter.
        aT._description = getDescription(aTmrs, tpath);
        var memoryUsed = getBytes(aTmrs, tpath);
        if (memoryUsed !== kUnknown) {
          aT._memoryUsed = memoryUsed;
        } else {
          aT._memoryUsed = 0;
          aT._hasProblem = true;
        }
      } else {
        // Non-leaf node.  Get the size of the children.
        var childrenBytes = 0;
        for (var i = 0; i < aT._kids.length; i++) {
          // Allow for kUnknown, treat it like 0.
          var b = fillInTree(aT._kids[i], tpath);
          childrenBytes += (b === kUnknown ? 0 : b);
        }
        if (aT._hasReporter === true) {
          aT._description = getDescription(aTmrs, tpath);
          var memoryUsed = getBytes(aTmrs, tpath);
          if (memoryUsed !== kUnknown) {
            // Non-leaf node with its own reporter.  Use the reporter and add
            // an "other" child node.
            aT._memoryUsed = memoryUsed;
            var other = {
              _name: "other",
              _kind: MR_OTHER,
              _description: "All unclassified " + aT._name + " memory.",
              _memoryUsed: aT._memoryUsed - childrenBytes,
              _kids: []
            };
            aT._kids.push(other);
          } else {
            // Non-leaf node with a reporter that returns kUnknown.
            // Use the sum of the children and mark it as problematic.
            aT._memoryUsed = childrenBytes;
            aT._hasProblem = true;
          }
        } else {
          // Non-leaf node without its own reporter.  Derive its size and
          // description entirely from its children.
          aT._memoryUsed = childrenBytes;
          aT._description = "The sum of all entries below '" + aT._name + "'.";
        }
      }
      return aT._memoryUsed;
    }
    fillInTree(t, "");

    // Determine how many bytes are reported by heap reporters.  Be careful
    // with non-leaf reporters;  if we count a non-leaf reporter we don't want
    // to count any of its child reporters.
    var s = "";
    function getKnownHeapUsedBytes(aT)
    {
      if (aT._kind === MR_HEAP) {
        return aT._memoryUsed;
      } else {
        var n = 0;
        for (var i = 0; i < aT._kids.length; i++) {
          n += getKnownHeapUsedBytes(aT._kids[i]);
        }
        return n;
      }
    }

    // A special case:  compute the derived "heap-unclassified" value.  Don't
    // mark "heap-used" when we get its size because we want it to appear in
    // the "Other Measurements" list.
    var heapUsedBytes = getBytes(aTmrs, "heap-used", true);
    var unknownHeapUsedBytes = 0;
    var hasProblem = true;
    if (heapUsedBytes !== kUnknown) {
      unknownHeapUsedBytes = heapUsedBytes - getKnownHeapUsedBytes(t);
      hasProblem = false;
    }
    var heapUnclassified = {
      _name: "heap-unclassified",
      _kind: MR_HEAP,
      _description:
        "Memory not classified by a more specific reporter. This includes " +
        "memory allocated by the heap allocator in excess of that requested " +
        "by the application; this can happen when the heap allocator rounds " +
        "up request sizes.",
      _memoryUsed: unknownHeapUsedBytes,
      _hasProblem: hasProblem,
      _kids: []
    }
    t._kids.push(heapUnclassified);
    t._memoryUsed += unknownHeapUsedBytes;

    function shouldOmit(aBytes)
    {
      return !gVerbose &&
             t._memoryUsed !== kUnknown &&
             (100 * aBytes / t._memoryUsed) < omitThresholdPerc;
    }

    /**
     * Sort all kid nodes from largest to smallest and aggregate
     * insignificant nodes.
     *
     * @param aT
     *        The tree
     */
    function filterTree(aT)
    {
      aT._kids.sort(cmpTmrs);

      for (var i = 0; i < aT._kids.length; i++) {
        if (shouldOmit(aT._kids[i]._memoryUsed)) {
          // This sub-tree is below the significance threshold
          // Remove it and all remaining (smaller) sub-trees, and
          // replace them with a single aggregate node.
          var i0 = i;
          var aggBytes = 0;
          var aggNames = [];
          for ( ; i < aT._kids.length; i++) {
            aggBytes += aT._kids[i]._memoryUsed;
            aggNames.push(aT._kids[i]._name);
          }
          aT._kids.splice(i0);
          var n = i - i0;
          var tmrSub = {
            _name: "(" + n + " omitted)",
            _kind: MR_OTHER,
            _description: "Omitted sub-trees: " + aggNames.join(", ") + ".",
            _memoryUsed: aggBytes,
            _kids: []
          };
          aT._kids[i0] = tmrSub;
          break;
        }
        filterTree(aT._kids[i]);
      }
    }
    filterTree(t);

    return t;
  }

  // Nb: the newlines give nice spacing if we cut+paste into a text buffer.
  var text = "";
  text += "<h1>" + aProcess + " Process</h1>\n\n";
  text += genTreeText(buildTree());
  text += genOtherText(aTmrs);
  text += "<hr></hr>";
  return text;
}

/**
 * Converts a byte count to an appropriate string representation.
 *
 * @param aBytes
 *        The byte count
 * @return The string representation
 */
function formatBytes(aBytes)
{
  var unit = gVerbose ? "B" : "MB";

  function formatInt(aN)
  {
    var neg = false;
    if (aN < 0) {
      neg = true;
      aN = -aN;
    }
    var s = "";
    while (true) {
      var k = aN % 1000;
      aN = Math.floor(aN / 1000);
      if (aN > 0) {
        if (k < 10) {
          s = ",00" + k + s;
        } else if (k < 100) {
          s = ",0" + k + s;
        } else {
          s = "," + k + s;
        }
      } else {
        s = k + s;
        break;
      }
    }
    return neg ? "-" + s : s;
  }

  var s;
  if (gVerbose) {
    s = formatInt(aBytes) + " " + unit;
  } else {
    var mbytes = (aBytes / (1024 * 1024)).toFixed(2);
    var a = String(mbytes).split(".");
    s = formatInt(a[0]) + "." + a[1] + " " + unit;
  }
  return s;
}

/**
 * Right-justifies a string in a field of a given width, padding as necessary
 *
 * @param aS
 *        The string
 * @param aN
 *        The field width
 * @param aC
 *        The char used to pad
 * @return The string representation
 */
function pad(aS, aN, aC)
{
  var padding = "";
  var n2 = aN - aS.length;
  for (var i = 0; i < n2; i++) {
    padding += aC;
  }
  return padding + aS;
}

/**
 * Gets the byte count for a particular memory reporter and sets its _done
 * property.
 *
 * @param aTmrs
 *        Table of Tmrs for this process
 * @param aTpath
 *        The tpath of the memory reporter
 * @param aDoNotMark
 *        If set, the _done property is not set.
 * @return The byte count
 */
function getBytes(aTmrs, aTpath, aDoNotMark)
{
  var tmr = aTmrs[aTpath];
  if (tmr) {
    var bytes = tmr._memoryUsed;
    if (!aDoNotMark) {
      tmr._done = true;
    }
    return bytes;
  }
  // Nb: this should never occur; all tpaths have been extracted from aTmrs and
  // so the lookup will succeed.  Return an obviously wrong number that will
  // likely be noticed.
  return -2 * 1024 * 1024;
}

/**
 * Gets the description for a particular memory reporter.
 *
 * @param aTmrs
 *        Table of Tmrs for this process
 * @param aTpath
 *        The tpath of the memory reporter
 * @return The description
 */
function getDescription(aTmrs, aTpath)
{
  var r = aTmrs[aTpath];
  return r ? r._description : "???";
}

function genMrValueText(aValue)
{
  return "<span class='mrValue'>" + aValue + "</span>";
}

function kindToString(aKind)
{
  switch (aKind) {
   case MR_MAPPED: return "(Mapped) ";
   case MR_HEAP:   return "(Heap) ";
   case MR_OTHER:  return "";
   default:        return "(???) ";
  }
}

function escapeQuotes(aStr)
{
  return aStr.replace(/'/g, '&#39;');
}

function genMrNameText(aKind, aDesc, aName, aHasProblem)
{
  const problemDesc =
    "Warning: this memory reporter was unable to compute a useful value. " +
    "The reported value is the sum of all entries below '" + aName + "', " +
    "which is probably less than the true value.";
  var text = "-- <span class='mrName hasDesc' title='" +
             kindToString(aKind) + escapeQuotes(aDesc) +
             "'>" + aName + "</span>";
  text += aHasProblem
        ? " <span class='mrStar' title=\"" + problemDesc + "\">[*]</span>\n"
        : "\n";
  return text;
}

/**
 * Generates the text for the tree, including its heading.
 *
 * @param aT
 *        The tree
 * @return The generated text
 */
function genTreeText(aT)
{
  var treeBytes = aT._memoryUsed;
  var treeBytesLength = formatBytes(treeBytes).length;

  /**
   * Generates the text for a particular tree, without a heading.
   *
   * @param aT
   *        The tree
   * @param aIndentGuide
   *        Records what indentation is required for this tree.  It has one
   *        entry per level of indentation.  For each entry, ._isLastKid
   *        records whether the node in question is the last child, and
   *        ._depth records how many chars of indentation are required.
   * @param aParentBytesLength
   *        The length of the formatted byte count of the top node in the tree
   * @return The generated text
   */
  function genTreeText2(aT, aIndentGuide, aParentBytesLength)
  {
    function repeatStr(aC, aN)
    {
      var s = "";
      for (var i = 0; i < aN; i++) {
        s += aC;
      }
      return s;
    }

    // Generate the indent.  There's a subset of the Unicode "light"
    // box-drawing chars that are widely implemented in terminals, and
    // this code sticks to that subset to maximize the chance that
    // cutting and pasting about:memory output to a terminal will work
    // correctly:
    const kHorizontal       = "\u2500",
          kVertical         = "\u2502",
          kUpAndRight       = "\u2514",
          kVerticalAndRight = "\u251c";
    var indent = "<span class='treeLine'>";
    if (aIndentGuide.length > 0) {
      for (var i = 0; i < aIndentGuide.length - 1; i++) {
        indent += aIndentGuide[i]._isLastKid ? " " : kVertical;
        indent += repeatStr(" ", aIndentGuide[i]._depth - 1);
      }
      indent += aIndentGuide[i]._isLastKid ? kUpAndRight : kVerticalAndRight;
      indent += repeatStr(kHorizontal, aIndentGuide[i]._depth - 1);
    }

    // Indent more if this entry is narrower than its parent, and update
    // aIndentGuide accordingly.
    var tMemoryUsedStr = formatBytes(aT._memoryUsed);
    var tBytesLength = tMemoryUsedStr.length;
    var extraIndentLength = Math.max(aParentBytesLength - tBytesLength, 0);
    if (extraIndentLength > 0) {
      for (var i = 0; i < extraIndentLength; i++) {
        indent += kHorizontal;
      }
      aIndentGuide[aIndentGuide.length - 1]._depth += extraIndentLength;
    }
    indent += "</span>";

    // Generate the percentage.
    var perc = "";
    if (aT._memoryUsed === treeBytes) {
      perc = "100.0";
    } else {
      perc = (100 * aT._memoryUsed / treeBytes).toFixed(2);
      perc = pad(perc, 5, '0');
    }
    perc = "<span class='mrPerc'>(" + perc + "%)</span> ";

    var text = indent + genMrValueText(tMemoryUsedStr) + " " + perc +
               genMrNameText(aT._kind, aT._description, aT._name,
                             aT._hasProblem);

    for (var i = 0; i < aT._kids.length; i++) {
      // 3 is the standard depth, the callee adjusts it if necessary.
      aIndentGuide.push({ _isLastKid: (i === aT._kids.length - 1), _depth: 3 });
      text += genTreeText2(aT._kids[i], aIndentGuide, tBytesLength);
      aIndentGuide.pop();
    }
    return text;
  }

  var text = genTreeText2(aT, [], treeBytesLength);
  // Nb: the newlines give nice spacing if we cut+paste into a text buffer.
  const desc =
    "This tree covers explicit memory allocations by the application, " +
    "both at the operating system level (via calls to functions such as " +
    "VirtualAlloc, vm_allocate, and mmap), and at the heap allocation level " +
    "(via functions such as malloc, calloc, realloc, memalign, operator " +
    "new, and operator new[]).  It excludes memory that is mapped implicitly " +
    "such as code and data segments, and thread stacks.  It also excludes " +
    "heap memory that has been freed by the application but is still being " +
    "held onto by the heap allocator.  It is not guaranteed to cover every " +
    "explicit allocation, but it does cover most (including the entire " +
    "heap), and therefore it is the single best number to focus on when " +
    "trying to reduce memory usage.";
               
  return "<h2 class='hasDesc' title='" + escapeQuotes(desc) +
         "'>Explicit Allocations</h2>\n" + "<pre>" + text + "</pre>\n";
}

/**
 * Generates the text for the "Other Measurements" section.
 *
 * @param aTmrs
 *        Table of Tmrs for this process
 * @return The generated text
 */
function genOtherText(aTmrs)
{
  // Generate an array of tmr-like elements, stripping out all the tmrs that
  // have already been handled.  Also find the width of the widest element, so
  // we can format things nicely.
  var maxBytesLength = 0;
  var tmrArray = [];
  for (var tpath in aTmrs) {
    var tmr = aTmrs[tpath];
    if (!tmr._done) {
      var hasProblem = false;
      if (tmr._memoryUsed === kUnknown) {
        hasProblem = true;
      }
      var elem = {
        _tpath:       tmr._tpath,
        _kind:        tmr._kind,
        _description: tmr._description,
        _memoryUsed:  hasProblem ? 0 : tmr._memoryUsed,
        _hasProblem:  hasProblem
      };
      tmrArray.push(elem);
      var thisBytesLength = formatBytes(elem._memoryUsed).length;
      if (thisBytesLength > maxBytesLength) {
        maxBytesLength = thisBytesLength;
      }
    }
  }
  tmrArray.sort(cmpTmrs);

  // Generate text for the not-yet-printed values.
  var text = "";
  for (var i = 0; i < tmrArray.length; i++) {
    var elem = tmrArray[i];
    text += genMrValueText(
              pad(formatBytes(elem._memoryUsed), maxBytesLength, ' ')) + " ";
    text += genMrNameText(elem._kind, elem._description, elem._tpath,
                          elem._hasProblem);
  }

  // Nb: the newlines give nice spacing if we cut+paste into a text buffer.
  const desc = "This list contains other memory measurements that cross-cut " +
               "the requested memory measurements above."
  return "<h2 class='hasDesc' title='" + desc + "'>Other Measurements</h2>\n" +
         "<pre>" + text + "</pre>\n";
}

