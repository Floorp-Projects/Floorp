/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["frame"];

/** @namespace */
this.frame = {};

const FRAME_SCRIPT = "chrome://marionette/content/listener.js";

/**
 * An object representing a frame that Marionette has loaded a
 * frame script in.
 */
frame.RemoteFrame = function(windowId, frameId) {
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
    this.driver = driver;
  }

  /**
   * Receives all messages from content messageManager.
   */
  /*eslint-disable*/
  receiveMessage(message) {
    switch (message.name) {
      case "MarionetteFrame:getCurrentFrameId":
        if (this.currentRemoteFrame !== null) {
          return this.currentRemoteFrame.frameId;
        }
    }
  }
  /* eslint-enable*/

  getOopFrame(winId, frameId) {
    // get original frame window
    let outerWin = Services.wm.getOuterWindowWithId(winId);
    // find the OOP frame
    let f = outerWin.document.getElementsByTagName("iframe")[frameId];
    return f;
  }

  getFrameMM(winId, frameId) {
    let oopFrame = this.getOopFrame(winId, frameId);
    let mm = oopFrame.frameLoader.messageManager;
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
    for (let i = 0; i < this.remoteFrames.length; i++) {
      let f = this.remoteFrames[i];
      let fmm = f.messageManager.get();
      try {
        fmm.sendAsyncMessage("aliveCheck", {});
      } catch (e) {
        if (e.result == Cr.NS_ERROR_NOT_INITIALIZED) {
          this.remoteFrames.splice(i--, 1);
          continue;
        }
      }

      if (fmm == mm) {
        this.currentRemoteFrame = f;
        this.addMessageManagerListeners(mm);

        return oopFrame.id;
      }
    }

    // if we get here, then we need to load the frame script in this frame,
    // and set the frame's ChromeMessageSender as the active message manager
    // the driver will listen to.
    this.addMessageManagerListeners(mm);
    let f = new frame.RemoteFrame(winId, frameId);
    f.messageManager = Cu.getWeakReference(mm);
    this.remoteFrames.push(f);
    this.currentRemoteFrame = f;

    mm.loadFrameScript(FRAME_SCRIPT, true, true);

    return oopFrame.id;
  }

  /**
   * Adds message listeners to the driver,  listening for
   * messages from content frame scripts.
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
    mm.addWeakMessageListener("Marionette:switchedToFrame", this.driver);
    mm.addWeakMessageListener("Marionette:getVisibleCookies", this.driver);
    mm.addWeakMessageListener("Marionette:register", this.driver);
    mm.addWeakMessageListener("Marionette:listenersAttached", this.driver);
    mm.addWeakMessageListener("Marionette:GetLogLevel", this.driver);
    mm.addWeakMessageListener("MarionetteFrame:getCurrentFrameId", this);
  }

  /**
   * Removes listeners for messages from content frame scripts.
   *
   * @param {nsIMessageListenerManager} mm
   *     The message manager object, typically
   *     ChromeMessageBroadcaster or ChromeMessageSender.
   */
  removeMessageManagerListeners(mm) {
    mm.removeWeakMessageListener("Marionette:ok", this.driver);
    mm.removeWeakMessageListener("Marionette:done", this.driver);
    mm.removeWeakMessageListener("Marionette:error", this.driver);
    mm.removeWeakMessageListener("Marionette:switchedToFrame", this.driver);
    mm.removeWeakMessageListener("Marionette:getVisibleCookies", this.driver);
    mm.removeWeakMessageListener(
        "Marionette:getImportedScripts", this.driver.importedScripts);
    mm.removeWeakMessageListener("Marionette:GetLogLevel", this.driver);
    mm.removeWeakMessageListener("Marionette:listenersAttached", this.driver);
    mm.removeWeakMessageListener("Marionette:register", this.driver);
    mm.removeWeakMessageListener("MarionetteFrame:getCurrentFrameId", this);
  }
};

frame.Manager.prototype.QueryInterface = XPCOMUtils.generateQI(
    [Ci.nsIMessageListener, Ci.nsISupportsWeakReference]);
