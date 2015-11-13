/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["FrameManager"];

var FRAME_SCRIPT = "chrome://marionette/content/listener.js";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
               .getService(Ci.mozIJSSubScriptLoader);

//list of OOP frames that has the frame script loaded
var remoteFrames = [];

/**
 * An object representing a frame that Marionette has loaded a
 * frame script in.
 */
function MarionetteRemoteFrame(windowId, frameId) {
  this.windowId = windowId; //outerWindowId relative to main process
  this.frameId = frameId; //actual frame relative to windowId's frames list
  this.targetFrameId = this.frameId; //assigned FrameId, used for messaging
};

/**
 * The FrameManager will maintain the list of Out Of Process (OOP) frames and will handle
 * frame switching between them.
 * It handles explicit frame switching (switchToFrame), and implicit frame switching, which
 * occurs when a modal dialog is triggered in B2G.
 *
 */
this.FrameManager = function FrameManager(server) {
  //messageManager maintains the messageManager for the current process' chrome frame or the global message manager
  this.currentRemoteFrame = null; //holds a member of remoteFrames (for an OOP frame) or null (for the main process)
  this.previousRemoteFrame = null; //frame we'll need to restore once interrupt is gone
  this.handledModal = false; //set to true when we have been interrupted by a modal
  this.server = server; // a reference to the marionette server
};

FrameManager.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIMessageListener,
                                         Ci.nsISupportsWeakReference]),

  /**
   * Receives all messages from content messageManager
   */
  receiveMessage: function FM_receiveMessage(message) {
    switch (message.name) {
      case "MarionetteFrame:getInterruptedState":
        // This will return true if the calling frame was interrupted by a modal dialog
        if (this.previousRemoteFrame) {
          let interruptedFrame = Services.wm.getOuterWindowWithId(this.previousRemoteFrame.windowId);//get the frame window of the interrupted frame
          if (this.previousRemoteFrame.frameId != null) {
            interruptedFrame = interruptedFrame.document.getElementsByTagName("iframe")[this.previousRemoteFrame.frameId]; //find the OOP frame
          }
          //check if the interrupted frame is the same as the calling frame
          if (interruptedFrame.src == message.target.src) {
            return {value: this.handledModal};
          }
        }
        else if (this.currentRemoteFrame == null) {
          // we get here if previousRemoteFrame and currentRemoteFrame are null, ie: if we're in a non-OOP process, or we haven't switched into an OOP frame, in which case, handledModal can't be set to true.
          return {value: this.handledModal};
        }
        return {value: false};
      case "MarionetteFrame:handleModal":
        /*
         * handleModal is called when we need to switch frames to the main process due to a modal dialog interrupt.
         */
        // If previousRemoteFrame was set, that means we switched into a remote frame.
        // If this is the case, then we want to switch back into the system frame.
        // If it isn't the case, then we're in a non-OOP environment, so we don't need to handle remote frames
        let isLocal = true;
        if (this.currentRemoteFrame != null) {
          isLocal = false;
          this.removeMessageManagerListeners(this.currentRemoteFrame.messageManager.get());
          //store the previous frame so we can switch back to it when the modal is dismissed
          this.previousRemoteFrame = this.currentRemoteFrame;
          //by setting currentRemoteFrame to null, it signifies we're in the main process
          this.currentRemoteFrame = null;
          this.server.messageManager = Cc["@mozilla.org/globalmessagemanager;1"]
                                       .getService(Ci.nsIMessageBroadcaster);
        }
        this.handledModal = true;
        this.server.sendOk(this.server.command_id);
        return {value: isLocal};
      case "MarionetteFrame:getCurrentFrameId":
        if (this.currentRemoteFrame != null) {
          return this.currentRemoteFrame.frameId;
        }
    }
  },

  getOopFrame: function FM_getOopFrame(winId, frameId) {
    // get original frame window
    let outerWin = Services.wm.getOuterWindowWithId(winId);
    // find the OOP frame
    let f = outerWin.document.getElementsByTagName("iframe")[frameId];
    return f;
  },

  getFrameMM: function FM_getFrameMM(winId, frameId) {
    let oopFrame = this.getOopFrame(winId, frameId);
    let mm = oopFrame.QueryInterface(Ci.nsIFrameLoaderOwner)
        .frameLoader.messageManager;
    return mm;
  },

  /**
   * Switch to OOP frame.  We're handling this here
   * so we can maintain a list of remote frames.
   */
  switchToFrame: function FM_switchToFrame(winId, frameId) {
    let oopFrame = this.getOopFrame(winId, frameId);
    let mm = this.getFrameMM(winId, frameId);

    // See if this frame already has our frame script loaded in it;
    // if so, just wake it up.
    for (let i = 0; i < remoteFrames.length; i++) {
      let frame = remoteFrames[i];
      let frameMessageManager = frame.messageManager.get();
      try {
        frameMessageManager.sendAsyncMessage("aliveCheck", {});
      } catch (e) {
        if (e.result ==  Components.results.NS_ERROR_NOT_INITIALIZED) {
          remoteFrames.splice(i--, 1);
          continue;
        }
      }
      if (frameMessageManager == mm) {
        this.currentRemoteFrame = frame;
        this.addMessageManagerListeners(mm);

        mm.sendAsyncMessage("Marionette:restart");
        return oopFrame.id;
      }
    }

    // If we get here, then we need to load the frame script in this frame,
    // and set the frame's ChromeMessageSender as the active message manager
    // the server will listen to.
    this.addMessageManagerListeners(mm);
    let aFrame = new MarionetteRemoteFrame(winId, frameId);
    aFrame.messageManager = Cu.getWeakReference(mm);
    remoteFrames.push(aFrame);
    this.currentRemoteFrame = aFrame;

    mm.loadFrameScript(FRAME_SCRIPT, true, true);

    return oopFrame.id;
  },

  /*
   * This function handles switching back to the frame that was interrupted by the modal dialog.
   * This function gets called by the interrupted frame once the dialog is dismissed and the frame resumes its process
   */
  switchToModalOrigin: function FM_switchToModalOrigin() {
    //only handle this if we indeed switched out of the modal's originating frame
    if (this.previousRemoteFrame != null) {
      this.currentRemoteFrame = this.previousRemoteFrame;
      this.addMessageManagerListeners(this.currentRemoteFrame.messageManager.get());
    }
    this.handledModal = false;
  },

  /**
   * Adds message listeners to the server, 
   * listening for messages from content frame scripts.
   * It also adds a MarionetteFrame:getInterruptedState
   * message listener to the FrameManager,
   * so the frame manager's state can be checked by the frame.
   *
   * @param {nsIMessageListenerManager} mm
   *     The message manager object, typically
   *     ChromeMessageBroadcaster or ChromeMessageSender.
   */
  addMessageManagerListeners: function FM_addMessageManagerListeners(mm) {
    mm.addWeakMessageListener("Marionette:emitTouchEvent", this.server);
    mm.addWeakMessageListener("Marionette:log", this.server);
    mm.addWeakMessageListener("Marionette:runEmulatorCmd", this.server);
    mm.addWeakMessageListener("Marionette:runEmulatorShell", this.server);
    mm.addWeakMessageListener("Marionette:shareData", this.server);
    mm.addWeakMessageListener("Marionette:switchToModalOrigin", this.server);
    mm.addWeakMessageListener("Marionette:switchedToFrame", this.server);
    mm.addWeakMessageListener("Marionette:getVisibleCookies", this.server);
    mm.addWeakMessageListener("Marionette:register", this.server);
    mm.addWeakMessageListener("Marionette:listenersAttached", this.server);
    mm.addWeakMessageListener("Marionette:getFiles", this.server);
    mm.addWeakMessageListener("MarionetteFrame:handleModal", this);
    mm.addWeakMessageListener("MarionetteFrame:getCurrentFrameId", this);
    mm.addWeakMessageListener("MarionetteFrame:getInterruptedState", this);
  },

  /**
   * Removes listeners for messages from content frame scripts.
   * We do not remove the MarionetteFrame:getInterruptedState
   * or the Marionette:switchToModalOrigin message listener,
   * because we want to allow all known frames to contact the frame manager
   * so that it can check if it was interrupted, and if so,
   * it will call switchToModalOrigin when its process gets resumed.
   *
   * @param {nsIMessageListenerManager} mm
   *     The message manager object, typically
   *     ChromeMessageBroadcaster or ChromeMessageSender.
   */
  removeMessageManagerListeners: function FM_removeMessageManagerListeners(mm) {
    mm.removeWeakMessageListener("Marionette:log", this.server);
    mm.removeWeakMessageListener("Marionette:shareData", this.server);
    mm.removeWeakMessageListener("Marionette:runEmulatorCmd", this.server);
    mm.removeWeakMessageListener("Marionette:runEmulatorShell", this.server);
    mm.removeWeakMessageListener("Marionette:switchedToFrame", this.server);
    mm.removeWeakMessageListener("Marionette:getVisibleCookies", this.server);
    mm.removeWeakMessageListener("Marionette:listenersAttached", this.server);
    mm.removeWeakMessageListener("Marionette:register", this.server);
    mm.removeWeakMessageListener("Marionette:getFiles", this.server);
    mm.removeWeakMessageListener("MarionetteFrame:handleModal", this);
    mm.removeWeakMessageListener("MarionetteFrame:getCurrentFrameId", this);
  }
};
