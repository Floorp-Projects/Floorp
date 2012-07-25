/* -*- Mode: JavaScript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * This is an implementation of a "Shared Worker" using an iframe in the
 * hidden DOM window.  A subset of new APIs are introduced to the window
 * by cloning methods from the worker's JS origin.
 */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/MessagePortBase.jsm");

const EXPORTED_SYMBOLS = ["getFrameWorkerHandle"];

var workerCache = {}; // keyed by URL.
var _nextPortId = 1;

// Retrieves a reference to a WorkerHandle associated with a FrameWorker and a
// new ClientPort.
function getFrameWorkerHandle(url, clientWindow, name) {
  // first create the client port we are going to use.  Later we will
  // message the worker to create the worker port.
  let portid = _nextPortId++;
  let clientPort = new ClientPort(portid, clientWindow);

  let existingWorker = workerCache[url];
  if (!existingWorker) {
    // setup the worker and add this connection to the pending queue
    let worker = new FrameWorker(url, name);
    worker.pendingPorts.push(clientPort);
    existingWorker = workerCache[url] = worker;
  } else {
    // already have a worker - either queue or make the connection.
    if (existingWorker.loaded) {
      try {
        clientPort._createWorkerAndEntangle(existingWorker);
      }
      catch (ex) {
        Cu.reportError("FrameWorker: Failed to connect a port: " + e + "\n" + e.stack);
      }
    } else {
      existingWorker.pendingPorts.push(clientPort);
    }
  }

  // return the pseudo worker object.
  return new WorkerHandle(clientPort, existingWorker);
};

/**
 * FrameWorker
 *
 * A FrameWorker is an iframe that is attached to the hiddenWindow,
 * which contains a pair of MessagePorts.  It is constructed with the
 * URL of some JavaScript that will be run in the context of the window;
 * the script does not have a full DOM but is instead run in a sandbox
 * that has a select set of methods cloned from the URL's domain.
 */
function FrameWorker(url, name) {
  this.url = url;
  this.name = name || url;
  this.ports = {};
  this.pendingPorts = [];
  this.loaded = false;

  this.frame = makeHiddenFrame();

  var self = this;
  Services.obs.addObserver(function injectController(doc, topic, data) {
    if (!doc.defaultView || doc.defaultView != self.frame.contentWindow) {
      return;
    }
    Services.obs.removeObserver(injectController, "document-element-inserted", false);
    try {
      self.createSandbox();
    } catch (e) {
      Cu.reportError("FrameWorker: failed to create sandbox for " + url + ". " + e);
    }
  }, "document-element-inserted", false);

  this.frame.setAttribute("src", url);
}

FrameWorker.prototype = {
  createSandbox: function createSandbox() {
    let workerWindow = this.frame.contentWindow;
    let sandbox = new Cu.Sandbox(workerWindow);

    // copy the window apis onto the sandbox namespace only functions or
    // objects that are naturally a part of an iframe, I'm assuming they are
    // safe to import this way
    let workerAPI = ['MozWebSocket', 'WebSocket', 'localStorage',
                     'atob', 'btoa', 'clearInterval', 'clearTimeout', 'dump',
                     'setInterval', 'setTimeout', 'XMLHttpRequest',
                     'MozBlobBuilder', 'FileReader', 'Blob',
                     'location'];
    workerAPI.forEach(function(fn) {
      try {
        // XXX Need to unwrap for this to work - find out why!
        sandbox[fn] = XPCNativeWrapper.unwrap(workerWindow)[fn];
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

    // and we delegate ononline and onoffline events to the worker.
    // See http://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#workerglobalscope
    this.frame.addEventListener('offline', function fw_onoffline(event) {
      Cu.evalInSandbox("onoffline();", sandbox);
    }, false);
    this.frame.addEventListener('online', function fw_ononline(event) {
      Cu.evalInSandbox("ononline();", sandbox);
    }, false);

    sandbox._postMessage = function fw_postMessage(d, o) {
      workerWindow.postMessage(d, o)
    };
    sandbox._addEventListener = function fw_addEventListener(t, l, c) {
      workerWindow.addEventListener(t, l, c)
    };

    // And a very hacky work-around for bug 734215
    sandbox.bufferToArrayHack = function fw_bufferToArrayHack(a) {
      return new workerWindow.Uint8Array(a);
    };

    this.sandbox = sandbox;

    let worker = this;

    workerWindow.addEventListener("load", function loadListener() {
      workerWindow.removeEventListener("load", loadListener);
      // the iframe has loaded the js file as text - first inject the magic
      // port-handling code into the sandbox.
      try {
        Services.scriptloader.loadSubScript("resource://gre/modules/MessagePortBase.jsm", sandbox);
        Services.scriptloader.loadSubScript("resource://gre/modules/MessagePortWorker.js", sandbox);
      }
      catch (e) {
        Cu.reportError("FrameWorker: Error injecting port code into content side of the worker: " + e + "\n" + e.stack);
      }

      // and wire up the client message handling.
      try {
        initClientMessageHandler(worker, workerWindow);
      }
      catch (e) {
        Cu.reportError("FrameWorker: Error setting up event listener for chrome side of the worker: " + e + "\n" + e.stack);
      }

      // Now get the worker js code and eval it into the sandbox
      try {
        let scriptText = workerWindow.document.body.textContent;
        Cu.evalInSandbox(scriptText, sandbox, "1.8", workerWindow.location.href, 1);
      } catch (e) {
        Cu.reportError("FrameWorker: Error evaluating worker script for " + worker.name + ": " + e + "; " +
            (e.lineNumber ? ("Line #" + e.lineNumber) : "") +
            (e.stack ? ("\n" + e.stack) : ""));
        return;
      }

      // so finally we are ready to roll - dequeue all the pending connects
      worker.loaded = true;

      let pending = worker.pendingPorts;
      while (pending.length) {
        let port = pending.shift();
        if (port._portid) { // may have already been closed!
          try {
            port._createWorkerAndEntangle(worker);
          }
          catch(e) {
            Cu.reportError("FrameWorker: Failed to create worker port: " + e + "\n" + e.stack);
          }
        }
      }
    });
  },

  terminate: function terminate() {
    // closing the port also removes it from this.ports via port-close
    for (let [portid, port] in Iterator(this.ports)) {
      // port may have been closed as a side-effect from closing another port
      if (!port)
        continue;
      try {
        port.close();
      } catch (ex) {
        Cu.reportError("FrameWorker: failed to close port. " + ex);
      }
    }

    delete workerCache[this.url];

    // let pending events get delivered before actually removing the frame
    Services.tm.mainThread.dispatch(function deleteWorkerFrame() {
      // now nuke the iframe itself and forget everything about this worker.
      this.frame.parentNode.removeChild(this.frame);
    }.bind(this), Ci.nsIThread.DISPATCH_NORMAL);
  }
};

function makeHiddenFrame() {
  let hiddenDoc = Services.appShell.hiddenDOMWindow.document;
  let iframe = hiddenDoc.createElementNS("http://www.w3.org/1999/xhtml", "iframe");
  iframe.setAttribute("mozframetype", "content");

  hiddenDoc.documentElement.appendChild(iframe);

  // Disable some types of content
  let docShell = iframe.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDocShell);
  docShell.allowAuth = false;
  docShell.allowPlugins = false;
  docShell.allowImages = false;
  docShell.allowWindowControl = false;
  // TODO: disable media (bug 759964)
  
  // Mark this docShell as a "browserFrame", to break script access to e.g. window.top
  docShell.setIsBrowserElement();

  return iframe;
}

function WorkerHandle(port, worker) {
  this.port = port;
  this._worker = worker;
}
WorkerHandle.prototype = {
  // XXX - workers have no .close() method, but *do* have a .terminate()
  // method which we should implement. However, the worker spec doesn't define
  // a callback to be made in the worker when this happens - it all just dies.
  // TODO: work out a sane impl for 'terminate'.
  terminate: function terminate() {
    this._worker.terminate();
  }
};

// This is the message listener for the *client* (ie, chrome) side of the world.
function initClientMessageHandler(worker, workerWindow) {
  function _messageHandler(event) {
    // We will ignore all messages destined for otherType.
    let data = event.data;
    let portid = data.portId;
    let port;
    if (!data.portFromType || data.portFromType === "client") {
      // this is a message posted by ourself so ignore it.
      return;
    }
    switch (data.portTopic) {
      // No "port-create" here - client ports are created explicitly.

      case "port-close":
        // the worker side of the port was closed, so close this side too.
        port = worker.ports[portid];
        if (!port) {
          // port already closed (which will happen when we call port.close()
          // below - the worker side will send us this message but we've
          // already closed it.)
          return;
        }
        delete worker.ports[portid];
        port.close();
        break;

      case "port-message":
        // the client posted a message to this worker port.
        port = worker.ports[portid];
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
  workerWindow.addEventListener('message', messageHandler);
}

/**
 * ClientPort
 *
 * Client side of the entangled ports. The ClientPort is used by both XUL
 * windows and Content windows to communicate with the worker
 *
 * constructor:
 * @param {integer} portid
 * @param {nsiDOMWindow} clientWindow, optional
 */
function ClientPort(portid, clientWindow) {
  this._clientWindow = clientWindow;
  this._window = null;
  // messages posted to the worker before the worker has loaded.
  this._pendingMessagesOutgoing = [];
  AbstractPort.call(this, portid);
}

ClientPort.prototype = {
  __exposedProps__: {
    onmessage: "rw",
    postMessage: "r",
    close: "r",
    toString: "r"
  },
  __proto__: AbstractPort.prototype,
  _portType: "client",

  _JSONParse: function fw_ClientPort_JSONParse(data) {
    if (this._clientWindow) {
      return XPCNativeWrapper.unwrap(this._clientWindow).JSON.parse(data);
    }
    return JSON.parse(data);
  },

  _createWorkerAndEntangle: function fw_ClientPort_createWorkerAndEntangle(worker) {
    this._window = worker.frame.contentWindow;
    worker.ports[this._portid] = this;
    this._postControlMessage("port-create");
    while (this._pendingMessagesOutgoing.length) {
      this._dopost(this._pendingMessagesOutgoing.shift());
    }
  },

  _dopost: function fw_ClientPort_dopost(data) {
    if (!this._window) {
      this._pendingMessagesOutgoing.push(data);
    } else {
      this._window.postMessage(data, "*");
    }
  },

  _onerror: function fw_ClientPort_onerror(err) {
    Cu.reportError("FrameWorker: Port " + this + " handler failed: " + err + "\n" + err.stack);
  },

  close: function fw_ClientPort_close() {
    if (!this._portid) {
      return; // already closed.
    }
    // a leaky abstraction due to the worker spec not specifying how the
    // other end of a port knows it is closing.
    this.postMessage({topic: "social.port-closing"});
    AbstractPort.prototype.close.call(this);
    this._window = null;
    this._clientWindow = null;
    this._pendingMessagesOutgoing = null;
  }
}
