"use strict";

// Helper to observe process shutdowns. Used to detect when extension processes
// have shut down. For simplicity, this helper does not filter by extension
// processes because the callers knowingly pass extension process childIDs only.
class ProcessWatcher {
  constructor() {
    // Map of childID to boolean (whether process ended abnormally)
    this.seenChildIDs = new Map();
    this.onShutdownCallbacks = new Set();
    Services.obs.addObserver(this, "ipc:content-shutdown");

    // See setExtProcessTerminationDeadline and waitAndCheckIsProcessAlive.
    // We measure the duration of an earlier test to determine the reasonable
    // duration during which a terminated extension process should stay alive.
    // Use a high default in case that task was skipped, e.g. by .only().
    this.deadDeadline = 5000;
  }

  unregister() {
    Services.obs.removeObserver(this, "ipc:content-shutdown");
  }

  observe(subject, topic, data) {
    const childID = parseInt(data, 10);
    const abnormal = subject.QueryInterface(Ci.nsIPropertyBag2).get("abnormal");
    info(`Observed content shutdown, childID=${childID}, abnormal=${abnormal}`);
    this.seenChildIDs.set(childID, !!abnormal);
    for (let onShutdownCallback of this.onShutdownCallbacks) {
      onShutdownCallback(childID);
    }
  }

  isProcessAlive(childID) {
    return !this.seenChildIDs.has(childID);
  }

  async waitForTermination(childID, expectAbnormal = false) {
    // We only expect content processes, so childID should never be zero.
    Assert.ok(childID, `waitForTermination: ${childID}`);

    if (!this.isProcessAlive(childID)) {
      info(`Process has already shut down: ${childID}`);
    } else {
      info(`Waiting for process to shut down: ${childID}`);
      await new Promise(resolve => {
        const onShutdownCallback = _childID => {
          if (_childID === childID) {
            info(`Process has shut down: ${childID}`);
            this.onShutdownCallbacks.delete(onShutdownCallback);
            resolve();
          }
        };
        this.onShutdownCallbacks.add(onShutdownCallback);
      });
    }

    // When we get here, !isProcessAlive or onShutdownCallback was called,
    // which implies that childID is a key in the seenChildIDs Map.
    const abnormal = this.seenChildIDs.get(childID);
    if (expectAbnormal) {
      Assert.ok(abnormal, "Process should have ended abnormally.");
    } else if (AppConstants.platform === "android" && abnormal) {
      // On Android, the implementation sometimes triggers abnormal shutdowns
      // when we expect normal shutdown. This is undesired, but as it happens
      // intermittently, pretend that everything is OK and log a message.
      Assert.ok(true, "Process should have ended normally, but did not.");
    } else {
      Assert.ok(!abnormal, "Process should have ended normally.");
    }
  }

  // Set the deadline as used by "waitAndCheckIsProcessAlive". The deadline is
  // the time by which an unexpected process termination should happen to catch
  // unexpected process termination.
  setExtProcessTerminationDeadline(deadline) {
    // Have some reasonably small minimum deadline, in case the caller
    // experiences a drifted timer that results in negative value.
    const MIN_DEADLINE = 1000;
    // Tests time out after 30 seconds. Enforce a maximum deadline below that
    // limit, e.g. in case a process is being debugged.
    const MAX_DEADLINE = 20000;
    if (deadline < MIN_DEADLINE) {
      this.deadDeadline = MIN_DEADLINE;
    } else if (deadline > MAX_DEADLINE) {
      this.deadDeadline = MAX_DEADLINE;
    } else {
      this.deadDeadline = deadline;
    }
  }

  async waitAndCheckIsProcessAlive(childID) {
    Assert.ok(this.isProcessAlive(childID), `Process ${childID} is alive`);

    // We want to verify that the extension process does not shut down too soon.
    // There is no great way to verify this, other than waiting for a bit and
    // verifying that the process is still around.
    info(`Waiting for ${this.deadDeadline} ms and process ${childID}`);

    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, this.deadDeadline));

    Assert.ok(this.isProcessAlive(childID), `Process ${childID} still alive`);
  }
}

// Register early so we catch all terminations.
const processWatcher = new ProcessWatcher();
registerCleanupFunction(() => processWatcher.unregister());

function pidOfContentPage(contentPage) {
  return contentPage.browsingContext.currentWindowGlobal.domProcess.childID;
}

function pidOfBackgroundPage(extension) {
  return extension.extension.backgroundContext.xulBrowser.browsingContext
    .currentWindowGlobal.domProcess.childID;
}

async function loadExtensionAndGetPid() {
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.sendMessage("bg_loaded");
    },
  });
  await extension.startup();
  await extension.awaitMessage("bg_loaded");
  let pid = pidOfBackgroundPage(extension);
  await extension.unload();
  return pid;
}

add_setup(async function setup_start_and_quit_addon_manager() {
  // None of this setup is strictly required for the test file to pass, but
  // exists to trigger conditions that were historically associated with bugs
  // and test failures.

  // As a regression test for bug 1845352: Verify that (simulating) shut down
  // of the AddonManager does not break the behavior of extension process
  // spawning. For details see bug 1845352 and bug 1845778.
  ExtensionTestUtils.mockAppInfo();
  AddonTestUtils.init(globalThis);
  await AddonTestUtils.promiseStartupManager();
  info("Starting an extension to load the extension process");
  let extension = ExtensionTestUtils.loadExtension({
    background() {
      window.onload = () => browser.test.sendMessage("first_run");
    },
  });
  await extension.startup();
  await extension.awaitMessage("first_run");
  info("Fully loaded initial extension and its process, shutting down now");
  await extension.unload();
  await AddonTestUtils.promiseShutdownManager();
  // Bug 1845352 regression test: the above call broke the test that verified
  // process reuse, because unexpectedly the extension process was shut down
  // when promiseShutdownManager triggered "quit-application-granted".
});

add_task(
  {
    // Here we confirm the usual default behavior. We explicitly set the pref
    // to 0 because head_remote.js sets the value to 1.
    pref_set: [["dom.ipc.keepProcessesAlive.extension", 0]],
  },
  async function shutdown_extension_process_on_extension_background_unload() {
    info("Starting and unloading first extension");
    let pid1 = await loadExtensionAndGetPid();

    info("Extension process should end after unloading the only extension doc");
    await processWatcher.waitForTermination(pid1);
  }
);

add_task(
  {
    // This test verifies that dom.ipc.keepProcessesAlive.extension=1 works,
    // because we rely on it in unit tests, mainly to minimize overhead.
    pref_set: [["dom.ipc.keepProcessesAlive.extension", 1]],
  },
  async function extension_process_reused_between_background_page_restarts() {
    info("Starting and unloading first extension");
    let pid1 = await loadExtensionAndGetPid();

    info("Process should be alive after unloading the only extension (1)");
    await processWatcher.waitAndCheckIsProcessAlive(pid1);

    info("Starting and unloading second extension");
    let pid2 = await loadExtensionAndGetPid();
    Assert.equal(pid1, pid2, "Extension process was reused");

    info("Process should be alive after unloading the only extension (2)");
    await processWatcher.waitAndCheckIsProcessAlive(pid1);

    // Try again repeatedly for many times to verify that this is not a fluke.
    // The number of attempts is arbitrarily chosen.
    for (let i = 1; i <= 9; ++i) {
      let pid3 = await loadExtensionAndGetPid();
      Assert.equal(pid1, pid3, `Extension process was reused at attempt ${i}`);
    }

    info("Process should be alive after unloading the only extension (3)");
    await processWatcher.waitAndCheckIsProcessAlive(pid1);

    // Note: while this task started without extension process, we end this
    // task with an extension process still running.
  }
);

add_task(
  {
    // Here we confirm the usual default behavior. We explicitly set the pref
    // to 0 because head_remote.js sets the value to 1.
    pref_set: [["dom.ipc.keepProcessesAlive.extension", 0]],
  },
  async function shutdown_extension_process_on_last_extension_page_unload() {
    let extension = ExtensionTestUtils.loadExtension({
      files: {
        "page.html": `<!DOCTYPE html><script src="page.js"></script>`,
        "page.js": () => browser.test.sendMessage("page_loaded"),
      },
    });

    await extension.startup();
    const EXT_PAGE = `moz-extension://${extension.uuid}/page.html`;
    async function openOnlyExtensionPageAndGetPid() {
      let contentPage = await ExtensionTestUtils.loadContentPage(EXT_PAGE);
      await extension.awaitMessage("page_loaded");
      let pid = pidOfContentPage(contentPage);
      await contentPage.close();
      return pid;
    }

    const timeStart = Date.now();
    info("Opening first page");
    let contentPage = await ExtensionTestUtils.loadContentPage(EXT_PAGE);
    await extension.awaitMessage("page_loaded");
    let pid1 = pidOfContentPage(contentPage);

    info("Opening and closing second page while the first is open");
    let pid2 = await openOnlyExtensionPageAndGetPid();
    Assert.equal(pid1, pid2, "Second page should re-use first page's process");
    Assert.ok(processWatcher.isProcessAlive(pid1), "Process not dead");
    await contentPage.close();
    info("Closed last page - extension process should terminate");
    // pid1 should have died when we closed ContentPage. But in case shut down
    // is not immediate, wait a little bit.
    await processWatcher.waitForTermination(pid1);

    let pid3 = await openOnlyExtensionPageAndGetPid();
    Assert.notEqual(pid2, pid3, "Should get a new extension process");

    await extension.unload();
    await processWatcher.waitForTermination(pid3);

    // By now, we have witnessed:
    // 1. extension process spawned.
    // 2. first extension tab loaded.
    // 3. second extension tab loaded.
    // 4. extension process terminated after closing tabs.
    // 5. extension process spawned + terminated after opening and closing tab.
    // This should be plenty of time for any unexpected process termination to
    // have been observed. So wait for that time and not longer.
    processWatcher.setExtProcessTerminationDeadline(Date.now() - timeStart);
  }
);

add_task(
  {
    // This test verifies that dom.ipc.keepProcessesAlive.extension=1 works,
    // because we rely on it in unit tests, mainly to minimize overhead.
    pref_set: [["dom.ipc.keepProcessesAlive.extension", 1]],
  },
  async function keep_extension_process_on_last_extension_page_unload() {
    let extension = ExtensionTestUtils.loadExtension({
      files: {
        "page.html": `<!DOCTYPE html><script src="page.js"></script>`,
        "page.js": () => browser.test.sendMessage("page_loaded"),
      },
    });

    await extension.startup();
    const EXT_PAGE = `moz-extension://${extension.uuid}/page.html`;
    async function openOnlyExtensionPageAndGetPid() {
      let contentPage = await ExtensionTestUtils.loadContentPage(EXT_PAGE);
      await extension.awaitMessage("page_loaded");
      let pid = pidOfContentPage(contentPage);
      await contentPage.close();
      return pid;
    }

    info("Opening and closing first page");
    let pid1 = await openOnlyExtensionPageAndGetPid();

    info("No extension pages, but extension process should still be alive (1)");
    await processWatcher.waitAndCheckIsProcessAlive(pid1);

    let pid2 = await openOnlyExtensionPageAndGetPid();
    Assert.equal(pid1, pid2, "Extension process is reused by second page");

    info("No extension pages, but extension process should still be alive (2)");
    await processWatcher.waitAndCheckIsProcessAlive(pid1);

    await extension.unload();
    info("No extensions around, but extension process should still be alive");

    await processWatcher.waitAndCheckIsProcessAlive(pid1);

    // Note: while this task started without extension process, we end this
    // task with an extension process still running.
  }
);
