/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// the singleton frameworker, available for (ab)use by tests.
let frameworker;

(function () { // bug 673569 workaround :(

/*
 * This is an implementation of a "Shared Worker" using a remote <browser>
 * element hosted in the hidden DOM window.  This is the "content script"
 * implementation - it runs in the child process but has chrome permissions.
 *
 * A set of new APIs that simulate a shared worker are introduced to a sandbox
 * by cloning methods from the worker's JS origin.
 */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/MessagePortBase.jsm");

function navigate(url) {
  let webnav = docShell.QueryInterface(Ci.nsIWebNavigation);
  webnav.loadURI(url, Ci.nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
}

/**
 * FrameWorker
 *
 * A FrameWorker is a <browser> element hosted by the hiddenWindow.
 * It is constructed with the URL of some JavaScript that will be run in
 * the context of the browser; the script does not have a full DOM but is
 * instead run in a sandbox that has a select set of methods cloned from the
 * URL's domain.
 */
function FrameWorker(url, name, origin, exposeLocalStorage) {
  this.url = url;
  this.name = name || url;
  this.ports = new Map(); // all unclosed ports, including ones yet to be entangled
  this.loaded = false;
  this.origin = origin;
  this._injectController = null;
  this.exposeLocalStorage = exposeLocalStorage;

  this.load();
}

FrameWorker.prototype = {
  load: function FrameWorker_loadWorker() {
    this._injectController = function(doc, topic, data) {
      if (!doc.defaultView || doc.defaultView != content) {
        return;
      }
      this._maybeRemoveInjectController();
      try {
        this.createSandbox();
      } catch (e) {
        Cu.reportError("FrameWorker: failed to create sandbox for " + this.url + ". " + e);
      }
    }.bind(this);

    Services.obs.addObserver(this._injectController, "document-element-inserted", false);
    navigate(this.url);
  },

  _maybeRemoveInjectController: function() {
    if (this._injectController) {
      Services.obs.removeObserver(this._injectController, "document-element-inserted");
      this._injectController = null;
    }
  },

  createSandbox: function createSandbox() {
    let workerWindow = content;
    let sandbox = new Cu.Sandbox(workerWindow);

    // copy the window apis onto the sandbox namespace only functions or
    // objects that are naturally a part of an iframe, I'm assuming they are
    // safe to import this way
    let workerAPI = ['WebSocket', 'atob', 'btoa',
                     'clearInterval', 'clearTimeout', 'dump',
                     'setInterval', 'setTimeout', 'XMLHttpRequest',
                     'FileReader', 'Blob', 'EventSource', 'indexedDB',
                     'location', 'Worker'];

    // Only expose localStorage if the caller opted-in
    if (this.exposeLocalStorage) {
      workerAPI.push('localStorage');
    }

    // Bug 798660 - XHR, WebSocket and Worker have issues in a sandbox and need
    // to be unwrapped to work
    let needsWaive = ['XMLHttpRequest', 'WebSocket', 'Worker' ];
    // Methods need to be bound with the proper |this|.
    let needsBind = ['atob', 'btoa', 'dump', 'setInterval', 'clearInterval',
                     'setTimeout', 'clearTimeout'];
    workerAPI.forEach(function(fn) {
      try {
        if (needsWaive.indexOf(fn) != -1)
          sandbox[fn] = XPCNativeWrapper.unwrap(workerWindow)[fn];
        else if (needsBind.indexOf(fn) != -1)
          sandbox[fn] = workerWindow[fn].bind(workerWindow);
        else
          sandbox[fn] = workerWindow[fn];
      }
      catch(e) {
        Cu.reportError("FrameWorker: failed to import API "+fn+"\n"+e+"\n");
      }
    });
    // the "navigator" object in a worker is a subset of the full navigator;
    // specifically, just the interfaces 'NavigatorID' and 'NavigatorOnLine'
    let navigator = {
      __exposedProps__: {
        "appName": "r",
        "appVersion": "r",
        "platform": "r",
        "userAgent": "r",
        "onLine": "r"
      },
      // interface NavigatorID
      appName: workerWindow.navigator.appName,
      appVersion: workerWindow.navigator.appVersion,
      platform: workerWindow.navigator.platform,
      userAgent: workerWindow.navigator.userAgent,
      // interface NavigatorOnLine
      get onLine() workerWindow.navigator.onLine
    };
    sandbox.navigator = navigator;

    // Our importScripts function needs to 'eval' the script code from inside
    // a function, but using eval() directly means functions in the script
    // don't end up in the global scope.
    sandbox._evalInSandbox = function(s, url) {
      let baseURI = Services.io.newURI(workerWindow.location.href, null, null);
      Cu.evalInSandbox(s, sandbox, "1.8",
                       Services.io.newURI(url, null, baseURI).spec, 1);
    };

    // and we delegate ononline and onoffline events to the worker.
    // See http://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#workerglobalscope
    workerWindow.addEventListener('offline', function fw_onoffline(event) {
      Cu.evalInSandbox("onoffline();", sandbox);
    }, false);
    workerWindow.addEventListener('online', function fw_ononline(event) {
      Cu.evalInSandbox("ononline();", sandbox);
    }, false);

    sandbox._postMessage = function fw_postMessage(d, o) {
      workerWindow.postMessage(d, o)
    };
    sandbox._addEventListener = function fw_addEventListener(t, l, c) {
      workerWindow.addEventListener(t, l, c)
    };

    // Note we don't need to stash |sandbox| in |this| as the unload handler
    // has a reference in its closure, so it can't die until that handler is
    // removed - at which time we've explicitly killed it anyway.
    let worker = this;

    workerWindow.addEventListener("DOMContentLoaded", function loadListener() {
      workerWindow.removeEventListener("DOMContentLoaded", loadListener);

      // no script, error out now rather than creating ports, etc
      let scriptText = workerWindow.document.body.textContent.trim();
      if (!scriptText) {
        Cu.reportError("FrameWorker: Empty worker script received");
        notifyWorkerError();
        return;
      }

      // now that we've got the script text, remove it from the DOM;
      // no need for it to keep occupying memory there
      workerWindow.document.body.textContent = "";

      // the content has loaded the js file as text - first inject the magic
      // port-handling code into the sandbox.
      try {
        Services.scriptloader.loadSubScript("resource://gre/modules/MessagePortBase.jsm", sandbox);
        Services.scriptloader.loadSubScript("resource://gre/modules/MessagePortWorker.js", sandbox);
      }
      catch (e) {
        Cu.reportError("FrameWorker: Error injecting port code into content side of the worker: " + e + "\n" + e.stack);
        notifyWorkerError();
        return;
      }

      // and wire up the client message handling.
      try {
        initClientMessageHandler();
      }
      catch (e) {
        Cu.reportError("FrameWorker: Error setting up event listener for chrome side of the worker: " + e + "\n" + e.stack);
        notifyWorkerError();
        return;
      }

      // Now get the worker js code and eval it into the sandbox
      try {
        Cu.evalInSandbox(scriptText, sandbox, "1.8", workerWindow.location.href, 1);
      } catch (e) {
        Cu.reportError("FrameWorker: Error evaluating worker script for " + worker.name + ": " + e + "; " +
            (e.lineNumber ? ("Line #" + e.lineNumber) : "") +
            (e.stack ? ("\n" + e.stack) : ""));
        notifyWorkerError();
        return;
      }

      // so finally we are ready to roll - dequeue all the pending connects
      worker.loaded = true;
      for (let [,port] of worker.ports) { // enumeration is in insertion order
        if (!port._entangled) {
          try {
            port._createWorkerAndEntangle(worker);
          }
          catch(e) {
            Cu.reportError("FrameWorker: Failed to entangle worker port: " + e + "\n" + e.stack);
          }
        }
      }
    });

    // the 'unload' listener cleans up the worker and the sandbox.  This
    // will be triggered by the window unloading as part of shutdown or reload.
    workerWindow.addEventListener("unload", function unloadListener() {
      workerWindow.removeEventListener("unload", unloadListener);
      worker.loaded = false;
      // No need to close ports - the worker probably wont see a
      // social.port-closing message and certainly isn't going to have time to
      // do anything if it did see it.
      worker.ports.clear();
      if (sandbox) {
        Cu.nukeSandbox(sandbox);
        sandbox = null;
      }
    });
  },
};

const FrameWorkerManager = {
  init: function() {
    // first, setup the docShell to disable some types of content
    docShell.allowAuth = false;
    docShell.allowPlugins = false;
    docShell.allowImages = false;
    docShell.allowMedia = false;
    docShell.allowWindowControl = false;

    addMessageListener("frameworker:init", this._onInit);
    addMessageListener("frameworker:connect", this._onConnect);
    addMessageListener("frameworker:port-message", this._onPortMessage);
    addMessageListener("frameworker:cookie-get", this._onCookieGet);
  },

  // This new frameworker is being created.  This should only be called once.
  _onInit: function(msg) {
    let {url, name, origin, exposeLocalStorage} = msg.data;
    frameworker = new FrameWorker(url, name, origin, exposeLocalStorage);
  },

  // A new port is being established for this frameworker.
  _onConnect: function(msg) {
    let port = new ClientPort(msg.data.portId);
    frameworker.ports.set(msg.data.portId, port);
    if (frameworker.loaded && !frameworker.reloading)
      port._createWorkerAndEntangle(frameworker);
  },

  // A message related to a port.
  _onPortMessage: function(msg) {
    // find the "client" port for this message and have it post it into
    // the worker.
    let port = frameworker.ports.get(msg.data.portId);
    port._dopost(msg.data);
  },

  _onCookieGet: function(msg) {
    sendAsyncMessage("frameworker:cookie-get-response", content.document.cookie);
  },

};

FrameWorkerManager.init();

// This is the message listener for the chrome side of the world - ie, the
// port that exists with chrome permissions inside the <browser/> (ie, in the
// content process if a remote browser is used).
function initClientMessageHandler() {
  function _messageHandler(event) {
    // We will ignore all messages destined for otherType.
    let data = event.data;
    let portid = data.portId;
    let port;
    if (!data.portFromType || data.portFromType !== "worker") {
      // this is a message posted by ourself so ignore it.
      return;
    }
    switch (data.portTopic) {
      // No "port-create" here - client ports are created explicitly.
      case "port-connection-error":
        // onconnect failed, we cannot connect the port, the worker has
        // become invalid
        notifyWorkerError();
        break;
      case "port-close":
        // the worker side of the port was closed, so close this side too.
        port = frameworker.ports.get(portid);
        if (!port) {
          // port already closed (which will happen when we call port.close()
          // below - the worker side will send us this message but we've
          // already closed it.)
          return;
        }
        frameworker.ports.delete(portid);
        port.close();
        break;

      case "port-message":
        // the client posted a message to this worker port.
        port = frameworker.ports.get(portid);
        if (!port) {
          return;
        }
        port._onmessage(data.data);
        break;

      default:
        break;
    }
  }
  // this can probably go once debugged and working correctly!
  function messageHandler(event) {
    try {
      _messageHandler(event);
    } catch (ex) {
      Cu.reportError("FrameWorker: Error handling client port control message: " + ex + "\n" + ex.stack);
    }
  }
  content.addEventListener('message', messageHandler);
}

/**
 * ClientPort
 *
 * Client side of the entangled ports. This is just a shim that sends messages
 * back to the "parent" port living in the chrome process.
 *
 * constructor:
 * @param {integer} portid
 */
function ClientPort(portid) {
  // messages posted to the worker before the worker has loaded.
  this._pendingMessagesOutgoing = [];
  AbstractPort.call(this, portid);
}

ClientPort.prototype = {
  __proto__: AbstractPort.prototype,
  _portType: "client",
  // _entangled records if the port has ever been entangled (although may be
  // reset during a reload).
  _entangled: false,

  _createWorkerAndEntangle: function fw_ClientPort_createWorkerAndEntangle(worker) {
    this._entangled = true;
    this._postControlMessage("port-create");
    for (let message of this._pendingMessagesOutgoing) {
      this._dopost(message);
    }
    this._pendingMessagesOutgoing = [];
    // The client side of the port might have been closed before it was
    // "entangled" with the worker, in which case we need to disentangle it
    if (this._closed) {
      worker.ports.delete(this._portid);
    }
  },

  _dopost: function fw_ClientPort_dopost(data) {
    if (!this._entangled) {
      this._pendingMessagesOutgoing.push(data);
    } else {
      content.postMessage(data, "*");
    }
  },

  // we are just a "shim" - any messages we get are just forwarded back to
  // the chrome parent process.
  _onmessage: function(data) {
    sendAsyncMessage("frameworker:port-message", {portId: this._portid, data: data});
  },

  _onerror: function fw_ClientPort_onerror(err) {
    Cu.reportError("FrameWorker: Port " + this + " handler failed: " + err + "\n" + err.stack);
  },

  close: function fw_ClientPort_close() {
    if (this._closed) {
      return; // already closed.
    }
    // a leaky abstraction due to the worker spec not specifying how the
    // other end of a port knows it is closing.
    this.postMessage({topic: "social.port-closing"});
    AbstractPort.prototype.close.call(this);
    // this._pendingMessagesOutgoing should still be drained, as a closed
    // port will still get "entangled" quickly enough to deliver the messages.
  }
}

function notifyWorkerError() {
  sendAsyncMessage("frameworker:notify-worker-error", {origin: frameworker.origin});
}

}());
