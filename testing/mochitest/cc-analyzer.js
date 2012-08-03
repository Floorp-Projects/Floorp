/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function CCAnalyzer() {
}

CCAnalyzer.prototype = {
  clear: function () {
    this.callback = null;
    this.processingCount = 0;
    this.graph = {};
    this.roots = [];
    this.garbage = [];
    this.edges = [];
    this.listener = null;
  },

  run: function (aCallback) {
    this.clear();
    this.callback = aCallback;

    this.listener = Cc["@mozilla.org/cycle-collector-logger;1"].
      createInstance(Ci.nsICycleCollectorListener);

    this.listener.disableLog = true;
    this.listener.wantAfterProcessing = true;

    this.runCC(3);
  },

  runCC: function (aCounter) {
    let utils = window.QueryInterface(Ci.nsIInterfaceRequestor).
        getInterface(Ci.nsIDOMWindowUtils);

    if (aCounter > 1) {
      utils.garbageCollect();
      setTimeout(this.runCC.bind(this, aCounter - 1), 0);
    } else {
      utils.garbageCollect(this.listener);
      this.processLog();
    }
  },

  processLog: function () {
    // Process entire heap step by step in 5K chunks
    for (let i = 0; i < 5000; i++) {
      if (!this.listener.processNext(this)) {
        this.callback();
        this.clear();
        return;
      }
    }

    // Next chunk on timeout.
    setTimeout(this.processLog.bind(this), 0);
  },

  noteRefCountedObject: function (aAddress, aRefCount, aObjectDescription) {
    let o = this.ensureObject(aAddress);
    o.address = aAddress;
    o.refcount = aRefCount;
    o.name = aObjectDescription;
  },

  noteGCedObject: function (aAddress, aMarked, aObjectDescription) {
    let o = this.ensureObject(aAddress);
    o.address = aAddress;
    o.gcmarked = aMarked;
    o.name = aObjectDescription;
  },

  noteEdge: function (aFromAddress, aToAddress, aEdgeName) {
    let fromObject = this.ensureObject(aFromAddress);
    let toObject = this.ensureObject(aToAddress);
    fromObject.edges.push({name: aEdgeName, to: toObject});
    toObject.owners.push({name: aEdgeName, from: fromObject});

    this.edges.push({
      name: aEdgeName,
      from: fromObject,
      to: toObject
    });
  },

  describeRoot: function (aAddress, aKnownEdges) {
    let o = this.ensureObject(aAddress);
    o.root = true;
    o.knownEdges = aKnownEdges;
    this.roots.push(o);
  },

  describeGarbage: function (aAddress) {
    let o = this.ensureObject(aAddress);
    o.garbage = true;
    this.garbage.push(o);
  },

  ensureObject: function (aAddress) {
    if (!this.graph[aAddress])
      this.graph[aAddress] = new CCObject();

    return this.graph[aAddress];
  },

  find: function (aText) {
    let result = [];
    for each (let o in this.graph) {
      if (!o.garbage && o.name.indexOf(aText) >= 0)
        result.push(o);
    }
    return result;
  }
};

function CCObject() {
  this.name = "";
  this.address = null;
  this.refcount = 0;
  this.gcmarked = false;
  this.root = false;
  this.garbage = false;
  this.knownEdges = 0;
  this.edges = [];
  this.owners = [];
}
