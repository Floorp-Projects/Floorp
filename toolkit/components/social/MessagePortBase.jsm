/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Code that is shared between clients and workers.
const EXPORTED_SYMBOLS = ["AbstractPort"];

function AbstractPort(portid) {
  this._portid = portid;
  this._handler = undefined;
  // pending messages sent to this port before it has a message handler.
  this._pendingMessagesIncoming = [];
}

AbstractPort.prototype = {
  _portType: null, // set by a subclass.
  // abstract methods to be overridden.
  _dopost: function fw_AbstractPort_dopost(data) {
    throw new Error("not implemented");
  },
  _onerror: function fw_AbstractPort_onerror(err) {
    throw new Error("not implemented");
  },

  // and concrete methods shared by client and workers.
  toString: function fw_AbstractPort_toString() {
    return "MessagePort(portType='" + this._portType + "', portId=" + this._portid + ")";
  },
  _JSONParse: function fw_AbstractPort_JSONParse(data) JSON.parse(data),

 _postControlMessage: function fw_AbstractPort_postControlMessage(topic, data) {
    let postData = {portTopic: topic,
                    portId: this._portid,
                    portFromType: this._portType,
                    data: data,
                    __exposedProps__: {
                      portTopic: 'r',
                      portId: 'r',
                      portFromType: 'r',
                      data: 'r'
                    }
                   };
    this._dopost(postData);
  },

  _onmessage: function fw_AbstractPort_onmessage(data) {
    // See comments in postMessage below - we work around a cloning
    // issue by using JSON for these messages.
    // Further, we allow the workers to override exactly how the JSON parsing
    // is done - we try and do such parsing in the client window so things
    // like prototype overrides on Array work as expected.
    data = this._JSONParse(data);
    if (!this._handler) {
      this._pendingMessagesIncoming.push(data);
    }
    else {
      try {
        this._handler({data: data,
                       __exposedProps__: {data: 'r'}
                      });
      }
      catch (ex) {
        this._onerror(ex);
      }
    }
  },

  set onmessage(handler) { // property setter for onmessage
    this._handler = handler;
    while (this._pendingMessagesIncoming.length) {
      this._onmessage(this._pendingMessagesIncoming.shift());
    }
  },

  /**
   * postMessage
   *
   * Send data to the onmessage handler on the other end of the port.  The
   * data object should have a topic property.
   *
   * @param {jsobj} data
   */
  postMessage: function fw_AbstractPort_postMessage(data) {
    if (this._portid === null) {
      throw new Error("port is closed");
    }
    // There seems to be an issue with passing objects directly and letting
    // the structured clone thing work - we sometimes get:
    // [Exception... "The object could not be cloned."  code: "25" nsresult: "0x80530019 (DataCloneError)"]
    // The best guess is that this happens when funky things have been added to the prototypes.
    // It doesn't happen for our "control" messages, only in messages from
    // content - so we explicitly use JSON on these messages as that avoids
    // the problem.
    this._postControlMessage("port-message", JSON.stringify(data));
  },

  close: function fw_AbstractPort_close() {
    if (!this._portid) {
      return; // already closed.
    }
    this._postControlMessage("port-close");
    // and clean myself up.
    this._handler = null;
    this._pendingMessagesIncoming = [];
    this._portid = null;
  }
}
