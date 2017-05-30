/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["frame"];

this.frame = {};

const FRAME_SCRIPT = "chrome://marionette/content/listener.js";

// list of OOP frames that has the frame script loaded
var remoteFrames = [];

/**
 * An object representing a frame that Marionette has loaded a
 * frame script in.
 */
frame.RemoteFrame = function (windowId, frameId) {
  // outerWindowId relative to main process
  this.windowId = windowId;
  // actual frame relative to the windowId's frames list
  this.frameId = frameId;
  // assigned frame ID, used for messaging
  this.targetFrameId = this.frameId;
  // list of OOP frames that has the frame script loaded
  this.remoteFrames = [];
};

/**
 * The FrameManager will maintain the list of Out Of Process (OOP)
 * frames and will handle frame switching between them.
 *
 * It handles explicit frame switching (switchToFrame), and implicit
 * frame switching, which occurs when a modal dialog is triggered in B2G.
 *
 * @param {GeckoDriver} driver
 *     Reference to the driver instance.
 */
frame.Manager = class {
  constructor(driver) {
    // messageManager maintains the messageManager
    // for the current process' chrome frame or the global message manager

    // holds a member of the remoteFrames (for an OOP frame)
    // or null (for the main process)
    this.currentRemoteFrame = null;
    // frame we'll need to restore once interrupt is gone
    this.previousRemoteFrame = null;
    // set to true when we have been interrupted by a modal
    this.handledModal = false;
    this.driver = driver;
  }

  /**
   * Receives all messages from content messageManager.
   */
  receiveMessage(message) {
    switch (message.name) {
      case "MarionetteFrame:getInterruptedState":
        // this will return true if the calling frame was interrupted by a modal dialog
        if (this.previousRemoteFrame) {
          // get the frame window of the interrupted frame
          let interruptedFrame = Services.wm.getOuterWindowWithId(
              this.previousRemoteFrame.windowId);

          if (this.previousRemoteFrame.frameId !== null) {
            // find OOP frame
            let iframes = interruptedFrame.document.getElementsByTagName("iframe");
            interruptedFrame = iframes[this.previousRemoteFrame.frameId];
          }

          // check if the interrupted frame is the same as the calling frame
          if (interruptedFrame.src == message.target.src) {
            return {value: this.handledModal};
          }

        // we get here if previousRemoteFrame and currentRemoteFrame are null,
        // i.e. if we're in a non-OOP process, or we haven't switched into an OOP frame,
        // in which case, handledModal can't be set to true
        } else if (this.currentRemoteFrame === null) {
          return {value: this.handledModal};
        }
        return {value: false};

      // handleModal is called when we need to switch frames to the main
      // process due to a modal dialog interrupt
      case "MarionetteFrame:handleModal":
        // If previousRemoteFrame was set, that means we switched into a
        // remote frame.  If this is the case, then we want to switch back
        // into the system frame.  If it isn't the case, then we're in a
        // non-OOP environment, so we don't need to handle remote frames.
        let isLocal = true;
        if (this.currentRemoteFrame !== null) {
          isLocal = false;
          this.removeMessageManagerListeners(
              this.currentRemoteFrame.messageManager.get());

          // store the previous frame so we can switch back to it when
          // the modal is dismissed
          this.previousRemoteFrame = this.currentRemoteFrame;

          // by setting currentRemoteFrame to null,
          // it signifies we're in the main process
          this.currentRemoteFrame = null;
          this.driver.messageManager = Cc["@mozilla.org/globalmessagemanager;1"]
              .getService(Ci.nsIMessageBroadcaster);
        }

        this.handledModal = true;
        this.driver.sendOk(this.driver.command_id);
        return {value: isLocal};

      case "MarionetteFrame:getCurrentFrameId":
        if (this.currentRemoteFrame !== null) {
          return this.currentRemoteFrame.frameId;
        }
    }
  }

  getOopFrame(winId, frameId) {
    // get original frame window
    let outerWin = Services.wm.getOuterWindowWithId(winId);
    // find the OOP frame
    let f = outerWin.document.getElementsByTagName("iframe")[frameId];
    return f;
  }

  getFrameMM(winId, frameId) {
    let oopFrame = this.getOopFrame(winId, frameId);
    let mm = oopFrame.QueryInterface(Ci.nsIFrameLoaderOwner)
        .frameLoader.messageManager;
    return mm;
  }

  /**
   * Switch to OOP frame.  We're handling this here so we can maintain
   * a list of remote frames.
   */
  switchToFrame(winId, frameId) {
    let oopFrame = this.getOopFrame(winId, frameId);
    let mm = this.getFrameMM(winId, frameId);

    // see if this frame already has our frame script loaded in it;
    // if so, just wake it up
    for (let i = 0; i < remoteFrames.length; i++) {
      let f = remoteFrames[i];
      let fmm = f.messageManager.get();
      try {
        fmm.sendAsyncMessage("aliveCheck", {});
      } catch (e) {
        if (e.result == Cr.NS_ERROR_NOT_INITIALIZED) {
          remoteFrames.splice(i--, 1);
          continue;
        }
      }

      if (fmm == mm) {
        this.currentRemoteFrame = f;
        this.addMessageManagerListeners(mm);

        mm.sendAsyncMessage("Marionette:restart");
        return oopFrame.id;
      }
    }

    // if we get here, then we need to load the frame script in this frame,
    // and set the frame's ChromeMessageSender as the active message manager
    // the driver will listen to.
    this.addMessageManagerListeners(mm);
    let f = new frame.RemoteFrame(winId, frameId);
    f.messageManager = Cu.getWeakReference(mm);
    remoteFrames.push(f);
    this.currentRemoteFrame = f;

    mm.loadFrameScript(FRAME_SCRIPT, true, true);

    return oopFrame.id;
  }

  /*
   * This function handles switching back to the frame that was
   * interrupted by the modal dialog.  It gets called by the interrupted
   * frame once the dialog is dismissed and the frame resumes its process.
   */
  switchToModalOrigin() {
    // only handle this if we indeed switched out of the modal's
    // originating frame
    if (this.previousRemoteFrame !== null) {
      this.currentRemoteFrame = this.previousRemoteFrame;
      let mm = this.currentRemoteFrame.messageManager.get();
      this.addMessageManagerListeners(mm);
    }
    this.handledModal = false;
  }

  /**
   * Adds message listeners to the driver,  listening for
   * messages from content frame scripts.  It also adds a
   * MarionetteFrame:getInterruptedState message listener to the
   * FrameManager, so the frame manager's state can be checked by the frame.
   *
   * @param {nsIMessageListenerManager} mm
   *     The message manager object, typically
   *     ChromeMessageBroadcaster or ChromeMessageSender.
   */
  addMessageManagerListeners(mm) {
    mm.addWeakMessageListener("Marionette:ok", this.driver);
    mm.addWeakMessageListener("Marionette:done", this.driver);
    mm.addWeakMessageListener("Marionette:error", this.driver);
    mm.addWeakMessageListener("Marionette:emitTouchEvent", this.driver);
    mm.addWeakMessageListener("Marionette:log", this.driver);
    mm.addWeakMessageListener("Marionette:shareData", this.driver);
    mm.addWeakMessageListener("Marionette:switchToModalOrigin", this.driver);
    mm.addWeakMessageListener("Marionette:switchedToFrame", this.driver);
    mm.addWeakMessageListener("Marionette:getVisibleCookies", this.driver);
    mm.addWeakMessageListener("Marionette:register", this.driver);
    mm.addWeakMessageListener("Marionette:listenersAttached", this.driver);
    mm.addWeakMessageListener("MarionetteFrame:handleModal", this);
    mm.addWeakMessageListener("MarionetteFrame:getCurrentFrameId", this);
    mm.addWeakMessageListener("MarionetteFrame:getInterruptedState", this);
  }

  /**
   * Removes listeners for messages from content frame scripts.
   * We do not remove the MarionetteFrame:getInterruptedState or
   * the Marionette:switchToModalOrigin message listener, because we
   * want to allow all known frames to contact the frame manager so
   * that it can check if it was interrupted, and if so, it will call
   * switchToModalOrigin when its process gets resumed.
   *
   * @param {nsIMessageListenerManager} mm
   *     The message manager object, typically
   *     ChromeMessageBroadcaster or ChromeMessageSender.
   */
  removeMessageManagerListeners(mm) {
    mm.removeWeakMessageListener("Marionette:ok", this.driver);
    mm.removeWeakMessageListener("Marionette:done", this.driver);
    mm.removeWeakMessageListener("Marionette:error", this.driver);
    mm.removeWeakMessageListener("Marionette:log", this.driver);
    mm.removeWeakMessageListener("Marionette:shareData", this.driver);
    mm.removeWeakMessageListener("Marionette:switchedToFrame", this.driver);
    mm.removeWeakMessageListener("Marionette:getVisibleCookies", this.driver);
    mm.removeWeakMessageListener("Marionette:getImportedScripts", this.driver.importedScripts);
    mm.removeWeakMessageListener("Marionette:listenersAttached", this.driver);
    mm.removeWeakMessageListener("Marionette:register", this.driver);
    mm.removeWeakMessageListener("MarionetteFrame:handleModal", this);
    mm.removeWeakMessageListener("MarionetteFrame:getCurrentFrameId", this);
  }
};

frame.Manager.prototype.QueryInterface = XPCOMUtils.generateQI(
    [Ci.nsIMessageListener, Ci.nsISupportsWeakReference]);
