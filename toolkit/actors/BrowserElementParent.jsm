/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserElementParent", "PermitUnloader"];

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

let { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "unloadTimeoutMs",
  "dom.beforeunload_timeout_ms"
);

// Out-of-process subframes might be in the following states following a request
// to permit closing:
//
// We're waiting to hear back from the frame on whether or not we should permit
// unloading.
const STATE_WAITING = 0;
// The out-of-process iframe is running its permitUnload routine. The frame stays
// in this state when the permit unload modal dialog is being displayed.
const STATE_RUNNING = 1;
// The permitUnload routine has completed, and any modal dialogs for that subframe
// have been cleared. This frame, at this point, has answered the questions of whether
// or not it wants to be unloaded.
const STATE_DONE = 2;

/**
 * The BrowserElementParent is for performing actions on one or more subframes of
 * a <xul:browser> from the browser element binding.
 */
class BrowserElementParent extends JSWindowActorParent {
  /**
   * NOTE: It is expected that this function is only called from PermitUnloader.
   * Callers who want to check if a <xul:browser> wants to allow being unloaded
   * should use PermitUnloader instead.
   *
   * Sends a request to a subframe to run the docShell's permitUnload routine
   * with the passed flags.
   *
   * @param {Number} flags See nsIContentViewer.idl for the types of flags that
   * can be passed.
   */
  sendPermitUnload(flags) {
    this.sendAsyncMessage("PermitUnload", { flags });
  }

  receiveMessage(message) {
    switch (message.name) {
      case "Running": {
        PermitUnloader._transitionFrameState(this.manager, STATE_RUNNING);
        break;
      }
      case "Done": {
        let permitUnload = message.data.permitUnload;
        if (!permitUnload) {
          PermitUnloader._doNotPermitUnload(this.manager);
        } else {
          PermitUnloader._transitionFrameState(this.manager, STATE_DONE);
        }
        break;
      }
      case "DOMWindowClose": {
        // This message is sent whenever window.close() is called within a window
        // that had originally been opened via window.open. Double-check that this is
        // coming from a top-level frame, and then dispatch the DOMWindowClose event
        // on the browser so that the front-end code can do the right thing with the
        // request to close.
        if (!this.manager.browsingContext.parent) {
          let browser = this.manager.browsingContext.embedderElement;
          let win = browser.ownerGlobal;
          // If this is a non-remote browser, the DOMWindowClose event will bubble
          // up naturally, and doesn't need to be re-dispatched.
          if (browser.isRemoteBrowser) {
            browser.dispatchEvent(
              new win.CustomEvent("DOMWindowClose", {
                bubbles: true,
              })
            );
          }
        }
        break;
      }
    }
  }
}

/**
 * PermitUnloader is a parent-process singleton that manages checking to see whether
 * or not <xul:browser> elements would prefer to be unloaded or not.
 *
 * PermitUnloader works by first finding the root nodes of process-contiguous trees
 * of frames. This means that if a document has the following frame structure:
 *
 *                   a
 *                /     \
 *               b        c
 *             /   \    /  \
 *            b    d   c    c
 *                /          \
 *               d            d
 *
 * where each letter represents the process that each frame runs in, then we consider
 * process-contiguous trees of frames to be ones where all frames are directly connected
 * and running in the same process. Process-contiguous subtrees are denoted in the following
 * graph with shared numbers.
 *
 *                   a1
 *                /     \
 *               b2        c3
 *             /    \    /   \
 *            b2    d4  c3   c3
 *                 /          \
 *               d4            d5
 *
 * Specifically, note that the d5 leaf node, while belonging to the same process as the
 * d4 nodes are not directly connected, so it's not process-contiguous.
 *
 * Messaging the roots of these process-contiguous subtrees concurrently allows us to ask
 * each of these subtrees to simultaneously run their permit unload checks in
 * breadth-first order. This appears to be Chromium's algorithm.
 */
var PermitUnloader = {
  // Maps a frameLoader to the state of a permitUnload request. If the frameLoader
  // doesn't exist in the map, then either permitUnload has never been called on
  // the frameLoader, or a previous permitUnload check has already completed.
  //
  // See permitUnload for the mapping structure.
  inProgressPermitUnload: new WeakMap(),

  /* Public methods */

  /**
   * Returns true if the frameLoader associated with a tab is still determining
   * whether unloading it is preferred. This might mean that beforeunload events
   * are still running, or the modal UI to prevent unloading is waiting on the
   * user to respond.
   *
   * @param {FrameLoader} frameLoader the frameLoader associated with a <xul:browser>
   * that might still be determining unload-ability.
   * @return {Boolean} true if unload-ability is still being determined.
   */
  inPermitUnload(frameLoader) {
    return this.inProgressPermitUnload.has(frameLoader);
  },

  /**
   * Returns an Object indicating whether or not a <xul:browser> wants to be
   * unloaded or not.
   *
   * Note: this function spins a nested event loop while it waits for a response
   * from each process-contiguous subtree, and will throw to avoid re-entry for
   * the same frameLoader.
   *
   * @param {FrameLoader} frameLoader the frameLoader that should be checked for
   * unload-ability.
   * @param {Number} flags See nsIContentViewer.idl for the types of flags that
   * can be passed.
   *
   * @return {Object} an Object with the following structure:
   *
   * {
   *   permitUnload: Boolean (true if all frames prefer to be unloaded.)
   *   timedOut: Boolean (true if contacting all frames timed out.)
   * }
   */
  permitUnload(frameLoader, flags) {
    // Don't allow re-entry for the same tab
    if (this.inPermitUnload(frameLoader)) {
      throw new Error("permitUnload is already running for this tab.");
    }

    let frameStates = new Map();

    // Until JS Window Actor teardown methods are implemented, we'll use the
    // message-manager-close observer notification to notice if the top-level
    // context has gone away.
    let mm = frameLoader.messageManager;
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    let roots = this.getProcessContiguousRoots(frameLoader.browsingContext);

    let state = {
      frameStates,
      timedOut: false,
      permitUnload: true,
      timer,
      waitingCount: roots.length,
    };

    let observer = subject => {
      if (subject == mm) {
        this._finish(state);
      }
    };

    try {
      this.inProgressPermitUnload.set(frameLoader, state);
      Services.obs.addObserver(observer, "message-manager-close");

      for (let windowGlobalParent of roots) {
        let actor = windowGlobalParent.getActor("BrowserElement");
        frameStates.set(actor.manager, STATE_WAITING);
        actor.sendPermitUnload(flags);
      }

      timer.initWithCallback(
        () => {
          this._onTimeout(frameLoader);
        },
        unloadTimeoutMs,
        timer.TYPE_ONE_SHOT
      );

      Services.tm.spinEventLoopUntilOrShutdown(() => {
        return this._finishedPermitUnload(frameStates);
      });
    } finally {
      this._finish(state);
      this.inProgressPermitUnload.delete(frameLoader);
      Services.obs.removeObserver(observer, "message-manager-close");
    }

    return {
      permitUnload: state.permitUnload,
      timedOut: state.timedOut,
    };
  },

  /**
   * Given a BrowsingContext, returns that BrowsingContext's WindowGlobalParent
   * along with all of the WindowGlobalParents for the roots of the process
   * contiguous subtrees under the passed BrowsingContext. See the documentation
   * for PermitUnloader for a detailed illustration of what roots of
   * process-contiguous subtrees are.
   *
   * @param {BrowsingContext} browsingContext the BrowsingContext whose descendants
   * should be searched. The WindowGlobalParent of this BrowsingContext will be
   * included in the returned Array.
   *
   * @return {Array<WindowGlobalParent>} The WindowGlobalParent's for all roots of
   * process-contiguous subtrees of the passed in BrowsingContext, along with the
   * WindowGlobalParent of the passed in BrowsingContext.
   */
  getProcessContiguousRoots(browsingContext) {
    let contextsToWalk = [browsingContext];
    let roots = [];

    while (contextsToWalk.length) {
      let currentContext = contextsToWalk.pop();
      let windowGlobal = currentContext.currentWindowGlobal;

      if (!windowGlobal) {
        continue;
      }

      if (windowGlobal.isProcessRoot) {
        roots.push(windowGlobal);
      }

      contextsToWalk.push(...currentContext.children);
    }

    return roots;
  },

  /**
   * Returns true if any of the subframes associated with the frameLoader
   * have a beforeunload event handler set. If this returns false, there's
   * no point in running permitUnload on the frameLoader, as we know that
   * no frames will prevent it.
   *
   * @param {FrameLoader} frameLoader the frameLoader that should be checked for
   * beforeunload event handlers.
   *
   * @return {Boolean} true if there's at least one beforeunload event handler
   * set in any of the frameLoader's frames.
   */
  hasBeforeUnload(frameLoader) {
    if (frameLoader.remoteTab) {
      return frameLoader.remoteTab.hasBeforeUnload;
    }
    return false;
  },

  /**
   * Private methods - the following methods are only expected to be called
   * from PermitUnloader or BrowserElementParent.
   */

  /**
   * This is called when the BrowserElementParent receives a message from the
   * process-contiguous subtree root alerting it that it is either starting to
   * run the permitUnload routine, or has completed running the permitUnload
   * routine.
   *
   * @param {WindowGlobalParent} windowGlobal the WindowGlobalParent for the
   * root of the process-contiguous subtree that we're receiving the state
   * update from.
   * @param Number newFrameState one of STATE_RUNNING or STATE_DONE.
   */
  _transitionFrameState(windowGlobal, newFrameState) {
    let frameLoader = windowGlobal.rootFrameLoader;
    let state = this.inProgressPermitUnload.get(frameLoader);

    if (!state) {
      return;
    }

    let oldFrameState = state.frameStates.get(windowGlobal);

    if (oldFrameState == STATE_WAITING) {
      state.waitingCount--;

      if (!state.waitingCount) {
        state.timer.cancel();
      }
    }

    state.frameStates.set(windowGlobal, newFrameState);
  },

  /**
   * Called from the permitUnload nested event loop to check whether we've
   * gotten enough information to exit that loop. Enough information means
   * either that we've heard from all process-contiguous subtrees, or we've
   * gotten an answer from one of the process-contiguous subtrees saying that
   * we shouldn't unload the <xul:browser>, or that we've timed out waiting
   * for all responses to come back.
   *
   * @param {Map} frameStates the mapping of process-contiguous subtree roots
   * to their loading states as set by permitUnload.
   *
   * @return {Boolean} true if the nested event loop should exit.
   */
  _finishedPermitUnload(frameStates) {
    for (let [, state] of frameStates) {
      if (state != STATE_DONE) {
        return false;
      }
    }

    return true;
  },

  /**
   * Called when we no longer want to run the nested event loop, and no longer want
   * to fire the messaging timeout function.
   *
   * @param {Object} state the state for the frameLoader that we're currently checking
   * for unload-ability.
   */
  _finish(state) {
    state.frameStates.clear();
    state.timer.cancel();
  },

  /**
   * Called when the timeout for checking for unload-ability has fired. This means
   * we haven't heard back from all of the process-contiguous subtrees in time,
   * in which case, we assume that it's safe to unload the <xul:browser>.
   *
   * @param {FrameLoader} frameLoader the frameLoader that took too long to detect
   * unload-ability.
   */
  _onTimeout(frameLoader) {
    let state = this.inProgressPermitUnload.get(frameLoader);
    state.timedOut = true;
    this._finish(state);
    // Dispatch something to ensure that the main thread wakes up.
    Services.tm.dispatchToMainThread(function() {});
  },

  /**
   * Called by the BrowserElementParent if a process-contiguous subtree reports that
   * it shouldn't be unloaded.
   *
   * @param {WindowGlobalParent} windowGlobal the WindowGlobalParent for the
   * root of the process-contiguous subtree that requested that unloading not
   * occur.
   */
  _doNotPermitUnload(windowGlobal) {
    let frameLoader = windowGlobal.rootFrameLoader;
    let state = this.inProgressPermitUnload.get(frameLoader);
    // We might have already heard from a previous process-contiguous subtree
    // that unload should not be permitted, in which case this state will have
    // been cleared. In that case, we'll just ignore the message.
    if (state) {
      this._finish(state);
      state.permitUnload = false;
    }
  },
};
