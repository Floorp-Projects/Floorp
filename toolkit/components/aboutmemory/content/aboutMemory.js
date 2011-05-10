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

// Must use .href here instead of .search because "about:memory" is a
// non-standard URL.
var gVerbose = (location.href.split(/[\?,]/).indexOf("verbose") !== -1);

function onLoad()
{
  var os = Cc["@mozilla.org/observer-service;1"].
      getService(Ci.nsIObserverService);
  os.notifyObservers(null, "child-memory-reporter-request", null);

  os.addObserver(ChildMemoryListener, "child-memory-reporter-update", false);

  update();
}

function onUnload()
{
  var os = Cc["@mozilla.org/observer-service;1"].
      getService(Ci.nsIObserverService);
  os.removeObserver(ChildMemoryListener, "child-memory-reporter-update");
}

function ChildMemoryListener(aSubject, aTopic, aData)
{
  update();
}

function $(n)
{
  return document.getElementById(n);
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
  //       _description: string;
  //       _memoryUsed: number;
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

  // Generate verbosity option link at the bottom.
  text += gVerbose
        ? "<span class='option'><a href='about:memory'>Less verbose</a></span>"
        : "<span class='option'><a href='about:memory?verbose'>More verbose</a></span>";

  var div = document.createElement("div");
  div.innerHTML = text;
  content.appendChild(div);
}

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
   * @param aTreeName
   *        The name of the tree;  either "mapped" or "heap-used"
   * @param aTreeRootTpath
   *        The tpath of the top node in the tree
   * @param aTreeRootDesc
   *        The description of the top node in the tree
   * @param aOtherDescTail
   *        Extra description for the end of the "aTreeName/other" entry
   * @param aOmitThresholdPerc
   *        The threshold percentage;  entries that account for less than
   *        this fraction are aggregated
   * @return The built tree.  The tree nodes have this structure:
   *         interface Node {
   *           _name: string;
   *           _description: string;
   *           _memoryUsed: number;
   *           _kids: [Node];
   *         }
   */
  function buildTree(aTreeName, aTreeRootTpath, aTreeRootDesc, aOtherDescTail,
                     aOmitThresholdPerc)
  {
    function findKid(aName, aKids)
    {
      for (var i = 0; i < aKids.length; i++) {
        if (aKids[i]._name === aName) {
          return aKids[i];
        }
      }
      return undefined;
    }

    // We want to process all reporters that begin with 'aTreeName'.
    // First we build the tree but only filling in '_name' and '_kids'.
    var t = { _name:aTreeName, _kids:[] };
    for (var _tpath in aTmrs) {
      var tmr = aTmrs[_tpath];
      if (tmr._tpath.slice(0, aTreeName.length + 1) === aTreeName + "/") {
        var names = tmr._tpath.slice(aTreeName.length + 1).split('/');
        var u = t;
        for (var i = 0; i < names.length; i++) {
          var name = names[i];
          var uMatch = findKid(name, u._kids);
          if (uMatch) {
            u = uMatch;
          } else {
            var v = { _name:name, _kids:[] };
            u._kids.push(v);
            u = v;
          }
        }
      }
    }

    // Next, fill in '_description' and '_memoryUsed' for each node.  For
    // interior nodes, '_memoryUsed' is computed by summing child nodes.
    function fillInTree(aT, aPretpath)
    {
      var tpath = aPretpath ? aPretpath + '/' + aT._name : aT._name;
      if (aT._kids.length === 0) {
        aT._memoryUsed = getBytes(aTmrs, tpath);
        aT._description = getDescription(aTmrs, tpath);
      } else {
        var bytes = 0;
        for (var i = 0; i < aT._kids.length; i++) {
          // Allow for -1 (ie. "unknown"), treat it like 0.
          var b = fillInTree(aT._kids[i], tpath);
          bytes += (b === -1 ? 0 : b);
        }
        aT._memoryUsed = bytes;
        aT._description = "The sum of all entries below " + tpath + ".";
      }
      return aT._memoryUsed;
    }
    fillInTree(t, "");

    // Add the "aTreeName/other" node, which is derived from existing
    // nodes, then update the root node accordingly.  (But don't do this
    // if the root node byte count is -1, ie. unknown).
    var nonOtherBytes = t._memoryUsed;
    var treeBytes = getBytes(aTmrs, aTreeRootTpath);
    if (treeBytes !== -1) {
      var otherBytes = treeBytes - nonOtherBytes;
      var other = {
        _name:"other",
        _description:"All unclassified " + aTreeName + " memory." +
                     aOtherDescTail,
        _memoryUsed:otherBytes,
        _kids:[]
      };
      t._kids.push(other);
    }
    t._memoryUsed = treeBytes;
    t._description = aTreeRootDesc;

    function shouldOmit(aBytes)
    {
      return !gVerbose &&
             treeBytes !== -1 &&
             (100 * aBytes / treeBytes) < aOmitThresholdPerc;
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
      var cmpTmrs = function(a, b) { return b._memoryUsed - a._memoryUsed };
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
            _description: "Omitted sub-trees: " + aggNames.join(", ") + ".",
            _memoryUsed: aggBytes,
            _kids:[]
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

  var mappedOtherDescTail =
      " This includes code and data segments, and thread stacks."
  var mappedRootDesc = getDescription(aTmrs, "mapped");
  // The threshold used here is much lower than the one for the heap-used
  // tree, because the "mapped" total size is so much bigger relative to
  // the interesting entries in the "mapped" tree.
  var mappedTree = buildTree("mapped", "mapped", mappedRootDesc,
                             mappedOtherDescTail, 0.01);

  var heapUsedOtherDescTail = "";
  var heapUsedRootDesc = "See mapped/heap/used above.";
  var heapUsedTree = buildTree("heap-used", "mapped/heap/used",
                               heapUsedRootDesc, heapUsedOtherDescTail, 0.1);

  // Nb: the newlines give nice spacing if we cut+paste into a text buffer.
  var text = "";
  text += "<h1>" + aProcess + " Process</h1>\n\n";
  text += genTreeText(mappedTree, "Mapped Memory");
  text += genTreeText(heapUsedTree, "Used Heap Memory");
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

  if (aBytes === -1) {
    return "??? " + unit;
  }

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
 * Gets the byte count for a particular memory reporter.
 *
 * @param aTmrs
 *        Table of Tmrs for this process
 * @param aTpath
 *        The tpath of the memory reporter
 * @return The byte count
 */
function getBytes(aTmrs, aTpath)
{
  var tmr = aTmrs[aTpath];
  if (tmr) {
    var bytes = tmr._memoryUsed;
    tmr.done = true;
    return bytes;
  }
  // Nb: this should never occur;  "mapped" and "mapped/heap/used" should
  // always be registered, and all other tpaths have been extracted from
  // aTmrs and so the lookup will succeed.  Return an obviously wrong
  // number that will likely be noticed.
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

function genMrNameText(aDesc, aName)
{
  return "-- <span class='mrName' title=\"" + aDesc + "\">" +
         aName + "</span>\n";
}

/**
 * Generates the text for a particular tree, including its heading.
 *
 * @param aT
 *        The tree
 * @param aTreeName
 *        The tree's name
 * @return The generated text
 */
function genTreeText(aT, aTreeName)
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
    if (treeBytes !== -1) {
      if (aT._memoryUsed === treeBytes) {
        perc = "100.0";
      } else {
        perc = (100 * aT._memoryUsed / treeBytes).toFixed(2);
        perc = pad(perc, 5, '0');
      }
      perc = "<span class='mrPerc'>(" + perc + "%)</span> ";
    }

    var text = indent + genMrValueText(tMemoryUsedStr) + " " + perc +
               genMrNameText(aT._description, aT._name);

    for (var i = 0; i < aT._kids.length; i++) {
      // 3 is the standard depth, the callee adjusts it if necessary.
      aIndentGuide.push({ _isLastKid:(i === aT._kids.length - 1), _depth:3 });
      text += genTreeText2(aT._kids[i], aIndentGuide, tBytesLength);
      aIndentGuide.pop();
    }
    return text;
  }

  var text = genTreeText2(aT, [], treeBytesLength);
  // Nb: the newlines give nice spacing if we cut+paste into a text buffer.
  return "<h2>" + aTreeName + "</h2>\n<pre>" + text + "</pre>\n";
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
  // Get the biggest not-yet-printed value, to determine the field width
  // for all these entries.  These should all be "other" values, assuming
  // all paths are well-formed.
  var maxBytes = 0;
  for (var tpath in aTmrs) {
    var tmr = aTmrs[tpath];
    if (!tmr.done && tmr._memoryUsed > maxBytes) {
      maxBytes = tmr._memoryUsed;
    }
  }

  // Generate text for the not-yet-yet-printed values.
  var maxBytesLength = formatBytes(maxBytes).length;
  var text = "";
  for (var tpath in aTmrs) {
    var tmr = aTmrs[tpath];
    if (!tmr.done) {
      text += genMrValueText(
                pad(formatBytes(tmr._memoryUsed), maxBytesLength, ' ')) + " ";
      text += genMrNameText(tmr._description, tmr._tpath);
    }
  }

  // Nb: the newlines give nice spacing if we cut+paste into a text buffer.
  return "<h2>Other Measurements</h2>\n<pre>" + text + "</pre>\n";
}

