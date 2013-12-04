/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "FrameManager"
];

let FRAME_SCRIPT = "chrome://marionette/content/marionette-listener.js";
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://gre/modules/Log.jsm");
let logger = Log.repository.getLogger("Marionette");

//list of OOP frames that has the frame script loaded
let remoteFrames = [];

/**
 * An object representing a frame that Marionette has loaded a
 * frame script in.
 */
function MarionetteRemoteFrame(windowId, frameId) {
  this.windowId = windowId; //outerWindowId relative to main process
  this.frameId = frameId ? frameId : null; //actual frame relative to windowId's frames list
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
    }
  },

  //This is just 'switch to OOP frame'. We're handling this here so we can maintain a list of remoteFrames. 
  switchToFrame: function FM_switchToFrame(message) {
    // Switch to a remote frame.
    let frameWindow = Services.wm.getOuterWindowWithId(message.json.win); //get the original frame window
    let oopFrame = frameWindow.document.getElementsByTagName("iframe")[message.json.frame]; //find the OOP frame
    let mm = oopFrame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager; //get the OOP frame's mm

    // See if this frame already has our frame script loaded in it; if so,
    // just wake it up.
    for (let i = 0; i < remoteFrames.length; i++) {
      let frame = remoteFrames[i];
      let frameMessageManager = frame.messageManager.get();
      logger.info("trying remote frame " + i);
      try {
        frameMessageManager.sendAsyncMessage("aliveCheck", {});
      }
      catch(e) {
        if (e.result ==  Components.results.NS_ERROR_NOT_INITIALIZED) {
          logger.info("deleting frame");
          remoteFrames.splice(i, 1);
          continue;
        }
      }
      if (frameMessageManager == mm) {
        this.currentRemoteFrame = frame;
        this.addMessageManagerListeners(mm);
        mm.sendAsyncMessage("Marionette:restart", {});
        return;
      }
    }

    // If we get here, then we need to load the frame script in this frame, 
    // and set the frame's ChromeMessageSender as the active message manager the server will listen to
    this.addMessageManagerListeners(mm);
    logger.info("frame-manager load script: " + mm.toString());
    mm.loadFrameScript(FRAME_SCRIPT, true);
    let aFrame = new MarionetteRemoteFrame(message.json.win, message.json.frame);
    aFrame.messageManager = Cu.getWeakReference(mm);
    remoteFrames.push(aFrame);
    this.currentRemoteFrame = aFrame;
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
   * Adds message listeners to the server, listening for messages from content frame scripts.
   * It also adds a "MarionetteFrame:getInterruptedState" message listener to the FrameManager,
   * so the frame manager's state can be checked by the frame
   *
   * @param object messageManager
   *        The messageManager object (ChromeMessageBroadcaster or ChromeMessageSender)
   *        to which the listeners should be added.
   */
  addMessageManagerListeners: function MDA_addMessageManagerListeners(messageManager) {
    messageManager.addWeakMessageListener("Marionette:ok", this.server);
    messageManager.addWeakMessageListener("Marionette:done", this.server);
    messageManager.addWeakMessageListener("Marionette:error", this.server);
    messageManager.addWeakMessageListener("Marionette:log", this.server);
    messageManager.addWeakMessageListener("Marionette:shareData", this.server);
    messageManager.addWeakMessageListener("Marionette:register", this.server);
    messageManager.addWeakMessageListener("Marionette:runEmulatorCmd", this.server);
    messageManager.addWeakMessageListener("Marionette:switchToModalOrigin", this.server);
    messageManager.addWeakMessageListener("Marionette:switchToFrame", this.server);
    messageManager.addWeakMessageListener("Marionette:switchedToFrame", this.server);
    messageManager.addWeakMessageListener("MarionetteFrame:handleModal", this);
    messageManager.addWeakMessageListener("MarionetteFrame:getInterruptedState", this);
  },

  /**
   * Removes listeners for messages from content frame scripts.
   * We do not remove the "MarionetteFrame:getInterruptedState" or the
   * "Marioentte:switchToModalOrigin" message listener,
   * because we want to allow all known frames to contact the frame manager so that
   * it can check if it was interrupted, and if so, it will call switchToModalOrigin
   * when its process gets resumed.
   *
   * @param object messageManager
   *        The messageManager object (ChromeMessageBroadcaster or ChromeMessageSender)
   *        from which the listeners should be removed.
   */
  removeMessageManagerListeners: function MDA_removeMessageManagerListeners(messageManager) {
    messageManager.removeWeakMessageListener("Marionette:ok", this.server);
    messageManager.removeWeakMessageListener("Marionette:done", this.server);
    messageManager.removeWeakMessageListener("Marionette:error", this.server);
    messageManager.removeWeakMessageListener("Marionette:log", this.server);
    messageManager.removeWeakMessageListener("Marionette:shareData", this.server);
    messageManager.removeWeakMessageListener("Marionette:register", this.server);
    messageManager.removeWeakMessageListener("Marionette:runEmulatorCmd", this.server);
    messageManager.removeWeakMessageListener("Marionette:switchToFrame", this.server);
    messageManager.removeWeakMessageListener("Marionette:switchedToFrame", this.server);
    messageManager.removeWeakMessageListener("MarionetteFrame:handleModal", this);
  },

};
