"use strict";

let { Ci, Cu } = require("chrome");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(
  this, "wdm",
  "@mozilla.org/dom/workers/workerdebuggermanager;1",
  "nsIWorkerDebuggerManager"
);

function matchWorkerDebugger(dbg, options) {
  if ("window" in options) {
    let window = dbg.window;
    while (window !== null && window.parent !== window) {
      window = window.parent;
    }

    if (window !== options.window) {
      return false;
    }
  }

  return true;
}

function WorkerActor(dbg) {
  this._dbg = dbg;
}

WorkerActor.prototype = {
  actorPrefix: "worker",

  form: function () {
    return {
      actor: this.actorID,
      url: this._dbg.url
    };
  }
};

exports.WorkerActor = WorkerActor;

function WorkerActorList(options) {
  this._options = options;
  this._actors = new Map();
  this._onListChanged = null;
  this._mustNotify = false;
  this.onRegister = this.onRegister.bind(this);
  this.onUnregister = this.onUnregister.bind(this);
}

WorkerActorList.prototype = {
  getList: function () {
    let dbgs = new Set();
    let e = wdm.getWorkerDebuggerEnumerator();
    while (e.hasMoreElements()) {
      let dbg = e.getNext().QueryInterface(Ci.nsIWorkerDebugger);
      if (matchWorkerDebugger(dbg, this._options)) {
        dbgs.add(dbg);
      }
    }

    for (let [dbg, ] of this._actors) {
      if (!dbgs.has(dbg)) {
        this._actors.delete(dbg);
      }
    }

    for (let dbg of dbgs) {
      if (!this._actors.has(dbg)) {
        this._actors.set(dbg, new WorkerActor(dbg));
      }
    }

    let actors = [];
    for (let [, actor] of this._actors) {
      actors.push(actor);
    }

    this._mustNotify = true;
    this._checkListening();

    return Promise.resolve(actors);
  },

  get onListChanged() {
    return this._onListChanged;
  },

  set onListChanged(onListChanged) {
    if (typeof onListChanged !== "function" && onListChanged !== null) {
      throw new Error("onListChanged must be either a function or null.");
    }

    this._onListChanged = onListChanged;
    this._checkListening();
  },

  _checkListening: function () {
    if (this._onListChanged !== null && this._mustNotify) {
      wdm.addListener(this);
    } else {
      wdm.removeListener(this);
    }
  },

  _notifyListChanged: function () {
    if (this._mustNotify) {
      this._onListChanged();
      this._mustNotify = false;
    }
  },

  onRegister: function (dbg) {
    if (matchWorkerDebugger(dbg, this._options)) {
      this._notifyListChanged();
    }
  },

  onUnregister: function (dbg) {
    if (matchWorkerDebugger(dbg, this._options)) {
      this._notifyListChanged();
    }
  }
};

exports.WorkerActorList = WorkerActorList;
