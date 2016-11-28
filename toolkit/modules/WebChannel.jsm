/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * WebChannel is an abstraction that uses the Message Manager and Custom Events
 * to create a two-way communication channel between chrome and content code.
 */

this.EXPORTED_SYMBOLS = ["WebChannel", "WebChannelBroker"];

const ERRNO_MISSING_PRINCIPAL          = 1;
const ERRNO_NO_SUCH_CHANNEL            = 2;
const ERRNO_UNKNOWN_ERROR              = 999;
const ERROR_UNKNOWN                    = "UNKNOWN_ERROR";


const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");


/**
 * WebChannelBroker is a global object that helps manage WebChannel objects.
 * This object handles channel registration, origin validation and message multiplexing.
 */

var WebChannelBroker = Object.create({
  /**
   * Register a new channel that callbacks messages
   * based on proper origin and channel name
   *
   * @param channel {WebChannel}
   */
  registerChannel: function(channel) {
    if (!this._channelMap.has(channel)) {
      this._channelMap.set(channel);
    } else {
      Cu.reportError("Failed to register the channel. Channel already exists.");
    }

    // attach the global message listener if needed
    if (!this._messageListenerAttached) {
      this._messageListenerAttached = true;
      this._manager.addMessageListener("WebChannelMessageToChrome", this._listener.bind(this));
    }
  },

  /**
   * Unregister a channel
   *
   * @param channelToRemove {WebChannel}
   *        WebChannel to remove from the channel map
   *
   * Removes the specified channel from the channel map
   */
  unregisterChannel: function(channelToRemove) {
    if (!this._channelMap.delete(channelToRemove)) {
      Cu.reportError("Failed to unregister the channel. Channel not found.");
    }
  },

  /**
   * @param event {Event}
   *        Message Manager event
   * @private
   */
  _listener: function(event) {
    let data = event.data;
    let sendingContext = {
      browser: event.target,
      eventTarget: event.objects.eventTarget,
      principal: event.principal,
    };
    // data must be a string except for a few legacy origins allowed by browser-content.js.
    if (typeof data == "string") {
      try {
        data = JSON.parse(data);
      } catch (e) {
        Cu.reportError("Failed to parse WebChannel data as a JSON object");
        return;
      }
    }

    if (data && data.id) {
      if (!event.principal) {
        this._sendErrorEventToContent(data.id, sendingContext, ERRNO_MISSING_PRINCIPAL, "Message principal missing");
      } else {
        let validChannelFound = false;
        data.message = data.message || {};

        for (var channel of this._channelMap.keys()) {
          if (channel.id === data.id &&
            channel._originCheckCallback(event.principal)) {
            validChannelFound = true;
            channel.deliver(data, sendingContext);
          }
        }

        // if no valid origins send an event that there is no such valid channel
        if (!validChannelFound) {
          this._sendErrorEventToContent(data.id, sendingContext, ERRNO_NO_SUCH_CHANNEL, "No Such Channel");
        }
      }
    } else {
      Cu.reportError("WebChannel channel id missing");
    }
  },
  /**
   * The global message manager operates on every <browser>
   */
  _manager: Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager),
  /**
   * Boolean used to detect if the global message manager event is already attached
   */
  _messageListenerAttached: false,
  /**
   * Object to store pairs of message origins and callback functions
   */
  _channelMap: new Map(),
  /**
   *
   * @param id {String}
   *        The WebChannel id to include in the message
   * @param sendingContext {Object}
   *        Message sending context
   * @param [errorMsg] {String}
   *        Error message
   * @private
   */
  _sendErrorEventToContent: function(id, sendingContext, errorNo, errorMsg) {
    let { browser: targetBrowser, eventTarget, principal: targetPrincipal } = sendingContext;

    errorMsg = errorMsg || "Web Channel Broker error";

    if (targetBrowser && targetBrowser.messageManager) {
      targetBrowser.messageManager.sendAsyncMessage("WebChannelMessageToContent", {
        id: id,
        message: {
          errno: errorNo,
          error: errorMsg,
        },
      }, { eventTarget: eventTarget }, targetPrincipal);
    } else {
      Cu.reportError("Failed to send a WebChannel error. Target invalid.");
    }
    Cu.reportError(id.toString() + " error message. " + errorMsg);
  },
});


/**
 * Creates a new WebChannel that listens and sends messages over some channel id
 *
 * @param id {String}
 *        WebChannel id
 * @param originOrPermission {nsIURI/string}
 *        If an nsIURI, a valid origin that should be part of requests for
 *        this channel.  If a string, a permission for which the permission
 *        manager will be checked to determine if the request is allowed. Note
 *        that in addition to the permission manager check, the request must
 *        be made over https://
 * @constructor
 */
this.WebChannel = function(id, originOrPermission) {
  if (!id || !originOrPermission) {
    throw new Error("WebChannel id and originOrPermission are required.");
  }

  this.id = id;
  // originOrPermission can be either an nsIURI or a string representing a
  // permission name.
  if (typeof originOrPermission == "string") {
    this._originCheckCallback = requestPrincipal => {
      // The permission manager operates on domain names rather than true
      // origins (bug 1066517).  To mitigate that, we explicitly check that
      // the scheme is https://.
      let uri = Services.io.newURI(requestPrincipal.originNoSuffix, null, null);
      if (uri.scheme != "https") {
        return false;
      }
      // OK - we have https - now we can check the permission.
      let perm = Services.perms.testExactPermissionFromPrincipal(requestPrincipal,
                                                                 originOrPermission);
      return perm == Ci.nsIPermissionManager.ALLOW_ACTION;
    }
  } else {
    // a simple URI, so just check for an exact match.
    this._originCheckCallback = requestPrincipal => {
      return originOrPermission.prePath === requestPrincipal.originNoSuffix;
    }
  }
  this._originOrPermission = originOrPermission;
};

this.WebChannel.prototype = {

  /**
   * WebChannel id
   */
  id: null,

  /**
   * The originOrPermission value passed to the constructor, mainly for
   * debugging and tests.
   */
  _originOrPermission: null,

  /**
   * Callback that will be called with the principal of an incoming message
   * to check if the request should be dispatched to the listeners.
   */
  _originCheckCallback: null,

  /**
   * WebChannelBroker that manages WebChannels
   */
  _broker: WebChannelBroker,

  /**
   * Callback that will be called with the contents of an incoming message
   */
  _deliverCallback: null,

  /**
   * Registers the callback for messages on this channel
   * Registers the channel itself with the WebChannelBroker
   *
   * @param callback {Function}
   *        Callback that will be called when there is a message
   *        @param {String} id
   *        The WebChannel id that was used for this message
   *        @param {Object} message
   *        The message itself
   *        @param sendingContext {Object}
   *        The sending context of the source of the message. Can be passed to
   *        `send` to respond to a message.
   *               @param sendingContext.browser {browser}
   *                      The <browser> object that captured the
   *                      WebChannelMessageToChrome.
   *               @param sendingContext.eventTarget {EventTarget}
   *                      The <EventTarget> where the message was sent.
   *               @param sendingContext.principal {Principal}
   *                      The <Principal> of the EventTarget where the
   *                      message was sent.
   */
  listen: function(callback) {
    if (this._deliverCallback) {
      throw new Error("Failed to listen. Listener already attached.");
    } else if (!callback) {
      throw new Error("Failed to listen. Callback argument missing.");
    } else {
      this._deliverCallback = callback;
      this._broker.registerChannel(this);
    }
  },

  /**
   * Resets the callback for messages on this channel
   * Removes the channel from the WebChannelBroker
   */
  stopListening: function() {
    this._broker.unregisterChannel(this);
    this._deliverCallback = null;
  },

  /**
   * Sends messages over the WebChannel id using the "WebChannelMessageToContent" event
   *
   * @param message {Object}
   *        The message object that will be sent
   * @param target {Object}
   *        A <target> with the information of where to send the message.
   *        @param target.browser {browser}
   *               The <browser> object with a "messageManager" that will
   *               be used to send the message.
   *        @param target.principal {Principal}
   *               Principal of the target. Prevents messages from
   *               being dispatched to unexpected origins. The system principal
   *               can be specified to send to any target.
   *        @param [target.eventTarget] {EventTarget}
   *               Optional eventTarget within the browser, use to send to a
   *               specific element, e.g., an iframe.
   */
  send: function(message, target) {
    let { browser, principal, eventTarget } = target;

    if (message && browser && browser.messageManager && principal) {
      browser.messageManager.sendAsyncMessage("WebChannelMessageToContent", {
        id: this.id,
        message: message
      }, { eventTarget }, principal);
    } else if (!message) {
      Cu.reportError("Failed to send a WebChannel message. Message not set.");
    } else {
      Cu.reportError("Failed to send a WebChannel message. Target invalid.");
    }
  },

  /**
   * Deliver WebChannel messages to the set "_channelCallback"
   *
   * @param data {Object}
   *        Message data
   * @param sendingContext {Object}
   *        Message sending context.
   *        @param sendingContext.browser {browser}
   *               The <browser> object that captured the
   *               WebChannelMessageToChrome.
   *        @param sendingContext.eventTarget {EventTarget}
   *               The <EventTarget> where the message was sent.
   *        @param sendingContext.principal {Principal}
   *               The <Principal> of the EventTarget where the message was sent.
   *
   */
  deliver: function(data, sendingContext) {
    if (this._deliverCallback) {
      try {
        this._deliverCallback(data.id, data.message, sendingContext);
      } catch (ex) {
        this.send({
          errno: ERRNO_UNKNOWN_ERROR,
          error: ex.message ? ex.message : ERROR_UNKNOWN
        }, sendingContext);
        Cu.reportError("Failed to execute WebChannel callback:");
        Cu.reportError(ex);
      }
    } else {
      Cu.reportError("No callback set for this channel.");
    }
  }
};
