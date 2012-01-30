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

// Paths, names and descriptions all need to be sanitized before being
// displayed, because the user has some control over them via names of
// compartments, windows, etc.  Also, forward slashes in URLs in paths are
// represented with backslashes to avoid being mistaken for path separators, so
// we need to undo that as well.

function escapeAll(aStr)
{
  return aStr.replace(/\&/g, '&amp;').replace(/'/g, '&#39;').
              replace(/\</g, '&lt;').replace(/>/g, '&gt;').
              replace(/\"/g, '&quot;');
}

function flipBackslashes(aStr)
{
  return aStr.replace(/\\/g, '/');
}

function makeSafe(aUnsafeStr)
{
  return escapeAll(flipBackslashes(aUnsafeStr));
}

const kTreeUnsafeDescriptions = {
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

function Reporter(aUnsafePath, aKind, aUnits, aAmount, aUnsafeDesc)
{
  this._unsafePath  = aUnsafePath;
  this._kind        = aKind;
  this._units       = aUnits;
  this._amount      = aAmount;
  this._unsafeDescription = aUnsafeDesc;
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
    // Nb: the '/' must be present, because we have a KIND_OTHER reporter
    // called "explicit" which is not part of the "explicit" tree.
    aTreeName += "/";
    return this._unsafePath.slice(0, aTreeName.length) === aTreeName;
  }
};

function getReportersByProcess(aMgr)
{
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

  function addReporter(aProcess, aUnsafePath, aKind, aUnits, aAmount,
                       aUnsafeDesc)
  {
    var process = aProcess === "" ? "Main" : aProcess;
    var r = new Reporter(aUnsafePath, aKind, aUnits, aAmount, aUnsafeDesc);
    if (!reportersByProcess[process]) {
      reportersByProcess[process] = {};
    }
    var reporters = reportersByProcess[process];
    var reporter = reporters[r._unsafePath];
    if (reporter) {
      // Already an entry;  must be a duplicated reporter.  This can happen
      // legitimately.  Merge them.
      reporter.merge(r);
    } else {
      reporters[r._unsafePath] = r;
    }
  }

  // Process vanilla reporters first, then multi-reporters.
  var e = aMgr.enumerateReporters();
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
  var e = aMgr.enumerateMultiReporters();
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

  var mgr = Cc["@mozilla.org/memory-reporter-manager;1"].
      getService(Ci.nsIMemoryReporterManager);

  var text = "";

  // Generate output for one process at a time.  Always start with the
  // Main process.
  var reportersByProcess = getReportersByProcess(mgr);
  var hasMozMallocUsableSize = mgr.hasMozMallocUsableSize;
  text += genProcessText("Main", reportersByProcess["Main"],
                         hasMozMallocUsableSize);
  for (var process in reportersByProcess) {
    if (process !== "Main") {
      text += genProcessText(process, reportersByProcess[process],
                             hasMozMallocUsableSize);
    }
  }

  // Memory-related actions.
  const UpDesc = "Re-measure.";
  const GCDesc = "Do a global garbage collection.";
  const CCDesc = "Do a cycle collection.";
  const MPDesc = "Send three \"heap-minimize\" notifications in a " +
                 "row.  Each notification triggers a global garbage " +
                 "collection followed by a cycle collection, and causes the " +
                 "process to reduce memory usage in other ways, e.g. by " +
                 "flushing various caches.";

  // The "Update" button has an id so it can be clicked in a test.
  text += "<div>" +
    "<button title='" + UpDesc + "' onclick='update()' id='updateButton'>Update</button>" +
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
          "<span class='legend'>Click on a non-leaf node in a tree to expand ('++') " +
          "or collapse ('--') its children.</span>" +
          "</div>";
  text += "<div>" +
          "<span class='legend'>Hover the pointer over the name of a memory " +
          "reporter to see a description of what it measures.</span>";

  var div = document.createElement("div");
  div.innerHTML = text;
  content.appendChild(div);
}

// There are two kinds of TreeNode.
// - Leaf TreeNodes correspond to Reporters and have more properties.
// - Non-leaf TreeNodes are just scaffolding nodes for the tree;  their values
//   are derived from their children.
function TreeNode(aUnsafeName)
{
  // Nb: _units is not needed, it's always UNITS_BYTES.
  this._unsafeName = aUnsafeName;
  this._kids = [];
  // All TreeNodes have these properties added later:
  // - _amount (which is never |kUnknown|)
  // - _unsafeDescription
  //
  // Leaf TreeNodes have these properties added later:
  // - _kind
  // - _nMerged (if > 1)
  // - _isUnknown (only defined if true)
  //
  // Non-leaf TreeNodes have these properties added later:
  // - _hideKids (only defined if true)
}

TreeNode.prototype = {
  findKid: function(aUnsafeName) {
    for (var i = 0; i < this._kids.length; i++) {
      if (this._kids[i]._unsafeName === aUnsafeName) {
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
 *        The table of Reporters, indexed by _unsafePath.
 * @param aTreeName
 *        The name of the tree being built.
 * @return The built tree.
 */
function buildTree(aReporters, aTreeName)
{
  // We want to process all reporters that begin with |aTreeName|.  First we
  // build the tree but only fill the properties that we can with a top-down
  // traversal.

  // There should always be at least one matching reporter when |aTreeName| is
  // "explicit".  But there may be zero for "map" trees;  if that happens,
  // bail.
  var foundReporter = false;
  for (var unsafePath in aReporters) {
    if (aReporters[unsafePath].treeNameMatches(aTreeName)) {
      foundReporter = true;
      break;
    }
  }
  if (!foundReporter) {
    assert(aTreeName !== 'explicit');
    return null;
  }

  var t = new TreeNode("falseRoot");
  for (var unsafePath in aReporters) {
    // Add any missing nodes in the tree implied by the unsafePath.
    var r = aReporters[unsafePath];
    if (r.treeNameMatches(aTreeName)) {
      assert(r._kind === KIND_HEAP || r._kind === KIND_NONHEAP,
             "reporters in the tree must have KIND_HEAP or KIND_NONHEAP");
      assert(r._units === UNITS_BYTES, "r._units === UNITS_BYTES");
      var unsafeNames = r._unsafePath.split('/');
      var u = t;
      for (var i = 0; i < unsafeNames.length; i++) {
        var unsafeName = unsafeNames[i];
        var uMatch = u.findKid(unsafeName);
        if (uMatch) {
          u = uMatch;
        } else {
          var v = new TreeNode(unsafeName);
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
  function fillInTree(aT, aUnsafePrePath)
  {
    var unsafePath =
      aUnsafePrePath ? aUnsafePrePath + '/' + aT._unsafeName : aT._unsafeName; 
    if (aT._kids.length === 0) {
      // Leaf node.  Must have a reporter.
      assert(aT._kind !== undefined, "aT._kind is undefined for leaf node");
      aT._unsafeDescription = getUnsafeDescription(aReporters, unsafePath);
      var amount = getBytes(aReporters, unsafePath);
      if (amount !== kUnknown) {
        aT._amount = amount;
      } else {
        aT._amount = 0;
        aT._isUnknown = true;
      }
    } else {
      // Non-leaf node.  Derive its size and description entirely from its
      // children.
      assert(aT._kind === undefined, "aT._kind is defined for non-leaf node");
      var childrenBytes = 0;
      for (var i = 0; i < aT._kids.length; i++) {
        childrenBytes += fillInTree(aT._kids[i], unsafePath);
      }
      aT._amount = childrenBytes;
      aT._unsafeDescription =
        "The sum of all entries below '" + aT._unsafeName + "'.";
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

  // Set the (unsafe) description on the root node.
  t._unsafeDescription = kTreeUnsafeDescriptions[t._unsafeName];

  return t;
}

/**
 * Ignore all the memory reporters that belong to a tree;  this involves
 * explicitly marking them as done.
 *
 * @param aReporters
 *        The table of Reporters, indexed by _unsafePath.
 * @param aTreeName
 *        The name of the tree being built.
 */
function ignoreTree(aReporters, aTreeName)
{
  for (var unsafePath in aReporters) {
    var r = aReporters[unsafePath];
    if (r.treeNameMatches(aTreeName)) {
      var dummy = getBytes(aReporters, unsafePath);
    }
  }
}

/**
 * Do some work which only makes sense for the 'explicit' tree.
 *
 * @param aT
 *        The tree.
 * @param aReporters
 *        Table of Reporters for this process, indexed by _unsafePath.
 * @return A boolean indicating if "heap-allocated" is known for the process.
 */
function fixUpExplicitTree(aT, aReporters)
{
  // Determine how many bytes are reported by heap reporters.
  var s = "";
  function getKnownHeapUsedBytes(aT)
  {
    var n = 0;
    if (aT._kids.length === 0) {
      // Leaf node.
      assert(aT._kind !== undefined, "aT._kind is undefined for leaf node");
      n = aT._kind === KIND_HEAP ? aT._amount : 0;
    } else {
      for (var i = 0; i < aT._kids.length; i++) {
        n += getKnownHeapUsedBytes(aT._kids[i]);
      }
    }
    return n;
  }

  // A special case:  compute the derived "heap-unclassified" value.  Don't
  // mark "heap-allocated" when we get its size because we want it to appear
  // in the "Other Measurements" list.
  var heapAllocatedBytes = getBytes(aReporters, "heap-allocated", true);
  var heapUnclassifiedT = new TreeNode("heap-unclassified");
  var hasKnownHeapAllocated = heapAllocatedBytes !== kUnknown;
  if (hasKnownHeapAllocated) {
    heapUnclassifiedT._amount =
      heapAllocatedBytes - getKnownHeapUsedBytes(aT);
  } else {
    heapUnclassifiedT._amount = 0;
    heapUnclassifiedT._isUnknown = true;
  }
  // This kindToString() ensures the "(Heap)" prefix is set without having to
  // set the _kind property, which would mean that there is a corresponding
  // Reporter for this TreeNode (which isn't true)
  heapUnclassifiedT._unsafeDescription = kindToString(KIND_HEAP) +
      "Memory not classified by a more specific reporter. This includes " +
      "slop bytes due to internal fragmentation in the heap allocator " +
      "(caused when the allocator rounds up request sizes).";

  aT._kids.push(heapUnclassifiedT);
  aT._amount += heapUnclassifiedT._amount;

  return hasKnownHeapAllocated;
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
function sortTreeAndInsertAggregateNodes(aTotalBytes, aT)
{
  const kSignificanceThresholdPerc = 1;

  function isInsignificant(aT)
  {
    return !gVerbose &&
           aTotalBytes !== kUnknown &&
           (100 * aT._amount / aTotalBytes) < kSignificanceThresholdPerc;
  }

  if (aT._kids.length === 0) {
    return;
  }

  aT._kids.sort(TreeNode.compare);

  // If the first child is insignificant, they all are, and there's no point
  // creating an aggregate node that lacks siblings.  Just set the parent's
  // _hideKids property and process all children.
  if (isInsignificant(aT._kids[0])) {
    aT._hideKids = true;
    for (var i = 0; i < aT._kids.length; i++) {
      sortTreeAndInsertAggregateNodes(aTotalBytes, aT._kids[i]);
    }
    return;
  }

  // Look at all children except the last one.
  for (var i = 0; i < aT._kids.length - 1; i++) {
    if (isInsignificant(aT._kids[i])) {
      // This child is below the significance threshold.  If there are other
      // (smaller) children remaining, move them under an aggregate node.
      var i0 = i;
      var nAgg = aT._kids.length - i0;
      // Create an aggregate node.
      var aggT = new TreeNode("(" + nAgg + " tiny)");
      var aggBytes = 0;
      for ( ; i < aT._kids.length; i++) {
        aggBytes += aT._kids[i]._amount;
        aggT._kids.push(aT._kids[i]);
      }
      aggT._hideKids = true;
      aggT._amount = aggBytes;
      aggT._unsafeDescription =
        nAgg + " sub-trees that are below the " + kSignificanceThresholdPerc +
        "% significance threshold.";
      aT._kids.splice(i0, nAgg, aggT);
      aT._kids.sort(TreeNode.compare);

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
// process;  it holds the unsafePaths of any such reporters.  It is reset for
// each new process.
var gUnsafePathsWithInvalidValuesForThisProcess = [];

function genWarningText(aHasKnownHeapAllocated, aHasMozMallocUsableSize)
{
  var warningText = "";

  if (!aHasKnownHeapAllocated && !aHasMozMallocUsableSize) {
    warningText =
      "<p class='accuracyWarning'>WARNING: the 'heap-allocated' memory " +
      "reporter and the moz_malloc_usable_size() function do not work for " +
      "this platform and/or configuration.  This means that " +
      "'heap-unclassified' is zero and the 'explicit' tree shows " +
      "much less memory than it should.</p>\n\n";

  } else if (!aHasKnownHeapAllocated) {
    warningText =
      "<p class='accuracyWarning'>WARNING: the 'heap-allocated' memory " +
      "reporter does not work for this platform and/or configuration. " +
      "This means that 'heap-unclassified' is zero and the 'explicit' tree " +
      "shows less memory than it should.</p>\n\n";

  } else if (!aHasMozMallocUsableSize) {
    warningText =
      "<p class='accuracyWarning'>WARNING: the moz_malloc_usable_size() " +
      "function does not work for this platform and/or configuration. " +
      "This means that much of the heap-allocated memory is not measured " +
      "by individual memory reporters and so will fall under " +
      "'heap-unclassified'.</p>\n\n";
  }

  if (gUnsafePathsWithInvalidValuesForThisProcess.length > 0) {
    warningText +=
      "<div class='accuracyWarning'>" +
      "<p>WARNING: the following values are negative or unreasonably " +
      "large.</p>\n" +
      "<ul>";
    for (var i = 0;
         i < gUnsafePathsWithInvalidValuesForThisProcess.length;
         i++)
    {
      warningText +=
        " <li>" +
        makeSafe(gUnsafePathsWithInvalidValuesForThisProcess[i]) +
        "</li>\n";
    }
    warningText +=
      "</ul>" +
      "<p>This indicates a defect in one or more memory reporters.  The " +
      "invalid values are highlighted, but you may need to expand one " +
      "or more sub-trees to see them.</p>\n\n" +
      "</div>";
    gUnsafePathsWithInvalidValuesForThisProcess = [];  // reset for the next process
  }

  return warningText;
}

/**
 * Generates the text for a single process.
 *
 * @param aProcess
 *        The name of the process.
 * @param aReporters
 *        Table of Reporters for this process, indexed by _unsafePath.
 * @param aHasMozMallocUsableSize
 *        Boolean indicating if moz_malloc_usable_size works.
 * @return The generated text.
 */
function genProcessText(aProcess, aReporters, aHasMozMallocUsableSize)
{
  var explicitTree = buildTree(aReporters, 'explicit');
  var hasKnownHeapAllocated = fixUpExplicitTree(explicitTree, aReporters);
  sortTreeAndInsertAggregateNodes(explicitTree._amount, explicitTree);
  var explicitText = genTreeText(explicitTree, aProcess);

  // We only show these breakdown trees in verbose mode.
  var mapTreeText = "";
  kMapTreePaths.forEach(function(t) {
    if (gVerbose) {
      var tree = buildTree(aReporters, t);

      // |tree| will be null if we don't have any reporters for the given
      // unsafePath.
      if (tree) {
        sortTreeAndInsertAggregateNodes(tree._amount, tree);
        tree._hideKids = true;   // map trees are always initially collapsed
        mapTreeText += genTreeText(tree, aProcess);
      }
    } else {
      ignoreTree(aReporters, t);
    }
  });

  // We have to call genOtherText after we process all the trees, because it
  // looks at all the reporters which aren't part of a tree.
  var otherText = genOtherText(aReporters, aProcess);

  // Generate any warnings about inaccuracies due to platform limitations.
  // This must come after generating all the text.  The newlines give nice
  // spacing if we cut+paste into a text buffer.
  var warningText = "";
  var warningText =
        genWarningText(hasKnownHeapAllocated, aHasMozMallocUsableSize);

  // The newlines give nice spacing if we cut+paste into a text buffer.
  return "<h1>" + aProcess + " Process</h1>\n\n" +
         warningText + explicitText + mapTreeText + otherText +
         "<hr></hr>";
}

/**
 * Determines if a number has a negative sign when converted to a string.
 * Works even for -0.
 *
 * @param aN
 *        The number.
 * @return A boolean.
 */
function hasNegativeSign(aN)
{
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
 * @return A human-readable string representing the int.
 */
function formatInt(aN)
{
  var neg = false;
  if (hasNegativeSign(aN)) {
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
    // If the argument to formatInt() is -0, it will print the negative sign.
    s = formatInt(Number(a[0])) + "." + a[1] + " " + unit;
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
 *        Table of Reporters for this process, indexed by _unsafePath.
 * @param aUnsafePath
 *        The unsafePath of the R.
 * @param aDoNotMark
 *        If true, the _done property is not set.
 * @return The byte count.
 */
function getBytes(aReporters, aUnsafePath, aDoNotMark)
{
  var r = aReporters[aUnsafePath];
  assert(r, "getBytes: no such Reporter: " + makeSafe(aUnsafePath));
  if (!aDoNotMark) {
    r._done = true;
  }
  return r._amount;
}

/**
 * Gets the (unsafe) description for a particular Reporter.
 *
 * @param aReporters
 *        Table of Reporters for this process, indexed by _unsafePath.
 * @param aUnsafePath
 *        The unsafePath of the Reporter.
 * @return The description.
 */
function getUnsafeDescription(aReporters, aUnsafePath)
{
  var r = aReporters[aUnsafePath];
  assert(r, "getUnsafeDescription: no such Reporter: " + makeSafe(aUnsafePath));
  return r._unsafeDescription;
}

// There's a subset of the Unicode "light" box-drawing chars that are widely
// implemented in terminals, and this code sticks to that subset to maximize
// the chance that cutting and pasting about:memory output to a terminal will
// work correctly:
const kHorizontal       = "\u2500",
      kVertical         = "\u2502",
      kUpAndRight       = "\u2514",
      kVerticalAndRight = "\u251c";

function genMrValueText(aValue, aIsInvalid)
{
  return aIsInvalid ?
         "<span class='mrValue invalid'>" + aValue + "</span>" :
         "<span class='mrValue'>"         + aValue + "</span>";
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

function genMrNameText(aKind, aShowSubtrees, aHasKids, aUnsafeDesc,
                       aUnsafeName, aIsUnknown, aIsInvalid, aNMerged)
{
  var text = "";
  if (aHasKids) {
    if (aShowSubtrees) {
      text += "<span class='mrSep hidden'> ++ </span>";
      text += "<span class='mrSep'> -- </span>";
    } else {
      text += "<span class='mrSep'> ++ </span>";
      text += "<span class='mrSep hidden'> -- </span>";
    }
  } else {
    text += "<span class='mrSep'> " + kHorizontal + kHorizontal + " </span>";
  }
  text += "<span class='mrName' title='" +
          kindToString(aKind) + makeSafe(aUnsafeDesc) + "'>" +
          makeSafe(aUnsafeName) + "</span>";
  if (aIsUnknown) {
    const problemDesc =
      "Warning: this memory reporter was unable to compute a useful value. ";
    text += "<span class='mrNote' title=\"" + problemDesc + "\"> [*]</span>";
  }
  if (aIsInvalid) {
    const invalidDesc =
      "Warning: this value is invalid and indicates a bug in one or more " +
      "memory reporters. ";
    text += "<span class='mrNote' title=\"" + invalidDesc + "\"> [?!]</span>";
  }
  if (aNMerged) {
    const dupDesc = "This value is the sum of " + aNMerged +
                    " memory reporters that all have the same path.";
    text += "<span class='mrNote' title=\"" + dupDesc + "\"> [" +
            aNMerged + "]</span>";
  }
  return text + '\n';
}

// This is used to record the (safe) IDs of which sub-trees have been toggled,
// so the collapsed/expanded state can be replicated when the page is
// regenerated.  It can end up holding IDs of nodes that no longer exist, e.g.
// for compartments that have been closed.  This doesn't seem like a big deal,
// because the number is limited by the number of entries the user has changed
// from their original state.
var gTogglesBySafeTreeId = {};

function toggle(aEvent)
{
  // This relies on each line being a span that contains at least five spans:
  // mrValue, mrPerc, mrSep ('++'), mrSep ('--'), mrName, and then zero or more
  // mrNotes.  All whitespace must be within one of these spans for this
  // function to find the right nodes.  And the span containing the children of
  // this line must immediately follow.  Assertions check this.

  function assertClassName(span, className) {
    assert(span, "undefined " + className);
    assert(span.nodeName === "span", "non-span " + className);
    assert(span.classList.contains(className), "bad " + className);
  }

  // |aEvent.target| will be one of the five spans.  Get the outer span.
  var outerSpan = aEvent.target.parentNode;
  assertClassName(outerSpan, "hasKids");

  // Toggle visibility of the '++' and '--' separators.
  var plusSpan  = outerSpan.childNodes[2];
  var minusSpan = outerSpan.childNodes[3];
  assertClassName(plusSpan,  "mrSep");
  assertClassName(minusSpan, "mrSep");
  plusSpan .classList.toggle("hidden");
  minusSpan.classList.toggle("hidden");

  // Toggle visibility of the span containing this node's children.
  var subTreeSpan = outerSpan.nextSibling;
  assertClassName(subTreeSpan, "kids");
  subTreeSpan.classList.toggle("hidden");

  // Record/unrecord that this sub-tree was toggled.
  var safeTreeId = outerSpan.id;
  if (gTogglesBySafeTreeId[safeTreeId]) {
    delete gTogglesBySafeTreeId[safeTreeId];
  } else {
    gTogglesBySafeTreeId[safeTreeId] = true;
  }
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
  var isExplicitTree = aT._unsafeName == 'explicit';

  /**
   * Generates the text for a particular tree, without a heading.
   *
   * @param aUnsafePrePath
   *        The partial unsafePath leading up to this node.
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
  function genTreeText2(aUnsafePrePath, aT, aIndentGuide, aParentStringLength)
  {
    function repeatStr(aC, aN)
    {
      var s = "";
      for (var i = 0; i < aN; i++) {
        s += aC;
      }
      return s;
    }

    // Determine if we should show the sub-tree below this entry;  this
    // involves reinstating any previous toggling of the sub-tree.
    var unsafePath = aUnsafePrePath + aT._unsafeName;
    var safeTreeId = escapeAll(aProcess + ":" + unsafePath);
    var showSubtrees = !aT._hideKids;
    if (gTogglesBySafeTreeId[safeTreeId]) {
      showSubtrees = !showSubtrees;
    }

    // Generate the indent.
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

    // Generate the percentage;  detect and record invalid values at the same
    // time.
    var percText = "";
    var tIsInvalid = false;
    if (aT._amount === treeBytes) {
      percText = "100.0";
    } else {
      var perc = (100 * aT._amount / treeBytes);
      if (!(0 <= perc && perc <= 100)) {
        tIsInvalid = true;
        gUnsafePathsWithInvalidValuesForThisProcess.push(unsafePath);
      }
      percText = (100 * aT._amount / treeBytes).toFixed(2);
      percText = pad(percText, 5, '0');
    }
    percText = tIsInvalid ?
               "<span class='mrPerc invalid'> (" + percText + "%)</span>" :
               "<span class='mrPerc'> ("         + percText + "%)</span>";

    // We don't want to show '(nonheap)' on a tree like 'map/vsize', since the
    // whole tree is non-heap.
    var kind = isExplicitTree ? aT._kind : undefined;

    // For non-leaf nodes, the entire sub-tree is put within a span so it can
    // be collapsed if the node is clicked on.
    var hasKids = aT._kids.length > 0;
    if (!hasKids) {
      assert(!aT._hideKids, "leaf node with _hideKids set")
    }
    var text = indent;
    if (hasKids) {
      text += "<span onclick='toggle(event)' class='hasKids' id='" +
              safeTreeId + "'>";
    }
    text += genMrValueText(tString, tIsInvalid) + percText;
    text += genMrNameText(kind, showSubtrees, hasKids, aT._unsafeDescription,
                          aT._unsafeName, aT._isUnknown, tIsInvalid,
                          aT._nMerged);
    if (hasKids) {
      var hiddenText = showSubtrees ? "" : " hidden";
      // The 'kids' class is just used for sanity checking in toggle().
      text += "</span><span class='kids" + hiddenText + "'>";
    }

    for (var i = 0; i < aT._kids.length; i++) {
      // 3 is the standard depth, the callee adjusts it if necessary.
      aIndentGuide.push({ _isLastKid: (i === aT._kids.length - 1), _depth: 3 });
      text += genTreeText2(unsafePath + "/", aT._kids[i], aIndentGuide,
                           tString.length);
      aIndentGuide.pop();
    }
    text += hasKids ? "</span>" : "";
    return text;
  }

  var text = genTreeText2(/* prePath = */"", aT, [], rootStringLength);

  return genSectionMarkup(aT._unsafeName, text);
}

function OtherReporter(aUnsafePath, aUnits, aAmount, aUnsafeDesc, aNMerged)
{
  // Nb: _kind is not needed, it's always KIND_OTHER.
  this._unsafePath = aUnsafePath;
  this._units    = aUnits;
  if (aAmount === kUnknown) {
    this._amount     = 0;
    this._isUnknown = true;
  } else {
    this._amount = aAmount;
  }
  this._unsafeDescription = aUnsafeDesc;
  this._asString = this.toString();
}

OtherReporter.prototype = {
  toString: function() {
    switch (this._units) {
      case UNITS_BYTES:            return formatBytes(this._amount);
      case UNITS_COUNT:
      case UNITS_COUNT_CUMULATIVE: return formatInt(this._amount);
      case UNITS_PERCENTAGE:       return formatPercentage(this._amount);
      default:
        assert(false, "bad units in OtherReporter.toString");
    }
  },

  isInvalid: function() {
    var n = this._amount;
    switch (this._units) {
      case UNITS_BYTES:
      case UNITS_COUNT:
      case UNITS_COUNT_CUMULATIVE: return (n !== kUnknown && n < 0);
      case UNITS_PERCENTAGE:       return (n !== kUnknown &&
                                           !(0 <= n && n <= 10000));
      default:
        assert(false, "bad units in OtherReporter.isInvalid");
    }
  }
};

OtherReporter.compare = function(a, b) {
  return a._unsafePath < b._unsafePath ? -1 :
         a._unsafePath > b._unsafePath ?  1 :
         0;
};

/**
 * Generates the text for the "Other Measurements" section.
 *
 * @param aReportersByProcess
 *        Table of Reporters for this process, indexed by _unsafePath.
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
  for (var unsafePath in aReportersByProcess) {
    var r = aReportersByProcess[unsafePath];
    if (!r._done) {
      assert(r._kind === KIND_OTHER,
             "_kind !== KIND_OTHER for " + makeSafe(r._unsafePath));
      assert(r._nMerged === undefined);  // we don't allow dup'd OTHER reporters
      var o = new OtherReporter(r._unsafePath, r._units, r._amount,
                                r._unsafeDescription);
      otherReporters.push(o);
      if (o._asString.length > maxStringLength) {
        maxStringLength = o._asString.length;
      }
    }
  }
  otherReporters.sort(OtherReporter.compare);

  // Generate text for the not-yet-printed values.
  var text = "";
  for (var i = 0; i < otherReporters.length; i++) {
    var o = otherReporters[i];
    var oIsInvalid = o.isInvalid();
    if (oIsInvalid) {
      gUnsafePathsWithInvalidValuesForThisProcess.push(o._unsafePath);
    }
    text += genMrValueText(pad(o._asString, maxStringLength, ' '), oIsInvalid);
    text += genMrNameText(KIND_OTHER, /* showSubtrees = */true,
                          /* hasKids = */false, o._unsafeDescription,
                          o._unsafePath, o._isUnknown, oIsInvalid);
  }

  return genSectionMarkup('other', text);
}

function genSectionMarkup(aName, aText)
{
  return "<h2 class='sectionHeader'>" + kTreeNames[aName] + "</h2>\n" +
         "<pre class='tree'>" + aText + "</pre>\n";
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
