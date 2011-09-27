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

const KIND_NONHEAP = Ci.nsIMemoryReporter.KIND_NONHEAP;
const KIND_HEAP    = Ci.nsIMemoryReporter.KIND_HEAP;
const KIND_OTHER   = Ci.nsIMemoryReporter.KIND_OTHER;
const UNITS_BYTES  = Ci.nsIMemoryReporter.UNITS_BYTES;
const UNITS_COUNT  = Ci.nsIMemoryReporter.UNITS_COUNT;
const UNITS_COUNT_CUMULATIVE = Ci.nsIMemoryReporter.UNITS_COUNT_CUMULATIVE;
const UNITS_PERCENTAGE = Ci.nsIMemoryReporter.UNITS_PERCENTAGE;

const kUnknown = -1;    // used for _amount if a memory reporter failed

const kTreeDescriptions = {
  'explicit' :
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
    "trying to reduce memory usage.",

  'resident':
    "This tree shows how much space in physical memory each of the " +
    "process's mappings is currently using (the mapping's 'resident set size', " +
    "or 'RSS'). This is a good measure of the 'cost' of the mapping, although " +
    "it does not take into account the fact that shared libraries may be mapped " +
    "by multiple processes but appear only once in physical memory. " +
    "Note that the 'resident' value here might not equal the value for " +
    "'resident' under 'Other Measurements' because the two measurements are not " +
    "taken at exactly the same time.",

  'pss':
    "This tree shows how much space in physical memory can be 'blamed' on this " +
    "process.  For each mapping, its 'proportional set size' (PSS) is the " +
    "mapping's resident size divided by the number of processes which use the " +
    "mapping.  So if a mapping is private to this process, its PSS should equal " +
    "its RSS.  But if a mapping is shared between three processes, its PSS in " +
    "each of the processes would be 1/3 its RSS.",

  'vsize':
    "This tree shows how much virtual addres space each of the process's " +
    "mappings takes up (the mapping's 'vsize').  A mapping may have a large " +
    "vsize but use only a small amount of physical memory; the resident set size " +
    "of a mapping is a better measure of the mapping's 'cost'. Note that the " +
    "'vsize' value here might not equal the value for 'vsize' under 'Other " +
    "Measurements' because the two measurements are not taken at exactly the " +
    "same time.",

  'swap':
    "This tree shows how much space in the swap file each of the process's " +
    "mappings is currently using. Mappings which are not in the swap file " +
    "(i.e., nodes which would have a value of 0 in this tree) are omitted."
};

const kTreeNames = {
  'explicit': 'Explicit Allocations',
  'resident': 'Resident Set Size (RSS) Breakdown',
  'pss':      'Proportional Set Size (PSS) Breakdown',
  'vsize':    'Virtual Size Breakdown',
  'swap':     'Swap Usage Breakdown',
  'other':    'Other Measurements'
};

const kMapTreePaths = ['map/resident', 'map/pss', 'map/vsize', 'map/swap'];

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
  var os = Cc["@mozilla.org/observer-service;1"]
            .getService(Ci.nsIObserverService);
  os.notifyObservers(null, "child-gc-request", null);
  update();
}

function doCC()
{
  window.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils)
        .cycleCollect();
  var os = Cc["@mozilla.org/observer-service;1"]
            .getService(Ci.nsIObserverService);
  os.notifyObservers(null, "child-cc-request", null);
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

function toggleTreeVisibility(aEvent)
{
  var headerElem = aEvent.target;

  // Replace "header-" with "pre-" in the header element's id to get the id of
  // the corresponding pre element.
  var treeElem = $(headerElem.id.replace(/^header-/, 'pre-'));

  treeElem.classList.toggle('collapsed');
}

function Reporter(aPath, aKind, aUnits, aAmount, aDescription)
{
  this._path        = aPath;
  this._kind        = aKind;
  this._units       = aUnits;
  this._amount      = aAmount;
  this._description = aDescription;
  // this._nMerged is only defined if > 1
  // this._done is defined when getBytes is called
}

Reporter.prototype = {
  // Sum the values (accounting for possible kUnknown amounts), and mark |this|
  // as a dup.  We mark dups because it's useful to know when a reporter is
  // duplicated;  it might be worth investigating and splitting up to have
  // non-duplicated names.
  merge: function(r) {
    if (this._amount !== kUnknown && r._amount !== kUnknown) {
      this._amount += r._amount;
    } else if (this._amount === kUnknown && r._amount !== kUnknown) {
      this._amount = r._amount;
    }
    this._nMerged = this._nMerged ? this._nMerged + 1 : 2;
  },

  treeNameMatches: function(aTreeName) {
    return this._path.slice(0, aTreeName.length) === aTreeName;
  }
};

function getReportersByProcess()
{
  var mgr = Cc["@mozilla.org/memory-reporter-manager;1"].
      getService(Ci.nsIMemoryReporterManager);

  // Process each memory reporter:
  // - Make a copy of it into a sub-table indexed by its process.  Each copy
  //   is a Reporter object.  After this point we never use the original memory
  //   reporter again.
  //
  // - Note that copying rOrig.amount (which calls a C++ function under the
  //   IDL covers) to r._amount for every reporter now means that the
  //   results as consistent as possible -- measurements are made all at
  //   once before most of the memory required to generate this page is
  //   allocated.
  var reportersByProcess = {};

  function addReporter(aProcess, aPath, aKind, aUnits, aAmount, aDescription)
  {
    var process = aProcess === "" ? "Main" : aProcess;
    var r = new Reporter(aPath, aKind, aUnits, aAmount, aDescription);
    if (!reportersByProcess[process]) {
      reportersByProcess[process] = {};
    }
    var reporters = reportersByProcess[process];
    var reporter = reporters[r._path];
    if (reporter) {
      // Already an entry;  must be a duplicated reporter.  This can happen
      // legitimately.  Merge them.
      reporter.merge(r);
    } else {
      reporters[r._path] = r;
    }
  }

  // Process vanilla reporters first, then multi-reporters.
  var e = mgr.enumerateReporters();
  while (e.hasMoreElements()) {
    var rOrig = e.getNext().QueryInterface(Ci.nsIMemoryReporter);
    try {
      addReporter(rOrig.process, rOrig.path, rOrig.kind, rOrig.units,
                  rOrig.amount, rOrig.description);
    }
    catch(e) {
      debug("An error occurred when collecting results from the memory reporter " +
            rOrig.path + ": " + e);
    }
  }
  var e = mgr.enumerateMultiReporters();
  while (e.hasMoreElements()) {
    var mrOrig = e.getNext().QueryInterface(Ci.nsIMemoryMultiReporter);
    try {
      mrOrig.collectReports(addReporter, null);
    }
    catch(e) {
      debug("An error occurred when collecting a multi-reporter's results: " + e);
    }
  }

  return reportersByProcess;
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

  if (gVerbose)
    content.parentNode.classList.add('verbose');
  else
    content.parentNode.classList.add('non-verbose');

  // Generate output for one process at a time.  Always start with the
  // Main process.
  var reportersByProcess = getReportersByProcess();
  var text = genProcessText("Main", reportersByProcess["Main"]);
  for (var process in reportersByProcess) {
    if (process !== "Main") {
      text += genProcessText(process, reportersByProcess[process]);
    }
  }

  // Memory-related actions.
  const GCDesc = "Do a global garbage collection.";
  const CCDesc = "Do a cycle collection.";
  const MPDesc = "Send three \"heap-minimize\" notifications in a " +
                 "row.  Each notification triggers a global garbage " +
                 "collection followed by a cycle collection, and causes the " +
                 "process to reduce memory usage in other ways, e.g. by " +
                 "flushing various caches.";

  text += "<div>" +
    "<button title='" + GCDesc + "' onclick='doGlobalGC()'>GC</button>" +
    "<button title='" + CCDesc + "' onclick='doCC()'>CC</button>" +
    "<button title='" + MPDesc + "' onclick='sendHeapMinNotifications()'>" + "Minimize memory usage</button>" +
    "</div>";

  // Generate verbosity option link at the bottom.
  text += "<div>";
  text += gVerbose
        ? "<span class='option'><a href='about:memory'>Less verbose</a></span>"
        : "<span class='option'><a href='about:memory?verbose'>More verbose</a></span>";
  text += "</div>";

  text += "<div>" +
          "<span class='option'><a href='about:support'>Troubleshooting information</a></span>" +
          "</div>";

  text += "<div>" +
          "<span class='legend'>Hover the pointer over the name of a memory " +
          "reporter to see a detailed description of what it measures. Click a " +
          "heading to expand or collapse its tree.</span>" +
          "</div>";

  var div = document.createElement("div");
  div.innerHTML = text;
  content.appendChild(div);
}

// There are two kinds of TreeNode.  Those that correspond to Reporters
// have more properties.  The remainder are just scaffolding nodes for the
// tree, whose values are derived from their children.
function TreeNode(aName)
{
  // Nb: _units is not needed, it's always UNITS_BYTES.
  this._name = aName;
  this._kids = [];
  // All TreeNodes have these properties added later:
  // - _amount (which is never |kUnknown|)
  // - _description
  //
  // TreeNodes corresponding to Reporters have these properties added later:
  // - _kind
  // - _nMerged (if > 1)
  // - _hasProblem (only defined if true)
}

TreeNode.prototype = {
  findKid: function(aName) {
    for (var i = 0; i < this._kids.length; i++) {
      if (this._kids[i]._name === aName) {
        return this._kids[i];
      }
    }
    return undefined;
  },

  toString: function() {
    return formatBytes(this._amount);
  }
};

TreeNode.compare = function(a, b) {
  return b._amount - a._amount;
};

/**
 * From a list of memory reporters, builds a tree that mirrors the tree
 * structure that will be shown as output.
 *
 * @param aReporters
 *        The table of Reporters, indexed by path.
 * @param aTreeName
 *        The name of the tree being built.
 * @return The built tree.
 */
function buildTree(aReporters, aTreeName)
{
  // We want to process all reporters that begin with |aTreeName|.  First we
  // build the tree but only fill the properties that we can with a top-down
  // traversal.

  // Is there any reporter which matches aTreeName?  If not, we'll create a
  // dummy one.
  var foundReporter = false;
  for (var path in aReporters) {
    if (aReporters[path].treeNameMatches(aTreeName)) {
      foundReporter = true;
      break;
    }
  }

  if (!foundReporter) {
    // We didn't find any reporters for this tree, so bail.
    return null;
  }

  var t = new TreeNode("falseRoot");
  for (var path in aReporters) {
    // Add any missing nodes in the tree implied by the path.
    var r = aReporters[path];
    if (r.treeNameMatches(aTreeName)) {
      assert(r._kind === KIND_HEAP || r._kind === KIND_NONHEAP,
             "reporters in the tree must have KIND_HEAP or KIND_NONHEAP");
      assert(r._units === UNITS_BYTES, "r._units === UNITS_BYTES");
      var names = r._path.split('/');
      var u = t;
      for (var i = 0; i < names.length; i++) {
        var name = names[i];
        var uMatch = u.findKid(name);
        if (uMatch) {
          u = uMatch;
        } else {
          var v = new TreeNode(name);
          u._kids.push(v);
          u = v;
        }
      }
      // Fill in extra details from the Reporter.
      u._kind = r._kind;
      if (r._nMerged) {
        u._nMerged = r._nMerged;
      }
    }
  }

  // Using falseRoot makes the above code simpler.  Now discard it, leaving
  // aTreeName at the root.
  t = t._kids[0];

  // Next, fill in the remaining properties bottom-up.
  // Note that this function never returns kUnknown.
  function fillInTree(aT, aPrepath)
  {
    var path = aPrepath ? aPrepath + '/' + aT._name : aT._name;
    if (aT._kids.length === 0) {
      // Leaf node.  Must have a reporter.
      assert(aT._kind !== undefined, "aT._kind !== undefined");
      aT._description = getDescription(aReporters, path);
      var amount = getBytes(aReporters, path);
      if (amount !== kUnknown) {
        aT._amount = amount;
      } else {
        aT._amount = 0;
        aT._hasProblem = true;
      }
    } else {
      // Non-leaf node.  Get the size of the children.
      var childrenBytes = 0;
      for (var i = 0; i < aT._kids.length; i++) {
        // Allow for kUnknown, treat it like 0.
        childrenBytes += fillInTree(aT._kids[i], path);
      }
      if (aT._kind !== undefined) {
        aT._description = getDescription(aReporters, path);
        var amount = getBytes(aReporters, path);
        if (amount !== kUnknown) {
          // Non-leaf node with its own reporter.  Use the reporter and add
          // an "other" child node.
          aT._amount = amount;
          var other = new TreeNode("other");
          other._description = "All unclassified " + aT._name + " memory.",
          other._amount = aT._amount - childrenBytes,
          aT._kids.push(other);
        } else {
          // Non-leaf node with a reporter that returns kUnknown.
          // Use the sum of the children and mark it as problematic.
          aT._amount = childrenBytes;
          aT._hasProblem = true;
        }
      } else {
        // Non-leaf node without its own reporter.  Derive its size and
        // description entirely from its children.
        aT._amount = childrenBytes;
        aT._description = "The sum of all entries below '" + aT._name + "'.";
      }
    }
    assert(aT._amount !== kUnknown, "aT._amount !== kUnknown");
    return aT._amount;
  }

  fillInTree(t, "");

  // Reduce the depth of the tree by the number of occurrences of '/' in
  // aTreeName.  (Thus the tree named 'foo/bar/baz' will be rooted at 'baz'.)
  var slashCount = 0;
  for (var i = 0; i < aTreeName.length; i++) {
    if (aTreeName[i] == '/') {
      assert(t._kids.length == 1, "Not expecting multiple kids here.");
      t = t._kids[0];
    }
  }

  // Set the description on the root node.
  t._description = kTreeDescriptions[t._name];

  return t;
}

/**
 * Do some work which only makes sense for the 'explicit' tree.
 */
function fixUpExplicitTree(aT, aReporters) {
  // Determine how many bytes are reported by heap reporters.  Be careful
  // with non-leaf reporters;  if we count a non-leaf reporter we don't want
  // to count any of its child reporters.
  var s = "";
  function getKnownHeapUsedBytes(aT)
  {
    if (aT._kind === KIND_HEAP) {
      return aT._amount;
    } else {
      var n = 0;
      for (var i = 0; i < aT._kids.length; i++) {
        n += getKnownHeapUsedBytes(aT._kids[i]);
      }
      return n;
    }
  }

  // A special case:  compute the derived "heap-unclassified" value.  Don't
  // mark "heap-allocated" when we get its size because we want it to appear
  // in the "Other Measurements" list.
  var heapUsedBytes = getBytes(aReporters, "heap-allocated", true);
  var unknownHeapUsedBytes = 0;
  var hasProblem = true;
  if (heapUsedBytes !== kUnknown) {
    unknownHeapUsedBytes = heapUsedBytes - getKnownHeapUsedBytes(aT);
    hasProblem = false;
  }
  var heapUnclassified = new TreeNode("heap-unclassified");
  // This kindToString() ensures the "(Heap)" prefix is set without having to
  // set the _kind property, which would mean that there is a corresponding
  // Reporter for this TreeNode (which isn't true).
  heapUnclassified._description =
      kindToString(KIND_HEAP) +
      "Memory not classified by a more specific reporter. This includes " +
      "waste due to internal fragmentation in the heap allocator (caused " +
      "when the allocator rounds up request sizes).";
  heapUnclassified._amount = unknownHeapUsedBytes;
  if (hasProblem) {
    heapUnclassified._hasProblem = true;
  }

  aT._kids.push(heapUnclassified);
  aT._amount += unknownHeapUsedBytes;
}

/**
 * Sort all kid nodes from largest to smallest and aggregate insignificant
 * nodes.
 *
 * @param aTotalBytes
 *        The size of the tree's root node.
 * @param aT
 *        The tree.
 */
function filterTree(aTotalBytes, aT)
{
  const omitThresholdPerc = 0.5; /* percent */

  function shouldOmit(aBytes)
  {
    return !gVerbose &&
           aTotalBytes !== kUnknown &&
           (100 * aBytes / aTotalBytes) < omitThresholdPerc;
  }

  aT._kids.sort(TreeNode.compare);

  for (var i = 0; i < aT._kids.length; i++) {
    if (shouldOmit(aT._kids[i]._amount)) {
      // This sub-tree is below the significance threshold
      // Remove it and all remaining (smaller) sub-trees, and
      // replace them with a single aggregate node.
      var i0 = i;
      var aggBytes = 0;
      for ( ; i < aT._kids.length; i++) {
        aggBytes += aT._kids[i]._amount;
      }
      aT._kids.splice(i0, aT._kids.length);
      var n = i - i0;
      var rSub = new TreeNode("(" + n + " omitted)");
      rSub._amount = aggBytes;
      rSub._description =
        n + " sub-trees that were below the " + omitThresholdPerc +
        "% significance threshold.  Click 'More verbose' at the bottom of " +
        "this page to see them.";

      // Add the "omitted" sub-tree at the end and then re-sort, because the
      // sum of the omitted sub-trees may be larger than some of the shown
      // sub-trees.
      aT._kids[i0] = rSub;
      aT._kids.sort(TreeNode.compare);
      break;
    }
    filterTree(aTotalBytes, aT._kids[i]);
  }
}

/**
 * Generates the text for a single process.
 *
 * @param aProcess
 *        The name of the process.
 * @param aReporters
 *        Table of Reporters for this process, indexed by _path.
 * @return The generated text.
 */
function genProcessText(aProcess, aReporters)
{
  var explicitTree = buildTree(aReporters, 'explicit');
  fixUpExplicitTree(explicitTree, aReporters);
  filterTree(explicitTree._amount, explicitTree);
  var explicitText = genTreeText(explicitTree, aProcess);

  var mapTreeText = '';
  kMapTreePaths.forEach(function(t) {
    var tree = buildTree(aReporters, t);

    // |tree| will be null if we don't have any reporters for the given path.
    if (tree) {
      filterTree(tree._amount, tree);
      mapTreeText += genTreeText(tree, aProcess);
    }
  });

  // We have to call genOtherText after we process all the trees, because it
  // looks at all the reporters which aren't part of a tree.
  var otherText = genOtherText(aReporters, aProcess);

  // The newlines give nice spacing if we cut+paste into a text buffer.
  return "<h1>" + aProcess + " Process</h1>\n\n" +
         explicitText + mapTreeText + otherText +
         "<hr></hr>";
}

/**
 * Formats an int as a human-readable string.
 *
 * @param aN
 *        The integer to format.
 * @return A human-readable string representing the int.
 */
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

/**
 * Converts a byte count to an appropriate string representation.
 *
 * @param aBytes
 *        The byte count.
 * @return The string representation.
 */
function formatBytes(aBytes)
{
  var unit = gVerbose ? "B" : "MB";

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
 * Converts a percentage to an appropriate string representation.
 *
 * @param aPerc100x
 *        The percentage, multiplied by 100 (see nsIMemoryReporter).
 * @return The string representation
 */
function formatPercentage(aPerc100x)
{
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
 * Gets the byte count for a particular Reporter and sets its _done
 * property.
 *
 * @param aReporters
 *        Table of Reporters for this process, indexed by _path.
 * @param aPath
 *        The path of the R.
 * @param aDoNotMark
 *        If true, the _done property is not set.
 * @return The byte count.
 */
function getBytes(aReporters, aPath, aDoNotMark)
{
  var r = aReporters[aPath];
  assert(r, "getBytes: no such Reporter: " + aPath);
  if (!aDoNotMark) {
    r._done = true;
  }
  return r._amount;
}

/**
 * Gets the description for a particular Reporter.
 *
 * @param aReporters
 *        Table of Reporters for this process, indexed by _path.
 * @param aPath
 *        The path of the Reporter.
 * @return The description.
 */
function getDescription(aReporters, aPath)
{
  var r = aReporters[aPath];
  assert(r, "getDescription: no such Reporter: " + aPath);
  return r._description;
}

function genMrValueText(aValue)
{
  return "<span class='mrValue'>" + aValue + "</span>";
}

function kindToString(aKind)
{
  switch (aKind) {
   case KIND_NONHEAP: return "(Non-heap) ";
   case KIND_HEAP:    return "(Heap) ";
   case KIND_OTHER:
   case undefined:    return "";
   default:           assert(false, "bad kind in kindToString");
  }
}

function escapeQuotes(aStr)
{
  return aStr.replace(/\&/g, '&amp;').replace(/'/g, '&#39;');
}

// For user-controlled strings.
function escapeAll(aStr)
{
  return aStr.replace(/\&/g, '&amp;').replace(/'/g, '&#39;').
              replace(/\</g, '&lt;').replace(/>/g, '&gt;').
              replace(/\"/g, '&quot;');
}

// Compartment reporter names are URLs and so can include forward slashes.  But
// forward slash is the memory reporter path separator.  So the memory
// reporters change them to backslashes.  Undo that here.  
function flipBackslashes(aStr)
{
  return aStr.replace(/\\/g, '/');
}

function prepName(aStr)
{
  return escapeAll(flipBackslashes(aStr));
}

function prepDesc(aStr)
{
  return escapeQuotes(flipBackslashes(aStr));
}

function genMrNameText(aKind, aDesc, aName, aHasProblem, aNMerged)
{
  var text = "-- <span class='mrName hasDesc' title='" +
             kindToString(aKind) + prepDesc(aDesc) +
             "'>" + prepName(aName) + "</span>";
  if (aHasProblem) {
    const problemDesc =
      "Warning: this memory reporter was unable to compute a useful value. " +
      "The reported value is the sum of all entries below '" + aName + "', " +
      "which is probably less than the true value.";
    text += " <span class='mrStar' title=\"" + problemDesc + "\">[*]</span>";
  }
  if (aNMerged) {
    const dupDesc = "This value is the sum of " + aNMerged +
                    " memory reporters that all have the same path.";
    text += " <span class='mrStar' title=\"" + dupDesc + "\">[" + 
            aNMerged + "]</span>";
  }
  return text + '\n';
}

/**
 * Generates the text for the tree, including its heading.
 *
 * @param aT
 *        The tree.
 * @param aProcess
 *        The process the tree corresponds to.
 * @return The generated text.
 */
function genTreeText(aT, aProcess)
{
  var treeBytes = aT._amount;
  var rootStringLength = aT.toString().length;
  var isExplicitTree = aT._name == 'explicit';

  /**
   * Generates the text for a particular tree, without a heading.
   *
   * @param aT
   *        The tree.
   * @param aIndentGuide
   *        Records what indentation is required for this tree.  It has one
   *        entry per level of indentation.  For each entry, ._isLastKid
   *        records whether the node in question is the last child, and
   *        ._depth records how many chars of indentation are required.
   * @param aParentStringLength
   *        The length of the formatted byte count of the top node in the tree.
   * @return The generated text.
   */
  function genTreeText2(aT, aIndentGuide, aParentStringLength)
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
    var tString = aT.toString();
    var extraIndentLength = Math.max(aParentStringLength - tString.length, 0);
    if (extraIndentLength > 0) {
      for (var i = 0; i < extraIndentLength; i++) {
        indent += kHorizontal;
      }
      aIndentGuide[aIndentGuide.length - 1]._depth += extraIndentLength;
    }
    indent += "</span>";

    // Generate the percentage.
    var perc = "";
    if (aT._amount === treeBytes) {
      perc = "100.0";
    } else {
      perc = (100 * aT._amount / treeBytes).toFixed(2);
      perc = pad(perc, 5, '0');
    }
    perc = "<span class='mrPerc'>(" + perc + "%)</span> ";

    // We don't want to show '(nonheap)' on a tree like 'map/vsize', since the
    // whole tree is non-heap.
    var kind = isExplicitTree ? aT._kind : undefined;
    var text = indent + genMrValueText(tString) + " " + perc +
               genMrNameText(kind, aT._description, aT._name,
                             aT._hasProblem, aT._nMerged);

    for (var i = 0; i < aT._kids.length; i++) {
      // 3 is the standard depth, the callee adjusts it if necessary.
      aIndentGuide.push({ _isLastKid: (i === aT._kids.length - 1), _depth: 3 });
      text += genTreeText2(aT._kids[i], aIndentGuide, tString.length);
      aIndentGuide.pop();
    }
    return text;
  }

  var text = genTreeText2(aT, [], rootStringLength);

  // The explicit tree is not collapsed, but all other trees are, so pass
  // !isExplicitTree for genSectionMarkup's aCollapsed parameter.
  return genSectionMarkup(aProcess, aT._name, text, !isExplicitTree);
}

function OtherReporter(aPath, aUnits, aAmount, aDescription, 
                       aNMerged)
{
  // Nb: _kind is not needed, it's always KIND_OTHER.
  this._path        = aPath;
  this._units       = aUnits;
  if (aAmount === kUnknown) {
    this._amount     = 0;
    this._hasProblem = true;
  } else {
    this._amount = aAmount;
  }
  this._description = aDescription;
  this.asString = this.toString();
}

OtherReporter.prototype = {
  toString: function() {
    switch(this._units) {
      case UNITS_BYTES:            return formatBytes(this._amount);
      case UNITS_COUNT:
      case UNITS_COUNT_CUMULATIVE: return formatInt(this._amount);
      case UNITS_PERCENTAGE:       return formatPercentage(this._amount);
      default:
        assert(false, "bad units in OtherReporter.toString");
    }
  }
};

OtherReporter.compare = function(a, b) {
  return a._path < b._path ? -1 :
         a._path > b._path ?  1 :
         0;
};

/**
 * Generates the text for the "Other Measurements" section.
 *
 * @param aReportersByProcess
 *        Table of Reporters for this process, indexed by _path.
 * @param aProcess
 *        The process these reporters correspond to.
 * @return The generated text.
 */
function genOtherText(aReportersByProcess, aProcess)
{
  // Generate an array of Reporter-like elements, stripping out all the
  // Reporters that have already been handled.  Also find the width of the
  // widest element, so we can format things nicely.
  var maxStringLength = 0;
  var otherReporters = [];
  for (var path in aReportersByProcess) {
    var r = aReportersByProcess[path];
    if (!r._done) {
      assert(r._kind === KIND_OTHER, "_kind !== KIND_OTHER for " + r._path);
      assert(r.nMerged === undefined);  // we don't allow dup'd OTHER reporters 
      var hasProblem = false;
      if (r._amount === kUnknown) {
        hasProblem = true;
      }
      var o = new OtherReporter(r._path, r._units, r._amount, r._description);
      otherReporters.push(o);
      if (o.asString.length > maxStringLength) {
        maxStringLength = o.asString.length;
      }
    }
  }
  otherReporters.sort(OtherReporter.compare);

  // Generate text for the not-yet-printed values.
  var text = "";
  for (var i = 0; i < otherReporters.length; i++) {
    var o = otherReporters[i];
    text += genMrValueText(pad(o.asString, maxStringLength, ' ')) + " ";
    text += genMrNameText(KIND_OTHER, o._description, o._path, o._hasProblem);
  }

  // Nb: the newlines give nice spacing if we cut+paste into a text buffer.
  const desc = "This list contains other memory measurements that cross-cut " +
               "the requested memory measurements above."

  return genSectionMarkup(aProcess, 'other', text, false);
}

function genSectionMarkup(aProcess, aName, aText, aCollapsed)
{
  var headerId = 'header-' + aProcess + '-' + aName;
  var preId = 'pre-' + aProcess + '-' + aName;
  var elemClass = (aCollapsed ? 'collapsed' : '') + ' tree';

  // Ugh.
  return '<h2 id="' + headerId + '" class="' + elemClass + '" ' +
         'onclick="toggleTreeVisibility(event)">' +
           kTreeNames[aName] +
         '</h2>\n' +
         '<pre id="' + preId + '" class="' + elemClass + '">' + aText + '</pre>\n';
}

function assert(aCond, aMsg)
{
  if (!aCond) {
    throw("assertion failed: " + aMsg);
  }
}

function debug(x)
{
  var content = $("content");
  var div = document.createElement("div");
  div.innerHTML = JSON.stringify(x);
  content.appendChild(div);
}
