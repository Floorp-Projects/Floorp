/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { ProcessType } = ChromeUtils.importESModule(
  "resource://gre/modules/ProcessType.sys.mjs"
);

let AboutThirdParty = null;
let CrashModuleSet = null;
let gBackgroundTasksDone = false;

function moduleCompareForDisplay(a, b) {
  // First, show blocked modules that were blocked at launch - this will keep the ordering
  // consistent when the user blocks/unblocks things.
  const bBlocked =
    b.typeFlags & Ci.nsIAboutThirdParty.ModuleType_BlockedByUserAtLaunch
      ? 1
      : 0;
  const aBlocked =
    a.typeFlags & Ci.nsIAboutThirdParty.ModuleType_BlockedByUserAtLaunch
      ? 1
      : 0;

  let diff = bBlocked - aBlocked;
  if (diff) {
    return diff;
  }

  // Next, show crasher modules
  diff = b.isCrasher - a.isCrasher;
  if (diff) {
    return diff;
  }

  // Then unknown-type modules
  diff = a.typeFlags - b.typeFlags;
  if (diff) {
    return diff;
  }

  // Lastly sort the remaining modules in descending order
  // of duration to move up slower modules.
  return b.loadingOnMain - a.loadingOnMain;
}

async function fetchData() {
  let data = null;
  try {
    // Wait until the module load events are ready (bug 1833152)
    const sleep = delayInMs =>
      new Promise(resolve => setTimeout(resolve, delayInMs));
    let loadEventsReady = Services.telemetry.areUntrustedModuleLoadEventsReady;
    let numberOfAttempts = 0;
    // Just to make sure we don't infinite loop here. (this is normally quite
    // quick) If we do hit this limit, the page will return an empty list of
    // modules.
    const MAX_ATTEMPTS = 30;
    while (!loadEventsReady && numberOfAttempts < MAX_ATTEMPTS) {
      await sleep(1000);
      numberOfAttempts++;
      loadEventsReady = Services.telemetry.areUntrustedModuleLoadEventsReady;
    }

    data = await Services.telemetry.getUntrustedModuleLoadEvents(
      Services.telemetry.INCLUDE_OLD_LOADEVENTS |
        Services.telemetry.KEEP_LOADEVENTS_NEW |
        Services.telemetry.INCLUDE_PRIVATE_FIELDS_IN_LOADEVENTS |
        Services.telemetry.EXCLUDE_STACKINFO_FROM_LOADEVENTS
    );
  } catch (e) {
    // No error report in case of NS_ERROR_NOT_AVAILABLE
    // because the method throws it when data is empty.
    if (
      !(e instanceof Components.Exception) ||
      e.result != Cr.NS_ERROR_NOT_AVAILABLE
    ) {
      console.error(e);
    }
  }

  if (!data || !data.modules || !data.processes) {
    return null;
  }

  // The original telemetry data structure has an array of modules
  // and an array of loading events referring to the module array's
  // item via its index.
  // To easily display data per module, we put loading events into
  // a corresponding module object and return the module array.

  for (const module of data.modules) {
    module.events = [];
    module.loadingOnMain = { count: 0, sum: 0 };

    const moduleName = module.dllFile?.leafName;
    module.typeFlags = AboutThirdParty.lookupModuleType(moduleName);
    module.isCrasher = CrashModuleSet?.has(moduleName);

    module.application = AboutThirdParty.lookupApplication(
      module.dllFile?.path
    );
    module.moduleName = module.dllFile?.leafName;
    module.hasLoadInformation = true;
  }

  let blockedModules = data.blockedModules.map(blockedModuleName => {
    return {
      moduleName: blockedModuleName,
      typeFlags: AboutThirdParty.lookupModuleType(blockedModuleName),
      isCrasher: CrashModuleSet?.has(blockedModuleName),
      hasLoadInformation: false,
    };
  });

  for (const [proc, perProc] of Object.entries(data.processes)) {
    for (const event of perProc.events) {
      // The expected format of |proc| is <type>.<pid> like "browser.0x1234"
      const [ptype, pidHex] = proc.split(".");
      event.processType = ptype;
      event.processID = parseInt(pidHex, 16);

      event.mainThread =
        event.threadName == "MainThread" || event.threadName == "Main Thread";

      const module = data.modules[event.moduleIndex];
      if (event.mainThread) {
        ++module.loadingOnMain.count;
        module.loadingOnMain.sum += event.loadDurationMS;
      }

      module.events.push(event);
    }
  }

  for (const module of data.modules) {
    const avg = module.loadingOnMain.count
      ? module.loadingOnMain.sum / module.loadingOnMain.count
      : 0;
    module.loadingOnMain = avg;
    module.events.sort((a, b) => {
      const diff = a.processType.localeCompare(b.processType);
      return diff ? diff : a.processID - b.processID;
    });
    // If this module was blocked but not by the user, it must have been blocked
    // by the static blocklist.
    // But we don't know this for sure unless the background tasks were done
    // by the time we gathered data about the module above.
    if (gBackgroundTasksDone) {
      module.isBlockedByBuiltin =
        !(
          module.typeFlags &
          Ci.nsIAboutThirdParty.ModuleType_BlockedByUserAtLaunch
        ) &&
        !!module.events.length &&
        module.events.every(e => e.loadStatus !== 0);
    } else {
      module.isBlockedByBuiltin = false;
    }
  }

  data.modules.sort(moduleCompareForDisplay);

  return { modules: data.modules, blocked: blockedModules };
}

function setContent(element, text, l10n) {
  if (text) {
    element.textContent = text;
  } else if (l10n) {
    document.l10n.setAttributes(element, l10n);
  }
}

function onClickOpenDir(event) {
  const module = event.target.closest(".card").module;
  if (!module?.dllFile) {
    return;
  }
  module.dllFile.reveal();
}

// Returns whether we should restart.
async function confirmRestartPrompt() {
  let [msg, title, restartButtonText, restartLaterButtonText] =
    await document.l10n.formatValues([
      { id: "third-party-blocking-requires-restart" },
      { id: "third-party-should-restart-title" },
      { id: "third-party-restart-now" },
      { id: "third-party-restart-later" },
    ]);
  let buttonFlags =
    Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
    Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_IS_STRING +
    Services.prompt.BUTTON_POS_1_DEFAULT;
  let buttonIndex = Services.prompt.confirmEx(
    window.browsingContext.topChromeWindow,
    title,
    msg,
    buttonFlags,
    restartButtonText,
    restartLaterButtonText,
    null,
    null,
    {}
  );
  return buttonIndex === 0;
}

let processingBlockRequest = false;
async function onBlock(event) {
  const module = event.target.closest(".card").module;
  if (!module?.moduleName) {
    return;
  }
  // To avoid race conditions, don't allow any modules to be blocked/unblocked
  // until we've updated and written the blocklist.
  if (processingBlockRequest) {
    return;
  }
  processingBlockRequest = true;

  let updatedBlocklist = false;
  try {
    const wasBlocked = event.target.classList.contains("module-blocked");
    await AboutThirdParty.updateBlocklist(module.moduleName, !wasBlocked);

    event.target.classList.toggle("module-blocked");
    let blockButtonL10nId;
    if (wasBlocked) {
      blockButtonL10nId = "third-party-button-to-block-module";
    } else {
      blockButtonL10nId = AboutThirdParty.isDynamicBlocklistDisabled
        ? "third-party-button-to-unblock-module-disabled"
        : "third-party-button-to-unblock-module";
    }
    document.l10n.setAttributes(event.target, blockButtonL10nId);
    updatedBlocklist = true;
  } catch (ex) {
    console.error("Failed to update the blocklist file - ", ex.result);
  } finally {
    processingBlockRequest = false;
  }
  if (updatedBlocklist && (await confirmRestartPrompt())) {
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(
      cancelQuit,
      "quit-application-requested",
      "restart"
    );
    if (!cancelQuit.data) {
      // restart was not cancelled.
      // Note that even if we're in safe mode, we don't restart
      // into safe mode, because it's likely the user is trying to
      // fix a crash or something, and they'd probably like to
      // see if it works.
      Services.startup.quit(
        Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
      );
    }
  }
}

function onClickExpand(event) {
  const card = event.target.closest(".card");
  const button = event.target.closest("button");

  const table = card.querySelector(".event-table");
  if (!table) {
    return;
  }

  if (table.hidden) {
    table.hidden = false;
    button.classList.add("button-collapse");
    button.classList.remove("button-expand");
    setContent(button, null, "third-party-button-collapse");
  } else {
    table.hidden = true;
    button.classList.add("button-expand");
    button.classList.remove("button-collapse");
    setContent(button, null, "third-party-button-expand");
  }
}

function createDetailRow(label, value) {
  if (!document.templateDetailRow) {
    document.templateDetailRow = document.querySelector(
      "template[name=module-detail-row]"
    );
  }

  const fragment = document.templateDetailRow.content.cloneNode(true);
  setContent(fragment.querySelector("div > label"), null, label);
  setContent(fragment.querySelector("div > span"), value);
  return fragment;
}

function copyDataToClipboard(aData) {
  const modulesData = aData.modules.map(module => {
    const copied = {
      name: module.moduleName,
      fileVersion: module.fileVersion,
    };

    // We include the typeFlags field only when it's not 0 because
    // typeFlags == 0 means system info is not yet collected.
    if (module.typeFlags) {
      copied.typeFlags = module.typeFlags;
    }
    if (module.signedBy) {
      copied.signedBy = module.signedBy;
    }
    if (module.isCrasher) {
      copied.isCrasher = module.isCrasher;
    }
    if (module.companyName) {
      copied.companyName = module.companyName;
    }
    if (module.application) {
      copied.applicationName = module.application.name;
      copied.applicationPublisher = module.application.publisher;
    }

    if (Array.isArray(module.events)) {
      copied.events = module.events.map(event => {
        return {
          processType: event.processType,
          processID: event.processID,
          threadID: event.threadID,
          loadStatus: event.loadStatus,
          loadDurationMS: event.loadDurationMS,
        };
      });
    }

    return copied;
  });
  const blockedData = aData.blocked.map(blockedModule => {
    const copied = {
      name: blockedModule.moduleName,
    };
    // We include the typeFlags field only when it's not 0 because
    // typeFlags == 0 means system info is not yet collected.
    if (blockedModule.typeFlags) {
      copied.typeFlags = blockedModule.typeFlags;
    }
    if (blockedModule.isCrasher) {
      copied.isCrasher = blockedModule.isCrasher;
    }
    return copied;
  });
  let clipboardData = { modules: modulesData, blocked: blockedData };

  return navigator.clipboard.writeText(JSON.stringify(clipboardData, null, 2));
}

function correctProcessTypeForFluent(type) {
  // GetProcessTypeString() in UntrustedModulesDataSerializer.cpp converted
  // the "default" process type to "browser" to send as telemetry.  We revert
  // it to pass to ProcessType API.
  const geckoType = type == "browser" ? "default" : type;
  return ProcessType.fluentNameFromProcessTypeString(geckoType);
}

function setUpBlockButton(aCard, isBlocklistDisabled, aModule) {
  const blockButton = aCard.querySelector(".button-block");
  if (aModule.hasLoadInformation) {
    if (!aModule.isBlockedByBuiltin) {
      blockButton.hidden = aModule.typeFlags == 0;
    }
  } else {
    // This means that this is an entry in the dynamic blocklist that
    // has not attempted to load, thus we have very little information
    // about it (just its name). So this should always show up.
    blockButton.hidden = false;
    // Bug 1808904 - don't allow unblocking this module before we've loaded
    // the list of blocked modules in the background task.
    blockButton.disabled = !gBackgroundTasksDone;
  }
  // If we haven't loaded the typeFlags yet and we don't have any load information for this
  // module, default to showing that the module is blocked (because we must have gotten this
  // module's info from the dynamic blocklist)
  if (
    aModule.typeFlags & Ci.nsIAboutThirdParty.ModuleType_BlockedByUser ||
    (aModule.typeFlags == 0 && !aModule.hasLoadInformation)
  ) {
    blockButton.classList.add("module-blocked");
  }

  if (isBlocklistDisabled) {
    blockButton.classList.add("blocklist-disabled");
  }
  if (blockButton.classList.contains("module-blocked")) {
    document.l10n.setAttributes(
      blockButton,
      isBlocklistDisabled
        ? "third-party-button-to-unblock-module-disabled"
        : "third-party-button-to-unblock-module"
    );
  }
}

function visualizeData(aData) {
  const templateCard = document.querySelector("template[name=card]");
  const templateBlockedCard = document.querySelector(
    "template[name=card-blocked]"
  );
  const templateTableRow = document.querySelector(
    "template[name=event-table-row]"
  );

  // These correspond to the enum ModuleLoadInfo::Status
  const labelLoadStatus = [
    "third-party-status-loaded",
    "third-party-status-blocked",
    "third-party-status-redirected",
    "third-party-status-blocked",
  ];

  const isBlocklistAvailable =
    AboutThirdParty.isDynamicBlocklistAvailable &&
    Services.policies.isAllowed("thirdPartyModuleBlocking");
  const isBlocklistDisabled = AboutThirdParty.isDynamicBlocklistDisabled;

  const mainContentFragment = new DocumentFragment();

  // Blocklist entries are case-insensitive
  let lowercaseModuleNames = new Set(
    aData.modules.map(module => module.moduleName.toLowerCase())
  );
  for (const module of aData.blocked) {
    if (lowercaseModuleNames.has(module.moduleName.toLowerCase())) {
      // Only show entries that we haven't already tried to load,
      // because those will already show up in the page
      continue;
    }
    const newCard = templateBlockedCard.content.cloneNode(true);
    setContent(newCard.querySelector(".module-name"), module.moduleName);
    // Referred by the button click handlers
    newCard.querySelector(".card").module = {
      moduleName: module.moduleName,
    };

    if (isBlocklistAvailable) {
      setUpBlockButton(newCard, isBlocklistDisabled, module);
    }
    if (module.isCrasher) {
      newCard.querySelector(".image-warning").hidden = false;
    }
    mainContentFragment.appendChild(newCard);
  }

  for (const module of aData.modules) {
    const newCard = templateCard.content.cloneNode(true);
    const moduleName = module.moduleName;

    // Referred by the button click handlers
    newCard.querySelector(".card").module = {
      dllFile: module.dllFile,
      moduleName: module.moduleName,
      fileVersion: module.fileVersion,
    };

    setContent(newCard.querySelector(".module-name"), moduleName);

    const modTagsContainer = newCard.querySelector(".module-tags");
    if (module.typeFlags & Ci.nsIAboutThirdParty.ModuleType_IME) {
      modTagsContainer.querySelector(".tag-ime").hidden = false;
    }
    if (module.typeFlags & Ci.nsIAboutThirdParty.ModuleType_ShellExtension) {
      modTagsContainer.querySelector(".tag-shellex").hidden = false;
    }

    newCard.querySelector(".blocked-by-builtin").hidden =
      !module.isBlockedByBuiltin;
    if (isBlocklistAvailable) {
      setUpBlockButton(newCard, isBlocklistDisabled, module);
    }

    if (module.isCrasher) {
      newCard.querySelector(".image-warning").hidden = false;
    }

    if (!module.signedBy) {
      newCard.querySelector(".image-unsigned").hidden = false;
    }

    const modDetailContainer = newCard.querySelector(".module-details");

    if (module.application) {
      modDetailContainer.appendChild(
        createDetailRow("third-party-detail-app", module.application.name)
      );
      modDetailContainer.appendChild(
        createDetailRow(
          "third-party-detail-publisher",
          module.application.publisher
        )
      );
    }

    if (module.fileVersion) {
      modDetailContainer.appendChild(
        createDetailRow("third-party-detail-version", module.fileVersion)
      );
    }

    const vendorInfo = module.signedBy || module.companyName;
    if (vendorInfo) {
      modDetailContainer.appendChild(
        createDetailRow("third-party-detail-vendor", vendorInfo)
      );
    }

    modDetailContainer.appendChild(
      createDetailRow("third-party-detail-occurrences", module.events.length)
    );
    modDetailContainer.appendChild(
      createDetailRow(
        "third-party-detail-duration",
        module.loadingOnMain || "-"
      )
    );

    const eventTable = newCard.querySelector(".event-table > tbody");
    for (const event of module.events) {
      const fragment = templateTableRow.content.cloneNode(true);

      const row = fragment.querySelector("tr");

      setContent(
        row.children[0].querySelector(".process-type"),
        null,
        correctProcessTypeForFluent(event.processType)
      );
      setContent(row.children[0].querySelector(".process-id"), event.processID);

      // Use setContent() instead of simple assignment because
      // loadDurationMS can be empty (not zero) when a module is
      // loaded very early in the process and we need to show
      // a text in that case.
      setContent(
        row.children[1].querySelector(".event-duration"),
        event.loadDurationMS,
        "third-party-message-no-duration"
      );
      row.querySelector(".tag-background").hidden = event.mainThread;

      setContent(row.children[2], null, labelLoadStatus[event.loadStatus]);
      eventTable.appendChild(fragment);
    }

    mainContentFragment.appendChild(newCard);
  }

  const main = document.getElementById("main");
  main.appendChild(mainContentFragment);
  main.addEventListener("click", onClickInMain);
}

function onClickInMain(event) {
  const classList = event.target.classList;
  if (classList.contains("button-open-dir")) {
    onClickOpenDir(event);
  } else if (classList.contains("button-block")) {
    onBlock(event);
  } else if (
    classList.contains("button-expand") ||
    classList.contains("button-collapse")
  ) {
    onClickExpand(event);
  }
}

function clearVisualizedData() {
  const mainDiv = document.getElementById("main");
  while (mainDiv.firstChild) {
    mainDiv.firstChild.remove();
  }
}

async function collectCrashInfo() {
  const parseBigInt = maybeBigInt => {
    try {
      return BigInt(maybeBigInt);
    } catch (e) {
      console.error(e);
    }
    return NaN;
  };

  if (CrashModuleSet || !AppConstants.MOZ_CRASHREPORTER) {
    return;
  }

  const { getCrashManager } = ChromeUtils.importESModule(
    "resource://gre/modules/CrashManager.sys.mjs"
  );
  const crashes = await getCrashManager().getCrashes();
  CrashModuleSet = new Set(
    crashes.map(crash => {
      const stackInfo = crash.metadata?.StackTraces;
      if (!stackInfo) {
        return null;
      }

      const crashAddr = parseBigInt(stackInfo.crash_info?.address);
      if (typeof crashAddr !== "bigint") {
        return null;
      }

      // Find modules whose address range includes the crashing address.
      // No need to check the type of the return value from parseBigInt
      // because comparing BigInt with NaN returns false.
      return stackInfo.modules?.find(
        module =>
          crashAddr >= parseBigInt(module.base_addr) &&
          crashAddr < parseBigInt(module.end_addr)
      )?.filename;
    })
  );
}

async function onLoad() {
  document
    .getElementById("button-copy-to-clipboard")
    .addEventListener("click", async e => {
      e.target.disabled = true;

      const data = await fetchData();
      await copyDataToClipboard(data || []).catch(console.error);

      e.target.disabled = false;
    });

  const backgroundTasks = [
    AboutThirdParty.collectSystemInfo(),
    collectCrashInfo(),
  ];

  let hasData = false;
  Promise.all(backgroundTasks)
    .then(() => {
      gBackgroundTasksDone = true;
      // Reload button will either show or is not needed, so we can hide the
      // loading indicator.
      document.getElementById("background-data-loading").hidden = true;
      if (!hasData) {
        // If all async tasks were completed before fetchData,
        // or there was no data available, visualizeData shows
        // full info and the reload button is not needed.
        return;
      }

      // Add {once: true} to prevent multiple listeners from being scheduled
      const button = document.getElementById("button-reload");
      button.addEventListener(
        "click",
        async event => {
          // Update the content with data we've already collected.
          clearVisualizedData();
          visualizeData(await fetchData());
          event.target.hidden = true;
        },
        { once: true }
      );

      // Coming here means visualizeData is completed before the background
      // tasks are completed.  Because the page does not show full information,
      // we show the reload button to call visualizeData again.
      button.hidden = false;
    })
    .catch(console.error);

  const data = await fetchData();
  // Used for testing purposes
  window.fetchDataDone = true;

  hasData = !!data?.modules.length || !!data?.blocked.length;
  if (!hasData) {
    document.getElementById("no-data").hidden = false;
    return;
  }

  visualizeData(data);
}

try {
  AboutThirdParty = Cc["@mozilla.org/about-thirdparty;1"].getService(
    Ci.nsIAboutThirdParty
  );
  document.addEventListener("DOMContentLoaded", onLoad, { once: true });
} catch (ex) {
  // Do nothing if we fail to create a singleton instance,
  // showing the default no-module message.
  console.error(ex);
}
