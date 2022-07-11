/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { ProcessType } = ChromeUtils.import(
  "resource://gre/modules/ProcessType.jsm"
);

let AboutThirdParty = null;
let CrashModuleSet = null;

async function fetchData() {
  let data = null;
  try {
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
      Cu.reportError(e);
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
  }

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
  }

  data.modules.sort((a, b) => {
    // Firstly, show crasher modules
    let diff = b.isCrasher - a.isCrasher;
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
  });

  return data.modules;
}

function setContent(element, text, l10n) {
  if (text) {
    element.textContent = text;
  } else if (l10n) {
    element.setAttribute("data-l10n-id", l10n);
  }
}

function onClickOpenDir(event) {
  const button = event.target.closest("button");
  if (!button?.fileObj) {
    return;
  }
  button.fileObj.reveal();
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
  const clipboardData = aData.map(module => {
    const copied = {
      name: module.dllFile?.leafName,
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

  return navigator.clipboard.writeText(JSON.stringify(clipboardData, null, 2));
}

function correctProcessTypeForFluent(type) {
  // GetProcessTypeString() in UntrustedModulesDataSerializer.cpp converted
  // the "default" process type to "browser" to send as telemetry.  We revert
  // it to pass to ProcessType API.
  const geckoType = type == "browser" ? "default" : type;
  return ProcessType.fluentNameFromProcessTypeString(geckoType);
}

function visualizeData(aData) {
  const templateCard = document.querySelector("template[name=card]");
  const templateTableRow = document.querySelector(
    "template[name=event-table-row]"
  );

  const labelLoadStatus = [
    "third-party-status-loaded",
    "third-party-status-blocked",
    "third-party-status-redirected",
  ];

  const mainContentFragment = new DocumentFragment();

  for (const module of aData) {
    const newCard = templateCard.content.cloneNode(true);
    const leafName = module.dllFile?.leafName;

    setContent(newCard.querySelector(".module-name"), leafName);
    newCard
      .querySelector(".button-expand")
      .addEventListener("click", onClickExpand);

    const modTagsContainer = newCard.querySelector(".module-tags");
    if (module.typeFlags & Ci.nsIAboutThirdParty.ModuleType_IME) {
      modTagsContainer.querySelector(".tag-ime").hidden = false;
    }
    if (module.typeFlags & Ci.nsIAboutThirdParty.ModuleType_ShellExtension) {
      modTagsContainer.querySelector(".tag-shellex").hidden = false;
    }

    const btnOpenDir = newCard.querySelector(".button-open-dir");
    btnOpenDir.fileObj = module.dllFile;
    btnOpenDir.addEventListener("click", onClickOpenDir);

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

  document.getElementById("main").appendChild(mainContentFragment);
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
      Cu.reportError(e);
    }
    return NaN;
  };

  if (CrashModuleSet || !AppConstants.MOZ_CRASHREPORTER) {
    return;
  }

  const { getCrashManager } = ChromeUtils.import(
    "resource://gre/modules/CrashManager.jsm"
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
      await copyDataToClipboard(data || []).catch(Cu.reportError);

      e.target.disabled = false;
    });

  const backgroundTasks = [
    AboutThirdParty.collectSystemInfo(),
    collectCrashInfo(),
  ];

  let hasData = false;
  Promise.all(backgroundTasks)
    .then(() => {
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
    .catch(Cu.reportError);

  const data = await fetchData();
  window.fetchDataDone = true;

  hasData = data?.length;
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
  Cu.reportError(ex);
}
