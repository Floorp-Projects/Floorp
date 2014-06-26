/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Provides shared infrastructure for the automated tests.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "DownloadPaths",
                                  "resource://gre/modules/DownloadPaths.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FormAutofill",
                                  "resource://gre/modules/FormAutofill.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
                                  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
                                  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "OS",
                                  "resource://gre/modules/osfile.jsm");

let gTerminationTasks = [];
let add_termination_task = taskFn => gTerminationTasks.push(taskFn);

/**
 * None of the testing frameworks support asynchronous termination functions, so
 * this task must be registered later, after the other "add_task" calls.
 *
 * Even xpcshell doesn't support calling "add_task" in the "tail.js" file,
 * because it registers the task but does not wait for its termination,
 * potentially leading to intermittent failures in subsequent tests.
 */
let terminationTaskFn = function* test_common_terminate() {
  for (let taskFn of gTerminationTasks) {
    try {
      yield Task.spawn(taskFn);
    } catch (ex) {
      Output.print(ex);
      Assert.ok(false);
    }
  }
};

/* --- Global helpers --- */

// Some of these functions are already implemented in other parts of the source
// tree, see bug 946708 about sharing more code.

let TestUtils = {
  /**
   * Waits for at least one tick of the event loop.  This means that all pending
   * events at the time of this call will have been processed.  Other events may
   * be processed before the returned promise is resolved.
   *
   * @return {Promise}
   * @resolves When pending events have been processed.
   * @rejects Never.
   */
  waitForTick: function () {
    return new Promise(resolve => executeSoon(resolve));
  },

  /**
   * Waits for the specified timeout.
   *
   * @param aTimeMs
   *        Minimum time to wait from the moment of this call, in milliseconds.
   *        The actual wait may be longer, due to system timer resolution and
   *        pending events being processed before the promise is resolved.
   *
   * @return {Promise}
   * @resolves When the specified time has passed.
   * @rejects Never.
   */
  waitMs: function (aTimeMs) {
    return new Promise(resolve => setTimeout(resolve, aTimeMs));
  },

  /**
   * Allows waiting for an observer notification once.
   *
   * @param aTopic
   *        Notification topic to observe.
   *
   * @return {Promise}
   * @resolves The array [aSubject, aData] from the observed notification.
   * @rejects Never.
   */
  waitForNotification: function (aTopic) {
    Output.print("Waiting for notification: '" + aTopic + "'.");

    return new Promise(resolve => Services.obs.addObserver(
      function observe(aSubject, aTopic, aData) {
        Services.obs.removeObserver(observe, aTopic);
        resolve([aSubject, aData]);
      }, aTopic, false));
  },

  /**
   * Waits for a DOM event on the specified target.
   *
   * @param aTarget
   *        The DOM EventTarget on which addEventListener should be called.
   * @param aEventName
   *        String with the name of the event.
   * @param aUseCapture
   *        This parameter is passed to the addEventListener call.
   *
   * @return {Promise}
   * @resolves The arguments from the observed event.
   * @rejects Never.
   */
  waitForEvent: function (aTarget, aEventName, aUseCapture = false) {
    Output.print("Waiting for event: '" + aEventName + "' on " + aTarget + ".");

    return new Promise(resolve => aTarget.addEventListener(aEventName,
      function onEvent(...aArgs) {
        aTarget.removeEventListener(aEventName, onEvent, aUseCapture);
        resolve(...aArgs);
      }, aUseCapture));
  },

  // While the previous test file should have deleted all the temporary files it
  // used, on Windows these might still be pending deletion on the physical file
  // system.  Thus, start from a new base number every time, to make a collision
  // with a file that is still pending deletion highly unlikely.
  _fileCounter: Math.floor(Math.random() * 1000000),

  /**
   * Returns a reference to a temporary file, that is guaranteed not to exist,
   * and to have never been created before.
   *
   * @param aLeafName
   *        Suggested leaf name for the file to be created.
   *
   * @return {Promise}
   * @resolves Path of a non-existent file in a temporary directory.
   *
   * @note It is not enough to delete the file if it exists, or to delete the
   *       file after calling nsIFile.createUnique, because on Windows the
   *       delete operation in the file system may still be pending, preventing
   *       a new file with the same name to be created.
   */
  getTempFile: Task.async(function* (aLeafName) {
    // Prepend a serial number to the extension in the suggested leaf name.
    let [base, ext] = DownloadPaths.splitBaseNameAndExtension(aLeafName);
    let leafName = base + "-" + this._fileCounter + ext;
    this._fileCounter++;

    // Get a file reference under the temporary directory for this test file.
    let path = OS.Path.join(OS.Constants.Path.tmpDir, leafName);
    Assert.ok(!(yield OS.File.exists(path)));

    // Ensure the file is deleted whe the test terminates.
    add_termination_task(function* () {
      if (yield OS.File.exists(path)) {
        yield OS.File.remove(path);
      }
    });

    return path;
  }),
}

/* --- Initialization and termination functions common to all tests --- */

add_task(function* test_common_initialize() {
  // We must manually enable the feature while testing.
  Services.prefs.setBoolPref("dom.forms.requestAutocomplete", true);
  add_termination_task(function* () {
    Services.prefs.clearUserPref("dom.forms.requestAutocomplete");
  });
});
