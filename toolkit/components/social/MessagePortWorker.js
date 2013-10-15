/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: this is never instantiated in chrome - the source is sent across
// to the worker and it is evaluated there and created in response to a
// port-create message we send.
function WorkerPort(portid) {
  AbstractPort.call(this, portid);
}

WorkerPort.prototype = {
  __proto__: AbstractPort.prototype,
  _portType: "worker",

  _dopost: function fw_WorkerPort_dopost(data) {
    // postMessage is injected into the sandbox.
    _postMessage(data, "*");
  },

  _onerror: function fw_WorkerPort_onerror(err) {
    // We throw an object that "derives" from the exception, but with
    // a more detailed message.
    throw {message: "Port " + this + " handler failed: " + err.message, __proto__: err};
  }
}

function importScripts() {
  for (var i=0; i < arguments.length; i++) {
    // load the url *synchronously*
    var scriptURL = arguments[i];
    var xhr = new XMLHttpRequest();
    xhr.open('GET', scriptURL, false);
    xhr.onreadystatechange = function(aEvt) {
      if (xhr.readyState == 4) {
        if (xhr.status == 200 || xhr.status == 0) {
          _evalInSandbox(xhr.responseText, scriptURL);
        }
        else {
          throw new Error("Unable to importScripts ["+scriptURL+"], status " + xhr.status)
        }
      }
    };
    xhr.send(null);
  }
}

// This function is magically injected into the sandbox and used there.
// Thus, it is only ever dealing with "worker" ports.
function __initWorkerMessageHandler() {

  let ports = {}; // all "worker" ports currently alive, keyed by ID.

  function messageHandler(event) {
    // We will ignore all messages destined for otherType.
    let data = event.data;
    let portid = data.portId;
    let port;
    if (!data.portFromType || data.portFromType === "worker") {
      // this is a message posted by ourself so ignore it.
      return;
    }
    switch (data.portTopic) {
      case "port-create":
        // a new port was created on the "client" side - create a new worker
        // port and store it in the map
        port = new WorkerPort(portid);
        ports[portid] = port;
        // and call the "onconnect" handler.
        try {
          onconnect({ports: [port]});
        } catch(e) {
          // we have a bad worker and cannot continue, we need to signal
          // an error
          port._postControlMessage("port-connection-error", JSON.stringify(e.toString()));
          throw e;
        }
        break;

      case "port-close":
        // the client side of the port was closed, so close this side too.
        port = ports[portid];
        if (!port) {
          // port already closed (which will happen when we call port.close()
          // below - the client side will send us this message but we've
          // already closed it.)
          return;
        }
        delete ports[portid];
        port.close();
        break;

      case "port-message":
        // the client posted a message to this worker port.
        port = ports[portid];
        if (!port) {
          // port must be closed - this shouldn't happen!
          return;
        }
        port._onmessage(data.data);
        break;

      default:
        break;
    }
  }
  // addEventListener is injected into the sandbox.
  _addEventListener('message', messageHandler);
}
__initWorkerMessageHandler();
