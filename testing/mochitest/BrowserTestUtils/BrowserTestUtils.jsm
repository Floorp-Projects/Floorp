/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module implements a number of utilities useful for browser tests.
 *
 * All asynchronous helper methods should return promises, rather than being
 * callback based.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "BrowserTestUtils",
];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://testing-common/TestUtils.jsm");

Cc["@mozilla.org/globalmessagemanager;1"]
  .getService(Ci.nsIMessageListenerManager)
  .loadFrameScript(
    "chrome://mochikit/content/tests/BrowserTestUtils/content-utils.js", true);

XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils",
  "resource:///modules/E10SUtils.jsm");

this.BrowserTestUtils = {
  /**
   * Loads a page in a new tab, executes a Task and closes the tab.
   *
   * @param options
   *        An object with the following properties:
   *        {
   *          gBrowser:
   *            Reference to the "tabbrowser" element where the new tab should
   *            be opened.
   *          url:
   *            String with the URL of the page to load.
   *        }
   * @param taskFn
   *        Generator function representing a Task that will be executed while
   *        the tab is loaded. The first argument passed to the function is a
   *        reference to the browser object for the new tab.
   *
   * @return {} Returns the value that is returned from taskFn.
   * @resolves When the tab has been closed.
   * @rejects Any exception from taskFn is propagated.
   */
  withNewTab: Task.async(function* (options, taskFn) {
    let tab = yield BrowserTestUtils.openNewForegroundTab(options.gBrowser, options.url);
    let result = yield taskFn(tab.linkedBrowser);
    options.gBrowser.removeTab(tab);
    return Promise.resolve(result);
  }),

  /**
   * Opens a new tab in the foreground.
   *
   * @param {tabbrowser} tabbrowser
   *        The tabbrowser to open the tab new in.
   * @param {string} opening
   *        May be either a string URL to load in the tab, or a function that
   *        will be called to open a foreground tab. Defaults to "about:blank".
   * @param {boolean} waitForLoad
   *        True to wait for the page in the new tab to load. Defaults to true.
   *
   * @return {Promise}
   *         Resolves when the tab is ready and loaded as necessary.
   * @resolves The new tab.
   */
  openNewForegroundTab(tabbrowser, opening = "about:blank", aWaitForLoad = true) {
    let tab;
    let promises = [
      BrowserTestUtils.switchTab(tabbrowser, function () {
        if (typeof opening == "function") {
          opening();
          tab = tabbrowser.selectedTab;
        }
        else {
          tabbrowser.selectedTab = tab = tabbrowser.addTab(opening);
        }
      })
    ];

    if (aWaitForLoad) {
      promises.push(BrowserTestUtils.browserLoaded(tab.linkedBrowser));
    }

    return Promise.all(promises).then(() => tab);
  },

  /**
   * Switches to a tab and resolves when it is ready.
   *
   * @param {tabbrowser} tabbrowser
   *        The tabbrowser.
   * @param {tab} tab
   *        Either a tab element to switch to or a function to perform the switch.
   *
   * @return {Promise}
   *         Resolves when the tab has been switched to.
   * @resolves The tab switched to.
   */
  switchTab(tabbrowser, tab) {
    let promise = new Promise(resolve => {
      tabbrowser.addEventListener("TabSwitchDone", function onSwitch() {
        tabbrowser.removeEventListener("TabSwitchDone", onSwitch);
        TestUtils.executeSoon(() => resolve(tabbrowser.selectedTab));
      });
    });

    if (typeof tab == "function") {
      tab();
    }
    else {
      tabbrowser.selectedTab = tab;
    }
    return promise;
  },

  /**
   * Waits for an ongoing page load in a browser window to complete.
   *
   * This can be used in conjunction with any synchronous method for starting a
   * load, like the "addTab" method on "tabbrowser", and must be called before
   * yielding control to the event loop. This is guaranteed to work because the
   * way we're listening for the load is in the content-utils.js frame script,
   * and then sending an async message up, so we can't miss the message.
   *
   * @param {xul:browser} browser
   *        A xul:browser.
   * @param {Boolean} includeSubFrames
   *        A boolean indicating if loads from subframes should be included.
   *
   * @return {Promise}
   * @resolves When a load event is triggered for the browser.
   */
  browserLoaded(browser, includeSubFrames=false) {
    return new Promise(resolve => {
      let mm = browser.ownerDocument.defaultView.messageManager;
      mm.addMessageListener("browser-test-utils:loadEvent", function onLoad(msg) {
        if (msg.target == browser && (!msg.data.subframe || includeSubFrames)) {
          mm.removeMessageListener("browser-test-utils:loadEvent", onLoad);
          resolve();
        }
      });
    });
  },

  /**
   * Waits for the next tab to open and load a given URL.
   *
   * The method doesn't wait for the tab contents to load.
   *
   * @param {tabbrowser} tabbrowser
   *        The tabbrowser to look for the next new tab in.
   * @param {string} url
   *        A string URL to look for in the new tab. If null, allows any non-blank URL.
   *
   * @return {Promise}
   * @resolves With the {xul:tab} when a tab is opened and its location changes to the given URL.
   */
  waitForNewTab(tabbrowser, url) {
    return new Promise((resolve, reject) => {
      tabbrowser.tabContainer.addEventListener("TabOpen", function onTabOpen(openEvent) {
        tabbrowser.tabContainer.removeEventListener("TabOpen", onTabOpen);

        let progressListener = {
          onLocationChange(aBrowser) {
            if (aBrowser != openEvent.target.linkedBrowser ||
                (url && aBrowser.currentURI.spec != url) ||
                (!url && aBrowser.currentURI.spec == "about:blank")) {
              return;
            }

            tabbrowser.removeTabsProgressListener(progressListener);
            resolve(openEvent.target);
          },
        };
        tabbrowser.addTabsProgressListener(progressListener);

      });
    });
  },

  /**
   * Waits for the next browser window to open and be fully loaded.
   *
   * @return {Promise}
   *         A Promise which resolves the next time that a DOM window
   *         opens and the delayed startup observer notification fires.
   */
  waitForNewWindow: Task.async(function* (delayedStartup=true) {
    let win = yield this.domWindowOpened();

    yield TestUtils.topicObserved("browser-delayed-startup-finished",
                                   subject => subject == win);
    return win;
  }),

  /**
   * Loads a new URI in the given browser and waits until we really started
   * loading. In e10s browser.loadURI() can be an asynchronous operation due
   * to having to switch the browser's remoteness and keep its shistory data.
   *
   * @param {xul:browser} browser
   *        A xul:browser.
   * @param {string} uri
   *        The URI to load.
   *
   * @return {Promise}
   * @resolves When we started loading the given URI.
   */
  loadURI: Task.async(function* (browser, uri) {
    // Load the new URI.
    browser.loadURI(uri);

    // Nothing to do in non-e10s mode.
    if (!browser.ownerDocument.defaultView.gMultiProcessBrowser) {
      return;
    }

    // Retrieve the given browser's current process type.
    let process = browser.isRemoteBrowser ? Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT
                                          : Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

    // If the new URI can't load in the browser's current process then we
    // should wait for the new frameLoader to be created. This will happen
    // asynchronously when the browser's remoteness changes.
    if (!E10SUtils.canLoadURIInProcess(uri, process)) {
      yield this.waitForEvent(browser, "XULFrameLoaderCreated");
    }
  }),

  /**
   * @return {Promise}
   *         A Promise which resolves when a "domwindowopened" notification
   *         has been fired by the window watcher.
   */
  domWindowOpened() {
    return new Promise(resolve => {
      function observer(subject, topic, data) {
        if (topic != "domwindowopened") { return; }

        Services.ww.unregisterNotification(observer);
        resolve(subject.QueryInterface(Ci.nsIDOMWindow));
      }
      Services.ww.registerNotification(observer);
    });
  },

  /**
   * @param {Object} options
   *        {
   *          private: A boolean indicating if the window should be
   *                   private
   *          remote:  A boolean indicating if the window should run
   *                   remote browser tabs or not. If omitted, the window
   *                   will choose the profile default state.
   *        }
   * @return {Promise}
   *         Resolves with the new window once it is loaded.
   */
  openNewBrowserWindow(options={}) {
    let argString = Cc["@mozilla.org/supports-string;1"].
                    createInstance(Ci.nsISupportsString);
    argString.data = "";
    let features = "chrome,dialog=no,all";

    if (options.private) {
      features += ",private";
    }

    if (options.hasOwnProperty("remote")) {
      let remoteState = options.remote ? "remote" : "non-remote";
      features += `,${remoteState}`;
    }

    let win = Services.ww.openWindow(
      null, Services.prefs.getCharPref("browser.chromeURL"), "_blank",
      features, argString);

    // Wait for browser-delayed-startup-finished notification, it indicates
    // that the window has loaded completely and is ready to be used for
    // testing.
    return TestUtils.topicObserved("browser-delayed-startup-finished",
                                   subject => subject == win).then(() => win);
  },

  /**
   * Closes a window.
   *
   * @param {Window}
   *        A window to close.
   *
   * @return {Promise}
   *         Resolves when the provided window has been closed.
   */
  closeWindow(win) {
    return new Promise(resolve => {
      function observer(subject, topic, data) {
        if (topic == "domwindowclosed" && subject === win) {
          Services.ww.unregisterNotification(observer);
          resolve();
        }
      }
      Services.ww.registerNotification(observer);
      win.close();
    });
  },

  /**
   * Waits for an event to be fired on a specified element.
   *
   * Usage:
   *    let promiseEvent = BrowserTestUtil.waitForEvent(element, "eventName");
   *    // Do some processing here that will cause the event to be fired
   *    // ...
   *    // Now yield until the Promise is fulfilled
   *    let receivedEvent = yield promiseEvent;
   *
   * @param {Element} subject
   *        The element that should receive the event.
   * @param {string} eventName
   *        Name of the event to listen to.
   * @param {bool} capture [optional]
   *        True to use a capturing listener.
   * @param {function} checkFn [optional]
   *        Called with the Event object as argument, should return true if the
   *        event is the expected one, or false if it should be ignored and
   *        listening should continue. If not specified, the first event with
   *        the specified name resolves the returned promise.
   *
   * @note Because this function is intended for testing, any error in checkFn
   *       will cause the returned promise to be rejected instead of waiting for
   *       the next event, since this is probably a bug in the test.
   *
   * @returns {Promise}
   * @resolves The Event object.
   */
  waitForEvent(subject, eventName, capture, checkFn) {
    return new Promise((resolve, reject) => {
      subject.addEventListener(eventName, function listener(event) {
        try {
          if (checkFn && !checkFn(event)) {
            return;
          }
          subject.removeEventListener(eventName, listener, capture);
          resolve(event);
        } catch (ex) {
          try {
            subject.removeEventListener(eventName, listener, capture);
          } catch (ex2) {
            // Maybe the provided object does not support removeEventListener.
          }
          reject(ex);
        }
      }, capture);
    });
  },

  /**
   *  Versions of EventUtils.jsm synthesizeMouse functions that synthesize a
   *  mouse event in a child process and return promises that resolve when the
   *  event has fired and completed. Instead of a window, a browser is required
   *  to be passed to this function.
   *
   * @param target
   *        One of the following:
   *        - a selector string that identifies the element to target. The syntax is as
   *        for querySelector.
   *        - a CPOW element (for easier test-conversion).
   *        - a function to be run in the content process that returns the element to
   *        target
   *        - null, in which case the offset is from the content document's edge.
   * @param {integer} offsetX
   *        x offset from target's left bounding edge
   * @param {integer} offsetY
   *        y offset from target's top bounding edge
   * @param {Object} event object
   *        Additional arguments, similar to the EventUtils.jsm version
   * @param {Browser} browser
   *        Browser element, must not be null
   *
   * @returns {Promise}
   * @resolves True if the mouse event was cancelled.
   */
  synthesizeMouse(target, offsetX, offsetY, event, browser)
  {
    return new Promise(resolve => {
      let mm = browser.messageManager;
      mm.addMessageListener("Test:SynthesizeMouseDone", function mouseMsg(message) {
        mm.removeMessageListener("Test:SynthesizeMouseDone", mouseMsg);
        resolve(message.data.defaultPrevented);
      });

      let cpowObject = null;
      let targetFn = null;
      if (typeof target == "function") {
        targetFn = target.toString();
        target = null;
      } else if (typeof target != "string") {
        cpowObject = target;
        target = null;
      }

      mm.sendAsyncMessage("Test:SynthesizeMouse",
                          {target, targetFn, x: offsetX, y: offsetY, event: event},
                          {object: cpowObject});
    });
  },

  /**
   *  Version of synthesizeMouse that uses the center of the target as the mouse
   *  location. Arguments and the return value are the same.
   */
  synthesizeMouseAtCenter(target, event, browser)
  {
    // Use a flag to indicate to center rather than having a separate message.
    event.centered = true;
    return BrowserTestUtils.synthesizeMouse(target, 0, 0, event, browser);
  },

  /**
   *  Version of synthesizeMouse that uses a client point within the child
   *  window instead of a target as the offset. Otherwise, the arguments and
   *  return value are the same as synthesizeMouse.
   */
  synthesizeMouseAtPoint(offsetX, offsetY, event, browser)
  {
    return BrowserTestUtils.synthesizeMouse(null, offsetX, offsetY, event, browser);
  },

  /**
   * Removes the given tab from its parent tabbrowser and
   * waits until its final message has reached the parent.
   */
  removeTab(tab, options = {}) {
    let dontRemove = options && options.dontRemove;

    return new Promise(resolve => {
      let {messageManager: mm, frameLoader} = tab.linkedBrowser;
      mm.addMessageListener("SessionStore:update", function onMessage(msg) {
        if (msg.targetFrameLoader == frameLoader && msg.data.isFinal) {
          mm.removeMessageListener("SessionStore:update", onMessage);
          resolve();
        }
      }, true);

      if (!dontRemove && !tab.closing) {
        tab.ownerDocument.defaultView.gBrowser.removeTab(tab);
      }
    });
  },

  /**
   * Crashes a remote browser tab and cleans up the generated minidumps.
   * Resolves with the data from the .extra file (the crash annotations).
   *
   * @param (Browser) browser
   *        A remote <xul:browser> element. Must not be null.
   *
   * @returns (Promise)
   * @resolves An Object with key-value pairs representing the data from the
   *           crash report's extra file (if applicable).
   */
  crashBrowser: Task.async(function*(browser) {
    let extra = {};
    let KeyValueParser = {};
    if (AppConstants.MOZ_CRASHREPORTER) {
      Cu.import("resource://gre/modules/KeyValueParser.jsm", KeyValueParser);
    }

    if (!browser.isRemoteBrowser) {
      throw new Error("<xul:browser> needs to be remote in order to crash");
    }

    /**
     * Returns the directory where crash dumps are stored.
     *
     * @return nsIFile
     */
    function getMinidumpDirectory() {
      let dir = Services.dirsvc.get('ProfD', Ci.nsIFile);
      dir.append("minidumps");
      return dir;
    }

    /**
     * Removes a file from a directory. This is a no-op if the file does not
     * exist.
     *
     * @param directory
     *        The nsIFile representing the directory to remove from.
     * @param filename
     *        A string for the file to remove from the directory.
     */
    function removeFile(directory, filename) {
      let file = directory.clone();
      file.append(filename);
      if (file.exists()) {
        file.remove(false);
      }
    }

    // This frame script is injected into the remote browser, and used to
    // intentionally crash the tab. We crash by using js-ctypes and dereferencing
    // a bad pointer. The crash should happen immediately upon loading this
    // frame script.
    let frame_script = () => {
      const Cu = Components.utils;
      Cu.import("resource://gre/modules/ctypes.jsm");

      let dies = function() {
        privateNoteIntentionalCrash();
        let zero = new ctypes.intptr_t(8);
        let badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
        badptr.contents
      };

      dump("\nEt tu, Brute?\n");
      dies();
    }

    let crashCleanupPromise = new Promise((resolve, reject) => {
      let observer = (subject, topic, data) => {
        if (topic != "ipc:content-shutdown") {
          return reject("Received incorrect observer topic: " + topic);
        }
        if (!(subject instanceof Ci.nsIPropertyBag2)) {
          return reject("Subject did not implement nsIPropertyBag2");
        }
        // we might see this called as the process terminates due to previous tests.
        // We are only looking for "abnormal" exits...
        if (!subject.hasKey("abnormal")) {
          dump("\nThis is a normal termination and isn't the one we are looking for...\n");
          return;
        }

        let dumpID;
        if ('nsICrashReporter' in Ci) {
          dumpID = subject.getPropertyAsAString('dumpID');
          if (!dumpID) {
            return reject("dumpID was not present despite crash reporting " +
                          "being enabled");
          }
        }

        if (dumpID) {
          let minidumpDirectory = getMinidumpDirectory();
          let extrafile = minidumpDirectory.clone();
          extrafile.append(dumpID + '.extra');
          if (extrafile.exists()) {
            dump(`\nNo .extra file for dumpID: ${dumpID}\n`);
            if (AppConstants.MOZ_CRASHREPORTER) {
              extra = KeyValueParser.parseKeyValuePairsFromFile(extrafile);
            } else {
              dump('\nCrashReporter not enabled - will not return any extra data\n');
            }
          }

          removeFile(minidumpDirectory, dumpID + '.dmp');
          removeFile(minidumpDirectory, dumpID + '.extra');
        }

        Services.obs.removeObserver(observer, 'ipc:content-shutdown');
        dump("\nCrash cleaned up\n");
        resolve();
      };

      Services.obs.addObserver(observer, 'ipc:content-shutdown', false);
    });

    let aboutTabCrashedLoadPromise = new Promise((resolve, reject) => {
      browser.addEventListener("AboutTabCrashedLoad", function onCrash() {
        browser.removeEventListener("AboutTabCrashedLoad", onCrash, false);
        dump("\nabout:tabcrashed loaded\n");
        resolve();
      }, false, true);
    });

    // This frame script will crash the remote browser as soon as it is
    // evaluated.
    let mm = browser.messageManager;
    mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", false);

    yield Promise.all([crashCleanupPromise, aboutTabCrashedLoadPromise]);

    let gBrowser = browser.ownerDocument.defaultView.gBrowser;
    let tab = gBrowser.getTabForBrowser(browser);
    if (tab.getAttribute("crashed") != "true") {
      throw new Error("Tab should be marked as crashed");
    }

    return extra;
  }),
};
